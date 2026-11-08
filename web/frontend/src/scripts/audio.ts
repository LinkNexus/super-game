const ctx = new AudioContext();
const masterGain = new GainNode(ctx, {
  gain: 1,
});
const musicGain = new GainNode(ctx, { gain: 0 });
const sfxGain = new GainNode(ctx, { gain: 0.8 });

musicGain.connect(masterGain);
sfxGain.connect(masterGain);
masterGain.connect(ctx.destination);

const music = new Audio("backgroundsound.ogg");
music.loop = true;
new MediaElementAudioSourceNode(ctx, { mediaElement: music }).connect(
  musicGain,
);

const SFX = ["explosion", "boss_hit"] as const;
export type SfxName = (typeof SFX)[number];

const buffers: Partial<Record<SfxName, AudioBuffer>> = {};

export async function loadAudio() {
  await Promise.all(
    SFX.map(async (name) => {
      try {
        const res = await fetch(`${name}.ogg`);
        buffers[name] = await ctx.decodeAudioData(await res.arrayBuffer());
      } catch (e) {
        console.error(`Failed to load audio ${name}:`, e);
      }
    }),
  );
}

function unlock() {
  if (ctx.state === "suspended") ctx.resume();
  music.play().catch(() => {});
}

export function playSfx(name: SfxName) {
  const buffer = buffers[name];
  if (!buffer || ctx.state !== "running") return;

  const src = new AudioBufferSourceNode(ctx, { buffer });
  src.connect(sfxGain);
  src.start();
}

let musicTarget: number | null = null;
export function setMusicVolume(volume: number) {
  if (volume === musicTarget) return;

  musicTarget = volume;
  musicGain.gain.setTargetAtTime(volume, ctx.currentTime, 0.3);
}

export function toggleMute() {
  masterGain.gain.value = masterGain.gain.value > 0 ? 0 : 1;
}

window.addEventListener("keydown", unlock, {
  once: true,
});
window.addEventListener("pointerdown", unlock, {
  once: true,
});
