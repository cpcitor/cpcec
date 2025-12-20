/**
 * Bridge between WASM emulator UI callbacks and React dialogs
 *
 * NON-BLOCKING approach:
 * - Messages: fire-and-forget (dialog shows, emulator continues)
 * - Inputs/Lists/Files: emulator is PAUSED via session_signal, 
 *   and resumes when user responds via em_dialog_complete()
 */

import { appStore } from '@/store'
import { activeDialogAtom } from '@/store/dialog-atoms'
import type { ListItem } from '@/types'

/**
 * Resolve the current dialog - closes it
 * For messages: just closes the dialog (no callback to WASM needed)
 * For inputs/lists/files: the callback in the EM_JS handles resuming
 */
export function resolveDialog(result: unknown): void {
  console.log('[UIBridge] resolveDialog called with:', result)
  
  // Clear dialog immediately for UI responsiveness
  appStore.set(activeDialogAtom, null)
  
  // Re-focus the emulator so it gets keyboard input again
  requestAnimationFrame(() => {
    const focusTarget = document.getElementById('emulator-focus-target')
    if (focusTarget) {
      focusTarget.focus()
    }
  })
}

/**
 * Set up Module callbacks for WASM UI functions
 * Call this after the emulator is initialized
 */
export function setupEmulatorUIBridge(): void {
  const Module = (globalThis as typeof globalThis & { Module?: EmulatorModule }).Module
  if (!Module) {
    console.warn('[UIBridge] Module not available yet')
    return
  }

  console.log('[UIBridge] Setting up emulator UI callbacks (non-blocking mode)')

  /**
   * Show a message dialog (from session_message/session_aboutme)
   * Fire-and-forget: just show the dialog, callback immediately (no blocking)
   */
  Module.onShowMessage = (
    message: string,
    title: string,
    isAbout: number,
    callback: () => void
  ) => {
    console.log('[UIBridge] onShowMessage:', title, message, isAbout)

    if (isAbout) {
      appStore.set(activeDialogAtom, {
        type: 'about',
        props: {
          title,
          version: '',
          description: message
        }
      })
    } else {
      appStore.set(activeDialogAtom, {
        type: 'message',
        props: {
          title,
          message,
          icon: 'info'
        }
      })
    }
    
    // For messages, callback is just acknowledgement - call it immediately
    // The dialog stays open until user dismisses it, but emulator continues
    callback()
  }

  /**
   * Show an input dialog (from session_line)
   * Emulator is paused via emscripten_sleep - callback resumes it
   */
  Module.onShowInput = (
    title: string,
    currentValue: string,
    callback: (result: string | null) => void
  ) => {
    console.log('[UIBridge] onShowInput:', title, currentValue)

    appStore.set(activeDialogAtom, {
      type: 'input',
      props: {
        title,
        defaultValue: currentValue,
        placeholder: ''
      },
      resolve: (result: string | null) => {
        appStore.set(activeDialogAtom, null)
        callback(result)
      }
    })
  }

  /**
   * Show a list selection dialog (from session_list)
   * Emulator is paused via emscripten_sleep - callback resumes it
   */
  Module.onShowList = (
    title: string,
    items: string[],
    defaultItem: number,
    callback: (result: number | null) => void
  ) => {
    console.log('[UIBridge] onShowList:', title, items, defaultItem)

    const listItems: ListItem[] = items.map((item, index) => ({
      value: String(index),
      label: item
    }))

    appStore.set(activeDialogAtom, {
      type: 'list',
      props: {
        title,
        items: listItems,
        selectedIndex: defaultItem
      },
      resolve: (result: number | null) => {
        appStore.set(activeDialogAtom, null)
        callback(result)
      }
    })
  }

  /**
   * Show a key scan dialog (from session_scan)
   * Emulator is paused via emscripten_sleep - callback resumes it
   */
  Module.onShowKeyScan = (
    eventName: string,
    callback: (keyCode: number | null) => void
  ) => {
    console.log('[UIBridge] onShowKeyScan:', eventName)

    appStore.set(activeDialogAtom, {
      type: 'keyScan',
      props: {
        title: 'Key Configuration',
        prompt: `Press a key for: ${eventName}`
      },
      resolve: (keyCode: number | null) => {
        appStore.set(activeDialogAtom, null)
        callback(keyCode)
      }
    })
  }

  /**
   * Show a file dialog (from session_getfile/session_newfile)
   * Emulator is paused via emscripten_sleep - callback resumes it
   */
  Module.onShowFileDialog = (
    title: string,
    pattern: string,
    isOpen: number,
    _showReadOnly: number,
    callback: (result: string | null) => void
  ) => {
    console.log('[UIBridge] onShowFileDialog:', title, pattern, isOpen)

    const accept = pattern
      .split(';')
      .map((p) => p.replace('*', ''))
      .join(',')

    appStore.set(activeDialogAtom, {
      type: 'file',
      props: {
        title,
        accept,
        multiple: false,
        mode: isOpen ? 'open' : 'save'
      },
      resolve: (files: File[] | null) => {
        appStore.set(activeDialogAtom, null)
        if (files && files.length > 0) {
          callback(files[0].name)
        } else {
          callback(null)
        }
      }
    })
  }

  console.log('[UIBridge] Emulator UI callbacks set up successfully')
}

/**
 * Types for the emulator Module with UI callbacks
 */
interface EmulatorModule {
  _em_dialog_complete?: (result: number) => void
  _em_dialog_complete_string?: (result: number, strPtr: number) => void
  allocateUTF8?: (str: string) => number
  onShowMessage?: (
    message: string,
    title: string,
    isAbout: number,
    callback: () => void
  ) => void
  onShowInput?: (
    title: string,
    currentValue: string,
    callback: (result: string | null) => void
  ) => void
  onShowList?: (
    title: string,
    items: string[],
    defaultItem: number,
    callback: (result: number | null) => void
  ) => void
  onShowKeyScan?: (
    eventName: string,
    callback: (keyCode: number | null) => void
  ) => void
  onShowFileDialog?: (
    title: string,
    pattern: string,
    isOpen: number,
    showReadOnly: number,
    callback: (result: string | null) => void
  ) => void
}
