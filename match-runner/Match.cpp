#include "BWTest.h"
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <string>

static BWAPI::Race race(const char *value) {
  const std::string r(value);
  if (r == "Protoss")
    return BWAPI::Races::Protoss;
  if (r == "Terran")
    return BWAPI::Races::Terran;
  if (r == "Zerg")
    return BWAPI::Races::Zerg;
  std::cerr << "Unsupported race " << r << std::endl;
  std::exit(1);
}
static BWAPI::AIModule *loadModule(const char *path, int side) {
  auto handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    std::cerr << "dlopen failed: " << dlerror() << std::endl;
    std::exit(1);
  }
  auto init =
      reinterpret_cast<void (*)(BWAPI::Game *)>(dlsym(handle, "gameInit"));
  auto create =
      reinterpret_cast<BWAPI::AIModule *(*)()>(dlsym(handle, "newAIModule"));
  if (!init || !create) {
    std::cerr << "Missing module entry points in " << path << std::endl;
    std::exit(1);
  }
  const char *policy = std::getenv(side == 0 ? "BW_POLICY_A" : "BW_POLICY_B");
  if (policy) setenv("BW_EVOLVE_POLICY", policy, 1);
  else unsetenv("BW_EVOLVE_POLICY");
  init(BWAPI::BroodwarPtr);
  auto module = create();
  if (!module) {
    std::cerr << "Module factory returned null: " << path << std::endl;
    std::exit(1);
  }
  std::cout << "Loaded side " << side << " module " << path << std::endl;
  return module;
}
TEST(Match, Run) {
  const char *a = std::getenv("BW_MODULE_A"), *b = std::getenv("BW_MODULE_B");
  const char *ra = std::getenv("BW_RACE_A"), *rb = std::getenv("BW_RACE_B");
  ASSERT_TRUE(a && b && ra && rb);
  BWTest t;
  t.map = Maps::GetOne("(2)Destination.scx");
  ASSERT_NE(t.map, nullptr);
  const char *seed = std::getenv("BW_MATCH_SEED");
  t.randomSeed = seed ? std::stoi(seed) : 50224;
  t.frameLimit = 40000;
  t.timeLimit = std::getenv("BW_WATCH_MODE") ? 1800 : 120;
  t.myRace = race(ra);
  t.opponentRace = race(rb);
  t.myModule = [=] { return loadModule(a, 0); };
  t.opponentModule = [=] { return loadModule(b, 1); };
  t.expectWin = false;
  t.replayName = "match";
  int fa = 0, fb = 0;
  t.onStartMine = [] { std::ofstream("player0.txt") << BWAPI::Broodwar->self()->getID() << std::endl; };
  t.onStartOpponent = [] { std::ofstream("player1.txt") << BWAPI::Broodwar->self()->getID() << std::endl; };
  t.onFrameMine = [&] { fa = BWAPI::Broodwar->getFrameCount(); };
  t.onFrameOpponent = [&] { fb = BWAPI::Broodwar->getFrameCount(); };
  auto end = [](int side, int frame, bool won) {
    int buildings = 0;
    for (auto u : BWAPI::Broodwar->self()->getUnits())
      if (u->exists() && u->getType().isBuilding())
        ++buildings;
    std::ofstream f("result" + std::to_string(side) + ".txt");
    f << won << ' ' << frame << ' ' << buildings << std::endl;
  };
  t.onEndMine = [&](bool won) { end(0, fa, won); };
  t.onEndOpponent = [&](bool won) { end(1, fb, won); };
  t.onTermination = [](bool opponent, bool left) {
    std::ofstream f(opponent ? "termination1.txt" : "termination0.txt");
    f << left << std::endl;
  };
  t.run();
  int wins[2], frames[2], buildings[2], left[2];
  for (int i = 0; i < 2; ++i) {
    std::ifstream result("result" + std::to_string(i) + ".txt"),
        termination("termination" + std::to_string(i) + ".txt");
    ASSERT_TRUE(bool(result >> wins[i] >> frames[i] >> buildings[i]));
    ASSERT_TRUE(bool(termination >> left[i]));
    std::cout << "side=" << i << " won=" << wins[i] << " frame=" << frames[i]
              << " buildings=" << buildings[i] << " forced=" << left[i]
              << std::endl;
    EXPECT_EQ(left[i], 0);
    EXPECT_LT(frames[i], t.frameLimit - 10);
  }
  EXPECT_NE(wins[0], wins[1]);
}
