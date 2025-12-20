/**
 * Draggable Window Component
 * A floating window that can be dragged anywhere on screen
 */
import { Cross1Icon, MinusIcon } from '@radix-ui/react-icons'
import { useCallback, useEffect, useRef, useState } from 'react'
import styles from './draggable-window.module.css'

interface Position {
  x: number
  y: number
}

interface DraggableWindowProps {
  id: string
  title: string
  children: React.ReactNode
  defaultPosition?: Position
  onClose?: () => void
  initiallyMinimized?: boolean
}

export function DraggableWindow({
  id,
  title,
  children,
  defaultPosition = { x: 100, y: 100 },
  onClose,
  initiallyMinimized = false
}: DraggableWindowProps) {
  const [position, setPosition] = useState<Position>(() => {
    // Try to restore position from localStorage
    const saved = localStorage.getItem(`debug-window-${id}`)
    if (saved) {
      try {
        return JSON.parse(saved)
      } catch {
        return defaultPosition
      }
    }
    return defaultPosition
  })

  const [isMinimized, setIsMinimized] = useState(initiallyMinimized)
  const [isDragging, setIsDragging] = useState(false)
  const [dragOffset, setDragOffset] = useState<Position>({ x: 0, y: 0 })
  const windowRef = useRef<HTMLDivElement>(null)

  // Save position to localStorage when it changes
  useEffect(() => {
    localStorage.setItem(`debug-window-${id}`, JSON.stringify(position))
  }, [id, position])

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    if ((e.target as HTMLElement).closest('button')) return
    
    setIsDragging(true)
    setDragOffset({
      x: e.clientX - position.x,
      y: e.clientY - position.y
    })
    e.preventDefault()
  }, [position])

  useEffect(() => {
    if (!isDragging) return

    const handleMouseMove = (e: MouseEvent) => {
      const newX = Math.max(0, Math.min(window.innerWidth - 100, e.clientX - dragOffset.x))
      const newY = Math.max(0, Math.min(window.innerHeight - 50, e.clientY - dragOffset.y))
      setPosition({ x: newX, y: newY })
    }

    const handleMouseUp = () => {
      setIsDragging(false)
    }

    document.addEventListener('mousemove', handleMouseMove)
    document.addEventListener('mouseup', handleMouseUp)

    return () => {
      document.removeEventListener('mousemove', handleMouseMove)
      document.removeEventListener('mouseup', handleMouseUp)
    }
  }, [isDragging, dragOffset])

  return (
    <div
      ref={windowRef}
      className={`${styles.window} ${isDragging ? styles.dragging : ''}`}
      style={{
        left: position.x,
        top: position.y
      }}
    >
      <div
        className={styles.titleBar}
        onMouseDown={handleMouseDown}
      >
        <span className={styles.title}>{title}</span>
        <div className={styles.controls}>
          <button
            type="button"
            className={styles.controlButton}
            onClick={() => setIsMinimized(!isMinimized)}
            aria-label={isMinimized ? 'Expand' : 'Minimize'}
          >
            <MinusIcon width={12} height={12} />
          </button>
          {onClose && (
            <button
              type="button"
              className={`${styles.controlButton} ${styles.closeButton}`}
              onClick={onClose}
              aria-label="Close"
            >
              <Cross1Icon width={12} height={12} />
            </button>
          )}
        </div>
      </div>
      {!isMinimized && (
        <div className={styles.content}>
          {children}
        </div>
      )}
    </div>
  )
}
