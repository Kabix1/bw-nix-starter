{
  description = "Brood War bot development starter for Linux";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in { default = pkgs.mkShell.override { stdenv = pkgs.llvmPackages.stdenv; } {
          packages = with pkgs; [ cmake ninja pkg-config SDL2 git python3 ];
          shellHook = ''
            export BW_STARTER_ROOT="''${BW_STARTER_ROOT:-$PWD}"
            export BW_ENV_ROOT="$BW_STARTER_ROOT"
            export PATH="$BW_STARTER_ROOT/scripts:$PATH"
            export BW_BUILD_JOBS="''${BW_BUILD_JOBS:-2}"
          '';
        }; });
    };
}
