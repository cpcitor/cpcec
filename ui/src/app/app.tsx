import { Provider } from 'jotai'
import { CrtEffect } from '@/components/crt-effect'
import { DialogManager } from '@/components/dialogs'
import { MainLayout } from '@/components/layout'
import { ThemeProvider } from '@/components/theme'
import { appStore } from '@/store'

export default function App() {
  return (
    <Provider store={appStore}>
      <ThemeProvider>
        <CrtEffect />
        <MainLayout />
        <DialogManager />
      </ThemeProvider>
    </Provider>
  )
}
