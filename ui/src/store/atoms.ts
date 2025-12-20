import { atom } from 'jotai'

// Emulator state
export const emulatorReadyAtom = atom(false)
export const emulatorRunningAtom = atom(false)
export const emulatorPausedAtom = atom(false)

// Console messages
export type ConsoleMessage = {
  id: string
  type: 'info' | 'success' | 'error' | 'warning'
  text: string
  timestamp: Date
}

export const consoleMessagesAtom = atom<ConsoleMessage[]>([])

export const addConsoleMessageAtom = atom(
  null,
  (get, set, message: Omit<ConsoleMessage, 'id' | 'timestamp'>) => {
    const messages = get(consoleMessagesAtom)
    const newMessage: ConsoleMessage = {
      ...message,
      id: crypto.randomUUID(),
      timestamp: new Date()
    }
    set(consoleMessagesAtom, [...messages.slice(-99), newMessage])
  }
)

export const clearConsoleAtom = atom(null, (_get, set) => {
  set(consoleMessagesAtom, [])
})

// CRT Effect
export const crtEffectEnabledAtom = atom(true)

// Model selection
export type CpcModel = 'cpc464' | 'cpc664' | 'cpc6128' | 'cpcplus'
export const selectedModelAtom = atom<CpcModel>('cpc6128')

// Debug state
export const debugModeAtom = atom(false)
export const debugPanelVisibleAtom = atom(false)
