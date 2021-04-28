/*
 * searchparams.cpp
 * Author: davidwu
 */

#include <cstdio>
#include "../core/global.h"
#include "../learning/learner.h"
#include "../search/searchflags.h"
#include "../search/searchparams.h"
#include "../search/searchutils.h"
#include "../search/searchstats.h"

using namespace std;

SearchParams::SearchParams()
:name("SearchParams")
{
  defaultMaxDepth = AUTO_DEPTH;
  defaultMaxTime = AUTO_TIME;
  init();
}

SearchParams::SearchParams(int depth, double time)
:name("SearchParamsD" + Global::intToString(depth) + "T" + Global::doubleToString(time))
{
  defaultMaxDepth = depth;
  defaultMaxTime = time;
  init();
}

SearchParams::SearchParams(string nm)
:name(nm)
{
  defaultMaxDepth = AUTO_DEPTH;
  defaultMaxTime = AUTO_TIME;
  init();
}

SearchParams::SearchParams(string nm, int depth, double time)
:name(nm)
{
  defaultMaxDepth = depth;
  defaultMaxTime = time;
  init();
}

void SearchParams::init()
{
  randomize = false;
  randDelta = 0;
  randSeed = 0;
  numThreads = 1;

  output = &cout;

  disablePartialSearch = false;
  moveImmediatelyIfOnlyOneNonlosing = false;
  avoidEarlyTrade = false;
  avoidEarlyBlockade = false;
  overrideMainPla = false;
  overrideMainPlaTo = NPLA;
  allowReduce = true;

  fullMoveHashExp = DEFAULT_FULLMOVE_HASH_EXP;
  mainHashExp = DEFAULT_MAIN_HASH_EXP;

  qEnable = true;
  extensionEnable = true;

  rootMoveFeatureSet = ArimaaFeatureSet();
  rootMinDontPrune = 10;
  rootFancyPrune = false;
  rootFixedPrune = false;
  rootPruneAllButProp = 1.0;
  rootPruneSoft = false;
  stupidPrune = false;
  rootBiasStrength = DEFAULT_ROOT_BIAS;
  rootMaxNonRelTactics = 4;
  treePrune = false;
  unsafePruneEnable = false;
  nullMoveEnable = false;

  viewOn = false;
  viewPrintMoves = false;
  viewPrintBetterMoves = false;
  viewEvaluate = false;

}

SearchParams::~SearchParams()
{
}

//Threading-------------------------

//How many parallel threads to use for the search?
void SearchParams::setNumThreads(int num)
{
  if(num <= 0 || num > MAX_THREADS)
    Global::fatalError("Invalid number of threads: " + Global::intToString(num));
  numThreads = num;
}

//Output-----------------------------

void SearchParams::setOutput(ostream* out)
{
  if(out == NULL)
    Global::fatalError("Set searchparams output to NULL");
  output = out;
}

//Randomization--------------------

//Do we randomize the pv among equally strong moves? And do we add any deltas to the evals?
void SearchParams::setRandomize(bool r, eval_t rDelta, uint64_t seed)
{
  if(rDelta < 0 || rDelta > 16383)
    Global::fatalError(string("Invalid rDelta: ") + Global::intToString(rDelta));

  randomize = r;
  randDelta = rDelta;
  randSeed = seed;
}

void SearchParams::setRandomSeed(uint64_t seed)
{
  randSeed = seed;
}


//Move feature ordering ---------------------------

//Add features and weights for root move ordering
void SearchParams::initRootMoveFeatures(const BradleyTerry& bt)
{
  rootMoveFeatureSet = bt.afset;
  rootMoveFeatureWeights = bt.logGamma;
}

//Use root move ordering for pruning too. Prune after the first prop of moves
void SearchParams::setRootFixedPrune(bool b, double prop, bool soft)
{
  if(rootMoveFeatureSet.fset == NULL && b == true)
    Global::fatalError("Tried to set root fixed pruning without feature set!");
  stupidPrune = false;
  rootFixedPrune = b;
  rootFancyPrune = false;
  rootPruneSoft = soft;
  rootPruneAllButProp = prop;
}

void SearchParams::setStupidPrune(bool b, double prop)
{
  stupidPrune = b;
  rootFixedPrune = b;
  rootFancyPrune = false;
  rootPruneSoft = false;
  rootPruneAllButProp = prop;
}

//Use root move ordering for fancier pruning
void SearchParams::setRootFancyPrune(bool b)
{
  if(rootMoveFeatureSet.fset == NULL && b == true)
    Global::fatalError("Tried to set root fancy pruning without feature set!");
  stupidPrune = false;
  rootFixedPrune = false;
  rootFancyPrune = b;
}

void SearchParams::setRootBiasStrength(double bias)
{
  rootBiasStrength = bias;
}

void SearchParams::setUnsafePruneEnable(bool b)
{
  unsafePruneEnable = b;
}

void SearchParams::setNullMoveEnable(bool b)
{
  nullMoveEnable = b;
}

void SearchParams::setExtensionEnable(bool b)
{
  extensionEnable = b;
}

//Search options ------------------------------

void SearchParams::setAvoidEarlyTrade(bool avoid)
{
  avoidEarlyTrade = avoid;
}

void SearchParams::setAvoidEarlyBlockade(bool avoid)
{
  avoidEarlyBlockade = avoid;
}

void SearchParams::setOverrideMainPla(bool override, pla_t to)
{
  overrideMainPla = override;
  overrideMainPlaTo = to;
}

void SearchParams::setAllowReduce(bool allow)
{
  allowReduce = allow;
}

void SearchParams::setDefaultMaxDepthTime(int depth, double time)
{
  defaultMaxDepth = depth;
  defaultMaxTime = time;
}

void SearchParams::setDisablePartialSearch(bool b)
{
  disablePartialSearch = b;
}

void SearchParams::setMoveImmediatelyIfOnlyOneNonlosing(bool b)
{
  moveImmediatelyIfOnlyOneNonlosing = b;
}

//VIEW--------------------------------------------------------------------------------------

void SearchParams::initView(const Board& b, const string& moveStr, bool printBetterMoves, bool printMoves, bool exactHash)
{
  ostream& out = *output;
  viewOn = true;
  viewStartBoard = b;
  viewHashes.clear();
  out << "---VIEWING---------------------- " << endl;

  vector<move_t> moves = Board::readMoveSequence(moveStr);
  for(int i = 0; i<(int)moves.size(); i++)
  {
    if(exactHash)
      viewHashes.push_back(viewStartBoard.sitCurrentHash);
    move_t move = moves[i];
    out << Board::writeMove(viewStartBoard,move) << " " << endl;

    bool suc = viewStartBoard.makeMoveLegalNoUndo(move);
    if(!suc)
      Global::fatalError("Viewing illegal move!");
  }
  viewPrintBetterMoves = printBetterMoves;
  viewPrintMoves = printMoves;
  viewExact = exactHash;
  viewEvaluate = true;

  if(exactHash && viewStartBoard.step == 0)
    viewHashes.push_back(viewStartBoard.sitCurrentHash);

  out << viewStartBoard << endl;
  out << "------------------------------" << endl;
}

bool SearchParams::viewing(const Board& b, const BoardHistory& hist) const
{
  if(viewOn && b.sitCurrentHash == viewStartBoard.sitCurrentHash)
  {
    if(!viewExact)
      return true;
    int turnNumber = viewStartBoard.turnNumber;
    int viewHashIdx = viewHashes.size()-1;
    if(turnNumber > hist.maxTurnNumber)
      return false;
    while(turnNumber >= hist.minTurnNumber && viewHashIdx >= 0)
    {
      if(hist.turnSitHash[turnNumber] != viewHashes[viewHashIdx])
        return false;
      turnNumber--;
      viewHashIdx--;
    }
    return true;
  }
  return false;
}

void SearchParams::viewSearch(const Board& b, const BoardHistory& hist,
    int cDepth, int rDepth, int alpha, int beta, const SearchStats& stats) const
{
  ostream& out = *output;
  int startTurnNumber = b.turnNumber - (cDepth - b.step + 3) / 4;
  int endTurnNumber = b.turnNumber;
  DEBUGASSERT(startTurnNumber >= hist.minTurnNumber);
  DEBUGASSERT(endTurnNumber <= hist.maxTurnNumber);
  out << "---VIEW------------ (";
  for(int i = startTurnNumber; i<=endTurnNumber; i++)
  {
    if(hist.turnMove[i] == ERRMOVE || (i == endTurnNumber && b.step == 0))
      continue;
    if(i > startTurnNumber) out << "  ";
    if(i == endTurnNumber)
      out << Board::writeMove(getPreMove(hist.turnMove[i],b.step),true);
    else
      out << Board::writeMove(hist.turnMove[i],true);
  }
  out << ")";
  out << endl;
  out << "rDepth " << rDepth << " cDepth " << cDepth << endl;
  out << "AlphaBeta (" << alpha << "," << beta << ") " << (stats.mNodes + stats.qNodes) << endl;
}

void SearchParams::viewResearch(int cDepth, int rDepth, int alpha, int beta, const SearchStats& stats) const
{
  ostream& out = *output;
  out << "---VIEW RE-SEARCH------------" << endl;
  out << "rDepth " << rDepth << " cDepth " << cDepth << endl;
  out << "AlphaBeta (" << alpha << "," << beta << ") " << (stats.mNodes + stats.qNodes) << endl;
}

void SearchParams::viewMove(const Board& b, move_t move, int hm, eval_t eval, eval_t prevBestEval,
    bool reSearched, int origDepthAdj, move_t* pv, int pvLen, const SearchStats& stats) const
{
  if(!viewPrintMoves && !viewPrintBetterMoves)
    return;

  if(viewPrintBetterMoves && !viewPrintMoves && eval <= prevBestEval)
    return;

  ostream& out = *output;
  out << Board::writeMove(b,move) << " ";
  out << Global::strprintf(" %5d %5d %7lld ", eval, prevBestEval, stats.mNodes + stats.qNodes);
  Board copy = b;
  copy.makeMove(move);
  if(hm == NULL_SCORE)
    out << "(nullmove) ";
  else if(hm == HASH_SCORE)
    out << "(hashmove) ";
  else if(hm >= KILLER_SCORE0)
    out << "(killermove0) ";
  else if(hm >= KILLER_SCORE0 - 50 * KILLER_SCORE_DECREMENT)
    out << "(killermove" << ((KILLER_SCORE0 - hm) / KILLER_SCORE_DECREMENT) << ") ";
  else if(hm >= HASH_MOVE_PREFIX_SCORE)
    out << "(hashprefix) ";
  else
    out << "(order " << hm << ") ";

  if(origDepthAdj != 0 || reSearched)
  {
    if(origDepthAdj > 0)
      out << "e" << origDepthAdj << " ";
    else if(origDepthAdj == DEPTH_ADJ_PRUNE)
      out << "rX ";
    else
      out << "r" << (-origDepthAdj) << " ";

    if(reSearched && eval <= prevBestEval) out << "RSx ";
    else if(reSearched) out << "RS+ ";
  }

  if(pv != NULL)
    out << Board::writeMoves(copy, pv, pvLen);

  out << endl;
}

void SearchParams::viewMovePruned(const Board& b, move_t move) const
{
  if(!viewPrintMoves)
    return;

  ostream& out = *output;
  out << Board::writeMove(b,move) << " pruned " << endl;
}

void SearchParams::viewHash(const Board& b, eval_t hashEval, flag_t hashFlag, int hashDepth, move_t hashMove) const
{
  ostream& out = *output;
  out <<
      "HashEval: " << hashEval <<
      " HashFlag: " << Flags::FLAG_NAMES[hashFlag] <<
      " HashDepth: " << hashDepth <<
      " Hashmove: " << Board::writeMove(b,hashMove) <<
      " Hash: " << b.sitCurrentHash << endl;
  out << "-----------------" << endl;
}

void SearchParams::viewFinished(const Board& b, bool changedPlayer, eval_t bestEval, eval_t extraEval,
    move_t bestMove, flag_t bestFlag, const SearchStats& stats) const
{
  ostream& out = *output;
  const char* flagStr =
      bestFlag == Flags::FLAG_BETA ? "beta" :
      bestFlag == Flags::FLAG_EXACT ? "exact" :
      bestFlag == Flags::FLAG_ALPHA ? "alpha" :
      "(none)";
  if(extraEval == 0)
    out << Global::strprintf("BestEval: %5d %s, ParentEval %5d, Nodes %lld",
        bestEval, flagStr, changedPlayer ? -bestEval : bestEval, stats.mNodes + stats.qNodes);
  else
    out << Global::strprintf("BestEval: %5d %s, ParentEval %5d%+4d, Nodes %lld",
        bestEval, flagStr, changedPlayer ? -bestEval : bestEval, extraEval, stats.mNodes + stats.qNodes);
  out << " BestMove: " << Board::writeMove(b,bestMove,true) << endl;
}


//QSEARCH--------------------------------------------------------------------------------------

//Depth 0: Goaldef,Caps,SGT,CapDefs (finishes when the msearch stops in the middle of turn)
//Depth 1: Goaldef,Caps,SGT (takes the next turn when the msearch stops on a turn boundary)
//Depth 2: Goaldef,Caps  (continues from depth0)
//Depth 3: Goaldef,Caps (continues from 1 or 2, only if the turn was completed voluntarily)
//Depth 4: Eval
const int SearchParams::QDEPTH_NEXT[5] = {2,3,3,4,4};
const int SearchParams::QDEPTH_NEXT_IF_QPASSED[5] = {2,4,4,4,4};
const int SearchParams::QDEPTH_EVAL = 4;
const int SearchParams::QDEPTH_START[4] = {1,0,0,0};
const int SearchParams::QMAX_FDEPTH = 12;
const int SearchParams::QMAX_CDEPTH = 12;
const int SearchParams::Q0PASS_BONUS[5] = {0,100,200,300,400};
const int SearchParams::STEPS_LEFT_BONUS[5] = {-200,-100,0,100,200};

//This is at least 3, because with, say, a nominal search depth of 5,
//we could make a pass move in the search and reach cdepth 8.
const int SearchParams::MAX_MSEARCH_CDEPTH_OVER_NOMINAL = 3;


