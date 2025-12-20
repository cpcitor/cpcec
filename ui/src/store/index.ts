import { createStore } from 'jotai'

// Create a single shared store for the entire app
// This store is used both by React components and by external code (like emulator-ui-bridge)
export const appStore = createStore()

export * from './atoms'
