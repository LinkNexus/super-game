import { appModule, canvasCtx, FONT_FAMILY } from "./constants";
import type { GameScreen } from "./types";

export function assertNeverForScreen(screen: GameScreen) {
  throw new Error(`unhandled screen ${JSON.stringify(screen)}`);
}

export function stripFileExtension(filename: string): string {
  return filename.replace(/\.[^/.]+$/, "");
}

export function setFont(size: number) {
  canvasCtx.font = `${size}px ${stripFileExtension(FONT_FAMILY)}`;
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
