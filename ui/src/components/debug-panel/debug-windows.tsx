/**
 * Debug Windows Manager
 * Manages multiple floating debug windows
 */
import { Cross1Icon } from '@radix-ui/react-icons'
import { useCallback, useEffect, useState } from 'react'
import { useDebugState } from '@/hooks/use-debug-state'
import type { CpuState, DisassemblyLine } from '@/types'
import styles from './debug-panel.module.css'
import { DraggableWindow } from './draggable-window'

type WindowId = 'registers' | 'disassembly' | 'memory' | 'stack' | 'breakpoints'

export function DebugWindows() {
  const {
    cpu,
    stack,
    disassembly,
    isPaused,
    step,
    stepOver,
    run,
    pause,
    reset,
    peekMemory,
    refresh
  } = useDebugState(true, 100)

  // Debug logging
  console.log('[DebugWindows] cpu:', !!cpu, 'stack:', stack?.length, 'disasm:', disassembly?.length)

  const [openWindows, setOpenWindows] = useState<Set<WindowId>>(
    new Set(['registers', 'disassembly', 'stack'])
  )

  const [breakpoints, setBreakpoints] = useState<number[]>([])
  const [newBreakpointAddr, setNewBreakpointAddr] = useState('')

  const toggleWindow = useCallback((id: WindowId) => {
    setOpenWindows(prev => {
      const next = new Set(prev)
      if (next.has(id)) {
        next.delete(id)
      } else {
        next.add(id)
      }
      return next
    })
  }, [])

  const closeWindow = useCallback((id: WindowId) => {
    setOpenWindows(prev => {
      const next = new Set(prev)
      next.delete(id)
      return next
    })
  }, [])

  const handleStep = useCallback(() => {
    step()
    setTimeout(refresh, 10)
  }, [step, refresh])

  const handleStepOver = useCallback(() => {
    stepOver()
    setTimeout(refresh, 10)
  }, [stepOver, refresh])

  const handleReset = useCallback(() => {
    reset()
    setTimeout(refresh, 100)
  }, [reset, refresh])

  const handleAddBreakpoint = useCallback(() => {
    const addr = Number.parseInt(newBreakpointAddr, 16)
    if (!Number.isNaN(addr) && addr >= 0 && addr <= 0xffff) {
      setBreakpoints(prev => [...prev.filter(a => a !== addr), addr].sort((a, b) => a - b))
      setNewBreakpointAddr('')
    }
  }, [newBreakpointAddr])

  const handleRemoveBreakpoint = useCallback((addr: number) => {
    setBreakpoints(prev => prev.filter(a => a !== addr))
  }, [])

  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  // Default positions for each window
  const defaultPositions = {
    registers: { x: 20, y: 120 },
    stack: { x: 20, y: 340 },
    disassembly: { x: 220, y: 80 },
    memory: { x: 480, y: 80 },
    breakpoints: { x: 480, y: 350 }
  }

  const windowOptions: { id: WindowId; label: string }[] = [
    { id: 'registers', label: 'Registers' },
    { id: 'stack', label: 'Stack' },
    { id: 'disassembly', label: 'Disassembly' },
    { id: 'memory', label: 'Memory' },
    { id: 'breakpoints', label: 'Breakpoints' }
  ]

  const handleSelectChange = useCallback((e: React.ChangeEvent<HTMLSelectElement>) => {
    const selectedId = e.target.value as WindowId
    if (selectedId) {
      toggleWindow(selectedId)
    }
    // Reset select to placeholder
    e.target.value = ''
  }, [toggleWindow])

  return (
    <>
      {/* Debug Toolbar with Controls */}
      <div className={styles.windowBar}>
        <div className={styles.controlsInline}>
          {isPaused ? (
            <>
              <button className={styles.controlButton} onClick={run} type="button" title="Continue (F5)">
                ▶
              </button>
              <button className={styles.controlButton} onClick={handleStep} type="button" title="Step Into (F11)">
                ↓
              </button>
              <button className={styles.controlButton} onClick={handleStepOver} type="button" title="Step Over (F10)">
                ↷
              </button>
            </>
          ) : (
            <button className={styles.controlButton} onClick={pause} type="button" title="Pause (F6)">
              ⏸
            </button>
          )}
          <button className={styles.controlButton} onClick={handleReset} type="button" title="Reset">
            ⟳
          </button>
          <span className={styles.statusBadge} data-status={isPaused ? 'paused' : 'running'}>
            {isPaused ? 'Paused' : 'Running'}
          </span>
        </div>
        <div className={styles.windowBarSeparator} />
        <select
          className={styles.windowSelect}
          onChange={handleSelectChange}
          defaultValue=""
        >
          <option value="" disabled>
            Windows...
          </option>
          {windowOptions.map(({ id, label }) => (
            <option key={id} value={id}>
              {openWindows.has(id) ? '✓ ' : '   '}{label}
            </option>
          ))}
        </select>
      </div>

      {/* Registers Window */}
      {openWindows.has('registers') && (
        <DraggableWindow
          id="registers"
          title="CPU Registers"
          defaultPosition={defaultPositions.registers}
          onClose={() => closeWindow('registers')}
        >
          {cpu ? (
            <RegistersContent cpu={cpu} />
          ) : (
            <div className={styles.empty}>Waiting for CPU state...</div>
          )}
        </DraggableWindow>
      )}

      {/* Disassembly Window */}
      {openWindows.has('disassembly') && (
        <DraggableWindow
          id="disassembly"
          title="Disassembly"
          defaultPosition={defaultPositions.disassembly}
          onClose={() => closeWindow('disassembly')}
        >
          {disassembly.length > 0 ? (
            <DisassemblyContent
              lines={disassembly}
              currentPc={cpu?.pc ?? 0}
              breakpoints={breakpoints}
              onToggleBreakpoint={(addr) =>
                breakpoints.includes(addr)
                  ? handleRemoveBreakpoint(addr)
                  : setBreakpoints(prev => [...prev, addr])
              }
            />
          ) : (
            <div className={styles.empty}>No disassembly available</div>
          )}
        </DraggableWindow>
      )}

      {/* Stack Window */}
      {openWindows.has('stack') && (
        <DraggableWindow
          id="stack"
          title="Stack"
          defaultPosition={defaultPositions.stack}
          onClose={() => closeWindow('stack')}
        >
          {stack && cpu ? (
            <StackContent stack={stack} sp={cpu.sp} />
          ) : (
            <div className={styles.empty}>No stack data</div>
          )}
        </DraggableWindow>
      )}

      {/* Memory Window */}
      {openWindows.has('memory') && (
        <DraggableWindow
          id="memory"
          title="Memory"
          defaultPosition={defaultPositions.memory}
          onClose={() => closeWindow('memory')}
        >
          <MemoryBrowser peekMemory={peekMemory} initialAddress={cpu?.pc ?? 0} />
        </DraggableWindow>
      )}

      {/* Breakpoints Window */}
      {openWindows.has('breakpoints') && (
        <DraggableWindow
          id="breakpoints"
          title={`Breakpoints (${breakpoints.length})`}
          defaultPosition={defaultPositions.breakpoints}
          onClose={() => closeWindow('breakpoints')}
        >
          <div className={styles.breakpoints}>
            {breakpoints.map((addr) => (
              <div key={addr} className={styles.breakpoint}>
                <span className={styles.breakpointAddress}>
                  ${formatHex(addr, 4)}
                </span>
                <button
                  className={styles.breakpointRemove}
                  onClick={() => handleRemoveBreakpoint(addr)}
                  type='button'
                >
                  <Cross1Icon width={10} height={10} />
                </button>
              </div>
            ))}
            <div className={styles.addBreakpoint}>
              <input
                type='text'
                className={styles.addBreakpointInput}
                placeholder='Address (hex)'
                value={newBreakpointAddr}
                onChange={(e) => setNewBreakpointAddr(e.target.value)}
                onKeyDown={(e) => e.key === 'Enter' && handleAddBreakpoint()}
              />
              <button
                className={styles.controlButton}
                onClick={handleAddBreakpoint}
                type='button'
              >
                Add
              </button>
            </div>
          </div>
        </DraggableWindow>
      )}
    </>
  )
}

// Registers Content Component
function RegistersContent({ cpu }: { cpu: CpuState }) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  const flags = cpu.af & 0xff
  const flagBits = {
    S: !!(flags & 0x80),
    Z: !!(flags & 0x40),
    H: !!(flags & 0x10),
    P: !!(flags & 0x04),
    N: !!(flags & 0x02),
    C: !!(flags & 0x01)
  }

  return (
    <div className={styles.registersCompact}>
      <div className={styles.regRow}>
        <span className={styles.regLabel}>PC</span>
        <span className={styles.regValue}>{formatHex(cpu.pc, 4)}</span>
        <span className={styles.regLabel}>SP</span>
        <span className={styles.regValue}>{formatHex(cpu.sp, 4)}</span>
      </div>
      <div className={styles.regRow}>
        <span className={styles.regLabel}>AF</span>
        <span className={styles.regValue}>{formatHex(cpu.af, 4)}</span>
        <span className={styles.regLabel}>AF'</span>
        <span className={styles.regValueAlt}>{formatHex(cpu.af2, 4)}</span>
      </div>
      <div className={styles.regRow}>
        <span className={styles.regLabel}>BC</span>
        <span className={styles.regValue}>{formatHex(cpu.bc, 4)}</span>
        <span className={styles.regLabel}>BC'</span>
        <span className={styles.regValueAlt}>{formatHex(cpu.bc2, 4)}</span>
      </div>
      <div className={styles.regRow}>
        <span className={styles.regLabel}>DE</span>
        <span className={styles.regValue}>{formatHex(cpu.de, 4)}</span>
        <span className={styles.regLabel}>DE'</span>
        <span className={styles.regValueAlt}>{formatHex(cpu.de2, 4)}</span>
      </div>
      <div className={styles.regRow}>
        <span className={styles.regLabel}>HL</span>
        <span className={styles.regValue}>{formatHex(cpu.hl, 4)}</span>
        <span className={styles.regLabel}>HL'</span>
        <span className={styles.regValueAlt}>{formatHex(cpu.hl2, 4)}</span>
      </div>
      <div className={styles.regRow}>
        <span className={styles.regLabel}>IX</span>
        <span className={styles.regValue}>{formatHex(cpu.ix, 4)}</span>
        <span className={styles.regLabel}>IY</span>
        <span className={styles.regValue}>{formatHex(cpu.iy, 4)}</span>
      </div>
      <div className={styles.flagsRow}>
        {Object.entries(flagBits).map(([name, active]) => (
          <span
            key={name}
            className={`${styles.flagBadge} ${active ? styles.flagActive : ''}`}
          >
            {name}
          </span>
        ))}
        <span className={styles.flagBadge}>IM{cpu.im}</span>
        <span className={`${styles.flagBadge} ${cpu.iff1 ? styles.flagActive : ''}`}>IF1</span>
        <span className={`${styles.flagBadge} ${cpu.iff2 ? styles.flagActive : ''}`}>IF2</span>
      </div>
    </div>
  )
}

// Disassembly Content Component
function DisassemblyContent({
  lines,
  currentPc,
  breakpoints,
  onToggleBreakpoint
}: {
  lines: DisassemblyLine[]
  currentPc: number
  breakpoints: number[]
  onToggleBreakpoint: (addr: number) => void
}) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  return (
    <div className={styles.disassemblyList}>
      {lines.map((line) => {
        const isCurrent = line.isCurrent ?? line.address === currentPc
        const hasBreakpoint = breakpoints.includes(line.address)
        const bytesStr = line.bytes.map(b => formatHex(b, 2)).join(' ')

        return (
          <div
            key={line.address}
            className={`${styles.disasmLine} ${isCurrent ? styles.disasmCurrent : ''} ${hasBreakpoint ? styles.disasmBreakpoint : ''}`}
            onClick={() => onToggleBreakpoint(line.address)}
          >
            <span className={styles.disasmAddr}>{formatHex(line.address, 4)}</span>
            <span className={styles.disasmBytes}>{bytesStr}</span>
            <span className={styles.disasmInstr}>{line.instruction}</span>
          </div>
        )
      })}
    </div>
  )
}

// Stack Content Component
function StackContent({ stack, sp }: { stack: number[]; sp: number }) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  return (
    <div className={styles.stackList}>
      <div className={styles.stackHeader}>
        <span>SP: ${formatHex(sp, 4)}</span>
      </div>
      {stack.map((value, i) => (
        <div key={i} className={`${styles.stackEntry} ${i === 0 ? styles.stackTop : ''}`}>
          <span className={styles.stackOffset}>+{i * 2}</span>
          <span className={styles.stackAddr}>${formatHex((sp + i * 2) & 0xFFFF, 4)}</span>
          <span className={styles.stackValue}>${formatHex(value, 4)}</span>
        </div>
      ))}
    </div>
  )
}

// Memory Browser Component with navigation
function MemoryBrowser({
  peekMemory,
  initialAddress
}: {
  peekMemory: (address: number, length?: number) => { startAddress: number; data: number[] } | null
  initialAddress: number
}) {
  const [address, setAddress] = useState(initialAddress)
  const [addressInput, setAddressInput] = useState('')
  const [memoryData, setMemoryData] = useState<{ startAddress: number; data: number[] } | null>(null)
  const [refreshTrigger, setRefreshTrigger] = useState(0)

  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  // Refresh memory periodically
  useEffect(() => {
    const data = peekMemory(address, 128)
    setMemoryData(data)
  }, [address, peekMemory, refreshTrigger])

  // Auto-refresh every 200ms
  useEffect(() => {
    const interval = setInterval(() => {
      setRefreshTrigger(prev => prev + 1)
    }, 200)
    return () => clearInterval(interval)
  }, [])

  const handleGoTo = () => {
    const addr = Number.parseInt(addressInput, 16)
    if (!Number.isNaN(addr) && addr >= 0 && addr <= 0xFFFF) {
      setAddress(addr)
      setAddressInput('')
    }
  }

  const handlePageUp = () => setAddress((prev) => Math.max(0, prev - 128) & 0xFFFF)
  const handlePageDown = () => setAddress((prev) => (prev + 128) & 0xFFFF)

  const lines: { addr: number; data: number[]; ascii: string }[] = []
  const bytesPerLine = 8

  if (memoryData) {
    for (let i = 0; i < memoryData.data.length; i += bytesPerLine) {
      const lineBytes = memoryData.data.slice(i, i + bytesPerLine)
      const ascii = lineBytes
        .map((b) => (b >= 32 && b < 127 ? String.fromCharCode(b) : '.'))
        .join('')
      lines.push({ addr: (memoryData.startAddress + i) & 0xFFFF, data: lineBytes, ascii })
    }
  }

  return (
    <div className={styles.memoryBrowser}>
      <div className={styles.memoryNav}>
        <button className={styles.memNavButton} onClick={handlePageUp} type="button" title="Page Up">
          ▲
        </button>
        <input
          type="text"
          className={styles.memAddrInput}
          placeholder="Address"
          value={addressInput}
          onChange={(e) => setAddressInput(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && handleGoTo()}
        />
        <button className={styles.memNavButton} onClick={handleGoTo} type="button">
          Go
        </button>
        <button className={styles.memNavButton} onClick={handlePageDown} type="button" title="Page Down">
          ▼
        </button>
      </div>
      <div className={styles.memoryList}>
        {lines.map((line) => (
          <div key={line.addr} className={styles.memLine}>
            <span className={styles.memAddr}>{formatHex(line.addr, 4)}:</span>
            <span className={styles.memBytes}>
              {line.data.map((b, i) => (
                <span key={i}>{formatHex(b, 2)} </span>
              ))}
            </span>
            <span className={styles.memAscii}>{line.ascii}</span>
          </div>
        ))}
      </div>
    </div>
  )
}
