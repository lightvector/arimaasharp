
/*
 * search.h
 * Author: davidwu
 *
 *  ----- SEARCHER NOTES ------
 *
 * Eval >= Eval::WIN or <= Eval::LOSE are considered error evals and are not hashtabled.
 *
 * All of the ways I can think of right now to lose or win:
 * By goal/goal tree (correct number of steps)
 * By elim/elim tree (correct number of steps)
 * By windefsearch (correct number of steps)
 * By immo in msearch or msearch leaf (correct number of steps)
 * By failing to have any generated move (should not happen) (win in -1)
 * By various static pruning in MSearch (reversals, freecaps) (win in 0)
 * By losing a predicted repetition fight (win in the length of the pv terminating in the repeated move (filled to the full turn))
 * By having the opponent make a 3rd repetition in msearch or msearch leaf (win in the length of the pv including the repetition)
 * By the hashtable returning a trusted WL score.
 *
 * Also returning to the posstarthash is outright pruned (upon trying it) from the movelist.
 *
 * ---Search Structure----
 * Front Search for the first turn of search.
 * Main Search with tactical fight extensions and heavy pruning
 * Quiescence Search
 *  - Trades, Goals, Fights each have different max added depths
 *  - Qsearch is completely determined by the added depth so far.
 *
 * Boundary between main and qsearch:
 *
 * Main remaining 1:
 *  Recurses...
 * Main remaining 0:
 *  End conditions
 *  Conditions that prune on bad moves from 1
 *  Goal tree
 * Qsearch 0
 *
 *
 * ---Depths---
 * FDepth is the depth in the number of recurses of search - the number of moves, not steps
 * CDepth is the depth in the number of steps we have made on the board.
 * RDepth is the depth remaining to be searched * 4.
 * QDepth is the mode of qsearch:
 *   0 = Cap + GoalDef + CapDef + GoalThreat
 *   1 = Cap + GoalDef
 *   2 = Evaluate
 * HDepth is the key we store in the hashtable (RDepth or -QDepth)
 *
 * Example:
 *  C   R   Q   H
 *  0  36   -  36
 *  1  32   -  32
 *  ...
 *  7   8   -   8
 *  8   4   -   4
 *  9   -   1  -1
 * 10   -   1  -1
 * 11   -   1  -1
 * 12   -   1  -1
 * 13   -   3  -3
 *
 *
 */

#ifndef SEARCH_H
#define SEARCH_H

#include "../core/global.h"
#include "../core/timer.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../search/timecontrol.h"
#include "../search/searchparams.h"
#include "../search/searchstats.h"


class QState;
class SearchHashTable;
class ExistsHashTable;

struct SplitPoint;
struct SearchThread;
class SearchTree;
class SimpleLock;
struct RatedMove;

class Searcher
{
  public:
  //CONSTANT============================================================================

  //QSEARCH --------------------------------------------------------------------
  typedef int SearchState;
  private:
  static const SearchState SS_MAIN = 0;
  static const SearchState SS_MAINLEAF = 1;
  static const SearchState SS_Q = 2;

  //UNCHANGEABLE DURING SEARCH===========================================================
  public:
  SearchParams params; //Parameters and options for the searcher
  bool doOutput;       //Do we output top-level data about the search?

  //TOP-LEVEL MUTABLE ====================================================================
  //These parameters are mutable by the top level search functions (searchID, fsearch, etc)
  //but are not changed by individual threads in the middle of a given search.

  //BOARD AND HISTORY ------------------------------------------------------------
  //None of these should be changed during an active search except at the start
  pla_t mainPla;   //The player whose we are searching for, at the top level.
  Board mainBoard; //The board we started searching on
  BoardHistory mainBoardHistory; //The history from the start of the game up to the start of search
  vector<UndoMoveData> mainUndoData; //The undo stack from the start of the game up to the start of search

  //Updated each iteration
  int currentIterDepth; //The depth of the current iteration of search

  //STATISTICS ------------------------------------------------------------------
  SearchStats stats; //Stats combined and updated at end of search

  private:
  //FSEARCH ---------------------------------------------------------------------
  //Buffer for generating full moves, unchanging per iteration of search,
  //holds the root moves in their proper search order.
  vector<RatedMove> fullMoves;
  ExistsHashTable* fullMoveHash;

  //IDPV REPORTING---------------------------------------------------------------
  move_t* idpv;  //Holds pv found during an interative deepening
  int idpvLen;   //Length of idpv

  //THREAD-MUTABLE SHARED=====================================================================
  //These parameters are mutated freely without any synchronization and by any thread
  //in the middle of the search.

  //HASHTABLE-------------------------------------------------------------------
  SearchHashTable* mainHash;

  //MOVE ORDERING AND PRUNING--------------------------------------------------
  //History Heuristic
  vector<int64_t*> historyTable;  //[cDepth][historyIndex of move]
  vector<int64_t> historyMax;     //[cDepth]

  //TIME CONTROL AND INTERRUPTION==================================================================
  //Mutex for checking or setting the ids from an external thread, or checking whether timeout
  //or interruption has happened from within a search, or for changing any of the time parameters.
  SimpleLock* timeLock;

  int searchId;               //Current search ID
  int interruptedId;          //Always terminate any search with this ID as fast as possible
  bool didInterruptOrTimeout; //Per-search-reset flag, only ever goes false->true within a search
  bool okayToTimeout;         //Per-search-reset flag, goes false->true once the initial depth finishes enough moves
  ClockTimer clockTimer;      //Freely read (but not written) by any searchThread to decide timeout
  double hardTimeCap;         //Based on the passed-in value

  //Current max time - if time goes over this, the search is over. Can be changed as search goes.
  //Currently, is changed in searchThread, every time the root node completes a branch.
  //Also might be changed by outside information, like ponder results.
  double clockDesiredTime;

  //Time parameters controlled externally:
  TimeControl timeControl;
  double tcMinTime;
  double tcNormalTime;
  double tcMaxTime;

  //Time parameters controlled by the search progress:
  eval_t clockBaseEval;
  eval_t clockBestEval;
  int clockNumMovesTotal;
  int clockNumMovesDone;

  //SEARCH TREE===================================================================================
  public:
  //Created and destroyed for every top-level search
  SearchTree* searchTree;

  //METHODS=================================================================================

  //CONSTRUCTION----------------------------------------------------------------
  public:
  Searcher();
  Searcher(SearchParams params);
  ~Searcher();

  private:
  void init();

  //ACCESSORS----------------------------------------------------------------------------------
  public:

  //Get the best move
  move_t getMove();

  //Get the PV
  vector<move_t> getIDPV();

  //Get the PV, but join all of the moves in it into full-turn moves as much as possible
  //(only the last move might not be full)
  vector<move_t> getIDPVFullMoves();

  //Get all the root moves of the search as they were sorted at the end of the search
  void getSortedRootMoves(vector<move_t>& buf);

  //Get the number of root moves generated (note that winlosspruning may make this smaller than
  //the true number of legal moves)
  int getNumRootMoves();

  //MAIN INTERFACE-----------------------------------------------------------------------------

  //Set an id number prior to a search, used as a reference for interruption
  void setSearchId(int searchId);
  //Dynamially change the desired time control for this search
  //Safe to call from other threads.
  void setTimeControl(const TimeControl& tc);
  //Can NOT call when there is any ongoing search, NOT threadsafe. Resizes the hashtables
  //if necessary if the [params] field has been set or edited since the searcher's creation
  void resizeHashIfNeeded();

  //Perform a search, using the default max depth, time, also obeying the above time control
  void searchID(const Board& b, const BoardHistory& hist, bool output = false);
  //Perform a search up to a max of depth using a max time of seconds, optionally outputting data,
  //also obeying the desired timecontrol, if set
  void searchID(const Board& b, const BoardHistory& hist, int depth, double seconds, bool output = false);

  //Kill the search if it has the given searchID, and continue to kill any searches that might be started with this id.
  //Safe to call from other threads.
  void interruptExternal(int searchId);

  //TOP LEVEL SEARCH--------------------------------------------------------------------------------------

  //Perform search and stores best eval found in stats.finalEval, behaving appropriately depending on allowance
  //of partial search. Similarly, fills in idpv with best pv so far
  //Marks numFinished with how many moves finished before interruption
  //Uses and modifies fullHm for ordering
  //Whenever a new move is best, stores its eval in fullHm, but for moves not best, stores Eval::LOSE-1.
  //And at the end, sorts the moves
  void fsearch(int rDepth4, eval_t alpha, eval_t beta, int& numFinished, bool briefLastDepth);

  //Perform qsearch directly from the top level, without going through msearch first
  void directQSearch(int depth, eval_t alpha, eval_t beta);

  //SEARCH HELPERS----------------------------------------------------------------------------------------
  //Search a move of the given SplitPoint.
  //The particular move searched is whatever curThread has selected.
  //newSpt will be null if there is an immediate eval, else it will be the child splitpoint.
  //The currentThread's board will be unchanged if there is no new spt, but it will be changed
  //to reflect the new move if there is one.
  //If an eval is returned, curThread's will have an appropriate PV for the move at [spt->fDepth+1]
  //and also isReSearchBuf and origDepthAdjBuf will be set appropriately to indicate
  //whether the returned eval is part of a research or was from a prunereduction or an extension
  void mSearch(SearchThread* curThread, SplitPoint* spt, SplitPoint*& newSpt,
      eval_t& eval, bool& isReSearchBuf, int& origDepthAdjBuf);

  //Call when the given SplitPoint is finished (all moves searched, or beta cutoff).
  //This function will do one of two things:
  // 1) Reinitialize the splitpoint for a research and return false ("NOT DONE")
  // 2) Produce an eval to pass back to the parent, undo the board, and return true ("DONE")
  //If an eval is returned, curThread's will have an appropriate PV for the move at [spt->fDepth+1],
  //unless the thread is terminated or the results are invalid.
  bool mSearchDone(SearchThread* curThread, SplitPoint* finishedSpt, eval_t& eval);

  //Perform a quiesence search of the position
  eval_t qSearch(SearchThread* curThread, Board& b, int fDepth, int cDepth, int qDepth4, eval_t alpha, eval_t beta, SearchState qState);

  //Perform the move generation and recursive search after the initial node-entry checks from qsearch
  void qSearchRecursion(SearchThread* curThread, Board& b, int fDepth, int cDepth, int qDepth,
      eval_t& alpha, eval_t& beta, bool hashFound, move_t hashMove,
      move_t& bestMove, eval_t& bestEval, int& bestIndex, flag_t& bestFlag,
      int& winDefSteps, bool& generatedEverything);

  //Specalization of qSearch for the leaf ply
  int qSearchQPassed(SearchThread* curThread, Board& b, int fDepth, int cDepth, int qDepth, eval_t alpha, eval_t beta);

  //Evaluate the actual board position
  eval_t evaluate(SearchThread* curThread, Board& b, bool print);

  //Compute posdata for tree features from the given board if the depth is appropriate and store in curThread
  void maybeComputePosData(Board& b, const BoardHistory& history, int rDepth4, SearchThread* curThread);

  //MOVES------------------------------------------------------------------------------------------------

  static const int TRYMOVE_STOP = 0;
  static const int TRYMOVE_CONT = 1;
  static const int TRYMOVE_ERR = 2;
  int tryMove(SearchThread* curThread, move_t move, int hm, int& moveIndex,
    move_t& bestMove, eval_t& bestEval, int& bestIndex, flag_t& bestFlag,
    Board& b, int fDepth, int cDepth, int rDepth4, eval_t& alpha, eval_t& beta);

  eval_t callNextQSearch(SearchThread* curThread, move_t move, int numSteps, bool changedPlayer,
      Board& bCopy, int fDepth, int cDepth, int rDepth4, eval_t alpha, eval_t beta);

  //THREADING HELPERS-------------------------------------------------------------------------------
  //Add fsearch moves to the splitpoint, and set the pruning thresholds
  void genFSearchMoves(SplitPoint* spt, bool briefLastDepth);

  //Add msearch moves to the splitpoint using the board and killer moves of the given thread
  //IMPORTANT: This function does not modify the part of the array prior to the moves.
  //Other threads could be still searching them while we're generating more.
  //Sets areMovesUngen, movegenCallState, the various pruning thresholds, and fills the arrays.
  void genMSearchMoves(SearchThread* curThread, SplitPoint* spt);

  move_t getRootMove(move_t* mv, int* hm, int numMoves, int idx);

  move_t getMove(move_t* mv, int* hm, int numMoves, int idx);

  //Time-----------------------------------------------------------------------------

  //Perform a timecheck from the top level, setting interrupted if out of time
  void mainCheckTime();
  //Maybe perform the timecheck, setting interrupted if out of time
  void tryCheckTime(SearchThread* curThread);
  //Update the time we should search for, based on the given root eval, movesTotal is ignored if movesDone is 0
  void updateDesiredTime(eval_t bestEval, int movesDone, int movesTotal);
  //Check how much time we've searched
  double currentTimeUsed();
  //Check what the current desired time is
  double currentDesiredTime();
  //Internal function - refreshes the desired time from all of the time parameter fields in searcher
  void refreshDesiredTimeNoLock();

  //THREADING-----------------------------------------------------------------------

  //Begin a parallel search on a root split point. Call this after retrieving the split point from the search tree
  //and calling init on it
  void parallelFSearch(SearchThread* curThread, SplitPoint* spt);

  //INTERNAL TO searchthread.cpp---------------------------------------------

  void mainLoop(SearchThread* curThread, SplitPoint* spt);
  SplitPoint* doWork(SearchThread* curThread, SplitPoint* spt);
  void abandonSpt(SearchThread* curThread, SplitPoint* spt);
  SplitPoint* finishSpt(SearchThread* curThread, SplitPoint* spt);
  void reportResult(SearchThread* curThread, SplitPoint* spt, eval_t result, int moveIdx, move_t move, bool resultInvalid);
  void freeSplitPoint(SearchThread* curThread, SplitPoint* spt);
  void work(SearchThread* curThread);
};

struct KillerEntry
{
  static const int NUM_KILLERS = 8;
  move_t killer[8];
  inline KillerEntry()
  {for(int i = 0; i<NUM_KILLERS; i++) killer[i] = ERRMOVE;}
};

struct SearchHashEntry
{
  //Note: for this to work, hash_t should be 64 bits!
  volatile uint64_t key;
  volatile uint64_t data;

  SearchHashEntry();

  void record(hash_t hash, int16_t depth, eval_t eval, flag_t flag, move_t move);
  bool lookup(hash_t hash, int16_t& depth, eval_t& eval, flag_t& flag, move_t& move);
};

//Thread safe, uses lockless xor scheme to ensure data integrity
class SearchHashTable
{
  public:
  int exponent;
  hash_t size;
  hash_t mask;
  hash_t clearOffset;
  SearchHashEntry* entries;

  SearchHashTable(int exponent); //Size will be (2 ** sizeExp)
  ~SearchHashTable();

  static int getExp(uint64_t maxMem);

  void clear();

  void record(const Board& b, int cDepth, int16_t depth, eval_t eval, flag_t flag, move_t move, bool isQSearchOrEval);
  bool lookup(move_t& hashMove, eval_t& hashEval, int16_t& hashDepth, flag_t& hashFlag, const Board& b, int cDepth, bool isQSearchOrEval);

};

//NOT THREADSAFE!!!
class ExistsHashTable
{
  public:
  int exponent;
  hash_t size;
  hash_t mask;
  hash_t clearOffset;
  hash_t* hashKeys;

  ExistsHashTable(int exponent); //Size will be (2 ** sizeExp)
  ~ExistsHashTable();

  void clear();

  bool lookup(const Board& b);
  void record(const Board& b);

};

struct RatedMove
{
  move_t move;
  double val;

  //A move comes earlier in order if it's rating val is higher
  bool operator<(const RatedMove& other) const;
  bool operator>(const RatedMove& other) const;
  bool operator<=(const RatedMove& other) const;
  bool operator>=(const RatedMove& other) const;
};

#endif
