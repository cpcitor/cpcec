import { useCallback, useRef } from "react";
import { useAtomValue, useSetAtom } from "jotai";
import {
  PlayIcon,
  PauseIcon,
  ResetIcon,
  UploadIcon,
  GearIcon,
} from "@radix-ui/react-icons";
import { Button } from "@/components/ui";
import { useEmulator } from "@/hooks";
import { crtEffectEnabledAtom, consoleMessagesAtom } from "@/store";
import styles from "./toolbar.module.css";

export function Toolbar() {
  const { isReady, isPaused, reset, pause, resume, loadSna, loadDsk } =
    useEmulator();
  const fileInputRef = useRef<HTMLInputElement>(null);
  const crtEnabled = useAtomValue(crtEffectEnabledAtom);
  const setCrtEnabled = useSetAtom(crtEffectEnabledAtom);
  const messages = useAtomValue(consoleMessagesAtom);
  const lastMessage = messages[messages.length - 1];

  const handleFileSelect = useCallback(
    async (e: React.ChangeEvent<HTMLInputElement>) => {
      const file = e.target.files?.[0];
      if (!file) return;

      const data = new Uint8Array(await file.arrayBuffer());
      const ext = file.name.toLowerCase().split(".").pop();

      if (ext === "sna") {
        loadSna(data);
      } else if (ext === "dsk") {
        loadDsk(data);
      }

      // Reset input
      e.target.value = "";
    },
    [loadSna, loadDsk]
  );

  const handleLoadClick = () => {
    fileInputRef.current?.click();
  };

  const handlePlayPause = () => {
    if (isPaused) {
      resume();
    } else {
      pause();
    }
  };

  const toggleCrt = () => {
    setCrtEnabled(!crtEnabled);
  };

  return (
    <div className={styles.toolbar}>
      <div className={styles.controls}>
        <input
          ref={fileInputRef}
          type="file"
          accept=".sna,.dsk"
          onChange={handleFileSelect}
          className={styles.hiddenInput}
        />

        <Button
          variant="primary"
          size="sm"
          onClick={handleLoadClick}
          disabled={!isReady}
          title="Load SNA/DSK file"
        >
          <UploadIcon />
          Load
        </Button>

        <Button
          variant="icon"
          onClick={handlePlayPause}
          disabled={!isReady}
          title={isPaused ? "Resume" : "Pause"}
        >
          {isPaused ? <PlayIcon /> : <PauseIcon />}
        </Button>

        <Button
          variant="icon"
          onClick={reset}
          disabled={!isReady}
          title="Reset"
        >
          <ResetIcon />
        </Button>

        <div className={styles.separator} />

        <Button
          variant={crtEnabled ? "primary" : "secondary"}
          size="sm"
          onClick={toggleCrt}
          title="Toggle CRT effect"
        >
          <GearIcon />
          CRT
        </Button>
      </div>

      <div className={styles.status}>
        {lastMessage && (
          <span className={styles[lastMessage.type]}>{lastMessage.text}</span>
        )}
      </div>
    </div>
  );
}
