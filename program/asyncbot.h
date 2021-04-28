
#ifndef ASYNCBOT_H_
#define ASYNCBOT_H_

#include <string>
#include <map>
#include "../core/boostthread.h"
#include "../core/rand.h"
#include "../board/btypes.h"

using namespace std;

typedef void (*SearchDoneFunc)(move_t bestMove, int searchId, const SearchStats& stats);
typedef void (*LogFunc)(const string& msg);

class AsyncBot
{
  STRUCT_NAMED_PAIR(move_t,bestMove,SearchStats,stats,BestmoveAndStats);

  std::mutex userMutex; //Additional mutex to make AsyncBot itself threadsafe for callers
  std::mutex mutex;
  std::thread thread;
  std::condition_variable threadVar;  //Search thread waits on this
  std::condition_variable controlVar; //Control threads (callers of AsyncBot functions) wait on this

  //Search parameters-----------------------------------
  SearchParams params;
  TimeControl tc;
  bool randomize;
  bool verbose;
  Rand rand;

  //Search data------------------------
  Board searchBoard;       //Used only briefly to communicate the board to the search thread
  BoardHistory searchHist; //Used only briefly to communicate the history to the search thread
  Searcher searcher;

  //Search Thread -> Control communication --------------------------------------
  hash_t ponderingFromHash;  //Situation hash of opp board position currently pondering from, if threadPondered
  int ponderingFromTurnNumber; //Turn number of opp board position currently pondering from, if threadPondered
  map<hash_t,BestmoveAndStats> ponderReplies; //Map from hash after opp move -> planned ponderReply, if threadPondered
  move_t currentPonderMove;  //Current pondered move, possibly ERRMOVE if still trying to guess, if threadPondered and threadRunning
  hash_t currentPonderHashAfterMove; //Hash after current pondered move, if not ERRMOVE
  bool threadPondered;       //Set to true by thread at start of pondering, set to false by control thread.
  int internalSearchId;      //Incremented and used internally for Searcher's searchId.
  ClockTimer threadSearchTimer; //Reset every time the search thread does a new search

  //Control -> Search Thread communication --------------------------------------
  int currentSearchId;           //searchId of the current search if running, or -1 if pondering. NOT the same as internalSearchId
  SearchDoneFunc searchDoneFunc; //Call this func when done searching, or NULL if pondering
  bool running;                  //Currently searching or pondering? Set to true by control thread, false by search thread
  bool interrupted;              //If true, the search thread should stop at the first chance
  bool killed;                   //Thread should exit at first chance?

  public:
  AsyncBot(const SearchParams& params);
  ~AsyncBot();

  //Calls to the below functions ARE threadsafe.

  //These parameters only hold for ponders and searches that start after,
  //and not any ponders or searches that were started prior. Non-time-control
  //parameters will also not apply to a search if it continues from a ponder
  //that was started prior to the setting of the parameter.
  void setParams(const SearchParams& params);
  void setTC(const TimeControl& tc);
  void setRandomize(bool b);
  void setVerbose(bool b);

  //Interrupt any previous search and start pondering from the given position,
  //where it is the opponent's move.
  //Returns once the new pondering is started.
  void ponder(const Board& b, const BoardHistory& hist);

  //Interrupts any previous search and starts a new search, unless the previous search
  //was a call to ponder that sucessfully guessed the opponent's move.
  //Returns once the new search is started.
  //When the search is done or interrupted, the provided callback will be called,
  //possibly with ERRMOVE if interrupted.
  //NOTE: The callback should NOT call any AsyncBot functions, this will deadlock.
  void search(const Board& b, const BoardHistory& hist, int searchId, SearchDoneFunc handleSearchDone, LogFunc logMessage);

  //Interrupt any previous search or ponder, if any.
  //Blocks until stopped (in particular, handleSearchDone of an interrupted search
  //may be called prior to stop returning).
  void stop();

  //INTERNAL USE ONLY
  void runSearchThread();
  void stopInternal(std::unique_lock<std::mutex>& lock);
};

#endif
