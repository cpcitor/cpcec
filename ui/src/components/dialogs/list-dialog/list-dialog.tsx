import { useCallback, useEffect, useRef, useState } from 'react'
import { Button } from '@/components/ui/button'
import { Dialog } from '@/components/ui/dialog'
import type { ListDialogProps } from '@/types'
import styles from './list-dialog.module.css'

export function ListDialog({
  title,
  items,
  selectedIndex = -1,
  onSelect,
  onCancel
}: ListDialogProps) {
  const [currentIndex, setCurrentIndex] = useState(selectedIndex)
  const listRef = useRef<HTMLDivElement>(null)
  const itemRefs = useRef<(HTMLButtonElement | null)[]>([])

  useEffect(() => {
    if (currentIndex >= 0 && itemRefs.current[currentIndex]) {
      itemRefs.current[currentIndex]?.focus()
    }
  }, [currentIndex])

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      switch (e.key) {
        case 'ArrowDown':
          e.preventDefault()
          setCurrentIndex((prev) => (prev < items.length - 1 ? prev + 1 : prev))
          break
        case 'ArrowUp':
          e.preventDefault()
          setCurrentIndex((prev) => (prev > 0 ? prev - 1 : prev))
          break
        case 'Home':
          e.preventDefault()
          setCurrentIndex(0)
          break
        case 'End':
          e.preventDefault()
          setCurrentIndex(items.length - 1)
          break
        case 'Enter':
          if (currentIndex >= 0) {
            onSelect(currentIndex, items[currentIndex])
          }
          break
        case 'Escape':
          onCancel()
          break
      }
    },
    [items, currentIndex, onSelect, onCancel]
  )

  const handleItemClick = useCallback(
    (index: number) => {
      setCurrentIndex(index)
      // Double-click or single click selection
      onSelect(index, items[index])
    },
    [items, onSelect]
  )

  const handleConfirm = useCallback(() => {
    if (currentIndex >= 0) {
      onSelect(currentIndex, items[currentIndex])
    }
  }, [currentIndex, items, onSelect])

  if (items.length === 0) {
    return (
      <Dialog title={title} open={true} onClose={onCancel} width={350}>
        <div className={styles.empty}>No items available</div>
      </Dialog>
    )
  }

  return (
    <Dialog
      title={title}
      open={true}
      onClose={onCancel}
      width={350}
      footer={
        <>
          <Button onClick={onCancel} variant='secondary'>
            Cancel
          </Button>
          <Button
            onClick={handleConfirm}
            variant='primary'
            disabled={currentIndex < 0}
          >
            Select
          </Button>
        </>
      }
    >
      <div
        ref={listRef}
        className={styles.list}
        onKeyDown={handleKeyDown}
        role='listbox'
        tabIndex={0}
        aria-activedescendant={
          currentIndex >= 0 ? `list-item-${currentIndex}` : undefined
        }
      >
        {items.map((item, index) => (
          <button
            key={item.value}
            id={`list-item-${index}`}
            ref={(el) => {
              itemRefs.current[index] = el
            }}
            className={`${styles.item} ${
              index === currentIndex ? styles.itemSelected : ''
            }`}
            onClick={() => handleItemClick(index)}
            onDoubleClick={() => onSelect(index, item)}
            role='option'
            aria-selected={index === currentIndex}
            type='button'
          >
            {item.label}
          </button>
        ))}
      </div>
    </Dialog>
  )
}
