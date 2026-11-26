import type { Vec3 } from "@wasm/supergame-web-sim.mjs";
import { appModule, canvasCtx, SCREEN_WIDTH } from "./constants";
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

const { FLAT_WORLD_FIELD_BOUNDS_MIN: min, FLAT_WORLD_FIELD_BOUNDS_MAX: max } =
  appModule;
const scale = SCREEN_WIDTH / (max.x - min.x);
export function toScreen({ x, z }: Vec3) {
  return {
    x: (x - min.x) * scale,
    y: (max.z - z) * scale,
  };
}
export function toScreenSize(size: number) {
  return size * scale;
}
export function toScreenSizeVec({ x, y, z }: Vec3) {
  return {
    x: x * scale,
    y: y * scale,
    z: z * scale,
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
