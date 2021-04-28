/*
 * mainposmoves.cpp
 * Author: davidwu
 */

#include <fstream>
#include <cstdlib>
#include <algorithm>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/gamerecord.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../eval/eval.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../program/init.h"
#include "../main/main.h"

using namespace std;

static void initParams(SearchParams& params, int numThreads,
    bool useHashMem, uint64_t hashMem, double rootBias, bool safePruning, bool noNullMove, bool noReduce,
    bool avoidEarlyStuff, pla_t mainPla,
    const string& excludes, const string& excludeSteps, const Board* b,
    bool randomize, int rDelta, uint64_t seed)
{
  BradleyTerry rootLearner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(rootLearner);
  //BradleyTerry treeLearner = BradleyTerry(MoveFeatureLite::getArimaaFeatureSet(),0);
  //params.initTreeMoveFeatures(treeLearner);

  params.setNumThreads(numThreads);
  params.setMoveImmediatelyIfOnlyOneNonlosing(false);
  //params.rootMaxNonRelTactics = 0;
  params.setRootFancyPrune(true);
  params.setUnsafePruneEnable(!safePruning);
  params.setNullMoveEnable(!safePruning && !noNullMove);
  params.setAllowReduce(!noReduce);
  params.setAvoidEarlyTrade(avoidEarlyStuff);
  params.setAvoidEarlyBlockade(avoidEarlyStuff);
  params.setOverrideMainPla(true,mainPla);
  params.setRootBiasStrength(rootBias);

  if(useHashMem)
  {
    int exp = SearchHashTable::getExp(hashMem);
    params.mainHashExp = exp;
    if(params.fullMoveHashExp > exp-1)
      params.fullMoveHashExp = max(exp-1,0);
  }

  if(randomize)
    params.setRandomize(true,rDelta,seed);

  if(excludes.length() > 0)
  {
    DEBUGASSERT(b != NULL);
    vector<string> pieces = Global::split(excludes,',');
    for(int i = 0; i<(int)pieces.size(); i++)
    {
      string s = Global::trim(pieces[i]);
      if(s.length() <= 0)
        continue;
      hash_t hash;
      move_t move;
      if(Global::tryStringToUInt64(s,hash))
      {
        params.excludeRootMoves.push_back(hash);
        continue;
      }
      else if(Board::tryReadMove(s,move))
      {
        Board copy = *b;
        bool suc = copy.makeMoveLegalNoUndo(move);
        if(!suc)
          Global::fatalError("Illegal move in exclude list: " + s);
        if(copy.step != 0)
        {
          suc = copy.makeMoveLegalNoUndo(PASSMOVE);
          DEBUGASSERT(suc);
        }
        params.excludeRootMoves.push_back(copy.sitCurrentHash);
        continue;
      }
      else
        Global::fatalError("Could not parse move or hash to exclude: " + s);
    }
  }

  if(excludeSteps.length() > 0)
  {
    DEBUGASSERT(b != NULL);
    vector<string> pieces = Global::split(excludeSteps,',');
    for(int i = 0; i<(int)pieces.size(); i++)
    {
      string s = Global::trim(pieces[i]);
      if(s.length() <= 0)
        continue;
      move_t move;
      if(Board::tryReadMove(s,move))
      {
        params.excludeRootStepSets.push_back(move);
        continue;
      }
      else
        Global::fatalError("Could not parse stepset to exclude: " + s);
    }
  }
}

static bool handlePosFromCommand(const vector<string>& mainCommand, vector<Board>& boards, vector<BoardHistory>& hists, int idx);
static bool handleMovesFromCommand(const vector<string>& mainCommand, vector<Board>& boards, vector<BoardHistory>& hists, int idx);

static void applyExtraMoves(Board& board, BoardHistory& hist, const vector<move_t>& moves);

static void performSingleSearch(const Board& b, const BoardHistory& hist, int depth, double time, const TimeControl& tc, const SearchParams& params);
static void performMultiSearch(const vector<Board>& boards, const vector<BoardHistory>& hists,int depth, double time, const TimeControl& tc, const SearchParams& params);

static void performSingleEval(const Board& b, pla_t mainPla);
static void performMultiEval(const vector<Board>& boards, pla_t mainPla);

static void performSingleDisplay(const Board& b, const BoardHistory& hist, bool viewPlacement, bool sandBox);
static void performMultiDisplay(const vector<Board>& boards, bool viewPlacement, bool sandBox);

static void performSingleOrder(const Board& b, const BoardHistory& hist, const string* weightsFile, move_t target, bool showFeatures);


//------------------------------------------------------------------------------------------

int MainFuncs::searchPos(int argc, const char* const *argv)
{
  const char* usage =
      "posfile "
      "<-d depth> "
      "<-t time> "
      "<-v moves to view> "
      "<-vb view better moves only> "
      "<-ve view exact> "
      "<-novieweval> "
      "<-m extra moves to make> "
      "<-idx idx to view in multiboard file> "
      "<-threads threads> "
      "<-tc timecontrol pm/rs/%/rm/gm/pmm/au/rc/gc> "
      "<-hashmem hashmem> "
      "<-rootbias bias> "
      "<-safeprune> "
      "<-nonullmove> "
      "<-noreduce> "
      "<-avoidearly> "
      "<-mainpla pla> "
      "<-rdelta rdelta for randomizing>"
      "<-seed for randomizing, random if unspecified>"
      "<-exclude commaseparated list of hashes or moves to forbid>"
      "<-excludesteps commaseparated list of stepsets to forbid>";
  const char* required = "";
  const char* allowed = "d t tc v vb ve novieweval threads m hashmem idx rootbias safeprune nonullmove noreduce avoidearly mainpla exclude excludesteps rdelta seed";
  const char* empty = "vb ve novieweval safeprune nonullmove noreduce avoidearly";
  const char* nonempty = "d t tc threads m hashmem idx rootbias exclude excludesteps rdelta seed mainpla";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  int depth = Command::getInt(flags,"d",SearchParams::AUTO_DEPTH);
  double time = Command::getDouble(flags,"t",SearchParams::AUTO_TIME);
  bool doView = Command::isSet(flags,"v");
  bool viewBetterOnly = Command::isSet(flags,"vb");
  bool viewExact = Command::isSet(flags,"ve");
  string viewMoves = Command::getString(flags,"v","");
  bool noViewEval = Command::isSet(flags,"novieweval");
  int numThreads = Command::getInt(flags,"threads",1);
  double rootBias = Command::getDouble(flags,"rootbias",SearchParams::DEFAULT_ROOT_BIAS);
  bool safePruning = Command::isSet(flags,"safeprune");
  bool noNullMove = Command::isSet(flags,"nonullmove");
  bool noReduce = Command::isSet(flags,"noreduce");
  bool avoidEarlyStuff = Command::isSet(flags,"avoidearly");
  pla_t mainPla = Command::isSet(flags,"mainpla") ? Board::readPla(Command::getString(flags,"mainpla")) : NPLA;
  bool randomize = Command::isSet(flags,"rdelta");
  int rDelta = Command::getInt(flags,"rdelta",0);

  uint64_t seed = Command::getUInt64(flags,"seed",0);
  if(randomize && !Command::isSet(flags,"seed"))
  {Rand rand; seed = rand.nextUInt64();}

  TimeControl tc;
  if(Command::isSet(flags,"tc"))
    tc = ArimaaIO::readTC(flags["tc"],0,0,0);

  bool useHashMem = Command::isSet(flags,"hashmem");
  uint64_t hashMem = 0;
  if(useHashMem)
    hashMem = ArimaaIO::readMem(flags["hashmem"]);

  string excludes = Command::getString(flags,"exclude",string());
  string excludeSteps = Command::getString(flags,"excludesteps",string());

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool success = handlePosFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  if(boards.size() == 1)
  {
    Board board = boards[0];
    BoardHistory hist = hists[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    applyExtraMoves(board,hist,extraMoves);

    SearchParams params;
    initParams(params,numThreads,useHashMem,hashMem,rootBias,safePruning,noNullMove,noReduce,avoidEarlyStuff,mainPla,excludes,excludeSteps,&board,randomize,rDelta,seed);
    if(doView)
    {
      params.initView(board,viewMoves,viewBetterOnly,!viewBetterOnly,viewExact);
      params.viewEvaluate = !noViewEval;
    }

    performSingleSearch(board, hist, depth, time, tc, params);
  }
  else
  {
    if(doView)
      Global::fatalError("Cannot view search for multiple positions");
    if(Command::isSet(flags,"m"))
      Global::fatalError("Cannot use -m for multiple positions");
    if(Command::isSet(flags,"exclude"))
      Global::fatalError("Cannot use -exclude for multiple positions");
    SearchParams params;
    initParams(params,numThreads,useHashMem,hashMem,rootBias,safePruning,noNullMove,noReduce,avoidEarlyStuff,mainPla,excludes,excludeSteps,NULL,randomize,rDelta,seed);
    performMultiSearch(boards, hists, depth, time, tc, params);
  }
  return EXIT_SUCCESS;
}

int MainFuncs::searchMoves(int argc, const char* const *argv)
{  
  const char* usage =
      "movesfile [platurn] "
      "<-d depth> "
      "<-t time> "
      "<-v moves to view> "
      "<-vb view better moves only> "
      "<-ve view exact> "
      "<-novieweval> "
      "<-m extra moves to make> "
      "<-idx idx to view in multigame file> "
      "<-threads threads> "
      "<-tc timecontrol pm/rs/%/rm/gm/pmm/au/rc/gc> "
      "<-hashmem hashmem> "
      "<-rootbias bias> "
      "<-safeprune> "
      "<-noreduce> "
      "<-avoidearly> "
      "<-asymeval> "
      "<-rdelta rdelta for randomizing>"
      "<-seed for randomizing, random if unspecified>"
      "<-exclude commaseparated list of hashes or moves to forbid>"
      "<-excludesteps commaseparated list of stepsets to forbid>";
  const char* required = "";
  const char* allowed = "d t tc v vb ve novieweval threads m hashmem idx rootbias safeprune nonullmove noreduce avoidearly mainpla exclude excludesteps rdelta seed";
  const char* empty = "vb ve novieweval safeprune nonullmove noreduce avoidearly";
  const char* nonempty = "d t tc threads m hashmem idx rootbias exclude excludesteps rdelta seed mainpla";

  if(!Init::ARIMAA_DEV) {
    usage =
      "MOVESFILE [TURN] "
      "<-d MAXDEPTH (limit search depth in steps)> "
      "<-t MAXTIME (limit search time in seconds)> "
      "<-m MOVES (specify extra moves to make on top of the game position)> "
      "<-threads NUM (default 1)> "
      "<-hashmem MEM (explicitly specify hashtbl memory, like 256M)> "
      "<-safeprune (disable theoretically unsafe pruning)> "
      "<-exclude commaseparated list of hashes or whole-turn moves to forbid> "
      "<-excludesteps commaseparated list of stepsets to forbid, e.g 'Ed5n, Rd1n Cd1n' forbids all moves containing Ed5n OR both of Rd1n Cd1n>";
    required = "";
    allowed = "d t threads m hashmem safeprune exclude excludesteps";
    empty = "safeprune";
    nonempty = "d t threads m hashmem exclude excludesteps";
  }

  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  int depth = Command::getInt(flags,"d",SearchParams::AUTO_DEPTH);
  double time = Command::getDouble(flags,"t",SearchParams::AUTO_TIME);
  bool doView = Command::isSet(flags,"v");
  bool viewBetterOnly = Command::isSet(flags,"vb");
  bool viewExact = Command::isSet(flags,"ve");
  string viewMoves = Command::getString(flags,"v","");
  bool noViewEval = Command::isSet(flags,"novieweval");
  int numThreads = Command::getInt(flags,"threads",1);
  double rootBias = Command::getDouble(flags,"rootbias",SearchParams::DEFAULT_ROOT_BIAS);
  bool safePruning = Command::isSet(flags,"safeprune");
  bool noNullMove = Command::isSet(flags,"nonullmove");
  bool noReduce = Command::isSet(flags,"noreduce");
  bool avoidEarlyStuff = Command::isSet(flags,"avoidearly");
  pla_t mainPla = Command::isSet(flags,"mainpla") ? Board::readPla(Command::getString(flags,"mainpla")) : NPLA;
  bool randomize = Command::isSet(flags,"rdelta");
  int rDelta = Command::getInt(flags,"rdelta",0);

  uint64_t seed = Command::getUInt64(flags,"seed",0);
  if(randomize && !Command::isSet(flags,"seed"))
  {Rand rand; seed = rand.nextUInt64();}

  TimeControl tc;
  if(Command::isSet(flags,"tc"))
    tc = ArimaaIO::readTC(flags["tc"],0,0,0);

  bool useHashMem = Command::isSet(flags,"hashmem");
  uint64_t hashMem = 0;
  if(useHashMem)
    hashMem = ArimaaIO::readMem(flags["hashmem"]);

  string excludes = Command::getString(flags,"exclude",string());
  string excludeSteps = Command::getString(flags,"excludesteps",string());

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool success = handleMovesFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  if(boards.size() == 1)
  {
    Board board = boards[0];
    BoardHistory hist = hists[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    applyExtraMoves(board,hist,extraMoves);

    SearchParams params;
    initParams(params,numThreads,useHashMem,hashMem,rootBias,safePruning,noNullMove,noReduce,avoidEarlyStuff,mainPla,excludes,excludeSteps,&board,randomize,rDelta,seed);
    if(doView)
    {
      params.initView(board,viewMoves,viewBetterOnly,!viewBetterOnly,viewExact);
      params.viewEvaluate = !noViewEval;
    }

    performSingleSearch(board, hist, depth, time, tc, params);
  }
  else
  {
    if(doView)
      Global::fatalError("Cannot view search for multiple positions");
    if(Command::isSet(flags,"m"))
      Global::fatalError("Cannot use -m for multiple positions");
    if(Command::isSet(flags,"exclude"))
      Global::fatalError("Cannot use -exclude for multiple positions");
    SearchParams params;
    initParams(params,numThreads,useHashMem,hashMem,rootBias,safePruning,noNullMove,noReduce,avoidEarlyStuff,mainPla,excludes,excludeSteps,NULL,randomize,rDelta,seed);
    performMultiSearch(boards, hists, depth, time, tc, params);
  }
  return EXIT_SUCCESS;
}

int MainFuncs::evalPos(int argc, const char* const *argv)
{
  const char* usage =
      "posfile "
      "<-m extra moves to make> "
      "<-mainpla pla>"
      "<-idx idx to view in multigame file> ";
  const char* required = "";
  const char* allowed = "m idx mainpla";
  const char* empty = "";
  const char* nonempty = "m idx mainpla";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  pla_t mainPla = NPLA;
  if(Command::isSet(flags,"mainpla"))
    mainPla = Board::readPla(flags["mainpla"]);

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool success = handlePosFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  if(boards.size() == 1)
  {
    Board board = boards[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    bool suc = board.makeMovesLegalNoUndo(extraMoves);
    DEBUGASSERT(suc);

    performSingleEval(board,mainPla);
  }
  else
  {
    if(Command::isSet(flags,"m"))
      Global::fatalError("Cannot use -m for multiple positions");
    performMultiEval(boards,mainPla);
  }

  return EXIT_SUCCESS;
}

int MainFuncs::evalMoves(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile [platurn] "
      "<-m extra moves to make> "
      "<-mainpla pla>"
      "<-idx idx to view in multigame file> ";
  const char* required = "";
  const char* allowed = "m idx mainpla";
  const char* empty = "";
  const char* nonempty = "m idx mainpla";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  pla_t mainPla = NPLA;
  if(Command::isSet(flags,"mainpla"))
    mainPla = Board::readPla(flags["mainpla"]);

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool success = handleMovesFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  if(boards.size() == 1)
  {
    Board board = boards[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    bool suc = board.makeMovesLegalNoUndo(extraMoves);
    DEBUGASSERT(suc);

    performSingleEval(board, mainPla);
  }
  else
  {
    if(Command::isSet(flags,"m"))
      Global::fatalError("Cannot use -m for multiple positions");
    performMultiEval(boards,mainPla);
  }

  return EXIT_SUCCESS;
}

int MainFuncs::moveOrderPos(int argc, const char* const *argv)
{
  const char* usage =
      "posfile "
      "<-m extra moves to make> "
      "<-idx idx to view in multigame file> "
      "<-target move to view>"
      "<-weights file>"
      "<-showfeatures>";
  const char* required = "";
  const char* allowed = "m idx target weights showfeatures";
  const char* empty = "showfeatures";
  const char* nonempty = "m idx target weights";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool success = handlePosFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  bool showFeatures = Command::isSet(flags,"showfeatures");

  string* weightsFile = NULL;
  if(Command::isSet(flags,"weights"))
    weightsFile = new string(Command::getString(flags,"weights"));

  if(boards.size() == 1)
  {
    Board board = boards[0];
    BoardHistory hist = hists[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    applyExtraMoves(board,hist,extraMoves);

    move_t target = ERRMOVE;
    if(Command::isSet(flags,"target"))
      target = Board::readMove(Command::getString(flags,"target"));

    performSingleOrder(board,hist,weightsFile,target,showFeatures);
  }
  else
  {
    Global::fatalError("Not implemented for multiple positions");
  }

  delete weightsFile;

  return EXIT_SUCCESS;
}

int MainFuncs::moveOrderMoves(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile [platurn] "
      "<-m extra moves to make> "
      "<-idx idx to view in multigame file> "
      "<-target move to view>"
      "<-weights file>"
      "<-showfeatures>";
  const char* required = "";
  const char* allowed = "m idx target weights showfeatures";
  const char* empty = "showfeatures";
  const char* nonempty = "m idx target weights";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool success = handleMovesFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  bool showFeatures = Command::isSet(flags,"showfeatures");

  string* weightsFile = NULL;
  if(Command::isSet(flags,"weights"))
    weightsFile = new string(Command::getString(flags,"weights"));

  if(boards.size() == 1)
  {
    Board board = boards[0];
    BoardHistory hist = hists[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    applyExtraMoves(board,hist,extraMoves);

    move_t target = ERRMOVE;
    if(Command::isSet(flags,"target"))
      target = Board::readMove(Command::getString(flags,"target"));

    performSingleOrder(board,hist,weightsFile,target,showFeatures);
  }
  else
  {
    Global::fatalError("Not implemented for multiple positions");
  }

  return EXIT_SUCCESS;
}


int MainFuncs::viewPos(int argc, const char* const *argv)
{
  const char* usage =
      "posfile "
      "<-m extra moves to make> "
      "<-idx idx to view in multigame file> "
      "<-placement to view a placement string for the current pos>"
      "<-sandbox for viewing some random test or eval stat>";
  const char* required = "";
  const char* allowed = "m idx sandbox placement";
  const char* empty = "sandbox placement";
  const char* nonempty = "m idx";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  bool sandBox = Command::isSet(flags,"sandbox");

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool viewPlacement = Command::isSet(flags,"placement");
  bool success = handlePosFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  if(boards.size() == 1)
  {
    Board board = boards[0];
    BoardHistory hist = hists[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    applyExtraMoves(board,hist,extraMoves);

    performSingleDisplay(board,hist,viewPlacement,sandBox);
  }
  else
  {
    if(Command::isSet(flags,"m"))
      Global::fatalError("Cannot use -m for multiple positions");
    performMultiDisplay(boards,viewPlacement,sandBox);
  }

  return EXIT_SUCCESS;
}

int MainFuncs::viewMoves(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile [platurn] "
      "<-m extra moves to make> "
      "<-idx idx to view in multigame file> "
      "<-sandbox for viewing some random test or eval stat>";
  const char* required = "";
  const char* allowed = "m idx sandbox placement";
  const char* empty = "sandbox placement";
  const char* nonempty = "m idx";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  bool sandBox = Command::isSet(flags,"sandbox");

  vector<Board> boards;
  vector<BoardHistory> hists;
  int idx = Command::getInt(flags,"idx",-1);
  bool viewPlacement = Command::isSet(flags,"placement");
  bool success = handleMovesFromCommand(mainCommand,boards,hists,idx);
  if(!success)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  if(boards.size() == 1)
  {
    Board board = boards[0];
    BoardHistory hist = hists[0];

    vector<move_t> extraMoves;
    if(Command::isSet(flags,"m"))
      extraMoves = Board::readMoveSequence(flags["m"],board.step);
    applyExtraMoves(board,hist,extraMoves);

    performSingleDisplay(board,hist,viewPlacement,sandBox);
  }
  else
  {
    if(Command::isSet(flags,"m"))
      Global::fatalError("Cannot use -m for multiple positions");
    performMultiDisplay(boards,viewPlacement,sandBox);
  }

  return EXIT_SUCCESS;
}


int MainFuncs::loadPos(int argc, const char* const *argv)
{
  const char* usage = "posfile";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, "", "", "", "");
  Command::parseFlags(argc, argv, usage, "", "", "", "");

  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}
  vector<Board> boards = ArimaaIO::readBoardFile(mainCommand[1]);
  if(boards.size() <= 0)
    Global::fatalError("No boards in file!");

  return EXIT_SUCCESS;
}

int MainFuncs::loadMoves(int argc, const char* const *argv)
{
  const char* usage = "movesfile [platurn]";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, "", "", "", "");
  Command::parseFlags(argc, argv, usage, "", "", "", "");

  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}
  vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[1]);
  if(games.size() <= 0)
    Global::fatalError("No games in file!");

  return EXIT_SUCCESS;
}

//--------------------------------------------------------------------------------------------------------

static bool handlePosFromCommand(const vector<string>& mainCommand, vector<Board>& boards, vector<BoardHistory>& hists, int idx)
{
  if(mainCommand.size() != 2)
    return false;

  boards = ArimaaIO::readBoardFile(mainCommand[1]);
  hists.clear();
  if(idx >= 0)
  {
    if(idx >= (int)boards.size())
      Global::fatalError(Global::strprintf("idx %d >= num positions %d", idx, boards.size()));
    Board b = boards[idx];
    boards.clear();
    boards.push_back(b);
  }
  for(int i = 0; i<(int)boards.size(); i++)
    hists.push_back(BoardHistory(boards[i]));

  cout << mainCommand[1] << " " << Command::gitRevisionId() << endl;

  return true;
}

static bool handleMovesFromCommand(const vector<string>& mainCommand, vector<Board>& boards, vector<BoardHistory>& hists, int idx)
{
  if(mainCommand.size() != 2 && mainCommand.size() != 3)
    return false;

  if(mainCommand.size() == 2)
  {
    vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[1]);
    boards.clear();
    hists.clear();
    int start = 0;
    int stop = games.size();
    if(idx >= 0)
    {
      if(idx >= (int)games.size())
        Global::fatalError(Global::strprintf("idx %d >= num games %d", idx, games.size()));
      start = idx;
      stop = idx+1;
    }
    for(int i = start; i<stop; i++)
    {
      hists.push_back(BoardHistory(games[i]));
      DEBUGASSERT(hists[i-start].maxTurnNumber == hists[i-start].maxTurnBoardNumber);
      boards.push_back(hists[i-start].turnBoard[hists[i-start].maxTurnNumber]);
    }
    cout << mainCommand[1] << " " << Command::gitRevisionId() << endl;
    return true;
  }
  else
  {
    int turn;
    bool success = Board::tryReadPlaTurnOrInt(mainCommand[2],turn);
    if(success)
    {
      vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[1]);
      if(games.size() <= 0)
        Global::fatalError("No games in file!");

      boards.clear();
      hists.clear();
      int start = 0;
      int stop = games.size();
      if(idx >= 0)
      {
        if(idx >= (int)games.size())
          Global::fatalError(Global::strprintf("idx %d >= num games %d", idx, games.size()));
        start = idx;
        stop = idx+1;
      }
      for(int i = start; i<stop; i++)
      {
        GameRecord& game = games[i];
        hists.push_back(BoardHistory(game));
        DEBUGASSERT(hists[i-start].maxTurnNumber == hists[i-start].maxTurnBoardNumber);
        if(turn > hists[i-start].maxTurnNumber)
          Global::fatalError("Requested turn " + Board::writePlaTurn(turn) +
              " but record only goes until " + Board::writePlaTurn(hists[i-start].maxTurnNumber));
        boards.push_back(hists[i-start].turnBoard[min(turn,hists[i-start].maxTurnNumber)]);
      }
      cout << mainCommand[1] << " " << Command::gitRevisionId() << endl;
      return true;
    }

    Global::fatalError(string("Unknown platurn:") + mainCommand[2]);
    return false;
  }
}

static void performSingleSearch(const Board& b, const BoardHistory& hist, int depth, double time,
    const TimeControl& tc, const SearchParams& params)
{
  cout << b << endl;
  double min, normal, max;
  tc.getMinNormalMax(min,normal,max);
  if(min < SearchParams::AUTO_TIME)
    cout << "Base Time: " << min << "/" << normal << "/" << max << endl;

  Searcher searcher(params);
  searcher.setTimeControl(tc);
  searcher.searchID(b,hist,depth,time,true);
  SearchStats::printBasic(cout,searcher.stats);
  cout << searcher.stats << endl;
}

static void performMultiSearch(const vector<Board>& boards, const vector<BoardHistory>& hists, int depth, double time,
    const TimeControl& tc, const SearchParams& params)
{
  Searcher searcher(params);
  searcher.setTimeControl(tc);
  for(int i = 0; i<(int)boards.size(); i++)
  {
    searcher.searchID(boards[i],hists[i],depth,time,false);
    SearchStats::printBasic(cout,searcher.stats,i);
  }
}

static void performSingleEval(const Board& b, pla_t mainPla)
{
  Board copy = b;
  Eval::evaluate(copy,mainPla,0,&cout);
}

static void performMultiEval(const vector<Board>& boards, pla_t mainPla)
{
  for(int i = 0; i<(int)boards.size(); i++)
  {
    cout << i << "------------------------------------------------" << endl;
    performSingleEval(boards[i],mainPla);
    cout << endl;
  }
}

static void performSingleOrder(const Board& b, const BoardHistory& hist, const string* weightsFile, move_t target, bool showFeatures)
{
  ArimaaFeatureSet afset = MoveFeature::getArimaaFeatureSet();
  BradleyTerry learner = weightsFile == NULL ?
      BradleyTerry::inputFromDefault(afset) :
      BradleyTerry::inputFromFile(afset,weightsFile->c_str());

  vector<move_t> mvVec;
  ExistsHashTable* fullmoveHash = new ExistsHashTable(21);
  SearchMoveGen::genFullMoves(b, hist, mvVec, 4, true, 4, fullmoveHash, NULL, NULL);
  delete fullmoveHash;

  Board copy = b;
  UndoMoveData uData;
  pla_t pla = copy.player;

  vector<double> matchParallelFactors;
  void* data = afset.getPosData(copy,hist,pla);
  afset.getParallelWeights(data,matchParallelFactors);

  hash_t targetHash = 0;
  double winningEval = 0;
  vector<findex_t> winningFeatures;
  if(target != ERRMOVE)
  {
    bool suc = copy.makeMoveLegalNoUndo(target,uData);
    DEBUGASSERT(suc);
    targetHash = copy.sitCurrentHash;
    winningFeatures = afset.getFeatures(copy,data,pla,target,hist);
    winningEval = learner.evaluate(winningFeatures,matchParallelFactors);
    copy.undoMove(uData);
  }

  int numMoves = mvVec.size();
  vector<RatedMove> ratedMoves;
  int numBetter = 0;
  for(int i = 0; i<numMoves; i++)
  {
    bool suc = copy.makeMoveLegalNoUndo(mvVec[i],uData);
    DEBUGASSERT(suc);

    vector<findex_t> features = afset.getFeatures(copy,data,pla,mvVec[i],hist);
    double eval = learner.evaluate(features,matchParallelFactors);
    if(eval > winningEval && copy.sitCurrentHash != targetHash)
      numBetter++;

    RatedMove rmove;
    rmove.move = mvVec[i];
    rmove.val = eval;
    ratedMoves.push_back(rmove);

    copy.undoMove(uData);
  }

  afset.freePosData(data);

  if(target != ERRMOVE)
  {
    cout << b << endl;
    cout << "Ordering: " << numBetter << " / " << numMoves << endl;
    cout << "Ordering pct: " << ((double)numBetter / numMoves * 100) << "%" << endl;

    if(showFeatures)
    {
      int numFeatures = winningFeatures.size();
      for(int i = 0; i<numFeatures; i++)
        cout << afset.fset->getName(winningFeatures[i]) << endl;
    }
  }
  else
  {
    cout << b << endl;
    std::sort(ratedMoves.begin(),ratedMoves.end());
    for(int i = 0; i<numMoves; i++)
      cout << Board::writeMove(b,ratedMoves[i].move) << " " << ratedMoves[i].val << " " << i << endl;
  }
}

static void performSingleDisplay(const Board& b, const BoardHistory& hist, bool viewPlacement, bool sandBox)
{
  (void)hist;
  cout << b << endl;
  if(viewPlacement)
  {
    cout << "1g " << Board::writePlacements(b,GOLD) << endl;
    cout << "1s " << Board::writePlacements(b,SILV) << endl;
  }
  if(sandBox)
  {
    /*
    const char* chars = "0123456789ABCDEF";
    Bitmap goalThreatDistIs[9];
    Threats::getGoalDistBitmapEst(b,b.player,goalThreatDistIs);

    int goalThreatDist[BSIZE];
    for(int i = 0; i<BSIZE; i++)
      goalThreatDist[i] = 15;
    for(int i = 8; i>=0; i--)
    {
      Bitmap map = goalThreatDistIs[i];
      while(map.hasBits())
        goalThreatDist[map.nextBit()] = i;
    }
    for(int y = 7; y >= 0; y--)
    {
      for(int x = 0; x < 8; x++)
        cout << chars[goalThreatDist[gLoc(x,y)]] << " ";
      cout << endl;
    }
    cout << endl;
    */

    /*
    move_t mv[8192];
    Board copy = b;
    int num = SearchMoveGen::genRelevantTactics(copy,true,mv,NULL);
    cout << num << " relevant tactics" << endl;
    for(int i = 0; i<num; i++)
    {
      cout << i << " " << Board::writeMove(copy,mv[i]) << endl;
    }
    */
    /*
    ExistsHashTable* fullmoveHash = new ExistsHashTable(SearchParams::DEFAULT_FULLMOVE_HASH_EXP);
    vector<move_t> moveBuf;
    SearchMoveGen::genFullMoves(b,hist,moveBuf,4,true,4,fullmoveHash,NULL);
    Rand rand;
    for(int i = 0; i<(int)moveBuf.size(); i++)
    {
      bucketsM[i*100/(moveBuf.size())]++;
      if(SearchPrune::canHaizhiPruneWholeMove(b,b.player,moveBuf[i]))
        buckets[i*100/(moveBuf.size())]++;
    }
    //for(int i = 0; i<100; i++)
    //  cout << i << " " << buckets[i] << " " << bucketsM[i] << endl;
    cout << moveBuf.size() << endl;
    delete fullmoveHash;
    */

    /*
    move_t mv[8192];
    Board copy = b;
    Bitmap tryRunawayMap = b.dominatedMap & b.pieceMaps[b.player][0] & Board::PGOALMASKS[5][b.player] & ~Bitmap::BMPTRAPS;
    int maxTryRunawaySteps = min(4-b.step,3);
    int num = 0;
    pla_t pla = b.player;
    pla_t opp = gOpp(pla);
    while(tryRunawayMap.hasBits())
    {
      loc_t ploc = tryRunawayMap.nextBit();
      if(b.pieces[ploc] == RAB && ((gX(ploc) == 0 && b.owners[ploc E] == pla) || (gX(ploc) == 7 && b.owners[ploc W] == pla)))
          continue;
      num += BoardTrees::genRunawayDefs(copy,b.player,ploc,maxTryRunawaySteps,true,mv+num);
    }
    cout << "generated " << num << " runaways" << endl;
    for(int i = 0; i<num; i++)
    {
      cout << i << " " << Board::writeMove(copy,mv[i]) << endl;
    }
    */

    /*
    move_t mv[8192];
    Board copy = b;
    int num = BoardTrees::genUsefulElephantMoves(copy,copy.player,4-b.step,mv);
    for(int i = 0; i<num; i++)
    {
      cout << i << " " << Board::writeMove(copy,mv[i]) << endl;
    }
    */

    /*
    move_t mv[8192];
    Board copy = b;
    int num = BoardTrees::genCamelHostageRunaways(copy,copy.player,4-b.step,mv);
    for(int i = 0; i<num; i++)
    {
      cout << "runaway" << i << " " << Board::writeMove(copy,mv[i]) << endl;
    }
    */

    /*
    move_t mv[8192];
    Board copy = b;
    Bitmap pStrongerMaps[2][NUMTYPES];
    b.initializeStrongerMaps(pStrongerMaps);
    int num = BoardTrees::genDistantAdvances(copy,copy.player,4-b.step,pStrongerMaps,mv);
    for(int i = 0; i<num; i++)
    {
      cout << "distadv " << i << " " << Board::writeMove(copy,mv[i]) << endl;
    }
    */

    /*
    Bitmap singleDefenderTraps;
    num = 0;
    for(int i = 0; i < 2; i++)
    {
      int trapIndex = Board::PLATRAPINDICES[pla][i];
      loc_t kt = Board::TRAPLOCS[trapIndex];
      if(b.trapGuardCounts[pla][trapIndex] <= 1)
        singleDefenderTraps |= Board::RADIUS[1][kt];
      else
        num += BoardTrees::genContestedDefenses(copy,pla,kt,4-b.step,mv+num);
    }
    for(int i = 0; i < 2; i++)
    {
      int trapIndex = Board::PLATRAPINDICES[opp][i];
      loc_t kt = Board::TRAPLOCS[trapIndex];
      if(b.trapGuardCounts[pla][trapIndex] == 1)
        singleDefenderTraps |= Board::RADIUS[1][kt];
      else
        num += BoardTrees::genContestedDefenses(copy,pla,kt,4-b.step,mv+num);
    }
    num += BoardMoveGen::genStepsInto(copy,pla,mv+num,singleDefenderTraps);
    cout << "generated " << num << " defs" << endl;
    for(int i = 0; i<num; i++)
    {
      cout << i << " " << Board::writeMove(copy,mv[i]) << endl;
    }
    */

    /*
    loc_t plaEleLoc = b.findElephant(b.player);
    if(plaEleLoc != ERRLOC)
    {
      vector<move_t> mvVec;
      ExistsHashTable* fullmoveHash = new ExistsHashTable(21);
      int rootMaxNonRelTactics = 4;
      SearchMoveGen::genFullMoves(b, hist, mvVec, 4-b.step, true, rootMaxNonRelTactics, fullmoveHash, NULL);
      Bitmap eleReached;
      eleReached.setOn(plaEleLoc);
      for(int i = 0; i<(int)mvVec.size(); i++)
      {
        int num = 0;
        loc_t src[8];
        loc_t dest[8];
        num = b.getChanges(mvVec[i],src,dest);
        for(int i = 0; i<num; i++)
        {
          if(src[i] == plaEleLoc && dest[i] != ERRLOC)
            eleReached.setOn(dest[i]);
        }
      }
      cout << "Ele reached via reltactics" << endl;
      cout << eleReached << endl;
      delete fullmoveHash;
    }
    */
  }
}

static void performMultiDisplay(const vector<Board>& boards, bool viewPlacement, bool sandBox)
{
  for(int i = 0; i<(int)boards.size(); i++)
  {
    cout << i << "------------------------------------------------" << endl;
    BoardHistory hist(boards[i]);
    performSingleDisplay(boards[i], hist, viewPlacement, sandBox);
    cout << endl;
  }
}

static void applyExtraMoves(Board& board, BoardHistory& hist, const vector<move_t>& extraMoves)
{
  for(int i = 0; i<(int)extraMoves.size(); i++)
  {
    step_t oldStep = board.step;
    Board copy = board;
    bool suc = board.makeMoveLegalNoUndo(extraMoves[i]);
    if(!suc) Global::fatalError("Illegal extra move " + Board::writeMove(copy,extraMoves[i]));
    DEBUGASSERT(suc);
    hist.reportMove(board,extraMoves[i],oldStep);
  }
}






