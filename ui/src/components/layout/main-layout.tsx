import { EmulatorCanvas } from "@/components/emulator";
import { Header } from "./header";
import { Toolbar } from "./toolbar";
import styles from "./main-layout.module.css";

export function MainLayout() {
  return (
    <div className={styles.layout}>
      <Header />
      <Toolbar />
      <main className={styles.main}>
        <div className={styles.emulatorWrapper}>
          <EmulatorCanvas />
        </div>
      </main>
      <footer className={styles.footer}>
        <span>CPCEC by CNGSoft</span>
        <span>•</span>
        <span>Web version</span>
      </footer>
    </div>
  );
}
