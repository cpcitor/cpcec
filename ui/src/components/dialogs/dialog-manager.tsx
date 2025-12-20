import { useAtomValue } from 'jotai'
import {
  AboutDialog,
  FileDialog,
  InputDialog,
  KeyScanDialog,
  ListDialog,
  MessageDialog
} from '@/components/dialogs'
import { resolveDialog } from '@/lib/emulator-ui-bridge'
import { activeDialogAtom } from '@/store/dialog-atoms'

/**
 * Dialog Manager - renders the currently active dialog
 * This replaces all SDL UI windows (session_ui_*)
 */
export function DialogManager() {
  const activeDialog = useAtomValue(activeDialogAtom)

  if (!activeDialog) return null

  switch (activeDialog.type) {
    case 'message':
      return (
        <MessageDialog
          {...activeDialog.props}
          onClose={() => {
            resolveDialog(null)
          }}
        />
      )

    case 'input':
      return (
        <InputDialog
          {...activeDialog.props}
          onConfirm={(value) => {
            resolveDialog(value)
          }}
          onCancel={() => {
            resolveDialog(null)
          }}
        />
      )

    case 'list':
      return (
        <ListDialog
          {...activeDialog.props}
          onSelect={(index) => {
            resolveDialog(index)
          }}
          onCancel={() => {
            resolveDialog(-1)
          }}
        />
      )

    case 'file':
      return (
        <FileDialog
          {...activeDialog.props}
          onSelect={(files) => {
            resolveDialog(files)
          }}
          onCancel={() => {
            resolveDialog(null)
          }}
        />
      )

    case 'keyScan':
      return (
        <KeyScanDialog
          {...activeDialog.props}
          onKeyPress={(keyCode) => {
            resolveDialog(keyCode)
          }}
          onCancel={() => {
            resolveDialog(null)
          }}
        />
      )

    case 'about':
      return (
        <AboutDialog
          {...activeDialog.props}
          onClose={() => {
            resolveDialog(null)
          }}
        />
      )

    default:
      return null
  }
}
