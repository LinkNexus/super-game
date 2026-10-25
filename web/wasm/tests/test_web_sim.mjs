// Drives supergame-web-sim through its JS API, the way web/frontend will.
// Usage: node test_web_sim.mjs <path to supergame-web-sim.mjs>
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const { default: createModule } = await import(
  pathToFileURL(process.argv[2]).href
);
const Module = await createModule();

// shared::ServerMessageType / CoopGameSim::Phase, as numbers on the wire.
const PVP_GAME_STATE = 3;
const COOP_GAME_STATE = 4;
const ENEMIES_ENTRANCE = 0;
const BUTTON_SHOOT = 1 << 2;

let failures = 0;
function test(name, fn) {
  try {
    fn();
    console.log(`PASS ${name}`);
  } catch (e) {
    failures++;
    console.log(`FAIL ${name}\n  ${e.message ?? e}`);
  }
}

// Inputs and ids are always MAX_PLAYERS_ON_THIS_MACHINE slots, null-padded
// (a std::array of optionals on the C++ side).
const SLOTS = Module.MAX_PLAYERS_ON_THIS_MACHINE;
const padded = (values) => [...values, ...Array(SLOTS - values.length).fill(null)];
const NO_INPUT = padded([]);

function step(sim, inputs) {
  return JSON.parse(sim.step(JSON.stringify(inputs)));
}

test("coop: step returns the server's envelope", () => {
  const sim = new Module.WebSim();
  sim.startCoop(1);
  const msg = step(sim, NO_INPUT);

  assert.equal(msg.type, COOP_GAME_STATE);
  assert.ok(msg.payload, "no payload");
  assert.equal(msg.payload.phase, ENEMIES_ENTRANCE);
  assert.equal(msg.payload.players.filter((p) => p !== null).length, 1);
  sim.delete();
});

test("coop: the entrance ends within 60 s of ticks", () => {
  const sim = new Module.WebSim();
  sim.startCoop(2);
  let msg;
  for (let i = 0; i < 60 * 60; i++) {
    msg = step(
      sim,
      padded([
        { buttons: BUTTON_SHOOT, player_id: 1 },
        { buttons: 0, player_id: 2 },
      ]),
    );
    if (msg.payload.phase !== ENEMIES_ENTRANCE) break;
  }
  assert.notEqual(msg.payload.phase, ENEMIES_ENTRANCE);
  sim.delete();
});

test("coop: getPlayerIds is the local players' ids, null-padded", () => {
  const sim = new Module.WebSim();
  sim.startCoop(2);
  assert.deepEqual(JSON.parse(sim.getPlayerIds()), padded([1, 2]));
  // Restarting (e.g. back to the menu) must not keep the old second id.
  sim.startCoop(1);
  assert.deepEqual(JSON.parse(sim.getPlayerIds()), padded([1]));
  sim.delete();
});

test("pvp: getPlayerIds gives one distinct id per local player", () => {
  const sim = new Module.WebSim();
  sim.startPvP(2, 1);
  const ids = JSON.parse(sim.getPlayerIds());
  assert.equal(ids.length, SLOTS);
  const set = ids.filter((id) => id !== null);
  assert.equal(set.length, 2);
  assert.notEqual(set[0], set[1]);
  sim.delete();
});

// The C++ exception reaches JS opaque (a WebAssembly.Exception with no
// message), so this can only check that something was thrown.
test("pvp: more players than this machine's seats throws", () => {
  const sim = new Module.WebSim();
  const teamSize = SLOTS; // two teams of SLOTS is always one team too many
  assert.throws(() => sim.startPvP(2, teamSize));
  sim.delete();
});

test("pvp: 1v1 returns a PvP envelope with two teams", () => {
  const sim = new Module.WebSim();
  sim.startPvP(2, 1);
  const msg = step(sim, NO_INPUT);

  assert.equal(msg.type, PVP_GAME_STATE);
  assert.equal(msg.payload.teams.filter((t) => t !== null).length, 2);
  sim.delete();
});

console.log(failures ? `${failures} failed` : "all passed");
process.exit(failures ? 1 : 0);
