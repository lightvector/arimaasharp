
/*
 * maingetmove.cpp
 * Author: davidwu
 */

#include <fstream>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../setups/setup.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../program/init.h"
#include "../main/main.h"

using namespace std;

static const char* USAGE_STRING = (
"\n\
Usage: sharp2015.exe (aei|analyze)\n\
bot_sharp supports and prefers the Arimaa Engine Interface (AEI).\n\
Call \"sharp2015.exe aei\" as above to run with AEI.\n\
See http://arimaa.janzert.com/aei/aei-protocol.html for a description\n\
of the AEI protocol\n\
\n\
Call \"sharp2015.exe analyze\" to directly analyze a position\n\
-------------------------------------------------------------------\n\
For legacy reasons, the old getmove-style interface is still available:\n\
sharp2015.exe <posFile> <moveFile> <stateFile> <-flags>  (use files)\n\
sharp2015.exe <-flags>  (no files, specify board and time control in flags)\n\
For the first usage, posFile and moveFile are strictly optional.\n\
\n\
Flags are:\n\
-b B: override board position to B, where B is a position string (see below)\n\
-p P: override current player to P, where P is 1 if Gold, 0 if Silver\n\
-d D: limit maximum nominal search depth to D steps\n\
-t T: set maximum search time to T seconds, or 0 for unbounded time.\n\
-movefile <moveFile>: use the provide movefile to infer the board.\n\
-threads N: search using N parallel threads \n\
-seed S: use fixed seed S, rather than randomizing \n\
-hashmem H: use at most H memory for the main hashtable (ex: 512MB). \n\
            (memory usage may be a little more than this due to things \n\
            other than the main hashtable) \n\
\n\
Note that when using the -d flag, the bot will also respect any time control\n\
as well. To force a search of that depth, use -t 0 to make the time unbounded.\n\
\n\
Valid position string consists of:\n\
{RCDHME} - gold (1st player) pieces\n\
{rcdhme} - silver (2nd player) pieces\n\
{.,*xX}  - empty spaces\n\
Board should be listed from the northwest corner to the southeast corner in\n\
scanline order. Other non-whitespace characters will be ignored, and can\n\
optionally be used as line delimiters. For instance:\n\
rrrddrrr/r.c.mc.r/.h*..*h./...eE.../......../..*..*H./RMCH.C.R/RRRDDRRR\n\
");

static bool tryGetMove(int argc, const char* const *argv);

int MainFuncs::getMove(int argc, const char* const *argv)
{
  if(argc <= 1)
  {
    cout << USAGE_STRING << endl;
    cout << "Author: " << MainVersion::BOT_AUTHOR << endl;
    cout << "Version: " << MainVersion::BOT_VERSION << endl;
    cout << "(revision id " << Command::gitRevisionId() << ")" << endl;
    return EXIT_SUCCESS;
  }
  else if(!tryGetMove(argc,argv))
  {
    cout << "Run this program with no arguments for usage" << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

static bool tryGetMove(int argc, const char* const *argv)
{
  string requiredFlags;
  string allowedFlags;
  string emptyFlags;
  string nonemptyFlags;
  if(Init::ARIMAA_DEV)
  {
    requiredFlags = string("");
    allowedFlags = string("b p t d s turnearly turnmid threads seed hashmem movefile no-output-files");
    emptyFlags = string("turnearly turnmid no-output-files");
    nonemptyFlags = string("b p t d s threads seed hashmem movefile");
  }
  else
  {
    requiredFlags = string("");
    allowedFlags = string("b p t d threads seed hashmem movefile no-output-files");
    emptyFlags = string("no-output-files");
    nonemptyFlags = string("b p t d threads seed hashmem movefile");
  }

  vector<string> args = Command::parseCommand(argc, argv, USAGE_STRING, requiredFlags, allowedFlags, emptyFlags, nonemptyFlags);
  map<string,string> flags = Command::parseFlags(argc, argv, USAGE_STRING, requiredFlags, allowedFlags, emptyFlags, nonemptyFlags);

  string sfilename;
  bool bfilename = false;
  if(args.size() > 1)
  {
    sfilename = args[args.size()-1];
    bfilename = true;
  }

  string emptyDefault;
  string sboard = map_get(flags,"b",emptyDefault);
  bool bboard = map_contains(flags,"b");
  string splayer = map_get(flags,"p",emptyDefault);
  bool bplayer = map_contains(flags,"p");
  string stime = map_get(flags,"t",emptyDefault);
  bool btime = map_contains(flags,"t");
  string sdepth = map_get(flags,"d",emptyDefault);
  bool bdepth = map_contains(flags,"d");
  string ssetup = map_get(flags,"s",emptyDefault);
  bool bsetup = map_contains(flags,"s");

  bool bearly = map_contains(flags,"turnearly");
  bool bmid = map_contains(flags,"turnmid");
  string shashmem = map_get(flags,"hashmem",emptyDefault);
  bool bhashmem = map_contains(flags,"hashmem");
  string sthreads = map_get(flags,"threads",emptyDefault);
  bool bthreads = map_contains(flags,"threads");
  string smovefile = map_get(flags,"movefile",emptyDefault);
  bool bmovefile = map_contains(flags,"movefile");
  bool bnooutputfiles = map_contains(flags,"no-output-files");


  Rand rand;
  uint64_t seed = rand.getSeed();
  if(map_contains(flags,"seed"))
  {
    string seedString = flags["seed"];
    seed = Global::stringToUInt64(seedString);
    rand.init(seed);
  }

  if(bmovefile && bfilename)
  {cout << "Cannot specify both gamestate files and -movefile" << endl; return false;}
  if(bmovefile && bboard)
  {cout << "Cannot specify both -b and -movefile" << endl; return false;}

  //Process game state file?
  map<string,string> gameState;
  if(bfilename)
  {
    ifstream stateStream;
    stateStream.open(sfilename.c_str());
    if(!stateStream.good())
    {cout << "File " << sfilename << " not found" << endl; return false;}
    gameState = ArimaaIO::readGameState(stateStream);
  }

  //Parse board...
  if(!bboard && !bfilename && !bmovefile)
  {cout << "No board state specified (no gamestate and no -b flag and no -movefile flag)" << endl; return false;}

  Board board;
  BoardHistory hist;
  //...from gamestate file
  if(bfilename)
  {
    GameRecord record = GameRecord::read(gameState["moves"]);
    hist = BoardHistory(record);
    DEBUGASSERT(hist.maxTurnNumber == hist.maxTurnBoardNumber);
    board = hist.turnBoard[hist.maxTurnNumber];
  }
  //... from moves file
  else if(bmovefile)
  {
    vector<GameRecord> records = ArimaaIO::readMovesFile(smovefile);
    if(records.size() <= 0)
    {cout << "Could not find any moves in file" << endl; return false;}
    if(records.size() > 1)
    {cout << "More than one move list in file?" << endl; return false;}
    hist = BoardHistory(records[0]);
    DEBUGASSERT(hist.maxTurnNumber == hist.maxTurnBoardNumber);
    board = hist.turnBoard[hist.maxTurnNumber];
  }
  //...from command line
  if(bboard)
  {
    board = Board::read(sboard);
    hist.reset(board);
  }

  //Parse player
  if(bplayer)
  {
    if(splayer[0] == '0')
      board.setPlaStep(0,board.step);
    else if(splayer[0] == '1')
      board.setPlaStep(1,board.step);
    else
    {cout << "-p: Invalid player" << endl; return false;}
    hist.reset(board);
  }

  //Ensure player determined
  if(!bplayer && bboard)
  {cout << "No player to move specified" << endl; return false;}

  //Parse max depth
  int maxDepth = SearchParams::AUTO_DEPTH;
  if(bdepth)
  {
    istringstream in(sdepth);
    in >> maxDepth;
    if(in.fail())
    {cout << "-d: Invalid depth" << endl; return false;}
    if(maxDepth > SearchParams::AUTO_DEPTH)
    {cout << "-d: Depth greater than " << SearchParams::AUTO_DEPTH << endl; return false;}
    if(maxDepth < 4)
    {cout << "-d: Depth less than 4" << endl; return false;}
  }

  //Parse time control
  bool maxTimeOnly = true;
  double maxTime = SearchParams::AUTO_TIME;
  TimeControl tc;
  if(btime)
  {
    istringstream in(stime);
    in >> maxTime;
    if(in.fail())
    {cout << "-t: Invalid time" << endl; return false;}
    if(maxTime > SearchParams::AUTO_TIME)
    {cout << "-t: Time greater than " << SearchParams::AUTO_TIME << endl; return false;}

    if(maxTime <= 0)
      maxTime = -1; //Infinite time
  }
  else if(bfilename)
  {
    tc = ArimaaIO::getTCFromGameState(gameState);
    maxTimeOnly = false;
  }
  else
  {cout << "No time control specified" << endl; return false;}

  //Setup!
  if(board.pieceCounts[0][0] == 0 || board.pieceCounts[1][0] == 0)
  {
    int setupMode = 0;
    if(bsetup)
    {
      istringstream in(ssetup);
      in >> setupMode;
    }
    if(setupMode == 1)
      Setup::setupRatedRandom(board,seed);
    else if(setupMode == 2)
      Setup::setupPartialRandom(board,seed);
    else if(setupMode == 3)
      Setup::setupRandom(board,seed);
    else
      Setup::setupNormal(board,seed);

    cout << Board::writePlacements(board,gOpp(board.player)) << endl;
    return true;
  }

  //Build searcher---------------------------------------------------
  SearchParams params;
  if(!bboard || bearly)
  {
    params.setAvoidEarlyTrade(true);
    params.setAvoidEarlyBlockade(true);
  }
  else if(bmid)
  {
    params.setAvoidEarlyBlockade(true);
  }

  params.setRandomize(true,SearchParams::DEFAULT_RANDOM_DELTA,rand.nextUInt64());
  params.setMoveImmediatelyIfOnlyOneNonlosing(true);

  if(bthreads)
  {
    params.numThreads = Global::stringToInt(sthreads);
    if(params.numThreads < 1 || params.numThreads > 256)
      Global::fatalError("-threads: T < 1 or T > 256");
  }

  //Hash memory
  if(bhashmem)
  {
    uint64_t hashMem = ArimaaIO::readMem(shashmem);
    int exp = SearchHashTable::getExp(hashMem);
    params.mainHashExp = exp;
    if(params.fullMoveHashExp > exp-1)
      params.fullMoveHashExp = max(exp-1,0);
  }

  //Bradley Terry Move ordering?
  BradleyTerry learner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(learner);
  params.setRootFancyPrune(true);
  params.setUnsafePruneEnable(true);
  params.setNullMoveEnable(true);
  Searcher searcher(params);

  //Search!
  if(!maxTimeOnly)
    searcher.setTimeControl(tc);

  move_t bestMove = ERRMOVE;
  //See if we can just make the same move we did earlier
  if(SearchParams::IMMEDIATELY_REMAKE_REVERSED_MOVE &&
     BoardHistory::occurredEarlierAndMoveIsLegalNow(board,hist,bestMove))
  {
    //bestMove is already filled now by sameSituationEarlierInHistory
    //Clear search stats, since we output them below.
    searcher.stats = SearchStats();
  }
  else
  {
    //Search for a best move the hard way
    searcher.searchID(board,hist,maxDepth,maxTime,false);
    bestMove = searcher.getMove();
  }

  //Report best move to console
  cout << Board::writeMove(board,bestMove,false) << endl;

  //Output!
  ostringstream sout;
  sout << "Seed " << seed << " Threads " << params.numThreads << " Hashexp " << params.mainHashExp << endl;
  sout << searcher.stats << endl;
  sout << board << endl;
  string verboseData = sout.str();

  //To log file
  if(!bnooutputfiles)
  {
    string filename;
    if(bfilename)
    {
      filename += "g";
      filename += gameState["tableId"];
    }
    else
    {
      filename += "t";
      char str[100];
      sprintf(str,"%d",(int)time(NULL));
      filename += str;
    }

    {
      ofstream out;
      string fname = filename + ".log";
      out.open(fname.c_str(), ios::app);

      out << endl;
      out << gameState["plycount"];
      out << gameState["s"];
      out << endl;
      out << Board::writeMove(board,bestMove,false) << endl;
      out << verboseData << endl;

      out.close();
    }

    //Warn if already used is > 10
    if(!maxTimeOnly && tc.alreadyUsed > 10)
    {
      ofstream out;
      string fname = filename + ".time";
      out.open(fname.c_str(), ios::app);

      out << endl;
      out << "WARNING ";
      out << gameState["plycount"];
      out << gameState["s"];
      out << ": large wused or bused: " << tc.alreadyUsed;
      out << endl;
      out.close();
    }

  }

  return true;

}



