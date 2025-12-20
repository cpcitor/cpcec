/**
 * Hook to fetch debug state from the WASM emulator
 */
import { useCallback, useEffect, useState } from 'react'
import type { CpuState, DisassemblyLine, MemoryView } from '@/types'

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
  _em_is_paused: () => number
  _em_step: () => void
  _em_step_over: () => void
  _em_pause: () => void
  _em_run: () => void
  _em_run_to: (address: number) => void
  _em_reset: () => void
  HEAPU8: Uint8Array
}

function getModule(): EmulatorModule | null {
  const g = globalThis as typeof globalThis & { Module?: EmulatorModule }
  return g.Module ?? null
}

export interface DebugState {
  cpu: CpuState | null
  memory: MemoryView | null
  disassembly: DisassemblyLine[]
  isPaused: boolean
}

export function useDebugState(enabled: boolean, refreshInterval = 100) {
  const [state, setState] = useState<DebugState>({
    cpu: null,
    memory: null,
    disassembly: [],
    isPaused: false
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
      const ptr = Module._em_peek_range(address, length)
      const data = new Uint8Array(length)
      for (let i = 0; i < length; i++) {
        data[i] = Module.HEAPU8[ptr + i]
      }
      return {
        startAddress: address,
        data: Array.from(data)
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

  // Simple disassembly - just show bytes at PC for now
  // A full disassembler would require more work
  const fetchDisassembly = useCallback((pc: number): DisassemblyLine[] => {
    const Module = getModule()
    if (!Module) return []

    const lines: DisassemblyLine[] = []
    try {
      // Fetch 32 bytes starting from PC
      const ptr = Module._em_peek_range(pc, 32)
      let addr = pc

      // Simple byte display (not a real disassembler)
      for (let i = 0; i < 16; i++) {
        const byte = Module.HEAPU8[ptr + i]
        lines.push({
          address: addr,
          bytes: [byte],
          instruction: `DB $${byte.toString(16).toUpperCase().padStart(2, '0')}`,
          isCurrent: i === 0
        })
        addr = (addr + 1) & 0xFFFF
      }
    } catch {
      // Ignore errors
    }
    return lines
  }, [])

  // Refresh state periodically when enabled
  useEffect(() => {
    if (!enabled) return

    const refresh = () => {
      const isPaused = checkPaused()
      const cpu = fetchCpuState()
      const memory = cpu ? fetchMemory(cpu.pc, 256) : null
      const disassembly = cpu ? fetchDisassembly(cpu.pc) : []

      setState({ cpu, memory, disassembly, isPaused })
    }

    // Initial fetch
    refresh()

    // Set up interval
    const interval = setInterval(refresh, refreshInterval)
    return () => clearInterval(interval)
  }, [enabled, refreshInterval, fetchCpuState, fetchMemory, fetchDisassembly, checkPaused])

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

  const stepOver = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        Module._em_step_over()
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

  const run = useCallback(() => {
    const Module = getModule()
    if (Module) {
      try {
        Module._em_run()
      } catch {
        // Ignore errors
      }
    }
  }, [])

  const runTo = useCallback((address: number) => {
    const Module = getModule()
    if (Module) {
      try {
        Module._em_run_to(address)
      } catch {
        // Ignore errors
      }
    }
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

  return {
    ...state,
    step,
    stepOver,
    pause,
    run,
    runTo,
    reset,
    togglePause,
    refresh: useCallback(() => {
      const isPaused = checkPaused()
      const cpu = fetchCpuState()
      const memory = cpu ? fetchMemory(cpu.pc, 256) : null
      const disassembly = cpu ? fetchDisassembly(cpu.pc) : []
      setState({ cpu, memory, disassembly, isPaused })
    }, [checkPaused, fetchCpuState, fetchMemory, fetchDisassembly])
  }
}
