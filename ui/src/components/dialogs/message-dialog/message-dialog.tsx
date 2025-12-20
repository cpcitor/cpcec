import { Button } from '@/components/ui/button'
import { Dialog } from '@/components/ui/dialog'
import type { MessageDialogProps } from '@/types'
import styles from './message-dialog.module.css'

export function MessageDialog({
  title,
  message,
  icon = 'info',
  onClose
}: MessageDialogProps) {
  return (
    <Dialog
      title={title}
      icon={icon}
      open={true}
      onClose={onClose}
      footer={
        <Button onClick={onClose} variant='primary'>
          OK
        </Button>
      }
    >
      <div className={styles.message}>{message}</div>
    </Dialog>
  )
}
