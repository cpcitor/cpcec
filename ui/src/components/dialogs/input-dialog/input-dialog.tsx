import { useCallback, useEffect, useRef, useState } from 'react'
import { Button } from '@/components/ui/button'
import { Dialog } from '@/components/ui/dialog'
import type { InputDialogProps } from '@/types'
import styles from './input-dialog.module.css'

export function InputDialog({
  title,
  defaultValue = '',
  placeholder,
  onConfirm,
  onCancel
}: InputDialogProps) {
  const [value, setValue] = useState(defaultValue)
  const inputRef = useRef<HTMLInputElement>(null)

  useEffect(() => {
    inputRef.current?.focus()
    inputRef.current?.select()
  }, [])

  const handleSubmit = useCallback(
    (e: React.FormEvent) => {
      e.preventDefault()
      onConfirm(value)
    },
    [value, onConfirm]
  )

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      if (e.key === 'Enter') {
        onConfirm(value)
      } else if (e.key === 'Escape') {
        onCancel()
      }
    },
    [value, onConfirm, onCancel]
  )

  return (
    <Dialog
      title={title}
      open={true}
      onClose={onCancel}
      width={400}
      footer={
        <>
          <Button onClick={onCancel} variant='secondary'>
            Cancel
          </Button>
          <Button onClick={() => onConfirm(value)} variant='primary'>
            OK
          </Button>
        </>
      }
    >
      <form className={styles.form} onSubmit={handleSubmit}>
        <input
          ref={inputRef}
          type='text'
          className={styles.input}
          value={value}
          onChange={(e) => setValue(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder={placeholder}
          autoComplete='off'
          spellCheck={false}
        />
      </form>
    </Dialog>
  )
}
