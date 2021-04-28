/*
 * searchutils.h
 *
 *  Created on: Jun 2, 2011
 *      Author: davidwu
 */

#ifndef SEARCHUTILS_H_
#define SEARCHUTILS_H_

#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../search/searchflags.h"

class KillerEntry;
struct RatedMove;

namespace SearchUtils
{
  //GAME END CONDITIONS------------------------------------------------------------
  //If won or lost, returns the eval, else returns 0.
  eval_t checkGameEndConditions(const Board& b, const BoardHistory& hist, int cDepth);
  eval_t checkGameEndConditionsQSearch(const Board& b, int cDepth);

  //Is this eval a game-ending eval?
  bool isTerminalEval(eval_t eval) PUREFUNC;
  bool isLoseEval(eval_t eval) PUREFUNC;
  bool isWinEval(eval_t eval) PUREFUNC;

  //Is this eval a game-ending eval that indicates some sort of illegal move or error?
  bool isIllegalityTerminalEval(eval_t eval) PUREFUNC;

  //EVAL FLAG BOUNDING--------------------------------------------------------------
  //Is the result one that proves a value or bound on this position, given alpha and beta?
  bool canEvalCutoff(eval_t alpha, eval_t beta, eval_t result, flag_t resultFlag) PUREFUNC;

  //Is the result a terminal eval that proves a value or bound on this position, given alpha and beta?
  //Depending on allowed to be HASH_WL_AB_STRICT, allowed to return the wrong answer so long as
  //it's still a proven win or a loss
  bool canTerminalEvalCutoff(eval_t alpha, eval_t beta, eval_t result, flag_t resultFlag) PUREFUNC;

  //MOVE PROCESSING AND MISC-------------------------------------------------------------------
  //Ensure array big enough, allocate or reallocate it if needed
  void ensureMoveHistoryArray(move_t*& mv, int& capacity, int newCapacity);

  //Ensure move array big enough, allocate or reallocate it if needed
  void ensureMoveArray(move_t*& mv, int*& hm, int& mvCapacity, int newCapacity);

  //Find the best move in the list at or after index and swap it to index
  void selectBest(move_t* mv, int* hm, int num, int index);

  //Find the longest strict prefix of the given move, if any, and set its hm to the given orderingValue.
  void setOrderingForLongestStrictPrefix(move_t move, const move_t* mv, int* hm, int num, int orderingValue);

  //Randomly shuffle the given moves
  //Expensive, because it creates and initializes an RNG!
  void shuffle(uint64_t seed, vector<move_t>& mvB);
  void shuffle(uint64_t seed, move_t* mvB, int num);

  //Stable insertion sort from largest to smallest by values in hm
  void insertionSort(move_t* mv, int* hm, int numMoves);
  void insertionSort(vector<RatedMove>& mv, int* hm);

  //Output that an illegal move was made, and exit
  void reportIllegalMove(const Board& b, move_t move);

  //Convert any any terminal pass or qpass in the move to the other
  move_t convertQPassesToPasses(move_t mv);
  move_t convertPassesToQPasses(move_t mv);

  //Delete any qpasses or regular passes in the given move
  move_t removeQPassesAndPasses(move_t mv);
  //Delete any steps after passes or qpasses or errsteps in the given move
  move_t removeRedundantSteps(move_t mv);
  //Finds the step, push, or pull that begins mv.
  move_t pruneToRegularMove(const Board& b, move_t mv);
  //Extracts the longest possible move for the given turn from the given pv
  move_t extractTurnMoveFromPV(const move_t* pv, int pvLen, const Board& b);

  //Get the qdepth to begin quiescence at
  int getStartQDepth(const Board& b);

  //Checks if the last move, by the opponent, is reversable.
  //The board is the board after the move that might be reversible
  //Returns 0 if not reversible, 1 if we can reverse 3 step, 2 if we can reverse fully
  int isReversibleAssumeNoCaps(move_t move, const Board& b, move_t& reverseMove);
  int isReversible(const Board& b, const BoardHistory& hist, move_t& reverseMove);

  //Check if the move that arrived at the current position loses a repetition fight and therefore effectively cannot be made
  //turnsToLoseByRep is 0 if the move itself is actually illegal due to repetition, 2 if the move becomes illegal after
  //the opponent reverses us, 4 for a longer cycle than that, etc.
  bool losesRepetitionFight(const Board& b, const BoardHistory& hist, int cDepth, int& turnsToLoseByRep);

  //Call at step 3 of a search. Not all three steps can be independent. (haizhi step pruning)
  bool isStepPrunable(const Board& b, const BoardHistory& hist, int cDepth);

  //Was this a move that gives a freely capturable piece?
  //ASSUMES that no captures were made by that player, although sacs might have been made by that player.
  bool isFreeCapturable(Board& finalBoard, move_t move, int& nsNotSpentOn, int& nsCapture, loc_t& freeCapLoc);

  //KILLERS------------------------------------------------------------------------------------
  //Killers might not be legal, so should be checked for legality prior to using
  void getKillers(KillerEntry& buf, const KillerEntry* killerMoves, int killerMovesLen, int cDepth);
  void recordKiller(KillerEntry* killerMoves, int killerMovesLen, move_t move, int cDepth);

  //HISTORY TABLE------------------------------------------------------------------------------
  //Ensure that the history table has been allocated up to the given requested depth of search.
  //Not threadsafe, call only before search!
  void ensureHistory(vector<int64_t*>& historyTable, vector<int64_t>& historyMax, int depth);

  //Call whenever we want to clear the history table
  void clearHistory(vector<int64_t*>& historyTable, vector<int64_t>& historyMax);

  //Cut all history scores down, call between iterations of iterative deepening
  void decayHistory(vector<int64_t*>& historyTable, vector<int64_t>& historyMax);

  //Update the history table
  void updateHistoryTable(vector<int64_t*>& historyTable, vector<int64_t>& historyMax,
      const Board& b, int cDepth, move_t move);

  //Fill history scores into the hm array for all moves
  void getHistoryScores(const vector<int64_t*>& historyTable, const vector<int64_t>& historyMax,
      const Board& b, pla_t pla, int cDepth, const move_t* mv, int* hm, int len);

  //Get the history score for a single move
  int getHistoryScore(const vector<int64_t*>& historyTable, const vector<int64_t>& historyMax,
      const Board& b, pla_t pla, int cDepth, move_t move);

  //PV--------------------------------------------------------------------------------
  void endPV(int& pvLen);
  void copyPV(const move_t* srcpv, int srcpvLen, move_t* destpv, int& destpvLen);
  void copyExtendPV(move_t move, const move_t* srcpv, int srcpvLen, move_t* destpv, int& destpvLen);

}



#endif /* SEARCHUTILS_H_ */
