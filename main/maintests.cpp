
/*
 * maintests.cpp
 * Author: davidwu
 */

#include <fstream>
#include <cstdlib>
#include <algorithm>
#include "../core/global.h"
#include "../core/rand.h"
#include "../core/timer.h"
#include "../board/board.h"
#include "../board/boardtrees.h"
#include "../board/boardhistory.h"
#include "../board/boardmovegen.h"
#include "../board/gamerecord.h"
#include "../board/hashtable.h"
#include "../learning/learner.h"
#include "../learning/gameiterator.h"
#include "../search/searchutils.h"
#include "../search/searchmovegen.h"
#include "../search/search.h"
#include "../test/tests.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../program/init.h"
#include "../main/main.h"

using namespace std;

int MainFuncs::runBasicTests(int argc, const char* const *argv)
{
  const char* usage =
      "<-seed seed>";
  const char* required = "";
  const char* allowed = "seed";
  const char* empty = "";
  const char* nonempty = "seed";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 1)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  uint64_t seed = Command::getUInt64(flags,"seed",0);
  cout << "SEED " << seed << endl;

  Tests::runBasicTests(seed);

  return EXIT_SUCCESS;
}

int MainFuncs::runGoalPosesTest(int argc, const char* const *argv)
{
  const char* usage =
      "posesfile <-idx idx>";
  const char* required = "";
  const char* allowed = "idx";
  const char* empty = "";
  const char* nonempty = "idx";

  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  cout << Global::concat(argv,argc," ") << endl << Command::gitRevisionId() << endl;

  string boardFile = mainCommand[1];
  vector<BoardRecord> records;
  int startIdx = 0;
  bool ignoreConsistency = true;
  if(!Command::isSet(flags,"idx"))
    records = ArimaaIO::readBoardRecordFile(boardFile,ignoreConsistency);
  else
  {
    startIdx = Command::getInt(flags,"idx");
    records.push_back(ArimaaIO::readBoardRecordFile(boardFile,startIdx,ignoreConsistency));
  }

  int numRecords = records.size();
  for(int i = 0; i<numRecords; i++)
  {
    BoardRecord& record = records[i];
    if(!map_contains(record.keyValues,"cangoal"))
    {
      cout << record << endl;
      Global::fatalError("keyvalues for position does not contain 'cangoal'");
    }
    string target = Global::toLower(Global::trim(record.keyValues["cangoal"]));
    bool targetCanGoal = target != "false";
    int targetGoalSteps = (target != "true" && target != "false") ? Global::stringToInt(target) : -1;

    Board b = record.board;
    int goalSteps = BoardTrees::goalDist(b,b.player,4);
    if((goalSteps <= 4) != targetCanGoal || (targetGoalSteps != -1 && goalSteps != targetGoalSteps))
    {
      cout << "Idx: " << (i+startIdx) << endl;
      cout << "Tree: " << goalSteps << endl;
      cout << record << endl;
      Global::fatalError("Failed on goal test");
    }
  }

  cout << "Passed all goal tests" << endl;
  return EXIT_SUCCESS;
}

int MainFuncs::runGoalTest(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile trustdepth testdepth <-idx idx> <-seed seed> <-perturbations num perturbations>";
  const char* required = "";
  const char* allowed = "idx perturbations seed";
  const char* empty = "";
  const char* nonempty = "idx perturbations seed";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 4)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string movesFile = mainCommand[1];
  int trustDepth = Global::stringToInt(mainCommand[2]);
  int testDepth = Global::stringToInt(mainCommand[3]);

  uint64_t seed = Command::getUInt64(flags,"seed",0);
  cout << Global::concat(argv,argc," ") << endl << Command::gitRevisionId() << endl;
  cout << "SEED " << seed << endl;

  vector<GameRecord> games;
  if(!Command::isSet(flags,"idx"))
    games = ArimaaIO::readMovesFile(movesFile);
  else
    games.push_back(ArimaaIO::readMovesFile(movesFile,Command::getInt(flags,"idx")));

  int perturbations = Command::getInt(flags,"perturbations",0);

  cout << "Loaded games, starting..." << endl;
  ClockTimer timer;
  Tests::testGoalTree(games,trustDepth,testDepth,perturbations,seed);
  double seconds = timer.getSeconds();
  cout << "Test time: " << seconds << endl;

  return EXIT_SUCCESS;
}

int MainFuncs::runCapTest(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile trustdepth testdepth <-idx idx> <-seed seed> <-perturbations num perturbations>";
  const char* required = "";
  const char* allowed = "idx perturbations seed";
  const char* empty = "";
  const char* nonempty = "idx perturbations seed";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 4)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string movesFile = mainCommand[1];
  int trustDepth = Global::stringToInt(mainCommand[2]);
  int testDepth = Global::stringToInt(mainCommand[3]);

  uint64_t seed = Command::getUInt64(flags,"seed",0);
  cout << Global::concat(argv,argc," ") << endl << Command::gitRevisionId() << endl;
  cout << "SEED " << seed << endl;

  vector<GameRecord> games;
  if(!Command::isSet(flags,"idx"))
    games = ArimaaIO::readMovesFile(movesFile);
  else
    games.push_back(ArimaaIO::readMovesFile(movesFile,Command::getInt(flags,"idx")));

  int perturbations = Command::getInt(flags,"perturbations",0);

  cout << "Loaded games, starting..." << endl;
  ClockTimer timer;
  Tests::testCapTree(games,trustDepth,testDepth,perturbations,seed);
  double seconds = timer.getSeconds();
  cout << "Test time: " << seconds << endl;

  return EXIT_SUCCESS;
}

int MainFuncs::runRelTacticsGenTest(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile <-idx idx> <-seed seed> <-perturbations num perturbations>";
  const char* required = "";
  const char* allowed = "idx perturbations seed";
  const char* empty = "";
  const char* nonempty = "idx perturbations seed";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string movesFile = mainCommand[1];

  uint64_t seed = Command::getUInt64(flags,"seed",0);
  cout << Global::concat(argv,argc," ") << endl << Command::gitRevisionId() << endl;
  cout << "SEED " << seed << endl;

  vector<GameRecord> games;
  if(!Command::isSet(flags,"idx"))
    games = ArimaaIO::readMovesFile(movesFile);
  else
    games.push_back(ArimaaIO::readMovesFile(movesFile,Command::getInt(flags,"idx")));

  int perturbations = Command::getInt(flags,"perturbations",0);

  cout << "Loaded games, starting..." << endl;
  ClockTimer timer;
  Tests::testRelTacticsGen(games,perturbations,seed);
  double seconds = timer.getSeconds();
  cout << "Test time: " << seconds << endl;

  return EXIT_SUCCESS;
}

int MainFuncs::runElimTest(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}
  string movesFile = mainCommand[1];

  cout << Global::concat(argv,argc," ") << endl << Command::gitRevisionId() << endl;

  vector<GameRecord> games = ArimaaIO::readMovesFile(movesFile);

  cout << "Loaded games, starting..." << endl;
  ClockTimer timer;
  Tests::testElimTree(games);
  double seconds = timer.getSeconds();
  cout << "Test time: " << seconds << endl;

  return EXIT_SUCCESS;
}

int MainFuncs::runBTGradientTest(int argc, const char* const *argv)
{
  const char* usage =
      "";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  Tests::testBTGradient();

  return EXIT_SUCCESS;
}

int MainFuncs::runBasicSearchTest(int argc, const char* const *argv)
{
  const char* usage =
      "";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  Tests::testBasicSearch();
  return EXIT_SUCCESS;
}

int MainFuncs::runUFDistTests(int argc, const char* const *argv)
{
  const char* usage = "";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  Tests::testUfDist(true);
  return EXIT_SUCCESS;
}

static void removeDupHashes(vector<hash_t>& sortedVec)
{
  int size = sortedVec.size();
  int newSize = 0;
  for(int i = 0; i<size; i++)
  {
    if(i == size-1 || sortedVec[i] != sortedVec[i+1])
      sortedVec[newSize++] = sortedVec[i];
  }
  sortedVec.resize(newSize);
}

static int genMoves(Board& b, move_t* mv, pla_t pla, bool winLossPrune)
{
  int num = -1;
  if(winLossPrune)
  {
    num = SearchMoveGen::genWinDefMovesIfNeeded(b,mv,NULL,4-b.step);
    //No possible defenses for loss
    if(num == 0)
      return num;
  }
  //Regular movegen
  if(num == -1)
  {
    num = 0;
    if(b.step < 3)
      num += BoardMoveGen::genPushPulls(b,pla,mv+num);
    num += BoardMoveGen::genSteps(b,pla,mv+num);
    if(b.step != 0)
      mv[num++] = PASSMOVE;
  }
  return num;
}

static void findAllNonLosingHashes(Board& b, pla_t pla, bool winLossPrune,
    FastExistsHashTable* table, vector<hash_t>& hashes, vector<hash_t>* reportWhenHashesNotIn)
{
  hash_t hash = b.sitCurrentHash;
  if(table->lookup(hash))
    return;
  table->record(hash);

  //Terminate if the turn has switched
  if(b.player != pla)
  {
    //If now your opponent can win, prune.
    if(!b.isGoal(pla) && !b.isRabbitless(b.player) &&
       (BoardTrees::goalDist(b,b.player,4) < 5 || BoardTrees::canElim(b,b.player,4)))
      return;

    //Record hash for a legal nonlosing move
    hashes.push_back(b.sitCurrentHash);

    if(reportWhenHashesNotIn != NULL)
    {
      bool found = false;
      vector<hash_t>& vec = *reportWhenHashesNotIn;
      int size = vec.size();
      for(int i = 0; i<size; i++)
        if(b.sitCurrentHash == vec[i])
        {found = true; break;}
      if(!found)
        cout << "Hash not found for:" << endl << b << endl;
    }

    return;
  }

  //Generate moves
  move_t mv[256];
  int num = genMoves(b,mv,pla,winLossPrune);

  UndoMoveData uData;
  for(int i = 0; i<num; i++)
  {
    move_t move = mv[i];
    bool suc = b.makeMoveLegalNoUndo(move,uData);
    DEBUGASSERT(suc);
    if(b.posCurrentHash != b.posStartHash)
      findAllNonLosingHashes(b,pla,winLossPrune,table,hashes,reportWhenHashesNotIn);
    b.undoMove(uData);
  }
}

static void goalMapTestSinglePos(Board& b, const Board& orig, int gameIdx, FastExistsHashTable* table,
    vector<hash_t>& fullHashes, vector<hash_t>& winDefHashes)
{
  fullHashes.clear();
  winDefHashes.clear();

  table->clear();
  findAllNonLosingHashes(b,b.player,false,table,fullHashes,NULL);
  if(!Board::areIdentical(b,orig))
  {
    cout << b << endl;
    cout << "Orig:" << endl;
    cout << orig << endl;
    Global::fatalError("Boards no longer identical after findAllNonLosingHashes full");
  }

  table->clear();
  findAllNonLosingHashes(b,b.player,true,table,winDefHashes,NULL);
  if(!Board::areIdentical(b,orig))
  {
    cout << b << endl;
    cout << "Orig:" << endl;
    cout << orig << endl;
    Global::fatalError("Boards no longer identical after findAllNonLosingHashes winprune");
  }

  std::sort(fullHashes.begin(),fullHashes.end());
  std::sort(winDefHashes.begin(),winDefHashes.end());

  removeDupHashes(fullHashes);
  removeDupHashes(winDefHashes);
  if(fullHashes.size() != winDefHashes.size())
  {
    cout << "Game " << gameIdx << endl;
    cout << b << endl;
    cout << "Full: " << fullHashes.size() << endl;
    cout << "Windef: " << winDefHashes.size() << endl;
    vector<hash_t> temp;
    table->clear();
    findAllNonLosingHashes(b,b.player,false,table,temp,&winDefHashes);
    Global::fatalError("Full search and windef movegen don't produce same move set");
  }
}

int MainFuncs::runGoalMapTest(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile <-idx idx> <-allposes> <-perturbations N> <-branchstatsonly>";
  const char* required = "";
  const char* allowed = "idx allposes perturbations branchstatsonly";
  const char* empty = "allposes branchstatsonly";
  const char* nonempty = "idx perturbations";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string movesFile = mainCommand[1];

  bool allPoses = Command::isSet(flags,"allposes");
  int perturbations = Command::getInt(flags,"perturbations",0);
  bool branchStatsOnly = Command::isSet(flags,"branchstatsonly");

  GameIterator* iter = NULL;
  if(!Command::isSet(flags,"idx"))
    iter = new GameIterator(movesFile);
  else
    iter = new GameIterator(movesFile, Command::getInt(flags,"idx"));
  iter->setDoPrint(false);

  cout << "Loaded " << iter->getTotalNumGames() << " games, starting..." << endl;
  ClockTimer timer;

  FastExistsHashTable* table = new FastExistsHashTable(20);
  Rand rand(0);

  vector<hash_t> fullHashes;
  vector<hash_t> winDefHashes;

  int64_t count = 0;
  int64_t fullBranchSum[5] = {0,0,0,0,0};
  int64_t winDefBranchSum[5] = {0,0,0,0,0};
  while(iter->next())
  {
    Board b = iter->getBoard();
    DEBUGASSERT(b.step == 0);

    if(perturbations > 0)
    {
      for(int i = 0; i<perturbations; i++)
      {
        Board copy = b;
        Tests::perturbBoard(copy,rand);
        //If the game's ended, skip it
        if(copy.getWinner() != NPLA)
          continue;
        //Always skip the position if you yourself can win right away
        if(BoardTrees::goalDist(copy,copy.player,4) <= 4 || BoardTrees::canElim(copy,copy.player,4))
          continue;
        //Unless we're doing all positions, restrict only to the positions where the opp is threatening goal
        if(!allPoses && BoardTrees::goalDist(copy,gOpp(copy.player),4) >= 5)
          continue;

        Board copycopy = copy;
        if(!branchStatsOnly)
          goalMapTestSinglePos(copycopy,copy, iter->getGameIdx(), table, fullHashes, winDefHashes);
      }
    }

    //If the game's ended, skip it
    if(b.getWinner() != NPLA)
      continue;
    //Always skip the position if you yourself can win right away
    if(BoardTrees::goalDist(b,b.player,4) <= 4 || BoardTrees::canElim(b,b.player,4))
      continue;
    //Unless we're doing all positions, restrict only to the positions where the opp is threatening goal
    if(!allPoses && BoardTrees::goalDist(b,gOpp(b.player),4) >= 5)
      continue;

    if(!branchStatsOnly)
      goalMapTestSinglePos(b,iter->getBoard(), iter->getGameIdx(), table, fullHashes, winDefHashes);

    count++;

    //Generate moves
    move_t mv[256];
    for(int stepsLeft = 1; stepsLeft <= 4; stepsLeft++)
    {
      Board copy = b;
      copy.setPlaStep(b.player,4-stepsLeft);
      int fullBranch = genMoves(copy,mv,copy.player,false);
      int winDefBranch = genMoves(copy,mv,copy.player,true);
      fullBranchSum[stepsLeft] += fullBranch;
      winDefBranchSum[stepsLeft] += winDefBranch;
    }

    if(count % 1000 == 0)
      cout << "Game " << iter->getGameIdx() << " "
           << "Count " << count << " "
           << "AvgFullBranch "
           << ((double)fullBranchSum[1] / count) << " "
           << ((double)fullBranchSum[2] / count) << " "
           << ((double)fullBranchSum[3] / count) << " "
           << ((double)fullBranchSum[4] / count) << " "
           << "AvgWinDefBranch "
           << ((double)winDefBranchSum[1] / count) << " "
           << ((double)winDefBranchSum[2] / count) << " "
           << ((double)winDefBranchSum[3] / count) << " "
           << ((double)winDefBranchSum[4] / count) << " "
           << endl;
  }

  cout << "Game " << iter->getGameIdx() << " "
       << "Count " << count << " "
       << "AvgFullBranch "
       << ((double)fullBranchSum[1] / count) << " "
       << ((double)fullBranchSum[2] / count) << " "
       << ((double)fullBranchSum[3] / count) << " "
       << ((double)fullBranchSum[4] / count) << " "
       << "AvgWinDefBranch "
       << ((double)winDefBranchSum[1] / count) << " "
       << ((double)winDefBranchSum[2] / count) << " "
       << ((double)winDefBranchSum[3] / count) << " "
       << ((double)winDefBranchSum[4] / count) << " "
       << endl;

  double seconds = timer.getSeconds();
  cout << "Test time: " << seconds << endl;

  delete iter;
  delete table;
  return EXIT_SUCCESS;
}

int MainFuncs::runGoalMapSandbox(int argc, const char* const *argv)
{
  const char* usage = "posfile steps";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  cout << "Testing..." << endl;

  Board b;
  loc_t rloc;
  SearchMoveGen::RelevantArea relBuf;

  if(mainCommand.size() < 3)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  b = ArimaaIO::readBoardFile(mainCommand[1],0,true);
  cout << b << endl;

  BoardTrees::goalDist(b,gOpp(b.player),4);
  cout << Board::writeMove(b,b.goalTreeMove) << endl;

  SearchMoveGen::getGoalDefRelArea(b,Global::stringToInt(mainCommand[2]),rloc,relBuf);
  cout << relBuf.stepTo << "\n" << relBuf.stepFrom << endl;
  cout << relBuf.pushOpp << "\n" << relBuf.pullOpp << endl;
  cout << relBuf.pushOppTo << "\n" << relBuf.pullOppTo << endl;

  //Board copy = b;
  //FastExistsHashTable* table = new FastExistsHashTable(20);
  //vector<hash_t> fullHashes;
  //vector<hash_t> winDefHashes;
  //goalMapTestSinglePos(copy,b,0,table,fullHashes,winDefHashes);

  //findAllNonLosingHashes(b,b.player,true,table,winDefHashes,NULL);
  //cout << winDefHashes.size() << endl;

  return EXIT_SUCCESS;
}

