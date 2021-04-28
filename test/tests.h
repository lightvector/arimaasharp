
/*
 * tests.h
 * Author: davidwu
 */

#ifndef TESTS_H
#define TESTS_H

#include "../core/global.h"
#include "../board/board.h"
#include "../board/gamerecord.h"

using namespace std;

class Rand;

namespace Tests
{
  void runEvalFeatures(const char* filename);

  //Basic tests--------------------------------------------------
  //Tests the included tests below as well as some extremely basic movegen and bitmap stuff.
  void runBasicTests(uint64_t seed);

  //Included in runBasicTests
  void testBlockades();
  void testUfDist(bool print);
  void testBasicSearch();

  //Tests that depend on positions-------------------------------
  void testGoalTree(const vector<GameRecord>& games, int trustDepth, int testDepth, int numRandomPerturbations, uint64_t seed);
  void testCapTree(const vector<GameRecord>& games, int trustDepth, int testDepth, int numRandomPerturbations, uint64_t seed);
  void testRelTacticsGen(const vector<GameRecord>& games, int numRandomPerturbations, uint64_t seed);
  void testElimTree(const vector<GameRecord>& games);

  //Misc Tests---------------
  void testBTGradient();
  void testPatterns();

  //Helpers------------------

  //Get a seed from a game suitable for initialize a Rand.
  uint64_t hashGame(const GameRecord& game, uint64_t seed);
  //Randomly perturb a board position. Leaves the platurn unchanged but does refresh start hash.
  void perturbBoard(Board& b, Rand& rand);

}

#endif
