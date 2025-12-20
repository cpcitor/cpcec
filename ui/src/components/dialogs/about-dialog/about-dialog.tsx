import { Button } from '@/components/ui/button'
import { Dialog } from '@/components/ui/dialog'
import type { AboutDialogProps } from '@/types'
import styles from './about-dialog.module.css'

export function AboutDialog({
  title,
  version,
  description,
  onClose
}: AboutDialogProps) {
  return (
    <Dialog
      title={title}
      open={true}
      onClose={onClose}
      width={350}
      footer={
        <Button onClick={onClose} variant='primary'>
          OK
        </Button>
      }
    >
      <div className={styles.about}>
        <div className={styles.logo}>💾</div>
        <div className={styles.name}>CPCEC</div>
        <div className={styles.version}>Version {version}</div>
        <div className={styles.description}>{description}</div>
        <div className={styles.credits}>
          By{' '}
          <a
            href='mailto:cngsoft@gmail.com'
            className={styles.link}
            target='_blank'
            rel='noopener noreferrer'
          >
            Cesar Nicolas-Gonzalez
          </a>
          <br />
          React UI by pixsaur
        </div>
      </div>
    </Dialog>
  )
}
