import { useAtom } from 'jotai'
import { ConnectedDebugPanel } from '@/components/debug-panel'
import { EmulatorCanvas } from '@/components/emulator'
import { debugPanelVisibleAtom } from '@/store'
import { Header } from './header'
import styles from './main-layout.module.css'
import { Toolbar } from './toolbar'

export function MainLayout() {
  const [debugVisible, setDebugVisible] = useAtom(debugPanelVisibleAtom)

  return (
    <div className={styles.layout}>
      <Header />
      <Toolbar />
      <main className={styles.main}>
        <div className={styles.emulatorWrapper}>
          <EmulatorCanvas />
        </div>
        {debugVisible && (
          <ConnectedDebugPanel onClose={() => setDebugVisible(false)} />
        )}
      </main>
      <footer className={styles.footer}>
        <span>CPCEC by CNGSoft</span>
        <span>•</span>
        <span>Web version</span>
      </footer>
    </div>
  )
}
