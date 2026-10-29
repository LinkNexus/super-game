import createModule from "@wasm/supergame-web-sim.mjs";

export const appModule = await createModule();
appModule.rndGeneratorSeed();

export const sim = new appModule.WebSim();

export const canvas = document.getElementById("game") as HTMLCanvasElement;
const dpr = window.devicePixelRatio || 1;

canvas.width = appModule.SCREEN_WIDTH * dpr;
canvas.height = appModule.SCREEN_HEIGHT * dpr;
// canvas.style.width = `${appModule.SCREEN_WIDTH}px`;
// canvas.style.height = `${appModule.SCREEN_HEIGHT}px`;
// canvas.style.aspectRatio = `${appModule.SCREEN_WIDTH} / ${appModule.SCREEN_HEIGHT}`;

export const canvasCtx = canvas.getContext("2d") as CanvasRenderingContext2D;
canvasCtx.scale(dpr, dpr);

export const GAME_TYPES = ["local", "online"] as const;
export const MODES = ["coop", "pvp"] as const;
export const CONTROLS = [
  {
    ArrowLeft: appModule.Button.BUTTON_LEFT,
    ArrowRight: appModule.Button.BUTTON_RIGHT,
    Space: appModule.Button.BUTTON_SHOOT,
  },
  {
    KeyA: appModule.Button.BUTTON_LEFT,
    KeyD: appModule.Button.BUTTON_RIGHT,
    KeyW: appModule.Button.BUTTON_SHOOT,
  },
] as const;

export const FONT_FAMILY = "PackMan.ttf";
