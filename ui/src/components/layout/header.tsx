import { GitHubLogoIcon } from '@radix-ui/react-icons'
import styles from './header.module.css'

export function Header() {
  return (
    <header className={styles.header}>
      <div className={styles.logo}>
        <h1 className={styles.title}>CPCEC</h1>
        <span className={styles.subtitle}>Amstrad CPC Emulator</span>
      </div>

      <div className={styles.actions}>
        <a
          href='https://github.com/cngsoft/cpcec'
          target='_blank'
          rel='noopener noreferrer'
          className={styles.link}
          title='View on GitHub'
        >
          <GitHubLogoIcon width={20} height={20} />
        </a>
      </div>
    </header>
  )
}
