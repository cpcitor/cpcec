import { useEffect, useRef, useState } from 'react'
import { useEmulator } from '@/hooks'
import styles from './emulator-canvas.module.css'

// Flag to control whether CPCEC should receive keyboard events
let emulatorHasFocus = false

export function getEmulatorFocus() {
  return emulatorHasFocus
}

// Get CPCEC Module reference
function getCpcecModule(): any {
  return (globalThis as any).Module
}

export function EmulatorCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const { initialize, isReady } = useEmulator()
  const [hasFocus, setHasFocus] = useState(false)

  useEffect(() => {
    if (canvasRef.current && !isReady) {
      initialize(canvasRef.current)
    }
  }, [initialize, isReady])

  // Block keyboard events from reaching CPCEC (which listens on window)
  useEffect(() => {
    const blockKeyboardForCPCEC = (e: KeyboardEvent) => {
      if (emulatorHasFocus) {
        return
      }
      e.stopPropagation()
    }

    document.body.addEventListener('keydown', blockKeyboardForCPCEC, false)
    document.body.addEventListener('keyup', blockKeyboardForCPCEC, false)
    document.body.addEventListener('keypress', blockKeyboardForCPCEC, false)

    return () => {
      document.body.removeEventListener('keydown', blockKeyboardForCPCEC, false)
      document.body.removeEventListener('keyup', blockKeyboardForCPCEC, false)
      document.body.removeEventListener(
        'keypress',
        blockKeyboardForCPCEC,
        false
      )
    }
  }, [])

  // Handle special CPC keyboard mappings when emulator has focus
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (!emulatorHasFocus) return

      const Module = getCpcecModule()
      if (!Module) return

      // Map Option/Alt key to CPC COPY (0x09)
      if (
        e.altKey &&
        !e.shiftKey &&
        !e.ctrlKey &&
        !e.metaKey &&
        (e.code === 'AltLeft' || e.code === 'AltRight')
      ) {
        e.preventDefault()
        if (Module._em_key_press) {
          Module._em_key_press(0x09)
        }
      }

      // Map Shift+0-9 to CPC function keys F0-F9
      if (e.shiftKey && e.code && e.code.startsWith('Digit')) {
        e.preventDefault()
        e.stopPropagation()
        const fnNum = parseInt(e.code.charAt(5), 10)
        if (Module._em_press_fn) {
          Module._em_press_fn(fnNum)
        }
      }
    }

    const handleKeyUp = (e: KeyboardEvent) => {
      if (!emulatorHasFocus) return

      const Module = getCpcecModule()
      if (!Module) return

      if (e.code === 'AltLeft' || e.code === 'AltRight') {
        if (Module._em_key_release) {
          Module._em_key_release(0x09)
        }
      }

      if (e.code?.startsWith('Digit')) {
        const fnNum = parseInt(e.code.charAt(5), 10)
        if (Module._em_release_fn) {
          Module._em_release_fn(fnNum)
        }
      }
    }

    document.addEventListener('keydown', handleKeyDown)
    document.addEventListener('keyup', handleKeyUp)

    return () => {
      document.removeEventListener('keydown', handleKeyDown)
      document.removeEventListener('keyup', handleKeyUp)
    }
  }, [])

  const handleFocus = () => {
    emulatorHasFocus = true
    setHasFocus(true)
  }

  const handleBlur = () => {
    emulatorHasFocus = false
    setHasFocus(false)
  }

  return (
    <div className={styles.container} ref={containerRef}>
      <div className={styles.header}>
        <span className={styles.title}>CPC Emulator</span>
        <span className={styles.status}>
          {isReady
            ? hasFocus
              ? '● Active'
              : '○ Click to type'
            : '○ Loading...'}
        </span>
      </div>
      <button
        type='button'
        id='emulator-focus-target'
        className={`${styles.canvasWrapper} ${hasFocus ? styles.focused : ''}`}
        onFocus={handleFocus}
        onBlur={handleBlur}
      >
        <canvas
          ref={canvasRef}
          id='canvas'
          className={styles.canvas}
          width={768}
          height={544}
        />
      </button>
    </div>
  )
}
