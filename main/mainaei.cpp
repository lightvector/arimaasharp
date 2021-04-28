/*
 * mainaei.cpp
 * Author: davidwu
 */

#include <fstream>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include "../core/global.h"
#include "../core/boostthread.h"
#include "../core/threadsafequeue.h"
#include "../core/prefixstream.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../eval/eval.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../setups/setup.h"
#include "../program/command.h"
#include "../program/init.h"
#include "../program/asyncbot.h"
#include "../main/main.h"

static void setDefaultParams(SearchParams& params)
{
  BradleyTerry learner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(learner);
  params.setRootFancyPrune(true);
  params.setMoveImmediatelyIfOnlyOneNonlosing(true);
  params.setUnsafePruneEnable(true);
  params.setNullMoveEnable(true);

  //Avoidance of early trades is done inline based on
  //whether we're playing a game or whether a position is set directly

  //Randomization is done in asyncBot
}

static TimeControl getDefaultTC()
{
  TimeControl tc;
  tc.perMove = 15;
  tc.percent = 100;
  tc.reserveStart = 60;
  tc.reserveMax = 120;
  tc.gameMax = SearchParams::AUTO_TIME;
  tc.perMoveMax = SearchParams::AUTO_TIME;

  tc.alreadyUsed = 0;
  tc.reserveCurrent = 60;
  tc.gameCurrent = 0;

  return tc;
}

namespace {
struct Event
{
  int type;

  static const int INPUT_AEI = 0;
  static const int INPUT_ISREADY = 1;
  static const int INPUT_NEWGAME = 2;
  static const int INPUT_SETPOSITION = 3;
  static const int INPUT_SETOPTION = 4;
  static const int INPUT_MAKESETUP = 5;
  static const int INPUT_MAKEMOVE = 6;
  static const int INPUT_GO = 7;
  static const int INPUT_STOP = 8;
  static const int INPUT_QUIT = 9;
  static const int INPUT_ERROR = 10;

  //Only in dev
  static const int INPUT_EVAL = 11;

  static const int BESTMOVE = 12;
  union {
    pla_t inputSetPositionSide;
    string* inputSetOptionKey;
    move_t inputMakemoveMove;
    string* inputMakesetupSetup;
    bool inputGoPonder;
    pla_t inputEvalPlayer;
    move_t bestMoveMove;
  };
  union {
    string* inputSetPositionBoard;
    string* inputSetOptionValue;
    int inputEvalStep;
    int bestMoveSearchId;
  };
  string* bestMoveLogMessage;

  void free()
  {
    if(type == INPUT_SETPOSITION)
    {
      delete inputSetPositionBoard;
    }
    else if(type == INPUT_SETOPTION)
    {
      delete inputSetOptionKey;
      delete inputSetOptionValue;
    }
    else if(type == INPUT_MAKESETUP)
    {
      delete inputMakesetupSetup;
    }
    else if(type == BESTMOVE)
    {
      delete bestMoveLogMessage;
    }
  }

  static Event inputErrorEvent()
  {
    Event event;
    event.type = INPUT_ERROR;
    return event;
  }

  static Event quitEvent()
  {
    Event event;
    event.type = INPUT_QUIT;
    return event;
  }
};
}

static void reply(const char* s) {cout << s << endl;}
static void reply(const string& s) {cout << s << endl;}
static void logError(const char* s) {cout << "log Error: " << s << endl;}
static void logError(const string& s) {cout << "log Error: " << s << endl;}
//static void logWarning(const char* s) {cout << "Warning: " << s << endl;}
static void logWarning(const string& s) {cout << "log Warning: " << s << endl;}
static void logMessage(const char* s) {cout << "log " << s << endl;}
static void logMessage(const string& s) {cout << "log " << s << endl;}

static ThreadSafeQueue<Event>* eventQueue = NULL;

//Returns the empty string when cin gets EOF and there is no more input.
static string getInputBlocking()
{
  string data;
  while(true)
  {
    getline(cin,data);
    data = Global::trim(data);
    if(data.length() > 0)
      break;
    if(cin.eof())
    {
      data = string();
      break;
    }
  }
  return data;
}

static Event parseInput(const string& s)
{
  size_t spacePos = s.find_first_of(' ');
  if(spacePos == string::npos)
    spacePos = s.size();

  string typeStr = s.substr(0,spacePos);
  string rest = spacePos+1 >= s.size() ? string() : s.substr(spacePos+1);
  typeStr = Global::trim(typeStr);
  rest = Global::trim(rest);

  Event event;
  if(typeStr == "aei")
  {
    if(rest.size() != 0) logWarning(typeStr + " command was followed by nonempty string: " + rest);
    event.type = Event::INPUT_AEI;
  }
  else if(typeStr == "isready")
  {
    if(rest.size() != 0) logWarning(typeStr + " command was followed by nonempty string: " + rest);
    event.type = Event::INPUT_ISREADY;
  }
  else if(typeStr == "newgame")
  {
    if(rest.size() != 0) logWarning(typeStr + " command was followed by nonempty string: " + rest);
    event.type = Event::INPUT_NEWGAME;
  }
  else if(typeStr == "setposition")
  {
    event.type = Event::INPUT_SETPOSITION;

    size_t spacePos2 = rest.find_first_of(' ');
    if(spacePos2 == string::npos || spacePos2 + 1 >= rest.size())
    {logError("Could not parse arguments to " + typeStr + " command: " + rest); return Event::inputErrorEvent();}
    string sideStr = rest.substr(0,spacePos2);
    string boardStr = rest.substr(spacePos2+1);
    sideStr = Global::trim(sideStr);
    boardStr = Global::trim(boardStr);
    if(sideStr == "g")
      event.inputSetPositionSide = GOLD;
    else if(sideStr == "s")
      event.inputSetPositionSide = SILV;
    else
    {logError("Could not parse arguments to " + typeStr + " command: " + rest); return Event::inputErrorEvent();}

    if(boardStr.size() != 66) //Expect "[<64 chars of board>]"
    {logError("Could not parse arguments to " + typeStr + " command: " + rest); return Event::inputErrorEvent();}

    //Test out the parsing immediately
    boardStr = boardStr.substr(1,64);
    if(!Global::stringCharsAllAllowed(boardStr,"rcdhmeRCDHME "))
    {logError("Could not parse arguments to " + typeStr + " command: " + rest); return Event::inputErrorEvent();}
    //Convert to a format that board parsing later will understand - can't use spaces
    for(int i = 0; i<64; i++)
      if(boardStr[i] == ' ')
        boardStr[i] = '.';
    event.inputSetPositionBoard = new string(boardStr);
  }
  else if(typeStr == "setoption")
  {
    event.type = Event::INPUT_SETOPTION;
    if(rest.size() > 262144)
    {logError("Too much input encountered when parsing setoption"); return Event::inputErrorEvent();}

    vector<string> pieces = Global::split(rest);
    int size = pieces.size();
    if(size < 4 || pieces[0] != "name" || pieces[2] != "value")
    {logError("Could not parse arguments to " + typeStr + " command: " + rest); return Event::inputErrorEvent();}
    string keyStr = pieces[1];
    string valueStr = Global::concat(pieces," ",3,pieces.size());

    event.inputSetOptionKey = new string(keyStr);
    event.inputSetOptionValue = new string(valueStr);
  }
  else if(typeStr == "makemove")
  {
    move_t move;
    if(Board::tryReadMove(rest,move))
    {
      event.type = Event::INPUT_MAKEMOVE;
      event.inputMakemoveMove = move;
    }
    else
    {
      vector<Board::Placement> placements;
      if(Board::tryReadPlacements(rest,placements))
      {
        event.type = Event::INPUT_MAKESETUP;
        event.inputMakesetupSetup = new string(rest);
      }
      else
      {logError("Could not parse arguments to " + typeStr + " command: " + rest); return Event::inputErrorEvent();}
    }
  }
  else if(typeStr == "go")
  {
    event.type = Event::INPUT_GO;
    if(rest == "")
      event.inputGoPonder = false;
    else if(rest == "ponder")
      event.inputGoPonder = true;
    else
    {logError("Could not parse arguments to " + typeStr + " command: " + rest); return Event::inputErrorEvent();}
  }
  else if(typeStr == "stop")
  {
    if(rest.size() != 0) logWarning(typeStr + " command was followed by nonempty string: " + rest);
    event.type = Event::INPUT_STOP;
  }
  else if(typeStr == "quit")
  {
    if(rest.size() != 0) logWarning(typeStr + " command was followed by nonempty string: " + rest);
    event.type = Event::INPUT_QUIT;
  }
  else if(Init::ARIMAA_DEV && typeStr == "eval")
  {
    event.type = Event::INPUT_EVAL;
    event.inputEvalPlayer = NPLA;
    event.inputEvalStep = -1;
    vector<string> pieces = Global::split(rest);
    for(int i = 0; i<(int)pieces.size(); i++)
    {
      pla_t pla;
      int step;
      if(Global::tryStringToInt(pieces[i],step))
        event.inputEvalStep = step;
      else if(Board::tryReadPla(pieces[i],pla))
        event.inputEvalPlayer = pla;
    }
  }
  else
  {
    logError("Unknown command: " + s);
    return Event::inputErrorEvent();
  }

  return event;
}

static void inputThreadMain()
{
  DEBUGASSERT(eventQueue != NULL);
  while(true)
  {
    string s = getInputBlocking();
    Event event = s.length() == 0 ? Event::quitEvent() : parseInput(s);
    eventQueue->push(event);
    if(event.type == Event::INPUT_ERROR || event.type == Event::INPUT_QUIT)
      break;
    if(eventQueue->size() > 65536)
    {
      logError("Too much input encountered");
      eventQueue->push(Event::inputErrorEvent());
      break;
    }
  }
}

static void handleSearchDone(move_t move, int searchId, const SearchStats& stats)
{
  Event event;
  event.type = Event::BESTMOVE;
  event.bestMoveMove = move;
  event.bestMoveSearchId = searchId;

  ostringstream bestMoveLogMessage;
  bestMoveLogMessage
  << "Depth " << stats.depthReached << ((int)stats.depthReached == stats.depthReached ? "" : "+")
  << " Eval " << stats.finalEval
  << " Time " << stats.timeTaken
  << " Seed " << Global::uint64ToHexString(stats.randSeed);
  event.bestMoveLogMessage = new string(bestMoveLogMessage.str());
  eventQueue->push(event);
}

int MainFuncs::aei(int argc, const char* const *argv)
{
  (void)argc;
  (void)argv;

  //Initialize event queue
  DEBUGASSERT(eventQueue == NULL);
  eventQueue = new ThreadSafeQueue<Event>();

  //Spin off the input thread
  std::thread inputThread;
  inputThread = std::thread(&inputThreadMain);

  //Initialize state
  Board b;
  BoardHistory hist;
  AsyncBot* asyncBot = NULL;
  TimeControl goldTc = getDefaultTC();
  TimeControl silvTc = getDefaultTC();
  PrefixStream prefixStream(string("log "),cout);
  SearchParams params;
  setDefaultParams(params);
  params.setOutput(&prefixStream);
  Rand rand;
  bool randomize = true;
  bool ignoreTC = false;
  bool verbose = false;
  bool evalMode = false;
  bool explicitPasses = false;
  int searchId = 0;
  bool aeiReceived = false;
  bool exitedOnError = false;
  while(true)
  {
    Event event = eventQueue->waitPop();

    bool exiting = false;
    switch(event.type)
    {
    case Event::INPUT_AEI:
    {
      if(aeiReceived)
      {
        logWarning("Unexpected command: aei");
        break;
      }
      aeiReceived = true;
      string gitRevisionStr = Command::gitRevisionId();
      reply("protocol-version 1");
      reply("id name " + string(MainVersion::BOT_NAME));
      reply("id author " + string(MainVersion::BOT_AUTHOR));
      reply("id version " + string(MainVersion::BOT_VERSION) + " (" + gitRevisionStr + ")");
      reply("aeiok");
      break;
    }
    case Event::INPUT_ISREADY:
    {
      reply("readyok");
      break;
    }
    case Event::INPUT_NEWGAME:
    {
      if(asyncBot != NULL)
      {
        searchId++; //Invalidate any earlier search's move
        asyncBot->stop();
        delete asyncBot;
        asyncBot = NULL;
      }
      b = Board();
      hist = BoardHistory();
      goldTc.restart();
      silvTc.restart();
      if(!evalMode)
      {
        params.setAvoidEarlyTrade(true);
        params.setAvoidEarlyBlockade(true);
      }
      asyncBot = new AsyncBot(params);
      logMessage("Started new game");
      break;
    }
    case Event::INPUT_SETPOSITION:
    {
      if(asyncBot == NULL)
      {
        logError("setposition called before newgame");
        break;
      }
      b = Board::read(*(event.inputSetPositionBoard));
      hist.reset(b);
      params.setAvoidEarlyTrade(false);
      params.setAvoidEarlyBlockade(false);
      searchId++; //Invalidate any earlier search's move
      asyncBot->stop();
      logMessage("Position set");
      break;
    }
    case Event::INPUT_SETOPTION:
    {
      int i;
      double d;
      bool bit;
      if(*(event.inputSetOptionKey) == "tcmove" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {goldTc.perMove = d; silvTc.perMove = d;}
      else if(*(event.inputSetOptionKey) == "tcreserve" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {goldTc.reserveStart = d; silvTc.reserveStart = d;}
      else if(*(event.inputSetOptionKey) == "tcpercent" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {goldTc.percent = d; silvTc.percent = d;}
      else if(*(event.inputSetOptionKey) == "tcmax" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {if(d <= 0) d = SearchParams::AUTO_TIME; goldTc.reserveMax = d; silvTc.reserveMax = d;}
      else if(*(event.inputSetOptionKey) == "tctotal" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {if(d <= 0) d = SearchParams::AUTO_TIME; goldTc.gameMax = d; silvTc.gameMax = d;}
      else if(*(event.inputSetOptionKey) == "tcturns" && Global::tryStringToInt(*(event.inputSetOptionValue),i))
      {}
      else if(*(event.inputSetOptionKey) == "tcturntime" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {if(d <= 0) d = SearchParams::AUTO_TIME; goldTc.perMoveMax = d; silvTc.perMoveMax = d;}
      else if(*(event.inputSetOptionKey) == "greserve" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {goldTc.reserveCurrent = d;}
      else if(*(event.inputSetOptionKey) == "sreserve" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {silvTc.reserveCurrent = d;}
      else if(*(event.inputSetOptionKey) == "gused" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {}
      else if(*(event.inputSetOptionKey) == "sused" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {}
      else if(*(event.inputSetOptionKey) == "lastmoveused" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {}
      else if(*(event.inputSetOptionKey) == "moveused" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {
        if(d > goldTc.perMove || d > 10)
          logWarning("moveused is large, unexpected time lost: moveused = " + Global::doubleToString(d));
        goldTc.alreadyUsed = d; silvTc.alreadyUsed = d;
      }
      else if(*(event.inputSetOptionKey) == "opponent")
      {}
      else if(*(event.inputSetOptionKey) == "opponent_rating")
      {}
      else if(*(event.inputSetOptionKey) == "rating")
      {}
      else if(*(event.inputSetOptionKey) == "rated" && Global::tryStringToInt(*(event.inputSetOptionValue),i))
      {}
      else if(*(event.inputSetOptionKey) == "event")
      {}
      else if(*(event.inputSetOptionKey) == "depth" && Global::tryStringToInt(*(event.inputSetOptionValue),i))
      {
        int depth = i;
        if(depth <= 0 && !Init::ARIMAA_DEV) depth = SearchParams::AUTO_DEPTH;
        if(depth < 4) depth = 4;
        if(depth > SearchParams::AUTO_DEPTH) depth = SearchParams::AUTO_DEPTH;
        params.defaultMaxDepth = depth;
        logMessage("Max depth set to " + Global::intToString(depth));
      }
      else if(*(event.inputSetOptionKey) == "hash" && Global::tryStringToDouble(*(event.inputSetOptionValue),d))
      {
        uint64_t hashMem = (uint64_t)(round(d * 1048576.0));
        int exp = SearchHashTable::getExp(hashMem);
        if(exp > 36) exp = 36;
        params.mainHashExp = exp;
        int defaultExp = SearchParams::DEFAULT_FULLMOVE_HASH_EXP;
        params.fullMoveHashExp = max(14,min(exp-1,defaultExp));
        logMessage("Hashtable size set to 2^" + Global::intToString(exp) + ", effective next search");
      }
      else if(*(event.inputSetOptionKey) == "threads" && Global::tryStringToInt(*(event.inputSetOptionValue),i))
      {
        int threads = i;
        if(threads <= 0) threads = 1;
        if(threads > SearchParams::MAX_THREADS) threads = SearchParams::MAX_THREADS;
        params.setNumThreads(threads);
        logMessage("Threads set to " + Global::intToString(threads));
      }
      else if(*(event.inputSetOptionKey) == "ignoretc" && Global::tryStringToBool(*(event.inputSetOptionValue),bit))
      {
        ignoreTC = bit;
        if(ignoreTC)
          logMessage("Ignore tc set to true");
        else
          logMessage("Ignore tc set to false");
      }
      //Dev-only option, print out inter-search details like mainposmoves when doing a search
      else if(Init::ARIMAA_DEV && *(event.inputSetOptionKey) == "verbose" && Global::tryStringToBool(*(event.inputSetOptionValue),bit))
      {
        verbose = bit;
        if(verbose)
          logMessage("Verbose set to true");
        else
          logMessage("Verbose set to false");
      }
      //Dev-only option, can disable randomization
      else if(Init::ARIMAA_DEV && *(event.inputSetOptionKey) == "randomize" && Global::tryStringToBool(*(event.inputSetOptionValue),bit))
      {
        randomize = bit;
        if(randomize)
          logMessage("Randomize set to true");
        else
          logMessage("Randomize set to false");
      }
      //Dev-only option, can get rid of asymmetric eval and early trade avoidance
      else if(Init::ARIMAA_DEV && *(event.inputSetOptionKey) == "evalmode" && Global::tryStringToBool(*(event.inputSetOptionValue),bit))
      {
        evalMode = bit;
        if(randomize)
          logMessage("Evalmode set to true");
        else
          logMessage("Evalmode set to false");
      }
      //Dev-only option, make it so that makemoves with fewer than 4 steps are NOT assumed to have passed.
      else if(Init::ARIMAA_DEV && *(event.inputSetOptionKey) == "explicitpasses" && Global::tryStringToBool(*(event.inputSetOptionValue),bit))
      {
        explicitPasses = bit;
        if(explicitPasses)
          logMessage("Explicitpasses set to true");
        else
          logMessage("Explicitpasses set to false");
      }
      else
      {
        logWarning("Unknown option: " + (*(event.inputSetOptionKey)) + " " + (*(event.inputSetOptionValue)));
      }
      break;
    }

    case Event::INPUT_MAKESETUP:
    {
      if(asyncBot == NULL)
      {
        logError("makemove called before newgame");
        break;
      }
      vector<Board::Placement> placements;
      bool suc = Board::tryReadPlacements(*event.inputMakesetupSetup,placements);
      DEBUGASSERT(suc);
      int numPlacements = placements.size();
      Board newBoard = b;
      for(int i = 0; i<numPlacements; i++)
        newBoard.setPiece(placements[i].loc,placements[i].owner,placements[i].piece);

      if(newBoard.pieceCounts[GOLD][0] == 0)
        newBoard.setPlaStep(GOLD,0);
      else if(newBoard.pieceCounts[SILV][0] == 0)
        newBoard.setPlaStep(SILV,0);
      else
        newBoard.setPlaStep(GOLD,0);

      newBoard.refreshStartHash();
      if(!newBoard.testConsistency(prefixStream))
        logError("Illegal placement receieved, ignoring");
      else
      {
        b = newBoard;
        hist.reset(b);
      }

      searchId++; //Invalidate any earlier search's move
      break;
    }
    case Event::INPUT_MAKEMOVE:
    {
      if(asyncBot == NULL)
      {
        logError("makemove called before newgame");
        break;
      }
      move_t move = event.inputMakemoveMove;
      vector<UndoMoveData> uDatas;
      DEBUGASSERT(b.step == 0);
      bool suc = b.makeMoveLegal(move,uDatas);
      if(!suc)
      {
        logError("Illegal move, ignoring: " + Board::writeMove(move));
        break;
      }
      hist.reportMove(b,move,0);
      int step = b.step;
      if(step != 0 && !explicitPasses)
      {
        suc = b.makeMoveLegal(PASSMOVE,uDatas);
        if(!suc)
        {
          logError("Illegal move, ignoring: " + Board::writeMove(move));
          break;
        }
        hist.reportMove(b,PASSMOVE,step);
      }
      searchId++; //Invalidate any earlier search's move
      break;
    }
    case Event::INPUT_GO:
    {
      if(asyncBot == NULL)
      {
        logError("go called without newgame");
        break;
      }
      searchId++; //Invalidate any earlier search's move
      if(b.pieceCounts[0][0] == 0 || b.pieceCounts[1][0] == 0)
      {
        if(event.inputGoPonder)
          break;
        Board copy = b;
        Setup::setupNormal(copy,rand.nextUInt64());
        reply("bestmove " + Board::writePlacements(copy,gOpp(copy.player)));
        break;
      }
      asyncBot->setParams(params);
      TimeControl tc;
      if(ignoreTC) tc = TimeControl();
      else tc = b.player == GOLD ? goldTc : silvTc;
      asyncBot->setTC(tc);
      asyncBot->setRandomize(randomize);
      asyncBot->setVerbose(verbose);
      if(event.inputGoPonder)
        asyncBot->ponder(b,hist);
      else
      {
        //Log the time control status
        if(!ignoreTC)
        {
          ostringstream logTc;
          logTc << "PerMove " << tc.perMove << " (max " << tc.perMoveMax << ")" << " Reserve current " << tc.reserveCurrent << " Already used " << tc.alreadyUsed;
          logMessage(logTc.str());
        }
        asyncBot->search(b,hist,searchId,&handleSearchDone,&logMessage);
      }
      break;
    }
    case Event::INPUT_STOP:
    {
      if(asyncBot == NULL)
      {
        logError("stop called without newgame");
        break;
      }
      asyncBot->stop();
      break;
    }
    case Event::INPUT_QUIT:
    {
      if(asyncBot != NULL)
      {
        searchId++; //Invalidate any earlier search's move
        asyncBot->stop();
        delete asyncBot;
        asyncBot = NULL;
      }
      logMessage("Exiting...");
      exiting = true;
      break;
    }
    case Event::INPUT_ERROR:
    {
      if(asyncBot != NULL)
      {
        searchId++; //Invalidate any earlier search's move
        asyncBot->stop();
        delete asyncBot;
        asyncBot = NULL;
      }
      logError("Error encountered, exiting...");
      exiting = true;
      exitedOnError = true;
      break;
    }
    case Event::INPUT_EVAL:
    {
      Board copy = b;
      if(event.inputEvalPlayer != NPLA)
        copy.setPlaStep(event.inputEvalPlayer,copy.step);
      if(event.inputEvalStep >= 0 && event.inputEvalStep < 4)
        copy.setPlaStep(copy.player,event.inputEvalStep);
      Eval::evaluate(copy,copy.player,0,params.output);
      break;
    }
    case Event::BESTMOVE:
    {
      if(event.bestMoveSearchId == searchId)
      {
        move_t move = event.bestMoveMove;
        Board copy = b;
        bool suc = copy.makeMoveLegalNoUndo(move);
        if(!suc)
          logError("Bot tried to make illegal move: " + Board::writeMove(move));
        else
        {
          logMessage(*event.bestMoveLogMessage);
          reply("bestmove " + Board::writeMove(b,move,false));
        }
      }
      break;
    }
    default:
      DEBUGASSERT(false);
      break;
    }

    event.free();
    if(exiting)
      break;
  }

  inputThread.join();

  delete eventQueue;
  eventQueue = NULL;

  if(exitedOnError)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}










