# Linux validation

The patched engine/API and bot sources were built on x86-64 Linux under QEMU
in a Colima VM with 2 CPUs and 4 GiB RAM. Toolchain: Clang 21.1.7, CMake 4.1.2
and Nix 2.28.7, using the checked-in Nixpkgs lock.

- OpenBW/BWAPI, demo bot, Steamhammer, Locutus and test executable built.
- Eight test scenarios enumerated successfully.
- `Steamhammer.RunOne` completed a 6,792-frame game; opponent exited and a replay was saved.
- Standalone match runner and replay viewer built; frames 0 and 120 rendered.
- Linux shared-module entry points loaded successfully.

The friend-facing Nix setup was then extracted into a fresh directory, its
file manifest verified, its relocated development shell checked, and its
missing-game-data error checked. All 2,715 bundled engine/bot/map/tool source
files matched the previously verified build inputs. Its `./dev bw-check` and
optional build helpers passed, including another completed game of 6,637 frames.
This packaging check reused the matching C++ build cache; the preceding test
performed the source build.

Both smoke-test games were accepted execution checks; the demo bot lost.
They do not establish playing strength. WSL itself and interactive graphics
have not been tested. The default game starts the demo bot as Protoss.

The GitHub README adds clone instructions and the Blizzard game-data download
link. Runtime source, Nix lock and build helpers match the validated archive.
`MANIFEST.json` records this initial repository snapshot; later bot edits are
tracked by Git.
