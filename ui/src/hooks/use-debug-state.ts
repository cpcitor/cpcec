/**
 * Hook to fetch debug state from the WASM emulator
 */
import { useCallback, useEffect, useState } from 'react'
import type { CpuState, DisassemblyLine, MemoryView, CrtcState, GateArrayState, AsicState, SpriteInfo } from '@/types'
import { disassembleInstruction } from '@/utils/z80-disassembler'

interface EmulatorModule {
  _em_get_reg_af: () => number
  _em_get_reg_bc: () => number
  _em_get_reg_de: () => number
  _em_get_reg_hl: () => number
  _em_get_reg_af2: () => number
  _em_get_reg_bc2: () => number
  _em_get_reg_de2: () => number
  _em_get_reg_hl2: () => number
  _em_get_reg_ix: () => number
  _em_get_reg_iy: () => number
  _em_get_reg_sp: () => number
  _em_get_reg_pc: () => number
  _em_get_reg_ir: () => number
  _em_get_reg_iff: () => number
  _em_peek: (address: number) => number
  _em_peek_range: (address: number, length: number) => number
  _em_get_stack: (count: number) => number
  _em_is_paused: () => number
  _em_step: () => void
  _em_step_over?: () => void
  _em_pause: () => void
  _em_run?: () => void
  _em_run_to?: (address: number) => void
  _em_reset: () => void
  // CRTC functions
  _em_get_crtc_reg?: (reg: number) => number
  _em_get_crtc_state?: () => number
  // Gate Array functions
  _em_get_gate_palette?: (index: number) => number
  _em_get_gate_state?: () => number
  _em_get_screen_mode?: () => number
  // ASIC functions
  _em_is_plus_enabled?: () => number
  _em_is_asic_unlocked?: () => number
  _em_get_asic_state?: () => number
  _em_get_sprite_info?: (sprite: number) => number
  HEAPU8?: Uint8Array
  wasmMemory?: WebAssembly.Memory
}

function getModule(): EmulatorModule | null {
  const g = globalThis as typeof globalThis & { Module?: EmulatorModule }
  return g.Module ?? null
}

export interface DebugState {
  cpu: CpuState | null
  memory: MemoryView | null
  stack: number[] | null
  disassembly: DisassemblyLine[]
  isPaused: boolean
  crtc: CrtcState | null
  gateArray: GateArrayState | null
  asic: AsicState | null
  sprites: SpriteInfo[]
}

export function useDebugState(enabled: boolean, refreshInterval = 100) {
  const [state, setState] = useState<DebugState>({
    cpu: null,
    memory: null,
    stack: null,
    disassembly: [],
    isPaused: false,
    crtc: null,
    gateArray: null,
    asic: null,
    sprites: []
  })

  const fetchCpuState = useCallback((): CpuState | null => {
    const Module = getModule()
    if (!Module) return null

    try {
      const iff = Module._em_get_reg_iff()
      return {
        af: Module._em_get_reg_af(),
        bc: Module._em_get_reg_bc(),
        de: Module._em_get_reg_de(),
        hl: Module._em_get_reg_hl(),
        af2: Module._em_get_reg_af2(),
        bc2: Module._em_get_reg_bc2(),
        de2: Module._em_get_reg_de2(),
        hl2: Module._em_get_reg_hl2(),
        ix: Module._em_get_reg_ix(),
        iy: Module._em_get_reg_iy(),
        sp: Module._em_get_reg_sp(),
        pc: Module._em_get_reg_pc(),
        iff1: (iff & 0x01) !== 0,
        iff2: (iff & 0x04) !== 0,
        im: (iff >> 8) & 0x03
      }
    } catch {
      return null
    }
  }, [])

  const fetchMemory = useCallback((address: number, length: number): MemoryView | null => {
    const Module = getModule()
    if (!Module) return null

    try {
      // Use em_peek for individual bytes - more reliable than HEAPU8
      const data: number[] = []
      for (let i = 0; i < length; i++) {
        data.push(Module._em_peek((address + i) & 0xFFFF))
      }
      return {
        startAddress: address,
        data
      }
    } catch {
      return null
    }
  }, [])

  const checkPaused = useCallback((): boolean => {
    const Module = getModule()
    if (!Module) return false
    try {
      return Module._em_is_paused() !== 0
    } catch {
      return false
    }
  }, [])

  const fetchStack = useCallback((count = 8): number[] | null => {
    const Module = getModule()
    if (!Module) {
      console.log('[DEBUG] fetchStack: Module not available')
      return null
    }

    try {
      // Get SP from registers
      const sp = Module._em_get_reg_sp()
      console.log('[DEBUG] fetchStack SP:', sp.toString(16))
      const values: number[] = []
      for (let i = 0; i < count; i++) {
        // Stack entries are 16-bit values (little-endian)
        const addr = (sp + i * 2) & 0xFFFF
        const lo = Module._em_peek(addr)
        const hi = Module._em_peek((addr + 1) & 0xFFFF)
        values.push((hi << 8) | lo)
      }
      console.log('[DEBUG] fetchStack values:', values)
      return values
    } catch (e) {
      console.error('[DEBUG] fetchStack error:', e)
      return null
    }
  }, [])

  // Z80 disassembly using proper disassembler
  const fetchDisassembly = useCallback((pc: number): DisassemblyLine[] => {
    const Module = getModule()
    if (!Module) {
      console.log('[DEBUG] fetchDisassembly: Module not available')
      return []
    }

    const lines: DisassemblyLine[] = []
    try {
      console.log('[DEBUG] fetchDisassembly pc:', pc.toString(16))
      let addr = pc

      // Disassemble 16 instructions starting from PC
      for (let i = 0; i < 16; i++) {
        const result = disassembleInstruction((a) => Module._em_peek(a), addr)
        lines.push({
          address: addr,
          bytes: result.bytes,
          instruction: result.instruction,
          isCurrent: i === 0
        })
        addr = (addr + result.length) & 0xFFFF
      }
      console.log('[DEBUG] fetchDisassembly lines:', lines.length)
    } catch (e) {
      console.error('[DEBUG] fetchDisassembly error:', e)
    }
    return lines
  }, [])

  // Fetch CRTC state
  const fetchCrtcState = useCallback((): CrtcState | null => {
    const Module = getModule()
    if (!Module) return null

    try {
      // Try to use dedicated function if available
      if (Module._em_get_crtc_reg) {
        const registers: number[] = []
        for (let i = 0; i < 18; i++) {
          registers.push(Module._em_get_crtc_reg(i))
        }
        return {
          registers,
          index: 0,
          hcc: 0,
          vcc: 0,
          vlc: 0,
          vtac: 0,
          hsc: 0,
          vsc: 0,
          type: 1,
          status: 0,
          screenAddr: (registers[12] << 8) | registers[13],
          line: 0,
          isLive: true
        }
      }
      
      // Fallback: return placeholder data
      // Standard CPC CRTC values
      return {
        registers: [63, 40, 46, 142, 38, 0, 25, 30, 0, 7, 0, 0, 48, 0, 0, 0, 0, 0],
        index: 0,
        hcc: 0,
        vcc: 0,
        vlc: 0,
        vtac: 0,
        hsc: 0,
        vsc: 0,
        type: 1, // UMC
        status: 0,
        screenAddr: 0xC000,
        line: 0,
        isLive: false
      }
    } catch {
      return null
    }
  }, [])

  // Fetch Gate Array state
  const fetchGateArrayState = useCallback((): GateArrayState | null => {
    const Module = getModule()
    if (!Module) return null

    try {
      // Try to use dedicated functions if available
      if (Module._em_get_gate_palette) {
        const palette: number[] = []
        for (let i = 0; i < 17; i++) {
          palette.push(Module._em_get_gate_palette(i))
        }
        const mode = Module._em_get_screen_mode?.() ?? 1
        return {
          palette,
          index: 0,
          status: mode,
          mcr: mode,
          ram: 0,
          rom: 0,
          hCounter: 0,
          vCounter: 0,
          irqSteps: 0,
          isLive: true
        }
      }
      
      // Fallback: return placeholder data to show the panel works
      // Real data will show after WASM recompilation
      return {
        palette: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 1],
        index: 0,
        status: 1,
        mcr: 1,
        ram: 0,
        rom: 0,
        hCounter: 0,
        vCounter: 0,
        irqSteps: 0,
        isLive: false
      }
    } catch {
      return null
    }
  }, [])

  // Fetch ASIC state (CPC Plus)
  const fetchAsicState = useCallback((): AsicState | null => {
    const Module = getModule()
    if (!Module) return null

    try {
      // Default CPC Plus palette (similar to CPC hardware colors in 12-bit RGB)
      const defaultPalette = [
        0x111, 0x111, 0x00F, 0x11F, 0xF00, 0xF0F, 0xFF0, 0xFFF,
        0x00F, 0x0FF, 0xFF0, 0xFFF, 0x00F, 0x0FF, 0xFF0, 0xFFF,
        0x000, 0x111, 0x222, 0x333, 0x444, 0x555, 0x666, 0x777,
        0x888, 0x999, 0xAAA, 0xBBB, 0xCCC, 0xDDD, 0xEEE, 0xFFF
      ]
      
      // Try to use dedicated function if available
      if (Module._em_is_plus_enabled) {
        const enabled = Module._em_is_plus_enabled() !== 0
        if (!enabled) return null

        const unlocked = Module._em_is_asic_unlocked?.() !== 0
        return {
          enabled,
          unlocked,
          lockCounter: 0,
          rmr2: 0,
          irqBug: false,
          dmaIndex: 0,
          dmaDelay: 0,
          dmaCache: [0, 0, 0],
          palette: defaultPalette,
          isLive: true
        }
      }
      
      // Fallback: return placeholder showing ASIC as locked
      // User can see the panel structure
      return {
        enabled: true,
        unlocked: false,
        lockCounter: 0,
        rmr2: 0,
        irqBug: false,
        dmaIndex: 0,
        dmaDelay: 0,
        dmaCache: [0, 0, 0],
        palette: defaultPalette,
        isLive: false
      }
    } catch {
      return null
    }
  }, [])

  // Fetch sprite info (CPC Plus)
  const fetchSprites = useCallback((): SpriteInfo[] => {
    const Module = getModule()
    if (!Module || !Module._em_is_plus_enabled || !Module._em_get_sprite_info) return []

    try {
      if (Module._em_is_plus_enabled() === 0) return []

      const sprites: SpriteInfo[] = []
      for (let i = 0; i < 16; i++) {
        // Call em_get_sprite_info and read the result via em_peek
        // Since we can't access the returned pointer, we'll just return placeholder
        sprites.push({ x: 0, y: 0, magnification: 0 })
      }
      return sprites
    } catch {
      return []
    }
  }, [])

  // Refresh state periodically when enabled
  useEffect(() => {
    if (!enabled) return

    const refresh = () => {
      const isPaused = checkPaused()
      const cpu = fetchCpuState()
      const memory = cpu ? fetchMemory(cpu.pc, 256) : null
      const stack = fetchStack(8)
      const disassembly = cpu ? fetchDisassembly(cpu.pc) : []
      
      // Only fetch hardware state when paused to avoid interfering with emulation
      const crtc = isPaused ? fetchCrtcState() : null
      const gateArray = isPaused ? fetchGateArrayState() : null
      const asic = isPaused ? fetchAsicState() : null
      const sprites = isPaused ? fetchSprites() : []

      setState({ cpu, memory, stack, disassembly, isPaused, crtc, gateArray, asic, sprites })
    }

    // Initial fetch
    refresh()

    // Set up interval
    const interval = setInterval(refresh, refreshInterval)
    return () => clearInterval(interval)
  }, [enabled, refreshInterval, fetchCpuState, fetchMemory, fetchStack, fetchDisassembly, checkPaused, fetchCrtcState, fetchGateArrayState, fetchAsicState, fetchSprites])

  // Control functions
  const step = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        Module._em_step()
      } catch {
        // Ignore errors
      }
    }
  }, [])

  // Step over - for now same as step (needs WASM recompile for full support)
  const stepOver = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        // Try step_over if available, fallback to step
        if (typeof Module._em_step_over === 'function') {
          Module._em_step_over()
        } else {
          Module._em_step()
        }
      } catch {
        // Ignore errors
      }
    }
  }, [])

  const pause = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        Module._em_pause()
      } catch {
        // Ignore errors
      }
    }
  }, [])

  // Run/continue - use pause toggle
  const run = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        // em_pause toggles, so if paused it will run
        Module._em_pause()
      } catch {
        // Ignore errors
      }
    }
  }, [])

  // runTo not available yet without WASM recompile
  const runTo = useCallback((_address: number) => {
    // Not implemented without WASM support
  }, [])

  const reset = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        Module._em_reset()
      } catch {
        // Ignore errors
      }
    }
  }, [])

  const togglePause = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        Module._em_pause()
      } catch {
        // Ignore errors
      }
    }
  }, [])

  // Function to peek at arbitrary memory addresses
  const peekMemory = useCallback((address: number, length = 256): MemoryView | null => {
    const Module = getModule()
    if (!Module) return null

    try {
      const data: number[] = []
      for (let i = 0; i < length && i < 256; i++) {
        data.push(Module._em_peek((address + i) & 0xFFFF))
      }
      return { startAddress: address & 0xFFFF, data }
    } catch {
      return null
    }
  }, [])

  return {
    ...state,
    step,
    stepOver,
    pause,
    run,
    runTo,
    reset,
    togglePause,
    peekMemory,
    refresh: useCallback(() => {
      const isPaused = checkPaused()
      const cpu = fetchCpuState()
      const memory = cpu ? fetchMemory(cpu.pc, 256) : null
      const stack = fetchStack(8)
      const disassembly = cpu ? fetchDisassembly(cpu.pc) : []
      const crtc = fetchCrtcState()
      const gateArray = fetchGateArrayState()
      const asic = fetchAsicState()
      const sprites = fetchSprites()
      setState({ cpu, memory, stack, disassembly, isPaused, crtc, gateArray, asic, sprites })
    }, [checkPaused, fetchCpuState, fetchMemory, fetchStack, fetchDisassembly, fetchCrtcState, fetchGateArrayState, fetchAsicState, fetchSprites])
  }
}
