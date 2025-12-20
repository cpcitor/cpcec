import { Slot } from '@radix-ui/react-slot'
import clsx from 'clsx'
import type { ButtonHTMLAttributes, ReactNode } from 'react'
import styles from './button.module.css'

type Variant = 'primary' | 'secondary' | 'icon' | 'danger'

type Props = {
  children: ReactNode
  asChild?: boolean
  variant?: Variant
  size?: 'sm' | 'md' | 'lg'
  className?: string
} & ButtonHTMLAttributes<HTMLButtonElement>

export function Button({
  children,
  asChild = false,
  variant = 'primary',
  size = 'md',
  className = '',
  disabled,
  ...props
}: Props) {
  const Comp = asChild ? Slot : 'button'

  return (
    <Comp
      className={clsx(
        styles.button,
        styles[variant],
        styles[size],
        disabled && styles.disabled,
        className
      )}
      disabled={disabled}
      {...props}
    >
      {children}
    </Comp>
  )
}
