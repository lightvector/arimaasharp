/*
 * internal.h
 * Author: davidwu
 */

//Internal functions for evaluation

#ifndef INTERNAL_H_
#define INTERNAL_H_

#include "../core/global.h"
#include "../board/board.h"
#include "../eval/strats.h"

namespace Logistic
{
  void init();

  //Logistics are out of 1024, and stretched by a factor of 20, or 16, from the standard logistic
  extern int LOGISTIC_ARR20[255];
  extern int LOGISTIC_ARR16[255];
  extern int LOGISTIC_INTEGRAL_ARR[255];
  static const int PROP_SHIFT = 10; //out of 1024

  int prop20(int x);
  int prop16(int x);
}

namespace PieceSquare
{
  //Simple straightforward piece-square bonus
  eval_t get(const Board& b, const int pStronger[2][NUMTYPES]);

  void print(const Board& b, const int pStronger[2][NUMTYPES], ostream& out);
}

namespace UFDist
{
  const int MAX_UF_DIST = 6;

  //Fill ufDist with estimated distance to unfreeze each frozen or immo piece.
  //Includes pieces that are immo/blockaded but not technically frozen.
  void get(const Board& b, int ufDist[BSIZE], const Bitmap pStrongerMaps[2][NUMTYPES]);
}

namespace TrapControl
{
  //Simple eval bonus for just having pieces next to traps, regardless of control status
  eval_t getSimple(const Board& b);

  //Fill tc with simple trap control values
  //100 to 70 Iron control
  //40 to 60 Decent control
  //30 to 20 Tenuous control
  //0 to 10 - Shared
  //tcEle is the portion of the tc that comes from the pla's elephant
  void get(const Board& b, const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDists[BSIZE], int tc[2][4], int tcEle[2][4]);

  //Get the eval associated with some trap control
  eval_t getEval(pla_t pla, const int tc[2][4]);

  //For debug output
  void printEval(pla_t pla, const int tc[2][4], ostream& out);

  //Approx contribution of a piece to a trap, somewhat hacky
  int pieceContribution(const Board& b,
      const int pStronger[2][NUMTYPES], const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDists[BSIZE],
      pla_t owner, piece_t piece, loc_t loc, loc_t trapLoc);
}


namespace Mobility
{
  //len == length of eleCanReachInN should be this much longer in length than the max passed in, and [0,max-1] will be trustable
  const int EXTRA_LEN = 2;
  //If changing these, change the appropriate arrays in pthreats
  #define EMOB_MAX 7
  #define EMOB_ARR_LEN 9
  void getEle(const Board& b, pla_t pla, Bitmap* eleCanReachInN, int max, int len);

  eval_t getEval(const Board& b, const Bitmap eleCanReach[EMOB_ARR_LEN], pla_t pla);

  void print(const Board& b, const Bitmap* eleCanReachInN, pla_t pla, int max, ostream& out);
}

namespace PThreats
{
  //Accumulate score for threats against pieces in an attack and takeover of a trap
  void accumOppAttack(const Board& b,
      const Bitmap pStrongerMaps[2][NUMTYPES], const int pValues[2][NUMTYPES], const int ufDist[BSIZE],
      const int tc[2][4], const Bitmap eleCanReach[2][EMOB_ARR_LEN], eval_t trapThreats[2][4], eval_t pieceThreats[BSIZE]);

  //Accumulate score for threats against pieces pushed or pulled to a home trap
  void accumBasicThreats(const Board& b, const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int pValues[2][NUMTYPES], const int ufDist[BSIZE],
    const int tc[2][4], const Bitmap eleCanReach[2][EMOB_ARR_LEN],
    eval_t trapThreats[2][4], eval_t reducibleTrapThreats[2][4], eval_t pieceThreats[BSIZE]);

  //Accumulate score additionally for threats against heavy advanced pieces
  void accumHeavyThreats(const Board& b, const int pStronger[2][NUMTYPES],
      const Bitmap pStrongerMaps[2][NUMTYPES], const int pValues[2][NUMTYPES], const int ufDist[BSIZE],
      const int tc[2][4], eval_t trapThreats[2][4], eval_t pieceThreats[BSIZE]);

  //For debug output
  void printEval(pla_t pla, const eval_t trapThreats[2][4], ostream& out);

  //Eval training and learning
  void getBasicThreatCoeffsAndBases(vector<string>& names, vector<double*>& coeffs,
      vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale);

  eval_t adjustForReducibleThreats(const Board& b, pla_t pla,
      const eval_t trapThreats[2][4], const eval_t reducibleTrapThreats[2][4],
      const int numPStrats[2], const Strat pStrats[2][Strats::numStratsMax], loc_t& bestGainAtBuf);
}

namespace Placement
{
  //Some logic to try to get the camel to hang around the right places
  eval_t getSheriffAdvancementThreat(const Board& b, pla_t pla,
      const int tc[2][4], const int tcEle[2][4], ostream* out);

  //Adds a bonus for the opponent when a home trap has no >= piece than an attacker on that side of the board.
  void getAlignmentScore(const Board& b, const int ufDist[BSIZE], eval_t evals[2], ostream* out);

  //Computes a penalty for when own pieces are imbalanced
  void getImbalanceScore(const Board& b, const int ufDist[BSIZE], eval_t evals[2], ostream* out);
}

namespace RabGoal
{
  void init();

  //Get the eval for advanced threatening rabbits
  void getScore(const Board& b, const int ufDist[BSIZE], const int tc[2][4],
      ostream* out, eval_t rabThreatScore[2]);
}

namespace EvalShared
{
  //TODO comment this
  int computeLogistic(double prop, double total, int scale);
  //TODO comment this
  double computeSFPAdvGoodFactor(const Board& b, pla_t pla, loc_t sfpCenter, const double* advancementGood);
  //TODO comment this
  int computeSFPScore(const double* pieceCountsPla, const double* pieceCountsOpp,
      double* advancementGood = NULL, piece_t breakPenaltyPiece = EMP);
}

namespace Hostages
{
  int getStrats(const Board& b, pla_t pla, const eval_t pValues[2][NUMTYPES], const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], const int tc[2][4],
    Strat* strats, ostream* out);
}

namespace Frames
{
  int getStrats(const Board& b, pla_t pla, const eval_t pValues[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], int ufDist[BSIZE],
    Strat* strats, ostream* out);
}

namespace Blockades
{
  int fillBlockadeRotationArrays(const Board& b, pla_t pla, loc_t* holderRotLocs, int* holderRotDists,
      loc_t* holderRotBlockers, bool* holderIsSwarm, int* minStrNeededArr, Bitmap heldMap, Bitmap holderMap, Bitmap rotatableMap,
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE]);
  int evalEleBlockade(Board& b, pla_t pla, const int pStronger[2][NUMTYPES], const Bitmap pStrongerMaps[2][NUMTYPES],
      const int tc[2][4], int ufDist[BSIZE], pla_t mainPla, eval_t earlyBlockadePenalty, ostream* out);
}

namespace Captures
{
  eval_t getEval(Board& b, pla_t pla, int numSteps, const eval_t pValues[2][NUMTYPES],
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], const eval_t pieceThreats[BSIZE],
      const int numPStrats[2], const Strat pStrats[2][Strats::numStratsMax], eval_t& retRaw, loc_t& retCapLoc);
}

namespace Variance
{
  eval_t getAdjustment(eval_t eval, eval_t plaVarianceEval, eval_t oppVarianceEval);
}

namespace CoeffBasis
{
  //For single parameters
  void single(const string& name, double* x, double scale,
      vector<string>& names, vector<double*>& coeffs,
      vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale);

  //For arrays where the left end is higher, with basis vectors left-justified
  void arrayLeftHigh(const string& namePrefix, double* arr, int len, double lScale, double rScale,
      vector<string>& names, vector<double*>& coeffs,
      vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale);
  //For arrays where the right end is higher, with basis vectors right-justified
  void arrayRightHigh(const string& namePrefix, double* arr, int len, double lScale, double rScale,
      vector<string>& names, vector<double*>& coeffs,
      vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale);

  //For board-sized arrays, with basis vectors of rows and columns reflecting mirror and color symmetry
  void boardArray(const string& namePrefix, double arr[2][BSIZE], int len,
      double rowScale, double colScale,
      vector<string>& names, vector<double*>& coeffs,
      vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale);

  //For an array where the parameter should definitely be linear, with one basis vector constant on all entries
  //and one basis vector with a linear slope (slopeScale per index) equal to zero at center.
  void linear(const string& namePrefix, double* arr, int len, double constantScale, double slopeScale, double center,
      vector<string>& names, vector<double*>& coeffs,
      vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale);
}

#endif /* INTERNAL_H_ */
