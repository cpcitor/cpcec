import {
  DndContext,
  closestCenter,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
  type DragEndEvent,
} from "@dnd-kit/core";
import {
  arrayMove,
  SortableContext,
  sortableKeyboardCoordinates,
  useSortable,
  verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { CSS } from "@dnd-kit/utilities";
import {
  ChevronDownIcon,
  ChevronRightIcon,
  Cross1Icon,
  DragHandleDots2Icon,
  DoubleArrowRightIcon,
  PauseIcon,
  PlayIcon,
  TrackNextIcon,
} from "@radix-ui/react-icons";
import { useCallback, useState } from "react";
import type { CpuState, DebugPanelProps, DisassemblyLine } from "@/types";
import styles from "./debug-panel.module.css";

type SectionId = "registers" | "disassembly" | "memory" | "breakpoints";

interface DebugPanelFullProps extends DebugPanelProps {
  onClose: () => void;
  isRunning: boolean;
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
  isRunning,
}: DebugPanelFullProps) {
  const [expandedSections, setExpandedSections] = useState<
    Record<SectionId, boolean>
  >({
    registers: true,
    disassembly: true,
    memory: true,
    breakpoints: true,
  });

  const [sectionOrder, setSectionOrder] = useState<SectionId[]>([
    "registers",
    "disassembly",
    "memory",
    "breakpoints",
  ]);

  const [newBreakpointAddr, setNewBreakpointAddr] = useState("");

  const sensors = useSensors(
    useSensor(PointerSensor, {
      activationConstraint: {
        distance: 5,
      },
    }),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    })
  );

  const toggleSection = useCallback((section: SectionId) => {
    setExpandedSections((prev) => ({
      ...prev,
      [section]: !prev[section],
    }));
  }, []);

  const handleDragEnd = useCallback((event: DragEndEvent) => {
    const { active, over } = event;
    if (over && active.id !== over.id) {
      setSectionOrder((items) => {
        const oldIndex = items.indexOf(active.id as SectionId);
        const newIndex = items.indexOf(over.id as SectionId);
        return arrayMove(items, oldIndex, newIndex);
      });
    }
  }, []);

  const handleAddBreakpoint = useCallback(() => {
    const addr = Number.parseInt(newBreakpointAddr, 16);
    if (!Number.isNaN(addr) && addr >= 0 && addr <= 0xffff) {
      onAddBreakpoint(addr);
      setNewBreakpointAddr("");
    }
  }, [newBreakpointAddr, onAddBreakpoint]);

  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, "0");

  return (
    <div className={styles.panel}>
      <div className={styles.header}>
        <span>Z80 Debugger</span>
        <button
          className={styles.closeButton}
          onClick={onClose}
          type="button"
          aria-label="Close debug panel"
        >
          <Cross1Icon width={14} height={14} />
        </button>
      </div>

      <div className={styles.controls}>
        {isRunning ? (
          <button
            className={styles.controlButton}
            onClick={onBreak}
            type="button"
            title="Break (Pause execution)"
          >
            <PauseIcon width={12} height={12} />
            Break
          </button>
        ) : (
          <>
            <button
              className={styles.controlButton}
              onClick={onContinue}
              type="button"
              title="Continue (F5)"
            >
              <PlayIcon width={12} height={12} />
              Run
            </button>
            <button
              className={styles.controlButton}
              onClick={onStep}
              type="button"
              title="Step Into (F11)"
            >
              <TrackNextIcon width={12} height={12} />
              Step
            </button>
            <button
              className={styles.controlButton}
              onClick={onStepOver}
              type="button"
              title="Step Over (F10)"
            >
              <DoubleArrowRightIcon width={12} height={12} />
              Over
            </button>
          </>
        )}
      </div>

      <div className={styles.content}>
        <DndContext
          sensors={sensors}
          collisionDetection={closestCenter}
          onDragEnd={handleDragEnd}
        >
          <SortableContext
            items={sectionOrder}
            strategy={verticalListSortingStrategy}
          >
            {sectionOrder.map((sectionId) => {
              const sectionConfig = {
                registers: {
                  title: "CPU Registers",
                  content: cpu ? (
                    <RegistersView cpu={cpu} />
                  ) : (
                    <div className={styles.empty}>Waiting for CPU state...</div>
                  ),
                },
                disassembly: {
                  title: "Disassembly",
                  content:
                    disassembly.length > 0 ? (
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
                      <div className={styles.empty}>
                        No disassembly available
                      </div>
                    ),
                },
                memory: {
                  title: "Memory",
                  content: memory ? (
                    <MemoryView
                      startAddress={memory.startAddress}
                      data={memory.data}
                    />
                  ) : (
                    <div className={styles.empty}>No memory view</div>
                  ),
                },
                breakpoints: {
                  title: `Breakpoints (${breakpoints.length})`,
                  content: (
                    <div className={styles.breakpoints}>
                      {breakpoints.map((addr) => (
                        <div key={addr} className={styles.breakpoint}>
                          <span className={styles.breakpointAddress}>
                            ${formatHex(addr, 4)}
                          </span>
                          <button
                            className={styles.breakpointRemove}
                            onClick={() => onRemoveBreakpoint(addr)}
                            type="button"
                            aria-label={`Remove breakpoint at ${formatHex(
                              addr,
                              4
                            )}`}
                          >
                            <Cross1Icon width={10} height={10} />
                          </button>
                        </div>
                      ))}
                      <div className={styles.addBreakpoint}>
                        <input
                          type="text"
                          className={styles.addBreakpointInput}
                          placeholder="Address (hex)"
                          value={newBreakpointAddr}
                          onChange={(e) => setNewBreakpointAddr(e.target.value)}
                          onKeyDown={(e) =>
                            e.key === "Enter" && handleAddBreakpoint()
                          }
                        />
                        <button
                          className={styles.controlButton}
                          onClick={handleAddBreakpoint}
                          type="button"
                        >
                          Add
                        </button>
                      </div>
                    </div>
                  ),
                },
              };

              const config = sectionConfig[sectionId];
              return (
                <SortableSection
                  key={sectionId}
                  id={sectionId}
                  title={config.title}
                  expanded={expandedSections[sectionId]}
                  onToggle={() => toggleSection(sectionId)}
                >
                  {config.content}
                </SortableSection>
              );
            })}
          </SortableContext>
        </DndContext>
      </div>
    </div>
  );
}

// Sortable Section Component
interface SortableSectionProps {
  id: string;
  title: string;
  expanded: boolean;
  onToggle: () => void;
  children: React.ReactNode;
}

function SortableSection({
  id,
  title,
  expanded,
  onToggle,
  children,
}: SortableSectionProps) {
  const {
    attributes,
    listeners,
    setNodeRef,
    transform,
    transition,
    isDragging,
  } = useSortable({ id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
    opacity: isDragging ? 0.5 : 1,
    zIndex: isDragging ? 1000 : "auto",
  };

  return (
    <div ref={setNodeRef} style={style} className={styles.section}>
      <div className={styles.sectionHeader}>
        <button
          className={styles.dragHandle}
          {...attributes}
          {...listeners}
          type="button"
          aria-label="Drag to reorder"
        >
          <DragHandleDots2Icon width={12} height={12} />
        </button>
        <div
          className={styles.sectionTitle}
          onClick={onToggle}
          onKeyDown={(e) => e.key === "Enter" && onToggle()}
          role="button"
          tabIndex={0}
        >
          <span>{title}</span>
          {expanded ? (
            <ChevronDownIcon width={14} height={14} />
          ) : (
            <ChevronRightIcon width={14} height={14} />
          )}
        </div>
      </div>
      {expanded && <div className={styles.sectionContent}>{children}</div>}
    </div>
  );
}

function RegistersView({ cpu }: { cpu: CpuState }) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, "0");

  // Extract flags from AF register
  const flags = cpu.af & 0xff;
  const flagBits = {
    S: !!(flags & 0x80),
    Z: !!(flags & 0x40),
    H: !!(flags & 0x10),
    P: !!(flags & 0x04),
    N: !!(flags & 0x02),
    C: !!(flags & 0x01),
  };

  // Helper to render a register pair (16-bit = 2x8-bit)
  const RegPair = ({
    name,
    value,
    altName,
    altValue,
  }: {
    name: string;
    value: number;
    altName?: string;
    altValue?: number;
  }) => {
    const hi = (value >> 8) & 0xff;
    const lo = value & 0xff;
    const hiName = name[0];
    const loName = name[1] || name[0];

    return (
      <div className={styles.registerPair}>
        <div className={styles.registerPairMain}>
          <span className={styles.registerPairName}>{name}</span>
          <span className={styles.registerPairValue}>
            {formatHex(value, 4)}
          </span>
          <span className={styles.registerPairBytes}>
            <span className={styles.registerByte}>
              <span className={styles.byteName}>{hiName}</span>
              <span className={styles.byteValue}>{formatHex(hi, 2)}</span>
            </span>
            <span className={styles.registerByte}>
              <span className={styles.byteName}>{loName}</span>
              <span className={styles.byteValue}>{formatHex(lo, 2)}</span>
            </span>
          </span>
        </div>
        {altName !== undefined && altValue !== undefined && (
          <div className={styles.registerPairAlt}>
            <span className={styles.registerPairName}>{altName}</span>
            <span className={styles.registerPairValue}>
              {formatHex(altValue, 4)}
            </span>
          </div>
        )}
      </div>
    );
  };

  return (
    <>
      {/* Program Counter & Stack Pointer */}
      <div className={styles.registerRow}>
        <div className={styles.registerSingle}>
          <span className={styles.registerName}>PC</span>
          <span className={styles.registerValue}>{formatHex(cpu.pc, 4)}</span>
        </div>
        <div className={styles.registerSingle}>
          <span className={styles.registerName}>SP</span>
          <span className={styles.registerValue}>{formatHex(cpu.sp, 4)}</span>
        </div>
      </div>

      {/* Main registers with alternates */}
      <div className={styles.registerPairs}>
        <RegPair name="AF" value={cpu.af} altName="AF'" altValue={cpu.af2} />
        <RegPair name="BC" value={cpu.bc} altName="BC'" altValue={cpu.bc2} />
        <RegPair name="DE" value={cpu.de} altName="DE'" altValue={cpu.de2} />
        <RegPair name="HL" value={cpu.hl} altName="HL'" altValue={cpu.hl2} />
      </div>

      {/* Index registers */}
      <div className={styles.registerRow}>
        <div className={styles.registerSingle}>
          <span className={styles.registerName}>IX</span>
          <span className={styles.registerValue}>{formatHex(cpu.ix, 4)}</span>
        </div>
        <div className={styles.registerSingle}>
          <span className={styles.registerName}>IY</span>
          <span className={styles.registerValue}>{formatHex(cpu.iy, 4)}</span>
        </div>
      </div>

      {/* Flags */}
      <div className={styles.flags}>
        {Object.entries(flagBits).map(([name, active]) => (
          <div
            key={name}
            className={`${styles.flag} ${active ? styles.flagActive : ""}`}
            title={`${name} flag: ${active ? "set" : "clear"}`}
          >
            {name}
          </div>
        ))}
        <div className={styles.flag} title={`IM: ${cpu.im}`}>
          IM{cpu.im}
        </div>
        <div
          className={`${styles.flag} ${cpu.iff1 ? styles.flagActive : ""}`}
          title={`IFF1: ${cpu.iff1 ? "enabled" : "disabled"}`}
        >
          IF1
        </div>
        <div
          className={`${styles.flag} ${cpu.iff2 ? styles.flagActive : ""}`}
          title={`IFF2: ${cpu.iff2 ? "enabled" : "disabled"}`}
        >
          IF2
        </div>
      </div>
    </>
  );
}

function DisassemblyView({
  lines,
  currentPc,
  breakpoints,
  onToggleBreakpoint,
}: {
  lines: DisassemblyLine[];
  currentPc: number;
  breakpoints: number[];
  onToggleBreakpoint: (addr: number) => void;
}) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, "0");

  return (
    <div className={styles.disassembly}>
      {lines.map((line) => {
        const isCurrent = line.isCurrent ?? line.address === currentPc;
        const hasBreakpoint = breakpoints.includes(line.address);
        const bytesStr = line.bytes.map((b) => formatHex(b, 2)).join(" ");

        return (
          <div
            key={line.address}
            className={`${styles.disasmLine} ${
              isCurrent ? styles.disasmLineCurrent : ""
            } ${
              hasBreakpoint && !isCurrent ? styles.disasmLineBreakpoint : ""
            }`}
            onClick={() => onToggleBreakpoint(line.address)}
            onKeyDown={(e) =>
              e.key === "Enter" && onToggleBreakpoint(line.address)
            }
            role="button"
            tabIndex={0}
          >
            <span className={styles.disasmAddress}>
              {formatHex(line.address, 4)}
            </span>
            <span className={styles.disasmBytes}>{bytesStr}</span>
            <span className={styles.disasmMnemonic}>{line.instruction}</span>
          </div>
        );
      })}
    </div>
  );
}

function MemoryView({
  startAddress,
  data,
}: {
  startAddress: number;
  data: number[];
}) {
  const formatHex = (value: number, digits: number) =>
    value.toString(16).toUpperCase().padStart(digits, "0");

  const lines: { addr: number; data: number[]; ascii: string }[] = [];
  const bytesPerLine = 8;

  for (let i = 0; i < data.length; i += bytesPerLine) {
    const lineBytes = data.slice(i, i + bytesPerLine);
    const ascii = lineBytes
      .map((b) => (b >= 32 && b < 127 ? String.fromCharCode(b) : "."))
      .join("");
    lines.push({
      addr: startAddress + i,
      data: lineBytes,
      ascii,
    });
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
  );
}
