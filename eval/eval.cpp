
/*
 * eval.cpp
 * Author: davidwu
 */

#include <cstdio>
#include <cmath>
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../eval/internal.h"
#include "../eval/threats.h"
#include "../eval/strats.h"
#include "../eval/evalparams.h"
#include "../eval/eval.h"
#include "../search/searchparams.h"
#include "../program/arimaaio.h"

using namespace std;

eval_t Eval::evaluate(Board& b, pla_t mainPla, eval_t earlyBlockadePenalty, ostream* outstream)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  eval_t totalScore = 0;
  eval_t plaVarPoints = 0;
  eval_t oppVarPoints = 0;

  //Material---------------------------------------------------------------------------
  eval_t materialScore = getMaterialScore(b, pla);
  totalScore += materialScore;

  //Initialize a bunch of data---------------------------------------------------------
  int pStronger[2][NUMTYPES];
  Bitmap pStrongerMaps[2][NUMTYPES];
  eval_t pValues[2][NUMTYPES];
  int ufDist[BSIZE];
  b.initializeStronger(pStronger);
  b.initializeStrongerMaps(pStrongerMaps);
  initializeValues(b,pValues);
  UFDist::get(b,ufDist,pStrongerMaps);

  //Piece-square tables----------------------------------------------------------------
  eval_t psScore = PieceSquare::get(b,pStronger);
  totalScore += psScore;

  //Trap control-----------------------------------------------------------------------
  int tc[2][4];
  int tcEle[2][4];
  TrapControl::get(b,pStronger,pStrongerMaps,ufDist,tc,tcEle);
  eval_t tDefScore = TrapControl::getSimple(b);
  eval_t tcScore = TrapControl::getEval(pla,tc);
  totalScore += tDefScore;
  totalScore += tcScore;

  //Elephant Mobility------------------------------------------------------------------
  const int eleCanReachMax = EMOB_MAX;
  Bitmap eleCanReach[2][EMOB_ARR_LEN];
  Mobility::getEle(b,SILV,eleCanReach[SILV],eleCanReachMax,EMOB_ARR_LEN);
  Mobility::getEle(b,GOLD,eleCanReach[GOLD],eleCanReachMax,EMOB_ARR_LEN);

  eval_t mobilityScorePla = Mobility::getEval(b,eleCanReach[pla],pla);
  eval_t mobilityScoreOpp = Mobility::getEval(b,eleCanReach[opp],opp);
  if(mainPla != pla) mobilityScorePla = mobilityScorePla * 3/8;
  if(mainPla != opp) mobilityScoreOpp = mobilityScoreOpp * 3/8;
  totalScore += mobilityScorePla;
  totalScore -= mobilityScoreOpp;

  //Threats----------------------------------------------------------------------------
  eval_t trapThreats[2][4];
  eval_t reducibleTrapThreats[2][4];
  eval_t pieceThreats[BSIZE];
  for(int i = 0; i<4; i++)
  {trapThreats[SILV][i] = 0; trapThreats[GOLD][i] = 0;}
  for(int i = 0; i<4; i++)
  {reducibleTrapThreats[SILV][i] = 0; reducibleTrapThreats[GOLD][i] = 0;}
  for(int i = 0; i<BSIZE; i++)
  {pieceThreats[i] = 0;}

  PThreats::accumOppAttack(b,pStrongerMaps,pValues,ufDist,tc,eleCanReach,trapThreats,pieceThreats);
  PThreats::accumBasicThreats(b,pStronger,pStrongerMaps,pValues,ufDist,tc,eleCanReach,trapThreats,reducibleTrapThreats,pieceThreats);
  PThreats::accumHeavyThreats(b,pStronger,pStrongerMaps,pValues,ufDist,tc,trapThreats,pieceThreats);

  eval_t threatScore = 0;
  for(int i = 0; i<4; i++)
  {
    //TODO I think it would be good to modify this. Decrease threats based on elephant proximity to the trap
    //and increase the final score based on whether you have threats in multiple traps
    threatScore -= trapThreats[pla][i];
    threatScore += trapThreats[opp][i];
  }
  totalScore += threatScore;

  //Frames, Hostages, Blockades-------------------------------------------------------------
  int numPStrats[2];
  Strat pStrats[2][numStratsMax];
  eval_t stratScore[2];
  getStrats(b,pValues,pStronger,pStrongerMaps,ufDist,tc,
      pieceThreats,numPStrats,pStrats,stratScore,mainPla,earlyBlockadePenalty,mobilityScorePla,mobilityScoreOpp,NULL);
  eval_t stratScorePla = stratScore[pla];
  eval_t stratScoreOpp = stratScore[opp];
  totalScore += stratScorePla;
  totalScore -= stratScoreOpp;

  //Threat reduction-------------------------------------------------------------------
  loc_t plaBestReductionAt;
  loc_t oppBestReductionAt;
  eval_t threatReductionPla = PThreats::adjustForReducibleThreats(b,pla,trapThreats,reducibleTrapThreats,numPStrats,pStrats,plaBestReductionAt);
  eval_t threatReductionOpp = PThreats::adjustForReducibleThreats(b,opp,trapThreats,reducibleTrapThreats,numPStrats,pStrats,oppBestReductionAt);
  totalScore += threatReductionPla;
  totalScore -= threatReductionOpp;

  //Camel Advancement------------------------------------------------------------------
  eval_t sheriffScorePla = Placement::getSheriffAdvancementThreat(b,pla,tc,tcEle,NULL);
  eval_t sheriffScoreOpp = Placement::getSheriffAdvancementThreat(b,opp,tc,tcEle,NULL);
  totalScore += sheriffScorePla;
  totalScore -= sheriffScoreOpp;

  //Piece Alignment--------------------------------------------------------------------
  eval_t alignmentScore[2];
  Placement::getAlignmentScore(b,ufDist,alignmentScore,NULL);
  totalScore += alignmentScore[pla] - alignmentScore[opp];

  eval_t imbalanceScore[2];
  Placement::getImbalanceScore(b,ufDist,imbalanceScore,NULL);
  totalScore += imbalanceScore[pla] - imbalanceScore[opp];

  //Goal threatening--------------------------------------------------------------------------
  eval_t rabbitThreatScore[2];
  RabGoal::getScore(b,ufDist,tc,outstream,rabbitThreatScore);

  //The proportion of rabbit threat score used as variance points
  static const int RAB_VAR_CONTRIB = 40; //out of 128
  plaVarPoints += rabbitThreatScore[pla] * RAB_VAR_CONTRIB/128;
  oppVarPoints += rabbitThreatScore[opp] * RAB_VAR_CONTRIB/128;
  totalScore += rabbitThreatScore[pla];
  totalScore -= rabbitThreatScore[opp];

  //Captures------------------------------------------------------------------------------
  int numSteps = 4-b.step;
  eval_t bestCapRaw;
  loc_t bestCapLoc;
  eval_t capScore = Captures::getEval(b,pla,numSteps,pValues,pStrongerMaps,ufDist,
      pieceThreats,numPStrats,pStrats,bestCapRaw,bestCapLoc);
  totalScore += capScore;

  //Variance adjustment--------------------------------------------------------------------
  eval_t varianceAdj = Variance::getAdjustment(totalScore,plaVarPoints,oppVarPoints);

  //Numsteps bonus---------------------------------------------------------------------
  eval_t nsScore = SearchParams::STEPS_LEFT_BONUS[numSteps];

  //Final---------------------------------------------------------------------------------
  eval_t finalScore = totalScore + varianceAdj + nsScore;

  ONLYINDEBUG(if(outstream)
  {
    ostream& out = *outstream;
    out << "---Eval (mainpla = " << Board::writePla(mainPla) << " )-----------------" << endl;
    out << b;
    out << "Material: " << materialScore << " " << Board::writeMaterial(b.pieceCounts) << endl;
    out << "PieceSquare: " << psScore << endl;
    //PieceSquare::print(b,pStronger,out);
    out << "TDefScore: " << tDefScore << endl;
    out << "TCScore: " << tcScore << endl;
    TrapControl::printEval(pla,tc,out);
    out << "PEleMobility: " << mobilityScorePla << endl;
    out << "OEleMobility: " << mobilityScoreOpp << endl;
    //Mobility::print(b,eleCanReach[GOLD],GOLD,eleCanReachMax,out);
    //Mobility::print(b,eleCanReach[SILV],SILV,eleCanReachMax,out);
    out << "ThreatScore: " << threatScore << endl;
    PThreats::printEval(pla,trapThreats,out);
    out << ArimaaIO::writeBArray(pieceThreats,"%+5d ") << endl;
    out << "StratScorePla: " << stratScore[pla] << endl;
    out << "StratScoreOpp: " << stratScore[opp] << endl;
    //Strat buffer[Strats::numStratsMax];
    //Hostages::getStrats(b,pla,pValues,pStronger,pStrongerMaps,ufDist,tc,buffer,outstream);
    //Hostages::getStrats(b,opp,pValues,pStronger,pStrongerMaps,ufDist,tc,buffer,outstream);
    out << "ThreatReductionPla: " << threatReductionPla << " at " << Board::writeLoc(plaBestReductionAt) << endl;
    out << "ThreatReductionOpp: " << threatReductionOpp << " at " << Board::writeLoc(oppBestReductionAt) << endl;
    out << "SheriffScorePla: " << sheriffScorePla << endl;
    out << "SheriffScoreOpp: " << sheriffScoreOpp << endl;
    //out << "AlignmentOld: " << alignmentScoreOld << endl;
    out << "AlignPla: " << alignmentScore[pla] << endl;
    out << "AlignOpp: " << alignmentScore[opp] << endl;
    //Placement::getAlignmentScore(b,ufDist,alignmentScore,outstream);
    out << "ImbalancePla: " << imbalanceScore[pla] << endl;
    out << "ImbalanceOpp: " << imbalanceScore[opp] << endl;
    //Placement::getImbalanceScore(b,ufDist,imbalanceScore,outstream);
    out << "RabScorePla: " << rabbitThreatScore[pla] << endl;
    out << "RabScoreOpp: " << rabbitThreatScore[opp] << endl;
    if(bestCapLoc != ERRLOC) out << "Best cap " << Board::writeLoc(bestCapLoc) << " Raw " << bestCapRaw << " Net " << capScore << endl;
    else                     out << "No cap" << endl;
    out << "Total Score: " << totalScore << endl;
    out << "PlaVarPoints: " << plaVarPoints << endl;
    out << "OppVarPoints: " << oppVarPoints << endl;
    out << "VarAdjustment: " << varianceAdj << endl;
    out << "NumStepsScore: " << nsScore << endl;
    out << "Final Score: " << finalScore << endl;
    out << "---------------------------------" << endl;
  })

  return finalScore;
}

//We model our winning probability as a logistic 1/(1+exp(-X/K)) where X is the eval score and where this value is K.
//Some points that we would normally score we count as "variance points", and omit from the eval score until the end.
//At the end, we compute our winning probability P given our score so far. We assume that due to local variance we
//win right away with odds W and lose with odds L, and with odds 1, we win with probability P,
//so that our total probability P' = W + P(1-W-L)
//And then we reverse it via X' = -log(1/P' - 1)*K, and the difference X'-X is the adjustment.
//We choose W so that if X so far is zero, then X' is exactly equal to X+(desired variance points), and similarly L.

//This has the property that dX'/d(variance points) = 1/(2P) when variance points = 0.
//So for example, if
//X = -2VARIANCE_WINPROB_OF_SCORE, then variance points are worth 0.5(1+e^2)   ~= 4.19
//X = - VARIANCE_WINPROB_OF_SCORE, then variance points are worth 0.5(1+e)     ~= 1.86
//X = 0                          , then variance points are worth 1
//X =   VARIANCE_WINPROB_OF_SCORE, then variance points are worth 0.5(1+1/e)   ~= 0.68
//X = 2 VARIANCE_WINPROB_OF_SCORE, then variance points are worth 0.5(1+1/e^2) ~= 0.567
static const double VARIANCE_WINPROB_OF_SCORE = 4500;
eval_t Variance::getAdjustment(eval_t eval, eval_t plaVarPoints, eval_t oppVarPoints)
{
  //Assumes eval has all the var points added in already - subtract them out
  eval_t subtractedEval = eval - plaVarPoints + oppVarPoints;

  double longTermWinProb = 1.0/(1.0 + exp(-subtractedEval/VARIANCE_WINPROB_OF_SCORE));
  //1/(2*VARIANCE_WINPROB_OF_SCORE) is the necessary scaling to make it so that at score 0, one plaVarPoint is one point.
  double immediateWinOdds = plaVarPoints / (2.0*VARIANCE_WINPROB_OF_SCORE);
  double immediateLoseOdds = oppVarPoints / (2.0*VARIANCE_WINPROB_OF_SCORE);
  double totalOdds = immediateWinOdds + immediateLoseOdds + 1.0;
  double totalWinProb = (immediateWinOdds + longTermWinProb)/totalOdds;
  double finalEval = -log(1.0/totalWinProb - 1.0) * VARIANCE_WINPROB_OF_SCORE;

  //Just to get symmetric rounding behaviour
  if(finalEval >= eval) return (eval_t)(finalEval - eval);
  else                  return -(eval_t)(eval-finalEval);
}


static const double INFLUENCE_SPREAD = 0.16;
static const double INFLUENCE_STAY = 0.36;
static const double INFLUENCE_STAY_OVER_SPREAD = INFLUENCE_STAY / INFLUENCE_SPREAD;
static const int INFLUENCE_REPS = 4;
static const double INFLUENCE_NORM = 100.0/0.12/80.0*INFLUENCE_SPREAD*INFLUENCE_SPREAD*INFLUENCE_SPREAD*INFLUENCE_SPREAD;
static const double INFLUENCE_NUMSTRONGER[9] =
{75*INFLUENCE_NORM,
 55*INFLUENCE_NORM,
 50*INFLUENCE_NORM,
 45*INFLUENCE_NORM,
 40*INFLUENCE_NORM,
 35*INFLUENCE_NORM,
 30*INFLUENCE_NORM,
 25*INFLUENCE_NORM,
 15*INFLUENCE_NORM};


//100 units of influence is an elephant there
void Eval::getInfluence(const Board& b, const int pStronger[2][NUMTYPES], int influence[2][BSIZE])
{
  double infb1[64];
  double infb2[64];
  double* inf = infb1;
  double* other = infb2;
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      int i = y*8+x;
      loc_t loc = gLoc(x,y);
      if(b.owners[loc] == GOLD)
        inf[i] = INFLUENCE_NUMSTRONGER[pStronger[GOLD][b.pieces[loc]]];
      else if(b.owners[loc] == SILV)
        inf[i] = -INFLUENCE_NUMSTRONGER[pStronger[SILV][b.pieces[loc]]];
      else
        inf[i] = 0;
    }
  }

  for(int reps = 0; reps < INFLUENCE_REPS; reps++)
  {
    {
      other[0] = inf[0] * INFLUENCE_STAY_OVER_SPREAD + (inf[8]+inf[1]);
      for(int x = 1; x < 7; x++)
      {
        other[x] = inf[x] * INFLUENCE_STAY_OVER_SPREAD + (inf[x+8]+(inf[x-1]+inf[x+1]));
      }
      other[7] = inf[7] * INFLUENCE_STAY_OVER_SPREAD + (inf[7+8]+inf[7-1]);
    }
    for(int y = 1; y < 7; y++)
    {
      int i0 = y*8;
      other[i0] = inf[i0] * INFLUENCE_STAY_OVER_SPREAD + ((inf[i0-8]+inf[i0+8])+inf[i0+1]);
      for(int x = 1; x < 7; x++)
      {
        int i = y*8+x;
        other[i] = inf[i] * INFLUENCE_STAY_OVER_SPREAD + ((inf[i-8]+inf[i+8])+(inf[i-1]+inf[i+1]));
      }
      int i7 = y*8+7;
      other[i7] = inf[i7] * INFLUENCE_STAY_OVER_SPREAD + ((inf[i7-8]+inf[i7+8])+inf[i7-1]);
    }
    {
      other[56] = inf[56] * INFLUENCE_STAY_OVER_SPREAD + (inf[56-8]+inf[56+1]);
      for(int x = 1; x < 7; x++)
      {
        int i = 56+x;
        other[i] = inf[i] * INFLUENCE_STAY_OVER_SPREAD + (inf[i-8]+(inf[i-1]+inf[i+1]));
      }
      other[63] = inf[63] * INFLUENCE_STAY_OVER_SPREAD + (inf[63-8]+inf[63-1]);
    }
    double* temp = other;
    other = inf;
    inf = temp;
  }
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      int infl = (int)(inf[y*8+x]);
      influence[GOLD][loc] = infl;
      influence[SILV][loc] = -infl;
    }
  }

}
