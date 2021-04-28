
/*
 * main.h
 * Author: davidwu
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "../core/global.h"

using namespace std;

typedef int (*MainFunc)(int, const char* const*);

struct MainFuncEntry
{
  MainFunc func;
  string name;

  MainFuncEntry();
  MainFuncEntry(const string& name, MainFunc func);
  MainFuncEntry(const char* name, MainFunc func);
  ~MainFuncEntry();
};

namespace MainVersion
{
  extern const char* const BOT_NAME;
  extern const char* const BOT_AUTHOR;
  extern const char* const BOT_VERSION;
}

//A bunch of top-level routines. Return value is an exit code.
namespace MainFuncs
{
  //AEI game player----------------------------------------------------
  int aei(int argc, const char* const* argv);

  //Getmove program, for legacy bot script-----------------------------
  int getMove(int argc, const char* const* argv);

  //Version------------------------------------------------------------
  int version(int argc, const char* const *argv);

  //Positions----------------------------------------------------------
  int searchPos(int argc, const char* const *argv);
  int searchMoves(int argc, const char* const *argv);
  int evalPos(int argc, const char* const *argv);
  int evalMoves(int argc, const char* const *argv);
  int viewPos(int argc, const char* const *argv);
  int viewMoves(int argc, const char* const *argv);
  int moveOrderPos(int argc, const char* const *argv);
  int moveOrderMoves(int argc, const char* const *argv);
  int loadPos(int argc, const char* const *argv);
  int loadMoves(int argc, const char* const *argv);

  //Match--------------------------------------------------------------
  int playGame(int argc, const char* const *argv);

  //Stats--------------------------------------------------------------
  int avgBranching(int argc, const char* const *argv);
  int avgGameLength(int argc, const char* const *argv);

  //Move Learning------------------------------------------------------
  int learnMoveOrdering(int argc, const char* const *argv);
  int testMoveOrdering(int argc, const char* const *argv);
  int testEvalOrdering(int argc, const char* const *argv);

  int testGameIterator(int argc, const char* const *argv);
  int testIteratorWeights(int argc, const char* const *argv);
  int viewMoveFeatures(int argc, const char* const *argv);
  int testMoveFeatureSpeed(int argc, const char* const *argv);

  int findBlunders(int argc, const char* const *argv);

  //Eval---------------------------------------------------------------
  int winProbByEval(int argc, const char* const *argv);
  int modelEvalLikelihood(int argc, const char* const *argv);
  int optimizeEval(int argc, const char* const *argv);
  int evalComponentVariance(int argc, const char* const *argv);

  //Goal Pattern Generation--------------------------------------------
  int genGoalPatterns(int argc, const char* const *argv);
  int verifyGoalPatterns(int argc, const char* const *argv);
  int buildGoalTree(int argc, const char* const *argv);
  int findAndSortGoals(int argc, const char* const *argv);
  int testGoalLoseInOnePatterns(int argc, const char* const *argv);
  int findActualGoalsInTwo(int argc, const char* const *argv);
  int findActualGoalsInThree(int argc, const char* const *argv);

  //Prune Pattern Generation-------------------------------------------
  int testPrunePatterns(int argc, const char* const *argv);

  //Book---------------------------------------------------------------
  int testBook(int argc, const char* const *argv);

  //Tests--------------------------------------------------------------
  int runBasicTests(int argc, const char* const *argv);
  int runGoalPosesTest(int argc, const char* const *argv);
  int runGoalTest(int argc, const char* const *argv);
  int runCapTest(int argc, const char* const *argv);
  int runRelTacticsGenTest(int argc, const char* const *argv);
  int runElimTest(int argc, const char* const *argv);
  int runBTGradientTest(int argc, const char* const *argv);
  int runBasicSearchTest(int argc, const char* const *argv);
  int runThreadTests(int argc, const char* const *argv);
  int runUFDistTests(int argc, const char* const *argv);
  int runGoalMapTest(int argc, const char* const *argv);

  int runGoalMapSandbox(int argc, const char* const *argv);
  int runTimeTests(int argc, const char* const *argv);

  //Misc---------------------------------------------------------------
  int createBenchmark(int argc, const char* const *argv);
  int createEvalBadGoodTest(int argc, const char* const *argv);
  int randomizePosFile(int argc, const char* const *argv);

  int ponderHitRate(int argc, const char* const *argv);
  int checkSpeed(int argc, const char* const *argv);
  int testTactics(int argc, const char* const *argv);

  int boardProperties(int argc, const char* const *argv);

  int getCacheLineSize(int argc, const char* const *argv);


}


#endif /* MAIN_H_ */
