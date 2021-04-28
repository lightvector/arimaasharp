/*
 * mainspeed.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../core/rand.h"
#include "../core/timer.h"
#include "../board/board.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../board/gamerecord.h"
#include "../eval/threats.h"
#include "../eval/eval.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

static void* newStuff()
{return (void*)(0);}
static void deleteStuff(void* p)
{(void)p;}

static uint64_t doStuffMode0(Board& b, void* p)
{
  (void)p;
  return b.sitCurrentHash;
}

static uint64_t doStuffMode1(Board& b, void* p)
{
  (void)p;
  return (uint64_t)Eval::evaluate(b,b.player,0,NULL);

  //move_t mv[512];
  //int hm[512];
  //int num = SearchMoveGen::genWinDefMovesIfNeeded(b,mv,hm,4-b.step);
  //return num > 0 ? num + mv[num-1] : 0;

  //move_t mv[2048];
  //return SearchMoveGen::genRelevantTactics(b,true,mv,NULL);

  //move_t mv[2048];
  //int hm[2048];
  //int num = 0;
  //int numSteps = 4-b.step;
  //num += SearchMoveGen::genCaptureMoves(b,numSteps,mv+num,hm+num);
  //num += SearchMoveGen::genCaptureDefMoves(b,numSteps,false,mv+num,hm+num);
  //num += SearchMoveGen::genBlockadeMoves(b,mv+num,hm+num);
  //num += BoardTrees::genSevereGoalThreats(b,b.player,numSteps,mv+num);
  //return num;

  //Bitmap goalThreatDistIs[9];
  //Threats::getGoalDistBitmapEst(b,b.player,goalThreatDistIs);
  //return goalThreatDistIs[8].countBits();

  //int goalDist[BSIZE];
  //Threats::getGoalDistEst(b,b.player,goalDist,8-b.step);
  //return (uint64_t)(goalDist[TRAP0]+goalDist[TRAP1]+goalDist[TRAP2]+goalDist[TRAP3]);
}

static int recursiveStuff(Board& b, int depth, int mode, int& numCalls, void* p)
{
  move_t mv[256];
  int num = 0;
  if(b.step < 3)
    num += BoardMoveGen::genPushPulls(b,b.player,mv+num);
  num += BoardMoveGen::genSteps(b,b.player,mv+num);
  if(b.step > 0 && b.posCurrentHash != b.posStartHash)
    mv[num++] = PASSMOVE;

  uint64_t sum = 0;
  UndoMoveData udata;
  for(int i = 0; i<num; i++)
  {
    int oldBoardStep = b.step;
    bool suc = b.makeMoveLegalNoUndo(mv[i],udata);
    DEBUGASSERT(suc);
    numCalls++;

    bool changedPlayer = (b.step == 0);
    int numSteps = changedPlayer ? 4-oldBoardStep : b.step-oldBoardStep;

    if(mode == 0) sum += doStuffMode0(b,p);
    else          sum += doStuffMode1(b,p);

    int newDepth = depth - numSteps;
    if(newDepth > 0)
      sum += recursiveStuff(b,newDepth,mode,numCalls,p);
    b.undoMove(udata);
  }
  return sum;
}

int MainFuncs::checkSpeed(int argc, const char* const *argv)
{
  const char* usage =
      "posfile <-idx IDX>";
  const char* required = "d";
  const char* allowed = "idx";
  const char* empty = "";
  const char* nonempty = "d idx";
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

  int depth = Command::getInt(flags,"d");

  void* p = newStuff();

  double modeSecs[2] = {0,0};
  int modeCounts[2] = {0,0};
  uint64_t modeSum[2] = {0,0};
  Rand rand;
  ClockTimer timer;
  for(int i = 0; i<(int)records.size(); i++)
  {
    cout << "Running position " << i << endl;
    int mode = rand.nextInt(0,1);
    int otherMode = 1-mode;
    Board b = records[i].board;
    int numCalls1 = 0;
    int numCalls2 = 0;

    timer.reset();
    modeSum[mode] += recursiveStuff(b,depth,mode,numCalls1,p);
    modeCounts[mode] += numCalls1;
    modeSecs[mode] += timer.getSeconds();

    timer.reset();
    modeSum[otherMode] += recursiveStuff(b,depth,otherMode,numCalls2,p);
    modeCounts[otherMode] += numCalls2;
    modeSecs[otherMode] += timer.getSeconds();
  }

  deleteStuff(p);

  cout << "Num positions: " << records.size() << endl;
  cout << "Mode 0" << endl;
  cout << "Sum to prevent optimizer: " << modeSum[0] << endl;
  cout << "Num calls: " << modeCounts[0] << endl;
  cout << "Time: " << modeSecs[0] << endl;
  cout << "Calls/s: " << ((double)modeCounts[0] / modeSecs[0]) << endl;
  cout << endl;
  cout << "Mode 1" << endl;
  cout << "Sum to prevent optimizer: " << modeSum[1] << endl;
  cout << "Num calls: " << modeCounts[1] << endl;
  cout << "Time: " << modeSecs[1] << endl;
  cout << "Calls/s: " << ((double)modeCounts[1] / modeSecs[1]) << endl;
  cout << endl;

  return EXIT_SUCCESS;
}





