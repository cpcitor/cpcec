import { useAtomValue } from "jotai";
import { crtEffectEnabledAtom } from "@/store";
import styles from "./crt-effect.module.css";

export function CrtEffect() {
  const enabled = useAtomValue(crtEffectEnabledAtom);

  if (!enabled) return null;

  return <div className={styles.crtOverlay} aria-hidden="true" />;
}
