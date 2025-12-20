/**
 * Types for emulator UI elements
 * These replace the SDL UI widgets from cpcec-ox.h
 */

// Menu system types (replaces session_ui_menu)
export interface MenuItem {
  id: number
  label: string
  shortcut?: string
  checked?: boolean
  disabled?: boolean
  separator?: boolean
  radio?: boolean
}

export interface MenuCategory {
  label: string
  items: MenuItem[]
}

export type MenuData = MenuCategory[]

// Dialog types (replaces session_ui_text / session_message)
export interface MessageDialogProps {
  title: string
  message: string
  icon?: 'info' | 'warning' | 'error'
  onClose: () => void
}

// Input dialog types (replaces session_ui_line / session_line)
export interface InputDialogProps {
  title: string
  defaultValue?: string
  placeholder?: string
  onConfirm: (value: string) => void
  onCancel: () => void
}

// List dialog types (replaces session_ui_list / session_list)
export interface ListItem {
  value: string
  label: string
}

export interface ListDialogProps {
  title: string
  items: ListItem[]
  selectedIndex?: number
  onSelect: (index: number, item: ListItem) => void
  onCancel: () => void
}

// File dialog types (replaces session_filedialog)
export interface FileDialogProps {
  title: string
  accept?: string
  multiple?: boolean
  mode: 'open' | 'save'
  onSelect: (files: File[]) => void
  onCancel: () => void
}

// Key scan dialog types (replaces session_ui_scan / session_scan)
export interface KeyScanDialogProps {
  title: string
  prompt: string
  onKeyPress: (keyCode: number) => void
  onCancel: () => void
}

// About dialog
export interface AboutDialogProps {
  title: string
  version: string
  description: string
  onClose: () => void
}

// Debug panel types
export interface CpuState {
  pc: number
  sp: number
  af: number
  bc: number
  de: number
  hl: number
  ix: number
  iy: number
  af2: number
  bc2: number
  de2: number
  hl2: number
  iff1: boolean
  iff2: boolean
  im: number
}

export interface MemoryView {
  startAddress: number
  data: number[]
}

export interface DisassemblyLine {
  address: number
  bytes: number[]
  instruction: string
  isCurrent?: boolean
}

export interface DebugPanelProps {
  cpu: CpuState | null
  memory: MemoryView | null
  disassembly: DisassemblyLine[]
  breakpoints: number[]
  onStep: () => void
  onStepOver: () => void
  onContinue: () => void
  onBreak: () => void
  onAddBreakpoint: (address: number) => void
  onRemoveBreakpoint: (address: number) => void
}

// Emulator events sent from WASM
export type EmulatorUIEvent =
  | { type: 'message'; title: string; message: string; icon?: string }
  | { type: 'input'; title: string; defaultValue?: string }
  | { type: 'list'; title: string; items: string[]; selectedIndex?: number }
  | { type: 'fileOpen'; title: string; filter: string }
  | { type: 'fileSave'; title: string; filter: string; defaultName?: string }
  | { type: 'keyScan'; title: string; prompt: string }
  | { type: 'menu'; visible: boolean }
  | {
      type: 'debug'
      cpu: CpuState
      memory: MemoryView
      disassembly: DisassemblyLine[]
    }
