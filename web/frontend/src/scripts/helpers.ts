import { appModule, canvasCtx } from "./constants";
import type { GameScreen } from "./types";

export function assertNeverForScreen(screen: GameScreen) {
  throw new Error(`unhandled screen ${JSON.stringify(screen)}`);
}

export function stripFileExtension(filename: string): string {
  return filename.replace(/\.[^/.]+$/, "");
}

export function setFont(size: number) {
  canvasCtx.font = `${size}px "PackMan"`;
}

export function countOptional<T>(arr: (T | null)[]): number {
  return arr.reduce((acc, el) => acc + (el !== null ? 1 : 0), 0);
}

export function toScreen({ x, y }: { x: number; y: number }) {
  return {
    x,
    y: appModule.SCREEN_HEIGHT - y,
  };
}

export function isTouchOnly() {
  return (
    matchMedia("(pointer: coarse)").matches &&
    !matchMedia("(any-pointer: fine)").matches
  );
}

/// Android only: iOS has no element fullscreen (Add to Home Screen instead).
/// Browsers allow it only inside a tap, so call it from a click handler.
export function enterFullscreenOnTouch() {
  if (
    isTouchOnly() &&
    document.fullscreenEnabled &&
    !document.fullscreenElement
  ) {
    document.documentElement.requestFullscreen().catch(() => {});
  }
}
