import { useCallback, useEffect, useRef, useState } from 'react'
import { Button } from '@/components/ui/button'
import { Dialog } from '@/components/ui/dialog'
import type { KeyScanDialogProps } from '@/types'
import styles from './key-scan-dialog.module.css'

// Map key codes to display names
const keyDisplayNames: Record<string, string> = {
  ArrowUp: '↑',
  ArrowDown: '↓',
  ArrowLeft: '←',
  ArrowRight: '→',
  Space: 'SPACE',
  Enter: 'ENTER',
  Escape: 'ESC',
  Backspace: 'BKSP',
  Tab: 'TAB',
  CapsLock: 'CAPS',
  ShiftLeft: 'L-SHIFT',
  ShiftRight: 'R-SHIFT',
  ControlLeft: 'L-CTRL',
  ControlRight: 'R-CTRL',
  AltLeft: 'L-ALT',
  AltRight: 'R-ALT',
  Delete: 'DEL',
  Insert: 'INS',
  Home: 'HOME',
  End: 'END',
  PageUp: 'PGUP',
  PageDown: 'PGDN'
}

function getKeyDisplayName(code: string, key: string): string {
  if (keyDisplayNames[code]) {
    return keyDisplayNames[code]
  }
  if (code.startsWith('Key')) {
    return code.slice(3)
  }
  if (code.startsWith('Digit')) {
    return code.slice(5)
  }
  if (code.startsWith('Numpad')) {
    return `NUM ${code.slice(6)}`
  }
  return key.toUpperCase()
}

export function KeyScanDialog({
  title,
  prompt,
  onKeyPress,
  onCancel
}: KeyScanDialogProps) {
  const [pressedKey, setPressedKey] = useState<string | null>(null)
  const [keyCode, setKeyCode] = useState<number | null>(null)
  const containerRef = useRef<HTMLDivElement>(null)

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      e.preventDefault()
      e.stopPropagation()

      // Ignore modifier-only keys for now, wait for the actual key
      if (
        e.key === 'Shift' ||
        e.key === 'Control' ||
        e.key === 'Alt' ||
        e.key === 'Meta'
      ) {
        return
      }

      // Use DOM keyCode for compatibility with emulator
      const code = e.keyCode || e.which
      setKeyCode(code)
      setPressedKey(getKeyDisplayName(e.code, e.key))

      // Short delay before confirming
      setTimeout(() => {
        onKeyPress(code)
      }, 200)
    },
    [onKeyPress]
  )

  useEffect(() => {
    const container = containerRef.current
    if (container) {
      container.focus()
    }

    window.addEventListener('keydown', handleKeyDown)
    return () => {
      window.removeEventListener('keydown', handleKeyDown)
    }
  }, [handleKeyDown])

  return (
    <Dialog
      title={title}
      open={true}
      onClose={onCancel}
      width={300}
      footer={
        <Button onClick={onCancel} variant='secondary'>
          Cancel
        </Button>
      }
    >
      <div ref={containerRef} className={styles.container} tabIndex={-1}>
        <div className={styles.prompt}>{prompt}</div>
        <div className={styles.keyDisplay}>
          {pressedKey ? (
            pressedKey
          ) : (
            <span className={styles.waiting}>Press a key...</span>
          )}
        </div>
        {keyCode !== null && <div className={styles.hint}>Code: {keyCode}</div>}
        <div className={styles.hint}>Press ESC to cancel</div>
      </div>
    </Dialog>
  )
}
