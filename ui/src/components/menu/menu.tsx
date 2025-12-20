import { CheckIcon } from '@radix-ui/react-icons'
import { useCallback, useEffect, useRef, useState } from 'react'
import type { MenuData, MenuItem as MenuItemType } from '@/types'
import styles from './menu.module.css'

interface MenuBarProps {
  data: MenuData
  onAction: (id: number, shift: boolean) => void
}

export function MenuBar({ data, onAction }: MenuBarProps) {
  const [activeMenu, setActiveMenu] = useState<number | null>(null)
  const [selectedItem, setSelectedItem] = useState<number>(-1)
  const menuBarRef = useRef<HTMLDivElement>(null)

  const handleMenuClick = useCallback((index: number) => {
    setActiveMenu((prev) => (prev === index ? null : index))
    setSelectedItem(-1)
  }, [])

  const handleMenuHover = useCallback(
    (index: number) => {
      if (activeMenu !== null) {
        setActiveMenu(index)
        setSelectedItem(-1)
      }
    },
    [activeMenu]
  )

  const handleItemClick = useCallback(
    (item: MenuItemType, e: React.MouseEvent) => {
      if (item.disabled || item.separator) return
      onAction(item.id, e.shiftKey)
      setActiveMenu(null)
    },
    [onAction]
  )

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      if (activeMenu === null) return

      const menu = data[activeMenu]
      const items = menu.items.filter((i) => !i.separator)

      switch (e.key) {
        case 'Escape':
          setActiveMenu(null)
          break
        case 'ArrowLeft':
          e.preventDefault()
          setActiveMenu((prev) =>
            prev === null ? null : prev > 0 ? prev - 1 : data.length - 1
          )
          setSelectedItem(-1)
          break
        case 'ArrowRight':
          e.preventDefault()
          setActiveMenu((prev) =>
            prev === null ? null : prev < data.length - 1 ? prev + 1 : 0
          )
          setSelectedItem(-1)
          break
        case 'ArrowDown':
          e.preventDefault()
          setSelectedItem((prev) => (prev < items.length - 1 ? prev + 1 : 0))
          break
        case 'ArrowUp':
          e.preventDefault()
          setSelectedItem((prev) => (prev > 0 ? prev - 1 : items.length - 1))
          break
        case 'Enter':
          if (selectedItem >= 0 && items[selectedItem]) {
            const item = items[selectedItem]
            if (!item.disabled) {
              onAction(item.id, e.shiftKey)
              setActiveMenu(null)
            }
          }
          break
      }
    },
    [activeMenu, data, selectedItem, onAction]
  )

  // Close menu when clicking outside
  useEffect(() => {
    if (activeMenu === null) return

    const handleClickOutside = (e: MouseEvent) => {
      if (
        menuBarRef.current &&
        !menuBarRef.current.contains(e.target as Node)
      ) {
        setActiveMenu(null)
      }
    }

    document.addEventListener('mousedown', handleClickOutside)
    return () => document.removeEventListener('mousedown', handleClickOutside)
  }, [activeMenu])

  return (
    <div
      ref={menuBarRef}
      className={styles.menuBar}
      onKeyDown={handleKeyDown}
      role='menubar'
    >
      {data.map((category, index) => (
        <div key={category.label} style={{ position: 'relative' }}>
          <button
            className={`${styles.menuTrigger} ${
              activeMenu === index ? styles.menuTriggerActive : ''
            }`}
            onClick={() => handleMenuClick(index)}
            onMouseEnter={() => handleMenuHover(index)}
            role='menuitem'
            aria-haspopup='true'
            aria-expanded={activeMenu === index}
            type='button'
          >
            {category.label}
          </button>
          {activeMenu === index && (
            <MenuDropdown
              items={category.items}
              selectedIndex={selectedItem}
              onItemClick={handleItemClick}
              onItemHover={setSelectedItem}
            />
          )}
        </div>
      ))}
      {activeMenu !== null && (
        <div
          className={styles.overlay}
          onClick={() => setActiveMenu(null)}
          role='presentation'
        />
      )}
    </div>
  )
}

interface MenuDropdownProps {
  items: MenuItemType[]
  selectedIndex: number
  onItemClick: (item: MenuItemType, e: React.MouseEvent) => void
  onItemHover: (index: number) => void
}

function MenuDropdown({
  items,
  selectedIndex,
  onItemClick,
  onItemHover
}: MenuDropdownProps) {
  let itemIndex = -1

  return (
    <div className={styles.dropdown} role='menu'>
      {items.map((item, index) => {
        if (item.separator) {
          return <div key={`sep-${index}`} className={styles.separator} />
        }

        itemIndex++
        const currentIndex = itemIndex

        return (
          <button
            key={item.id}
            className={`${styles.menuItem} ${
              currentIndex === selectedIndex ? styles.menuItemSelected : ''
            }`}
            onClick={(e) => onItemClick(item, e)}
            onMouseEnter={() => onItemHover(currentIndex)}
            disabled={item.disabled}
            role='menuitem'
            type='button'
          >
            <span className={styles.menuItemLabel}>
              <span className={styles.checkmark}>
                {item.checked && <CheckIcon width={12} height={12} />}
                {item.radio && item.checked && (
                  <span className={styles.radioDot} />
                )}
              </span>
              {item.label}
            </span>
            {item.shortcut && (
              <span className={styles.shortcut}>{item.shortcut}</span>
            )}
          </button>
        )
      })}
    </div>
  )
}
