/*
 * mainthreadtest.cpp
 * Author: davidwu
 */

#include <cstdlib>
#include "../core/global.h"
#include "../core/boostthread.h"
#include "../core/timer.h"
#include "../core/threadsafequeue.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../program/asyncbot.h"
#include "../main/main.h"

static void setDefaultParams(SearchParams& params)
{
  BradleyTerry learner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(learner);
  params.setRootFancyPrune(true);
  params.setMoveImmediatelyIfOnlyOneNonlosing(false);
}

namespace {
typedef void (*Func)(void* data);
STRUCT_NAMED_TRIPLE(double,delay,Func,func,void*,data,DelayedFunc);

STRUCT_NAMED_PAIR(Searcher*,searcher,int,searchId,SearcherAndId);
STRUCT_NAMED_PAIR(Searcher*,searcher,TimeControl,tc,SearcherAndTc);
}

static ThreadSafeQueue<DelayedFunc>* delayedFuncQueue = NULL;
static ThreadSafeQueue<bool>* waitQueue = NULL;
static ThreadSafeQueue<bool>* doneQueue = NULL;

static void runDelayedThread()
{
  while(true)
  {
    DelayedFunc func = delayedFuncQueue->waitPop();
    if(func.func == NULL)
      break;

    const std::chrono::nanoseconds nanos((int64_t)(func.delay * 1.0e9));
    std::this_thread::sleep_for(nanos);

    (*(func.func))(func.data);

    waitQueue->push(true);
  }
}

static void handleInterruptAfter(void* d)
{
  SearcherAndId* data = (SearcherAndId*)d;
  data->searcher->interruptExternal(data->searchId);
  delete data;
}
static void scheduleInterruptAfter(Searcher& searcher, int searchId, double delay)
{
  SearcherAndId* data = new SearcherAndId(&searcher,searchId);
  delayedFuncQueue->push(DelayedFunc(delay,&handleInterruptAfter,data));
}

static void handleTcAfter(void* d)
{
  SearcherAndTc* data = (SearcherAndTc*)d;
  data->searcher->setTimeControl(data->tc);
  delete data;
}
static void scheduleTcAfter(Searcher& searcher, const TimeControl& tc, double delay)
{
  SearcherAndTc* data = new SearcherAndTc(&searcher,tc);
  delayedFuncQueue->push(DelayedFunc(delay,&handleTcAfter,data));
}

static void handleStopAfter(void* d)
{
  AsyncBot* bot = (AsyncBot*)d;
  bot->stop();
}
static void scheduleStopAfter(AsyncBot& bot, double delay)
{
  delayedFuncQueue->push(DelayedFunc(delay,&handleStopAfter,&bot));
}

static void handleNothing(void* d)
{
  (void)d;
}
static void scheduleNothing(double delay)
{
  delayedFuncQueue->push(DelayedFunc(delay,&handleNothing,NULL));
}

static void logMessage(const string& message)
{
  cout << message << endl;
}

static void handleSearchDone(move_t move, int searchId, const SearchStats& stats)
{
  (void)move;
  SearchStats::printBasic(cout,stats,searchId);
  doneQueue->push(true);
}

static void runSearcherThreadTests(const vector<Board>& boards)
{
  int numBoards = boards.size();

  int searchId = 0;
  SearchParams params;
  setDefaultParams(params);
  Searcher searcher(params);

  //Begin thread!
  delayedFuncQueue = new ThreadSafeQueue<DelayedFunc>();
  waitQueue = new ThreadSafeQueue<bool>();
  doneQueue = new ThreadSafeQueue<bool>();
  std::thread delayedThread(&runDelayedThread);

  cout << "Searching each position and interrupting after 0.1 seconds..." << endl;
  {
    double delay = 0.1;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      searcher.setSearchId(searchId);
      scheduleInterruptAfter(searcher,searchId,delay);
      searcher.searchID(b,hist,false);
      SearchStats::printBasic(cout,searcher.stats,i);

      searchId++;
      waitQueue->waitPop();
    }
  }

  cout << "Searching each position and interrupting after 2 seconds..." << endl;
  {
    double delay = 2.0;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      searcher.setSearchId(searchId);
      scheduleInterruptAfter(searcher,searchId,delay);
      searcher.searchID(b,hist,false);
      SearchStats::printBasic(cout,searcher.stats,i);

      searchId++;
      waitQueue->waitPop();
    }
  }

  cout << "Searching each position with a permissive time control and then changing to 3 seconds max after 4 seconds..." << endl;
  {
    double delay = 4.0;
    TimeControl permissive;
    TimeControl restrictive;
    restrictive.perMove = 2.0;
    restrictive.reserveCurrent = 12.0;
    restrictive.perMoveMax = 3.0;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      searcher.setSearchId(searchId);
      searcher.setTimeControl(permissive);
      scheduleTcAfter(searcher,restrictive,delay);
      searcher.searchID(b,hist,false);
      SearchStats::printBasic(cout,searcher.stats,i);

      searchId++;
      waitQueue->waitPop();
    }
  }

  cout << "Searching each position with a permissive time control and then changing to several seconds after 1 seconds..." << endl;
  {
    double delay = 1.0;
    TimeControl permissive;
    TimeControl restrictive;
    restrictive.perMove = 3.0;
    restrictive.reserveCurrent = 20.0;
    restrictive.perMoveMax = 6.0;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      searcher.setSearchId(searchId);
      searcher.setTimeControl(permissive);
      scheduleTcAfter(searcher,restrictive,delay);
      searcher.searchID(b,hist,false);
      SearchStats::printBasic(cout,searcher.stats,i);

      searchId++;
      waitQueue->waitPop();
    }
  }

  delayedFuncQueue->push(DelayedFunc(0,NULL,NULL));
  delayedThread.join();
  DEBUGASSERT(waitQueue->size() == 0);
  DEBUGASSERT(doneQueue->size() == 0);
  delete delayedFuncQueue;
  delete waitQueue;
  delete doneQueue;
  cout << "Yay, all done" << endl;
}

static void runAsyncBotThreadTests(const vector<Board>& boards)
{
  int numBoards = boards.size();

  SearchParams params;
  setDefaultParams(params);

  AsyncBot bot(params);
  bot.setTC(TimeControl());
  bot.setRandomize(false);

  //Begin thread!
  delayedFuncQueue = new ThreadSafeQueue<DelayedFunc>();
  waitQueue = new ThreadSafeQueue<bool>();
  doneQueue = new ThreadSafeQueue<bool>();
  std::thread delayedThread(&runDelayedThread);

  cout << "Searching each position and calling stop immediately..." << endl;
  {
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      bot.search(b,hist,i,&handleSearchDone,&logMessage);
      bot.stop();
      doneQueue->waitPop();
    }
  }

  cout << "Searching each position and immediately hopping ahead to the next without stop, except for the last after 10 seconds..." << endl;
  {
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      bot.search(b,hist,i,&handleSearchDone,&logMessage);
    }
    scheduleStopAfter(bot,10.0);
    waitQueue->waitPop();
    for(int i = 0; i<numBoards; i++)
      doneQueue->waitPop();
  }

  cout << "Searching each position and calling stop after 2 seconds..." << endl;
  {
    double delay = 2.0;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      bot.search(b,hist,i,&handleSearchDone,&logMessage);
      scheduleStopAfter(bot,delay);
      waitQueue->waitPop();
      doneQueue->waitPop();
    }
  }

  cout << "Searching all positions for 4 seconds to find simulated opp moves" << endl;
  vector<move_t> simulatedOppMoves;
  {
    Searcher searcher(params);
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);
      searcher.searchID(b,hist,SearchParams::AUTO_DEPTH,4.0,false);
      move_t move = searcher.getMove();
      simulatedOppMoves.push_back(move);
      DEBUGASSERT(move != ERRMOVE);
      SearchStats::printBasic(cout,searcher.stats,i);
    }
    cout << endl;
  }

  cout << "Basic time control desired times" << endl;
  {
    TimeControl tc;
    tc.perMove = 6.0;
    tc.reserveCurrent = 25.0;
    tc.perMoveMax = 12.0;
    cout << tc.getMinNormalMaxStr() << endl;
  }

  cout << "Searching all post-opp move positions using a simple time control of around 6 seconds" << endl;
  {
    TimeControl tc;
    tc.perMove = 6.0;
    tc.reserveCurrent = 25.0;
    tc.perMoveMax = 12.0;
    bot.setTC(tc);
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);

      Board copy = b;
      int oldStep = copy.step;
      move_t oppMove = simulatedOppMoves[i];
      bool suc = copy.makeMoveLegalNoUndo(oppMove);
      DEBUGASSERT(suc);
      hist.reportMove(copy,oppMove,oldStep);

      bot.search(copy,hist,i,&handleSearchDone,&logMessage);
      doneQueue->waitPop();
    }
  }

  cout << "Searching all positions using a simple time control of around 6 seconds but pondering for 4 seconds beforehand" << endl;
  {
    double ponderTime = 4.0;
    TimeControl tc;
    tc.perMove = 6.0;
    tc.reserveCurrent = 25.0;
    tc.perMoveMax = 12.0;
    bot.setTC(tc);
    ClockTimer timer;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);

      bot.ponder(b,hist);
      scheduleNothing(ponderTime);
      waitQueue->waitPop();

      Board copy = b;
      int oldStep = copy.step;
      move_t oppMove = simulatedOppMoves[i];
      bool suc = copy.makeMoveLegalNoUndo(oppMove);
      DEBUGASSERT(suc);
      hist.reportMove(copy,oppMove,oldStep);

      timer.reset();
      bot.search(copy,hist,i,&handleSearchDone,&logMessage);
      doneQueue->waitPop();
      cout << "External time " << timer.getSeconds() << endl;
    }
  }

  cout << "Searching all positions using a simple time control of around 6 seconds but pondering for 10 seconds beforehand" << endl;
  {
    double ponderTime = 10.0;
    TimeControl tc;
    tc.perMove = 6.0;
    tc.reserveCurrent = 25.0;
    tc.perMoveMax = 12.0;
    bot.setTC(tc);
    ClockTimer timer;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);

      bot.ponder(b,hist);
      scheduleNothing(ponderTime);
      waitQueue->waitPop();

      Board copy = b;
      int oldStep = copy.step;
      move_t oppMove = simulatedOppMoves[i];
      bool suc = copy.makeMoveLegalNoUndo(oppMove);
      DEBUGASSERT(suc);
      hist.reportMove(copy,oppMove,oldStep);

      timer.reset();
      bot.search(copy,hist,i,&handleSearchDone,&logMessage);
      doneQueue->waitPop();
      cout << "External time " << timer.getSeconds() << endl;
    }
  }

  cout << "Searching all positions using a simple time control of around 6 seconds but pondering for 10 seconds beforehand,"
       << "and the ponder uses a permissive time control" << endl;
  {
    double ponderTime = 10.0;
    TimeControl permissive;
    TimeControl tc;
    tc.perMove = 6.0;
    tc.reserveCurrent = 25.0;
    tc.perMoveMax = 12.0;
    ClockTimer timer;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);

      bot.setTC(permissive);
      bot.ponder(b,hist);
      scheduleNothing(ponderTime);
      waitQueue->waitPop();

      Board copy = b;
      int oldStep = copy.step;
      move_t oppMove = simulatedOppMoves[i];
      bool suc = copy.makeMoveLegalNoUndo(oppMove);
      DEBUGASSERT(suc);
      hist.reportMove(copy,oppMove,oldStep);

      timer.reset();
      bot.setTC(tc);
      bot.search(copy,hist,i,&handleSearchDone,&logMessage);
      doneQueue->waitPop();
      cout << "External time " << timer.getSeconds() << endl;
    }
  }


  cout << "Searching all positions using a simple time control of around 6 seconds but pondering for 10 seconds beforehand,"
       << "but also stopping between the ponder and search" << endl;
  {
    double ponderTime = 10.0;
    TimeControl tc;
    tc.perMove = 6.0;
    tc.reserveCurrent = 25.0;
    tc.perMoveMax = 12.0;
    bot.setTC(tc);
    ClockTimer timer;
    for(int i = 0; i<numBoards; i++)
    {
      const Board& b = boards[i];
      BoardHistory hist(b);

      bot.ponder(b,hist);
      scheduleNothing(ponderTime);
      waitQueue->waitPop();
      bot.stop();

      Board copy = b;
      int oldStep = copy.step;
      move_t oppMove = simulatedOppMoves[i];
      bool suc = copy.makeMoveLegalNoUndo(oppMove);
      DEBUGASSERT(suc);
      hist.reportMove(copy,oppMove,oldStep);

      timer.reset();
      bot.search(copy,hist,i,&handleSearchDone,&logMessage);
      doneQueue->waitPop();
      cout << "External time " << timer.getSeconds() << endl;
    }
  }

  delayedFuncQueue->push(DelayedFunc(0,NULL,NULL));
  delayedThread.join();
  DEBUGASSERT(waitQueue->size() == 0);
  DEBUGASSERT(doneQueue->size() == 0);
  delete delayedFuncQueue;
  delete waitQueue;
  delete doneQueue;
  cout << "Yay, all done" << endl;
}


int MainFuncs::runThreadTests(int argc, const char* const *argv)
{
  const char* usage =
      "posfile <-searcher> <-asyncbot>";
  const char* required = "";
  const char* allowed = "searcher asyncbot";
  const char* empty = "searcher asyncbot";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() <= 1)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  vector<Board> boards = ArimaaIO::readBoardFile(mainCommand[1]);

  if(!Command::isSet(flags,"searcher") && !Command::isSet(flags,"asyncbot"))
  {
    cout << "Please specify either -searcher or -asynbot to test" << endl;
    {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}
  }

  if(Command::isSet(flags,"searcher"))
    runSearcherThreadTests(boards);
  if(Command::isSet(flags,"asyncbot"))
    runAsyncBotThreadTests(boards);
  return EXIT_SUCCESS;
}




