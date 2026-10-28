import { appModule, CONTROLS } from "./constants";
import type { PlayerIds, PlayerInput } from "./messages";

export const keysDown: Set<string> = new Set();
export const keysPressed: Set<string> = new Set();
window.addEventListener("keydown", (e) => {
  if (
    ["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Space"].includes(
      e.code,
    )
  ) {
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

export function pollPlayerInputsInGame(playerIds: PlayerIds) {
  if (
    playerIds.length > appModule.MAX_PLAYERS_ON_THIS_MACHINE ||
    playerIds.length > CONTROLS.length
  )
    throw new Error(`Too many players on this machine: ${playerIds.length}`);

  const inputs: PlayerInput[] = Array(
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
