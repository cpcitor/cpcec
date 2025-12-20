import { useCallback, useEffect, useRef } from 'react'
import type { FileDialogProps } from '@/types'
import styles from './file-dialog.module.css'

/**
 * File dialog component - uses native browser file picker
 * This is a non-visual component that triggers the browser's file dialog
 */
export function FileDialog({
  accept,
  multiple = false,
  mode,
  onSelect,
  onCancel
}: FileDialogProps) {
  const inputRef = useRef<HTMLInputElement>(null)

  const handleChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const files = e.target.files
      if (files && files.length > 0) {
        onSelect(Array.from(files))
      } else {
        onCancel()
      }
    },
    [onSelect, onCancel]
  )

  useEffect(() => {
    // Trigger the file dialog immediately
    if (mode === 'open') {
      inputRef.current?.click()
    }
  }, [mode])

  // For save mode, we need a different approach
  // The browser's "save" functionality is limited - we'll handle it via download
  if (mode === 'save') {
    // Save mode should be handled externally with downloadFile utility
    onCancel()
    return null
  }

  return (
    <input
      ref={inputRef}
      type='file'
      className={styles.hidden}
      accept={accept}
      multiple={multiple}
      onChange={handleChange}
      onBlur={onCancel}
    />
  )
}

/**
 * Utility function to save a file (download)
 */
export function downloadFile(
  data: Blob | ArrayBuffer | string,
  filename: string,
  mimeType = 'application/octet-stream'
) {
  const blob =
    data instanceof Blob ? data : new Blob([data], { type: mimeType })

  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  document.body.appendChild(a)
  a.click()
  document.body.removeChild(a)
  URL.revokeObjectURL(url)
}
