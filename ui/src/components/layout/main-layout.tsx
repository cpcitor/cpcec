import { useAtom } from 'jotai'
import { useCallback, useState } from 'react'
import { DebugPanel } from '@/components/debug-panel'
import { EmulatorCanvas } from '@/components/emulator'
import { debugPanelVisibleAtom, emulatorPausedAtom } from '@/store'
import type { CpuState, DisassemblyLine, MemoryView } from '@/types'
import { Header } from './header'
import styles from './main-layout.module.css'
import { Toolbar } from './toolbar'

// Mock debug data for now - will be connected to WASM later
const mockCpu: CpuState = {
  pc: 0x0000,
  sp: 0xbffe,
  af: 0x0044,
  bc: 0x0000,
  de: 0x0000,
  hl: 0x0000,
  ix: 0x0000,
  iy: 0x0000,
  af2: 0x0000,
  bc2: 0x0000,
  de2: 0x0000,
  hl2: 0x0000,
  iff1: false,
  iff2: false,
  im: 1
}

const mockDisassembly: DisassemblyLine[] = [
  { address: 0x0000, bytes: 'F3', mnemonic: 'DI', operands: '' },
  { address: 0x0001, bytes: '01 89 7F', mnemonic: 'LD', operands: 'BC,$7F89' },
  { address: 0x0004, bytes: 'C3 CB 00', mnemonic: 'JP', operands: '$00CB' },
  { address: 0x0007, bytes: '00', mnemonic: 'NOP', operands: '' },
  { address: 0x0008, bytes: '00', mnemonic: 'NOP', operands: '' }
]

const mockMemory: MemoryView = {
  address: 0x0000,
  bytes: new Uint8Array([
    0xf3, 0x01, 0x89, 0x7f, 0xc3, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x7d, 0xbd, 0xc3, 0x98, 0xbd, 0xc3, 0xb9,
    0xbd, 0xc3, 0xbc, 0xbd, 0xc3, 0xbf, 0xbd, 0xc3
  ])
}

export function MainLayout() {
  const [debugVisible, setDebugVisible] = useAtom(debugPanelVisibleAtom)
  const [isPaused] = useAtom(emulatorPausedAtom)
  const [breakpoints, setBreakpoints] = useState<number[]>([])

  const handleAddBreakpoint = useCallback((addr: number) => {
    setBreakpoints((prev) =>
      [...prev.filter((a) => a !== addr), addr].sort((a, b) => a - b)
    )
  }, [])

  const handleRemoveBreakpoint = useCallback((addr: number) => {
    setBreakpoints((prev) => prev.filter((a) => a !== addr))
  }, [])

  // Debug control handlers (to be connected to WASM)
  const handleStep = useCallback(() => {
    console.log('Step')
  }, [])

  const handleStepOver = useCallback(() => {
    console.log('Step Over')
  }, [])

  const handleContinue = useCallback(() => {
    console.log('Continue')
  }, [])

  const handleBreak = useCallback(() => {
    console.log('Break')
  }, [])

  const handleMemoryRead = useCallback((addr: number) => {
    console.log('Read memory at', addr.toString(16))
  }, [])

  return (
    <div className={styles.layout}>
      <Header />
      <Toolbar />
      <main className={styles.main}>
        <div className={styles.emulatorWrapper}>
          <EmulatorCanvas />
        </div>
        {debugVisible && (
          <DebugPanel
            cpu={mockCpu}
            memory={mockMemory}
            disassembly={mockDisassembly}
            breakpoints={breakpoints}
            onStep={handleStep}
            onStepOver={handleStepOver}
            onContinue={handleContinue}
            onBreak={handleBreak}
            onAddBreakpoint={handleAddBreakpoint}
            onRemoveBreakpoint={handleRemoveBreakpoint}
            onMemoryRead={handleMemoryRead}
            onClose={() => setDebugVisible(false)}
            isRunning={!isPaused}
          />
        )}
      </main>
      <footer className={styles.footer}>
        <span>CPCEC by CNGSoft</span>
        <span>•</span>
        <span>Web version</span>
      </footer>
    </div>
  )
}
