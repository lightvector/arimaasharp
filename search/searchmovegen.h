/*
 * searchmovegen.h
 * Author: davidwu
 */

#ifndef SEARCHMOVEGEN_H_
#define SEARCHMOVEGEN_H_

#include "../board/board.h"
#include "../board/boardhistory.h"

class ExistsHashTable;

namespace SearchMoveGen
{
  //FULL MOVE GENERATION (ROOT) ---------------------------------------------------------
  //Generate all full legal moves in the board position
  void genFullMoves(const Board& b, const BoardHistory& hist, vector<move_t>& moves, int maxSteps,
      bool winLossPrune, int rootMaxNonRelTactics,
      ExistsHashTable* fullMoveHash, const vector<hash_t>* excludedHashes, const vector<move_t>* excludedStepSets);

  //Using the goal tree, get the full goaling move in the position, if it exists
  //Does not check for immo or elim winning moves.
  move_t getFullGoalMove(Board& b);

  //REGULAR MOVE GENERATION--------------------------------------------------------------
  //Generate all possible steps and pushpulls, as well a pass if legal.
  int genRegularMoves(const Board& b, move_t* mv);

  //CAPTURE-RELATED GENERATION-----------------------------------------------------------
  //Generate full capturing moves
  int genCaptureMoves(Board& b, int numSteps, move_t* mv, int* hm);
  //Generate moves that attempt to defend against capture
  int genCaptureDefMoves(Board& b, int numSteps, bool moreBranchy, move_t* mv, int* hm);
  int genCaptureDefMoves(Board& b, int numSteps, bool moreBranchy, move_t* mv, int* hm,
      bool oppCanCap[4], Bitmap& wholeCapMapBuf);

  //BLOCKADE-RELATED GENERATION----------------------------------------------------------
  //Generate moves that attempt to complete a potential blockade
  int genBlockadeMoves(Board& b, move_t* mv);

  //REVERSIBLE MOVES ------------------------------------------------------
  //Generates the following reverse moves (possibly not all of them if there are multiple variations on a way to step back)
  //1-step step a piece back to where the opp pushpulled it from
  //2-step unfreeze and then step back as well, optionally reverting the unfreezing piece as well
  //2-step push a piece back to where the opp pushpulled it from, dislodging an opp piece also somewhere
  //3-step unfreeze and then push back as well
  int genReverseMoves(const Board& b, const BoardHistory& boardHistory, int cDepth, move_t* mv);

  //QSEARCH------------------------------------------------------------------------------
  //Generate ordinary quiescence search moves, not including special win defense search moves.
  int genQuiescenceMoves(Board& b, const BoardHistory& hist, int cDepth, int qDepth4, move_t* mv, int* hm);

  //WIN GENERATION---------------------------------------------------------------------------

  struct RelevantArea
  {
    //Indicates a set of steps that is relevant for achieving a certain objective (like stopping goal or elim).
    //All steps that are not indicated in these bitmaps are provably not relevant to stopping the goal in that
    //there is no dependency - they could simply be made afterwards.
    Bitmap stepTo;   //Steps by pla pieces to these locations
    Bitmap stepFrom; //Steps by pla pieces from these locations
    Bitmap pushOpp;  //Pushes of these opps
    Bitmap pullOpp;  //Pulls of these opps
    Bitmap pushOppTo; //Pushes of opps to these locations
    Bitmap pullOppTo; //Pulls of opps to these locations
  };

  //If opp not threatening goal, returns false.
  //If opp threatening goal, returns the location of the threatening rabbit and the relevant
  //area for defense for the given number of defense steps.
  bool getGoalDefRelArea(Board& b, int maxSteps, loc_t& rlocBuf, RelevantArea& relBuf);
  //Same thing, but for elimination.
  //If there are no rabbits, trapsBuf is set to ERRLOC, otherwise if there is 1 trap involved,
  //both trapsBuf are set to that trap, else they are set to the 2 traps involved.
  bool getElimDefRelArea(Board& b, int maxSteps, loc_t trapsBuf[2], RelevantArea& relBuf);

  //If opp not threatening goal or elim, returns false.
  //Get the relevant area for defending against goal or elim, using all remaining steps in the turn.
  bool getWinDefRelArea(Board& b, RelevantArea& relBuf);

  //Assumes opponent is threatening goal. Generates moves for the appropriate given relArea and returns the num of defenses.
  //hm can be NULL, in which case no move ordering is produced
  int genGoalDefMoves(Board& b, move_t* mv, int* hm, int maxSteps, loc_t rloc, const RelevantArea& relArea);
  //Assumes opponent is threatening elim. Generates moves for the appropriate given relArea and returns the num of defenses.
  //hm can be NULL, in which case no move ordering is produced
  int genElimDefMoves(Board& b, move_t* mv, int* hm, int maxSteps, loc_t traps[2], const RelevantArea& relArea);

  //Checks if opponent is threatening goal or elim, and if so, generates the appropriate defenses.
  //Returns -1 if the opponent is not threatening either.
  //hm is allowed to be null, in which case there will be no ordering
  int genWinDefMovesIfNeeded(Board& b, move_t* mv, int* hm, int maxSteps);

  //WIN DEFENSE SEARCH GENERATION---------------------------------------------------------------------------

  //Perform a win defense search.
  //Finds the shortest or nearly-shortest moves for stopping a loss within a given number of steps.
  //Current player is defender, assumes pla does not goal threat (else pla could just win).
  //If a loss is forced, returns 0 and fills defenseSteps with the number of steps left in the turn.
  //If the opponent threatens but is stoppable, returns some moves to stop it (>= 0)
  //  and fills defenseSteps with the largest length such that all defenses of that length were generated.
  //If the opponent does not threaten at all, returns -1, and fills defenseSteps with 0.
  //Does NOT consider zugzwang loss.
  int genShortestFullWinDefMoves(Board& b, move_t* mv, int& numInteriorNodes, int& defenseSteps);

  //Determine the minimum number of steps needed to stop the opponent from winning, allowing at most maxSteps.
  //maxSteps must be leq than the number of steps left in the turn.
  //ASSUMING that it's not already an immediate win for the player to move.
  //Might not see or stop all ways the opponent could win such as those involving zugzwang or immo.
  int minWinDefSteps(Board& b, int maxSteps);

  //Uses minWinDefSteps, again not guaranteed to catch all forced losses in 2.
  bool definitelyForcedLossInTwo(Board& b);


  //RELEVANT TACTICS SEARCH GENERATION---------------------------------------------------------------------
  int genRelevantTactics(Board& b, bool genWinDef, move_t* mv, int* hm);
}

#endif
