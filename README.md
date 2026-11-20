# Brood War bot starter with Nix

Build your own C++ Brood War bot, test it against bundled opponents, and send
the source back for our challenge. This repository includes the patched OpenBW
engine/BWAPI interface, a small editable demo bot, maps, test opponents, and
pinned development dependencies. The Linux fixes are already applied.

## 1. Install Linux and Nix

**Windows:** open administrator PowerShell, run `wsl --install`, restart, and
open Ubuntu. Check `wsl --list --verbose` in PowerShell to confirm it uses WSL2.
See [Microsoft's WSL guide](https://learn.microsoft.com/en-us/windows/wsl/install).
Run the remaining commands inside Ubuntu and keep this project in your Linux
home directory, for example `~/bw-nix-starter`.

**Linux:** use your normal terminal.

Install Nix using the [official installation guide](https://nix.dev/install-nix).
For Linux, or WSL2 with systemd enabled, the documented multi-user installer is:

```sh
curl -L https://nixos.org/nix/install | sh -s -- --daemon
```

For WSL2 without systemd, use the guide's single-user command instead:

```sh
curl -L https://nixos.org/nix/install | sh -s -- --no-daemon
```

Open a new terminal and check `nix --version`. No separate compiler, Python,
BWAPI installer, Wine, Colima, or GPU setup is needed for this workflow.

## 2. Get the project and build

With Git installed and access to this private repository:

```sh
git clone https://github.com/Kabix1/bw-nix-starter.git
cd bw-nix-starter
./dev bw-build
./dev bw-test --gtest_list_tests
```

Alternatively, use GitHub's **Code → Download ZIP**, extract it, and open a
terminal in the extracted folder before running `./dev bw-build`. Private
repository access is required for either download method.

The build command downloads the pinned compiler and libraries and builds the
engine, demo bot, test opponents and tests. It uses two build jobs by default.
Allow several GiB of free disk space and at least 4 GiB RAM for this setup.
You can choose more jobs on a larger computer:

```sh
BW_BUILD_JOBS=4 ./dev bw-build
```

`./dev` enters an interactive shell; type `exit` to leave. Equivalently, from
the project root, use `nix --extra-experimental-features 'nix-command flakes' develop ./nix`.
The small `nix/` directory holds the flake so game data and
build outputs do not get copied into Nix's source snapshots.

## 3. Add your game data and run a game

Get the files from Blizzard's public **[StarEdit ZIP](http://download.blizzard.com/pub/starcraft/StarEdit/StarEdit.zip)**
(about 101 MB), linked in Blizzard's [official announcement](https://us.forums.blizzard.com/en/starcraft/t/public-staredit-download/457).
Extract the ZIP and locate the three MPQ files; you do not need to run or install
the editor. You can also copy them from a classic Brood War installation.

Put the files under `data/`, renaming them to these exact names:

```text
data/STARDAT.MPQ
data/BROODAT.MPQ
data/patch_rt.mpq
```

The wrapper creates the mixed-case filenames OpenBW needs at runtime. The MPQs
are not included in this repository and are not downloaded automatically.

```sh
./dev bw-check
```

This builds, lists scenarios, plays one headless game against Steamhammer,
checks that the Linux module can be loaded, and saves a replay under
`build/test/replays/`. A PASS means the game ran successfully; the starter bot
will usually lose. Running all tests without a filter starts longer suites and
includes tests that expect a stronger bot to win.

## 4. Write your bot

Edit `StardustDevEnvironment/src/DemoAIModule.cpp`, especially `onStart()` and
`onFrame()`. The initial bot gathers resources, trains workers and adds supply;
it is a development example, not a competitive army controller.

```sh
./dev bw-build
./dev bw-check
```

The build produces `build/src/libDemoAIModule.so`. Keep the exported `gameInit`
and `newAIModule` functions in `src/Dll.cpp`. Use the included BWAPI headers;
switching to a different BWAPI release can break compatibility with our runner.

For unit-control-only contests, implement the agreed combat rules in your bot
and use the same API. The supplied smoke test is a full-game development test;
it does not provide a fixed-army micro arena. Agree on the contest type,
armies/maps, upgrades, frame limits and starting-position swaps before competing.

The bundled `AGENTS.md` gives these commands to a coding assistant as well.

## 5. Send your submission

Send the source and `nix/flake.lock`, with your bot's race and any runtime/model
requirements. We rebuild both submissions on the same host and run a series
with swapped starting positions, saving replays. Linux `.so` and Windows
`.dll` files cannot be loaded directly on the Mac; source is the common format.

From this directory, you can make a source submission with:

```sh
tar --exclude='./build' --exclude='./data' --exclude='./.git' --exclude='./.nix-compat' \
  -czf ../friend-bot-source.tar.gz .
```

Remove any personal secrets/editor state you added before sharing. Models your
bot actually uses should be included separately with instructions.

## Optional match/replay tools

The sources and build helpers for our match runner and replay viewer are included:

```sh
./dev ./scripts/bw-build-match
./dev ./scripts/bw-build-watch
```

To export an existing replay without a desktop window, after a game has staged
the runtime MPQ aliases:

```sh
./dev env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/watch-ui-build/replay-viewer /absolute/path/game.rep build/test \
  --frames 0,120 --size 320 240 --output build/replay-frames
```

The Mac-specific graphical launcher and existing champion binaries are not
included. Interactive graphics and WSL itself have not been validated in this
package's Linux test environment.

## Versions and troubleshooting

`nix/flake.lock` pins Nixpkgs at
`b6018f87da91d19d0ab4cf979885689b469cdd41`; the tested compiler is Clang 21.1.7.
The source is based on StardustDevEnvironment
`37127c60cdd86bf0b81898a5a476747e4633efd7`, with the patches recorded in
`PROVENANCE.json` and `patches/` already applied. Third-party source and license
files are retained in `StardustDevEnvironment/`.

The x86-64 Linux verification completed a 6,792-frame game in a Colima
VM with 2 CPUs and 4 GiB RAM. It also built the match runner and viewer and
exported replay frames. The packaged `./dev bw-check` workflow subsequently
completed another game after 6,637 frames. See [VALIDATION.md](VALIDATION.md)
for scope and details. These checks establish execution, not playing strength.

In our QEMU-emulated Colima test, Nix 2.36.0 had a pseudoterminal syscall issue;
Nix 2.28.7 resolved it. If you encounter that exact error while emulating x86,
you can obtain the tested Nix version from the pinned dependencies:

```sh
nix --extra-experimental-features 'nix-command flakes' build \
  github:NixOS/nixpkgs/b6018f87da91d19d0ab4cf979885689b469cdd41#nixVersions.nix_2_28 \
  --out-link .nix-compat
PATH="$PWD/.nix-compat/bin:$PATH" ./dev bw-build
```

This workaround was needed for emulation, not established as a requirement on
native Linux. For build errors, share the failing command and its output, plus
`./dev clang++ --version` and `uname -m`.
