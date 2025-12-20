import { useAtom } from 'jotai'
import { useCallback } from 'react'
import { activeDialogAtom } from '@/store/dialog-atoms'
import type { ListItem } from '@/types'

/**
 * Hook to manage UI dialogs (replacements for SDL UI)
 */
export function useDialogs() {
  const [activeDialog, setActiveDialog] = useAtom(activeDialogAtom)

  const showMessage = useCallback(
    (
      title: string,
      message: string,
      icon?: 'info' | 'warning' | 'error'
    ): Promise<void> => {
      return new Promise((resolve) => {
        setActiveDialog({
          type: 'message',
          props: { title, message, icon }
        })
        // Message dialogs resolve when closed
        const checkClosed = setInterval(() => {
          // We'll handle closing in the dialog itself
        }, 100)
        // For simplicity, we resolve immediately and let the dialog handle closing
        resolve()
        clearInterval(checkClosed)
      })
    },
    [setActiveDialog]
  )

  const showInput = useCallback(
    (
      title: string,
      defaultValue?: string,
      placeholder?: string
    ): Promise<string | null> => {
      return new Promise((resolve) => {
        setActiveDialog({
          type: 'input',
          props: { title, defaultValue, placeholder },
          resolve
        })
      })
    },
    [setActiveDialog]
  )

  const showList = useCallback(
    (
      title: string,
      items: ListItem[],
      selectedIndex?: number
    ): Promise<number> => {
      return new Promise((resolve) => {
        setActiveDialog({
          type: 'list',
          props: { title, items, selectedIndex },
          resolve
        })
      })
    },
    [setActiveDialog]
  )

  const showFileOpen = useCallback(
    (
      title: string,
      accept?: string,
      multiple?: boolean
    ): Promise<File[] | null> => {
      return new Promise((resolve) => {
        setActiveDialog({
          type: 'file',
          props: { title, accept, multiple, mode: 'open' },
          resolve
        })
      })
    },
    [setActiveDialog]
  )

  const showKeyScan = useCallback(
    (title: string, prompt: string): Promise<number | null> => {
      return new Promise((resolve) => {
        setActiveDialog({
          type: 'keyScan',
          props: { title, prompt },
          resolve
        })
      })
    },
    [setActiveDialog]
  )

  const showAbout = useCallback(
    (title: string, version: string, description: string): void => {
      setActiveDialog({
        type: 'about',
        props: { title, version, description }
      })
    },
    [setActiveDialog]
  )

  const closeDialog = useCallback(() => {
    setActiveDialog(null)
  }, [setActiveDialog])

  return {
    activeDialog,
    showMessage,
    showInput,
    showList,
    showFileOpen,
    showKeyScan,
    showAbout,
    closeDialog
  }
}
