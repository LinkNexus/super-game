import { appModule, CONTROLS } from "./constants";
import type { PlayerIds, PlayerInput, PlayerInputs } from "./messages";

let gameHasKeyboard = false;
export const setGameHasKeyboard = (v: boolean) => (gameHasKeyboard = v);

const GAME_KEYS = ["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Space"];

export const keysDown: Set<string> = new Set();
export const keysPressed: Set<string> = new Set();
window.addEventListener("keydown", (e) => {
  if (gameHasKeyboard && GAME_KEYS.includes(e.code)) {
    e.preventDefault();
  }

  if (!e.repeat) keysPressed.add(e.code);
  keysDown.add(e.code);
});

window.addEventListener("keyup", (e) => {
  keysDown.delete(e.code);
});

window.addEventListener("blur", () => {
  keysDown.clear();
  keysPressed.clear();
});

const TOUCH_KEYS = {
  left: "ArrowLeft",
  right: "ArrowRight",
  shoot: "Space",
} as const;

const fingers = new Map<number, HTMLElement | null>();

function buttonAt(x: number, y: number) {
  return (
    document.elementFromPoint(x, y)?.closest<HTMLElement>("[data-button]") ??
    null
  );
}

let touchHeld = new Set<string>();
function syncTouchKeys() {
  const wanted = new Set<string>();

  for (const button of fingers.values()) {
    const name = button?.dataset.button;
    if (name && name in TOUCH_KEYS) {
      wanted.add(TOUCH_KEYS[name as keyof typeof TOUCH_KEYS]);
    }
  }

  for (const key of touchHeld) if (!wanted.has(key)) keysDown.delete(key);

  for (const key of wanted) keysDown.add(key);

  touchHeld = wanted;

  for (const el of document.querySelectorAll<HTMLElement>("[data-button]")) {
    el.classList.toggle("pressed", [...fingers.values()].includes(el));
  }
}

function track(e: PointerEvent) {
  if (e.pointerType !== "touch") return;

  const button = buttonAt(e.clientX, e.clientY);
  fingers.set(e.pointerId, button);
  syncTouchKeys();

  if (e.type === "pointerdown" && button?.dataset.button === "pause")
    keysPressed.add("Escape");
}

function untrack(e: PointerEvent) {
  fingers.delete(e.pointerId);
  syncTouchKeys();
}

window.addEventListener("pointerdown", track);
window.addEventListener("pointermove", track);
window.addEventListener("pointerup", untrack);
window.addEventListener("pointercancel", untrack);

for (const el of document.querySelectorAll<HTMLElement>("[data-button]")) {
  el.addEventListener("touchstart", (e) => e.preventDefault(), {
    passive: false,
  });
}

export function pollPlayerInputsInGame(playerIds: PlayerIds) {
  if (
    playerIds.length > appModule.MAX_PLAYERS_ON_THIS_MACHINE ||
    playerIds.length > CONTROLS.length
  )
    throw new Error(`Too many players on this machine: ${playerIds.length}`);

  const inputs: PlayerInputs = Array(
    appModule.MAX_PLAYERS_ON_THIS_MACHINE,
  ).fill(null);

  for (let i = 0; i < playerIds.length; ++i) {
    if (!playerIds[i]) continue;

    const input: PlayerInput = {
      buttons: 0,
      player_id: playerIds[i] as number,
    };

    for (const [key, button] of Object.entries(CONTROLS[i]!)) {
      if (keysDown.has(key)) {
        input.buttons |= button;
      }
    }

    inputs[i] = input;
  }

  return inputs;
}

export function endInputFrame() {
  keysPressed.clear();
}
