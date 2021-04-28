/*
 * mainponder.cpp
 * Author: davidwu
 */

#include <cstdlib>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/boardtrees.h"
#include "../board/gamerecord.h"
#include "../learning/learner.h"
#include "../learning/gameiterator.h"
#include "../learning/featuremove.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

static move_t directRootMoveOrder(Board& b, const BoardHistory& hist, ExistsHashTable* fullmoveHash, SearchParams& params)
{
  vector<move_t> mvVec;
  SearchMoveGen::genFullMoves(b, hist, mvVec, 4, true, 4, fullmoveHash, NULL, NULL);

  DEBUGASSERT(hist.maxTurnBoardNumber == hist.maxTurnNumber);
  Board copy = b;
  void* moveFeatureData = params.rootMoveFeatureSet.getPosData(copy,hist,b.player);
  vector<double> featureWeights;
  params.rootMoveFeatureSet.getFeatureWeights(moveFeatureData,params.rootMoveFeatureWeights,featureWeights);

  int size = mvVec.size();
  UndoMoveData uData;
  double bestVal = -1e20;
  move_t bestMove = ERRMOVE;
  for(int m = 0; m < size; m++)
  {
    move_t move = mvVec[m];
    copy.makeMove(move,uData);
    double val = params.rootMoveFeatureSet.getFeatureSum(
        copy,moveFeatureData,b.player,move,hist,featureWeights);
    copy.undoMove(uData);
    if(val > bestVal)
    {
      bestVal = val;
      bestMove = move;
    }
  }

  params.rootMoveFeatureSet.freePosData(moveFeatureData);
  return bestMove;
}


int MainFuncs::ponderHitRate(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile "
      "<-seed seed> <-poskeepprop prop>";
  const char* required = "";
  const char* allowed = "t seed poskeepprop";
  const char* empty = "";
  const char* nonempty = "t seed poskeepprop";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() < 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  cout << Command::gitRevisionId() << endl;
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;

  double posKeepProp = Command::getDouble(flags,"poskeepprop",1.0);

  vector<string> modeNames;
  vector<double> modeSearchTimes;
  vector<double> modeBiases;
  vector<double> modeDepth;

  modeNames.push_back(string("t1p5"));
  modeNames.push_back(string("t1p5bias"));
  //modeNames.push_back(string("t15"));
  //modeNames.push_back(string("t15bias"));
  modeNames.push_back(string("d6"));
  modeNames.push_back(string("d7"));
  modeNames.push_back(string("d8"));
  modeNames.push_back(string("d9"));

  modeSearchTimes.push_back(1.5);
  modeSearchTimes.push_back(1.5);
  //modeSearchTimes.push_back(15.0);
  //modeSearchTimes.push_back(15.0);
  modeSearchTimes.push_back(90.0);
  modeSearchTimes.push_back(90.0);
  modeSearchTimes.push_back(90.0);
  modeSearchTimes.push_back(90.0);

  modeBiases.push_back(0.0);
  modeBiases.push_back(500.0);
  //modeBiases.push_back(0.0);
  //modeBiases.push_back(500.0);
  modeBiases.push_back(0.0);
  modeBiases.push_back(0.0);
  modeBiases.push_back(0.0);
  modeBiases.push_back(0.0);

  modeDepth.push_back(SearchParams::AUTO_DEPTH);
  modeDepth.push_back(SearchParams::AUTO_DEPTH);
  //modeDepth.push_back(SearchParams::AUTO_DEPTH);
  //modeDepth.push_back(SearchParams::AUTO_DEPTH);
  modeDepth.push_back(6);
  modeDepth.push_back(7);
  modeDepth.push_back(8);
  modeDepth.push_back(9);


  uint64_t seed = Command::getUInt64(flags,"seed",0);
  Rand rand(seed);
  cout << "Seed " << seed << endl;

  SearchParams params;
  BradleyTerry rootLearner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(rootLearner);
  params.setRootFancyPrune(true);
  params.setAvoidEarlyTrade(true);
  params.setRandomize(true,SearchParams::DEFAULT_RANDOM_DELTA,rand.nextUInt64());
  params.setMoveImmediatelyIfOnlyOneNonlosing(true);
  Searcher searcher(params);
  ExistsHashTable* fullmoveHash = new ExistsHashTable(21);

  int totalCount = 0;
  vector<int> correctCounts;
  vector<move_t> guessMoves;
  vector<bool> wasCorrect;
  int numModes = modeSearchTimes.size();
  for(int i = 0; i<numModes; i++)
  {
    correctCounts.push_back(0);
    guessMoves.push_back(ERRMOVE);
    wasCorrect.push_back(false);
  }

  GameIterator iter(mainCommand[1]);
  iter.setDoFilter(true);
  iter.setDoFilterWins(true);
  iter.setDoFilterWinsInTwo(true);
  iter.setDoFilterLemmings(true);
  iter.setNumInitialToFilter(2);
  iter.setNumLoserToFilter(2);
  iter.setPosKeepProp(posKeepProp);
  iter.setRandSeed(seed);

  cout << "Game,Platurn,Count,Actual,";
  for(int i = 0; i<numModes; i++)
    cout << modeNames[i] << ",";
  for(int i = 0; i<numModes; i++)
    cout << modeNames[i] << ",";
  for(int i = 0; i<numModes; i++)
    cout << modeNames[i] << ",";
  cout << endl;

  //Here we go!-------------------------------------------------------
  while(iter.next())
  {
    Board b = iter.getBoard();
    const BoardHistory& hist = iter.getHist();
    move_t actualMove = iter.getNextMove();

    DEBUGASSERT(b.getWinner() == NPLA);
    DEBUGASSERT(actualMove != ERRMOVE);
    DEBUGASSERT(b.step == 0);

    hash_t correctHash = b.sitHashAfterLegal(actualMove);
    totalCount++;

    //Test each mode/configuration to see if it finds the move
    for(int i = 0; i<numModes; i++)
    {
      double time = modeSearchTimes[i];
      move_t bestMove;
      if(time <= 0)
      {
        bestMove = directRootMoveOrder(b,hist,fullmoveHash,params);
      }
      else
      {
        searcher.params.defaultMaxTime = time;
        searcher.params.defaultMaxDepth = modeDepth[i];
        searcher.params.setRootBiasStrength(modeBiases[i]);
        searcher.searchID(b,hist,false);
        bestMove = searcher.getMove();
      }

      guessMoves[i] = bestMove;
      hash_t chosenHash = b.sitHashAfterLegal(bestMove);
      if(chosenHash == correctHash)
      {
        wasCorrect[i] = true;
        correctCounts[i]++;
      }
      else
        wasCorrect[i] = false;

    }

    cout
    << iter.getGameIdx() << ","
    << Board::writePlaTurn(b.turnNumber) << ","
    << totalCount << ","
    << Board::writeMove(b,actualMove,true) << ",";
    for(int i = 0; i<numModes; i++)
      cout << (int)wasCorrect[i] << ",";
    for(int i = 0; i<numModes; i++)
      cout << correctCounts[i] << ",";
    for(int i = 0; i<numModes; i++)
      cout << Board::writeMove(b,guessMoves[i],true) << ",";
    cout << endl;
  }
  delete fullmoveHash;

  return EXIT_SUCCESS;
}


