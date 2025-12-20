import { CrtEffect } from "@/components/crt-effect";
import { MainLayout } from "@/components/layout";
import { ThemeProvider } from "@/components/theme";

export default function App() {
  return (
    <ThemeProvider>
      <CrtEffect />
      <MainLayout />
    </ThemeProvider>
  );
}
