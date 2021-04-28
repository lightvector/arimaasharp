/*
 * searchutils.cpp
 * Author: davidwu
 */

#include <sstream>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"
#include "../eval/eval.h"
#include "../search/searchutils.h"
#include "../search/searchparams.h"
#include "../search/search.h"

using namespace std;

//If won or lost, returns the eval, else returns 0.
//Note that for counting distance to win/loss, scores are adjusted as if it were impossible to win
//at the start of your turn, or lose immediately upon having made a legal move.
//Immobilization is counted as a loss at the end of the opponent's next turn, since we have no static detection
//yet we want to be able to say that if there are no static detected game end conds right now, a win/loss will not happen this turn.
//3-move reps are treated legal moves that cause you to lose but with a winloss distance of the end of the opp's next move)
eval_t SearchUtils::checkGameEndConditions(const Board& b, const BoardHistory& hist, int cDepth)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  if(b.step == 0)
  {
    if(b.isGoal(opp)) {return Eval::LOSE + cDepth;} //Lose if opponent goaled
    if(b.isGoal(pla)) {return Eval::WIN - cDepth - 4;} //Win if opponent made us goal
    if(b.isRabbitless(pla)) {return Eval::LOSE + cDepth;} //Lose if opponent eliminated our rabbits
    if(b.isRabbitless(opp)) {return Eval::WIN - cDepth - 4;} //Win if opponent killed all his own rabbits
    if(b.noMoves(pla)) {return Eval::LOSE + cDepth + 8;} //Lose if we have no moves
    if(cDepth > 0 && BoardHistory::isThirdRepetition(b,hist)) {return Eval::WIN - cDepth - 4;} //Win if opponent made a 3rd repetition
  }
  else
  {
    if(b.isGoal(pla)) {return Eval::WIN - cDepth - (4-b.step);}    //Win if we goaled
    if(b.isRabbitless(opp)) {return Eval::WIN - cDepth - (4-b.step);}  //Win if we killed all opponent's rabbits
    if(b.noMoves(opp) && b.posCurrentHash != b.posStartHash && !b.isGoal(opp) && !b.isRabbitless(pla))
    {return Eval::WIN - cDepth - (4-b.step) - 8;}  //Win if opp has no moves, pos different, opp no goal, we have rabbits
  }
  return 0;
}

//If won or lost, returns the eval, else returns 0.
//Note that for counting distance to win/loss, scores are adjusted as if it were impossible to win
//at the start of your turn, or lose immediately upon having made a legal move.
//(counting 3-move reps as legal moves that cause you to lose but with a winloss distance of the end of the opp's next move)
eval_t SearchUtils::checkGameEndConditionsQSearch(const Board& b, int cDepth)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  //We do not check 3rd move reps here. We also allow the opponent not to change the position, since we allow quiescence passes.
  if(b.step == 0)
  {
    if(b.isGoal(opp)) {return Eval::LOSE + cDepth;}    //Lose if opponent goaled
    if(b.isGoal(pla)) {return Eval::WIN - cDepth - 4;}    //Win if opponent made us goal
    if(b.isRabbitless(pla)) {return Eval::LOSE + cDepth;}  //Lose if opponent eliminated our rabbits
    if(b.isRabbitless(opp)) {return Eval::WIN - cDepth - 4;}  //Win if opponent killed all his own rabbits
    //Avoid generating immo-related losses because windefsearch doesn't cover it.
    //if(b.noMoves(pla)) {return Eval::LOSE + cDepth + 8;}    //Lose if we have no moves
    //if(cDepth > 0 && BoardHistory::isThirdRepetition(b,boardHistory)) {return Eval::WIN - cDepth - 4;} //Win if opponent made a 3rd repetition
  }
  else
  {
    if(b.isGoal(pla)) {return Eval::WIN - cDepth - (4-b.step);}    //Win if we goaled
    if(b.isRabbitless(opp)) {return Eval::WIN - cDepth - (4-b.step);}  //Win if we killed all opponent's rabbits
    //Avoid generating immo-related losses because windefsearch doesn't cover it.
    //if(b.noMoves(opp) && b.posCurrentHash != b.posStartHash && !b.isGoal(opp) && !b.isRabbitless(pla))
    //{return Eval::WIN - cDepth - (4-b.step) - 8;}  //Win if opp has no moves, pos different, opp no goal, we have rabbits
  }
  return 0;
}

//Is this eval a game-ending eval?
bool SearchUtils::isTerminalEval(eval_t eval)
{
  return (eval <= Eval::LOSE_TERMINAL) || (eval >= Eval::WIN_TERMINAL);
}

bool SearchUtils::isLoseEval(eval_t eval)
{
  return eval <= Eval::LOSE_TERMINAL;
}

bool SearchUtils::isWinEval(eval_t eval)
{
  return eval >= Eval::WIN_TERMINAL;
}


//Is this eval a game-ending eval that indicates some sort of illegal move or error?
bool SearchUtils::isIllegalityTerminalEval(eval_t eval)
{
  return eval <= Eval::LOSE || eval >= Eval::WIN;
}

//HELPERS - EVAL FLAG BOUNDING--------------------------------------------------------------

//Is the result one that proves a value or bound on this position, given alpha and beta?
bool SearchUtils::canEvalCutoff(eval_t alpha, eval_t beta, eval_t result, flag_t resultFlag)
{
  return
  (resultFlag == Flags::FLAG_EXACT) ||
  (resultFlag == Flags::FLAG_ALPHA && result <= alpha) ||
  (resultFlag == Flags::FLAG_BETA && result >= beta);
}


//Is the result a terminal eval that proves a value or bound on this position, given alpha and beta?
//Depending on allowed to be HASH_WL_AB_STRICT, allowed to return the wrong answer so long as
//it's still a proven win or a loss
bool SearchUtils::canTerminalEvalCutoff(eval_t alpha, eval_t beta, eval_t result, flag_t resultFlag)
{
  if(!isTerminalEval(result))
    return false;

  if(!SearchParams::HASH_WL_UNSAFE)
    return canEvalCutoff(alpha,beta,result,resultFlag);

  return !(result < 0 && resultFlag == Flags::FLAG_BETA) &&
         !(result > 0 && resultFlag == Flags::FLAG_ALPHA);
}

//HELPERS - MOVES PROCESSING----------------------------------------------------------------------------

void SearchUtils::ensureMoveHistoryArray(move_t*& mv, int& capacity, int newCapacity)
{
  if(capacity < newCapacity)
  {
    move_t* newmv = new move_t[newCapacity];
    if(mv != NULL)
    {
      for(int i = 0; i<capacity; i++)
        newmv[i] = mv[i];
    }

    move_t* mvTemp = mv;
    mv = newmv;
    capacity = newCapacity;

    if(mvTemp != NULL)
      delete[] mvTemp;
  }
}

void SearchUtils::ensureMoveArray(move_t*& mv, int*& hm, int& capacity, int newCapacity)
{
  if(capacity < newCapacity)
  {
    move_t* newmv = new move_t[newCapacity];
    int* newhm = new int[newCapacity];

    if(mv != NULL)
    {
      DEBUGASSERT(hm != NULL);
      for(int i = 0; i<capacity; i++)
      {
        newmv[i] = mv[i];
        newhm[i] = hm[i];
      }
    }

    move_t* mvTemp = mv;
    int* hmTemp = hm;

    mv = newmv;
    hm = newhm;
    capacity = newCapacity;

    if(mvTemp != NULL)
    {
      delete[] mvTemp;
      delete[] hmTemp;
    }
  }
}

//Find the best move in the list at or after index and swap it to index
void SearchUtils::selectBest(move_t* mv, int* hm, int num, int index)
{
  int bestIndex = index;
  int bestHM = hm[index];
  for(int i = index+1; i<num; i++)
  {
    int hmval = hm[i];
    if(hmval > bestHM)
    {
      bestHM = hmval;
      bestIndex = i;
    }
  }

  move_t utemp = mv[bestIndex];
  mv[bestIndex] = mv[index];
  mv[index] = utemp;

  int temp = hm[bestIndex];
  hm[bestIndex] = hm[index];
  hm[index] = temp;
}

void SearchUtils::setOrderingForLongestStrictPrefix(move_t move, const move_t* mv, int* hm, int num, int orderingValue)
{
  int longestLen = 0;
  int longestIdx = -1;
  for(int i = 0; i<num; i++)
  {
    if(isPrefix(mv[i],move) && mv[i] != move)
    {
      int ns = numStepsInMove(mv[i]);
      if(ns > longestLen)
      {longestLen = ns; longestIdx = i;}
    }
  }
  if(longestIdx >= 0)
    hm[longestIdx] = orderingValue;
}

void SearchUtils::shuffle(uint64_t seed, vector<move_t>& mv)
{
  Rand rand(seed);
  int num = mv.size();
  for(int i = num-1; i >= 1; i--)
  {
    int r = rand.nextUInt(i+1);
    move_t mtemp = mv[i];
    mv[i] = mv[r];
    mv[r] = mtemp;
  }
}

void SearchUtils::shuffle(uint64_t seed, move_t* mv, int num)
{
  Rand rand(seed);
  for(int i = num-1; i >= 1; i--)
  {
    int r = rand.nextUInt(i+1);
    move_t mtemp = mv[i];
    mv[i] = mv[r];
    mv[r] = mtemp;
  }
}

//Stable insertion sort from largest to smallest by values in hm
void SearchUtils::insertionSort(move_t* mv, int* hm, int numMoves)
{
  for(int i = 1; i < numMoves; i++)
  {
    move_t tempmv = mv[i];
    int temphm = hm[i];
    int j;
    for(j = i; j > 0 && hm[j-1] < temphm; j--)
    {
      mv[j] = mv[j-1];
      hm[j] = hm[j-1];
    }
    mv[j] = tempmv;
    hm[j] = temphm;
  }
}
void SearchUtils::insertionSort(vector<RatedMove>& mv, int* hm)
{
  int numMoves = mv.size();
  for(int i = 1; i < numMoves; i++)
  {
    RatedMove tempmv = mv[i];
    int temphm = hm[i];
    int j;
    for(j = i; j > 0 && hm[j-1] < temphm; j--)
    {
      mv[j] = mv[j-1];
      hm[j] = hm[j-1];
    }
    mv[j] = tempmv;
    hm[j] = temphm;
  }
}

void SearchUtils::reportIllegalMove(const Board& b, move_t move)
{
  ostringstream err;
  err << "Illegal move in search: " << Board::writeMove(b,move) << endl;
  err << b;
  Global::fatalError(err.str());
}

move_t SearchUtils::convertQPassesToPasses(move_t mv)
{
  for(int i = 0; i<4; i++)
  {
    step_t step = getStep(mv,i);
    if(step == ERRSTEP || step == PASSSTEP)
      return mv;
    if(step == QPASSSTEP)
      return setStepToPass(mv,i);
  }
  return mv;
}

move_t SearchUtils::convertPassesToQPasses(move_t mv)
{
  for(int i = 0; i<4; i++)
  {
    step_t step = getStep(mv,i);
    if(step == ERRSTEP || step == QPASSSTEP)
      return mv;
    if(step == PASSSTEP)
      return setStep(mv,QPASSSTEP,i);
  }
  return mv;
}

move_t SearchUtils::removeQPassesAndPasses(move_t mv)
{
  for(int i = 0; i<4; i++)
  {
    step_t step = getStep(mv,i);
    if(step == ERRSTEP)
      return mv;
    if(step == QPASSSTEP || step == PASSSTEP)
      return getPreMove(mv,i);
  }
  return mv;
}

move_t SearchUtils::removeRedundantSteps(move_t mv)
{
  //We only need to test up to the third step, since there can be no redundant steps
  //after the final step in the move.
  for(int i = 0; i<3; i++)
  {
    step_t step = getStep(mv,i);
    if(step == ERRSTEP || step == QPASSSTEP || step == PASSSTEP)
      return getPreMove(mv,i+1);
  }
  return mv;
}

//Finds the step, push, or pull that begins mv.
move_t SearchUtils::pruneToRegularMove(const Board& b, move_t mv)
{
  if(mv == ERRMOVE || mv == PASSMOVE || mv == QPASSMOVE)
    return mv;

  step_t step0 = getStep(mv,0);
  step_t step1 = getStep(mv,1);

  if(step1 == ERRSTEP || step1 == PASSSTEP || step1 == QPASSSTEP)
    return getMove(step0);

  loc_t k0 = gSrc(step0);
  loc_t k1 = gSrc(step1);

  //Ensure one pla and one opp adjacent
  if(!Board::isAdjacent(k0,k1) || b.owners[k0] == NPLA || b.owners[k1] == NPLA || b.owners[k0] == b.owners[k1])
    return getMove(step0);

  //Since there's an opp involved, if it's only 2 steps, it must be a valid push or pull.
  if(getStep(mv,2) == ERRSTEP)
    return mv;

  //Extract the push or pull from the longer sequence of steps

  //Pull
  if(b.owners[k0] == b.player)
  {
    //Catch the case where some third piece does a push that makes it look like we pulled
    if(gDest(step1) != k0 || b.pieces[k0] <= b.pieces[k1])
      return getMove(step0);
    return getMove(step0,step1);
  }
  //Push
  else
  {
    //Must be legal push
    return getMove(step0,step1);
  }
}

move_t SearchUtils::extractTurnMoveFromPV(const move_t* pv, int pvLen, const Board& b)
{
  if(pvLen <= 0)
    return ERRMOVE;
  int numSteps = 4-b.step;

  move_t move = pv[0];
  int ns = numRealStepsInMove(move);
  for(int i = 1; i<pvLen; i++)
  {
    if(ns >= numSteps)
      break;
    move_t pvMove = pv[i];
    move = concatMoves(move,pvMove,ns);
    ns = numRealStepsInMove(move);
  }
  return move;
}



//HELPERS - REVERSIBLES------------------------------------------------------------------------------


//TODO optimize this and other freecap functions by sharing the src and dest changes array?
//Checks if making move in prev resulting in b is reversable.
//Returns 0 if not reversible, 1 if we can reverse 3 step, 2 if we can reverse fully
int SearchUtils::isReversibleAssumeNoCaps(move_t move, const Board& b, move_t& reverseMove)
{
  //By default
  reverseMove = ERRMOVE;

  //Arrays to track pieces and where they went
  int num = 0;
  loc_t src[4];
  loc_t dest[4];
  num = b.getChangesInHindsightNoCaps(move,src,dest);

  pla_t pla = b.player;

  //Ensure that too many changes did not occur
  if(num > 2 || num == 0)
    return 0;

  //One change
  if(num == 1)
  {
    loc_t k0 = dest[0];
    loc_t k1 = src[0];
    //Ensure that the change was a player piece, it is unfrozen, and the dest is empty
    if(k0 == ERRLOC || b.owners[k0] != pla || b.isFrozen(k0) || b.owners[k1] != NPLA)
      return 0;

    //One step away
    if(Board::manhattanDist(k1,k0) == 1)
    {
      //Can reverse if no rabbit restrictions
      if(b.isRabOkay(pla,k0,k1))
        return 2;
    }
    //Two steps away
    else if(Board::manhattanDist(k1,k0) == 2)
    {
      int dx = gDXOffset(gX(k1)-gX(k0));
      int dy = gDYOffset(gY(k1)-gY(k0));
      int dk = (dx+dy)/2;
      //Diagonal step
      if(dx != 0 && dy != 0)
      {
        //Try both paths back
        if(b.owners[k0+dx] == NPLA && b.isTrapSafe2(pla,k0+dx) && b.wouldBeUF(pla,k0,k0+dx,k0) && b.isRabOkay(pla,k0+dx,k1))
          return 2;
        if(b.owners[k0+dy] == NPLA && b.isTrapSafe2(pla,k0+dy) && b.wouldBeUF(pla,k0,k0+dy,k0) && b.isRabOkay(pla,k0,k0+dy))
          return 2;

        //Try both paths with a piece swap:
        if(b.owners[k0+dx] == pla && b.pieces[k0+dx] == b.pieces[k0] && b.isTrapSafe2(pla,k1) &&
           b.wouldBeUF(pla,k0,k0,k0+dx) && b.isRabOkay(pla,k0+dx,k1))
          return 2;
        if(b.owners[k0+dy] == pla && b.pieces[k0+dy] == b.pieces[k0] && b.isTrapSafe2(pla,k1) &&
           b.wouldBeUF(pla,k0,k0,k0+dy) && b.isRabOkay(pla,k0,k0+dy))
          return 2;
      }
      //Double straight step
      else if(dk != 0)
      {
        //Try stepping back
        if(b.owners[k0+dk] == NPLA && b.isTrapSafe2(pla,k0+dk) && b.wouldBeUF(pla,k0,k0+dk,k0) && b.isRabOkay(pla,k0,k0+dk))
          return 2;
      }
    }
    return 0;
  }
  //Two changes
  else if(num == 2)
  {
    //Ensure both changes are a single step
    if(dest[0] == ERRLOC || dest[1] == ERRLOC || Board::manhattanDist(src[0],dest[0]) != 1 || Board::manhattanDist(src[1],dest[1]) != 1)
      return 0;

    //Ensure at least one piece is a player
    if(b.owners[dest[0]] != pla && b.owners[dest[1]] != pla)
      return 0;

    //If both pieces are players
    if(b.owners[dest[0]] == pla && b.owners[dest[1]] == pla)
    {
      loc_t k0 = dest[0];
      loc_t j0 = dest[1];
      loc_t k1 = src[0];
      loc_t j1 = src[1];
      //Try stepping both back
      if(b.isThawed(k0) && b.isThawed(j0) && b.isRabOkay(pla,k0,k1) && b.isRabOkay(pla,j0,j1))
      {
        if(k1 == j0 && b.owners[j1] == NPLA && b.wouldBeUF(pla,k0,k0,k1))
          return 2;
        if(j1 == k0 && b.owners[k1] == NPLA && b.wouldBeUF(pla,j0,j0,j1))
          return 2;
        if(b.owners[k1] == NPLA && b.owners[j1] == NPLA && (b.wouldBeUF(pla,k0,k0,j0) || b.wouldBeUF(pla,j0,j0,k0)))
          return 2;
      }
    }
    //One player one opponent
    else
    {
      loc_t k0,k1,o0,o1;
      if(b.owners[dest[0]] == pla){k0 = dest[0]; o0 = dest[1]; k1 = src[0]; o1 = src[1];}
      else                        {k0 = dest[1]; o0 = dest[0]; k1 = src[1]; o1 = src[0];}

      //Try pushing/pulling the opponent back
      if(b.pieces[k0] > b.pieces[o0] && b.isThawed(k0) && ((k1 == o0 && b.owners[o1] == NPLA) || (o1 == k0 && b.owners[k1] == NPLA)))
        return 2;

      //Can only step back our own piece
      if(b.isThawed(k0) && b.owners[k1] == NPLA && b.isRabOkay(pla,k0,k1))
      {reverseMove = getMove(gStepSrcDest(k0,k1)); return 1;}
    }
    return 0;
  }
  return 0;
}

//Checks if the last move, by the opponent, is reversable.
//Returns 0 if not reversible, 1 if we can reverse 3 step, 2 if we can reverse fully
int SearchUtils::isReversible(const Board& b, const BoardHistory& boardHistory, move_t& reverseMove)
{
  //By default
  reverseMove = ERRMOVE;

  //Make sure we call only when current step is 0
  if(b.step != 0)
    return 0;

  //Make sure last turn exists
  int lastTurn = b.turnNumber-1;
  if(lastTurn < boardHistory.minTurnNumber)
    return 0;

  //Ensure no pieces died last turn
  if(boardHistory.turnPieceCount[GOLD][lastTurn] + boardHistory.turnPieceCount[SILV][lastTurn] !=
    b.pieceCounts[GOLD][0] + b.pieceCounts[SILV][0])
    return 0;

  return isReversibleAssumeNoCaps(boardHistory.turnMove[lastTurn], b, reverseMove);
}

bool SearchUtils::losesRepetitionFight(const Board& b, const BoardHistory& boardHistory, int cDepth, int& turnsToLose)
{
  hash_t currentHash = b.sitCurrentHash;
  int currentTurn = b.turnNumber;
  int startTurn = b.turnNumber - (cDepth/4);
  if(startTurn > boardHistory.minTurnNumber)
    startTurn--; //Go back one turn before the start of search

  DEBUGASSERT(currentTurn-1 <= boardHistory.maxTurnNumber && startTurn >= boardHistory.minTurnNumber);

  //Walk backwards to see if we repeat any position occuring within the current search or one turn before it.
  //We only go back one turn before the start of search because we do want the search to be able to decide
  //to make the first few rep-fight-losing moves in a long cycle so that it can choose where it wants to deviate.
  //instead of being forced to deviate at the first moment.
  int repTurn = -1;
  for(int t = currentTurn-2; t >= startTurn; t -= 2)
  {
    //Don't check through a move made by null move pruning
    if(boardHistory.turnMove[t+1] == QPASSMOVE || boardHistory.turnMove[t] == QPASSMOVE)
      break;
    //Found a rep?
    if(currentHash == boardHistory.turnSitHash[t])
    {
      repTurn = t;
      break;
    }
  }

  if(repTurn == -1)
    return false;

  //Now see if we will lose this repetition fight
  //Loop forward over the repetitions and check who will have to deviate first
  int pieceCount = b.pieceCounts[GOLD][0] + b.pieceCounts[SILV][0];
  for(int t = repTurn; t < currentTurn; t++)
  {
    //If there is any instance in the past, we lose.
    bool foundPastInstance = false;
    hash_t hash = boardHistory.turnSitHash[t];
    for(int pt = t-2; pt >= boardHistory.minTurnNumber; pt--)
    {
      if(pieceCount != boardHistory.turnPieceCount[GOLD][pt] + boardHistory.turnPieceCount[SILV][pt])
        break;
      if(boardHistory.turnMove[pt+1] == QPASSMOVE || boardHistory.turnMove[pt] == QPASSMOVE) //nullmove
        break;
      if(boardHistory.turnSitHash[pt] == hash)
      {
        foundPastInstance = true;
        break;
      }
    }
    if(foundPastInstance)
    {
      turnsToLose = t - repTurn;
      return true;
    }

    //Next!
    t++;
    if(t >= currentTurn)
      break;

    //If there is any instance in the past for the opponent, he loses and we win
    foundPastInstance = false;
    hash = boardHistory.turnSitHash[t];
    for(int pt = t-2; pt >= boardHistory.minTurnNumber; pt--)
    {
      if(pieceCount != boardHistory.turnPieceCount[GOLD][pt] + boardHistory.turnPieceCount[SILV][pt])
        break;
      if(boardHistory.turnMove[pt+1] == QPASSMOVE || boardHistory.turnMove[pt] == QPASSMOVE) //nullmove
        break;
      if(boardHistory.turnSitHash[pt] == hash)
      {
        foundPastInstance = true;
        break;
      }
    }
    if(foundPastInstance)
      return false;
  }

  //Everything was okay. Then we lose because we can't repeat it yet again.
  turnsToLose = currentTurn - repTurn;
  return true;
}

/*
Dependencies:
push *
pull *
step to unfreeze subsequent stepping piece
vacate square for subsequent stepping piece
move same piece twice *
support a trap that the second piece to move was supporting
*/

//Call at step 3 of a search. Not all three steps can be independent.
/*
bool SearchUtils::isStepPrunable(const Board& b, const BoardHistory& boardHistory, int cDepth)
{
  //Ensure proper step
  if(b.step != 3)
    return false;

  //Make sure this turn exists
  int lastTurn = b.turnNumber;
  if(lastTurn < boardHistory.minTurnNumber)
    return 0;

  //Ensure no pieces died this turn
  if(boardHistory.turnPieceCount[GOLD][lastTurn] + boardHistory.turnPieceCount[SILV][lastTurn] !=
    b.pieceCounts[GOLD][0] + b.pieceCounts[SILV][0])
    return 0;

  //Arrays to track pieces and where they went
  int num = 0;
  loc_t src[8];
  loc_t dest[8];
  const Board& startBoard = boardHistory.turnBoard[lastTurn];
  num = startBoard.getChanges(boardHistory.turnMove[lastTurn],src,dest);

  pla_t pla = b.player;

  //Ensure all same player
  if(num != 3 || b.owners[dest[0]] != pla || b.owners[dest[1]] != pla || b.owners[dest[2]] != pla)
    return false;

  //Check for dependencies:

  //Empty square to step other piece on
  if(src[0] == dest[1] || src[0] == dest[2] || src[1] == dest[2])
    return false;

  //Leave trap for other piece to guard
  if(Board::ADJACENTTRAP[src[0]] != ERRLOC && (Board::ADJACENTTRAP[src[0]] == Board::ADJACENTTRAP[dest[1]] || Board::ADJACENTTRAP[src[0]] == Board::ADJACENTTRAP[dest[2]]))
    return false;
  if(Board::ADJACENTTRAP[src[1]] != ERRLOC && Board::ADJACENTTRAP[src[1]] == Board::ADJACENTTRAP[dest[2]])
    return false;


  //Step off trap that other piece leaves
  if(Board::ISTRAP[src[0]] && (Board::ADJACENTTRAP[src[1]] == src[0] || Board::ADJACENTTRAP[src[2]] == src[0]))
    return false;
  if(Board::ISTRAP[src[1]] && Board::ADJACENTTRAP[src[2]] == src[1])
    return false;

  //Unfreeze other piece
  if(startBoard.isFrozen(src[1]) || startBoard.isFrozen(src[2]))
    return false;
  if(!startBoard.wouldBeUF(pla,src[2],src[2],src[0]))
    return false;

  return true;
}*/

static loc_t potentiallyFreeCapturableTrap(Board& finalBoard, pla_t opp, loc_t oloc)
{
  //Ensure it is next to an opp unsafe trap or it is dominated rad2 from a trap with no opp defenders at all
  loc_t kt = Board::ADJACENTTRAP[oloc];
  if(kt != ERRLOC && !finalBoard.isTrapSafe2(opp,kt))
    return kt;

  kt = Board::RAD2TRAP[oloc];
  if(kt != ERRLOC && finalBoard.isDominated(oloc) && !finalBoard.isTrapSafe1(opp,kt))
    return kt;

  return ERRLOC;
}

bool SearchUtils::isFreeCapturable(Board& finalBoard, move_t move, int& nsNotSpentOn, int& nsCapture, loc_t& freeCapLocBuf)
{
  if(finalBoard.step != 0)
    return false;

  //Arrays to track pieces and where they went
  loc_t src[4];
  loc_t dest[4];
  //By precondition (see searchutils.h) no caps have happened, although sacs may have happened.
  int num = finalBoard.getChangesInHindsightNoCaps(move,src,dest);

  //Ensure that too many or too few changes did not occur
  if(num > 2 || num <= 0)
    return false;

  pla_t pla = finalBoard.player;
  pla_t opp = gOpp(pla);
  bool freeCapFound = false;
  loc_t freeCapLoc = ERRLOC;
  int freeCapDist = 5;
  int numExtraneousSteps = 0;

  for(int i = 0; i<num; i++)
  {
    loc_t oloc = dest[i];
    //By precondition, this must have been a sac of the opponent's piece, so just ignore it.
    //Note that this also won't count as a move the opponent made - that's fine - steps spent on
    //sacs are are happy to not count as steps and therefore be a bit more pruney.
    if(oloc == ERRLOC)
      continue;

    //A push/pull happened
    if(finalBoard.owners[oloc] != opp)
      return false;

    //Ensure it is next to an opp unsafe trap or it is dominated rad2 from a trap with no opp defenders at all
    loc_t kt = potentiallyFreeCapturableTrap(finalBoard,opp,oloc);
    if(kt != ERRLOC)
    {
      //Check if it can now be captured.
      Bitmap capMap;
      int capDist;
      if(BoardTrees::canCaps(finalBoard,pla,4,kt,capMap,capDist))
      {
        if(capMap.isOne(oloc))
        {
          freeCapFound = true;
          if(capDist < freeCapDist)
          {
            freeCapLoc = oloc;
            freeCapDist = capDist;
          }
          continue;
        }
      }
    }

    //We found a step that isn't capturable, so count extraneous steps
    int mDist = Board::manhattanDist(src[i],dest[i]);
    numExtraneousSteps += mDist;
    if(numExtraneousSteps > 1)
      return false;
  }

  if(freeCapFound)
  {
    nsNotSpentOn = numExtraneousSteps;
    nsCapture = freeCapDist;
    freeCapLocBuf = freeCapLoc;
    return true;
  }

  return false;
}

//KILLER MOVES---------------------------------------------------

//Might not be legal!
void SearchUtils::getKillers(KillerEntry& buf, const KillerEntry* killerMoves, int killerMovesLen, int cDepth)
{
  if(cDepth >= killerMovesLen)
    return;
  buf = killerMoves[cDepth];
}

void SearchUtils::recordKiller(KillerEntry* killerMoves, int killerMovesLen, move_t move, int cDepth)
{
  if(killerMovesLen <= cDepth)
    return;

  if(move == ERRMOVE)
    return;

  KillerEntry& k = killerMoves[cDepth];
  if(move == k.killer[0])
    return;

  int i = 1;
  while(i < KillerEntry::NUM_KILLERS - 1 && move != k.killer[i])
    i++;
  while(i > 0)
  {
    k.killer[i] = k.killer[i-1];
    i--;
  }
  k.killer[0] = move;
}


//HELPERS - HISTORY---------------------------------------------------------------------

//TODO make this distinguish pushpulls from consecutive nearby steps more perfectly
static int getHistoryIndex(move_t move)
{
  step_t step0 = getStep(move,0);
  step_t step1 = getStep(move,1);
  if(step1 == ERRSTEP || step1 == PASSSTEP || step1 == QPASSSTEP)
    return (int)step0;

  if(gDest(step1) != gSrc(step0) || gDest(step0) == gSrc(step1))
    return (int)step0;
  return (int)step0 + (int)gDir(step1)*256+256;
}

static int getHistoryDepth(int cDepth)
{
  return cDepth;
}

//HISTORY TABLE---------------------------------------------------
//Ensure that the history table has been allocated up to the given requested depth of search.
//Not threadsafe, call only before search!
void SearchUtils::ensureHistory(vector<int64_t*>& historyTable, vector<int64_t>& historyMax, int depth)
{
  while((int)historyTable.size() < depth + SearchParams::QMAX_CDEPTH)
  {
    int64_t* arr = new int64_t[SearchParams::HISTORY_LEN];
    for(int j = 0; j<SearchParams::HISTORY_LEN; j++)
      arr[j] = 0;
    historyTable.push_back(arr);
    historyMax.push_back(0);
  }
}

//Call whenever we want to clear the history table
void SearchUtils::clearHistory(vector<int64_t*>& historyTable, vector<int64_t>& historyMax)
{
  int size = historyTable.size();
  for(int i = 0; i<size; i++)
  {
    historyMax[i] = 0;
    for(int j = 0; j<SearchParams::HISTORY_LEN; j++)
      historyTable[i][j] = 0;
  }
}

//Cut all history scores down, call between iterations of iterative deepening
void SearchUtils::decayHistory(vector<int64_t*>& historyTable, vector<int64_t>& historyMax)
{
  int size = historyTable.size();
  for(int i = 0; i<size; i++)
  {
    historyMax[i] = (historyMax[i]+1)/6;
    for(int j = 0; j<SearchParams::HISTORY_LEN; j++)
      historyTable[i][j] = (historyTable[i][j]+1)/6;
  }
}

//TODO is it better if msearch, qdepth 0, and qdepth -1 don't share each other's history tables?
void SearchUtils::updateHistoryTable(vector<int64_t*>& historyTable, vector<int64_t>& historyMax,
    const Board& b, int cDepth, move_t move)
{
  (void)b;
  if(move == ERRMOVE)
    return;

  int dIndex = getHistoryDepth(cDepth);
  if((int)historyTable.size() <= dIndex)
    return;

  int hIndex = getHistoryIndex(move);
  int64_t val;
  if(hIndex >= 256)
    val = (historyTable[dIndex][hIndex] += 2);
  else
    val = ++(historyTable[dIndex][hIndex]);
  if(val > historyMax[dIndex])
    historyMax[dIndex] = val;
}

void SearchUtils::getHistoryScores(const vector<int64_t*>& historyTable, const vector<int64_t>& historyMax,
    const Board& b, pla_t pla, int cDepth, const move_t* mv, int* hm, int len)
{
  (void)b;
  (void)pla;
  int dIndex = getHistoryDepth(cDepth);
  DEBUGASSERT((int)historyTable.size() > dIndex)

  int64_t histMax = historyMax[dIndex];
  if(histMax == 0)
    return;

  int64_t* histTable = historyTable[dIndex];
  for(int m = 0; m < len; m++)
  {
    move_t move = mv[m];

    int hIndex = getHistoryIndex(move);
    int64_t score = histTable[hIndex];
    if(score > histMax)
      score = histMax;
    hm[m] += (int)(score*SearchParams::HISTORY_SCORE_MAX/histMax);
  }
}

int SearchUtils::getHistoryScore(const vector<int64_t*>& historyTable, const vector<int64_t>& historyMax,
    const Board& b, pla_t pla, int cDepth, move_t move)
{
  (void)b;
  (void)pla;
  int dIndex = getHistoryDepth(cDepth);
  DEBUGASSERT((int)historyTable.size() > dIndex)

  int64_t histMax = historyMax[dIndex];
  if(histMax == 0)
    return 0;

  int hIndex = getHistoryIndex(move);
  int64_t score = historyTable[dIndex][hIndex];
  if(score > histMax)
    score = histMax;
  return (int)(score*SearchParams::HISTORY_SCORE_MAX/histMax);
}




/*

//Requires startingBoard array
void Searcher::pruneSuicides(Board& b, int cDepth, move_t* mvB, int* hmB, int num)
{
  pla_t pla = b.player;
  for(int i = 0; i < num; i++)
  {
    //Find first pla move
    int s = 0;
    step_t step;
    for(int s = 0; s<4; s++)
    {
      step = getStep(mvB[i],s);
      if(step == ERRSTEP || step == PASSSTEP)
      {s = 4; break;}
      if(b.owners[gSrc(step)] == pla)
      {break;}
    }
    if(s == 4)
    {continue;}

    if(!b.isTrapSafe2(pla,gDest(step)))
    {hmB[i] = SUICIDEVAL;}
    else
    {
      loc_t kt = Board::CHECKCAPSAT[i];
      if(kt != ERRLOC && b.owners[kt] == pla && !b.isTrapSafe2(pla,kt) && startingBoard[cDepth/4].owners[kt] != pla)
      {
        hmB[i] = SUICIDEVAL;
      }
    }
  }
}
*/


//PV-------------------------------------------------------------------------------

void SearchUtils::endPV(int& pvLen)
{
  pvLen = 0;
}

void SearchUtils::copyPV(const move_t* srcpv, int srcpvLen, move_t* destpv, int& destpvLen)
{
  for(int i = 0; i<srcpvLen; i++)
    destpv[i] = srcpv[i];
  destpvLen = srcpvLen;
}

void SearchUtils::copyExtendPV(move_t move, const move_t* srcpv, int srcpvLen, move_t* destpv, int& destpvLen)
{
  destpv[0] = move;
  int pvArraySize = SearchParams::PV_ARRAY_SIZE;
  int newLen = min(srcpvLen+1,pvArraySize);
  for(int i = 1; i<newLen; i++)
    destpv[i] = srcpv[i-1];
  destpvLen = newLen;
}


//HASHTABLE-------------------------------------------------------------------------------------------

SearchHashEntry::SearchHashEntry()
{
  data = 0;
  key = 0;
}

void SearchHashEntry::record(hash_t hash, int16_t depth, eval_t eval, flag_t flag, move_t move)
{
  //Assert that eval uses only only 21 bits (except for sign extension)
  DEBUGASSERT((eval & 0xFFF00000) == 0 || (eval & 0xFFF00000) == 0xFFF00000);
  //Assert that flag uses only 2 bits
  DEBUGASSERT((flag & 0xFFFFFFFC) == 0);
  //Assert that depth uses only 9 bits (except for sign extension)
  DEBUGASSERT((depth & 0xFFFFFF00) == 0 || (depth & 0xFFFFFF00) == 0xFFFFFF00);

  //Attempt to avoid overwriting the old hashmove if we're storing an ERRMOVE
  //Or if we're storing a fail-low result whose bestmove might be bad and we have an earlier result in the table
  if(move == ERRMOVE || flag == Flags::FLAG_ALPHA)
  {
    uint64_t oldRData = data;
    uint64_t oldRKey = key;
    //Verify that hash key matches, using lockless xor scheme
    //And if so, then grab the old hashmove out and replace our move with it
    if((oldRKey ^ oldRData) == hash)
    {
      move_t oldMove = (move_t)(oldRData >> 32);
      if(oldMove != ERRMOVE)
        move = oldMove;
    }
  }

  //Put it all together!
  uint32_t subWord = ((uint32_t)eval << 11) | (((uint32_t)depth & 0x1FF) << 2) | (uint32_t)flag;
  uint64_t rData = ((uint64_t)move << 32) | subWord;

  data = rData;
  key = hash ^ rData;
}

bool SearchHashEntry::lookup(hash_t hash, int16_t& depth, eval_t& eval, flag_t& flag, move_t& move)
{
  uint64_t rData = data;
  uint64_t rKey = key;

  //Verify that hash key matches, using lockless xor scheme
  if((rKey ^ rData) != hash)
    return false;

  uint32_t subWord = (uint32_t)rData;
  flag_t theFlag = (flag_t)(subWord & 0x3);

  //Ensure there's supposed to be an entry here
  if(theFlag == Flags::FLAG_NONE)
    return false;

  flag = theFlag;
  move = (move_t)(rData >> 32);
  depth = (int16_t)(((int32_t)(subWord << 21)) >> 23);
  eval = (eval_t)((int32_t)subWord >> 11);

  return true;
}

int SearchHashTable::getExp(uint64_t maxMem)
{
  uint64_t maxEntries = maxMem / sizeof(SearchHashEntry);
  int shift = 0;
  while(shift < 63 && (1ULL << (shift+1)) <= maxEntries)
    shift++;
  return max(shift,10);
}

SearchHashTable::SearchHashTable(int exp)
{
  exponent = exp;
  size = ((hash_t)1) << exponent;
  mask = size-1;
  clearOffset = 0;
  entries = new SearchHashEntry[size];
  clear();
}

SearchHashTable::~SearchHashTable()
{
  delete[] entries;
}

void SearchHashTable::clear()
{
  if(clearOffset <= 0 || clearOffset >= size - 2)
  {
    //A hash of value i+1 can only get stored in an index of value i if clearOffset == size - 1
    //so that adding it to the indended index causes it to wrap around.
    for(hash_t i = 0; i<size; i++)
      entries[i].record(i+1,-64,0,Flags::FLAG_NONE,ERRMOVE);
    clearOffset = 1;
  }
  else
  {
    clearOffset++;
  }
}

bool SearchHashTable::lookup(move_t& hashMove, eval_t& hashEval, int16_t& hashDepth, flag_t& hashFlag,
    const Board& b, int cDepth, bool isQSearchOrEval)
{
  //Compute appropriate slot
  //Only mix in the start position hash if we're not qsearch or eval - qsearch is fine taking a hash that ignores
  //the starting position because it allows returning to the same position and qpassing, and of course
  //eval also doesn't care about history
  hash_t hash = b.sitCurrentHash + (SearchParams::STRICT_HASH_SAFETY && b.step != 0 && !isQSearchOrEval ?
      (hash_t)2862933555777941757ULL*b.posStartHash + (hash_t)3037000493ULL : (hash_t)0);
  int hashSlot = (int)((hash+clearOffset) & mask);

  //Attempt to look up the entry
  if(entries[hashSlot].lookup(hash,hashDepth,hashEval,hashFlag,hashMove))
  {
    //Adjust for terminal eval
    if(SearchUtils::isWinEval(hashEval))
      hashEval -= cDepth;
    else if(SearchUtils::isLoseEval(hashEval))
      hashEval += cDepth;
    return true;
  }
  return false;
}

void SearchHashTable::record(const Board& b, int cDepth,
    int16_t depth, eval_t eval, flag_t flag, move_t move, bool isQSearchOrEval)
{
  //Don't ever record evals that were obtained through some sort of illegal move check
  if(SearchUtils::isIllegalityTerminalEval(eval))
    return;

  //Adjust for win loss scores
  if(SearchUtils::isWinEval(eval))
    eval = min(Eval::WIN,eval+cDepth);
  else if(SearchUtils::isLoseEval(eval))
    eval = max(Eval::LOSE,eval-cDepth);

  //Record the hash!
  //In the case that we're qsearch or eval and we need to record a hash that's partway through the turn,
  //we record both ways so that consumers that do or don't care about the startpos can both find the entry.
  bool recordWithStartPos = SearchParams::STRICT_HASH_SAFETY && b.step != 0;
  bool recordWithoutStartPos = !recordWithStartPos || isQSearchOrEval;
  if(recordWithStartPos)
  {
    hash_t hash0 = b.sitCurrentHash + (hash_t)2862933555777941757ULL*b.posStartHash + (hash_t)3037000493ULL;
    int hashSlot0 = (int)((hash0+clearOffset) & mask);
    entries[hashSlot0].record(hash0,depth,eval,flag,move);
  }
  if(recordWithoutStartPos)
  {
    hash_t hash1 = b.sitCurrentHash;
    int hashSlot1 = (int)((hash1+clearOffset) & mask);
    entries[hashSlot1].record(hash1,depth,eval,flag,move);
  }
}

ExistsHashTable::ExistsHashTable(int exp)
{
  if(exp > 21)
    Global::fatalError("Cannot have ExistsHashTable size with exp > 21!");

  exponent = exp;
  size = ((hash_t)1) << exponent;
  mask = size-1;
  hashKeys = new hash_t[size];
  clearOffset = 0;
  clear();
}

ExistsHashTable::~ExistsHashTable()
{
  delete[] hashKeys;
}

void ExistsHashTable::clear()
{
  if(clearOffset <= 0 || clearOffset >= size - 2)
  {
    //A hash of value i+1 can only get stored in an index of value i if clearOffset == size - 1
    //so that adding it to the indended index causes it to wrap around.
    for(hash_t i = 0; i<size; i++)
      hashKeys[i] = (hash_t)i+1;
    clearOffset = 1;
  }
  else
  {
    clearOffset++;
  }
}

bool ExistsHashTable::lookup(const Board& b)
{
  hash_t hash = b.sitCurrentHash;
  int hashIndex = (int)((hash+clearOffset) & mask);
  if(hashKeys[hashIndex] == hash)
    return true;
  hash_t hash2 = (hash >> exponent) | (hash << (64-exponent));
  int hashIndex2 = (int)((hash2+clearOffset) & mask);
  if(hashKeys[hashIndex2] == hash2)
    return true;
  hash_t hash3 = (hash >> (exponent*2)) | (hash << (64-exponent*2));
  int hashIndex3 = (int)((hash3+clearOffset) & mask);
  if(hashKeys[hashIndex3] == hash3)
    return true;
  return false;
}

void ExistsHashTable::record(const Board& b)
{
  hash_t hash = b.sitCurrentHash;
  hash_t hash2 = (hash >> exponent) | (hash << (64-exponent));
  hash_t hash3 = (hash >> (exponent*2)) | (hash << (64-exponent*2));
  int hashIndex = (int)((hash+clearOffset) & mask);
  int hashIndex2 = (int)((hash2+clearOffset) & mask);
  int hashIndex3 = (int)((hash3+clearOffset) & mask);
  hashKeys[hashIndex] = hash;
  hashKeys[hashIndex2] = hash2;
  hashKeys[hashIndex3] = hash3;
}

