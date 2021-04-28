
/*
 * boardtrees.h
 * Author: David Wu
 *
 * Board functions for complex detections and move generation
 */

#ifndef BOARDTREES_H_
#define BOARDTREES_H_

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"

namespace BoardTrees
{
  //CAP TREE-------------------------------------------------------------------------
  //All the basic canCaps/genCaps functions should be safe with tempSteps

  bool canCaps(Board& b, pla_t pla, int steps);
  int genCaps(Board& b, pla_t pla, int steps, move_t* mv, int* hm);

  bool canCaps(Board& b, pla_t pla, int steps, int minSteps, loc_t kt);

  //capMap is not cleared, it is only set with bits when captures are found, and is
  //not mutated if captures are not found.
  //capDist is ONLY well-defined when captures are found, and may be mutated and nonsensical otherwise
  bool canCaps(Board& b, pla_t pla, int steps, loc_t kt, Bitmap& capMap, int& capDist);

  int genCaps(Board& b, pla_t pla, int steps, int minSteps, loc_t kt, move_t* mv, int* hm);
  int genCapsExtended(Board& b, pla_t pla, int steps, int minSteps, bool suicideExtend, loc_t kt, move_t* mv, int* hm, int* stepsUsed = NULL);
  int genCapsFull(Board& b, pla_t pla, int steps, int minSteps, bool suicideExtend, loc_t kt, move_t* mv, int* hm);

  //CAP DEFENSE----------------------------------------------------------------------

  //NOTE: Should NOT be called with numSteps <= 0, will do an out-of-bounds array access if so
  int genTrapDefs(Board& b, pla_t pla, loc_t kt, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv);

  int shortestGoodTrapDef(Board& b, pla_t pla, loc_t kt, move_t* mv, int num, const Bitmap& currentWholeCapMap);

  int genRunawayDefs(Board& b, pla_t pla, loc_t ploc, int numSteps, bool chainUFRunaways, move_t* mv);

  //ATTACKS--------------------------------------------------------------------------
  int genFreezeAttacks(Board& b, pla_t pla, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv);
  int genDistantTrapAttacks(Board& b, pla_t pla, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv);
  int genDistantAdvances(Board& b, pla_t pla, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv);

  //ELIMINATION---------------------------------------------------------------------
  bool canElim(Board& b, pla_t pla, int numSteps);

  //GOAL TREE------------------------------------------------------------------------
  int goalDist(Board& b, pla_t pla, int steps);
  int goalDist(Board& b, pla_t pla, int steps, loc_t rloc);

  //TODO perhaps add a feature to move predictor that checks for multiple independent goal threats using this.
  //Gets the full bitmap of rabbits threatening to goal
  Bitmap goalThreatRabsMap(Board& b, pla_t pla, int steps);

  //Apply a filter - only consider goal threats by rabs in the given area
  int goalDistForRabs(Board& b, pla_t pla, int steps, Bitmap rabFilter);

  //GOAL THREAT---------------------------------------------------------------------
  int genSevereGoalThreats(Board& b, pla_t pla, int numSteps, move_t* mv);

  int genGoalThreats(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genGoalThreatsRec(Board& b, pla_t pla, Bitmap relevant, int numStepsLeft, move_t* mv,
      move_t moveSoFar, int numStepsSoFar);

  int genGoalAttacks(Board& b, pla_t pla, int numSteps, move_t* mv);

  //TRAP DEFENSES-------------------------------------------------------------------
  int genImportantTrapDefenses(Board& b, pla_t pla, int numSteps, move_t* mv);

  //MISC----------------------------------------------------------------------------
  int genContestedDefenses(Board& b, pla_t pla, loc_t kt, int numSteps, move_t* mv);
  int genPhalanxesIfNeeded(Board& b, pla_t pla, int numSteps, loc_t loc, loc_t from, move_t* mv);
  int genFramePhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genTrapBackPhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genBackRowMiscPhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genOppElePhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genUsefulElephantMoves(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genCamelHostageRunaways(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genRabSideAdvances(Board& b, pla_t pla, int numSteps, move_t* mv);
  int genDistantTrapFightSwarms(Board& b, pla_t pla, int numSteps, move_t* mv);

  //RETRO---------------------------------------------------------------------------
  int genPP3Step(Board& b, move_t* mv, loc_t oloc);
  int genPP4Step(Board& b, move_t* mv, loc_t oloc);

  int genPPTo2Step(Board& b, move_t* mv, loc_t oloc, loc_t loc);
  int genPPTo3Step(Board& b, move_t* mv, loc_t oloc, loc_t loc);
  int genPPTo4Step(Board& b, move_t* mv, loc_t oloc, loc_t loc);

  int genStepGeqTo1S(Board& b, move_t* mv, loc_t loc, loc_t ign, piece_t piece);
  int genStepGeqAround1S(Board& b, move_t* mv, loc_t loc, loc_t ign, piece_t piece);
  int genStuffings2S(Board& b, move_t* mv, pla_t pla, loc_t loc);

  int genStepAndPush3Step(Board& b, move_t* mv, loc_t oloc);
}


#endif /* BOARDTREES_H_ */
