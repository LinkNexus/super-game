// STUB (L37 scaffold): only proves the wasm module loads, types check and one
// step() round-trips through messages.ts. Replaced by Levy's game loop
// (fixed-step accumulator, keyboard -> PlayerInputs, Session).
import createModule from "@wasm/supergame-web-sim.mjs";
import type { GameStateMessage } from "./messages";

const module = await createModule();
module.rndGeneratorSeed();

using sim = new module.WebSim();
sim.startCoop(1);

const slots = Array(module.MAX_PLAYERS_ON_THIS_MACHINE).fill(null);
const msg = JSON.parse(sim.step(JSON.stringify(slots))) as GameStateMessage;

if (msg.type === module.ServerMessageType.COOP_GAME_STATE) {
  console.log("coop phase", msg.payload.phase, msg.payload.players);
}
