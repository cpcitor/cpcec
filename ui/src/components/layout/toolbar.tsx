import {
  CodeIcon,
  GearIcon,
  InfoCircledIcon,
  PauseIcon,
  PlayIcon,
  ResetIcon,
  UploadIcon
} from '@radix-ui/react-icons'
import { useAtom, useAtomValue, useSetAtom } from 'jotai'
import { useCallback, useRef } from 'react'
import { Button } from '@/components/ui'
import { useDialogs, useEmulator } from '@/hooks'
import {
  consoleMessagesAtom,
  crtEffectEnabledAtom,
  debugPanelVisibleAtom
} from '@/store'
import styles from './toolbar.module.css'

export function Toolbar() {
  const { isReady, isPaused, reset, pause, resume, loadSna, loadDsk } =
    useEmulator()
  const { showAbout } = useDialogs()
  const fileInputRef = useRef<HTMLInputElement>(null)
  const crtEnabled = useAtomValue(crtEffectEnabledAtom)
  const setCrtEnabled = useSetAtom(crtEffectEnabledAtom)
  const [debugVisible, setDebugVisible] = useAtom(debugPanelVisibleAtom)
  const messages = useAtomValue(consoleMessagesAtom)
  const lastMessage = messages[messages.length - 1]

  const handleFileSelect = useCallback(
    async (e: React.ChangeEvent<HTMLInputElement>) => {
      const file = e.target.files?.[0]
      if (!file) return

      const data = new Uint8Array(await file.arrayBuffer())
      const ext = file.name.toLowerCase().split('.').pop()

      if (ext === 'sna') {
        loadSna(data)
      } else if (ext === 'dsk') {
        loadDsk(data)
      }

      // Reset input
      e.target.value = ''
    },
    [loadSna, loadDsk]
  )

  const handleLoadClick = () => {
    fileInputRef.current?.click()
  }

  const handlePlayPause = () => {
    if (isPaused) {
      resume()
    } else {
      pause()
    }
  }

  const toggleCrt = () => {
    setCrtEnabled(!crtEnabled)
  }

  const toggleDebug = () => {
    setDebugVisible(!debugVisible)
  }

  const handleAbout = () => {
    showAbout(
      'About CPCEC',
      '20241215',
      'Amstrad CPC emulator written in C by Cesar Nicolas-Gonzalez'
    )
  }

  return (
    <div className={styles.toolbar}>
      <div className={styles.controls}>
        <input
          ref={fileInputRef}
          type='file'
          accept='.sna,.dsk'
          onChange={handleFileSelect}
          className={styles.hiddenInput}
        />

        <Button
          variant='primary'
          size='sm'
          onClick={handleLoadClick}
          disabled={!isReady}
          title='Load SNA/DSK file'
        >
          <UploadIcon />
          Load
        </Button>

        <Button
          variant='icon'
          onClick={handlePlayPause}
          disabled={!isReady}
          title={isPaused ? 'Resume' : 'Pause'}
        >
          {isPaused ? <PlayIcon /> : <PauseIcon />}
        </Button>

        <Button
          variant='icon'
          onClick={reset}
          disabled={!isReady}
          title='Reset'
        >
          <ResetIcon />
        </Button>

        <div className={styles.separator} />

        <Button
          variant={crtEnabled ? 'primary' : 'secondary'}
          size='sm'
          onClick={toggleCrt}
          title='Toggle CRT effect'
        >
          <GearIcon />
          CRT
        </Button>

        <Button
          variant={debugVisible ? 'primary' : 'secondary'}
          size='sm'
          onClick={toggleDebug}
          title='Toggle Debug Panel'
        >
          <CodeIcon />
          Debug
        </Button>

        <div className={styles.separator} />

        <Button variant='icon' onClick={handleAbout} title='About CPCEC'>
          <InfoCircledIcon />
        </Button>
      </div>

      <div className={styles.status}>
        {lastMessage && (
          <span className={styles[lastMessage.type]}>{lastMessage.text}</span>
        )}
      </div>
    </div>
  )
}
