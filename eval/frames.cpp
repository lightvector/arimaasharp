/*
 * frames.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtreeconst.h"
#include "../eval/eval.h"
#include "../eval/threats.h"
#include "../eval/internal.h"

static const int FRAMEBREAKSTEPSDIV = 100;
static const int FRAMEBREAKSTEPSFACTORLEN = 12;
static const int FRAMEBREAKSTEPSFACTOR[FRAMEBREAKSTEPSFACTORLEN] =
{9,21,34,45,55,67,75,83,90,95,98,100};

static const int FRAME_SFP_LOGISTIC_DIV = 240;
static const double FRAMEPINMOBLOSS = 70;
static const double FRAMEMOBLOSS[2][BSIZE] = {
{
 70, 70, 70, 69, 69, 70, 70, 70,  0,0,0,0,0,0,0,0,
 69, 68, 66, 64, 64, 66, 68, 69,  0,0,0,0,0,0,0,0,
 66, 65, 60, 58, 58, 60, 65, 66,  0,0,0,0,0,0,0,0,
 64, 55, 54, 49, 49, 54, 55, 64,  0,0,0,0,0,0,0,0,
 64, 56, 55, 46, 46, 55, 56, 64,  0,0,0,0,0,0,0,0,
 66, 63, 58, 49, 49, 58, 63, 66,  0,0,0,0,0,0,0,0,
 68, 67, 67, 58, 58, 67, 67, 68,  0,0,0,0,0,0,0,0,
 70, 69, 68, 67, 67, 68, 69, 70,  0,0,0,0,0,0,0,0,
},
{
 65, 65, 65, 64, 64, 65, 65, 65,  0,0,0,0,0,0,0,0,
 64, 63, 61, 59, 59, 61, 63, 64,  0,0,0,0,0,0,0,0,
 60, 59, 54, 52, 52, 54, 59, 60,  0,0,0,0,0,0,0,0,
 58, 49, 48, 42, 42, 48, 49, 58,  0,0,0,0,0,0,0,0,
 57, 49, 52, 39, 39, 52, 49, 57,  0,0,0,0,0,0,0,0,
 68, 85, 42, 44, 44, 42, 85, 68,  0,0,0,0,0,0,0,0,
 61, 70, 70, 51, 51, 70, 70, 61,  0,0,0,0,0,0,0,0,
 68, 68, 66, 61, 61, 66, 68, 68,  0,0,0,0,0,0,0,0,
}};

static const int EFRAMEHOLDPENALTY[BSIZE] =
{
1400,1400,1400,1400,1400,1400,1400,1400,  0,0,0,0,0,0,0,0,
1400,1400,1000, 800, 800,1000,1400,1400,  0,0,0,0,0,0,0,0,
1000, 900, 200, 150, 150, 200, 900,1000,  0,0,0,0,0,0,0,0,
 700, 300, 150, 100, 100, 150, 300, 700,  0,0,0,0,0,0,0,0,
 700, 400, 100, 100, 100, 100, 400, 700,  0,0,0,0,0,0,0,0,
1100,1100, 200, 200, 200, 200,1100,1100,  0,0,0,0,0,0,0,0,
1600,1600,1200,1000,1000,1200,1600,1600,  0,0,0,0,0,0,0,0,
1600,1600,1600,1600,1600,1600,1600,1600,  0,0,0,0,0,0,0,0,
};

static const double ROTDIV = 100;
static const int rotationFactorLen = 18;
static const int rotationFactor[rotationFactorLen] =
{0,7,10,16,20,33,36,40,43,50,54,60,67,74,82,90,100,100};
static const int ROTATION_EFRAMEHOLDPENALTY_MOD[rotationFactorLen] =
{15, 19,24,30,36, 43,52,62,72, 80,87,92,96, 98,99,99,100, 100};

static double getFrameMobLoss(pla_t pla, loc_t loc, int mobilityLevel)
{
  if(pla == 1)
    return FRAMEMOBLOSS[mobilityLevel][loc]/100.0;
  else
    return FRAMEMOBLOSS[mobilityLevel][gRevLoc(loc)]/100.0;
}

static int getFrameBestSFP(const Board& b, pla_t pla, FrameThreat threat,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], double* advancementGood,
    int& eFrameHolderPenalty, ostream* out)
{
  pla_t opp = gOpp(pla);
  loc_t kt = threat.kt;
  loc_t pinnedLoc = threat.pinnedLoc;

  //Copy over counts
  double plaPieceCounts[NUMTYPES];
  double oppPieceCounts[NUMTYPES];
  for(int piece = ELE; piece > EMP; piece--)
  {
    plaPieceCounts[piece] = b.pieceCounts[pla][piece];
    oppPieceCounts[piece] = b.pieceCounts[opp][piece];
  }

  //Opp - Framed piece
  {
    double prop = FRAMEPINMOBLOSS/100.0;
    oppPieceCounts[b.pieces[kt]] -= prop;
    oppPieceCounts[0] -= prop;
    //Opp - Pinned piece
    prop = getFrameMobLoss(opp,pinnedLoc,0);
    oppPieceCounts[b.pieces[pinnedLoc]] -= prop;
    oppPieceCounts[0] -= prop;
  }

  //Pla - Holders
  Bitmap holderMap = threat.holderMap;
  eFrameHolderPenalty = 0;
  while(holderMap.hasBits())
  {
    loc_t loc = holderMap.nextBit();
    if(b.owners[loc] != pla)
      continue;
    double prop = getFrameMobLoss(pla,loc,1);
    plaPieceCounts[b.pieces[loc]] -= prop;
    plaPieceCounts[0] -= prop;

    if(b.pieces[loc] == ELE)
      eFrameHolderPenalty = EFRAMEHOLDPENALTY[pla == 1 ? loc : gRevLoc(loc)];
  }

  //Compute rotations
  Bitmap rotatableMap = threat.holderMap & ~b.pieceMaps[opp][0] & ~b.pieceMaps[pla][RAB] & ~b.pieceMaps[pla][CAT];
  int numRotatableMax = rotatableMap.countBits()*2;
  loc_t holderRotLocs[numRotatableMax];
  int holderRotDists[numRotatableMax];
  loc_t holderRotBlockers[numRotatableMax];
  bool holderIsSwarm[numRotatableMax];
  int minStrNeededArr[numRotatableMax];
  int numRotatable = Blockades::fillBlockadeRotationArrays(b,pla,holderRotLocs,holderRotDists,holderRotBlockers,holderIsSwarm,minStrNeededArr,
      Bitmap::makeLoc(kt),threat.holderMap & b.pieceMaps[pla][0],rotatableMap,pStrongerMaps,ufDist);

  //Try rotations - find the best SFP score for holding the frame
  int bestSfpVal = EvalShared::computeSFPScore(plaPieceCounts,oppPieceCounts,advancementGood,b.pieces[threat.kt]);
  for(int i = 0; i<numRotatable; i++)
  {
    loc_t loc = holderRotLocs[i];
    int rotDist = holderRotDists[i];
    loc_t blockerLoc = holderRotBlockers[i];
    if(rotDist >= rotationFactorLen)
      continue;
    ONLYINDEBUG(if(out) (*out) << "Rotate " << (int)loc << " in " << rotDist  << " with " << (int)blockerLoc << endl;)

    double mobilityPropCost = getFrameMobLoss(pla,loc,1);
    double rotationProp = rotationFactor[rotDist]/ROTDIV;
    plaPieceCounts[b.pieces[loc]] += mobilityPropCost;
    plaPieceCounts[b.pieces[loc]] -= mobilityPropCost * rotationProp;
    if(blockerLoc != ERRLOC)
      plaPieceCounts[b.pieces[blockerLoc]] -= mobilityPropCost * (1.0 - rotationProp);
    int sfpVal = EvalShared::computeSFPScore(plaPieceCounts,oppPieceCounts);
    if(blockerLoc != ERRLOC)
      plaPieceCounts[b.pieces[blockerLoc]] += mobilityPropCost * (1.0 - rotationProp);
    plaPieceCounts[b.pieces[loc]] += mobilityPropCost * rotationProp;
    plaPieceCounts[b.pieces[loc]] -= mobilityPropCost;

    if(b.pieces[loc] == ELE)
      eFrameHolderPenalty = eFrameHolderPenalty * ROTATION_EFRAMEHOLDPENALTY_MOD[rotDist]/100;

    if(sfpVal > bestSfpVal)
      bestSfpVal = sfpVal;
  }
  return bestSfpVal;
}

static int getFrameBreakDistance(const Board& b, pla_t pla, loc_t bloc, Bitmap defenseMap,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  pla_t opp = gOpp(pla);

  //For each square, compute ability to break, and take the min
  int minBreakDist = 16;
  while(defenseMap.hasBits())
  {
    loc_t loc = defenseMap.nextBit();
    int bs = 16;
    if(b.owners[loc] == opp)
    {
      if(b.isOpenToStep(loc)) bs = ufDist[loc]+3;
      else if(b.isOpenToMove(loc)) bs = ufDist[loc]+4;
    }
    else if(b.owners[loc] == pla)
    {
      bs = Threats::attackDist(b,loc,min(5,minBreakDist-3),pStrongerMaps,ufDist)+3;
      if(Board::isAdjacent(bloc,loc) && !b.isRabOkay(opp,bloc,loc))
        bs++;
    }
    else
    {
      bs = Threats::occupyDist(b,opp,loc,min(5,minBreakDist-2),pStrongerMaps,ufDist)+2;
    }

    if(bs < minBreakDist)
      minBreakDist = bs;
  }

  if(minBreakDist < 0)
    minBreakDist = 0;

  return minBreakDist;
}


//ufDist is not const because it is temporarily modified for the computation
static Strat evalSingle(const Board& b, pla_t pla, FrameThreat threat, const eval_t pValues[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], int ufDist[BSIZE], ostream* out)
{
  pla_t opp = gOpp(pla);
  loc_t kt = threat.kt;
  loc_t pinnedLoc = threat.pinnedLoc;
  int framedValue = Eval::evalCapOccur(b,pValues[opp],kt,0);

  double advancementGood[NUMTYPES];
  int eFrameHolderPenalty = 0;
  int bestSfpVal = getFrameBestSFP(b,pla,threat,pStrongerMaps,ufDist,advancementGood,eFrameHolderPenalty,out);

  //Evaluate cost
  int frameVal = EvalShared::computeLogistic(bestSfpVal,FRAME_SFP_LOGISTIC_DIV,framedValue);

  double sfpAdvGoodFactor = EvalShared::computeSFPAdvGoodFactor(b,pla,threat.kt,advancementGood);
  frameVal = (int)(frameVal * sfpAdvGoodFactor) - eFrameHolderPenalty;

  //Simulate freezing
  ufDist[kt] += 6; ufDist[pinnedLoc] += 6;
  int breakSteps = getFrameBreakDistance(b, pla, kt, threat.holderMap, pStrongerMaps, ufDist);
  ufDist[kt] -= 6; ufDist[pinnedLoc] -= 6;

  if(breakSteps < FRAMEBREAKSTEPSFACTORLEN)
    frameVal = frameVal * FRAMEBREAKSTEPSFACTOR[breakSteps] / FRAMEBREAKSTEPSDIV;

  //Adjust for partial frames
  if(threat.isPartial)
    frameVal /= 2;

  if(frameVal < 0)
    frameVal = 0;

  Strat strat(frameVal,kt,false,threat.holderMap & b.pieceMaps[pla][0]);

  ONLYINDEBUG(if(out)
  {
    (*out) << "Frame " << strat << endl;
    (*out) << "FramebestSFP " << bestSfpVal << " breaksteps " << breakSteps << endl;
    (*out) << "sfpAdvGoodFactor" << sfpAdvGoodFactor << endl;
  })

  return strat;
}

int Frames::getStrats(const Board& b, pla_t pla, const eval_t pValues[2][NUMTYPES],
  const Bitmap pStrongerMaps[2][NUMTYPES], int ufDist[BSIZE],
  Strat* strats, ostream* out)
{
  int numFrames = 0;
  FrameThreat frames[Strats::frameThreatMax];
  for(int i = 0; i<4; i++)
    numFrames += Strats::findFrame(b,pla,Board::TRAPLOCS[i],frames+numFrames);

  for(int i = 0; i<numFrames; i++)
    strats[i] = evalSingle(b,pla,frames[i],pValues,pStrongerMaps,ufDist,out);
  return numFrames;
}
