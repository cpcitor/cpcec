import {
  Cross1Icon,
  CrossCircledIcon,
  ExclamationTriangleIcon,
  InfoCircledIcon
} from '@radix-ui/react-icons'
import type { ReactNode } from 'react'
import { useCallback, useEffect, useRef } from 'react'
import styles from './dialog.module.css'

export interface DialogProps {
  title: string
  icon?: 'info' | 'warning' | 'error'
  open: boolean
  onClose: () => void
  children: ReactNode
  footer?: ReactNode
  width?: number
}

const iconMap = {
  info: InfoCircledIcon,
  warning: ExclamationTriangleIcon,
  error: CrossCircledIcon
}

const iconClassMap = {
  info: styles.iconInfo,
  warning: styles.iconWarning,
  error: styles.iconError
}

export function Dialog({
  title,
  icon,
  open,
  onClose,
  children,
  footer,
  width
}: DialogProps) {
  const dialogRef = useRef<HTMLDivElement>(null)

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.preventDefault()
        e.stopPropagation()
        onClose()
      }
    },
    [onClose]
  )

  const handleOverlayClick = useCallback(
    (e: React.MouseEvent) => {
      if (e.target === e.currentTarget) {
        onClose()
      }
    },
    [onClose]
  )

  useEffect(() => {
    if (open) {
      document.addEventListener('keydown', handleKeyDown)
      dialogRef.current?.focus()
      return () => {
        document.removeEventListener('keydown', handleKeyDown)
      }
    }
  }, [open, handleKeyDown])

  if (!open) return null

  const IconComponent = icon ? iconMap[icon] : null
  const iconClass = icon ? iconClassMap[icon] : ''

  return (
    <div
      className={styles.overlay}
      onClick={handleOverlayClick}
      role='presentation'
    >
      <div
        ref={dialogRef}
        className={styles.dialog}
        role='dialog'
        aria-modal='true'
        aria-labelledby='dialog-title'
        tabIndex={-1}
        style={width ? { width } : undefined}
      >
        <div className={styles.header}>
          <span className={styles.title} id='dialog-title'>
            {IconComponent && (
              <IconComponent className={iconClass} width={16} height={16} />
            )}
            {title}
          </span>
          <button
            className={styles.closeButton}
            onClick={onClose}
            aria-label='Close'
            type='button'
          >
            <Cross1Icon width={14} height={14} />
          </button>
        </div>
        <div className={styles.content}>{children}</div>
        {footer && <div className={styles.footer}>{footer}</div>}
      </div>
    </div>
  )
}
