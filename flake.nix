{
  description = "supergame: native, Windows-cross and web (Emscripten) toolchains";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/8edc0c72e3a38faf5434e40d1f19431125fe4b30";

  outputs = {
    self,
    nixpkgs,
  }: let
    forEachSystem =
      nixpkgs.lib.genAttrs
      ["aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux"];
  in {
    devShells = forEachSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      cross = pkgs.pkgsCross.mingwW64;
      # GCC 14 to match CI's Debian trixie image (bump together). Darwin keeps
      # clang, like macOS CI, but LLVM 22: the default (21) and 20 ASan
      # runtimes deadlock before main() on macOS 26.
      crossStdenv = cross.gcc14Stdenv;
      nativeStdenv =
        if pkgs.stdenv.hostPlatform.isLinux
        then pkgs.gcc14Stdenv
        else pkgs.llvmPackages_22.stdenv;
      buildTools = [
        pkgs.cmake
        pkgs.ninja
        pkgs.python3
      ];
    in {
      # `nix develop .#mingw` -- the environment `cmake --preset
      # windows-client-release` needs.
      mingw = (cross.mkShell.override {stdenv = crossStdenv;}) {
        nativeBuildInputs = [crossStdenv.cc] ++ buildTools;
      };

      default = (pkgs.mkShell.override {stdenv = nativeStdenv;}) {
        nativeBuildInputs = buildTools;
      };

      # `nix develop .#web` -- for `cmake --preset web`. Same emscripten as
      # CI's emsdk image (bump together). Emscripten writes its sysroot cache
      # next to itself by default, which is the read-only nix store.
      web = pkgs.mkShell {
        nativeBuildInputs = [pkgs.emscripten pkgs.nodejs] ++ buildTools;
        shellHook = ''
          export EM_CACHE="''${XDG_CACHE_HOME:-$HOME/.cache}/supergame-emscripten"
        '';
      };
    });
  };
}
