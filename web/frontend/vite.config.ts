import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

const repo = (path: string) =>
  fileURLToPath(new URL(`../../${path}`, import.meta.url));

export default defineConfig({
  resolve: {
    // The `web` CMake preset's output: supergame-web-sim.mjs/.wasm/.d.mts.
    // Build it first (`make web`); tsconfig.json's `paths` mirrors this.
    alias: { "@wasm": repo("build/web/bin") },
  },
  // Shared with the desktop client until client/ is retired (Phase 5).
  publicDir: repo("client/assets"),
  server: {
    fs: { allow: [repo(".")] },
  },
});
