#include <iostream>
#include <string>
#include <cstdlib>
#include "../core/global.h"
#include "../core/boostthread.h"
#include "../core/rand.h"
#include "../board/btypes.h"
#include "../board/board.h"
#include "../search/searchparams.h"
#include "../search/search.h"
#include "../program/asyncbot.h"

using namespace std;

static void runSearchThreadStatic(AsyncBot* bot)
{
  DEBUGASSERT(bot != NULL);
  bot->runSearchThread();
}

AsyncBot::AsyncBot(const SearchParams& params)
:params(params),
 randomize(true),
 verbose(false),
 searcher(params),
 ponderingFromHash(0),
 ponderingFromTurnNumber(0),
 currentPonderMove(ERRMOVE),
 currentPonderHashAfterMove(0),
 threadPondered(false),
 internalSearchId(0),
 currentSearchId(-1),
 searchDoneFunc(NULL),
 running(true),
 interrupted(false),
 killed(false)
{
  thread = std::thread(&runSearchThreadStatic,this);
}

AsyncBot::~AsyncBot()
{
  {
    std::unique_lock<std::mutex> lock(mutex);
    stopInternal(lock);
    DEBUGASSERT(!running);
    DEBUGASSERT(!killed);
    killed = true;
    threadVar.notify_all();
  }
  thread.join();
}

void AsyncBot::setParams(const SearchParams& p)
{
  std::lock_guard<std::mutex> userLock(userMutex);
  std::lock_guard<std::mutex> lock(mutex);
  params = p;
}
void AsyncBot::setTC(const TimeControl& t)
{
  std::lock_guard<std::mutex> userLock(userMutex);
  std::lock_guard<std::mutex> lock(mutex);
  tc = t;
}
void AsyncBot::setRandomize(bool b)
{
  std::lock_guard<std::mutex> userLock(userMutex);
  std::lock_guard<std::mutex> lock(mutex);
  randomize = b;
}
void AsyncBot::setVerbose(bool b)
{
  std::lock_guard<std::mutex> userLock(userMutex);
  std::lock_guard<std::mutex> lock(mutex);
  verbose = b;
}

//TODO possibly continue pondering through an opponent's reversal of your move
//This creates lots of ugly issues, such as what happens if the search generates
//an illegal move due to the search being for a game history different than the one
//resulting after the insta-reversal.
//For example, if you go A(me) -> B(opp) -> A(me) -> B(opp)
//and then do a ponder search from the first B that guesses opp will go B -> C, and then decides
//on C -> B as the reply, then from the second B that's actually an illegal move, so using the ponder
//search from the first B to generate a move from the second B, if the opponent goes C, is dangerous.
void AsyncBot::runSearchThread()
{
  std::unique_lock<std::mutex> lock(mutex);
  DEBUGASSERT(running); //Should be set in the constructor

  //Buffer for opp moves for pondering
  vector<move_t> oppMoves;

  //Loop repeatedly doing different searches
  while(true)
  {
    running = false;
    while(!killed && !running)
    {
      controlVar.notify_all();
      threadVar.wait(lock);
    }
    if(killed)
      break;

    //Already interrupted, just report the result and skip starting the search
    if(interrupted)
    {
      //Not pondering - report a null search result
      if(currentSearchId != -1)
      {
        DEBUGASSERT(searchDoneFunc != NULL);
        (*searchDoneFunc)(ERRMOVE,currentSearchId,SearchStats());
      }
      continue;
    }

    Board b(searchBoard);
    BoardHistory hist(searchHist);
    TimeControl baseTc = tc;
    bool isVerbose = verbose;
    searcher.params = params;
    if(randomize) searcher.params.setRandomize(true,SearchParams::DEFAULT_RANDOM_DELTA,rand.nextUInt64());
    else          searcher.params.setRandomize(false,0,0);
    searcher.resizeHashIfNeeded();

    //If pondering, then -1 indicates that we're searching the opp's turn to get some sorted oppMoves
    int numOppMovesPondered = -1;
    oppMoves.clear();

    //Loop repeatedly pondering moves
    while(true)
    {
      //See whether the control wants us to ponder
      bool shouldPonder = (currentSearchId == -1);

      //Start of pondering, haven't searched the opp's move yet
      if(shouldPonder && numOppMovesPondered == -1)
      {
        threadPondered = true;
        ponderingFromHash = b.sitCurrentHash;
        ponderingFromTurnNumber = b.turnNumber;
        ponderReplies.clear();
        currentPonderMove = ERRMOVE;
        currentPonderHashAfterMove = 0;

        TimeControl t = baseTc;
        t.isOpponentPonder = true;
        searcher.setTimeControl(t);
        searcher.params.setRootBiasStrength(SearchParams::PONDER_ROOT_BIAS);
      }
      //Pondering, already searched some moves
      else if(shouldPonder)
      {
        //Already fully pondered everything the opponent has to play, so we're done
        int numOppMoves = oppMoves.size();
        if(numOppMovesPondered >= numOppMoves)
          break;
        currentPonderMove = oppMoves[numOppMovesPondered];
        Board bb(b);
        bool suc = bb.makeMoveLegalNoUndo(currentPonderMove);
        DEBUGASSERT(suc);
        currentPonderHashAfterMove = bb.sitCurrentHash;

        TimeControl t = baseTc;
        t.isPlaPonder = true;
        searcher.setTimeControl(t);
        searcher.params.setRootBiasStrength(SearchParams::DEFAULT_ROOT_BIAS);
      }
      //Not pondering
      else
      {
        TimeControl t = baseTc;
        searcher.setTimeControl(t);
        searcher.params.setRootBiasStrength(SearchParams::DEFAULT_ROOT_BIAS);
      }

      //Increment searcher's internal id for the search we're about to begin
      internalSearchId++;
      searcher.setSearchId(internalSearchId);

      threadSearchTimer.reset();

      //BEGIN SEARCH----------------------------
      lock.unlock();
      move_t bestMove = ERRMOVE;
      SearchStats bestStats;
      if(shouldPonder && currentPonderMove == ERRMOVE)
      {
        //Pondering opp's move
        //Search opp's turn to gather some guesses
        searcher.searchID(b,hist);
        bestMove = searcher.getMove();
        bestStats = searcher.stats;
      }
      else
      {
        //Either pondering on self after opp's guessed move, or doing a regular search.
        //If pondering, make the guessed move
        Board bb(b);
        if(shouldPonder)
        {
          DEBUGASSERT(bb.step == 0);
          bool suc = bb.makeMoveLegalNoUndo(currentPonderMove);
          DEBUGASSERT(bb.step == 0);
          DEBUGASSERT(suc);
          hist.reportMove(bb,currentPonderMove,0);
        }

        //And either way, perform the search afterward.

        //Moreover, check if we can do an insta-reply by doing the same move we made earlier
        if(SearchParams::IMMEDIATELY_REMAKE_REVERSED_MOVE &&
           BoardHistory::occurredEarlierAndMoveIsLegalNow(bb,hist,bestMove))
        {
          //Do nothing - bestMove already filled in and bestStats are already the empty stats,
          //as they should be
        }
        else
        {
          searcher.searchID(bb,hist,isVerbose);
          bestMove = searcher.getMove();
          bestStats = searcher.stats;
        }
      }

      //END SEARCH------------------------------
      lock.lock();

      //Check if we're a regular search OR we've been converted to a regular search
      //in the meantime
      bool shouldPonderNew = (currentSearchId == -1);
      if(!shouldPonderNew)
      {
        //Regular or converted search, just go and return the best move
        DEBUGASSERT(searchDoneFunc != NULL);
        DEBUGASSERT(currentPonderMove != ERRMOVE || !shouldPonder); //Shouldn't ever get here from an opp's turn ponder
        (*searchDoneFunc)(bestMove,currentSearchId,bestStats);
        break;
      }
      //Still should be pondering
      else
      {
        DEBUGASSERT(shouldPonder);
        //Just searched the opp turn, now grab all the opp moves over
        if(currentPonderMove == ERRMOVE)
        {
          searcher.getSortedRootMoves(oppMoves);
          numOppMovesPondered = 0;
        }
        //Otherwise, start pondering the next move
        else
        {
          ponderReplies[currentPonderHashAfterMove] = BestmoveAndStats(bestMove,bestStats);
          numOppMovesPondered++;
        }
      }

      //Wait until now before we break when interrupted
      //because we still want to report the best move from search when interrupted
      if(interrupted)
        break;

    } //End ponder loop

  } //End thread search loop

}

void AsyncBot::stopInternal(std::unique_lock<std::mutex>& lock)
{
  //Thread in the middle of running
  if(running)
  {
    interrupted = true;
    searcher.interruptExternal(internalSearchId);

    //Note that we can drop the lock here in order to wait for the search thread to stop.
    //This means that we can't rely on the regular mutex to make AsyncBot itself threadsafe!
    //(Multiple threaded calls to stop, ponder, and search could cause problems).
    //So we use another "userMutex" throughout to guarantee user threadsafety
    while(running)
      controlVar.wait(lock);

    //Clear any pondering done by the thread
    threadPondered = false;
  }
}

void AsyncBot::ponder(const Board& b, const BoardHistory& hist)
{
  std::lock_guard<std::mutex> userLock(userMutex);
  std::unique_lock<std::mutex> lock(mutex);
  stopInternal(lock);
  searchBoard = b;
  searchHist = hist;
  currentSearchId = -1;
  searchDoneFunc = NULL;
  running = true;
  interrupted = false;
  threadVar.notify_all();
}

void AsyncBot::search(const Board& b, const BoardHistory& hist, int searchId,
    SearchDoneFunc handleSearchDone, LogFunc logMessage)
{
  (void)logMessage;
  DEBUGASSERT(searchId >= 0);
  std::lock_guard<std::mutex> userLock(userMutex);
  std::unique_lock<std::mutex> lock(mutex);

  //Check if the desired search is a continuation of a pondered position
  if(threadPondered &&
     currentSearchId == -1 &&
     hist.maxTurnBoardNumber >= b.turnNumber &&
     hist.minTurnNumber <= b.turnNumber - 1 &&
     ponderingFromHash == hist.turnSitHash[b.turnNumber-1] &&
     ponderingFromTurnNumber == b.turnNumber - 1 &&
     Board::pieceMapsAreIdentical(searchBoard,hist.turnBoard[b.turnNumber-1]))
  {
    //Check if we're pondering a move right now that matches the continuation move
    if(running && currentPonderMove != ERRMOVE && currentPonderHashAfterMove == b.sitCurrentHash)
    {
      //Sanity check - the current board matches the board you get if you take the search board and
      //make the pondered move
      Board ponderedBoard(searchBoard);
      bool suc = ponderedBoard.makeMoveLegalNoUndo(currentPonderMove);
      DEBUGASSERT(suc);
      DEBUGASSERT(ponderedBoard.sitCurrentHash == b.sitCurrentHash);
      if(Board::pieceMapsAreIdentical(ponderedBoard,b))
      {
        //Then just transform it into a regular search
        //Also jump in and set the time control to that of a regular search
        TimeControl t = tc;
        double ponderedTime = threadSearchTimer.getSeconds();
        t.addForPonder(ponderedTime);
        searcher.setTimeControl(t);
        currentSearchId = searchId;
        searchDoneFunc = handleSearchDone;
        //logMessage("Ponder hit for " + Board::writeMove(searchBoard,currentPonderMove) +
        //    ", search time " + Global::doubleToString(ponderedTime));
        return;
      }
    }
    //Check if we nonetheless have the pondered move as a triggered reply
    else if(map_contains(ponderReplies,b.sitCurrentHash))
    {
      //Get the reply
      BestmoveAndStats ponderReply = ponderReplies[b.sitCurrentHash];

      //Sanity check - make sure the reply is a legal move
      Board bb(b);
      bool suc = bb.makeMoveLegalNoUndo(ponderReply.bestMove);
      if(suc)
      {
        //Stop any current search and return the reply
        stopInternal(lock);
        //logMessage("Ponder hit for " + Board::writeMove(searchBoard,hist.turnMove[b.turnNumber-1]) +
        //    ", fully searched time " + Global::doubleToString(ponderReply.stats.timeTaken));
        (*handleSearchDone)(ponderReply.bestMove,searchId,ponderReply.stats);
        return;
      }
    }
  }

  //No ponder hit. Stop any current search if any, and start a new one
  stopInternal(lock);
  searchBoard = b;
  searchHist = hist;
  currentSearchId = searchId;
  searchDoneFunc = handleSearchDone;
  running = true;
  interrupted = false;
  threadVar.notify_all();
}

void AsyncBot::stop()
{
  std::lock_guard<std::mutex> userLock(userMutex);
  std::unique_lock<std::mutex> lock(mutex);
  stopInternal(lock);
}


