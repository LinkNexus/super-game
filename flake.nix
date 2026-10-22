{
  description = "supergame: cross-compilation toolchain for the Windows client";

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
      buildTools = [
        pkgs.cmake
        pkgs.ninja
        pkgs.python3
      ];
    in {
      # `nix develop .#mingw` -- the environment `cmake --preset
      # windows-client-release` needs.
      mingw = cross.mkShell {
        nativeBuildInputs = [cross.stdenv.cc] ++ buildTools;
      };

      default = pkgs.mkShell {
        nativeBuildInputs = buildTools;
      };
    });
  };
}
