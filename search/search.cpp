
/*
 * search.cpp
 * Author: davidwu
 */

#include <cmath>
#include <algorithm>
#include <ctime>
#include "../core/global.h"
#include "../core/boostthread.h"
#include "../core/timer.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../eval/eval.h"
#include "../learning/featuremove.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../search/searchutils.h"
#include "../search/searchmovegen.h"
#include "../search/searchthread.h"
#include "../search/timecontrol.h"
#include "../search/searchflags.h"
#include "../program/arimaaio.h"
#include "../program/init.h"

using namespace std;


//SEARCHER INITIALIZATION-----------------------------------------------------------------

Searcher::Searcher()
{
  init();
}

Searcher::Searcher(SearchParams p)
{
  params = p;
  init();
}

void Searcher::init()
{
  doOutput = false;
  mainPla = NPLA;
  fullMoveHash = new ExistsHashTable(params.fullMoveHashExp);

  int pvArraySize = SearchParams::PV_ARRAY_SIZE;
  idpv = new move_t[pvArraySize];
  idpvLen = 0;

  mainHash = new SearchHashTable(params.mainHashExp);

  searchTree = NULL;

  timeLock = new SimpleLock();
  searchId = 0;
  interruptedId = -1;
  didInterruptOrTimeout = false;
  okayToTimeout = false;

  clockDesiredTime = 0;
  clockBaseEval = Eval::LOSE;
  clockBestEval = Eval::LOSE;
  clockNumMovesDone = 0;
  clockNumMovesTotal = 0;
  currentIterDepth = 0;

  timeControl = TimeControl();
  timeControl.getMinNormalMax(tcMinTime,tcNormalTime,tcMaxTime);
  hardTimeCap = SearchParams::AUTO_TIME;
}

Searcher::~Searcher()
{
  delete fullMoveHash;
  delete[] idpv;
  delete mainHash;

  for(int i = 0; i<(int)historyTable.size(); i++)
    delete[] historyTable[i];

  delete searchTree;
  delete timeLock;
}


void Searcher::resizeHashIfNeeded()
{
  if(fullMoveHash->exponent != params.fullMoveHashExp)
  {
    delete fullMoveHash;
    fullMoveHash = new ExistsHashTable(params.fullMoveHashExp);
  }
  if(mainHash->exponent != params.mainHashExp)
  {
    delete mainHash;
    mainHash = new SearchHashTable(params.mainHashExp);
  }
}

//ID AND TOP LEVEL SEARCH-----------------------------------------------------------------

vector<move_t> Searcher::getIDPV()
{
  vector<move_t> moves;
  for(int i = 0; i<idpvLen; i++)
    moves.push_back(idpv[i]);
  return moves;
}

vector<move_t> Searcher::getIDPVFullMoves()
{
  vector<move_t> moves;
  Board copy = mainBoard;
  move_t moveSoFar = ERRMOVE;
  int ns = 0;
  pla_t pla = copy.player;
  for(int i = 0; i<idpvLen; i++)
  {
    bool suc = copy.makeMoveLegalNoUndo(idpv[i]);
    DEBUGASSERT(suc);

    moveSoFar = concatMoves(moveSoFar,idpv[i],ns);
    ns = numStepsInMove(moveSoFar);
    if(copy.player != pla)
    {
      moves.push_back(moveSoFar);
      moveSoFar = ERRMOVE;
      ns = 0;
      pla = copy.player;
    }
  }

  if(ns != 0)
    moves.push_back(moveSoFar);

  return moves;
}

//Get the best move
move_t Searcher::getMove()
{
  step_t steps[4];
  int numSteps = 0;
  for(int i = 0; i<idpvLen; i++)
  {
    bool done = false;
    move_t move = idpv[i];
    for(int j = 0; j<4; j++)
    {
      step_t step = getStep(move,j);
      if(step == ERRSTEP)
      {break;}
      if(step == PASSSTEP || step == QPASSSTEP)
      {
        steps[numSteps++] = PASSSTEP;
        done = true;
        break;
      }

      steps[numSteps++] = step;
      if(numSteps + mainBoard.step >= 4)
      {done = true; break;}
    }
    if(done)
      break;
  }

  switch(numSteps)
  {
  case 0: return ERRMOVE;
  case 1: return gMove(steps[0]);
  case 2: return gMove(steps[0],steps[1]);
  case 3: return gMove(steps[0],steps[1],steps[2]);
  case 4: return gMove(steps[0],steps[1],steps[2],steps[3]);
  default: return ERRMOVE;
  }
}

void Searcher::getSortedRootMoves(vector<move_t>& buf)
{
  buf.clear();
  size_t size = fullMoves.size();
  for(size_t i = 0; i<size; i++)
    buf.push_back(fullMoves[i].move);
}

int Searcher::getNumRootMoves()
{
  return (int)fullMoves.size();
}

void Searcher::searchID(const Board& b, const BoardHistory& hist, bool output)
{
  searchID(b,hist,params.defaultMaxDepth,params.defaultMaxTime,output);
}

void Searcher::setTimeControl(const TimeControl& tc)
{
  timeLock->lock();
  timeControl = tc;
  timeControl.getMinNormalMax(tcMinTime,tcNormalTime,tcMaxTime);
  refreshDesiredTimeNoLock();
  timeLock->unlock();
}

void Searcher::setSearchId(int s)
{
  if(s < 0)
    Global::fatalError("Searcher::setSearchId, searchId < 0 : " + Global::intToString(s));
  timeLock->lock();
  searchId = s;
  timeLock->unlock();
}
void Searcher::interruptExternal(int s)
{
  if(s < 0)
    Global::fatalError("Searcher::interruptExternal,searchId < 0 : " + Global::intToString(s));
  timeLock->lock();
  interruptedId = s;
  timeLock->unlock();
}

//Iterative deepening search-------------------------------------------------------------
void Searcher::searchID(const Board& b, const BoardHistory& hist, int maxDepth, double seconds, bool output)
{
  //Ensure the board is well-formed
  if(!b.testConsistency(cout))
    Global::fatalError("searchID given inconsistent board!");

  //Bound parameters
  if(seconds < 0) seconds = 0;
  if(seconds > SearchParams::AUTO_TIME) seconds = SearchParams::AUTO_TIME;
  if(maxDepth < -SearchParams::QDEPTH_EVAL) maxDepth = SearchParams::QDEPTH_EVAL;
  if(maxDepth > SearchParams::AUTO_DEPTH) maxDepth = SearchParams::AUTO_DEPTH;

  bool briefLastDepth = false;
  if(maxDepth <= 8 && !Init::ARIMAA_DEV)
  {
    maxDepth++;
    briefLastDepth = true;
  }

  //Compute desired depth
  int nsInTurn =  4-b.step;

  //Initialize stuff
  doOutput = output;
  mainPla = b.player;
  mainBoard = b;
  mainBoardHistory = hist;
  mainUndoData = vector<UndoMoveData>(); //TODO rewrite maybe can eliminate boardhistory in place of this, with this containing the history from the start of the game...?
  stats = SearchStats();
  fullMoves.clear(); //If we end up exiting before we generate root moves

  SearchUtils::endPV(idpvLen);

  int startDepth = min(maxDepth,nsInTurn);
  int maxMSearchCDepth =
      max(maxDepth,0) * SearchParams::MAX_MSEARCH_CDEPTH_FACTOR4_OVER_NOMINAL / 4
      + SearchParams::MAX_MSEARCH_CDEPTH_OVER_NOMINAL;
  int maxCDepth = maxMSearchCDepth + SearchParams::QMAX_CDEPTH;

  //If we got interrupted prior to even entering this function, the searchIds will still match
  //and we will discover it normally during a timeout/interrupt check during search.
  didInterruptOrTimeout = false;
  okayToTimeout = false;
  clockTimer.reset();
  clockBaseEval = Eval::LOSE;
  hardTimeCap = seconds;
  currentIterDepth = 0; //Temporarily, for the updateDesiredTime
  updateDesiredTime(Eval::LOSE,0,0);
  currentIterDepth = startDepth;

  mainHash->clear();
  SearchUtils::ensureHistory(historyTable,historyMax,maxMSearchCDepth);
  SearchUtils::clearHistory(historyTable,historyMax);

  //Check if the game is over. If so, we're done!
  eval_t gameEndVal = SearchUtils::checkGameEndConditions(b,hist,0);
  if(gameEndVal != 0)
  {
    stats.timeTaken = clockTimer.getSeconds();
    stats.finalEval = gameEndVal;
    return;
  }

  //Normal main search root move gen
  if(maxDepth > 0)
  {
    //Generate moves
    vector<move_t> mvVec;
    SearchMoveGen::genFullMoves(b, mainBoardHistory, mvVec, startDepth, true, params.rootMaxNonRelTactics,
        fullMoveHash, &params.excludeRootMoves, &params.excludeRootStepSets);

    if(params.randomize)
      SearchUtils::shuffle(params.randSeed,mvVec);

    mainCheckTime();
    if(didInterruptOrTimeout)
    {
      stats.timeTaken = clockTimer.getSeconds();
      return;
    }

    //BT MOVE ORDERING----------------
    if(params.rootMoveFeatureSet.fset != NULL && !params.stupidPrune && maxDepth >= 4)
    {
      DEBUGASSERT(mainBoardHistory.maxTurnBoardNumber == mainBoardHistory.maxTurnNumber);
      Board copy = b;
      void* moveFeatureData = params.rootMoveFeatureSet.getPosData(copy,mainBoardHistory,mainPla);
      vector<double> featureWeights;
      params.rootMoveFeatureSet.getFeatureWeights(moveFeatureData,params.rootMoveFeatureWeights,featureWeights);

      //Sort moves!
      size_t size = mvVec.size();
      UndoMoveData uData;
      fullMoves.resize(size);
      for(size_t m = 0; m < size; m++)
      {
        move_t move = mvVec[m];
        copy.makeMove(move,uData);
        fullMoves[m].move = move;
        fullMoves[m].val = params.rootMoveFeatureSet.getFeatureSum(
            copy,moveFeatureData,mainPla,move,mainBoardHistory,featureWeights);
        copy.undoMove(uData);
      }
      params.rootMoveFeatureSet.freePosData(moveFeatureData);

      std::stable_sort(fullMoves.begin(),fullMoves.end());
    }
    else
    {
      size_t size = mvVec.size();
      fullMoves.resize(size);
      for(size_t m = 0; m < size; m++)
      {
        move_t move = mvVec[m];
        fullMoves[m].move = move;
        fullMoves[m].val = 0;
      }
    }

    mainCheckTime();
    if(didInterruptOrTimeout)
    {
      stats.timeTaken = clockTimer.getSeconds();
      return;
    }
  }

  //Initialize search tree, which also spawns off threads ready to help when the search starts
  DEBUGASSERT(searchTree == NULL);
  searchTree = new SearchTree(this, params.numThreads, maxMSearchCDepth, maxCDepth, b, mainBoardHistory, mainUndoData);

  eval_t alpha = Eval::LOSE-1;
  eval_t beta = Eval::WIN+1;

  //Qsearch directly if the depth is negative
  if(maxDepth <= 0)
  {
    directQSearch(maxDepth,alpha,beta);
  }
  else
  {
    //Iteratively search deeper
    for(int depth = startDepth; depth <= maxDepth; depth++)
    {
      currentIterDepth = depth;
      int numFinished = 0;
      fsearch(depth*SearchParams::DEPTH_DIV,alpha,beta,numFinished,briefLastDepth && depth == maxDepth);

      //Update parameters for time and do an additional time check using the time bound for the next depth
      //so that we don't start if it we're only going to quit immediately, or worse, just after one move searched.
      if(!didInterruptOrTimeout)
      {
        clockBaseEval = stats.finalEval;
        currentIterDepth = depth+1;
        updateDesiredTime(stats.finalEval,0,fullMoves.size());
        mainCheckTime();
      }

      if(doOutput && numFinished > 0)
      {
        double timeUsed = currentTimeUsed();
        double desiredTime = currentDesiredTime();
        if(desiredTime == SearchParams::AUTO_TIME) desiredTime = 0;
        string evalString = ArimaaIO::writeEval(stats.finalEval);
        (*params.output) << Global::strprintf("ID Depth:  %-6.4g Eval: %6s Time: %.2f/%.2f",
            stats.depthReached, evalString.c_str(), timeUsed, desiredTime)
             << "  PV: " << stats.pvString << endl << endl;
      }

      //Ran out of time or search interrupted
      if(didInterruptOrTimeout)
        break;

      //Proven win or loss, return
      if(SearchUtils::isTerminalEval(stats.finalEval))
        break;

      //After completing the shallowest search, which initializes the idpv and various stats for us,
      //move right away if it's the only non-losing move (relying on the fact that we generate fullmoves
      //with win-loss pruning)
      if(params.moveImmediatelyIfOnlyOneNonlosing && depth <= startDepth && fullMoves.size() <= 1)
        break;

      //Decay the history tables if there's going to be another iteration
      if(depth < maxDepth)
        SearchUtils::decayHistory(historyTable,historyMax);
    }
  }

  //The whole search is done. The search tree destructor signals and waits for the helper threads to
  //terminate completely
  DEBUGASSERT(searchTree != NULL);
  delete searchTree;
  searchTree = NULL;

  //Update data
  stats.timeTaken = clockTimer.getSeconds();
  stats.randSeed = params.randomize ? params.randSeed : 0;
}


//FSEARCH-----------------------------------------------------------------------------

//Perform search and stores best eval found in stats.finalEval, behaving appropriately depending on allowance
//of partial search. Similarly, fills in idpv with best pv so far
//Marks numFinished with how many moves finished before interruption
//Uses and modifies fullHm for ordering
//Whenever a new move is best, stores its eval in fullHm, but for moves not best, stores Eval::LOSE-1.
//And at the end, sorts the moves
void Searcher::fsearch(int rDepth4, eval_t alpha, eval_t beta, int& numFinished, bool briefLastDepth)
{
  //Initialize stuff
  numFinished = 0;
  int cDepth = 0;

  const Board& b = mainBoard;
  if(params.viewing(b,mainBoardHistory)) params.viewSearch(b,mainBoardHistory,cDepth,rDepth4/4,alpha,beta,stats);

  //Get thread struct for ourselves, we are the master thread
  SearchThread* curThread = searchTree->getMasterThread();

  //Create the splitpoint and do the search on it
  DEBUGASSERT(searchTree != NULL);
  int fullNumMoves = fullMoves.size();
  SplitPoint* spt = searchTree->acquireRootNode();
  spt->init(NULL,0,ERRMOVE,SplitPoint::MODE_NORMAL,b.turnNumber*4+b.step,b.step == 0,false,0,
      cDepth,rDepth4,b.sitCurrentHash,alpha,beta,0,true,ERRMOVE,KillerEntry(),false,fullNumMoves,curThread->id);
  genFSearchMoves(spt,briefLastDepth);
  Board copy = b;
  maybeComputePosData(copy,curThread->boardHistory,rDepth4,curThread);

  parallelFSearch(curThread,spt);

  //Combine stats
  stats.overwriteAggregates(searchTree->getCombinedStats());

  //Update final stats data
  numFinished = spt->numMovesDone;
  if(numFinished > 0)
  {
    if(!params.disablePartialSearch || numFinished == spt->numMoves)
    {
      stats.finalEval = spt->bestEval;
      SearchUtils::copyPV(spt->pv,spt->pvLen,idpv,idpvLen);
      stats.pvString = Board::writeMoves(b,getIDPV());
      stats.depthReached = rDepth4/SearchParams::DEPTH_DIV - 1 + (double)numFinished/spt->numMoves;
    }

    if(numFinished == spt->numMoves)
    {
      //Reorder moves based on what was best
      //Relies on searchthread copying evals back at the root in reportResult
      DEBUGASSERT(spt->numMoves == fullNumMoves);
      SearchUtils::insertionSort(fullMoves,spt->hm);
    }
  }

  searchTree->freeRootNode(spt);
}

void Searcher::directQSearch(int depth, eval_t alpha, eval_t beta)
{
  const Board& b = mainBoard;

  int fDepth = 0;
  int cDepth = 0;
  int qDepth = -depth;
  if(qDepth >= SearchParams::QDEPTH_EVAL)
    qDepth = SearchParams::QDEPTH_EVAL;

  if(params.viewing(b,mainBoardHistory)) params.viewSearch(b,mainBoardHistory,cDepth,-qDepth,alpha,beta,stats);

  //Get thread struct for ourselves, we are the master thread
  SearchThread* curThread = searchTree->getMasterThread();

  Board copy = b;
  eval_t eval = qSearch(curThread, copy, fDepth, cDepth, qDepth, alpha, beta, SS_MAINLEAF);

  stats = searchTree->getCombinedStats();

  //Update final stats data
  stats.finalEval = eval;
  SearchUtils::copyPV(curThread->pv[0],curThread->pvLen[0],idpv,idpvLen);
  stats.pvString = Board::writeMoves(b,getIDPV());
  stats.depthReached = -qDepth;
}

//HELPERS-----------------------------------------------------------------------------------

//Utilities for adding extra adjustments to the eval that don't apply to wins or losses
static void addExtra(eval_t& eval, eval_t extra)
{
  if(extra == 0)
    return;
  if(eval > -Eval::EXTRA_EVAL_MAX_RANGE && eval < Eval::EXTRA_EVAL_MAX_RANGE)
  {
    eval += extra;
    if(eval < -Eval::EXTRA_EVAL_MAX_RANGE)
      eval = -Eval::EXTRA_EVAL_MAX_RANGE;
    else if(eval > Eval::EXTRA_EVAL_MAX_RANGE)
      eval = Eval::EXTRA_EVAL_MAX_RANGE;
  }
}
//This version ensures that alpha and beta never become a zero-sized window and
//makes sure that the appropriate bounds are used that match the correct semantics
//(evals <= alpha are an upper bound, evals >= beta are a lower bound, between is exact)
static void subtractExtraAlphaBeta(eval_t& alpha, eval_t& beta, eval_t extra)
{
  if(extra == 0)
    return;
  if(alpha >= -Eval::EXTRA_EVAL_MAX_RANGE && alpha < Eval::EXTRA_EVAL_MAX_RANGE)
  {
    alpha -= extra;
    if(alpha < -Eval::EXTRA_EVAL_MAX_RANGE)
      alpha = -Eval::EXTRA_EVAL_MAX_RANGE;
    else if(alpha >= Eval::EXTRA_EVAL_MAX_RANGE)
      alpha = Eval::EXTRA_EVAL_MAX_RANGE - 1;
  }

  if(beta > -Eval::EXTRA_EVAL_MAX_RANGE && beta <= Eval::EXTRA_EVAL_MAX_RANGE)
  {
    beta -= extra;
    if(beta <= -Eval::EXTRA_EVAL_MAX_RANGE)
      beta = -Eval::EXTRA_EVAL_MAX_RANGE + 1;
    else if(beta > Eval::EXTRA_EVAL_MAX_RANGE)
      beta = Eval::EXTRA_EVAL_MAX_RANGE;
  }
}

//Check that in at least one node for the turn that this splitpoint is for
//we are past searching the special moves the specified distance.
static bool atLeastOneLateMoveIdx(SplitPoint* spt, int numPastSpecial, int moveIdx)
{
  if(moveIdx >= spt->numSpecialMoves + numPastSpecial)
    return true;
  while(!(spt->changedPlayer) && spt->parent != NULL)
  {
    if(spt->parentMoveIndex >= spt->parent->numSpecialMoves + numPastSpecial)
      return true;
    spt = spt->parent;
  }
  return false;
}

//MSEARCH-----------------------------------------------------------------------------------

void Searcher::mSearch(SearchThread* curThread, SplitPoint* spt, SplitPoint*& newSpt,
    eval_t& evalBuf, bool& isReSearchBuf, int& depthAdjustmentBuf)
{
  //By default, if we return without setting these again
  newSpt = NULL;
  evalBuf = Eval::LOSE-1;
  isReSearchBuf = false;
  depthAdjustmentBuf = SearchParams::DEPTH_ADJ_PRUNE;

  Board& b = curThread->board;
  int moveIdx = curThread->curSplitPointMoveIdx;
  move_t move = curThread->curSplitPointMove;
  bool isNullMove = spt->hasNullMove && moveIdx == 0;

  DEBUGASSERT(move != ERRMOVE);
  DEBUGASSERT(!isNullMove || (move == QPASSMOVE && b.step == 0));

  //Just in case we have a result, record an empty pv
  curThread->endPV(spt->fDepth + 1);

  //Make sure histories are synced
  DEBUGASSERT(
    curThread->boardHistory.minTurnNumber <= b.turnNumber &&
    curThread->boardHistory.maxTurnNumber >= b.turnNumber &&
    curThread->boardHistory.turnPosHash[b.turnNumber] == (b.step == 0? b.posCurrentHash : b.posStartHash)
  );
  DEBUGASSERT(b.turnNumber*4+b.step == spt->cDepth + mainBoard.turnNumber*4+mainBoard.step);

  //We have a result, and it's a loss because we're pruning it
  if(moveIdx >= spt->pruneThreshold)
    return;

  //Try the move!
  vector<UndoMoveData>& uData = curThread->undoData;
  int oldBoardStep = b.step;
  //Always check legality for hash move, killer moves
  if(moveIdx < spt->numSpecialMoves)
  {
    bool suc = b.makeMoveLegal(move,uData);
    if(!suc) return;
  }
  else
  {
    //FIXME don't do move legality checking if not in debug
    bool suc = b.makeMoveLegal(move,uData);
    if(!suc)
    {
      ONLYINDEBUG(SearchUtils::reportIllegalMove(b,move));
      return;
    }
  }

  //Cannot return to the same position
  if(b.posCurrentHash == b.posStartHash && !isNullMove)
  {
    b.undoMove(uData);
    return;
  }

  bool changedPlayer = (b.step == 0);
  int numSteps = changedPlayer ? 4-oldBoardStep : b.step-oldBoardStep;

  //Add move to history and keep going
  curThread->boardHistory.reportMove(b,move,oldBoardStep);

  //Compute appropriate amount of reduction
  bool pruneReducing = curThread->curPruneReduceDesired;
  int depthAdjustment = curThread->curDepthAdjDesired;

  DEBUGASSERT(!isNullMove || (!pruneReducing && changedPlayer));

  //BEGIN NEW POSITION------------------------------------------------------------------------
  curThread->stats.mNodes++;

  pla_t pla = b.player;
  int fDepth = spt->fDepth + 1;
  int cDepth = spt->cDepth + numSteps;
  int rDepth4Unreduced = spt->rDepth4 - numSteps * SearchParams::DEPTH_DIV;

  if(isNullMove)
  {
    DEBUGASSERT(spt->rDepth4 >= SearchParams::NULLMOVE_MIN_RDEPTH * SearchParams::DEPTH_DIV);
    int reduction = spt->rDepth4 == SearchParams::NULLMOVE_MIN_RDEPTH * SearchParams::DEPTH_DIV ?
        SearchParams::NULLMOVE_REDUCE_DEPTH_BORDERLINE : SearchParams::NULLMOVE_REDUCE_DEPTH;
    rDepth4Unreduced -= reduction * SearchParams::DEPTH_DIV;
  }

  //TIME CHECK-------------------------------
  tryCheckTime(curThread);
  if(curThread->isTerminated)
  {
    evalBuf = 0;
    b.undoMove(uData);
    return;
  }

  //END CONDITION - GAME RULES---------------------------
  if(!isNullMove)
  {
    eval_t gameEndVal = SearchUtils::checkGameEndConditions(b,curThread->boardHistory,cDepth);
    if(gameEndVal != 0)
    {
      evalBuf = changedPlayer ? -gameEndVal : gameEndVal;
      b.undoMove(uData);
      return;
    }
  }

  //END CONDITION - WINNING BOUNDS ----------------------
  //Alpha and beta bounds at the parent
  eval_t oldAlpha = isNullMove ? spt->beta - 1 : spt->alpha.load(std::memory_order_relaxed);
  eval_t oldBeta = spt->beta;

  //If winning at the earliest possible time puts us below alpha, then we're certainly below alpha
  //Takes advantage of the assumption that for the purposes of win distance, you can't lose by the
  //end of your turn with a legal move available.
  int numStepsLeft = 4-b.step;
  int fourIfChangedPlayer = (int)changedPlayer * 4;
  //If changed player: The earliest time we can win (from the parent's perspective) since we haven't
  //won already, is at the end of our next move.
  //Else: The earliest time we can win is at the end of our *next* move, since otherwise goal and
  //elim trees should have found it earlier.
  if(Eval::WIN - cDepth - numStepsLeft - 8 + fourIfChangedPlayer <= oldAlpha)
  {
    evalBuf = oldAlpha;
    b.undoMove(uData);
    return;
  }
  //If losing at the earliest possible time puts us above beta, then we're certainly above beta
  //Takes advantage of the assumption that for the purposes of win distance, you can't lose by the
  //end of your turn with a legal move available.
  //If changed player: The earliest time that we can lose (from the parent's perspective) since we
  //haven't lost already is at the end of the opponent's move.
  //Else: The earliest time we can lose is at the end of the opponent's next move.
  if(Eval::LOSE + cDepth + numStepsLeft + 4 - fourIfChangedPlayer >= oldBeta)
  {
    evalBuf = oldBeta;
    b.undoMove(uData);
    return;
  }
  //If we're prunereducing but the tightened prunereduce beta would still be exceeded if we lost in one move
  //then don't prunereduce
  if(pruneReducing && Eval::LOSE + cDepth + numStepsLeft + 4 - fourIfChangedPlayer >= oldAlpha+1)
  {
    pruneReducing = false;
    depthAdjustment = 0;
  }

  //END CONDITION - GOAL TREE AND ELIM TREE --------------------
  if(changedPlayer)
  {
    int goalDist = BoardTrees::goalDist(b,pla,numStepsLeft);
    if(goalDist <= 4)
    {
      evalBuf = Eval::LOSE + cDepth + numStepsLeft;
      b.undoMove(uData);
      return;
    }

    bool canElim = BoardTrees::canElim(b,pla,numStepsLeft);
    if(canElim)
    {
      evalBuf = Eval::LOSE + cDepth + numStepsLeft;
      b.undoMove(uData);
      return;
    }

    //END CONDITION - WINNING BOUNDS AFTER BOARD TREES ----------------------
    //Now that we've tested goal trees...
    //The earliest time that we can lose (from the parent's perspective) since we
    //haven't lost already is at the end of the opponent's *next* move, after the current move.
    if(Eval::LOSE + cDepth + numStepsLeft + 8 >= oldBeta)
    {
      evalBuf = oldBeta;
      b.undoMove(uData);
      return;
    }
    //If we're prunereducing but the tightened prunereduce beta would still be exceeded if we lost in one move
    //then don't prunereduce
    if(pruneReducing && Eval::LOSE + cDepth + numStepsLeft + 8 >= oldAlpha+1)
    {
      pruneReducing = false;
      depthAdjustment = 0;
    }
  }

  //EXTRAEVAL CONDITION - ROOT BIAS-----------------------------------
  //Set by various conditions to disable us from considering extending here
  bool banExtensions = false;
  //Extra eval to be added to the result of this search, from the parent's perspective
  eval_t extraEval = 0;
  if(spt->cDepth == 0)
  {
    DEBUGASSERT(fullMoves[moveIdx].move == move);
    double moveOrderingVal = fullMoves[moveIdx].val;
    extraEval = (eval_t)(moveOrderingVal * params.rootBiasStrength);
    if(extraEval > Eval::EXTRA_ROOT_EVAL_MAX)
      extraEval = Eval::EXTRA_ROOT_EVAL_MAX;
    else if(extraEval < -Eval::EXTRA_ROOT_EVAL_MAX)
      extraEval = -Eval::EXTRA_ROOT_EVAL_MAX;
  }

  //END/EXTRAEVAL CONDITION - REPETITION FIGHT------------------------------
  //Check if we lose a repetition fight because of this move
  if(SearchParams::ENABLE_REP_FIGHT_CHECK && changedPlayer && !isNullMove)
  {
    int turnsToLoseByRep = 0;
    if(SearchUtils::losesRepetitionFight(b,curThread->boardHistory,cDepth,turnsToLoseByRep))
    {
      //Deeper in the tree, just prune lost repetition fights outright
      if(cDepth >= 8)
      {
        //Add 4 to conform with winloss distance always assuming that you can't lose after having
        //made a move - you can only lose at the end of the opponent's turn.
        evalBuf = Eval::LOSE + cDepth + turnsToLoseByRep*4 + 4;
        b.undoMove(uData);
        return;
      }
      //Near the root, simply discourage the move with extraeval and disable extensions
      else if(cDepth >= 4)
      {
        extraEval -= SearchParams::LOSING_REP_FIGHT_PENALTY;
        banExtensions = true;
      }
    }
  }

  //END CONDITION - UNSAFE STATIC PRUNING-----------------------------------------
  //Checking rDepth4Unreduced >= 0 to make sure we didn't get to this depth by a killer/hash/pushpull
  //or other long move that shot us past the intended depth
  //Check that cDepth >= 4 to disable static pruning on the very first ply if we're starting a middle of turn search.
  if(SearchParams::ALLOW_UNSTABLE && changedPlayer && params.unsafePruneEnable
      && !SearchUtils::isLoseEval(oldAlpha) && rDepth4Unreduced >= 0 && cDepth >= 4 && !isNullMove)
  {
    int turnDepth = b.turnNumber - mainBoard.turnNumber;
    int prevTurnDepth = turnDepth - 1;
    DEBUGASSERT((int)curThread->unsafePruneIdHash.size() >= turnDepth);
    DEBUGASSERT(curThread->unsafePruneIdHash[prevTurnDepth] == curThread->boardHistory.turnSitHash[b.turnNumber-1]);
    move_t prevMove = curThread->boardHistory.turnMove[b.turnNumber-1];

    bool shouldPrune = SearchPrune::matchesMove(b,gOpp(b.player),prevMove,*(curThread->unsafePruneIds[prevTurnDepth]));
    if(shouldPrune)
    {
      evalBuf = oldAlpha;
      b.undoMove(uData);
      return;
    }
  }
  //If this is one of the two deepest levels of msearch, and we have searched at least a few moves already,
  //and we are on the last step, then try haizhi pruning
  if(SearchParams::ALLOW_UNSTABLE && b.step == 3 && params.unsafePruneEnable
      && !SearchUtils::isLoseEval(oldAlpha) && rDepth4Unreduced >= 0
      && rDepth4Unreduced <= 16 && atLeastOneLateMoveIdx(spt,1,moveIdx))
  {
    DEBUGASSERT(!isNullMove);
    DEBUGASSERT(b.turnNumber >= curThread->boardHistory.minTurnNumber &&
                b.turnNumber <= curThread->boardHistory.maxTurnBoardNumber);
    move_t prevMove = curThread->boardHistory.turnMove[b.turnNumber];
    if(SearchPrune::canHaizhiPrune(curThread->boardHistory.turnBoard[b.turnNumber],b.player,prevMove))
    {
      evalBuf = oldAlpha;
      b.undoMove(uData);
      return;
    }
  }

  //EXTRAEVAL CONDITION - REVERSIBLES-----------------------------------
  if(SearchParams::REVERSE_ENABLE && changedPlayer && cDepth >= 4 && !isNullMove)
  {
    move_t reverseMove = ERRMOVE;
    int rev = SearchUtils::isReversible(b, curThread->boardHistory, reverseMove);
    //Full reverse
    if(rev == 2)
    {
      extraEval -= SearchParams::FULL_REVERSE_PENALTY;
      banExtensions = true;
    }
    //Partial reverse
    else if(rev == 1)
    {
      if(cDepth >= 5) extraEval -= SearchParams::PARTIAL_REVERSE_PENALTY_TREE;
      else            extraEval -= SearchParams::PARTIAL_REVERSE_PENALTY_ROOT;
      banExtensions = true;
    }
  }

  //EXTRAEVAL CONDITION - FREECAPTURABLES-------------------------------
  loc_t freeCapLoc = ERRLOC;
  //TODO test this check in qsearch to stop stupid qsearch "defenses"
  if(SearchParams::FREECAP_PRUNE_ENABLE && changedPlayer && cDepth >= 4 && !isNullMove)
  {
    //MINOR what if the freecap move makes a goal threat so that the capture isn't possible?
    //Probably not worth worrying about, particularly due to the case where the freecap piece itself is the "threat"
    int startTurn = b.turnNumber-1;
    const BoardHistory& hist = curThread->boardHistory;
    //Free caps that also capture the opp's piece are okay.
    if(startTurn >= hist.minTurnNumber &&
       hist.turnPieceCount[pla][startTurn] == b.pieceCounts[pla][0])
    {
      int nsNotSpentOn;
      int nsCapture;
      if(SearchUtils::isFreeCapturable(b,curThread->boardHistory.turnMove[startTurn],nsNotSpentOn,nsCapture,freeCapLoc))
      {
        banExtensions = true;
        //All steps spent on captured piece
        if(nsNotSpentOn == 0)
        {
          if(rDepth4Unreduced <= 16) extraEval -= nsCapture < 4 ? 8000 : 6000;
          else                       extraEval -= nsCapture < 4 ? 3000 : 1500;
        }
        //All but one step spent on captured piece
        else
        {
          if(rDepth4Unreduced <= 16) extraEval -= nsCapture < 4 ? 6000 : 4000;
        }
      }
    }
  }

  //EXTENSION CONDITION - GOAL THREATS------------------------------------------
  //Check rDepth4Unreduced >= 0 so that we can only extend if we actually got here with full depth
  if(SearchParams::GOAL_EXTENSION_ENABLE && params.extensionEnable && !banExtensions
      && changedPlayer && rDepth4Unreduced >= 0 && !isNullMove)
  {
    //Check if the opp made a goal threat
    pla_t opp = gOpp(pla);
    int goalDist = BoardTrees::goalDist(b,opp,numStepsLeft);
    int extensionSteps = 0;

    //Check how many steps are needed to defend against the goal threat
    //TODO note that this is slightly inefficient, since in the case where a goal threat
    //was made, we will run the goal tree an extra time, once above and once inside this call.
    //But hopefully it's not a big deal.
    int minWinDefSteps = SearchMoveGen::minWinDefSteps(b,rDepth4Unreduced >= 2 ? 3 : 2);

    //If the opponent can defend it in 1 step, then don't extend unless the goal threat is
    //fairly close in distance.
    if(minWinDefSteps <= 1)
      extensionSteps = goalDist >= 3 ? 0 : 1;
    //If the opponent can defend it in 2 steps, extend a step for sure.
    else if(minWinDefSteps == 2)
      extensionSteps = 1;
    //If the opponent needs 3 steps to defend it, extend 2 steps
    else if(minWinDefSteps == 3)
      extensionSteps = 2;
    //If the opponent needs all steps to defend, extend 3 steps
    else
      extensionSteps = 3;

    if(extensionSteps > 0)
    {
      //If we're extending from a goal threat, then prevent any prunereductions that are occuring
      //NOTE: If we change this, then we need to tweak mSearchDone because currently the rDepth4 for the
      //re-search won't take into account that an extension happened!
      pruneReducing = false;
      depthAdjustment = extensionSteps;
    }
  }


  //Apply extraeval----------------------------------------------
  subtractExtraAlphaBeta(oldAlpha,oldBeta,extraEval);

  //Compute alpha beta bounds and depth from the child's perspective
  eval_t reducedBeta = pruneReducing ? oldAlpha+1 : oldBeta;
  eval_t alpha = changedPlayer ? -reducedBeta : oldAlpha;
  eval_t beta = changedPlayer ? -oldAlpha : reducedBeta;
  int rDepth4 = rDepth4Unreduced + depthAdjustment * SearchParams::DEPTH_DIV;
  int origDepthAdjustment = depthAdjustment;

  bool isReSearch = false;
  depthAdjustmentBuf = depthAdjustment;

  //BEGIN PRUNE REDUCTION LOOP----------------------------------------------
  eval_t eval;
  while(true) //Loop again if we need to do a non-prune-reduced iteration
  {
  do //To allow early breaking for hasheval or other immediate eval
  {

  //END CONDITION - MAX DEPTH-------------------------------------
  //Switch to quiescence
  //FIXME turn off prunereduction if both the reduced and nonreduced would fall into qsearch
  if(rDepth4 <= 0)
  {
    //Should fill in fDepth's pv, as desired
    int qSearchEval = qSearch(curThread,b,fDepth,cDepth,SearchParams::QDEPTH_START[b.step],alpha,beta,SS_MAINLEAF);
    {eval = qSearchEval; break;}
  }

  //VIEW CHECK----------------------------------------------------
  bool isViewing = params.viewing(b,curThread->boardHistory);
  if(isViewing) params.viewSearch(b,curThread->boardHistory,cDepth,rDepth4/4,alpha,beta,curThread->stats);

  //END CONDITION - HASH LOOKUP--------------------------------------
  move_t hashMove = ERRMOVE;
  if(SearchParams::HASH_ENABLE)
  {
    eval_t hashEval;
    int16_t hashDepth;
    flag_t hashFlag;
    //Found a hash?
    if(mainHash->lookup(hashMove,hashEval,hashDepth,hashFlag,b,cDepth,false))
    {
      //Does the entry allow for a cutoff?
      if(((hashDepth == rDepth4/4 || (SearchParams::ALLOW_UNSTABLE && hashDepth >= rDepth4/4)) &&
          SearchUtils::canEvalCutoff(alpha,beta,hashEval,hashFlag)) ||
         (SearchParams::ALLOW_UNSTABLE && SearchUtils::canTerminalEvalCutoff(alpha,beta,hashEval,hashFlag))
        )
      {
        curThread->stats.mHashCuts++;
        if(isViewing) params.viewHash(b, hashEval, hashFlag, hashDepth, hashMove);
        {eval = hashEval; break;}
      }

      //Do not try passes that were generated in the qsearch
      if(hashMove == QPASSMOVE)
        hashMove = ERRMOVE;

      //If aiming for stability, clear hashmoves that from a greater depth that might cause us to act differently
      //since we might not generate such a move if it extends into qsearch
      if(!SearchParams::ALLOW_UNSTABLE && hashDepth > rDepth4/4)
        hashMove = ERRMOVE;

      if(hashDepth <= 0 && SearchParams::HASH_NO_USE_QBM_IN_MAIN)
        hashMove = ERRMOVE;
      else
      {
        hashMove = SearchUtils::convertQPassesToPasses(hashMove);

        //Probably unnecessary (the same way move legality checks for hashmoves are unnecessary)
        //but just in case, repair potential corrupted hashMove.
        hashMove = SearchUtils::removeRedundantSteps(hashMove);
      }
    }
  }

  KillerEntry killers;
  if(SearchParams::KILLER_ENABLE)
  {
    SearchUtils::getKillers(killers,curThread->killerMoves,curThread->killerMovesLen,cDepth);
    for(int i = 0; i<KillerEntry::NUM_KILLERS; i++)
    {
      if(killers.killer[i] == QPASSMOVE)
        killers.killer[i] = ERRMOVE;
      else
        killers.killer[i] = SearchUtils::convertQPassesToPasses(killers.killer[i]);
    }
  }

  maybeComputePosData(b,curThread->boardHistory,rDepth4,curThread);

  //TODO Think about:
  //Note that reductions in-tree might have confusing interactions if they cause us not to search a full move
  //as deeply as we planned, so we do mixed unsafePruning and other things that only want to be applied at the "last" depth of search.

  //Determine whether we should try a null move pruning first or not
  bool allowNull = params.nullMoveEnable;

  //Here we go! We actually have a new child splitpoint, so get one from the buffer and return
  int mode = pruneReducing ? (int)SplitPoint::MODE_REDUCED : (int)SplitPoint::MODE_NORMAL;
  DEBUGASSERT(curThread->lockedSpt == NULL);
  newSpt = curThread->curSplitPointBuffer->acquireSplitPoint(fDepth);
  newSpt->init(spt,moveIdx,move,mode,b.turnNumber*4+b.step,changedPlayer,isReSearch,origDepthAdjustment,
      cDepth,rDepth4,b.sitCurrentHash,alpha,beta,extraEval,false,hashMove,killers,allowNull,
      SearchParams::MSEARCH_MOVE_CAPACITY,curThread->id);
  return;
  }
  while(0);

  DEBUGASSERT(depthAdjustment == 0 || pruneReducing == (depthAdjustment < 0));

  //Flip eval to be the right sign
  eval = changedPlayer ? -eval : eval;
  //Break out and return the desired eval unless we are a prunereduce that failed high
  //Also break out and return the desired eval if the reduction is zero and we are
  //already above the beta we would re-search with.
  if(!pruneReducing || eval <= oldAlpha || (depthAdjustment == 0 && eval >= oldBeta))
  {
    //For prunereductions, we can't use failsoft alpha beta, but we can trust it
    //if was merely plain PVS, a regular search, or an extension
    if(depthAdjustment < 0)
      eval = oldAlpha;
    addExtra(eval,extraEval);
    break;
  }

  DEBUGASSERT(!isNullMove);

  //Prunereduce failed high, search again at full depth
  if(changedPlayer)
    alpha = -oldBeta;
  else
    beta = oldBeta;
  rDepth4 -= depthAdjustment * SearchParams::DEPTH_DIV;

  pruneReducing = false;
  depthAdjustment = 0;
  isReSearch = true;
  isReSearchBuf = true;

  } //Close prune reduce loop

  evalBuf = eval;
  b.undoMove(uData);
  return;
}

//Returns true if done
bool Searcher::mSearchDone(SearchThread* curThread, SplitPoint* spt, eval_t& evalBuf)
{
  evalBuf = Eval::LOSE - 1;
  Board& b = curThread->board;

  if(curThread->isTerminated || spt->resultsAreInvalid)
  {
    b.undoMove(curThread->undoData);
    return true;
  }

  bool nullMoveCutoff = spt->hasNullMove && spt->bestIndex == 0 && spt->bestFlag == Flags::FLAG_BETA;

  //TODO turn the arrows into local variables instead of repeated accessing?
  move_t moveToRecord;
  if(nullMoveCutoff)
    moveToRecord = spt->hashMove;
  else
  {
    moveToRecord = SearchUtils::extractTurnMoveFromPV(spt->pv, spt->pvLen, b);
    if(moveToRecord == ERRMOVE)
      moveToRecord = spt->bestMove;
  }
  //TODO on both this and a qsearch, consider storing the first legal tried move
  //or nothing at all in the hash for alpha nodes. Particularly with a deep search,
  //the fail-soft best move from the alpha node might be worthless and not a good guess for a best move.
  DEBUGASSERT((spt->rDepth4 & 0x3) == 0);
  mainHash->record(b, spt->cDepth, spt->rDepth4/4, spt->bestEval, spt->bestFlag, moveToRecord,false);

  if(spt->bestFlag != Flags::FLAG_ALPHA && !nullMoveCutoff)
  {
    if(SearchParams::KILLER_ENABLE)
      SearchUtils::recordKiller(curThread->killerMoves, curThread->killerMovesLen, moveToRecord, spt->cDepth);
    SearchUtils::updateHistoryTable(historyTable,historyMax,b,spt->cDepth,spt->bestMove);
  }

  //Note: bestIndex can be -1 and bestMove be ERRMOVE if every move loses immediately
  DEBUGASSERT(spt->numMoves <= 0 || (spt->bestIndex == -1 && spt->bestMove == ERRMOVE) || spt->mv[spt->bestIndex] == spt->bestMove);

  if(spt->cDepth > 0 && !nullMoveCutoff)
  {
    curThread->stats.bestMoveSum += spt->bestIndex >= 0 ? spt->bestIndex - (int)spt->hasNullMove : 0;
    curThread->stats.bestMoveCount++;
  }

  bool isViewing = params.viewing(b,curThread->boardHistory);
  if(isViewing)
    params.viewFinished(b,spt->changedPlayer,spt->bestEval,spt->extraEval,spt->bestMove,spt->bestFlag,curThread->stats);

  //Normal search done - return the eval
  int result = spt->changedPlayer ? -spt->bestEval : spt->bestEval;
  if(spt->searchMode == SplitPoint::MODE_NORMAL)
  {
    addExtra(result,spt->extraEval);
    evalBuf = result;
    if(spt->fDepth < SearchParams::PV_ARRAY_SIZE)
      SearchUtils::copyPV(spt->pv, spt->pvLen, curThread->pv[spt->fDepth], curThread->pvLen[spt->fDepth]);
    b.undoMove(curThread->undoData);
    return true;
  }

  //Reduced depth search
  DEBUGASSERT(spt->searchMode == SplitPoint::MODE_REDUCED)

  //If the reduced depth search got the expected result (failed below alpha at the parent)
  //then no further search is needed.
  //Also if it is already above the re-search beta and we were merely doing 0 depth reduction PVS, then it's also okay.
  //Note that we require that it failed below *alpha at the time of the start of search*, rather
  //than an alpha that might have been improved at the parent by another thread in the meantime!
  //Because if it's below the new alpha, it still might be the best move and the new pv if it failed high
  //past the original alpha+1.
  bool changedPlayer = spt->changedPlayer;
  int oldAlpha = changedPlayer ? -spt->beta : spt->beta-1;

  //New parent alphabeta, might have improved in the meantime due to multithreading
  eval_t parentAlpha = spt->parent->alpha.load(std::memory_order_relaxed);
  eval_t parentBeta = spt->parent->beta;
  subtractExtraAlphaBeta(parentAlpha,parentBeta,spt->extraEval);

  int cDepth = spt->cDepth;
  int numSteps = cDepth - spt->parent->cDepth;
  int rDepth4 = spt->parent->rDepth4 - numSteps * SearchParams::DEPTH_DIV;

  if(result <= oldAlpha || (rDepth4 == spt->rDepth4 && result >= parentBeta))
  {
    //For deeper prunereductions, we can't use failsoft alpha beta, but we can trust it
    //if was merely plain PVS
    if(rDepth4 != spt->rDepth4)
      result = oldAlpha;
    addExtra(result,spt->extraEval);
    evalBuf = result;
    b.undoMove(curThread->undoData);
    //In the case where we betacutoff, record the pv so that we can use it as a killer
    if(spt->fDepth < SearchParams::PV_ARRAY_SIZE)
      SearchUtils::copyPV(spt->pv, spt->pvLen, curThread->pv[spt->fDepth], curThread->pvLen[spt->fDepth]);
    return true;
  }

  eval_t alpha = changedPlayer ? -parentBeta : parentAlpha;
  eval_t beta = changedPlayer ? -parentAlpha : parentBeta;

  if(isViewing) params.viewResearch(cDepth,rDepth4/4,alpha,beta,curThread->stats);

  KillerEntry killers;
  if(SearchParams::KILLER_ENABLE)
  {
    SearchUtils::getKillers(killers,curThread->killerMoves,curThread->killerMovesLen,cDepth);
    for(int i = 0; i<KillerEntry::NUM_KILLERS; i++)
    {
      if(killers.killer[i] == QPASSMOVE)
        killers.killer[i] = ERRMOVE;
      else
        killers.killer[i] = SearchUtils::convertQPassesToPasses(killers.killer[i]);
    }
  }

  maybeComputePosData(b,curThread->boardHistory,rDepth4,curThread);

  bool allowNull = params.nullMoveEnable;

  DEBUGASSERT(curThread->lockedSpt == NULL);
  spt->init(spt->parent,spt->parentMoveIndex,spt->parentMove,SplitPoint::MODE_NORMAL,
      spt->stepTurn,changedPlayer,true,spt->origDepthAdjustment,
      cDepth,rDepth4,spt->hash,alpha,beta,spt->extraEval,false,spt->bestMove,killers,allowNull,
      SearchParams::MSEARCH_MOVE_CAPACITY,curThread->id);
  return false;
}

void Searcher::maybeComputePosData(Board& b, const BoardHistory& history, int rDepth4, SearchThread* curThread)
{
  (void)history;
  //NOTE syncWithSplitPointDistant relies on it being okay to pass a larger than true rDepth4 into this function
  //This will cause us to unnecessarily compute the featurePosData sometimes, but that's okay.
  if(SearchParams::ALLOW_UNSTABLE && params.unsafePruneEnable && b.step == 0)
  {
    int turnDepth = b.turnNumber - mainBoard.turnNumber;
    //Only compute posdata for pruning if we know we're searching deep enough to finish this whole turn.
    //Currently, extensions won't interfere with this because they only take effect at the end of whole turn searches.
    if(rDepth4 >= 16)
    {
      while((int)curThread->unsafePruneIds.size() < turnDepth+1)
      {
        curThread->unsafePruneIdHash.push_back(0);
        curThread->unsafePruneIds.push_back(new vector<SearchPrune::ID>());
      }
      curThread->unsafePruneIdHash[turnDepth] = b.sitCurrentHash;
      SearchPrune::findStartMatches(b,*(curThread->unsafePruneIds[turnDepth]));
    }
  }
}

//QSEARCH------------------------------------------------------------------------

//RAII style object for acquiring a chunk of a move list buffer to use for a particular fdepth during recursion
namespace {
struct MoveListAcquirer
{
  public:
  move_t* const mv; //These are the acquired lists, offset appropriately, that you can grab and stuff data into
  int* const hm;    //These are the acquired lists, offset appropriately, that you can grab and stuff data into

  private:
  const int maxCapacity;         //Max capacity of the whole list
  const int initialCapacityUsed; //Capacity prior to this MoveListAquirier
  int& capacityUsed;             //Set to new capacity (or -1 prior to reportUsage when debugging, to flag the possible error)
  int selfCapacity;              //How much capacity used by ourself, set by reportUsage

  public:
  MoveListAcquirer(move_t* mv, int* hm, int maxCapacity, int& capacityUsed)
  :mv(mv+capacityUsed),hm(hm+capacityUsed),maxCapacity(maxCapacity),
   initialCapacityUsed(capacityUsed),capacityUsed(capacityUsed),selfCapacity(0)
  {
    ONLYINDEBUG(
    if(capacityUsed == -1)
      Global::fatalError("Probably forgot to call reportUsage");
    capacityUsed = -1;
    selfCapacity = -1;
    )
  }

  ~MoveListAcquirer()
  {
    ONLYINDEBUG(
    if(selfCapacity == -1)
    {capacityUsed = initialCapacityUsed; return;}
    if(capacityUsed != initialCapacityUsed + selfCapacity)
      Global::fatalError("MoveListAcquirer: capacities don't match");
    )
    capacityUsed -= selfCapacity;

    ONLYINDEBUG(
    if(capacityUsed < 0)
      Global::fatalError("MoveListAcquirer: capacityUsed < 0");
    if(capacityUsed != initialCapacityUsed)
      Global::fatalError("MoveListAcquirer: capacityUsed not the same after destruction");
    )
  }

  //Can call more than once to report different changes in usage.
  //But should ONLY call when you are the head - if any further acquirers have taken
  //chunks of the list after you, you cannot change.
  void reportUsage(int num)
  {
    ONLYINDEBUG(
    if(selfCapacity != -1 && capacityUsed != initialCapacityUsed + selfCapacity)
      Global::fatalError("MoveListAcquirer: Reported usage but capacities don't match");
    )

    selfCapacity = max(selfCapacity,num);
    capacityUsed = initialCapacityUsed + selfCapacity;

    ONLYINDEBUG(
    if(capacityUsed > maxCapacity)
      Global::fatalError("MoveListAcquirer: capacityUsed > maxCapacity");
    )
  }
};
}

int Searcher::qSearchQPassed(SearchThread* curThread, Board& b, int fDepth, int cDepth, int qDepth, eval_t alpha, eval_t beta)
{
  //BEGIN QUIESCENCE SEARCH------------------------
  //pla_t pla = b.player;

  //Mark the end of the PV, in case we return
  curThread->endPV(fDepth);

  //QEND CONDITION - TIME CHECK-------------------------------
  //Skip time check in qpass nodes
  //Q END CONDITIONS----------------------------------------
  //Skip end condition check, qpass doesn't change the board state so no need to recheck
  //Q END CONDITIONS - WINNING BOUNDS ----------------------
  //If winning in one move puts us below alpha, then we're certainly below alpha
  int numStepsLeft = 4-b.step;
  if(Eval::WIN - cDepth - numStepsLeft <= alpha)
    return alpha;
  //If losing in one move puts us above beta, then we're certainly above beta
  //Adding 4 steps takes advantage of the assumption that you can't lose with a legal move available
  //(counting 3-move reps as legal moves that cause you to lose but with a winloss distance of the end of the opp's next move)
  if(Eval::LOSE + cDepth + numStepsLeft + 4 >= beta)
    return beta;

  //Q VIEW CHECK-----------------------------------------------
  bool isViewing = params.viewing(b,curThread->boardHistory);
  if(isViewing) params.viewSearch(b,curThread->boardHistory,cDepth,-qDepth,alpha,beta,curThread->stats);

  //Q END CONDITION - HASHTABLE--------------------------------
  if(SearchParams::HASH_ENABLE)
  {
    move_t hashMove;
    eval_t hashEval;
    int16_t hashDepth;
    flag_t hashFlag;
    //Found a hash?
    if(mainHash->lookup(hashMove,hashEval,hashDepth,hashFlag,b,cDepth,true))
    {
      //Does the entry allow for a cutoff?
      if((((SearchParams::ALLOW_UNSTABLE && hashDepth >= -qDepth) || hashDepth == -qDepth)
          && SearchUtils::canEvalCutoff(alpha,beta,hashEval,hashFlag))
         || (SearchParams::ALLOW_UNSTABLE && SearchUtils::canTerminalEvalCutoff(alpha,beta,hashEval,hashFlag))
        )
      {
        curThread->stats.qHashCuts++;
        if(isViewing) params.viewHash(b, hashEval, hashFlag, hashDepth, hashMove);
        return hashEval;
      }
    }
  }

  //Q EVALUATE-------------------------------------
  //Just evaluate
  if(SearchUtils::isLoseEval(beta))
    return beta;
  if(SearchUtils::isWinEval(alpha))
    return alpha;
  int eval = evaluate(curThread,b,isViewing && params.viewEvaluate);
  mainHash->record(b, cDepth, -qDepth, eval, Flags::FLAG_EXACT, ERRMOVE, true);
  return eval;
}


int Searcher::qSearch(SearchThread* curThread, Board& b, int fDepth, int cDepth, int qDepth, eval_t alpha, eval_t beta, SearchState qState)
{
  //BEGIN QUIESCENCE SEARCH------------------------
  pla_t pla = b.player;

  //Mark the end of the PV, in case we return
  curThread->endPV(fDepth);

  //QEND CONDITION - TIME CHECK-------------------------------
  tryCheckTime(curThread);
  if(curThread->isTerminated)
    return alpha;

  //Q END CONDITIONS----------------------------------------
  //Do not check end conditions at the main search leaf, since already checked
  if(qState == SS_Q)
  {
    eval_t gameEndVal = SearchUtils::checkGameEndConditionsQSearch(b,cDepth);
    if(gameEndVal != 0)
      return gameEndVal;
  }

  //Q END CONDITIONS - WINNING BOUNDS ----------------------
  //If winning in this turn puts us below alpha, then we're certainly below alpha
  int numStepsLeft = 4-b.step;
  if(Eval::WIN - cDepth - numStepsLeft <= alpha)
    return alpha;
  //If losing in at the end of the opponent's next move puts us above beta, then we're certainly above beta
  //This takes advantage of the assumption in winloss scores that you can't lose until the end of the opponent's turn.
  if(Eval::LOSE + cDepth + numStepsLeft + 4 >= beta)
    return beta;

  //Q END CONDITION - GOAL TREE AND ELIM TREE --------------
  //Do not check end conditions at the main search leaf, since already checked
  //And only check when the player changes, since the check for goal/elim is good for the whole turn.
  if(qState == SS_Q && b.step == 0)
  {
    int goalDist = BoardTrees::goalDist(b,pla,numStepsLeft);
    if(goalDist <= 4)
      return Eval::WIN - cDepth - numStepsLeft;

    if(BoardTrees::canElim(b,pla,numStepsLeft))
      return Eval::WIN - cDepth - numStepsLeft;
  }

  //Q END CONDITIONS - WINNING BOUNDS ----------------------
  //Goal trees tell us that we can't win this turn. So if winning *next* turn puts us below alpha, then we're certainly below alpha
  if(Eval::WIN - cDepth - numStepsLeft - 8 <= alpha)
    return alpha;

  //Q VIEW CHECK-----------------------------------------------
  bool isViewing = params.viewing(b,curThread->boardHistory);
  if(isViewing) params.viewSearch(b,curThread->boardHistory,cDepth,-qDepth,alpha,beta,curThread->stats);

  //Q END CONDITION - HASHTABLE--------------------------------
  bool hashFound = false;
  move_t hashMove = ERRMOVE;
  if(SearchParams::HASH_ENABLE)
  {
    eval_t hashEval;
    int16_t hashDepth;
    flag_t hashFlag;
    //Found a hash?
    if(mainHash->lookup(hashMove,hashEval,hashDepth,hashFlag,b,cDepth,true))
    {
      hashFound = true;

      //Does the entry allow for a cutoff?
      if(((SearchParams::ALLOW_UNSTABLE && hashDepth >= -qDepth) || hashDepth == -qDepth)
          && SearchUtils::canEvalCutoff(alpha,beta,hashEval,hashFlag))
      {
        curThread->stats.qHashCuts++;
        if(isViewing) params.viewHash(b, hashEval, hashFlag, hashDepth, hashMove);
        return hashEval;
      }

      //MINOR if the hashmove is a PASSMOVE from a higher depth, consider not making it.
      //Probably depending on how the eval scores things based on whose turn it is, and the bonusing
      //of remaining steps, it's probably strictly worse than qpassing, so there's no point doing it.
      //Note: I tried this, but it actually made things slower due to the extra code while not actually
      //helping anything because in 350 test positions, the hashmove was never a passmove, at least in
      //a way that mattered or affected the node count.
      //TODO rewrite For making stable search mode, you need to think about stuff like this too.
      //because passmoves and qpassmoves could differ on a ply if there are reductions... Maybe don't
      //do reductions in stable search

      if(hashDepth < -qDepth && hashMove == QPASSMOVE)
        hashMove = ERRMOVE;

      //If aiming for stability, clear hashmoves that from a greater depth that might cause us to act differently
      //since we might not generate such a move if it extends into qsearch
      if(!SearchParams::ALLOW_UNSTABLE && hashDepth > -qDepth)
        hashMove = ERRMOVE;

      //Probably unnecessary (the same way move legality checks for hashmoves are unnecessary)
      //but just in case, repair potential corrupted hashMove.
      hashMove = SearchUtils::removeRedundantSteps(hashMove);
    }
  }

  //Q EVALUATE-------------------------------------
  if(!SearchParams::Q_ENABLE || qDepth >= SearchParams::QDEPTH_EVAL || !params.qEnable)
  {
    if(SearchUtils::isLoseEval(beta))
      return beta;
    if(SearchUtils::isWinEval(alpha))
      return alpha;

    int eval = evaluate(curThread,b,isViewing && params.viewEvaluate);
    mainHash->record(b, cDepth, -qDepth, eval, Flags::FLAG_EXACT, ERRMOVE, true);
    return eval;
  }

  //QSEARCH-------------------------------------------
  int bestIndex = 0;
  eval_t bestEval = Eval::LOSE-1;
  move_t bestMove = ERRMOVE;
  flag_t bestFlag = Flags::FLAG_ALPHA;
  int winDefSteps = 0;
  bool generatedEverything = false;

  //Perform the actual move generation and recursion for the search, returning results
  //in the form of setting the above variables.
  qSearchRecursion(curThread,b,fDepth,cDepth,qDepth,alpha,beta,hashFound,hashMove,
      bestMove,bestEval,bestIndex,bestFlag,winDefSteps,generatedEverything);

  if(curThread->isTerminated)
    return bestEval;

  //No best move - such as if we actually found no defenses in windefsearch
  if(bestMove == ERRMOVE)
  {
    //So just record the result and move on
    mainHash->record(b, cDepth, -qDepth, bestEval, bestFlag, ERRMOVE, true);
  }
  //Otherwise, we actually did a search
  else
  {
    //Record statistics
    curThread->stats.bestMoveSum += bestIndex;
    curThread->stats.bestMoveCount++;

    //Try to extract the whole turn's move for recording hashmoves and killer moves
    move_t moveToRecord = ERRMOVE;
    if(fDepth < SearchParams::PV_ARRAY_SIZE)
      moveToRecord = SearchUtils::extractTurnMoveFromPV(curThread->pv[fDepth], curThread->pvLen[fDepth], b);
    if(moveToRecord == ERRMOVE)
      moveToRecord = bestMove;

    //Perform recording of hash, killers, and history for BETA CUTOFF
    if(bestFlag == Flags::FLAG_BETA)
    {
      mainHash->record(b, cDepth, -qDepth, bestEval, Flags::FLAG_BETA, moveToRecord, true);
      if(SearchParams::QKILLER_ENABLE)
        SearchUtils::recordKiller(curThread->killerMoves, curThread->killerMovesLen, moveToRecord, cDepth);
      SearchUtils::updateHistoryTable(historyTable,historyMax,b,cDepth,bestMove);
    }
    //Perform recording of hash, killers, and history for ALPHA OR EXACT
    else
    {
      //If we didn't find a beta cutoff and we didn't generate all the relevant moves, and we lose,
      //then we promote the eval a little because we don't know if we might have had something to stop the loss.
      //We also want to distrust the result when it indicates a loss far down the horizon, which may result due
      //to repetition fight detection or the hash table unstably pulling in a result from a deeper search. This is
      //because windefsearch only generates shortest defenses, so perhaps a longer defense of the same winthreat would
      //have saved us.
      if(SearchUtils::isLoseEval(bestEval) && !generatedEverything)
      {
        //MINOR Note we ?might? falsely conclude that we're lost in a position where windefsearch generatees
        //a short defense that turns out to be rep-fight-illegal, but a longer defense would have been okay.
        //However, this is really hard. We don't explicitly check for repetition as part of the game end condition
        //check in qsearch, we only do it as part of checking for repetition fight wins when making a *non-qpass* move
        //that *changes the player*.
        //So it can't be the simple case where a short defense followed by a qpass is rep-fight-illegal.
        //It also can't be where a short defense followed by some regular qmoves are all rep fight illegal, because
        //the qpass will get generated and provide an out.
        //So it has to be more like a case where a short defense is generated where a longer one is good, and then
        //a windefsearch is run for a different goal threat that...  hmmm, I don't know, because windefsearch runs to completion
        //for all threats. Maybe this is actually just impossible and we always do the right thing.

        int numStepsLeft = 4-b.step;
        //IMPORTANT: This criterion is also checked the move-loop of qSearchRecursion as a special case where for qdepth0,
        //rather than giving up and just setting LOSE_MAYBE, we actually extend the qsearch.
        //Trust the losing eval if...
        bool shouldTrustLoseEval =
            //The opponent had a win threat and it seems that we lose on the opponent's turn.
            //Windefsearch should be capable of finding a defense in such a case if any exists, relying
            //on the fact that we don't check immos in qsearch and the hardness/impossibility in the minor
            //note above.
            (winDefSteps > 0 && bestEval <= Eval::LOSE + cDepth + numStepsLeft + 4) ||
            //Windefsearch actually generated all possible defenses - so we should trust any result
            //returned, no matter how deep.
            (winDefSteps == numStepsLeft);


        if(!shouldTrustLoseEval)
        {
          //Promote the eval into stating that this position is only maybe a loss
          bestEval = Eval::LOSE_MAYBE;

          //At this qdepth, we've proven that every searched move is a loss for sure, regardless of what
          //alpha and beta were (it can't be the case that some searched move is actually a nonloss,
          //because we know that every move we searched had an eval < beta and therefore
          //was either exact or an upper bound.
          //So at this qdepth, we will always fail to find a move avoiding loss, but we haven't searched everything,
          //so we might as well say that at this depth, the value of this position is EXACTLY Eval::LOSE_MAYBE,
          //and cut off everytime we want to search this position again with any different bounds at this same depth.
          bestFlag = Flags::FLAG_EXACT;

          //And of course, every move seems to be a bad move, so clear out any order suggestions for the move to try.
          bestMove = ERRMOVE;
          moveToRecord = ERRMOVE;

          //Lastly, wipe the principal variation - we don't want to store anything here as a killer move, for example,
          //in the case where maybe losing is better than beta, because there is no single move that actually can cause
          //the beta cutoff or achieve the LOSE_MAYBE score.
          curThread->endPV(fDepth);
        }
      }

      mainHash->record(b, cDepth, -qDepth, bestEval, bestFlag, moveToRecord, true);

      if(bestFlag != Flags::FLAG_ALPHA)
      {
        if(SearchParams::QKILLER_ENABLE)
          SearchUtils::recordKiller(curThread->killerMoves, curThread->killerMovesLen, moveToRecord, cDepth);
        SearchUtils::updateHistoryTable(historyTable,historyMax,b,cDepth,bestMove);
      }
    }
  } //Done recording now


  if(isViewing)
    params.viewFinished(b,b.step == 0,bestEval,0,bestMove,bestFlag,curThread->stats);

  return bestEval;
}

void Searcher::qSearchRecursion(SearchThread* curThread, Board& b, int fDepth, int cDepth, int qDepth,
    eval_t& alpha, eval_t& beta, bool hashFound, move_t hashMove,
    move_t& bestMove, eval_t& bestEval, int& bestIndex, flag_t& bestFlag,
    int& winDefSteps, bool& generatedEverythingBuf)
{
  int moveIndex = 0;

  //Q HASH MOVE AND RECURSE-------------------------------------
  if(hashFound && hashMove != ERRMOVE)
  {
    //Make the move
    int status = tryMove(curThread,hashMove,SearchParams::HASH_SCORE,moveIndex,bestMove,bestEval,bestIndex,bestFlag,b,fDepth,cDepth,qDepth,alpha,beta);
    if(status == TRYMOVE_STOP)
      return;
  }

  //Q KILLER MOVE AND RECURSE-------------------------------------
  KillerEntry killers;
  if(SearchParams::QKILLER_ENABLE)
  {
    SearchUtils::getKillers(killers,curThread->killerMoves,curThread->killerMovesLen,cDepth);

    if(killers.killer[0] == hashMove)
      killers.killer[0] = ERRMOVE;
    if(killers.killer[1] == hashMove || killers.killer[1] == killers.killer[0])
      killers.killer[1] = ERRMOVE;

    //Cannot make a direct qpassmove (and thereby shortcircuit windefsearch) if the opponent
    //is threatening goal or elim.
    //Note that a longer chain of moves ending in a qpassmove is okay, because at worst it will fall into
    //a new qSearch rather than a qSearchQPassed and therefore be windefthreat checked to see if the
    //chain of moves actually defends goal and elim.
    if(killers.killer[0] == QPASSMOVE || killers.killer[1] == QPASSMOVE)
    {
      pla_t opp = gOpp(b.player);
      if(BoardTrees::goalDist(b,opp,4) <= 4 || BoardTrees::canElim(b,opp,4))
      {
        if(killers.killer[0] == QPASSMOVE)
          killers.killer[0] = ERRMOVE;
        if(killers.killer[1] == QPASSMOVE)
          killers.killer[1] = ERRMOVE;
      }
    }

    if(killers.killer[0] != ERRMOVE)
    {
      int status = tryMove(curThread,killers.killer[0],SearchParams::KILLER_SCORE0,moveIndex,bestMove,bestEval,bestIndex,bestFlag,b,fDepth,cDepth,qDepth,alpha,beta);
      if(status == TRYMOVE_STOP)
        return;
    }

    if(killers.killer[1] != ERRMOVE)
    {
      int status = tryMove(curThread,killers.killer[1],SearchParams::KILLER_SCORE1,moveIndex,bestMove,bestEval,bestIndex,bestFlag,b,fDepth,cDepth,qDepth,alpha,beta);
      if(status == TRYMOVE_STOP)
        return;
    }
  }

  //Q MOVES-------------------------------------
  MoveListAcquirer moveListAq(curThread->mvList,curThread->hmList,curThread->mvListCapacity,curThread->mvListCapacityUsed);
  move_t* mv = moveListAq.mv;
  int* hm = moveListAq.hm;

  //Q GOAL THREAT PRUNING----------------------
  int winDefSearchInteriorNodes = 0;
  //We used to limit the winDefSearch's max depth using beta, but that creates a major search
  //instability when you have a beta such that you fail to find a way the opponent can threaten
  //you any faster, so you don't conclude a loss, then you generate normal moves, including qpass,
  //and then store those results in the hash, later trusting them when beta is different and
  //concluding a lost position isn't lost.
  int num = SearchMoveGen::genShortestFullWinDefMoves(b,mv,winDefSearchInteriorNodes,winDefSteps);

  //Expensive, but not counted in our node count, so add it manually.
  curThread->stats.qNodes += winDefSearchInteriorNodes;
  //And also advance the time check counter so we keep to desired time more tightly
  curThread->timeCheckCounter += winDefSearchInteriorNodes;

  if(num == 0)
  {
    //Assume we lose in two turns - we move, opp makes a winning reply
    eval_t loseEval = Eval::LOSE + cDepth + (8-b.step);
    bestEval = loseEval;
    bestFlag = Flags::FLAG_EXACT;
    return;
  }

  //Win def search moves generated
  bool winDefActive = num > 0;
  if(winDefActive)
  {
    //Fill up hm array because windefsearch won't.
    //TODO add some move ordering here? History ordering is also done below, but maybe some extra ordering here could help
    for(int i = 0; i<num; i++)
      hm[i] = 0;
  }
  else
  {
    //Clear out the num = -1 that winDefSearch put when failing to find any opp threat
    num = 0;
  }

  //Q MAIN QUIESCENCE MOVE GEN-----------------------------------
  //Run the main qsearch if we're also generating qsearch moves when the opponent has a win threat,
  //or if there was no win threat.
  if(!winDefActive || (SearchParams::ALSOQ_ENABLE && b.step < 3))
  {
    int newNum = SearchMoveGen::genQuiescenceMoves(b,curThread->boardHistory,cDepth,qDepth,mv+num,hm+num);
    if(!winDefActive)
      num += newNum;
    else
    {
      SearchMoveGen::RelevantArea relArea;
      bool anyThreat = SearchMoveGen::getWinDefRelArea(b,relArea);
      DEBUGASSERT(anyThreat);
      int oldNum = num;
      for(int i = 0; i < newNum; i++)
      {
        move_t move = mv[i+oldNum];
        if(move == QPASSMOVE || move == PASSMOVE)
          continue;
        if(isSingleStepOrError(move))
        {
          step_t step = getStep(move,0);
          loc_t src = gSrc(step);
          loc_t dest = gDest(step);
          //FIXME fragile
          if(!relArea.stepFrom.isOne(src) && !relArea.stepTo.isOne(dest))
            continue;
        }
        else
        {
          step_t step0 = getStep(move,0);
          step_t step1 = getStep(move,1);
          loc_t src = gSrc(step1);
          loc_t dest = gSrc(step0);
          loc_t dest2 = gDest(step0);
          //FIXME broken
          Bitmap oppInvolved = relArea.pullOpp | relArea.pushOpp;
          //Note that this isn't quite right because genQuiescenceMoves might generate multistep moves that
          //arent pushes and pulls, but it should be close enough.
          if(!oppInvolved.isOne(src) && !oppInvolved.isOne(dest) && !oppInvolved.isOne(dest2))
            continue;
        }
        mv[num] = move;
        hm[num] = hm[i+oldNum];
        num++;
      }
    }
  }

  //Report move list usage out acquirer object
  moveListAq.reportUsage(num);

  //Q MOVE ORDERING------------------------------
  if(SearchParams::HISTORY_ENABLE)
    SearchUtils::getHistoryScores(historyTable,historyMax,b,b.player,cDepth,mv,hm,num);

  //Disabled - doesn't really seem to help or hurt
  //if(killerMove1 != ERRMOVE && !isSingleStepOrError(killerMove1))
  //  SearchUtils::setOrderingForLongestStrictPrefix(killerMove1,mv,hm,num,SearchParams::KILLER1_PREFIX_SCORE);
  //if(killerMove0 != ERRMOVE && !isSingleStepOrError(killerMove0))
  //  SearchUtils::setOrderingForLongestStrictPrefix(killerMove0,mv,hm,num,SearchParams::KILLER0_PREFIX_SCORE);
  //if(hashMove != ERRMOVE && !isSingleStepOrError(hashMove))
  //  SearchUtils::setOrderingForLongestStrictPrefix(hashMove,mv,hm,num,SearchParams::HASH_MOVE_PREFIX_SCORE);

  //QSEARCH! ------------------------------------------------
  bool generatedEverything = false;
  for(int m = 0; m < num; m++)
  {
    //Sort the next best move to the top
    SearchUtils::selectBest(mv,hm,num,m);
    move_t move = mv[m];

    //Don't search the hashed move or killers twice
    if(!(move == hashMove || move == killers.killer[0] || move == killers.killer[1]))
    {
      int status = tryMove(curThread,move,hm[m],moveIndex,bestMove,bestEval,bestIndex,bestFlag,b,fDepth,cDepth,qDepth,alpha,beta);

      if(status == TRYMOVE_ERR)
      {ONLYINDEBUG({SearchUtils::reportIllegalMove(b,move);}) continue;}
      else if(status == TRYMOVE_STOP)
        return;
    }

    //MINOR is there a way to clean this up? It's a bit messy
    //IMPORTANT: This is basically the same criterion as checked above in qSearch that we use to set LOSE_MAYBE.
    //If changing this, consider changing it above too.
    //At QDepth0, if every move looks like a loss, but a loss too deep for us to be sure we've generated
    //all defenses, then extend the search and generate every step.
    if(m == num-1 && qDepth == 0 && !generatedEverything && SearchUtils::isLoseEval(bestEval))
    {
      int numStepsLeft = 4-b.step;
      bool shouldTrustLoseEval =
          (winDefSteps > 0 && bestEval <= Eval::LOSE + cDepth + numStepsLeft + 4) ||
          (winDefSteps == numStepsLeft);
      if(!shouldTrustLoseEval)
      {
        generatedEverything = true;
        generatedEverythingBuf = true;
        int oldNum = num;
        if(b.step < 3)
          num += BoardMoveGen::genPushPulls(b,b.player,mv+num);
        num += BoardMoveGen::genSteps(b,b.player,mv+num);
        //No need to generate the pass move since either the opp has a goal threat, or we added a qpass move
        //via normal qsearch movegen

        for(int i = oldNum; i<num; i++)
          hm[i] = 0;
        if(SearchParams::HISTORY_ENABLE)
          SearchUtils::getHistoryScores(historyTable,historyMax,b,b.player,cDepth,mv+oldNum,hm+oldNum,num-oldNum);
        moveListAq.reportUsage(num);
      }
    }
  }
}

//TRYMOVE---------------------------------------------------------------------------------

eval_t Searcher::callNextQSearch(SearchThread* curThread, move_t move, int numSteps, bool changedPlayer,
    Board& b, int fDepth, int cDepth, int rDepth4, eval_t alpha, eval_t beta)
{
  //Compute new alpha and beta bounds given bonus for passing
  int newAlpha = alpha;
  int newBeta = beta;
  int bonus = 0;
  int qDepth = rDepth4;

  bool qpassed = false;
  if(b.step == 0)
  {
    int passedSteps = 0;
    for(int s = 0; s<4; s++)
    {
      step_t step = getStep(move,s);
      if(step == PASSSTEP || step == QPASSSTEP)
      {passedSteps = numSteps - s; if(step == QPASSSTEP) qpassed = true; break;}
      if(step == ERRSTEP)
        break;
    }
    if(passedSteps > 0)
    {
      bonus = SearchParams::Q0PASS_BONUS[passedSteps];
      newAlpha = SearchUtils::isTerminalEval(alpha) ? alpha : alpha - bonus;
      newBeta = SearchUtils::isTerminalEval(beta) ? beta : beta - bonus;
    }
  }

  //Compute next qdepth
  curThread->stats.qNodes++;
  int nextQDepth = qDepth;
  if(changedPlayer)
    nextQDepth = qpassed ? SearchParams::QDEPTH_NEXT_IF_QPASSED[qDepth] : SearchParams::QDEPTH_NEXT[qDepth];

  //Recurse!
  eval_t eval;
  if(changedPlayer) {eval = -qSearch(curThread,b,fDepth+1,cDepth+numSteps,nextQDepth,-newBeta,-newAlpha,SS_Q);}
  else              {eval =  qSearch(curThread,b,fDepth+1,cDepth+numSteps,nextQDepth, newAlpha, newBeta,SS_Q);}

  //Adjust for passing bonus
  if(!SearchUtils::isTerminalEval(eval))
    eval += bonus;
  return eval;
}

//Make and undo move, recurse, return status and evaluation, record PV, and record stats
//Doesn't record hash or push or pop situations
//Returns true if we should keep searching, false if we shouldn't, because of a beta cutoff
//or because we got terminated.
int Searcher::tryMove(SearchThread* curThread, move_t move, int hm, int& moveIndex,
  move_t& bestMove, eval_t& bestEval, int& bestIndex, flag_t& bestFlag,
  Board& b, int fDepth, int cDepth, int rDepth4, eval_t& alpha, eval_t& beta)
{
  //Eval of this move - to be computed
  //FIXME - it's really strange that after you store a killer that ends in a qpass,
  //each time when you apply the killer you actually will make the pass on-board
  //as opposed to the initial search that produced the killer where you just evaluate as-is
  //because of qSearchQPassed.
  eval_t eval;
  if(move == QPASSMOVE && SearchParams::QDEPTH_NEXT_IF_QPASSED[rDepth4] == SearchParams::QDEPTH_EVAL)
    eval = qSearchQPassed(curThread,b,fDepth+1,cDepth,SearchParams::QDEPTH_EVAL,alpha,beta);
  else
  {
    //Attempt move
    int oldBoardStep = b.step;
    bool suc = b.makeMoveLegal(move,curThread->undoData);
    if(!suc) return TRYMOVE_ERR;

    //In the qsearch, allow returning to the same position,
    //so that we can be less strict about including the turn start position in the hash
    //in case we transpose partially into the turn.

    //Compute new basic params
    bool changedPlayer = b.step == 0;
    int numSteps = changedPlayer ? 4-oldBoardStep : b.step-oldBoardStep;

    //Check repetition-related conditions
    bool illegalRepetition = false;
    int turnsToLoseByRep = 0;
    if(changedPlayer)
    {
      if(b.posCurrentHash == b.posStartHash)
      {illegalRepetition = true; turnsToLoseByRep = 0;}
      else if(SearchParams::ENABLE_REP_FIGHT_CHECK &&
          SearchUtils::losesRepetitionFight(b,curThread->boardHistory,cDepth+numSteps,turnsToLoseByRep))
      {illegalRepetition = true;}
    }

    //Ban the repetition if our move didn't include a qpassstep.
    //If our move did include one, we allow it, treating a qpassstep as explicit license to repeat.
    if(illegalRepetition && getStep(move,numStepsInMove(move)-1) != QPASSSTEP)
    {
      //Add 4 to conform with winloss distance always assuming that you can't lose after having
      //made a move - you can only lose at the end of the opponent's turn.
      eval = Eval::LOSE + cDepth + numSteps + turnsToLoseByRep*4 + 4;
      curThread->endPV(fDepth+1);
    }
    //All good, just recurse
    else
    {
      curThread->boardHistory.reportMoveNoTurnBoard(b,move,oldBoardStep);
      eval = callNextQSearch(curThread, move, numSteps, changedPlayer, b, fDepth, cDepth, rDepth4, alpha, beta);

      //If we got a winning eval but there was an illegal repetition, we might be in zugzwang, so demote the win
      //into only a maybe win.
      if(illegalRepetition && SearchUtils::isWinEval(eval))
        eval = Eval::WIN_MAYBE;
    }
    b.undoMove(curThread->undoData);
  }

  if(params.viewing(b,curThread->boardHistory))
    params.viewMove(b,move,hm,eval,bestEval,false,0,curThread->getPV(fDepth+1),curThread->getPVLen(fDepth+1),curThread->stats);

  //Terminated
  if(curThread->isTerminated)
    return TRYMOVE_STOP;

  //Beta cutoff
  if(eval >= beta)
  {
    curThread->copyExtendPVFromNextFDepth(move, fDepth);

    //Update statistics
    curThread->stats.betaCuts++;

    //Done
    bestIndex = moveIndex;
    bestMove = move;
    bestEval = eval;
    bestFlag = Flags::FLAG_BETA;
    moveIndex++;
    return TRYMOVE_STOP;
  }

  //Update best and alpha
  if(eval > bestEval)
  {
    bestMove = move;
    bestEval = eval;
    bestIndex = moveIndex;

    if(eval > alpha)
    {
      alpha = eval;
      bestFlag = Flags::FLAG_EXACT;
    }
    curThread->copyExtendPVFromNextFDepth(move, fDepth);
  }

  moveIndex++;
  return TRYMOVE_CONT;
}


//Pruning thresholds
static const double fancyThresholdPVS = 0.010;
static const int fancyBaseThresholdPVS = 4;
static const int fancyDepthArrayMax = 16;
static const double fancyThresholdR1Prop[fancyDepthArrayMax+1] = {
1, 1,1,1,1, 0.135,0.125,0.120,0.115, 0.110,0.105,0.100,0.095, 0.095,0.095,0.095,0.095
};
static const double fancyThresholdR2Prop[fancyDepthArrayMax+1] = {
1, 1,1,1,1, 1.000,0.325,0.300,0.275, 0.255,0.245,0.235,0.225, 0.220,0.220,0.220,0.220
};
static const double fancyThresholdR3Prop[fancyDepthArrayMax+1] = {
1, 1,1,1,1, 1.000,1.000,0.900,0.850, 0.800,0.800,0.800,0.750, 0.750,0.750,0.750,0.750
};

void Searcher::genFSearchMoves(SplitPoint* spt, bool briefLastDepth)
{
  DEBUGASSERT(spt->cDepth == 0);
  DEBUGASSERT(spt->numMoves == 0);
  DEBUGASSERT(spt->areMovesUngen);
  move_t* mv = spt->mv;
  int* hm = spt->hm;
  int numMoves = fullMoves.size();
  for(int i = 0; i<numMoves; i++)
  {
    mv[i] = fullMoves[i].move;
    //Set in anticipation of the search filling in hm for the root moves that are found to improve alpha
    //for later sorting. Not actually used to order the root moves in the normal way, since they're already sorted.
    hm[i] = Eval::LOSE-1;
  }

  //BT PRUNING AND PVS------------------------------------------------------------------------
  if(SearchParams::PVS_ENABLE)
  {
    spt->pvsThreshold = (int)(fancyThresholdPVS * numMoves) + fancyBaseThresholdPVS;
  }
  if(params.rootMoveFeatureSet.fset != NULL || params.stupidPrune)
  {
    if(params.rootFancyPrune)
    {
      int d = spt->rDepth4/SearchParams::DEPTH_DIV;
      if(d > fancyDepthArrayMax)
        d = fancyDepthArrayMax;
      spt->r1Threshold = (int)(fancyThresholdR1Prop[d] * numMoves)+params.rootMinDontPrune;
      spt->r2Threshold = (int)(fancyThresholdR2Prop[d] * numMoves)+params.rootMinDontPrune;
      spt->r3Threshold = (int)(fancyThresholdR3Prop[d] * numMoves)+params.rootMinDontPrune;
    }
    else if(params.rootFixedPrune)
    {
      if(params.rootPruneSoft)
        spt->r1Threshold = (int)(params.rootPruneAllButProp * max(0,numMoves-params.rootMinDontPrune))+params.rootMinDontPrune;
      else
        spt->pruneThreshold = (int)(params.rootPruneAllButProp * max(0,numMoves-10))+params.rootMinDontPrune;
    }
  }

  if(briefLastDepth)
  {
    spt->pruneThreshold = numMoves/500+10;
  }
  spt->numMoves += numMoves;
  spt->areMovesUngen = false;
}

void Searcher::genMSearchMoves(SearchThread* curThread, SplitPoint* spt)
{
  //Pull out parameters
  Board& b = curThread->board;
  pla_t pla = b.player;
  int cDepth = spt->cDepth;
  int rDepth4 = spt->rDepth4;
  int numMovesAlready = spt->numMoves;
  move_t* mv = spt->mv + numMovesAlready;
  int* hm = spt->hm + numMovesAlready;

  //BEGIN SWITCH----------------------------------------------------------
  int num = 0;
  int& moveGenCallState = spt->moveGenCallState;
  switch(moveGenCallState)
  {
  default:DEBUGASSERT(false)
  //CAP MOVE GENERATION---------------------------------------------------
  case 0:
  if(b.step < 3)
    num = SearchMoveGen::genCaptureMoves(b,4-b.step,mv,hm);
  if(num > 0)
  {
    for(int i = 0; i<num; i++)
      hm[i] = SearchParams::CAPTURE_SCORE + hm[i] * SearchParams::CAPTURE_SCORE_PER_MATERIAL;
    moveGenCallState = 1;
    spt->numMoves += num;
    return;
  }
  case 1:
  {
    int numStepsLeft = 4-b.step;
    num = SearchMoveGen::genWinDefMovesIfNeeded(b,mv,hm,numStepsLeft);
    if(num >= 0)
      spt->moveGenWinDefGen = true;
    else
    {
      num = SearchMoveGen::genRelevantTactics(b,false,mv,hm);
      //Generate the pass move nonreduced so that we aren't forced to make a step if we don't want to get reduced
      if(b.step != 0 && b.posCurrentHash != b.posStartHash)
      {mv[num] = PASSMOVE; hm[num] = 0; num++;}
    }
    if(SearchParams::HISTORY_ENABLE)
    {
      SearchUtils::getHistoryScores(historyTable,historyMax,b,pla,cDepth,mv,hm,num);
      for(int i = 0; i<num; i++)
        hm[i] += numStepsInMove(mv[i]);
    }

    moveGenCallState = 2;
    spt->numMoves += num;
    return;
  }

  //REGULAR MOVE GENERATION------------------------------------------------
  case 2:
  {
    //FIXME with reductions in existence, think about this - we could fall into qsearch faster
    //than we expect, the current behavior is probably nonsense because we're guaranteed
    //to reduce all these moves by 1

    //If on this turn we're likely to fall into qsearch, call the full move generation
    //because it might make good pre-steps before letting the qsearch generate the windefense
    //that the qsearch wouldn't know to do
    if(rDepth4 < 16 || !spt->moveGenWinDefGen)
      num = SearchMoveGen::genRegularMoves(b,mv);
  }

  //MOVE ORDERING------------------------------
  static const int TRAPGUARDBONUS[5] = {0,4,0,0,0};
  static const int TRAPREMOVEBONUS[5] = {0,3,4,2,0};
  pla_t opp = gOpp(pla);
  move_t hashMovePrefix = SearchUtils::pruneToRegularMove(b,spt->hashMove);
  if(hashMovePrefix == spt->hashMove)
    hashMovePrefix = ERRMOVE;
  for(int i = 0; i<num; i++)
  {
    move_t move = mv[i];
    int bonus = 0;
    if(move == hashMovePrefix)
    {
      hm[i] = SearchParams::HASH_MOVE_PREFIX_SCORE;
      continue;
    }
    for(int j = 0; j<4; j++)
    {
      step_t step = getStep(move,j);
      if(step == ERRSTEP || step == PASSSTEP || step == QPASSSTEP)
        break;
      loc_t src = gSrc(step);
      loc_t dest = gDest(step);
      if(b.owners[src] == pla)
      {
        if(Board::ADJACENTTRAPINDEX[dest] != -1)
        {
          int tgCount = b.trapGuardCounts[pla][Board::ADJACENTTRAPINDEX[dest]];
          bonus += TRAPGUARDBONUS[tgCount];
        }
        //bonus += (6-Board::CENTRALITY[dest]);
      }
      else if(b.owners[src] == opp)
      {
        bonus += 1;
        if(Board::ADJACENTTRAPINDEX[src] != -1)
        {
          int tgCount = b.trapGuardCounts[opp][Board::ADJACENTTRAPINDEX[src]];
          bonus += TRAPREMOVEBONUS[tgCount];
          if(tgCount == 1 && Board::ISTRAP[dest])
            bonus += SearchParams::CAPTURE_SCORE + b.pieces[dest] * SearchParams::CAPTURE_SCORE_PER_MATERIAL;
          else if(tgCount == 1 && b.owners[Board::ADJACENTTRAP[src]] == opp)
            bonus += SearchParams::CAPTURE_SCORE + b.pieces[Board::ADJACENTTRAP[src]] * SearchParams::CAPTURE_SCORE_PER_MATERIAL;
        }
      }
    }

    hm[i] = bonus;
  }

  //TODO apply PVS to the reltactics moves?
  //TODO try seeing if loosening or tightening the threshold helps
  //and also try to see if basing it off of the number of non-illegal moves before
  //rather than a straight move index helps.
  //If there will be a turn-swap in main search, then try pvs
  //if(rDepth4 >= (4 - b.step) * SearchParams::DEPTH_DIV)
  //  spt->pvsThreshold = numMovesAlready+2;
  spt->areMovesUngen = false;
  spt->r1Threshold = numMovesAlready;

  }//END SWITCH

  if(SearchParams::HISTORY_ENABLE)
  {
    SearchUtils::getHistoryScores(historyTable,historyMax,b,pla,cDepth,mv,hm,num);
    for(int i = 0; i<num; i++)
      hm[i] += numStepsInMove(mv[i]);
  }

  spt->numMoves += num;
}

void Searcher::mainCheckTime()
{
  double usedTime = clockTimer.getSeconds();
  timeLock->lock();
  if(searchId == interruptedId || (okayToTimeout && usedTime > clockDesiredTime))
    didInterruptOrTimeout = true;
  timeLock->unlock();
}

void Searcher::tryCheckTime(SearchThread* curThread)
{
  curThread->timeCheckCounter++;
  if(curThread->timeCheckCounter >= SearchParams::TIME_CHECK_PERIOD)
  {
    curThread->timeCheckCounter = 0;
    double usedTime = clockTimer.getSeconds();
    timeLock->lock();
    if(didInterruptOrTimeout)
      curThread->isTerminated = true;
    else if(searchId == interruptedId || (okayToTimeout && usedTime > clockDesiredTime))
    {
      didInterruptOrTimeout = true;
      curThread->isTerminated = true;
    }
    timeLock->unlock();
  }
}

void Searcher::refreshDesiredTimeNoLock()
{
  bool baseTerminal = SearchUtils::isTerminalEval(clockBaseEval);
  bool currentTerminal = SearchUtils::isTerminalEval(clockBestEval);
  double minTime = min(tcMinTime,hardTimeCap);
  double maxTime = min(tcMaxTime,hardTimeCap);

  //If our current best eval is a loss, or the minimal time to search is the maximal time
  //to search, then search as long as we can
  if(minTime >= maxTime || (clockBestEval < 0 && currentTerminal))
  {clockDesiredTime = maxTime; return;}

  double factor = 1.0;

  //Either eval is terminal - search the basic amount of time.
  if(baseTerminal || currentTerminal)
  {}
  else
  {
    //Compare our eval to the eval on the previous iteration
    eval_t evalDiff = clockBestEval - clockBaseEval;

    //Search longer if we're down, less if we're not.
    if(evalDiff > -300)
    {}
    else if(evalDiff > -600)  {factor *= 1.0 + (-300-evalDiff)/300.0 * 0.1;} //Linear 1.0 -> 1.1
    else if(evalDiff > -1300) {factor *= 1.1 + (-600-evalDiff)/350.0 * 0.1;} //Linear 1.1 -> 1.3
    else if(evalDiff > -2200) {factor *= 1.3 + (-1300-evalDiff)/450.0 * 0.1;} //Linear 1.3 -> 1.5
    else                      {factor *= 1.5;}
  }

  int movesTotal = clockNumMovesTotal;
  int movesDone = clockNumMovesDone;
  if(movesTotal > 0 && movesDone > 0)
  {
    //Based on number of moves finished
    if     (movesDone <=  1 + movesTotal *   1/2048) factor *= 1.350;
    else if(movesDone <=  2 + movesTotal *   5/8192) factor *= 1.310;
    else if(movesDone <=  3 + movesTotal *   3/4096) factor *= 1.270;
    else if(movesDone <=  4 + movesTotal *   7/8192) factor *= 1.240;
    else if(movesDone <=  5 + movesTotal *   1/1024) factor *= 1.210;
    else if(movesDone <=  6 + movesTotal *   3/2048) factor *= 1.185;
    else if(movesDone <=  7 + movesTotal *   2/1024) factor *= 1.160;
    else if(movesDone <=  8 + movesTotal *   3/1024) factor *= 1.140;
    else if(movesDone <=  9 + movesTotal *   5/1024) factor *= 1.115;
    else if(movesDone <= 10 + movesTotal *   8/1024) factor *= 1.090;
    else if(movesDone <= 11 + movesTotal *  16/1024) factor *= 1.060;
    else if(movesDone <= 12 + movesTotal *  32/1024) factor *= 1.040;
    else if(movesDone <= 13 + movesTotal *  64/1024) factor *= 1.025;
    else if(movesDone <= 14 + movesTotal * 128/1024) factor *= 1.015;
    else if(movesDone <= 15 + movesTotal * 256/1024) factor *= 1.010;
    else if(movesDone <= 16 + movesTotal * 512/1024) factor *= 1.005;
  }

  //Based on depth
  int depth = currentIterDepth;
  if(depth <= 5)       factor *= 1.22;
  else if(depth <= 6)  factor *= 1.16;
  else if(depth <= 7)  factor *= 1.09;
  else if(depth <= 8)  factor *= 1.08;
  else if(depth <= 9)  factor *= 1.02;
  else if(depth <= 10) factor *= 1.0;
  else if(depth <= 11) factor *= 0.98;
  else                 factor *= 0.98;

  //Plain overall scaling to tune aggressiveness of time use
  factor *= 0.96;

  double desiredTime = tcNormalTime * factor;
  if(desiredTime < minTime)
    clockDesiredTime = minTime;
  else if(desiredTime > maxTime)
    clockDesiredTime = maxTime;
  else
    clockDesiredTime = desiredTime;
}

void Searcher::updateDesiredTime(eval_t bestEval, int movesDone, int movesTotal)
{
  timeLock->lock();
  clockBestEval = bestEval;
  clockNumMovesDone = movesDone;
  clockNumMovesTotal = movesTotal;

  //If we've searched at least about 20% of the moves on the first iteration, then we're okay
  //to timeout, our move will probably not be too terrible if we play now.
  //okayToTimeout will remain true through future iterations even if we keep setting it here.
  if(currentIterDepth > 0 && (movesDone >= movesTotal || movesDone >= movesTotal / 5 + 10))
    okayToTimeout = true;

  refreshDesiredTimeNoLock();

  timeLock->unlock();
}

double Searcher::currentTimeUsed()
{
  return clockTimer.getSeconds();
}

double Searcher::currentDesiredTime()
{
  return clockDesiredTime;
}


move_t Searcher::getRootMove(move_t* mv, int* hm, int numMoves, int idx)
{
  (void)hm;
  DEBUGASSERT(idx < numMoves);
  return mv[idx];
}

move_t Searcher::getMove(move_t* mv, int* hm, int numMoves, int idx)
{
  DEBUGASSERT(idx < numMoves);
  SearchUtils::selectBest(mv,hm,numMoves,idx);
  return mv[idx];
}

//HELPERS - EVALUATION----------------------------------------------------------------------
#include "../board/locations.h"

eval_t Searcher::evaluate(SearchThread* curThread, Board& b, bool print)
{
  curThread->stats.evalCalls++;

  //Override the mainpla used for special early eval and asymmetric eval
  pla_t mPla = params.overrideMainPla ? params.overrideMainPlaTo : mainPla;

  eval_t earlyBlockadePenalty = 0;
  if(params.avoidEarlyBlockade && mainBoard.turnNumber <= SearchParams::EARLY_BLOCKADE_TURN_HALF && mPla != NPLA)
  {
    earlyBlockadePenalty = SearchParams::EARLY_BLOCKADE_PENALTY;
    //If the mainboard's elephant is very centralized, so almost certainly not already in a blockade, bump it up a bit more
    if((mainBoard.pieceMaps[mPla][ELE] & Bitmap(0x00003C3C3C3C0000ULL)).hasBits())
      earlyBlockadePenalty += SearchParams::EARLY_CENTER_BLOCKADE_PENALTY;
    //If we're past the earliest part of the game, reduce the penalty
    if(mainBoard.turnNumber > SearchParams::EARLY_BLOCKADE_TURN_FULL)
      earlyBlockadePenalty /= 2;
  }

  eval_t eval = Eval::evaluate(b,mPla,earlyBlockadePenalty,(print ? params.output : NULL));

  if(params.avoidEarlyTrade && mainBoard.turnNumber <= SearchParams::EARLY_TRADE_TURN_MAX && mPla != NPLA)
  {
    //If the mPla lost a piece, penalize the mPla for doing so
    if(b.pieceCounts[mPla][0] != mainBoard.pieceCounts[mPla][0])
    {
      if(mPla == b.player) eval -= SearchParams::EARLY_TRADE_PENALTY;
      else eval += SearchParams::EARLY_TRADE_PENALTY;
    }
  }

  if(params.avoidEarlyTrade && mainBoard.turnNumber <= SearchParams::EARLY_HORSE_ATTACKED_MAX)
  {
    if((mPla == GOLD && ((b.owners[B3] == SILV && b.pieces[B3] == HOR) || (b.owners[G3] == SILV && b.pieces[G3] == HOR))) ||
       (mPla == SILV && ((b.owners[B6] == GOLD && b.pieces[B6] == HOR) || (b.owners[G6] == GOLD && b.pieces[G6] == HOR))))
    {
      if(mPla == b.player) eval -= SearchParams::EARLY_HORSE_ATTACKED_PENALTY;
      else eval += SearchParams::EARLY_HORSE_ATTACKED_PENALTY;
    }
  }

  if(params.randomize && params.randDelta > 0)
  {
    int d = params.randDelta*2+1;
    int d2 = d*d;

    uint64_t randSeed = params.randSeed;
    uint64_t hash = b.sitCurrentHash;
    hash = hash * 0xC849B09232AFC387ULL;
    hash ^= (hash >> 32) & 0x00000000FFFFFFFFULL;
    hash += randSeed;
    hash ^= 0x7331F00DCAFEBABEULL;
    hash = hash * 0x25AFE4F5678219C1ULL;
    hash ^= (hash >> 32) & 0x00000000FFFFFFFFULL;

    int randvalues = hash % d2;
    int randvalues2 = ((hash >> 32) & 0x00000000FFFFFFFFULL) % d;
    eval += (randvalues % d)-params.randDelta;
    eval += (randvalues/d)-params.randDelta;
    eval += randvalues2-params.randDelta;
  }

  return eval;
}

//Helpers-----------------------------------------------------------------------

//Higher rated moves come earlier in the ordering
bool RatedMove::operator<(const RatedMove& other) const  {return val >  other.val;}
bool RatedMove::operator>(const RatedMove& other) const  {return val <  other.val;}
bool RatedMove::operator<=(const RatedMove& other) const {return val >= other.val;}
bool RatedMove::operator>=(const RatedMove& other) const {return val <= other.val;}




