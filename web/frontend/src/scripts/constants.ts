import createModule from "@wasm/supergame-web-sim.mjs";

export const appModule = await createModule();
export const sim = new appModule.WebSim();

export const SCREEN_WIDTH = 1000;
export const SCREEN_HEIGHT = 800;

export const canvas = document.getElementById("game") as HTMLCanvasElement;
const dpr = window.devicePixelRatio || 1;

canvas.width = SCREEN_WIDTH * dpr;
canvas.height = SCREEN_HEIGHT * dpr;

export const canvasCtx = canvas.getContext("2d") as CanvasRenderingContext2D;
canvasCtx.scale(dpr, dpr);

export const GAME_TYPES = ["local", "online"] as const;
export const MODES = ["coop", "pvp"] as const;
export const CONTROLS = {
  [appModule.Button.LEFT]: ["ArrowLeft", "KeyA"],
  [appModule.Button.RIGHT]: ["ArrowRight", "KeyD"],
  [appModule.Button.UP]: ["ArrowUp", "KeyW"],
  [appModule.Button.DOWN]: ["ArrowDown", "KeyS"],
  [appModule.Button.SHOOT]: ["Space", "KeyJ"],
} as const;

export const ARM_DELAY_MS = 800;
