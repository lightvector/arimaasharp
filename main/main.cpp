
/*
 * main.cpp
 * Author: davidwu
 */

#include <fstream>
#include <sstream>
#include <cstdlib>
#include "../core/global.h"
#include "../program/command.h"
#include "../program/init.h"
#include "../main/main.h"

using namespace std;

const char* const MainVersion::BOT_NAME = "sharp";
const char* const MainVersion::BOT_AUTHOR = "lightvector";
const char* const MainVersion::BOT_VERSION = "2015";

static int devMain(int argc, const char* const *argv);
static int callMain(int argc, const char* const *argv);

int main(int argc, char* argv[])
{
  //bool isDev = true;
  bool isDev = false;

  Init::init(isDev);

  if(isDev)
    return devMain(argc, argv);
  else if(argc >= 2 && string(argv[1]) == "aei")
    return MainFuncs::aei(argc,argv);
  else if(argc >= 2 && string(argv[1]) == "analyze")
    return MainFuncs::searchMoves(argc-1,&(argv[1]));
  else
    return MainFuncs::getMove(argc,argv);
}


//--------------------------------------------------------------------------------

static int devMain(int argc, const char* const *argv)
{
  if(argc <= 0)
    return EXIT_FAILURE;
  return callMain(argc-1,argv+1);
}

static void printCommands(const vector<MainFuncEntry>& commandArr)
{
  int i = 0;
  size_t numCommands = commandArr.size();
  for(size_t j = 0; j<numCommands; j++)
  {
    cout << commandArr[j].name << " ";
    if(++i >= 4)
    {i = 0; cout << endl;}
  }
  cout << endl;
}

static int callMain(int argc, const char* const *argv)
{
  vector<MainFuncEntry> commandArr
  {
    MainFuncEntry("aei", MainFuncs::aei),
    MainFuncEntry("getMove", MainFuncs::getMove),
    MainFuncEntry("version", MainFuncs::version),

    MainFuncEntry("searchPos", MainFuncs::searchPos),
    MainFuncEntry("searchMoves", MainFuncs::searchMoves),
    MainFuncEntry("evalPos", MainFuncs::evalPos),
    MainFuncEntry("evalMoves", MainFuncs::evalMoves),
    MainFuncEntry("viewPos", MainFuncs::viewPos),
    MainFuncEntry("viewMoves", MainFuncs::viewMoves),
    MainFuncEntry("moveOrderPos", MainFuncs::moveOrderPos),
    MainFuncEntry("moveOrderMoves", MainFuncs::moveOrderMoves),
    MainFuncEntry("loadPos", MainFuncs::loadPos),
    MainFuncEntry("loadMoves", MainFuncs::loadMoves),

    MainFuncEntry("playGame", MainFuncs::playGame),

    MainFuncEntry("avgBranching", MainFuncs::avgBranching),
    MainFuncEntry("avgGameLength", MainFuncs::avgGameLength),

    MainFuncEntry("learnMoveOrdering", MainFuncs::learnMoveOrdering),
    MainFuncEntry("testMoveOrdering", MainFuncs::testMoveOrdering),
    MainFuncEntry("testEvalOrdering", MainFuncs::testEvalOrdering),
    MainFuncEntry("testGameIterator", MainFuncs::testGameIterator),
    MainFuncEntry("testIteratorWeights", MainFuncs::testIteratorWeights),
    MainFuncEntry("viewMoveFeatures", MainFuncs::viewMoveFeatures),
    MainFuncEntry("testMoveFeatureSpeed", MainFuncs::testMoveFeatureSpeed),

    MainFuncEntry("findBlunders", MainFuncs::findBlunders),

    MainFuncEntry("winProbByEval", MainFuncs::winProbByEval),
    MainFuncEntry("modelEvalLikelihood", MainFuncs::modelEvalLikelihood),
    MainFuncEntry("optimizeEval", MainFuncs::optimizeEval),
    MainFuncEntry("evalComponentVariance", MainFuncs::evalComponentVariance),

    //MainFuncEntry("genGoalPatterns", MainFuncs::genGoalPatterns),
    //MainFuncEntry("verifyGoalPatterns", MainFuncs::verifyGoalPatterns),
    //MainFuncEntry("buildGoalTree", MainFuncs::buildGoalTree),
    MainFuncEntry("findAndSortGoals", MainFuncs::findAndSortGoals),
    //MainFuncEntry("testGoalLoseInOnePatterns", MainFuncs::testGoalLoseInOnePatterns),
    MainFuncEntry("findActualGoalsInTwo", MainFuncs::findActualGoalsInTwo),
    MainFuncEntry("findActualGoalsInThree", MainFuncs::findActualGoalsInThree),

    MainFuncEntry("testPrunePatterns", MainFuncs::testPrunePatterns),

    MainFuncEntry("testBook", MainFuncs::testBook),

    MainFuncEntry("runBasicTests", MainFuncs::runBasicTests),
    MainFuncEntry("runGoalPosesTest", MainFuncs::runGoalPosesTest),
    MainFuncEntry("runGoalTest", MainFuncs::runGoalTest),
    MainFuncEntry("runCapTest", MainFuncs::runCapTest),
    MainFuncEntry("runRelTacticsGenTest", MainFuncs::runRelTacticsGenTest),
    MainFuncEntry("runElimTest", MainFuncs::runElimTest),
    MainFuncEntry("runBTGradientTest", MainFuncs::runBTGradientTest),
    MainFuncEntry("runBasicSearchTest", MainFuncs::runBasicSearchTest),
    MainFuncEntry("runThreadTests", MainFuncs::runThreadTests),
    MainFuncEntry("runUFDistTests", MainFuncs::runUFDistTests),
    MainFuncEntry("runGoalMapTest", MainFuncs::runGoalMapTest),

    MainFuncEntry("runGoalMapSandbox", MainFuncs::runGoalMapSandbox),
    MainFuncEntry("runTimeTests", MainFuncs::runTimeTests),

    MainFuncEntry("createBenchmark", MainFuncs::createBenchmark),
    MainFuncEntry("createEvalBadGoodTest", MainFuncs::createEvalBadGoodTest),
    MainFuncEntry("randomizePosFile", MainFuncs::randomizePosFile),

    MainFuncEntry("ponderHitRate", MainFuncs::ponderHitRate),
    MainFuncEntry("checkSpeed", MainFuncs::checkSpeed),
    MainFuncEntry("testTactics", MainFuncs::testTactics),

    MainFuncEntry("boardProperties", MainFuncs::boardProperties),

    MainFuncEntry("getCacheLineSize", MainFuncs::getCacheLineSize),

  };

  if(argc <= 0)
  {
    printCommands(commandArr);
    return EXIT_SUCCESS;
  }

  //Construct map------------------------------------------------------------------------
  map<string,MainFuncEntry> commandMap;
  size_t numCommands = commandArr.size();
  for(size_t i = 0; i<numCommands; i++)
    commandMap[commandArr[i].name] = commandArr[i];

  //Lookup up desired command in map-----------------------------------------------------
  string commandName = string(argv[0]);

  if(commandMap.find(commandName) == commandMap.end())
  {
    cout << "Command not found: " << commandName << endl;
    printCommands(commandArr);
    return EXIT_FAILURE;
  }

  //Here we go---------------------------------------------------------------------------
  MainFuncEntry entry = commandMap[commandName];
  DEBUGASSERT(entry.func != NULL);

  int result = entry.func(argc, argv);
  if(result != EXIT_SUCCESS)
    cout << "Nonsuccessful error code returned" << endl;
  return result;
}

//---------------------------------------------------------------------------------------

MainFuncEntry::MainFuncEntry()
:func(NULL),name()
{}
MainFuncEntry::MainFuncEntry(const char* n, MainFunc f)
:func(f),name(n)
{}
MainFuncEntry::MainFuncEntry(const string& n, MainFunc f)
:func(f),name(n)
{}
MainFuncEntry::~MainFuncEntry()
{}


