/**
 * Connected Debug Panel - wrapper that connects DebugPanel to WASM state
 */
import { useCallback } from "react";
import { useDebugState } from "@/hooks/use-debug-state";
import { DebugPanel } from "./debug-panel";

interface ConnectedDebugPanelProps {
  onClose: () => void;
}

export function ConnectedDebugPanel({ onClose }: ConnectedDebugPanelProps) {
  const { cpu, memory, disassembly, isPaused, step, togglePause, refresh } =
    useDebugState(true, 100); // Refresh every 100ms

  // Breakpoints - stub for now (would require more WASM API)
  const breakpoints: number[] = [];

  const handleStep = useCallback(() => {
    step();
    // Refresh immediately after step
    setTimeout(refresh, 10);
  }, [step, refresh]);

  const handleStepOver = useCallback(() => {
    // Step over not implemented yet - just do step
    handleStep();
  }, [handleStep]);

  const handleContinue = useCallback(() => {
    togglePause();
  }, [togglePause]);

  const handleBreak = useCallback(() => {
    togglePause();
  }, [togglePause]);

  const handleAddBreakpoint = useCallback((_addr: number) => {
    // Not implemented yet
    console.log("Add breakpoint:", _addr.toString(16));
  }, []);

  const handleRemoveBreakpoint = useCallback((_addr: number) => {
    // Not implemented yet
    console.log("Remove breakpoint:", _addr.toString(16));
  }, []);

  return (
    <DebugPanel
      cpu={cpu}
      memory={memory}
      disassembly={disassembly}
      breakpoints={breakpoints}
      onStep={handleStep}
      onStepOver={handleStepOver}
      onContinue={handleContinue}
      onBreak={handleBreak}
      onAddBreakpoint={handleAddBreakpoint}
      onRemoveBreakpoint={handleRemoveBreakpoint}
      onClose={onClose}
      isRunning={!isPaused}
    />
  );
}
