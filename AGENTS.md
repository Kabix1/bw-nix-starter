# Brood War bot starter

- Use `./dev` to enter the pinned Nix development shell, or `./dev COMMAND`.
- Build with `./dev bw-build`; run the single-game smoke test with `./dev bw-check`.
- Bot behavior is in `StardustDevEnvironment/src/DemoAIModule.cpp`.
- Module entry points are in `StardustDevEnvironment/src/Dll.cpp`; preserve
  `gameInit` and `newAIModule` for compatibility with the match runner.
- Game data goes in `data/` as described in README.md. Keep MPQs, build outputs,
  credentials and local editor state out of source submissions.
- Run named scenarios explicitly. Running the test binary without a filter also
  starts long twenty-game suites. A smoke-test PASS does not mean the bot won.
- Preserve `nix/flake.lock` and the bundled engine/API when changing bot logic so
  another machine can rebuild the submission against the same interface.
