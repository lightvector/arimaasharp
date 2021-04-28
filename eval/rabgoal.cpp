/*
 * rabgoal.cpp
 * Author: davidwu
 */
#include <cmath>
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtreeconst.h"
#include "../eval/threats.h"
#include "../eval/internal.h"
#include "../eval/evalparams.h"
#include "../program/arimaaio.h"
#include "../board/locations.h"

static const int SFPLEN = 3;
static const int SFP_STRONGEST_R1[SFPLEN] = {7,4,2};
static const int SFP_STRONGEST_R2[SFPLEN] = {28,14,7};
static const int SFP_STRONGEST_R3[SFPLEN] = {24,12,6};
static const int SFP_STRONGEST_R4[SFPLEN] = {16,8,4};
static const int SFP_STRONGEST_R5[SFPLEN] = {8,4,2};

static int getSFPPoints(const Board& b, pla_t pla, const int* pointsByStrengthPlace, Bitmap area)
{
  pla_t opp = gOpp(pla);
  int i = 0;
  int points = 0;
  for(piece_t piece = ELE; piece >= CAT && i < SFPLEN; piece--)
  {
    bool plaHas = (b.pieceMaps[pla][piece] & area).hasBits();
    bool oppHas = (b.pieceMaps[opp][piece] & area).hasBits();
    if(!plaHas && !oppHas)
      continue;
    if(plaHas && oppHas)
    {}
    else if(plaHas)
      points += pointsByStrengthPlace[i];
    else if(oppHas)
      points -= pointsByStrengthPlace[i];
    i++;
  }
  return points;
}

static int getAnyPiecePoints(const Board& b, pla_t pla, Bitmap area)
{
  pla_t opp = gOpp(pla);
  int anyPiecePoints = 0;
  anyPiecePoints += (b.pieceMaps[pla][0] & area).countBits();
  anyPiecePoints += (b.pieceMaps[pla][0] & area & ~b.pieceMaps[pla][RAB]).countBits();
  anyPiecePoints -= (b.pieceMaps[opp][0] & area).countBits();
  anyPiecePoints -= (b.pieceMaps[opp][0] & area & ~b.pieceMaps[opp][RAB]).countBits();
  return anyPiecePoints;
}

//FIXME precompute all the bitmaps that will be used for center rather than calling Bitmap::adj all the time
static int getRabbitBitmapSFP(const Board& b, pla_t pla, Bitmap center)
{
  int points = 0;
  center |= Bitmap::adj(center);
  points += getSFPPoints(b,pla,SFP_STRONGEST_R1,center);
  center |= Bitmap::adj(center);
  points += getSFPPoints(b,pla,SFP_STRONGEST_R2,center);
  points += getAnyPiecePoints(b,pla,center)*3;
  center |= Bitmap::adj(center);
  points += getSFPPoints(b,pla,SFP_STRONGEST_R3,center);
  points += getAnyPiecePoints(b,pla,center)*2;
  center |= Bitmap::adj(center);
  points += getSFPPoints(b,pla,SFP_STRONGEST_R4,center);
  points += getAnyPiecePoints(b,pla,center);
  center |= Bitmap::adj(center);
  points += getSFPPoints(b,pla,SFP_STRONGEST_R5,center);
  return points/2; //FIXME test whether a larger divisor is better, to reduce the swingyness of this factor in rabbit eval
}

//Accounting for the fact that the edges help block rabbits
//Account for piece attackdist when computing blocker?
static const int intrinsicBlockerVal[2][BSIZE] =  {
    {
         0,0,0,0,0,0,0, 0,   0,0,0,0,0,0,0,0,
         8,4,2,0,0,2,4, 8,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
    },
    {
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
        10,5,2,0,0,2,5,10,   0,0,0,0,0,0,0,0,
         8,4,2,0,0,2,4, 8,   0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0, 0,   0,0,0,0,0,0,0,0,
    },
};

//[dy+3][dx+4] where 0,0 is the rabbit location, for gold
static const int blockerContrib[11][9] =
{
    {0,0,0,1,1,1,0,0,0},
    {0,0,1,1,2,1,1,0,0},
    {0,1,1,2,4,2,1,1,0},
    {1,2,3,7,0,7,3,2,1},
    {1,2,4,7,10,7,4,2,1},
    {1,2,3,6,7,6,3,2,1},
    {1,2,3,5,7,5,3,2,1},
    {1,2,3,5,6,5,3,2,1},
    {1,2,3,5,6,5,3,2,1},
    {1,2,3,5,6,5,3,2,1},
    {1,2,3,5,6,5,3,2,1},
};
static const int rabBlockerContrib[11][9] =
{
    {0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0},
    {0,0,1,3,0,3,1,0,0},
    {0,0,1,2,6,2,1,0,0},
    {0,0,1,2,5,2,1,0,0},
    {0,0,1,2,4,2,1,0,0},
    {0,0,1,2,4,2,1,0,0},
    {0,0,1,2,4,2,1,0,0},
    {0,0,1,2,4,2,1,0,0},
    {0,0,1,2,4,2,1,0,0},
};

static int rabBlockerContribPreprocessed[2][DIFFIDX_SIZE];
static int blockerContribPreprocessed[2][DIFFIDX_SIZE];

void RabGoal::init()
{
  loc_t corners[4] = {gLoc(0,0),gLoc(7,0),gLoc(0,7),gLoc(7,7)};
  for(int i = 0; i<4; i++)
  {
    loc_t rabLoc = corners[i];
    for(loc_t otherLoc = 0; otherLoc < BSIZE; otherLoc++)
    {
      if(otherLoc & 0x88)
        continue;
      int dx = gX(otherLoc)-gX(rabLoc);
      int dy = gY(otherLoc)-gY(rabLoc);
      int blockerValueGold = 0;
      int blockerValueSilv = 0;
      int rabBlockerValueGold = 0;
      int rabBlockerValueSilv = 0;
      if(!(dx < -4 || dx > 4 || dy < -3 || dy > 7))
      {
        blockerValueGold = blockerContrib[dy+3][dx+4];
        rabBlockerValueGold = rabBlockerContrib[dy+3][dx+4];
      }
      if(!(dx < -4 || dx > 4 || dy > 3 || dy < -7))
      {
        blockerValueSilv = blockerContrib[-dy+3][dx+4];
        rabBlockerValueSilv = rabBlockerContrib[-dy+3][dx+4];
      }
      blockerContribPreprocessed[GOLD][otherLoc-rabLoc+DIFFIDX_ADD] = blockerValueGold;
      blockerContribPreprocessed[SILV][otherLoc-rabLoc+DIFFIDX_ADD] = blockerValueSilv;
      rabBlockerContribPreprocessed[GOLD][otherLoc-rabLoc+DIFFIDX_ADD] = rabBlockerValueGold;
      rabBlockerContribPreprocessed[SILV][otherLoc-rabLoc+DIFFIDX_ADD] = rabBlockerValueSilv;
    }
  }
}

static const int RABBIT_SCORE_CONVOLUTION_LEN = 7;
static const double RABBIT_SCORE_CONVOLUTION_SQ[RABBIT_SCORE_CONVOLUTION_LEN] =
{0.07*0.07, 0.13*0.13, 0.19*0.19, 0.22*0.22, 0.19*0.19, 0.13*0.13, 0.07*0.07};
//static const double RABBIT_SCORE_CONVOLUTION[RABBIT_SCORE_CONVOLUTION_LEN] =
//{0.07,0.13,0.19,0.22,0.19,0.13,0.07};

static const double RAB_YDIST_TC_VALUES[8] = {2000,790,490,380,270,150,70,0};
static const double RAB_XPOS_TC[4] = {0.97398, 0.985342, 0.887231, 1.02873};
static const double RAB_TC[61] =
{0.005, 0.01, 0.02, 0.03, 0.04, 0.05, 0.07, 0.08, 0.094, 0.112,
 0.131, 0.149, 0.168, 0.186, 0.194, 0.213, 0.231, 0.250, 0.265,
 0.283, 0.301, 0.319, 0.337, 0.355, 0.370, 0.385, 0.398, 0.411,
 0.424, 0.439, 0.451, 0.462, 0.476, 0.490, 0.506, 0.525, 0.545,
 0.568, 0.591, 0.615, 0.639, 0.652, 0.685, 0.708, 0.730, 0.752,
 0.774, 0.795, 0.815, 0.835, 0.853, 0.871, 0.889, 0.904, 0.922,
 0.940, 0.956, 0.974, 0.992, 1.010, 1.030
};
static const double RAB_YDIST_VALUES[8] = {3000,2200,1770,1400,1040,640,270,0};

static const double RAB_XPOS[4] = {1.04329, 0.986563, 0.920916, 1.04774};
static const double RAB_FROZEN_UFDIST[6] = {1.05, 0.935, 0.920, 0.970, 0.970, 0.860};
static const double RAB_BLOCKER[61] = {
    1.440, 1.428, 1.416, 1.404, 1.392, 1.380, 1.368, 1.356, 1.344, 1.332,
    1.320, 1.308, 1.296, 1.284, 1.272, 1.260, 1.248, 1.236, 1.228, 1.217,
    1.185, 1.125, 1.041, 0.981, 0.826, 0.750, 0.673, 0.540, 0.427, 0.301,
    0.225, 0.195, 0.178, 0.167, 0.162, 0.158, 0.154, 0.150, 0.146, 0.143,
    0.139, 0.135, 0.132, 0.128, 0.125, 0.122, 0.120, 0.119, 0.117, 0.116,
    0.115, 0.114, 0.113, 0.112, 0.111, 0.110, 0.110, 0.109, 0.108, 0.107,
    0.107
};

static const double RAB_SFPGOAL[81] = {
    0.214,0.216,0.218,0.221,0.224,0.228,0.232,0.236,0.242,0.248,
    0.256,0.266,0.276,0.286,0.296,0.306,0.316,0.326,0.336,0.346,
    0.356,0.366,0.376,0.386,0.397,0.409,0.421,0.433,0.446,0.460,
    0.472,0.484,0.494,0.504,0.515,0.526,0.538,0.551,0.564,0.579,
    0.595,0.612,0.632,0.655,0.680,0.705,0.733,0.759,0.787,0.815,
    0.842,0.867,0.890,0.910,0.925,0.940,0.955,0.970,0.980,0.990,
    1.000,1.010,1.020,1.030,1.040,1.050,1.060,1.070,1.080,1.090,
    1.100,1.105,1.110,1.115,1.120,1.125,1.130,1.135,1.140,1.145,
    1.150
};

void RabGoal::getScore(const Board& b, const int ufDist[BSIZE], const int tc[2][4],
    ostream* out, eval_t rabThreatScore[2])
{
  (void)out;
  //Debugging output
  //int blockerarr[BSIZE]; for(int i = 0; i<BSIZE; i++) blockerarr[i] = 0;
  //int sfpgoalarr[BSIZE]; for(int i = 0; i<BSIZE; i++) sfpgoalarr[i] = 0;
  //double tscorearr[BSIZE]; for(int i = 0; i<BSIZE; i++) tscorearr[i] = 0;
  //double gscorearr[BSIZE]; for(int i = 0; i<BSIZE; i++) gscorearr[i] = 0;

  //double score = 0;
  int poweredScoresLen = 8+RABBIT_SCORE_CONVOLUTION_LEN-1;
  double poweredScores[2][poweredScoresLen];
  for(int i = 0; i<poweredScoresLen; i++)
    poweredScores[0][i] = 0;
  for(int i = 0; i<poweredScoresLen; i++)
    poweredScores[1][i] = 0;

  int bsfpX[2][8];
  int bsfpLeft = getRabbitBitmapSFP(b,SILV,Bitmap::makeLoc(B2));
  int bsfpCenter = getRabbitBitmapSFP(b,SILV,Bitmap::makeLoc(D2) | Bitmap::makeLoc(E2));
  int bsfpRight = getRabbitBitmapSFP(b,SILV,Bitmap::makeLoc(G2));
  bsfpX[SILV][0] = bsfpLeft;
  bsfpX[SILV][1] = bsfpLeft;
  bsfpX[SILV][2] = (bsfpLeft + bsfpCenter)/2;
  bsfpX[SILV][3] = bsfpCenter;
  bsfpX[SILV][4] = bsfpCenter;
  bsfpX[SILV][5] = (bsfpRight + bsfpCenter)/2;
  bsfpX[SILV][6] = bsfpRight;
  bsfpX[SILV][7] = bsfpRight;

  bsfpLeft = getRabbitBitmapSFP(b,GOLD,Bitmap::makeLoc(B7));
  bsfpCenter = getRabbitBitmapSFP(b,GOLD,Bitmap::makeLoc(D7) | Bitmap::makeLoc(E7));
  bsfpRight = getRabbitBitmapSFP(b,GOLD,Bitmap::makeLoc(G7));

  bsfpX[GOLD][0] = bsfpLeft;
  bsfpX[GOLD][1] = bsfpLeft;
  bsfpX[GOLD][2] = (bsfpLeft + bsfpCenter)/2;
  bsfpX[GOLD][3] = bsfpCenter;
  bsfpX[GOLD][4] = bsfpCenter;
  bsfpX[GOLD][5] = (bsfpRight + bsfpCenter)/2;
  bsfpX[GOLD][6] = bsfpRight;
  bsfpX[GOLD][7] = bsfpRight;


  for(pla_t owner = 0; owner <= 1; owner++)
  {
    Bitmap rabs = b.pieceMaps[owner][RAB] & ~Board::GOALMASKS[owner];
    while(rabs.hasBits())
    {
      loc_t loc = rabs.nextBit();
      int rx = gX(loc);
      int ry = gY(loc);
      int ydist = Board::GOALYDIST[owner][loc];
      if(ydist >= 7)
        continue;

      pla_t opp = gOpp(owner);
      int xpos = rx >= 4 ? 7 - rx : rx;
      int rabtc =
          rx <= 2 ? tc[owner][Board::PLATRAPINDICES[opp][0]] :
          rx >= 5 ? tc[owner][Board::PLATRAPINDICES[opp][1]] :
          rx == 3 ? (tc[owner][Board::PLATRAPINDICES[opp][0]]*2 + tc[owner][Board::PLATRAPINDICES[opp][1]])/3 :
                    (tc[owner][Board::PLATRAPINDICES[opp][1]]*2 + tc[owner][Board::PLATRAPINDICES[opp][0]])/3;

      int tcIdx = (rabtc * 15)/50 + 30;
      if(tcIdx < 0) tcIdx = 0;
      if(tcIdx > 60) tcIdx = 60;

      int isFrozenRab = b.isFrozen(loc);
      int ufDistRab = ufDist[loc] >= 2 ? 2 : ufDist[loc];
      int frozenUFDistIdx = isFrozenRab * 3 + ufDistRab;

      double tcScore =
          RAB_YDIST_TC_VALUES[ydist] *
          RAB_XPOS_TC[xpos] *
          RAB_FROZEN_UFDIST[frozenUFDistIdx] *
          RAB_TC[tcIdx];

      //FIXME and also it doesn't care about who is to move
      //TODO this rabbit eval right now doesn't give a bonus for cutting off defenders from the attack area
      //TODO and also blocker seems not to care enough whether we have a defender on the side or in front of a rabbit.
      //TODO add something for trap squares?
      //TODO decrease blocker a little when an in-front blocking piece is low attackdist?
      int blockedness = 0;
      int startY = (owner == SILV) ? min(ry+3,7) : max(ry-3,0);
      int goalY = (owner == SILV ? 0 : 7);
      int startX = max(rx-4,0);
      int endX = min(rx+4,7);

      Bitmap oppMap = b.pieceMaps[opp][0];
      Bitmap plaMap = b.pieceMaps[owner][0];
      Bitmap empty = ~(plaMap | oppMap);
      Bitmap relevantOppPieces = oppMap & Board::XINTERVAL[startX][endX] & Board::YINTERVAL[startY][goalY];
      Bitmap relevantOppPiecesOnGoal = relevantOppPieces & Board::GOALMASKS[owner];
      relevantOppPieces &= ~relevantOppPiecesOnGoal;

      Bitmap relevantOppRabs = relevantOppPieces & b.pieceMaps[opp][RAB];
      relevantOppPieces &= ~relevantOppRabs;

      const int BACK_ROW_START_X[8] = {0,0,0,1,2,3,4,4};
      const int BACK_ROW_END_X[8]   = {3,3,4,5,6,7,7,7};
      Bitmap thickBackRow =
          Board::XINTERVAL[BACK_ROW_START_X[rx]][BACK_ROW_END_X[rx]] & Board::YINTERVAL[goalY][goalY]
          & ~empty & ~(Bitmap::shiftW(~oppMap)) & ~(Bitmap::shiftE(~oppMap))
          & (oppMap ^ b.dominatedMap); //Either opp and undominated, or pla and dominated

      while(relevantOppPieces.hasBits())
      {
        loc_t oloc = relevantOppPieces.nextBit();
        blockedness += blockerContribPreprocessed[owner][oloc-loc+DIFFIDX_ADD];
      }
      while(relevantOppRabs.hasBits())
      {
        loc_t oloc = relevantOppRabs.nextBit();
        blockedness += rabBlockerContribPreprocessed[owner][oloc-loc+DIFFIDX_ADD];
      }
      while(relevantOppPiecesOnGoal.hasBits())
      {
        loc_t oloc = relevantOppPiecesOnGoal.nextBit();
        blockedness += rabBlockerContribPreprocessed[owner][oloc-loc+DIFFIDX_ADD] + 1;
      }

      //Opp pieces forming thick back row defense
      blockedness += (thickBackRow & oppMap).countBitsIterative();
      thickBackRow &= ~oppMap;
      //Else it's a pla piece, owned by the rabbit owner, check if it's become a brick in the wall.
      while(thickBackRow.hasBits())
      {
        loc_t bloc = thickBackRow.nextBit();
        //Partly the ISOs are here as an out-of-bounds check
        if(!(ISO(bloc W) && GT(bloc,bloc W)) &&
           !(ISO(bloc E) && GT(bloc,bloc E)) &&
           (GT(bloc W,bloc) || GT(bloc E, bloc)))
        {
          if((ISO(bloc W) && b.isDominated(bloc W)) || (ISO(bloc E) && b.isDominated(bloc E)))
            blockedness++;
          else
            blockedness += 1 + rabBlockerContribPreprocessed[owner][bloc-loc+DIFFIDX_ADD]/2;
        }
      }

      blockedness += intrinsicBlockerVal[owner][loc];

      //int sfp = sfpX[owner][rx];
      int sfp = bsfpX[owner][rx];

      double goalScore =
          RAB_YDIST_VALUES[ydist] *
          RAB_XPOS[xpos] *
          RAB_FROZEN_UFDIST[frozenUFDistIdx] *
          RAB_BLOCKER[max(min(blockedness,60),0)] *
          RAB_SFPGOAL[max(min(sfp+40,80),0)];

      double rabScore = tcScore + goalScore;

      //score += owner == pla ? rabScore : -rabScore;
      double rabScoreSq = rabScore * rabScore;
      for(int i = 0; i<RABBIT_SCORE_CONVOLUTION_LEN; i++)
        poweredScores[owner][rx+i] += rabScoreSq * RABBIT_SCORE_CONVOLUTION_SQ[i];

      //Debugging output
      //blockerarr[loc] = blockedness;
      //sfpgoalarr[loc] = sfp;
      //tscorearr[loc] = tcScore;
      //gscorearr[loc] = goalScore;
    }
  }
  //if(out)
  //{
  //  (*out) << "blocker" << endl; (*out) << ArimaaIO::writeBArray(blockerarr,"%4d") << endl;
  //  (*out) << "sfpgoal" << endl; (*out) << ArimaaIO::writeBArray(sfpgoalarr,"%5d") << endl;
  //  (*out) << "tscore" << endl; (*out) << ArimaaIO::writeBArray(tscorearr,"%5.0f") << endl;
  //  (*out) << "gscore" << endl; (*out) << ArimaaIO::writeBArray(gscorearr,"%5.0f") << endl;
  //}

  double silvScore = 0;
  double goldScore = 0;
  for(int i = 0; i<poweredScoresLen; i++)
    silvScore += sqrt(poweredScores[SILV][i]);
  for(int i = 0; i<poweredScoresLen; i++)
    goldScore += sqrt(poweredScores[GOLD][i]);

  rabThreatScore[SILV] = (eval_t)silvScore;
  rabThreatScore[GOLD] = (eval_t)goldScore;
}





