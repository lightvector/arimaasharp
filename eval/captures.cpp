/*
 * captures.cpp
 * Author: davidwu
 */
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"
#include "../eval/threats.h"
#include "../eval/internal.h"
#include "../eval/eval.h"
#include "../search/searchparams.h"

static const int CAP_NET_GAIN_SCALE = 38; //out of 64

static bool capInterfered(const Board& b, pla_t pla, loc_t eloc, loc_t loc, const int ufDist[BSIZE])
{
  return b.owners[loc] == pla && (b.pieces[loc] <= b.pieces[eloc] || ufDist[loc] != 0);
}

//Extra cost of sources of interference when pla is attempting to capture eloc in kt, assuming that eloc is radius 2 from kt
static bool capInterferePathR2(const Board& b, pla_t pla, loc_t kt, loc_t eloc, const int ufDist[BSIZE])
{
  if(capInterfered(b,pla,eloc,kt,ufDist))
    return true;

  //TODO rewrite can optimize things like this by subtracting first, then doing the mask or divide?
  int dx = gX(eloc) - gX(kt);
  int dy = gY(eloc) - gY(kt);
  if(dx == 0 || dy == 0)
  {
    if(dy < 0)       {if(capInterfered(b,pla,eloc,kt S,ufDist)) return true;}
    else if (dx < 0) {if(capInterfered(b,pla,eloc,kt W,ufDist)) return true;}
    else if (dx > 0) {if(capInterfered(b,pla,eloc,kt E,ufDist)) return true;}
    else             {if(capInterfered(b,pla,eloc,kt N,ufDist)) return true;}
  }
  else
  {
    loc_t kty = (dy > 0) ? kt N : kt S;
    loc_t ktx = (dx > 0) ? kt E : kt W;
    if(capInterfered(b,pla,eloc,kty,ufDist) && capInterfered(b,pla,eloc,ktx,ufDist))
      return true;
  }

  return false;
}

//ASSUMES maxCapDist <= 4
static int findCapThreats(Board& b, pla_t pla, loc_t kt, CapThreat* threats, const int ufDist[BSIZE],
    const Bitmap pStronger[2][NUMTYPES], int maxCapDist, int maxThreatLen)
{
  if(maxCapDist < 2)
    return 0;

  int numThreats = 0;
  pla_t opp = gOpp(pla);

  //Count up basic statistics
  int defCount = 0; //Defender count
  int removeDefSteps = 0; //+2 for each defender, +3 for each undominated defender
  piece_t strongestPiece = 0; //Strongest defender
  loc_t strongestLoc = ERRLOC; //Location of strongest defender
  loc_t defenders[4]; //Locations of defenders
  for(int i = 0; i<4; i++)
  {
    loc_t loc = kt + Board::ADJOFFSETS[i];
    if(b.owners[loc] == opp)
    {
      defenders[defCount] = loc;
      defCount++;
      removeDefSteps += 2;
      if(!b.isDominated(loc))
        removeDefSteps++;
      if(b.pieces[loc] > strongestPiece)
      {
        strongestPiece = b.pieces[loc];
        strongestLoc = loc;
      }
    }
  }

  //No hope of capture!
  if(removeDefSteps > maxCapDist || strongestPiece == ELE || (defCount == 0 && maxCapDist < 4))
    return 0;

  //Harder to cap when own piece on the trap
  if(defCount > 0 && b.owners[kt] == pla && !b.isDominating(kt))
    removeDefSteps++;

  //Can't possibly capture within max moves
  if(removeDefSteps > maxCapDist)
    return 0;

  //Defender count > 0
  if(defCount > 0)
  {
    //Expand outward over threateners
    int stepAdjustmentRadG1 = b.isDominated(strongestLoc) ? 0 : 1;
    for(int rad = 1; rad-2+removeDefSteps <= maxCapDist && rad <= 3; rad++)
    {
      //Find potential capturers
      Bitmap threateners;
      int stepAdjustment = 0;
      if(rad == 1)
        threateners = pStronger[opp][strongestPiece] & Board::DISK[rad][strongestLoc];
      else
      {
        //Adjust steps - one less if the strongest is dominated
        stepAdjustment = stepAdjustmentRadG1;
        threateners = pStronger[opp][strongestPiece] & Board::RADIUS[rad][strongestLoc];
      }

      //Iterate over the threateners!
      while(threateners.hasBits())
      {
        loc_t ploc = threateners.nextBit();

        //Init attack dist to be computed - 2 cases
        int attackSteps = 0;
        bool computeDistToStrongest = true;  //Do use dist to strongest, or just uf dist for threatener?

        //Case 0 - Always use UF dist for rad 1
        if(rad == 1)
          computeDistToStrongest = false;

        //Case 1 - Check if adjacent to some other defender
        else if(Board::manhattanDist(kt,ploc) == 2)
        {
          for(int k = 0; k<defCount; k++)
            if(Board::isAdjacent(ploc,defenders[k]))
            {computeDistToStrongest = false; break;}
        }

        //Compute attack dist!
        if(computeDistToStrongest)
          attackSteps = Threats::traverseAdjUfDist(b,pla,ploc,strongestLoc,ufDist,maxCapDist+1+stepAdjustment-removeDefSteps) - stepAdjustment;
        else
          attackSteps = ufDist[ploc];

        //Additional adjustment - don't double count the cost of a weak friendly piece on a trap in both the prelim checks and by attack dist
        if(b.owners[kt] == pla && rad == 2 && Board::isAdjacent(ploc,kt) && !b.isDominating(kt))
          attackSteps--;

        //Found a cap!
        if(removeDefSteps + attackSteps <= maxCapDist)
        {
          //Opp on trap
          if(b.owners[kt] == opp)
          {
            DEBUGASSERT(maxThreatLen > numThreats);
            threats[numThreats++] = CapThreat(removeDefSteps+attackSteps,kt,kt,ploc);
          }
          else
          {
            for(int j = 0; j<defCount; j++)
            {
              DEBUGASSERT(maxThreatLen > numThreats);
              threats[numThreats++] = CapThreat(removeDefSteps+attackSteps,defenders[j],kt,ploc);
            }
          }
        }
      }
    }
  }
  //Defender count == 0
  else
  {
    //Locate captures for further pieces at radius 2
    int rad = 2;
    //Compute base distance
    int basePushSteps = rad*2;

    //Iterate over potential capturable pieces
    Bitmap otherPieces = b.pieceMaps[opp][0] & Board::RADIUS[rad][kt];
    while(otherPieces.hasBits())
    {
      loc_t eloc = otherPieces.nextBit();
      if(capInterferePathR2(b,pla,kt,eloc,ufDist))
        continue;

      //Max rad to go to
      int maxThreatRad = maxCapDist + 1 - basePushSteps > 3 ? 3 : maxCapDist + 1 - basePushSteps;
      for(int threatRad = 1; threatRad <= maxThreatRad; threatRad++)
      {
        //Capturers
        Bitmap radThreateners = pStronger[opp][b.pieces[eloc]] & Board::RADIUS[threatRad][eloc];
        while(radThreateners.hasBits())
        {
          //Here we go!
          loc_t ploc = radThreateners.nextBit();
          int traverseDist = Threats::traverseAdjUfDist(b,pla,ploc,eloc,ufDist,maxThreatRad-1);
          int d = basePushSteps+traverseDist;
          if(d <= maxCapDist)
          {
            DEBUGASSERT(maxThreatLen > numThreats);
            threats[numThreats++] = CapThreat(d,eloc,kt,ploc);
          }
        }
      }
    }
  }

  //Ensure we don't raise any false alarms - find fastest cap
  int fastestDist = 64;
  for(int i = 0; i<numThreats; i++)
  {
    if(threats[i].dist < fastestDist && threats[i].dist != 0)
    {
      fastestDist = threats[i].dist;
      if(fastestDist <= 3 || (fastestDist <= 4 && removeDefSteps >= 4))
        break;
    }
  }

  //If fastest cap is exactly 4, on the edge of possibility...
  if(fastestDist == 4)
  {
    //Confirm that cap is actually possible!
    if(!BoardTrees::canCaps(b,pla,4,0,kt))
    {
      for(int i = 0; i<numThreats; i++)
        if(threats[i].dist <= 4 && threats[i].dist != 0)
          threats[i].dist = 5;
    }
  }

  return numThreats;
}


eval_t Captures::getEval(Board& b, pla_t pla, int numSteps, const eval_t pValues[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], const eval_t pieceThreats[BSIZE],
    const int numPStrats[2], const Strat pStrats[2][Strats::numStratsMax], eval_t& retRaw, loc_t& retCapLoc)
{
  //Find the biggest captures
  pla_t opp = gOpp(pla);
  const int capThreatMax = 48;
  CapThreat plaCapThreats[capThreatMax];
  int numPlaCapThreats = 0;

  for(int i = 0; i<4; i++)
  {
    loc_t kt = Board::TRAPLOCS[i];
    int pt = findCapThreats(b,pla,kt,plaCapThreats+numPlaCapThreats,ufDist,pStrongerMaps,4-b.step,capThreatMax-numPlaCapThreats);
    numPlaCapThreats += pt;
  }

  //Simulate making of the best capture
  loc_t bestCapLoc = ERRLOC;
  int bestNetGain = 0;
  int bestRaw = 0;
  for(int i = 0; i<numPlaCapThreats; i++)
  {
    CapThreat& threat = plaCapThreats[i];
    if(threat.dist >= 5)
      continue;
    int rawValue = Eval::evalCapOccur(b,pValues[opp],threat.oloc,threat.dist);
    int gain = rawValue - pieceThreats[threat.oloc];
    int loss = 0;
    if(threat.ploc != ERRLOC)
    {
      for(int j = 0; j<numPStrats[pla]; j++)
      {
        const Strat& strat = pStrats[pla][j];
        if(strat.pieceLoc == threat.oloc)
          continue;
        if(strat.plaHolders.isOne(threat.ploc))
          loss += strat.value;
      }
    }
    DEBUGASSERT(numSteps - threat.dist >= 0);
    loss += SearchParams::STEPS_LEFT_BONUS[numSteps] - SearchParams::STEPS_LEFT_BONUS[numSteps - threat.dist];
    int netGain = gain-loss;
    if(netGain > bestNetGain)
    {
      bestNetGain = netGain;
      bestRaw = rawValue;
      bestCapLoc = threat.oloc;
    }
  }

  retRaw = bestRaw;
  retCapLoc = bestCapLoc;
  return (bestNetGain * CAP_NET_GAIN_SCALE) >> 6;
}










