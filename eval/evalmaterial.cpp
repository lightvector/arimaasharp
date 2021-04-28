
/*
 * evalmaterial.cpp
 * Author: davidwu
 */

#include <cmath>
#include "../board/board.h"
#include "../eval/eval.h"

//Available evals
//Note: all material values are computed in centirabbits, - mult by 10 for real eval.
//This is party for safety, since we want to fit them into shorts.
//static short computeFAMEScore(Board& b, pla_t pla);
static short computeHarLogScore(Board& b, pla_t pla);

//Precomputed array for all possible material combinations of the value of that material combo. [plaindex][oppindex]
static short MATERIAL_SCORES[972][972];

//Constants for index
static const int MINDEX_MAX = 972;
static const int MINDEX_E = 486;
static const int MINDEX_M = 243;
static const int MINDEX_H = 81;
static const int MINDEX_D = 27;
static const int MINDEX_C = 9;
static const int MINDEX_R = 1;
static const int MINDEX_OF_PIECE[7] = {0,1,9,27,81,243,486};

//Min and max possible material scores. Must fit within a signed short.
static const short MSCORE_MIN = -30000;
static const short MSCORE_MAX = 30000;

static const int VALUECAP = 20000; //Pieces cannot be worth more than this for initializeValues
static const int RABVALUECAP = 7000; //And rabbits cannot be worth more than this

//INTERFACE----------------------------------------------------------------------

int Eval::getMaterialIndex(const Board& b, pla_t pla)
{
  return
  b.pieceCounts[pla][ELE] * MINDEX_E +
  b.pieceCounts[pla][CAM] * MINDEX_M +
  b.pieceCounts[pla][HOR] * MINDEX_H +
  b.pieceCounts[pla][DOG] * MINDEX_D +
  b.pieceCounts[pla][CAT] * MINDEX_C +
  b.pieceCounts[pla][RAB] * MINDEX_R;
}

eval_t Eval::getMaterialScore(const Board& b, pla_t pla)
{
  pla_t opp = gOpp(pla);
  int plaIndex = getMaterialIndex(b,pla);
  int oppIndex = getMaterialIndex(b,opp);
  int score = MATERIAL_SCORES[plaIndex][oppIndex];
  return score*10;
}

void Eval::initializeValues(const Board& b, eval_t pValues[2][NUMTYPES])
{
  int sBaseIndex = getMaterialIndex(b,SILV);
  int gBaseIndex = getMaterialIndex(b,GOLD);
  int baseScore = MATERIAL_SCORES[sBaseIndex][gBaseIndex];

  //Zero out the null pieces, just in case
  pValues[SILV][0] = 0;
  pValues[GOLD][0] = 0;

  //For each piece type
  for(int i = 1; i<NUMTYPES; i++)
  {
    //If there are pieces of that type
    if(b.pieceCounts[SILV][i] > 0)
    {
      //Simulate removal of a piece and evaluate material score
      int sIndex = sBaseIndex - MINDEX_OF_PIECE[i];

      //Take difference, then add back
      //TODO swap the access to MATERIAL_SCORES and negate for better cache friendliness?
      int value = 10*(baseScore - MATERIAL_SCORES[sIndex][gBaseIndex]);
      if(value > VALUECAP) value = VALUECAP;
      if(i == RAB && value > RABVALUECAP)  value = RABVALUECAP;
      pValues[SILV][i] = value;
    }
    else
      pValues[SILV][i] = pValues[SILV][i-1];

    if(b.pieceCounts[GOLD][i] > 0)
    {
      int gIndex = gBaseIndex - MINDEX_OF_PIECE[i];

      int value = -10*(baseScore - MATERIAL_SCORES[sBaseIndex][gIndex]);
      if(value > VALUECAP) value = VALUECAP;
      if(i == RAB && value > RABVALUECAP) value = RABVALUECAP;
      pValues[GOLD][i] = value;
    }
    else
      pValues[GOLD][i] = pValues[GOLD][i-1];
  }
}

void Eval::initMaterial()
{
  //Initialize material lookup table------
  Board b;
  b.setPlaStep(GOLD,0);
  for(int i = 0; i<MINDEX_MAX; i++)
  {
    int k = i;
    b.pieceCounts[GOLD][ELE] = k/MINDEX_E;  k = k%MINDEX_E;
    b.pieceCounts[GOLD][CAM] = k/MINDEX_M;  k = k%MINDEX_M;
    b.pieceCounts[GOLD][HOR] = k/MINDEX_H;  k = k%MINDEX_H;
    b.pieceCounts[GOLD][DOG] = k/MINDEX_D;  k = k%MINDEX_D;
    b.pieceCounts[GOLD][CAT] = k/MINDEX_C;  k = k%MINDEX_C;
    b.pieceCounts[GOLD][RAB] = k/MINDEX_R;
    b.pieceCounts[GOLD][0] =
      b.pieceCounts[GOLD][ELE] + b.pieceCounts[GOLD][CAM] + b.pieceCounts[GOLD][HOR] +
      b.pieceCounts[GOLD][DOG] + b.pieceCounts[GOLD][CAT] + b.pieceCounts[GOLD][RAB];

    for(int j = 0; j<MINDEX_MAX; j++)
    {
      k = j;
      b.pieceCounts[SILV][ELE] = k/MINDEX_E;  k = k%MINDEX_E;
      b.pieceCounts[SILV][CAM] = k/MINDEX_M;  k = k%MINDEX_M;
      b.pieceCounts[SILV][HOR] = k/MINDEX_H;  k = k%MINDEX_H;
      b.pieceCounts[SILV][DOG] = k/MINDEX_D;  k = k%MINDEX_D;
      b.pieceCounts[SILV][CAT] = k/MINDEX_C;  k = k%MINDEX_C;
      b.pieceCounts[SILV][RAB] = k/MINDEX_R;
      b.pieceCounts[SILV][0] =
        b.pieceCounts[SILV][ELE] + b.pieceCounts[SILV][CAM] + b.pieceCounts[SILV][HOR] +
        b.pieceCounts[SILV][DOG] + b.pieceCounts[SILV][CAT] + b.pieceCounts[SILV][RAB];

      MATERIAL_SCORES[i][j] = computeHarLogScore(b,GOLD);
    }
  }
}


//FAME----------------------------------------------------------------------------

/*
//Constants for computing FAME
const int FAME_NUM_MATCHES = 8;
const int FAME_MATCH_VALUES[] = {256, 85, 57, 38, 25, 17, 11, 7};
const double FAME_FACTOR = 3.02;
const int FAME_RABBIT_FACTOR = 600;

static short computeFAMEScore(Board& b, pla_t pla)
{
  pla_t opp = gOpp(pla);

  if(b.pieceCounts[pla][RAB] == 0 || b.pieceCounts[opp][RAB] == 0)
    return 0;
  else if(b.pieceCounts[pla][RAB] == 0)
    return MSCORE_MIN;
  else if(b.pieceCounts[opp][RAB] == 0)
    return MSCORE_MAX;

  int pieceIndexPla = ELE;
  int pieceIndexOpp = ELE;
  int numPla = b.pieceCounts[pla][pieceIndexPla];
  int numOpp = b.pieceCounts[opp][pieceIndexOpp];
  int score = 0;
  int i;

  for(i = 0; i<FAME_NUM_MATCHES; i++)
  {
    while(numPla <= 0 && pieceIndexPla >= RAB)
    {
      pieceIndexPla--;
      numPla = b.pieceCounts[pla][pieceIndexPla];
    }
    while(numOpp <= 0 && pieceIndexOpp >= RAB)
    {
      pieceIndexOpp--;
      numOpp = b.pieceCounts[opp][pieceIndexOpp];
    }

    if(pieceIndexOpp <= RAB && pieceIndexPla <= RAB)
      break;

    if(pieceIndexPla > pieceIndexOpp)
      score += FAME_MATCH_VALUES[i];
    else if(pieceIndexPla < pieceIndexOpp)
      score -= FAME_MATCH_VALUES[i];

    numPla--;
    numOpp--;
  }

  int plaRabs = b.pieceCounts[pla][RAB];
  int oppRabs = b.pieceCounts[opp][RAB];
  int plaRabsL = b.pieceCounts[pla][0] - i;
  int oppRabsL = b.pieceCounts[opp][0] - i;
  if(plaRabsL > plaRabs) {plaRabsL = plaRabs;}
  if(oppRabsL > oppRabs) {oppRabsL = oppRabs;}

  score += plaRabsL * FAME_RABBIT_FACTOR/(oppRabs + 2*(b.pieceCounts[opp][0]-oppRabs));
  score -= oppRabsL * FAME_RABBIT_FACTOR/(plaRabs + 2*(b.pieceCounts[pla][0]-plaRabs));

  score = (int)(score > 0 ? floor(score * FAME_FACTOR + 0.5) : ceil(score * FAME_FACTOR - 0.5));

  return score;
}*/

//HARLOG---------------------------------------------------------------------------

//Constants for computing HARLOG
const double HARLOG_Q = 1.447530126;
const double HARLOG_G = 0.6314442034;
const double HARLOG_NORMAL = 1.0/0.12507009891301202;
const double HARLOG_SBONUS = 2.0;

static short computeHarLogScore(Board& b, pla_t pla)
{
  pla_t opp = gOpp(pla);
  if(b.pieceCounts[pla][RAB] == 0 && b.pieceCounts[opp][RAB] == 0)
    return 0;
  else if(b.pieceCounts[pla][RAB] == 0)
    return MSCORE_MIN;
  else if(b.pieceCounts[opp][RAB] == 0)
    return MSCORE_MAX;

  int plaNumStronger[NUMTYPES];
  int oppNumStronger[NUMTYPES];
  plaNumStronger[ELE] = 0;
  oppNumStronger[ELE] = 0;
  for(int piece = CAM; piece >= RAB; piece--)
  {
    plaNumStronger[piece] = plaNumStronger[piece+1] + b.pieceCounts[opp][piece+1];
    oppNumStronger[piece] = oppNumStronger[piece+1] + b.pieceCounts[pla][piece+1];
  }
  double plaScore = 0;
  double oppScore = 0;
  for(int piece = ELE; piece >= CAT; piece--)
  {
    plaScore += 1.0/(HARLOG_Q+plaNumStronger[piece]) * b.pieceCounts[pla][piece];
    oppScore += 1.0/(HARLOG_Q+oppNumStronger[piece]) * b.pieceCounts[opp][piece];
  }
  plaScore += HARLOG_G * log((double)b.pieceCounts[pla][RAB] * b.pieceCounts[pla][0]);
  oppScore += HARLOG_G * log((double)b.pieceCounts[opp][RAB] * b.pieceCounts[opp][0]);

  plaScore *= HARLOG_NORMAL;
  oppScore *= HARLOG_NORMAL;
  return (int)(plaScore*100) - (int)(oppScore*100);
}

