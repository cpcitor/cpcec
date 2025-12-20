import {
  ChevronDownIcon,
  ChevronRightIcon,
  Cross1Icon,
  DoubleArrowRightIcon,
  PauseIcon,
  PlayIcon,
  TrackNextIcon
} from '@radix-ui/react-icons'
import { useCallback, useState } from 'react'
import type { CpuState, DebugPanelProps, DisassemblyLine } from '@/types'
import styles from './debug-panel.module.css'

interface DebugPanelFullProps extends DebugPanelProps {
  onClose: () => void
  isRunning: boolean
}

export function DebugPanel({
  cpu,
  memory,
  disassembly,
  breakpoints,
  onStep,
  onStepOver,
  onContinue,
  onBreak,
  onAddBreakpoint,
  onRemoveBreakpoint,
  onClose,
  isRunning
}: DebugPanelFullProps) {
  const [expandedSections, setExpandedSections] = useState({
    registers: true,
    disassembly: true,
    memory: true,
    breakpoints: true
  })

  const [newBreakpointAddr, setNewBreakpointAddr] = useState('')

  const toggleSection = useCallback(
    (section: keyof typeof expandedSections) => {
      setExpandedSections((prev) => ({
        ...prev,
        [section]: !prev[section]
      }))
    },
    []
  )

  const handleAddBreakpoint = useCallback(() => {
    const addr = Number.parseInt(newBreakpointAddr, 16)
    if (!Number.isNaN(addr) && addr >= 0 && addr <= 0xffff) {
      onAddBreakpoint(addr)
      setNewBreakpointAddr('')
    }
  }, [newBreakpointAddr, onAddBreakpoint])

  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  return (
    <div className={styles.panel}>
      <div className={styles.header}>
        <span>Z80 Debugger</span>
        <button
          className={styles.closeButton}
          onClick={onClose}
          type='button'
          aria-label='Close debug panel'
        >
          <Cross1Icon width={14} height={14} />
        </button>
      </div>

      <div className={styles.controls}>
        {isRunning ? (
          <button
            className={styles.controlButton}
            onClick={onBreak}
            type='button'
            title='Break (Pause execution)'
          >
            <PauseIcon width={12} height={12} />
            Break
          </button>
        ) : (
          <>
            <button
              className={styles.controlButton}
              onClick={onContinue}
              type='button'
              title='Continue (F5)'
            >
              <PlayIcon width={12} height={12} />
              Run
            </button>
            <button
              className={styles.controlButton}
              onClick={onStep}
              type='button'
              title='Step Into (F11)'
            >
              <TrackNextIcon width={12} height={12} />
              Step
            </button>
            <button
              className={styles.controlButton}
              onClick={onStepOver}
              type='button'
              title='Step Over (F10)'
            >
              <DoubleArrowRightIcon width={12} height={12} />
              Over
            </button>
          </>
        )}
      </div>

      <div className={styles.content}>
        {/* Registers Section */}
        <div className={styles.section}>
          <div
            className={styles.sectionHeader}
            onClick={() => toggleSection('registers')}
            onKeyDown={(e) => e.key === 'Enter' && toggleSection('registers')}
            role='button'
            tabIndex={0}
          >
            <span>CPU Registers</span>
            {expandedSections.registers ? (
              <ChevronDownIcon width={14} height={14} />
            ) : (
              <ChevronRightIcon width={14} height={14} />
            )}
          </div>
          {expandedSections.registers && (
            <div className={styles.sectionContent}>
              {cpu ? (
                <RegistersView cpu={cpu} />
              ) : (
                <div className={styles.empty}>Waiting for CPU state...</div>
              )}
            </div>
          )}
        </div>

        {/* Disassembly Section */}
        <div className={styles.section}>
          <div
            className={styles.sectionHeader}
            onClick={() => toggleSection('disassembly')}
            onKeyDown={(e) => e.key === 'Enter' && toggleSection('disassembly')}
            role='button'
            tabIndex={0}
          >
            <span>Disassembly</span>
            {expandedSections.disassembly ? (
              <ChevronDownIcon width={14} height={14} />
            ) : (
              <ChevronRightIcon width={14} height={14} />
            )}
          </div>
          {expandedSections.disassembly && (
            <div className={styles.sectionContent}>
              {disassembly.length > 0 ? (
                <DisassemblyView
                  lines={disassembly}
                  currentPc={cpu?.pc ?? 0}
                  breakpoints={breakpoints}
                  onToggleBreakpoint={(addr) =>
                    breakpoints.includes(addr)
                      ? onRemoveBreakpoint(addr)
                      : onAddBreakpoint(addr)
                  }
                />
              ) : (
                <div className={styles.empty}>No disassembly available</div>
              )}
            </div>
          )}
        </div>

        {/* Memory Section */}
        <div className={styles.section}>
          <div
            className={styles.sectionHeader}
            onClick={() => toggleSection('memory')}
            onKeyDown={(e) => e.key === 'Enter' && toggleSection('memory')}
            role='button'
            tabIndex={0}
          >
            <span>Memory</span>
            {expandedSections.memory ? (
              <ChevronDownIcon width={14} height={14} />
            ) : (
              <ChevronRightIcon width={14} height={14} />
            )}
          </div>
          {expandedSections.memory && (
            <div className={styles.sectionContent}>
              {memory ? (
                <MemoryView address={memory.address} bytes={memory.bytes} />
              ) : (
                <div className={styles.empty}>No memory view</div>
              )}
            </div>
          )}
        </div>

        {/* Breakpoints Section */}
        <div className={styles.section}>
          <div
            className={styles.sectionHeader}
            onClick={() => toggleSection('breakpoints')}
            onKeyDown={(e) => e.key === 'Enter' && toggleSection('breakpoints')}
            role='button'
            tabIndex={0}
          >
            <span>Breakpoints ({breakpoints.length})</span>
            {expandedSections.breakpoints ? (
              <ChevronDownIcon width={14} height={14} />
            ) : (
              <ChevronRightIcon width={14} height={14} />
            )}
          </div>
          {expandedSections.breakpoints && (
            <div className={styles.sectionContent}>
              <div className={styles.breakpoints}>
                {breakpoints.map((addr) => (
                  <div key={addr} className={styles.breakpoint}>
                    <span className={styles.breakpointAddress}>
                      ${formatHex(addr, 4)}
                    </span>
                    <button
                      className={styles.breakpointRemove}
                      onClick={() => onRemoveBreakpoint(addr)}
                      type='button'
                      aria-label={`Remove breakpoint at ${formatHex(addr, 4)}`}
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
                    onKeyDown={(e) =>
                      e.key === 'Enter' && handleAddBreakpoint()
                    }
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
            </div>
          )}
        </div>
      </div>
    </div>
  )
}

function RegistersView({ cpu }: { cpu: CpuState }) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  // Extract flags from AF register
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
    <>
      <div className={styles.registers}>
        <div className={`${styles.register} ${styles.registerWide}`}>
          <span className={styles.registerName}>PC</span>
          <span className={styles.registerValue}>{formatHex(cpu.pc, 4)}</span>
        </div>
        <div className={`${styles.register} ${styles.registerWide}`}>
          <span className={styles.registerName}>SP</span>
          <span className={styles.registerValue}>{formatHex(cpu.sp, 4)}</span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>A</span>
          <span className={styles.registerValue}>
            {formatHex((cpu.af >> 8) & 0xff, 2)}
          </span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>F</span>
          <span className={styles.registerValue}>
            {formatHex(cpu.af & 0xff, 2)}
          </span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>B</span>
          <span className={styles.registerValue}>
            {formatHex((cpu.bc >> 8) & 0xff, 2)}
          </span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>C</span>
          <span className={styles.registerValue}>
            {formatHex(cpu.bc & 0xff, 2)}
          </span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>D</span>
          <span className={styles.registerValue}>
            {formatHex((cpu.de >> 8) & 0xff, 2)}
          </span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>E</span>
          <span className={styles.registerValue}>
            {formatHex(cpu.de & 0xff, 2)}
          </span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>H</span>
          <span className={styles.registerValue}>
            {formatHex((cpu.hl >> 8) & 0xff, 2)}
          </span>
        </div>
        <div className={styles.register}>
          <span className={styles.registerName}>L</span>
          <span className={styles.registerValue}>
            {formatHex(cpu.hl & 0xff, 2)}
          </span>
        </div>
        <div className={`${styles.register} ${styles.registerWide}`}>
          <span className={styles.registerName}>IX</span>
          <span className={styles.registerValue}>{formatHex(cpu.ix, 4)}</span>
        </div>
        <div className={`${styles.register} ${styles.registerWide}`}>
          <span className={styles.registerName}>IY</span>
          <span className={styles.registerValue}>{formatHex(cpu.iy, 4)}</span>
        </div>
      </div>
      <div className={styles.flags} style={{ marginTop: 'var(--spacing-sm)' }}>
        {Object.entries(flagBits).map(([name, active]) => (
          <div
            key={name}
            className={`${styles.flag} ${active ? styles.flagActive : ''}`}
            title={`${name} flag: ${active ? 'set' : 'clear'}`}
          >
            {name}
          </div>
        ))}
        <div className={styles.flag} title={`IM: ${cpu.im}`}>
          {cpu.im}
        </div>
        <div
          className={`${styles.flag} ${cpu.iff1 ? styles.flagActive : ''}`}
          title={`IFF1: ${cpu.iff1 ? 'enabled' : 'disabled'}`}
        >
          I1
        </div>
        <div
          className={`${styles.flag} ${cpu.iff2 ? styles.flagActive : ''}`}
          title={`IFF2: ${cpu.iff2 ? 'enabled' : 'disabled'}`}
        >
          I2
        </div>
      </div>
    </>
  )
}

function DisassemblyView({
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
    <div className={styles.disassembly}>
      {lines.map((line) => {
        const isCurrent = line.address === currentPc
        const hasBreakpoint = breakpoints.includes(line.address)

        return (
          <div
            key={line.address}
            className={`${styles.disasmLine} ${
              isCurrent ? styles.disasmLineCurrent : ''
            } ${
              hasBreakpoint && !isCurrent ? styles.disasmLineBreakpoint : ''
            }`}
            onClick={() => onToggleBreakpoint(line.address)}
            onKeyDown={(e) =>
              e.key === 'Enter' && onToggleBreakpoint(line.address)
            }
            role='button'
            tabIndex={0}
          >
            <span className={styles.disasmAddress}>
              {formatHex(line.address, 4)}
            </span>
            <span className={styles.disasmBytes}>{line.bytes}</span>
            <span className={styles.disasmMnemonic}>
              {line.mnemonic} {line.operands}
            </span>
          </div>
        )
      })}
    </div>
  )
}

function MemoryView({
  address,
  bytes
}: {
  address: number
  bytes: Uint8Array
}) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, '0')

  const lines: { addr: number; data: number[]; ascii: string }[] = []
  const bytesPerLine = 8

  for (let i = 0; i < bytes.length; i += bytesPerLine) {
    const lineBytes = Array.from(bytes.slice(i, i + bytesPerLine))
    const ascii = lineBytes
      .map((b) => (b >= 32 && b < 127 ? String.fromCharCode(b) : '.'))
      .join('')
    lines.push({
      addr: address + i,
      data: lineBytes,
      ascii
    })
  }

  return (
    <div className={styles.memoryView}>
      {lines.map((line) => (
        <div key={line.addr} className={styles.memoryLine}>
          <span className={styles.memoryAddress}>
            {formatHex(line.addr, 4)}:
          </span>
          <span className={styles.memoryBytes}>
            {line.data.map((b, i) => (
              <span key={i} className={styles.memoryByte}>
                {formatHex(b, 2)}
              </span>
            ))}
          </span>
          <span className={styles.memoryAscii}>{line.ascii}</span>
        </div>
      ))}
    </div>
  )
}
