import { atom } from 'jotai'
import type {
  AboutDialogProps,
  FileDialogProps,
  InputDialogProps,
  KeyScanDialogProps,
  ListDialogProps,
  MessageDialogProps
} from '@/types'

// Dialog state atoms
type DialogType =
  | { type: 'message'; props: Omit<MessageDialogProps, 'onClose'> }
  | {
      type: 'input'
      props: Omit<InputDialogProps, 'onConfirm' | 'onCancel'>
      resolve: (value: string | null) => void
    }
  | {
      type: 'list'
      props: Omit<ListDialogProps, 'onSelect' | 'onCancel'>
      resolve: (index: number) => void
    }
  | {
      type: 'file'
      props: Omit<FileDialogProps, 'onSelect' | 'onCancel'>
      resolve: (files: File[] | null) => void
    }
  | {
      type: 'keyScan'
      props: Omit<KeyScanDialogProps, 'onKeyPress' | 'onCancel'>
      resolve: (keyCode: number | null) => void
    }
  | { type: 'about'; props: Omit<AboutDialogProps, 'onClose'> }
  | null

export const activeDialogAtom = atom<DialogType>(null)

// Debug panel state
export const debugPanelVisibleAtom = atom(false)
