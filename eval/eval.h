
/*
 * eval.h
 * Author: davidwu
 */

#ifndef EVAL_H
#define EVAL_H

#include "../eval/threats.h"
#include "../eval/strats.h"

class EvalParams;

namespace Eval
{
  //CONSTANTS--------------------------------------------------------------------
  //Evaluation approximately in millirabbits

  const eval_t LOSE = -1000000;                //LOSE + X = Lose in X steps
  const eval_t WIN = 1000000;                  //WIN - X = Win in X steps
  const eval_t LOSE_TERMINAL = LOSE + 10000;   //Highest value that still is a loss
  const eval_t WIN_TERMINAL = WIN - 10000;     //Lowest value that still is a win
  const eval_t LOSE_MAYBE = LOSE + 200000;  //Value for something that is possibly a loss but not sure due to pruning
  const eval_t WIN_MAYBE = WIN - 200000;    //Value for something that is possibly a win but not sure due to pruning

  //EXTRAEVAL:
  //Note that it's possible that due to hashtable interactions, evals close to the maximum are produced by the
  //reversible move extraeval penalties (ex: Gold can shuffle pieces to repeatedly force Silver to either
  //make 4-2 reversibles or else lose the game).
  //Max amount that a single extraeval bonus or penalty can convey for root ordering.
  const eval_t EXTRA_ROOT_EVAL_MAX = 100000;
  //Extraeval cannot affect evals outside this range
  const eval_t EXTRA_EVAL_MAX_RANGE = 120000;

  //Evaluate the current board position and return the score.
  //It is okay if mainPla is NPLA.
  //If out is not NULL, then will use out to print out some debugging info
  eval_t evaluate(Board& b, pla_t mainPla, eval_t earlyBlockadePenalty, ostream* out);

  //INITIALIZATION-------------------------------------------------------------------

  //Initialize eval tables. Call this before using Eval!
  void init();

  //Called by init
  void initMaterial();
  void initLogistic();

  //MATERIAL--------------------------------------------------------------

  //Compute an integer that uniquely represents the current material distribution for pla's pieces alone.
  int getMaterialIndex(const Board& b, pla_t pla);

  //Get the material score from pla's perspective on the given board.
  eval_t getMaterialScore(const Board& b, pla_t pla);

  //Fill arrays indicating for each player and piece type, its base material value.
  void initializeValues(const Board& b, eval_t pValues[2][NUMTYPES]);

  //TRAP CONTROL-------------------------------------------------------------
  //Basic trap control evaluation
  //100 to 70 Iron control
  //40 to 60 Decent control
  //30 to 20 Tenuous control
  //0 to 10 - Shared

  void getBasicTrapControls(const Board& b, const int pStronger[2][NUMTYPES],
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDists[BSIZE], int tc[2][4]);

  //Approx contribution of the elephant to trap control
  int elephantTrapControlContribution(const Board& b, pla_t pla, const int ufDists[BSIZE], int trapIndex);

  eval_t getTrapControlScore(pla_t pla, int trapIndex, int control);

  //PIECE THREAT--------------------------------------------------------------

  void getInfluence(const Board& b, const int pStronger[2][NUMTYPES], int influence[2][BSIZE]);

  //CAP THREATS--------------------------------------------------------------------------
  int evalCapOccur(const Board& b, const eval_t values[NUMTYPES], loc_t loc, int dist);

  //STRATS--------------------------------------------------------------------------------
  const int frameThreatMax = Strats::maxFramesPerKT*4;
  const int hostageThreatMax = Strats::maxHostagesPerKT*2;
  const int blockadeThreatMax = Strats::maxBlockadesPerPla;
  const int numStratsMax = frameThreatMax + hostageThreatMax + blockadeThreatMax;

  //The top level function. Compute all strats and eval them, storing the count in numPStrats, the strats themselves
  //in pStrats, and the total score if all strats are used in stratScore. Updates pieceThreats with the new
  //value for strats that threaten pieces. Note that the returned strats values are only the value that they
  //add above and beyond the value in pieceThreats.
  //ufDist is not const because it is temporarily modified for the computation
  void getStrats(Board& b, const eval_t pValues[2][NUMTYPES], const int pStronger[2][NUMTYPES],
      const Bitmap pStrongerMaps[2][NUMTYPES], int ufDist[BSIZE], const int tc[2][4],
      eval_t pieceThreats[BSIZE], int numPStrats[2], Strat pStrats[2][numStratsMax], eval_t stratScore[2],
      pla_t mainPla, eval_t earlyBlockadePenalty, eval_t mobilityScorePla, eval_t mobilityScoreOpp, ostream* out);

  void getMobility(const Board& b, pla_t pla, loc_t ploc, const Bitmap pStrongerMaps[2][NUMTYPES],
      const int ufDist[BSIZE], Bitmap mobMap[5]);

  eval_t getEMobilityScore(const Board& b, pla_t pla, const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], ostream* out);

}


#endif
