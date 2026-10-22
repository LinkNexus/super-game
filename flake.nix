{
  description = "supergame: cross-compilation toolchain for the Windows client";

  # Pinned to a revision, deliberately, rather than tracking a branch.
  #
  # This is the same nixpkgs revision the machine's own system flake is
  # locked to, so the cross toolchain here is the one already in the nix
  # store: nothing to download, and the Windows client is built with the
  # same GCC as everything else. More importantly, nixpkgs builds
  # mingwW64 GCC with `--enable-threads=mcf`, which makes every link pull
  # in mcfgthread -- an upstream change to that thread model would
  # silently change how this project links, so the revision moves only
  # when someone chooses to move it.
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

      # Tools that run on the build machine, needed by both shells.
      #
      # python3 is not optional and not obvious: vendor/mbedtls's CMake
      # requires a Python 3 interpreter to generate sources. The first
      # version of this flake left it out and the build worked anyway, by
      # silently borrowing the interpreter from the global profile --
      # visible in build/*/CMakeCache.txt as
      # /run/current-system/sw/bin/python3.14. Declaring it here is
      # precisely what lets the machine's global package set shrink
      # without breaking this project.
      buildTools = [
        pkgs.cmake
        pkgs.ninja
        pkgs.python3
      ];
    in {
      # `nix develop .#mingw` -- the environment `cmake --preset
      # windows-client-release` needs.
      #
      # Why a shell rather than installing the cross compiler globally:
      # nixpkgs' cross support is built around stdenv. The compiler is a
      # wrapper that takes its include and library paths from environment
      # variables exported by stdenv's setup hook, so the bare binary on
      # PATH cannot find mcfgthread (the thread library its own driver
      # implicitly links) and dies in CMake's compiler check with
      # "cannot find -lmcfgthread". The exact same compiler works inside
      # this shell, because here the setup hook has run.
      #
      # cmake/ninja are nativeBuildInputs: they run on this machine, not
      # on the Windows target. The project's own
      # cmake/mingw-w64-toolchain.cmake then finds the cross gcc/g++ and
      # windres by name on PATH, which is why stdenv.cc is listed too.
      #
      # Project dependencies are deliberately absent: raylib, ixwebsocket,
      # uWebSockets and mbedtls are git submodules under vendor/ built by
      # add_subdirectory, so they need no packaging here.
      mingw = cross.mkShell {
        nativeBuildInputs = [cross.stdenv.cc] ++ buildTools;
      };

      # `nix develop` / what direnv loads via .envrc -- the native build
      # (server, tests, and the non-cross client presets). Everything the
      # project links is a git submodule under vendor/ built by
      # add_subdirectory, so this is just a compiler plus build tools:
      # mkShell supplies the host stdenv's cc, and there are no
      # find_package or find_library calls anywhere outside vendor/ to
      # satisfy.
      default = pkgs.mkShell {
        nativeBuildInputs = buildTools;
      };
    });
  };
}
