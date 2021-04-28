
/*
 * searchthread.h
 * Author: davidwu
 */

#ifndef SEARCHTHREAD_H_
#define SEARCHTHREAD_H_

#include "../core/global.h"
#include "../core/boostthread.h"
#include "../board/board.h"
#include "../search/search.h"
#include "../search/searchprune.h"

using namespace std;

struct SplitPointBuffer;
struct SearchThread;
struct SplitPoint;
class SearchTree;

//Notes:
//
//OVERVIEW OF PARALLEL SEARCH-----------------------------------------------------------------------
//
//SplitPoints------------------
//
//All of the main search is coordinated using SplitPoints.  SplitPoints track the ongoing status of the search
//at each node, such as the alpha and beta bounds, the number of moves searched, the best move found so far, etc.
//They also contain the data necessary for any thread that wishes to join in.
//
//Whenever a new node needs to be searched, a SplitPoint is allocated for that node, and when the node is
//finished, the SplitPoint is freed. This allocation is not done dynamically, but rather from preallocated
//SplitPointBuffers (see below).
//
//Whenever a thread wishes, it can publicize the SplitPoint it is working on, adding it to a global list
//of available work for free threads. Critical SplitPoint operations are performed with a per-SplitPoint mutex.
//
//Every SplitPoint must always have at least one thread working on it or a subtree extending from it. To maintain
//this invariant, the last thread to finish working at a SplitPoint and finish the SplitPoint must back up to the
//parent and then begin working on the parent. This may or may not be the thread that created the SplitPoint to
//begin with - there is no concept of a "parent" thread for a SplitPoint, in terms of the search.
//
//No SplitPoint is ever freed when it has children below it in the SearchTree, even during beta cutoffs. Instead,
//the policy is to simply mark the SplitPoint as aborted, and then threads periodically poll up the tree to see
//if any SplitPoint above them has been aborted, and if so, they walk back up, cleaning up if they're the last
//thread out.
//
//SplitPointBuffers------------
//
//SplitPoints are never dynamically allocated, but rather are all allocated at the start of search in buffers.
//Each buffer is designed for use by one thread, and contains one splitpoints per fDepth, which is the most that
//should ever be necessary. During null move or lmr, the same splitpoint is simply reused for the re-search.
//
//A thread can request a buffer from the SearchTree, at which point it exclusively owns the buffer and can get
//and free SplitPoints without synchronization.
//
//When a thread becomes free and must find work elsewhere, because the current splitpoint has no more work but
//is not yet done, if the current splitpoint came from the thread's own buffer, it abandons ownership of the buffer
//and requests a new one from the tree, no longer owning the previous one. The thread that finishes the splitpoint
//is then responsible for freeing the splitpoint, and returning the buffer, if it is empty, back to the SearchTree.
//
//At all times, we maintain the following property: the allocated SplitPoints in a buffer must form a contiguous
//path in the game tree. Moreover, if the buffer is owned (has not yet been abandoned), it must be the case that
//the deepest splitpoint worked on by the owning thread is precisely the deepest SplitPoint in the buffer.
//This guarantees safety - while the buffer is owned, the current thread is the only one able to allocate and
//free splitpoints because it is always the one at the tip of the path.
//
//The metadata (isUsed, isFirst) in the buffer, for each SplitPoint is synchronized under the SplitPoint's mutex.
//This ensures that a consistent, synchronized view is maintained even if the buffer must be disowned and thereafter
//accessed by a different thread.
//
//Lock acquisition order:
//Can only acquire locks in this order:
//  SearchTree
//  Individual splitpoint
//

//---------------------------------------------------------------------------------------

//Buffer for holding allocated SplitPoint objects. Given out to each thread, and recycled when empty
//Each fDepth gets 2 SplitPoints currently.
struct SplitPointBuffer
{
  private:
  int maxFDepth;     //Max fDepth that we have SplitPoints for
  SplitPoint* spts;  //Array of buffered SplitPoints
  bool* isUsed;      //Indicates which SplitPoints are used
  int numUsed;       //Count of how many are used

  public:
  SplitPointBuffer(Searcher* searcher, int maxFDepth);
  ~SplitPointBuffer();

  //Error checking function
  void assertEverythingUnused();

  //Get a SplitPoint for the given fDepth.
  SplitPoint* acquireSplitPoint(int fDepth);

  //Free the SplitPoint, indicating that it is done
  //Returns true if this buffer is now empty, else false
  bool freeSplitPoint(SplitPoint* spt);

};

//Structure for managing SplitPoints during a search.
//Implements a monitor, all of the synchronization is done for you, just use the interface.
//
//IMPORTANT:
//Should not call searchtree functions while a SplitPoint is locked, to maintain lock acquisition order.
class SearchTree
{
  //Backreference to searcher
  Searcher* searcher;

  //Search data
  int numThreads;         //Number of threads
  SearchThread* threads;   //Array of all threads

  //SplitPoint buffers
  int initialNumFreeSptBufs;      //How many are there initially?
  int numFreeSptBufs;             //How many are there?
  SplitPointBuffer** freeSptBufs; //Array of SplitPoint buffers available to be handed out
  SplitPointBuffer* rootSptBuf;   //Special buffer whose only job is to contain the root node

  //Root node of search
  SplitPoint* rootNode;

  //Publicized splitpoints - SplitPoints that are available work for free threads.
  //Intrusive linked list using publicNext and publicPrev pointers in splitpoint
  //Head node is a dummy splitpoint, unused.
  SplitPoint* publicizedListHead;

  //Actual boost library thread objects
  std::thread* boostThreads;

  //Main mutex
  std::mutex mutex;

  //Control variables for the flow of threads in and out of the search.
  bool searchDone;         //Is the current entire search done? Allows helper threads to completely exit
  bool iterationGoing;     //Is an iteration of search in progress? Allows helper threads to enter main loop and forces them to exit public work
  int iterationNumWaiting; //Number of helper threads sleeping in the holding bay between iterations
  std::condition_variable publicWorkCondvar; //Threads wait here for public work in the main loop
  std::condition_variable iterationCondvar; //Condvar that they wait on in the holding bay
  std::condition_variable iterationMasterCondvar; //Master waits here until all threads are in holding

  public:
  SearchTree(Searcher* searcher, int numThreads, int maxMSearchDepth, int maxCDepth,
      const Board& b, const BoardHistory& hist, const vector<UndoMoveData>& uData);
  ~SearchTree();

  //SEARCH INTERFACE------------------------------------------------
  //Get the root node, for the purposes of initialization
  SplitPoint* acquireRootNode();
  //Free the root node
  void freeRootNode(SplitPoint* spt);

  //INTERNAL to searchthread.cpp-------------------------------------

  //Start one iteration of iterative deepening. Call from master thread before starting.
  void beginIteration();
  //End one iteration of iterative deepening, call from inside the main loop when the first thread finds itself done
  //with the root node.
  void endIterationInternal();
  //End one iteration of iterative deepening, waits for threads to exit the main loop. Call from master
  //thread after an iteration, including after timeout
  void endIteration();


  //Get a new buffer of splitpoints for a thread to use during search.
  SplitPointBuffer* acquireSplitPointBuffer();
  //Free an unused buffer of splitpoints that has been disowned, or one's own buffer
  void freeSplitPointBuffer(SplitPointBuffer* buf);


  //Publicize the SplitPoint so that other threads can help
  //The SplitPoint must be initialized with board data before this is called!
  void publicize(SplitPoint* spt);
  //Depublicize the SplitPoint. Call only when returning this splitpoint back to buffer.
  //Okay to call for splitpoints that aren't public
  void depublicize(SplitPoint* spt);


  //Find a good publicized SplitPoint and grab work from it for the given thread.
  //If none, blocks until there is some. Returns the splitpoint UNLOCKED, with
  //the thread having successfully grabbed work from it.
  //Returns NULL is the search is done.
  SplitPoint* getPublicWork(SearchThread* thread);

  //Return the master thread
  SearchThread* getMasterThread();

  //Get stats from all threads combined together
  SearchStats getCombinedStats();

  //Copy killer moves and the len from another thread into the given array and the len reference
  //Not synchronized.
  void copyKillersUnsynchronized(int fromThreadId, KillerEntry* killerMoves, int killerMovesLen);

  private:
  static void runChild(SearchTree* tree, Searcher* searcher, SearchThread* curThread);
  SplitPoint* actuallyLookForWork(SearchThread* curThread);
};

//A single node in the seach tree at which we are parallelizing
//All member arrays are owned by this SplitPoint.
struct SplitPoint
{
  //Constants--------------------------------

  //Node types
  static const int TYPE_INVAL = 0;
  static const int TYPE_ROOT = 1;  //Root node of whole search
  static const int TYPE_PV = 2;    //Expects exact value
  static const int TYPE_CUT = 3;   //Expects beta cutoff
  static const int TYPE_ALL = 4;   //Expects alpha fail

  //Modes of search
  static const int MODE_INVAL = 0;
  static const int MODE_NORMAL = 1;    //Normal search, back up the value when we finish
  static const int MODE_REDUCED = 2;   //Reduced search, re-search unless we fail low
  static const int MODE_NULLMOVE = 3;  //Null move search, re-search unless we fail high

  //Constant--------------------------------------------------------------------------------------

  //Guaranteed constant throughout the search
  Searcher* searcher;       //The searcher running this search
  int fDepth;               //Search tree recursion depth of this splitpoint.
  SplitPointBuffer* buffer; //The SplitPointBuffer containing this splitpoint
  int bufferIdx;            //The index of this splitpoint in the buffer

  //Data synchronized under SearchTree------------------------------------------------------------

  //Intrusive doubly-linked list for SearchTree::publicizedListHead
  SplitPoint* publicNext;
  SplitPoint* publicPrev;
  bool isPublic;

  //Data synchronized under SplitPoint------------------------------------------------------------
  //Any of the below, including parameters that "seem" constant, MAY CHANGE in the middle of search
  //because a re-search might reinitialize the splitpoint fresh!
  //This is okay, however, because this can only happen when a splitpoint is finished and would
  //be freeable anyways.

  //Mutex---------------------------------------------
  private:
  std::mutex mutex;
  public:

  //Constant during an active search (but could change on a research)----------------------
  SplitPoint* parent; //The parent splitpoint above this one in the tree, if any.
  bool isInitialized; //Set to false when freed, set true after initialization

  move_t* moveHistory; //Indexed by fdepth, tracks the moves made from the start of search.
  int moveHistoryCapacity;
  int moveHistoryLen;

  int stepTurn;  //Turn number * 4 + step, for error checking

  int parentMoveIndex; //The index of the move made in the parent to get to this move
  move_t parentMove; //The move of the parent
  int searchMode; //The mode of search this is from the parent
  bool changedPlayer; //Is the player to move different from the parent's?

  bool isReSearch; //Is this node a re-search of a prunereduced node?
  int origDepthAdjustment; //The number of steps this node was extended or reduced by on the initial search of the node
  bool hasNullMove; //Is the first move in the movelist of this node a null move that we wanted to try first?

  int cDepth;  //Depth in Arimaa steps from the start of search
  int rDepth4; //Depth remaining * 4 until the end of search.

  hash_t hash; //The situation board hash at this position. For error checking.

  //Mutable in the middle of search-------------------------------------------
  int nodeType; //Estimated type of node
  double nodeAllness; //For when we are an ALL node. To what degree does this node seem to be an ALL node?

  std::atomic<eval_t> alpha; //The current alpha bound at this node, may improve as search progresses
  eval_t beta; //The current beta bound at this node
  eval_t extraEval; //Extra eval points to add to node before returning to parent, if not a win/loss

  eval_t bestEval; //The best eval so far at this node
  move_t bestMove; //The best move found so far at this node
  flag_t bestFlag; //Eval flag indicating current alpha-beta bound status of this node
  int bestIndex;   //The index of the best move found so far

  move_t* pv; //A pv array, the best pv at this node
  int pvLen;  //The length of the best pv at this node.

  move_t* mv;     //A moves array
  int* hm;        //A move ordering heuristic array, or null
  int mvCapacity; //Capacity of the moves and hm array
  int numMoves;   //How many moves are there to search?

  bool areMovesUngen; //Are there still moves left to be generated?

  int moveGenCallState; //For determining what point we are in move generation
  bool moveGenWinDefGen; //Were there any windefmoves generated in this position?

  int numMovesStarted;  //The number of moves we have started searching at this splitpoint
  int numMovesStopped;  //The number of moves that have been searched and finished (or aborted, and the thread returned)
  int numMovesDone;     //The number of moves that have been searched and finished properly, without abort
  int numMovesDoneLegal;//The number of moves that have been searched and finished properly, without abort, and were legal and not pruned. Does not include nullmove.
  int numSpecialMoves;  //Number of special moves at the front of the move array, such as hash move and killer moves

  //For avoiding repeatedly prunereducing and and failing high but researching and failing low
  //Increased on every failure to reduce a search, decreased on every successful reduction.
  //Once it hits a certain threshold, we stop reducing.
  int reduceFailyness;
  //We only ban reductions by numbers of steps equal (or less) to the failures we've actually seen. So if we switch to
  //reducing more, we give a chance to restart the reducing if the deeper reduction is fine.
  int banReductionsLeq;

  //The hashmove, for move ordering
  move_t hashMove;

  //Has this SplitPoint been terminated due to a beta cutoff either here or in a parent or a timeout?
  //Set immediately upon beta cutoff report or any timed out thread reporting a result to this node,
  //and polled up the tree every so often and set if a parent has it set.
  //Indicates that no further results should be reported to this node and that this node has no more work.
  bool isAborted;
  //Should the results in this node be considered invalid?
  //Set when a poll found a parent was aborted for any reason, or a timed out
  //Prevents mSearchDone from storing any hashes, or attempting a re-search of this node.
  bool resultsAreInvalid;

  int copyKillersFromThread; //Copy killer moves from this thread when joining this splitpoint, if not -1.

  //Different pruning amounts. Perform this when moveIdx >= threshold
  int pvsThreshold;
  int r1Threshold;
  int r2Threshold;
  int r3Threshold;
  int pruneThreshold;


  SplitPoint();
  ~SplitPoint();

  //Initialize or reinitialize to be the split point for a given position
  //The last three parameters specify some starting moves to load it with, and are optional. Can be used
  //to load in a hashMove, or at the root, the full moves.
  //moveArraySize - how big to ensure the move array
  void init(SplitPoint* parent, int parentMoveIdx, move_t parentMove, int searchMode,
      int stepTurn, bool changedPlayer, bool isReSearch, int origDepthAdj,
      int cDepth, int rDepth4, hash_t hash, eval_t alpha, eval_t beta, eval_t extra, bool isRoot,
      move_t hashMove, KillerEntry killers, bool shouldNull,
      int moveArraySize, int copyKillersFrom);

  //Helper
  void initNodeType(bool isRoot);

  //Synchronization--------------------------------------
  void lock(SearchThread* thread);
  void unlock(SearchThread* thread);

  //Post-Initialization----------------------------------
  //Must be locked to use any of these!

  //Does this SplitPoint need to be publicized now for parallelization?
  bool needsPublication();

  //Checks if this splitpoint probably has work. Used by getPublicWork to check if a splitpoint might
  //have work before going through the costly task of syncing with it to actually try to get work.
  //Necessary because actually getting work might require generating moves, which requires the thread
  //to be synced first.
  bool probablyHasWork();

  //Get work for the current thread and record the appropriate fields.
  //IMPORATANT: Can only call this function if the current thread is synced with this splitpoint, because in the
  //case that moves are ungen, will use the thread's own board to generate moves!
  //Returns true if actual work was obtained, else false.
  bool getWork(SearchThread* curThread);

  //Is this SplitPoint done for this thread?
  //In the case that the thread is terminated, or that the splitpoint is aborted, will exit early
  //if this thread is the only one left.
  //Note that on abort or terminated, we still have to wait for numMovesStarted == numMovesDone.
  //Because we need to make sure all the threads notice the abort, before we recycle this SplitPoint.
  bool isDone(const SearchThread* curThread);

  //Report that a child search was finished, with the given result
  //Pass in the move and move index made that obtained this result, and also the pv of the child (not including the move)
  //curThread is assumed to have just finished a search, holding the pv for the move at fdepth+1.
  void reportResult(eval_t result, int moveIdx, move_t move, bool resultInvalid,
      bool reSearched, int origDepthAdj, SearchThread* curThread);

  //Unsynchronizedly check if any parent nodes are aborted.
  //Does NOT lock parents - safe because aborted can only toggle one way.
  //Although now this function doesn't mutate anything, note that previously this function used to set
  //aborted for intermediate parents up to the aborted parent without synchronizing. This was bad and
  //potentially buggy - there might have been be a bad case where a thread comes in on the parent and locks it
  //and takes an action knowing it's aborted, then unlocks it, then another thread comes in and locks it without
  //seeing that it's aborted, because that change never became visible because it never happened in a synchronized context!
  //Now, instead we maintain the property that isAborted, for mutation purposes, is synchronized under the lock
  //like everything else.
  bool anyParentAbortedUnsynchronized();

};

//Unsynchronized thread-local data
struct SearchThread
{
  //Thread id
  int id;

  //All members are local to the thread only, and should generally only be accessed by the owning thread,
  //unless stated otherwise

  //BACK REFERCENCE TO SEARCHER---------------------------------------------------
  Searcher* searcher;

  //STATISTICS -------------------------------------------------------------------
  SearchStats stats; //Stats individual to each thread

  //BOARD HISTORY -----------------------------------------------------------------
  Board board;                 //Current board in the search done by this thread
  BoardHistory boardHistory;   //Tracks data about the history of the board and moves up to the current point of search.
  vector<UndoMoveData> undoData;

  //UNSAFE PRUNING DATA----------------------------------------------------------
  vector<hash_t> unsafePruneIdHash;     //Hash of turnBoard from which featurePosData computed
  vector<vector<SearchPrune::ID>*> unsafePruneIds;

  //KILLERS----------------------------------------------------------------------
  //READ BY OTHER THREADS!!!
  //This is okay because we don't really care if a race causes a killer move to be copied incorrectly
  //since it's just for ordering and we do legality-check it. But we this means we should also never
  //reallocate the killerMoves array.
  KillerEntry* killerMoves;  //Killer moves for each cdepth
  int killerMovesLen;   //Maximum size of the array

  //MOVE GENERATION --------------------------------------------------------------
  //Move buffers for each fdepth of search, in case we don't use a SplitPoint (such as in qsearch)
  move_t* mvList;
  int* hmList;
  int mvListCapacity;
  int mvListCapacityUsed;

  //PV REPORTING -----------------------------------------------------------------
  move_t** pv;   //Scratchwork array for intermediate pvs, indexed [fdepth][move]
  int* pvLen;    //Length of pvs in scratchwork array, indexed [fdepth]

  //THREAD STATE -----------------------------------------------------------------
  int curSplitPointMoveIdx;
  move_t curSplitPointMove;
  bool curPruneReduceDesired;
  int curDepthAdjDesired;

  bool isTerminated; //Has this thread discovered that it needs to terminate due to interruption or timeout?

  //The current buffer of SplitPoints that we are using when we get splitpoints
  SplitPointBuffer* curSplitPointBuffer;

  //TIME CHECK-------------------------------------------------------------------
  int timeCheckCounter;  //Incremented every mnode or qnode, for determining when to check time

  //SYNCHRONIZATION--------------------------------------------------------------
  //If locking or about to lock an spt, store here
  //This field is just for error checking - to make sure a thread never unlocks a different
  //splitpoint than it locked, or that it locks a splitpoint when it already has one.
  SplitPoint* lockedSpt;


  //METHODS========================================================================
  SearchThread();
  ~SearchThread();

  //Initialize this search thread to be ready to search for the given root position
  //Called only once, at the start of search before any iterations
  void initRoot(int id, Searcher* searcher, const Board& b, const BoardHistory& hist,
      const vector<UndoMoveData>& uData, int maxCDepth);

  //Search control logic------------------------------------

  //Called whenever we begin searching at a new splitpoint that was NOT a
  //continuation of any existing splitpoint. That is, called when we begin searching at the root, as well
  //as when we jump to a different splitpoint in the tree and join in.
  //IMPORTANT: No need for splitpoint to be locked, but if so, then it must be the case that
  //the search tree is locked the whole time, so that this splitpoint won't unexpectedly get
  //depublicized and returned at the same time as this function runs!
  void syncWithSplitPointDistant(const SplitPoint* spt);

  //Copy killer moves from the appropriate thread for this splitpoint, when we are joining in
  //and trying to sync with it.
  //Splitpoint need not be locked, however this thread should have work allocated from it.
  void copyKillersForSplitPointDistant(const SplitPoint* spt);

  //Misc-----------------------------------------------------

  //Initialize or reinitialize this search thread to be ready to search from the given point
  //Must pass in the turn number of the initial board, so we can sync histories
  void init(SplitPoint* pt, int mainBoardTurnNumber);

  //Record any pv from the fdepth below us into the current one, or to an array of choice
  void copyExtendPVFromNextFDepth(move_t bestMove, int fDepth);
  void copyExtendPVFromNextFDepthTo(move_t bestMove, int fDepth, move_t* targetpv, int& targetlen);

  //Record a PV of length 0 at this depth
  void endPV(int fDepth);

  //fDepth-bounds-checked way of getting the pv
  move_t* getPV(int fDepth);

  //fDepth-bounds-checked way of getting the pv
  int getPVLen(int fDepth);
};

//Simple lock for search.h to use, so that we don't have to include boost thread
//in search.h which everything else includes.
class SimpleLock
{
  std::mutex mutex;
  public:
  SimpleLock();
  ~SimpleLock();
  void lock();
  void unlock();
};


#endif /* SEARCHTHREAD_H_ */
