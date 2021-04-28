/*
 * searchthread.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../core/boostthread.h"
#include "../learning/featuremove.h"
#include "../eval/eval.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../search/searchthread.h"
#include "../search/searchutils.h"
#include "../program/arimaaio.h"

using namespace std;

//SEARCHTREE---------------------------------------------------------------------------

SplitPointBuffer::SplitPointBuffer(Searcher* searcher, int maxfd)
{
  DEBUGASSERT(maxfd >= 0);
  maxFDepth = maxfd;
  spts = new SplitPoint[maxfd+1];
  isUsed = new bool[maxfd+1];
  numUsed = 0;

  for(int depth = 0; depth <= maxfd; depth++)
  {
    spts[depth].searcher = searcher;
    spts[depth].fDepth = depth;
    spts[depth].buffer = this;
    spts[depth].bufferIdx = depth;
    isUsed[depth] = false;
  }
}

SplitPointBuffer::~SplitPointBuffer()
{
  delete[] spts;
  delete[] isUsed;
}

void SplitPointBuffer::assertEverythingUnused()
{
  DEBUGASSERT(numUsed == 0);
  for(int depth = 0; depth <= maxFDepth; depth++)
  {
    DEBUGASSERT(!isUsed[depth]);
  }
}

SplitPoint* SplitPointBuffer::acquireSplitPoint(int fDepth)
{
  DEBUGASSERT(fDepth <= maxFDepth);
  int idx = fDepth;
  DEBUGASSERT(!isUsed[idx]);

  SplitPoint* spt = &(spts[idx]);
  isUsed[idx] = true;
  numUsed++;

  DEBUGASSERT(!spt->isInitialized);
  return spt;
}

bool SplitPointBuffer::freeSplitPoint(SplitPoint* spt)
{
  //Ensure it's actually from this buffer!
  int idx = spt->bufferIdx;
  DEBUGASSERT(&spts[idx] == spt);
  DEBUGASSERT(!spt->isPublic);
  DEBUGASSERT(isUsed[idx]);

  isUsed[idx] = false;
  numUsed--;
  spt->isInitialized = false;
  return numUsed == 0;
}

//--------------------------------------------------------------------------------------------------

SearchTree::SearchTree(Searcher* s, int numThr, int maxMSearchCDepth, int maxCDepth,
    const Board& b, const BoardHistory& hist, const vector<UndoMoveData>& uData)
{
  if(numThr <= 0 || numThr > SearchParams::MAX_THREADS)
    Global::fatalError(string("Invalid number of threads: ") + Global::intToString(numThr));

  searcher = s;
  numThreads = numThr;
  searchDone = false;
  iterationGoing = false;

  threads = new SearchThread[numThreads];
  for(int i = 0; i<numThreads; i++)
    threads[i].initRoot(i,searcher,b,hist,uData,maxCDepth);

  //Overestimated worst case - each thread has an empty buffer, plus each other buffer in use is abandoned and
  //contains exactly one splitpoint
  initialNumFreeSptBufs = numThr + numThr*(maxMSearchCDepth+1);
  numFreeSptBufs = initialNumFreeSptBufs;
  freeSptBufs = new SplitPointBuffer*[numFreeSptBufs];
  //The max fdepth of splitpoint we need is <= maxMSearchCDepth since each fdepth must advance at least one msearch cdepth.
  int maxSplitPointFDepthNeeded = maxMSearchCDepth;
  for(int i = 0; i<numFreeSptBufs; i++)
    freeSptBufs[i] = new SplitPointBuffer(s,maxSplitPointFDepthNeeded);
  rootSptBuf = new SplitPointBuffer(s,0);

  rootNode = NULL;

  //Circularly doubly linked list
  publicizedListHead = new SplitPoint();
  publicizedListHead->publicNext = publicizedListHead;
  publicizedListHead->publicPrev = publicizedListHead;

  iterationNumWaiting = 0;

  //Create a bunch of threads and start them going
  boostThreads = new std::thread[numThreads];

#ifdef MULTITHREADING_STD
  for(int i = 1; i<numThreads; i++)
    boostThreads[i] = std::thread(&runChild,this,searcher,&threads[i]);
#else
  if(numThreads > 1)
    Global::fatalError("Not compiled with multithreading support!");
#endif
}

SearchTree::~SearchTree()
{
  mutex.lock();
  DEBUGASSERT(!iterationGoing);
  DEBUGASSERT(iterationNumWaiting == numThreads-1);
  DEBUGASSERT(initialNumFreeSptBufs == numFreeSptBufs);
  DEBUGASSERT(publicizedListHead->publicNext == publicizedListHead);
  DEBUGASSERT(publicizedListHead->publicPrev == publicizedListHead);
  DEBUGASSERT(rootNode == NULL);

  //Mark search done and open the way for threads to exit
  searchDone = true;
  iterationCondvar.notify_all();
  mutex.unlock();

  //Wait for them all to exit
  for(int i = 1; i<numThreads; i++)
    boostThreads[i].join();

  DEBUGASSERT(iterationNumWaiting == 0);

  //Clean up memory
  delete publicizedListHead;
  delete[] threads;
  delete[] boostThreads;

  for(int i = 0; i<numFreeSptBufs; i++)
    delete freeSptBufs[i];
  delete[] freeSptBufs;

  delete rootSptBuf;
}

SplitPoint* SearchTree::acquireRootNode()
{
  DEBUGASSERT(rootNode == NULL);
  int rootFDepth = 0;
  rootNode = rootSptBuf->acquireSplitPoint(rootFDepth);
  return rootNode;
}

void SearchTree::freeRootNode(SplitPoint* spt)
{
  depublicize(spt);
  DEBUGASSERT(rootNode == spt);
  rootSptBuf->freeSplitPoint(spt);
  rootNode = NULL;
}

//Start one iteration of iterative deepening. Call from master thread before starting.
void SearchTree::beginIteration()
{
  std::lock_guard<std::mutex> lock(mutex);
  iterationGoing = true;
  //Notify threads waiting in the holding zone to enter the main search loop
  iterationCondvar.notify_all();
}

//End one iteration of iterative deepening, call from inside the main loop when the first thread finds itself done
//with the root node.
void SearchTree::endIterationInternal()
{
  std::unique_lock<std::mutex> lock(mutex);
  iterationGoing = false;
  //Notify threads waiting for public work to exit search and move to the holding area
  publicWorkCondvar.notify_all();
}

//End one iteration of iterative deepening, waits for threads to exit the main loop. Call from master
//thread after an iteration, including after timeout
void SearchTree::endIteration()
{
  std::unique_lock<std::mutex> lock(mutex);
  DEBUGASSERT(!iterationGoing);

  //Wait for all threads to exit the main running loop
  while(iterationNumWaiting != numThreads-1)
    iterationMasterCondvar.wait(lock);

  DEBUGASSERT(initialNumFreeSptBufs == numFreeSptBufs);
  ONLYINDEBUG(
    for(int i = 0; i<numFreeSptBufs; i++)
      freeSptBufs[i]->assertEverythingUnused();
  );
}

//Get a new buffer of splitpoints for this thread to use during search.
SplitPointBuffer* SearchTree::acquireSplitPointBuffer()
{
  std::lock_guard<std::mutex> lock(mutex);
  DEBUGASSERT(numFreeSptBufs > 0);

  numFreeSptBufs--;
  SplitPointBuffer* buf = freeSptBufs[numFreeSptBufs];
  freeSptBufs[numFreeSptBufs] = NULL;

  return buf;
}

//Free an unused buffer of splitpoints that has been disowned, or one's own buffer
void SearchTree::freeSplitPointBuffer(SplitPointBuffer* buf)
{
  std::lock_guard<std::mutex> lock(mutex);
  freeSptBufs[numFreeSptBufs] = buf;
  numFreeSptBufs++;

  DEBUGASSERT(numFreeSptBufs <= initialNumFreeSptBufs);
}

//Publicize the SplitPoint so that other threads can help
//The SplitPoint must be initialized with board data before this is called!
void SearchTree::publicize(SplitPoint* spt)
{
  std::lock_guard<std::mutex> lock(mutex);

  DEBUGASSERT(spt->isInitialized);
  DEBUGASSERT(!spt->isPublic);
  DEBUGASSERT(spt->publicNext == NULL);
  DEBUGASSERT(spt->publicPrev == NULL);
  spt->isPublic = true;

  //Insert into doubly linked public list
  spt->publicNext = publicizedListHead->publicNext;
  spt->publicPrev = publicizedListHead;
  publicizedListHead->publicNext->publicPrev = spt;
  publicizedListHead->publicNext = spt;

  publicWorkCondvar.notify_all();
}

//Depublicize the SplitPoint. Call before this splitpoint back to buffer.
//Okay to call for splitpoints that aren't public
void SearchTree::depublicize(SplitPoint* spt)
{
  std::lock_guard<std::mutex> lock(mutex);
  if(spt->isPublic)
  {
    DEBUGASSERT(spt->publicNext != NULL);
    DEBUGASSERT(spt->publicPrev != NULL);

    //Remove from doubly linked list
    spt->publicNext->publicPrev = spt->publicPrev;
    spt->publicPrev->publicNext = spt->publicNext;
    spt->publicNext = NULL;
    spt->publicPrev = NULL;
    spt->isPublic = false;
  }
}

//Iterates through the public list trying to grab some work for the
//current thread. Private, assumes SearchTree is locked.
//Returns NULL if no public work found.
SplitPoint* SearchTree::actuallyLookForWork(SearchThread* curThread)
{
  SplitPoint* bestSpt = NULL;
  for(SplitPoint* spt = publicizedListHead->publicNext; spt != publicizedListHead; spt = spt->publicNext)
  {
    spt->lock(curThread);

    if(spt->probablyHasWork())
    {
      //Probably has work, sync and check again.
      //Can sync with it while unlocked because the whole tree is locked, so the spt couldn't be returned
      //while we're manipulating it
      spt->unlock(curThread);
      curThread->syncWithSplitPointDistant(spt);

      //Relock and try to actually get work
      spt->lock(curThread);
      if(spt->getWork(curThread))
      {
        //Woohoo, we got work!
        bestSpt = spt;
        spt->unlock(curThread);
        curThread->copyKillersForSplitPointDistant(spt);
        curThread->stats.publicWorkRequests++;
        curThread->stats.publicWorkDepthSum += spt->cDepth;
        break;
      }
    }
    spt->unlock(curThread);
  }
  return bestSpt;
}

//Find a good publicized SplitPoint and grab work from it for the given thread.
//If none, blocks until there is some. Returns the splitpoint UNLOCKED, with
//the thread having successfully grabbed work from it.
//Returns NULL is the search is done.
SplitPoint* SearchTree::getPublicWork(SearchThread* curThread)
{
  DEBUGASSERT(curThread->lockedSpt == NULL);
  std::unique_lock<std::mutex> lock(mutex);

  while(true)
  {
    //We're done, folks
    if(!iterationGoing)
      break;

    //Only bother looking for work if we're not terminated, if we're terminated we can't work any more.
    if(!curThread->isTerminated)
    {
      SplitPoint* bestSpt = actuallyLookForWork(curThread);
      //Found work!
      if(bestSpt != NULL)
        return bestSpt;
    }

    //Don't have to check if(!iterationGoing) because we've held the mutex the whole time and checked at the top
    publicWorkCondvar.wait(lock);
  }

  return NULL;
}

//Return the master thread
SearchThread* SearchTree::getMasterThread()
{
  return &threads[0];
}

//Get stats from all threads combined together
SearchStats SearchTree::getCombinedStats()
{
  SearchStats stats;
  for(int i = 0; i<numThreads; i++)
    stats += threads[i].stats;
  return stats;
}


//Holding bay for "child" threads, namely all threads in the searcher that aren't the master
void SearchTree::runChild(SearchTree* tree, Searcher* searcher, SearchThread* curThread)
{
  std::unique_lock<std::mutex> lock(tree->mutex);
  while(true)
  {
    if(tree->searchDone)
      break;
    else if(tree->iterationGoing)
    {
      lock.unlock();
      searcher->mainLoop(curThread,NULL);
      lock.lock();
    }
    else
    {
      tree->iterationNumWaiting++;

      //If all the threads are here waiting, notify the master that we're ready
      //All threads should be guaranteed to wait here at the start
      if(tree->iterationNumWaiting == tree->numThreads-1)
        tree->iterationMasterCondvar.notify_all();

      tree->iterationCondvar.wait(lock);
      tree->iterationNumWaiting--;
    }
  }
}

void SearchTree::copyKillersUnsynchronized(int fromThreadId, KillerEntry* killerMoves, int killerMovesLen)
{
  SearchThread* otherThread = &threads[fromThreadId];
  volatile KillerEntry* otherKillerMoves = (volatile KillerEntry*)(otherThread->killerMoves);

  int len = min(otherThread->killerMovesLen,killerMovesLen);
  for(int i = 0; i<len; i++)
  {
    for(int j = 0; j<KillerEntry::NUM_KILLERS; j++)
      killerMoves[i].killer[j] = otherKillerMoves[i].killer[j];
  }
}

//SPLITPOINT ---------------------------------------------------------------------

SplitPoint::SplitPoint()
{
  searcher = NULL;
  fDepth = 0;
  buffer = NULL;
  bufferIdx = 0;

  publicNext = NULL;
  publicPrev = NULL;
  isPublic = false;

  parent = NULL;
  isAborted = false;
  resultsAreInvalid = false;

  isInitialized = false;

  moveHistory = NULL;
  moveHistoryCapacity = 0;
  moveHistoryLen = 0;

  stepTurn = 0;

  parentMoveIndex = -1;
  parentMove = ERRMOVE;
  searchMode = MODE_INVAL;
  changedPlayer = false;

  isReSearch = false;
  origDepthAdjustment = 0;
  hasNullMove = false;

  cDepth = 0;
  rDepth4 = 0;
  hash = 0;

  nodeType = TYPE_INVAL;
  nodeAllness = 0;

  alpha.store(0,std::memory_order_relaxed);
  beta = 0;
  extraEval = 0;

  bestEval = Eval::LOSE-1;
  bestMove = ERRMOVE;
  bestFlag = Flags::FLAG_NONE;
  bestIndex = -1;

  int pvArraySize = SearchParams::PV_ARRAY_SIZE;
  pv = new move_t[pvArraySize];
  pvLen = 0;

  mv = NULL;
  hm = NULL;
  mvCapacity = 0;
  numMoves = 0;

  areMovesUngen = true;
  moveGenCallState = 0;
  moveGenWinDefGen = false;
  numMovesStarted = 0;
  numMovesStopped = 0;
  numMovesDone = 0;
  numMovesDoneLegal = 0;
  numSpecialMoves = 0;

  reduceFailyness = 0;
  banReductionsLeq = 0;

  hashMove = ERRMOVE;

  copyKillersFromThread = -1;

  pvsThreshold = 0x3FFFFFFF;
  r1Threshold = 0x3FFFFFFF;
  r2Threshold = 0x3FFFFFFF;
  r3Threshold = 0x3FFFFFFF;
  pruneThreshold = 0x3FFFFFFF;
}

SplitPoint::~SplitPoint()
{
  delete[] moveHistory;
  delete[] mv;
  delete[] hm;
  delete[] pv;
}

static bool containsMove(move_t* moves, int len, move_t move)
{
  for(int i = 0; i<len; i++)
    if(moves[i] == move)
      return true;
  return false;
}

void SplitPoint::init(SplitPoint* prnt, int pMoveIdx, move_t pMove, int sMode,
    int stepTrn, bool chgPlyr, bool isRS, int origDepthAdj,
    int cD, int rD4, hash_t h, eval_t al, eval_t bt, eval_t extra, bool isRoot,
    move_t hashMv, KillerEntry killers, bool allowNull,
    int moveArraySize, int copyKillersFrom)
{
  DEBUGASSERT(!isPublic);

  //Handle move history
  SearchUtils::ensureMoveHistoryArray(moveHistory,moveHistoryCapacity,fDepth+1);

  parent = prnt;
  isAborted = false;
  resultsAreInvalid = false;

  //Root node - no move history
  if(parent == NULL)
  {
    moveHistoryLen = 0;
    stepTurn = stepTrn;
  }
  else
  {
    //Copy over move history
    moveHistoryLen = parent->moveHistoryLen;
    for(int i = 0; i<moveHistoryLen; i++)
      moveHistory[i] = parent->moveHistory[i];

    //Append the move just made
    moveHistory[moveHistoryLen] = pMove;
    moveHistoryLen++;

    stepTurn = stepTrn;
    DEBUGASSERT(parent->stepTurn < stepTurn && parent->stepTurn + 4 >= stepTurn);
    DEBUGASSERT(!chgPlyr || stepTurn % 4 == 0);
    DEBUGASSERT(chgPlyr || parent->stepTurn + numStepsInMove(pMove) == stepTurn)
  }

  parentMoveIndex = pMoveIdx;
  parentMove = pMove;
  searchMode = sMode;
  changedPlayer = chgPlyr;

  isReSearch = isRS;
  origDepthAdjustment = origDepthAdj;

  bool shouldNull =
      allowNull &&
      rD4 >= SearchParams::NULLMOVE_MIN_RDEPTH * SearchParams::DEPTH_DIV &&
      chgPlyr &&
      !SearchUtils::isWinEval(bt);
  hasNullMove = shouldNull;

  cDepth = cD;
  rDepth4 = rD4;
  hash = h;

  alpha.store(al,std::memory_order_relaxed);
  beta = bt;
  extraEval = extra;

  //If we have nothing to do, we lose after the opp's next turn
  //Except for the root, where we'd like to actually record a pv, so we make bestEval worse than any possible eval.
  bestEval = parent == NULL ? Eval::LOSE - 1 : Eval::LOSE + cDepth + 8 - (stepTurn % 4);
  bestMove = ERRMOVE;
  bestFlag = Flags::FLAG_ALPHA;
  bestIndex = -1;

  pvLen = 0;

  SearchUtils::ensureMoveArray(mv,hm,mvCapacity,moveArraySize);
  hashMove = hashMv;
  int nMoves = 0;
  if(shouldNull)
  {
    mv[nMoves] = QPASSMOVE;
    hm[nMoves] = SearchParams::NULL_SCORE;
    nMoves++;
  }
  if(hashMv != ERRMOVE)
  {
    mv[nMoves] = hashMv;
    hm[nMoves] = SearchParams::HASH_SCORE;
    nMoves++;
  }
  //Note that killers should generally be unique, except in cases such as having two otherwise
  //identical killers except one has a qpass and the other has a pass and both being converted to the
  //same killer.
  for(int i = 0; i<KillerEntry::NUM_KILLERS; i++)
  {
    move_t killer = killers.killer[i];
    if(killer == ERRMOVE || containsMove(mv,nMoves,killer))
      continue;
    mv[nMoves] = killer;
    hm[nMoves] = SearchParams::KILLER_SCORE0 - i * SearchParams::KILLER_SCORE_DECREMENT;
    nMoves++;
  }

  numMoves = nMoves;
  numSpecialMoves = nMoves;

  areMovesUngen = true;
  moveGenCallState = 0;
  moveGenWinDefGen = false;
  numMovesStarted = 0;
  numMovesStopped = 0;
  numMovesDone = 0;
  numMovesDoneLegal = 0;

  reduceFailyness = 0;
  banReductionsLeq = 0;

  copyKillersFromThread = copyKillersFrom;

  pvsThreshold = 0x3FFFFFFF;
  r1Threshold = 0x3FFFFFFF;
  r2Threshold = 0x3FFFFFFF;
  r3Threshold = 0x3FFFFFFF;
  pruneThreshold = 0x3FFFFFFF;

  //Node type estimation
  initNodeType(isRoot);

  isInitialized = true;
}

void SplitPoint::initNodeType(bool isRoot)
{
  nodeAllness = 0; //TODO unused

  if(isRoot)
    nodeType = TYPE_ROOT;
  else if(parentMoveIndex == 0 && (parent->nodeType == TYPE_ROOT || parent->nodeType == TYPE_PV))
    nodeType = TYPE_PV;
  else if(parent->nodeType != TYPE_ALL)
    nodeType = TYPE_ALL;
  else
    nodeType = TYPE_CUT;
}

void SplitPoint::lock(SearchThread* thread)
{
  DEBUGASSERT(thread->lockedSpt == NULL);
  thread->lockedSpt = this;
  mutex.lock();
}

void SplitPoint::unlock(SearchThread* thread)
{
  mutex.unlock();
  DEBUGASSERT(thread->lockedSpt == this);
  thread->lockedSpt = NULL;
}

//Does this SplitPoint need to be publicized now for parallelization?
bool SplitPoint::needsPublication()
{
  //TODO parallelize when? After killer move? After areMovesUnGen is false?
  return !isPublic && numMovesDoneLegal >= 1 && (areMovesUngen || numMovesStarted < numMoves);
}

//Unsynchronizedly check if any parent nodes are aborted.
//Does NOT lock parents - safe because aborted can only toggle one way.
//Although now this function doesn't mutate anything, note that previously this function used to set
//aborted for intermediate parents up to the aborted parent without synchronizing. This was bad and
//potentially buggy - there might have been be a bad case where a thread comes in on the parent and locks it
//and takes an action knowing it's aborted, then unlocks it, then another thread comes in and locks it without
//seeing that it's aborted, because that change never became visible because it never happened in a synchronized context!
//Now, instead we maintain the property that isAborted, for mutation purposes, is synchronized under the lock
//like everything else.
bool SplitPoint::anyParentAbortedUnsynchronized()
{
  SplitPoint* spt = parent;
  while(spt != NULL && !spt->isAborted)
    spt = spt->parent;

  return spt != NULL;
}

//Checks if this splitpoint probably has work. Used by getPublicWork to check if a splitpoint might
//have work before going through the costly task of syncing with it to actually try to get work.
//Necessary because actually getting work might require generating moves, which requires the thread
//to be synced first.
//MINOR replace this function with one that actually gets work without generating moves if possible
//but we did try this and it didn't speed up, maybe we should try again some time.
bool SplitPoint::probablyHasWork()
{
  if(isAborted)
    return false;
  if(areMovesUngen)
    return true;
  return numMovesStarted < numMoves;
}

//Does this have work to be done still?
//IMPORATANT: Can only call this function if the current thread is synced with this splitpoint, because in the
//case that moves are ungen, will use the thread's own board to generate moves!
//May return an ERRMOVE, if there actually wasn't any work. This might rarely occur when areMovesUngen and the hash or killer
//are actually the only legal moves, or some such event like this. Returns true if actual work was obtained, else false.
//SearchThread is used purely for stats like killer table, etc.
bool SplitPoint::getWork(SearchThread* curThread)
{
  if(curThread->isTerminated || isAborted)
    return false;

  //Repeatedly try to get a move
  while(true)
  {
    int idx = numMovesStarted;
    DEBUGASSERT(idx >= 0);
    while(idx >= numMoves && areMovesUngen)
    {
      //Generate more moves
      DEBUGASSERT(curThread->board.sitCurrentHash == hash);
      curThread->searcher->genMSearchMoves(curThread,this);
      DEBUGASSERT(numMoves <= mvCapacity);
    }

    //Done, no more moves to get
    if(idx >= numMoves)
      return false;

    //Get the next move
    move_t move;
    if(nodeType == SplitPoint::TYPE_ROOT)
      move = curThread->searcher->getRootMove(mv,hm,numMoves,idx);
    else
      move = curThread->searcher->getMove(mv,hm,numMoves,idx);

    //Start this move
    numMovesStarted++;

    //If it's a repeat of one of the earlier special moves, immediately finish it
    //and try to get a different move instead
    if(idx >= numSpecialMoves)
    {
      bool duplicatesSpecialMove = false;
      for(int i = 0; i<numSpecialMoves; i++)
        if(move == mv[i])
        {duplicatesSpecialMove = true; break;}
      if(duplicatesSpecialMove)
      {
        numMovesStopped++;
        numMovesDone++;
        //FIXME added to preserve old behavior, figure out whether we care! Probably
        //it's conceptually much cleaner to make the thresholds check against number of legal moves, not
        //against straight move index
        pvsThreshold++;
        continue;
      }
    }

    //Otherwise, set it as the next to search and return
    curThread->curSplitPointMoveIdx = idx;
    curThread->curSplitPointMove = move;

    int banRedLeq = 0;
    //If we don't allow reductions at all...
    if(!curThread->searcher->params.allowReduce)
      banRedLeq = 3;
    //If we've reduced and failed too much, ban reductions until we go to a larger reduction that might not fail so much.
    else if(reduceFailyness >= SearchParams::REDUCEFAIL_THRESHOLD)
      banRedLeq = banReductionsLeq;

    //We do the calculation of how much to prunereduce here under the lock because
    //it can change dynamically now as more moves are generated.
    bool pruneReducing = true;
    int depthAdjustment = 0;
    if(banRedLeq < 3 && idx >= r3Threshold) {depthAdjustment = -3;}
    else if(banRedLeq < 2 && idx >= r2Threshold) {depthAdjustment = -2;}
    else if(banRedLeq < 1 && idx >= r1Threshold) {depthAdjustment = -1;}
    else if(idx >= pvsThreshold) {}
    else {pruneReducing = false;}

    curThread->curPruneReduceDesired = pruneReducing;
    curThread->curDepthAdjDesired = depthAdjustment;

    return true;
  }
}


//Is this SplitPoint done for this thread?
//In the case that the thread is terminated, or that the splitpoint is aborted, will exit early
//if this thread is the only one left.
//Note that on abort or terminated, we still have to wait for numMovesStarted == numMovesStopped.
//Because we need to make sure all the threads notice the abort, before we recycle this SplitPoint.
bool SplitPoint::isDone(const SearchThread* curThread)
{
  return numMovesStarted == numMovesStopped && (numMovesStopped == numMoves || curThread->isTerminated || isAborted);
}

//Report a result to the given splitpoint
//NOTE: The PV at [fdepth+1] of curThread must be the pv for the move (or else be of zero length)
//When returning up to the parent, this is copied back to the thread in msearchDone.
//resultInvalid - should be true if the result reported was invalid due to an abort or termination
//reSearched - should be true if the spt just finished was the result of a reSearch of a prunereduced node
//origDepthAdj - the depth adjustment due to reductions or extensions, if any, on the initial search of this node.
void SplitPoint::reportResult(eval_t eval, int moveIdx, move_t move, bool resultInvalid,
    bool reSearched, int origDepthAdj, SearchThread* curThread)
{
  DEBUGASSERT(move != ERRMOVE);

  numMovesStopped++;
  DEBUGASSERT(numMovesStopped <= numMoves);

  //Aborted node - nothing to do here
  if(isAborted)
    return;

  //We timed out while searching something from this node
  //Or an invalid result was reported to this node.
  //Or a parent was aborted.
  //Either way, it makes this node's own results invalid so we should abort and return.
  if(curThread->isTerminated || resultInvalid || anyParentAbortedUnsynchronized())
  {
    isAborted = true;
    resultsAreInvalid = true;
    return;
  }

  numMovesDone++;
  DEBUGASSERT(numMovesDone <= numMoves);

  bool wasNullMove = hasNullMove && moveIdx == 0;
  DEBUGASSERT(!(wasNullMove && isPublic));

  //Don't count illegal moves or moves that are pruned or lose instantly in the threshold used for deciding
  //when to publicize
  //FIXME make this more robust? For example, should we be using origDepthAdj != DEPTH_ADJ_PRUNE now that we have it?
  if(eval > Eval::LOSE && !wasNullMove)
    numMovesDoneLegal++;

  if(searcher->params.viewing(curThread->board,curThread->boardHistory))
    searcher->params.viewMove(curThread->board, move, hm[moveIdx], eval, bestEval, reSearched, origDepthAdj,
        curThread->getPV(fDepth+1),curThread->getPVLen(fDepth+1),curThread->stats);

  //Beta cutoff
  if(eval >= beta)
  {
    bestMove = move;
    bestEval = wasNullMove ? beta : eval;
    bestFlag = Flags::FLAG_BETA;
    bestIndex = moveIdx;

    isAborted = true;
    curThread->stats.betaCuts++;

    //If there are any other threads working on this node, well their work was wasted.
    if(numMovesStarted != numMovesStopped)
    {
      curThread->stats.threadAborts++;
      //Every branch running besides this one was wasted.
      //Note that this isn't exactly correct in that maybe another branch came before this one in the ordering
      //and *would* have led to a beta cutoff - sometimes parallelizing at a beta node *can* speed up things.
      //But we go ahead and pessimistically count this as a bad result.
      curThread->stats.abortedBranches += numMovesStarted - numMovesStopped;
    }

    //childPv might be null here and in other copyExtendPVs, but only when childPvLen is also 0, so it's okay
    curThread->copyExtendPVFromNextFDepthTo(move,fDepth,pv,pvLen);
    return;
  }

  //If the null move didn't get a beta cutoff, then we have nothing else to do
  if(wasNullMove)
    return;

  //Record evals at root so that we get better root move ordering
  int alphaValue = alpha.load(std::memory_order_relaxed);
  if(nodeType == TYPE_ROOT)
  {
    //Record the value if it's good enough
    if(eval > alphaValue)
      hm[moveIdx] = eval;
    //Otherwise the value from the previous iteration should already be wiped out
    //so we don't need to do anything

    //TODO after introducing prunereduction within the tree, test this there
    //Avoid repeatedly prunereducing and failing high and then researching and failing low

    //Fail low on a research?
    if(reSearched && eval <= bestEval)
    {
      DEBUGASSERT(origDepthAdj != SearchParams::DEPTH_ADJ_PRUNE);
      reduceFailyness += SearchParams::ROOT_REDUCEFAIL_PER_FAIL;
      if(-origDepthAdj > banReductionsLeq)
        banReductionsLeq = -origDepthAdj;
    }
    else if(reduceFailyness > 0)
    {
      //If alpha was improved, then we can allow a little leeway in trying to reduce again
      if(eval > alphaValue && reduceFailyness > SearchParams::ROOT_CAP_REDUCEFAIL_WHEN_ALPHA_IMPROVES)
        reduceFailyness = SearchParams::ROOT_CAP_REDUCEFAIL_WHEN_ALPHA_IMPROVES;
      //If we had a successful reduction, then reduce the failyness
      else if(origDepthAdj < 0 && origDepthAdj != SearchParams::DEPTH_ADJ_PRUNE && !reSearched)
        reduceFailyness -= SearchParams::ROOT_REDUCEFAIL_PER_SUCCESS;
    }
  }

  //Update best and alpha
  if(eval > bestEval)
  {
    bestMove = move;
    bestEval = eval;
    bestIndex = moveIdx;

    if(eval > alphaValue)
    {
      alpha.store(eval,std::memory_order_relaxed);
      bestFlag = Flags::FLAG_EXACT;
    }

    curThread->copyExtendPVFromNextFDepthTo(move,fDepth,pv,pvLen);
  }

  if(nodeType == TYPE_ROOT)
  {
    searcher->updateDesiredTime(bestEval,numMovesDone,numMoves);

    if(bestIndex == moveIdx && searcher->doOutput)
    {
      double floatdepth = rDepth4/SearchParams::DEPTH_DIV - 1 + (double)moveIdx/numMoves;
      double timeUsed = searcher->currentTimeUsed();
      double desiredTime = searcher->currentDesiredTime();
      if(desiredTime == SearchParams::AUTO_TIME) desiredTime = 0;
      string evalString = ArimaaIO::writeEval(eval);
      (*(searcher->params.output)) << Global::strprintf("FS Depth: +%-6.4g Eval: %6s Time: %.2f/%.2f",
          floatdepth, evalString.c_str(), timeUsed, desiredTime)
           << "  PV: " << Board::writeMoves(curThread->board,pv,pvLen) << endl;
    }
  }
}


//SEARCHTHREAD --------------------------------------------------------

SearchThread::SearchThread()
{
  id = -1;
  searcher = NULL;

  pv = new move_t*[SearchParams::PV_ARRAY_SIZE];
  for(int i = 0; i<SearchParams::PV_ARRAY_SIZE; i++)
    pv[i] = new move_t[SearchParams::PV_ARRAY_SIZE];
  pvLen = new int[SearchParams::PV_ARRAY_SIZE];

  curSplitPointMoveIdx = -1;
  curSplitPointMove = ERRMOVE;
  curPruneReduceDesired = false;
  curDepthAdjDesired = 0;
  isTerminated = false;
  curSplitPointBuffer = NULL;
  timeCheckCounter = 0;
  lockedSpt = NULL;

  killerMoves = NULL;
  killerMovesLen = 0;

  mvListCapacity = SearchParams::QMAX_FDEPTH * SearchParams::QSEARCH_MOVE_CAPACITY;
  mvList = new move_t[mvListCapacity];
  hmList = new int[mvListCapacity];
  mvListCapacityUsed = 0;
}

SearchThread::~SearchThread()
{
  DEBUGASSERT(lockedSpt == NULL);

  delete[] mvList;
  delete[] hmList;

  delete[] killerMoves;

  for(size_t i = 0; i < unsafePruneIds.size(); i++)
    delete unsafePruneIds[i];

  for(int i = 0; i<SearchParams::PV_ARRAY_SIZE; i++)
    delete[] pv[i];
  delete[] pv;
  delete[] pvLen;
}

//Initialize this search thread to be ready to search for the given root position
void SearchThread::initRoot(int i, Searcher* s, const Board& b, const BoardHistory& hist,
    const vector<UndoMoveData>& uData, int maxCDepth)
{
  id = i;
  searcher = s;
  board = b;
  boardHistory = hist;
  undoData = uData;
  isTerminated = false;
  timeCheckCounter = 0;

  int killerLen = maxCDepth+1;
  killerMoves = new KillerEntry[killerLen];
  for(int j = 0; j<killerLen; j++)
    killerMoves[j] = KillerEntry();
  killerMovesLen = killerLen;
}

//Copy killer moves from the appropriate thread for this splitpoint, when we are joining in
//and trying to sync with it.
//Splitpoint need not be locked, however this thread should have work allocated from it.
void SearchThread::copyKillersForSplitPointDistant(const SplitPoint* spt)
{
  int killerMovesThreadId = spt->copyKillersFromThread;
  if(killerMovesThreadId != id)
  {
    if(killerMovesThreadId >= 0)
    {
      searcher->searchTree->copyKillersUnsynchronized(killerMovesThreadId,killerMoves,killerMovesLen);

      //Validate and clean up the copied killers
      for(int i = 0; i<killerMovesLen; i++)
      {
        KillerEntry& k = killerMoves[i];
        for(int i = 0; i<KillerEntry::NUM_KILLERS; i++)
          k.killer[i] = SearchUtils::removeRedundantSteps(k.killer[i]);

        //Remove duplicates
        int numGoodKillers = 0;
        for(int i = 0; i<KillerEntry::NUM_KILLERS; i++)
        {
          if(k.killer[i] == ERRMOVE)
            continue;
          bool isDup = false;
          for(int j = 0; j<numGoodKillers; j++)
          {
            if(k.killer[i] == k.killer[j])
            {isDup = true; break;}
          }
          if(isDup)
            continue;
          k.killer[numGoodKillers++] = k.killer[i];
        }
      }
    }
    else
    {
      for(int i = 0; i<killerMovesLen; i++)
        killerMoves[i] = KillerEntry();
    }
  }
}

//Called whenever we begin searching at a new splitpoint that was NOT a continuation of any
//existing splitpoint. That is, called when we begin searching at the root, as well as when
//we jump to a different splitpoint in the tree and join in.
//IMPORTANT: No need for splitpoint to be locked, but if so, then it must be the case that
//the search tree is locked the whole time, so that this splitpoint won't unexpectedly get
//depublicized and returned at the same time as this function runs!
void SearchThread::syncWithSplitPointDistant(const SplitPoint* spt)
{
  //Make moves forward from the start until we're at the splitpoint.
  board = searcher->mainBoard;
  int numSearchTurns = spt->moveHistoryLen;
  undoData = searcher->mainUndoData;

  //Every node has the property that its rdepth4 is less than or equal to the rdepth4 at the root node.
  //Except we need to take into account extensions! Partial-turn search plus extensions can cause this not to be true
  //So actually, we use max cdepth as a bound - the rdepth for a node is always less than the max possible cdepth for the whole search
  //(otherwise some branch, assuming no reductions, would have to end up going deeper).
  //Obviously this is massively conservative.
  int rDepth4UpperBound =
      (searcher->currentIterDepth * SearchParams::MAX_MSEARCH_CDEPTH_FACTOR4_OVER_NOMINAL / 4
       + SearchParams::MAX_MSEARCH_CDEPTH_OVER_NOMINAL) * SearchParams::DEPTH_DIV;

  int cDepth = 0;
  int finalCDepth = spt->cDepth;

  searcher->maybeComputePosData(board,boardHistory,rDepth4UpperBound,this);
  for(int i = 0; i<numSearchTurns; i++)
  {
    move_t move = spt->moveHistory[i];
    int oldBoardStep = board.step;
    bool suc = board.makeMoveLegal(move,undoData);
    DEBUGASSERT(suc);
    boardHistory.reportMove(board,move,oldBoardStep);

    //Figure out was cDepth should be as we walk down the tree and use it to figure out
    //whether we need to compute treeFeature posDatas or not.
    //We also use a super-conservative bound on rDepth4 by using the root rDepth4. This is an overestimate
    //of what the real rDepth4 was at that point in the search. However it should not be an underestimate, and
    //which means we might compute it when not needed, but never fail to when it is needed.
    bool changedPlayer = (board.step == 0);
    int numSteps = changedPlayer ? 4-oldBoardStep : board.step-oldBoardStep;
    cDepth += numSteps;
    if(changedPlayer)
      searcher->maybeComputePosData(board,boardHistory,rDepth4UpperBound,this);
  }
  DEBUGASSERT(cDepth == finalCDepth);
}


//Record any pv from the fdepth below us into the current one
void SearchThread::copyExtendPVFromNextFDepth(move_t bestMove, int fDepth)
{
  if(fDepth < SearchParams::PV_ARRAY_SIZE - 1)
    SearchUtils::copyExtendPV(bestMove,pv[fDepth+1],pvLen[fDepth+1],pv[fDepth],pvLen[fDepth]);
  else if(fDepth == SearchParams::PV_ARRAY_SIZE - 1)
  {
    pv[fDepth][0] = bestMove;
    pvLen[fDepth] = 1;
  }
}
void SearchThread::copyExtendPVFromNextFDepthTo(move_t bestMove, int fDepth, move_t* targetpv, int& targetlen)
{
  if(fDepth < SearchParams::PV_ARRAY_SIZE - 1)
    SearchUtils::copyExtendPV(bestMove,pv[fDepth+1],pvLen[fDepth+1],targetpv,targetlen);
  else if(fDepth == SearchParams::PV_ARRAY_SIZE - 1)
  {
    targetpv[0] = bestMove;
    targetlen = 1;
  }
}

void SearchThread::endPV(int fDepth)
{
  if(fDepth >= SearchParams::PV_ARRAY_SIZE)
    return;
  SearchUtils::endPV(pvLen[fDepth]);
}

move_t* SearchThread::getPV(int fDepth)
{
  if(fDepth >= SearchParams::PV_ARRAY_SIZE)
    return NULL;
  return pv[fDepth];
}

int SearchThread::getPVLen(int fDepth)
{
  if(fDepth >= SearchParams::PV_ARRAY_SIZE)
    return 0;
  return pvLen[fDepth];
}


//MAIN LOGIC---------------------------------------------------------------------------

//Begin a parallel search on a root split point. Call this after retrieving the split point from the search tree
//and calling init on it
void Searcher::parallelFSearch(SearchThread* curThread, SplitPoint* spt)
{
  DEBUGASSERT(spt->nodeType == SplitPoint::TYPE_ROOT);
  DEBUGASSERT(searchTree != NULL);
  DEBUGASSERT(curThread == searchTree->getMasterThread());

  //Go!
  searchTree->beginIteration();
  curThread->syncWithSplitPointDistant(spt);
  spt->lock(curThread);
  mainLoop(curThread,spt);
  searchTree->endIteration();
}

//Master thread: calls this function with spt = root node and LOCKED
//Other threads: calls this function with spt = NULL
//Postcondition for master thread: root node is UNLOCKED
void Searcher::mainLoop(SearchThread* curThread, SplitPoint* spt)
{
  //Get split point buffer
  curThread->curSplitPointBuffer = searchTree->acquireSplitPointBuffer();

  //At the beginning of each loop, spt, if not NULL, is a LOCKED spt that you want to try
  //to get more work from, and otherwise finish up if not.
  //Whenever a splitpoint is returned by a function to the main loop, we maintain the invariant
  //that curThread is synced with it.
  while(true)
  {
    //Do we have a locked splitpoint to try to get work from?
    if(spt == NULL)
    {
      //No - so look for some.
      spt = searchTree->getPublicWork(curThread);
      //If getPublicWork returns without a splitpoint, we know the search is over
      if(spt == NULL)
        break;
      //Otherwise we continue past the else to try to do the work.
      //Note that getPublicWork returns with its working splitpoint UNLOCKED.
      //This is fine because we have reserved a working move from it so it won't get
      //cleaned up out from under us.
    }
    //We do have a locked splitpoint to try to get work from
    else
    {
      //Try to get work from it
      bool hasWork = spt->getWork(curThread);
      //No work from spt
      if(!hasWork)
      {
        //If we're not the last thread out, then we need to leave and seek public work
        if(!spt->isDone(curThread))
        {
          //Drop the splitpoint and loop again for public work
          abandonSpt(curThread,spt);
          spt = NULL;
          continue;
        }

        //Otherwise, we're the last thread, so finish the splitpoint
        spt = finishSpt(curThread,spt);

        //If finishSpt only returns without a new locked node, it was the root node and we're done.
        if(spt == NULL)
        {
          searchTree->endIterationInternal();
          break;
        }

        //Otherwise we have a locked splitpoint to keep working on,
        //either a re-search of the same splitpoint due to reductions,
        //or its parent, which we reported a value to. Loop again to try to get a move.
        continue;
      }
      //Otherwise we do have work.
      //Unlock, fall out of this branch, publicize the splitpoint if necessary, and keep going.
      bool needsPublication = spt->needsPublication();
      spt->unlock(curThread);
      if(needsPublication)
        searchTree->publicize(spt);
    }

    //At this point, we have an unlocked splitpoint that we've grabbed work from.
    //Do the work and generate a new locked splitpoint for us to try to grab more work from.
    //Either the same splitpoint, or its parent.
    spt = doWork(curThread,spt);
    continue;
  }

  //Free split point buffer, give back to tree. Note that this might not be
  //the same splitpoint buffer given to the thread at the top of this function.
  searchTree->freeSplitPointBuffer(curThread->curSplitPointBuffer);
  curThread->curSplitPointBuffer = NULL;
}

//Work on the move grabbed from this splitpoint
//Preconditions: spt is UNLOCKED, but this thread HAS WORK from it.
//Returns: the next splitpoint to try to work on
//Postconditions: the returned splitpoint is NOT NULL and LOCKED
SplitPoint* Searcher::doWork(SearchThread* curThread, SplitPoint* spt)
{
  int moveIdx = curThread->curSplitPointMoveIdx;
  move_t move = curThread->curSplitPointMove;

  SplitPoint* newSpt;
  eval_t result;
  bool isReSearch;
  int origDepthAdj;
  mSearch(curThread,spt,newSpt,result,isReSearch,origDepthAdj);

  //No result yet, generated a new SplitPoint to work on
  //Also mSearch made the move so we're synced with it
  if(newSpt != NULL)
  {
    //Lock and return the new splitpoint for work
    newSpt->lock(curThread);
    return newSpt;
  }

  //Generated a result rather than a new splitpoint. Report it and continue.
  //Note that we always claim the result is valid (passing in false for resultInvalid)
  //This is okay, since if we timed out then curThread->isTerminated will be true and
  //so we won't record the result anyways.
  spt->lock(curThread);
  spt->reportResult(result,moveIdx,move,false,isReSearch,origDepthAdj,curThread);
  return spt;
}

//Abandon a splitpoint and before going off in search of public work
//Preconditions: spt is NOT NULL and LOCKED, has NO WORK, but is NOT DONE
//Postconditions: spt is UNLOCKED, all necessary resources are cleaned up so spt can be dropped.
void Searcher::abandonSpt(SearchThread* curThread, SplitPoint* spt)
{
  //Is this SplitPoint from our buffer?
  bool fromOwnBuffer = (spt->buffer == curThread->curSplitPointBuffer);
  spt->unlock(curThread);

  //If so, disown it, someone else will clean it up when this SplitPoint is done.
  //We can outright drop the pointer because others can access it through the splitpoint storing
  //a reference to its own buffer
  if(fromOwnBuffer)
    curThread->curSplitPointBuffer = searchTree->acquireSplitPointBuffer(); //Disown old buffer, get new one
}

//Finish a splitpoint after it's done
//Preconditions: spt is NOT NULL and LOCKED, has NO WORK, and IS DONE
//Returns: NULL if spt was the root node, else the next splitpoint to try to work on
//Postconditions: spt splitpoint is UNLOCKED, the returned splitpoint (possibly the same) is (re)LOCKED
SplitPoint* Searcher::finishSpt(SearchThread* curThread, SplitPoint* spt)
{
  //Thie splitpoint is done, so we can actually unlock it now.
  //And we need to, in order to depublicize it
  //As a guard, we set it to be aborted to make it absolutely clear that there's no more work here.
  //(if this thread has timed out but others haven't yet, threads may disagree about whether there
  //is work or not available).
  spt->isAborted = true;
  spt->unlock(curThread);

  //Depublicize the splitpoint if necessary
  //Note that it's safe to check this value without locking the search tree because
  //already the splitpoint has no work, so nobody else should be messing with it.
  if(spt->isPublic)
    searchTree->depublicize(spt);
  //Now we're absolutely the only ones to have any reference to this splitpoint.

  //If it's the root, then we're all the way done
  if(spt->parent == NULL)
  {
    DEBUGASSERT(spt->nodeType == SplitPoint::TYPE_ROOT);
    return NULL;
  }

  //Check back with the search functions to see if we need to keep working
  //(due to LMR, etc) or if we get a result for the parent.
  //Note that mSearchDone will do an undo for us if no new splitpoint, which is what
  //we want to keep us synced when we step to the parent
  eval_t result;
  bool isActuallyDone = mSearchDone(curThread, spt, result);
  if(!isActuallyDone)
  {
    //Relock and return the splitpoint for more work
    spt->lock(curThread);
    return spt;
  }

  //Otherwise we really are done. Free the splitpoint now and report the result to the parent.
  SplitPoint* parent = spt->parent;
  int moveIdx = spt->parentMoveIndex;
  move_t move = spt->parentMove;
  bool resultInvalid = spt->resultsAreInvalid;
  bool reSearched = spt->isReSearch;
  int origDepthAdj = spt->origDepthAdjustment;

  //We can now free the current spt, it's done
  freeSplitPoint(curThread,spt);

  //Report to the parent and return the parent for more work
  parent->lock(curThread);
  parent->reportResult(result,moveIdx,move,resultInvalid,reSearched,origDepthAdj,curThread);
  return parent;
}

//Free SplitPoint and handle buffer return.
//NOTE: DO NOT call with a splitpoint locked, because it might call in to the
//search tree, and we need to maintain lock acquisition order
void Searcher::freeSplitPoint(SearchThread* curThread, SplitPoint* spt)
{
  //SplitPoint is from our buffer
  SplitPointBuffer* buf = spt->buffer;
  if(buf == curThread->curSplitPointBuffer)
  {
    //Free it and do nothing else
    buf->freeSplitPoint(spt);
  }
  //SplitPoint is not from our buffer
  //Presumably this buffer has been disowned then
  else
  {
    bool isEmpty = buf->freeSplitPoint(spt);
    //Return empty buffers to SearchTree
    if(isEmpty)
      searchTree->freeSplitPointBuffer(buf);
  }
}


SimpleLock::SimpleLock()
:mutex()
{}
SimpleLock::~SimpleLock()
{}

void SimpleLock::lock()
{
  mutex.lock();
}
void SimpleLock::unlock()
{
  mutex.unlock();
}



