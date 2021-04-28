/*
 * mainmatch
 * Author: davidwu
 */

#include <fstream>
#include <cstdlib>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../eval/eval.h"
#include "../search/search.h"
#include "../setups/setup.h"
#include "../program/command.h"
#include "../program/init.h"
#include "../program/match.h"
#include "../main/main.h"

using namespace std;

static void initGoldParams(SearchParams& params)
{
  BradleyTerry learner = BradleyTerry::inputFromFile(MoveFeature::getArimaaFeatureSet(),"move_bti20_4.txt");
  params.initRootMoveFeatures(learner);
  params.setRandomize(true,SearchParams::DEFAULT_RANDOM_DELTA,0);
  params.setAvoidEarlyTrade(true);

  params.setDefaultMaxDepthTime(10,1);

  //setRootFancyPrune(true);
  //setRootFixedPrune(true,0.05,false);
  //setDisablePartialSearch(true);
}

static void initSilverParams(SearchParams& params)
{
  BradleyTerry learner = BradleyTerry::inputFromFile(MoveFeature::getArimaaFeatureSet(),"move_bti20_4.txt");
  params.initRootMoveFeatures(learner);
  params.setRandomize(true,SearchParams::DEFAULT_RANDOM_DELTA,0);
  params.setAvoidEarlyTrade(true);

  params.setDefaultMaxDepthTime(10,1);

  //setRootFancyPrune(true);
  //setRootFixedPrune(true,0.05,false);
  //setDisablePartialSearch(true);
}

int MainFuncs::playGame(int argc, const char* const *argv)
{
  const char* usage =
      "<-seed seed> ";
  const char* required = "";
  const char* allowed = "seed";
  const char* empty = "";
  const char* nonempty = "seed";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  SearchParams searchParamsGold;
  SearchParams searchParamsSilver;
  initGoldParams(searchParamsGold);
  initSilverParams(searchParamsSilver);
  Searcher searcherGold(searchParamsGold);
  Searcher searcherSilver(searchParamsSilver);

  Rand rand;
  uint64_t seed = rand.getSeed();
  if(Command::isSet(flags,"seed"))
    seed = Command::getUInt64(flags,"seed");;
  cout << "SEED " << seed << endl;

  Match::runGame(searcherGold,searcherSilver,Setup::SETUP_RATED_RANDOM,true,seed);

  return EXIT_SUCCESS;
}

