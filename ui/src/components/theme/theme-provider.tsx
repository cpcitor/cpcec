import { type ReactNode, useEffect } from "react";
import { theme } from "./theme";

export function ThemeProvider({ children }: { readonly children: ReactNode }) {
  useEffect(() => {
    function injectVars(
      prefix: string,
      obj: Record<string, string | object>,
      set: (k: string, v: string) => void
    ) {
      for (const [key, value] of Object.entries(obj)) {
        const fullKey = `${prefix}-${key}`;
        if (typeof value === "string") {
          set(fullKey, value);
        } else {
          injectVars(fullKey, value as Record<string, string | object>, set);
        }
      }
    }

    const root = document.documentElement;
    const setVar = (key: string, val: string) =>
      root.style.setProperty(key, val);

    injectVars("--color", theme.colors, setVar);
    injectVars("--font", theme.font, setVar);
    injectVars("--spacing", theme.spacing, setVar);
    injectVars("--radius", theme.radius, setVar);
    injectVars("--shadow", theme.shadow, setVar);
    injectVars("--breakpoints", theme.breakpoints, setVar);
  }, []);

  return <>{children}</>;
}
