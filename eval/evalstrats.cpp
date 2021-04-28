
/*
 * evalstrats.cpp
 * Author: davidwu
 */

#include <cmath>
#include <cstdio>
#include "../core/global.h"
#include "../board/board.h"
#include "../eval/eval.h"
#include "../eval/threats.h"
#include "../eval/strats.h"
#include "../eval/internal.h"
#include "../search/searchparams.h"

//SHARED------------------------------------------------------------------------------------

/**
 * Get a smooth logistic weighting for scale. As prop increases from zero to infinity, the return value of this function
 * will increase from zero and appoach scale. At prop = total, the return value will be around 70 percentish of scale.
 */
int EvalShared::computeLogistic(double prop, double total, int scale)
{
  return (int)(scale * (-1.0 + 2.0 / (1.0 + exp(-2.0 * prop/total))));
}

//SFP------------------------------------------------------------------------------

//Constants for computing SFP
static const double SFP_TIER_VAL[10] =
{500,140,50,20,10,5,2,1,1,1};

static const double SFP_TIER_PROP[10][10] = {
{  0,-100,-200,-290,-370,-440,-500,-550,-590,-620},
{100,   0, -50,-120,-190,-250,-300,-340,-370,-390},
{200,  50,   0, -20, -70,-120,-160,-200,-230,-220},
{290, 120,  20,   0, -10, -40, -80,-110,-140,-160},
{370, 190,  70,  10,   0,  -5, -25, -60, -90,-120},
{440, 250, 120,  40,   5,   0,  -2, -20, -50, -70},
{500, 300, 160,  80,  25,   2,   0,  -1, -15, -40},
{550, 340, 200, 110,  60,  15,   1,   0,  -1, -10},
{590, 370, 230, 140, 100,  50,  15,   1,   0,   0},
{620, 390, 220, 160, 120,  70,  40,  10,   0,   0},
};

inline static double max(double d0, double d1)
{
  return d0 > d1 ? d0 : d1;
}

static int getSFPTierScore(double tier, double pcount, double ocount)
{
  int t0 = (int)tier;
  int t1 = t0+1;
  double tprop = tier - t0;
  int pc0 = (int)pcount;
  int pc1 = pc0+1;
  double pcprop = pcount - pc0;
  int oc0 = (int)ocount;
  int oc1 = oc0+1;
  double ocprop = ocount - oc0;

  double stprop =
  (SFP_TIER_PROP[pc0][oc0]*(1-ocprop) + SFP_TIER_PROP[pc0][oc1]*(ocprop))*(1-pcprop) +
  (SFP_TIER_PROP[pc1][oc0]*(1-ocprop) + SFP_TIER_PROP[pc1][oc1]*(ocprop))*(pcprop);
  double stval = SFP_TIER_VAL[t0]*(1-tprop) + SFP_TIER_VAL[t1]*(tprop);

  return (int)round(stprop*stval/100.0);
}

//Factor to adjust by depending on relation to penalty piece (myPiece-penaltyPiece+6)
static const double ADVGOODDIFFTABLE[13] = {3.0,3.0,3.0,2.9,2.8,2.5,1.0,0.5,0.35,0.18,0.10,0.10,0.10};

/**
 * Get the material according to SFP.
 * @param b - the board, for debuggingpurposes
 * @param pieceCountsPla - the counts (possibly fractional) of pieces pla has
 * @param pieceCountsOpp - the counts (possibly fractional) of pieces opp has
 * @return the material score
 */
int EvalShared::computeSFPScore(const double* pieceCountsPla, const double* pieceCountsOpp,
    double* advancementGood, piece_t breakPenaltyPiece)
{
  double plaNumStronger[NUMTYPES];
  double oppNumStronger[NUMTYPES];
  plaNumStronger[ELE] = 0;
  oppNumStronger[ELE] = 0;
  if(advancementGood != NULL)
    advancementGood[ELE] = 0;
  for(int piece = CAM; piece >= RAB; piece--)
  {
    plaNumStronger[piece] = plaNumStronger[piece+1] + pieceCountsOpp[piece+1];
    oppNumStronger[piece] = oppNumStronger[piece+1] + pieceCountsPla[piece+1];

    DEBUGASSERT(!(pieceCountsPla[piece+1] < 0 || pieceCountsOpp[piece+1] < 0 || pieceCountsPla[piece+1] > 2.001 || pieceCountsOpp[piece+1] > 2.001))

    if(advancementGood != NULL)
    {
      advancementGood[piece] = 3.0/(oppNumStronger[piece]+1.5)/(oppNumStronger[piece]+1.2);
      advancementGood[piece] *= ADVGOODDIFFTABLE[piece-breakPenaltyPiece+6];
      advancementGood[piece] -= 0.1;
      if(advancementGood[piece] < 0) advancementGood[piece] = 0;
      //cout << "advancementGood " << Board::PIECECHARS[GOLD][piece] << " " << advancementGood[piece] << endl;
    }
  }

  int score = 0;
  double tier = 0.0;
  int piece = ELE;
  while(piece >= CAT)
  {
    double plac = 0.0;
    double oppc = 0.0;
    double tierinc = 0.0;
    while(true)
    {
      plac += pieceCountsPla[piece];
      oppc += pieceCountsOpp[piece];
      tierinc += max(pieceCountsPla[piece],pieceCountsOpp[piece]);
      piece--;
      if(piece <= RAB)
        break;
      if(plac == 0.0 && oppc == 0.0)
        continue;
      if(plac == 0.0 && pieceCountsPla[piece] == 0.0)
        continue;
      if(oppc == 0.0 && pieceCountsOpp[piece] == 0.0)
        continue;
      break;
    }
    int s = getSFPTierScore(tier,plac,oppc);
    score += s;
    //printf("tier %1.2f pla %1.2f opp %1.2f s %d\n", tier, plac, oppc, s);
    tier += tierinc;
  }

  return score;
}

//Percent bonus for staying away from the sfp situation
static const int SFPAdvGood_Percent_Rad[16] =
{30,40,58,76,84,91,96,100,100,100,100,100,100,100,100,100};
static const int SFPAdvGood_Percent_GYDist[8] =
{80,100,100,95,90,85,70,50};
static const int SFPAdvGood_Percent_Centrality[7] =
{100,100,100,86,70,50,30};

static const double SFPAdvMinFactor = 0.70;
static const double SFPAdvMaxFactor = 1.20;

double EvalShared::computeSFPAdvGoodFactor(const Board& b, pla_t pla, loc_t sfpCenter, const double* advancementGood)
{
  double totalAdvancementGood = 0.0;
  double totalPercent = 0.0;
  for(piece_t piece = CAT; piece <= ELE; piece++)
  {
    if(advancementGood[piece] < 0.001)
      continue;
    int count = b.pieceCounts[pla][piece];
    if(count <= 0)
      continue;
    totalAdvancementGood += count*advancementGood[piece];

    Bitmap map = b.pieceMaps[pla][piece];
    while(map.hasBits())
    {
      loc_t ploc = map.nextBit();
      double percent =
          SFPAdvGood_Percent_Rad[Board::manhattanDist(sfpCenter,ploc)] *
          SFPAdvGood_Percent_GYDist[Board::GOALYDIST[pla][ploc]] *
          SFPAdvGood_Percent_Centrality[Board::CENTERDIST[ploc]];
      totalPercent += percent/1000000.0 * advancementGood[piece];
    }
  }
  if(totalAdvancementGood < 0.001)
    return 1.0;

  totalPercent /= totalAdvancementGood;
  if(totalAdvancementGood > 3)
    totalAdvancementGood = 3;

  double factor = (totalPercent-0.5)*totalAdvancementGood/2.7 + 1.0;
  if(factor < SFPAdvMinFactor)
    factor = SFPAdvMinFactor;
  else if(factor > SFPAdvMaxFactor)
    factor = SFPAdvMaxFactor;
  return factor;
}



//FINAL-----------------------------------------------------------------------

void Eval::getStrats(Board& b, const eval_t pValues[2][NUMTYPES], const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], int ufDist[BSIZE], const int tc[2][4],
    eval_t pieceThreats[BSIZE], int numPStrats[2], Strat pStrats[2][numStratsMax], eval_t stratScore[2],
    pla_t mainPla, eval_t earlyBlockadePenalty, eval_t mobilityScorePla, eval_t mobilityScoreOpp, ostream* out)
{
  numPStrats[SILV] = 0;
  numPStrats[GOLD] = 0;

  numPStrats[SILV] += Frames::getStrats(b,SILV,pValues,pStrongerMaps,ufDist,pStrats[SILV]+numPStrats[SILV],out);
  numPStrats[GOLD] += Frames::getStrats(b,GOLD,pValues,pStrongerMaps,ufDist,pStrats[GOLD]+numPStrats[GOLD],out);
  numPStrats[SILV] += Hostages::getStrats(b,SILV,pValues,pStronger,pStrongerMaps,ufDist,tc,pStrats[SILV]+numPStrats[SILV],out);
  numPStrats[GOLD] += Hostages::getStrats(b,GOLD,pValues,pStronger,pStrongerMaps,ufDist,tc,pStrats[GOLD]+numPStrats[GOLD],out);

  //Compute value if all strats are used
  int eleBlockadeEval[2];
  eleBlockadeEval[SILV] = Blockades::evalEleBlockade(b, SILV, pStronger, pStrongerMaps, tc, ufDist, mainPla, earlyBlockadePenalty, out);
  eleBlockadeEval[GOLD] = Blockades::evalEleBlockade(b, GOLD, pStronger, pStrongerMaps, tc, ufDist, mainPla, earlyBlockadePenalty, out);
  int eleBlockOverlap[2] = {0,0};
  eleBlockOverlap[b.player] = -mobilityScoreOpp;
  eleBlockOverlap[gOpp(b.player)] = -mobilityScorePla;

  stratScore[0] = 0;
  stratScore[1] = 0;
  for(pla_t p = 0; p<2; p++)
  {
    for(int i = 0; i<numPStrats[p]; i++)
    {
      Strat& strat = pStrats[p][i];
      int valueDiff = strat.value - pieceThreats[strat.pieceLoc];
      if(valueDiff <= 0)
        valueDiff = 0;
      strat.value = valueDiff;
      pieceThreats[strat.pieceLoc] += valueDiff;
      stratScore[p] += valueDiff;

      //Reduce ele blockade eval in the case that other strats overlap and also pin the ele
      if(strat.wasHostage) eleBlockOverlap[gOpp(p)] += pValues[gOpp(p)][b.pieces[strat.pieceLoc]] / 6;
      else eleBlockOverlap[p] += b.pieces[strat.pieceLoc] == RAB ? 0 : pieceThreats[strat.pieceLoc] * 3/4;
    }
  }

  stratScore[0] += eleBlockadeEval[0]/6 + max(0,eleBlockadeEval[0]*5/6 - eleBlockOverlap[0]);
  stratScore[1] += eleBlockadeEval[1]/6 + max(0,eleBlockadeEval[1]*5/6 - eleBlockOverlap[1]);
}



