
/*
 * threats.cpp
 * Author: davidwu
 */

#include <cmath>
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"
#include "../eval/threats.h"

static int blockVal(const Board& b, pla_t pla, loc_t ploc, loc_t loc)
{
  if(ISE(loc))
  {
    if(Board::ISTRAP[loc])
    {
      int trapIndex = Board::TRAPINDEX[loc];
      if(b.trapGuardCounts[pla][trapIndex] <= (int)Board::isAdjacent(ploc,loc))
        return 1 + !b.canMakeTrapSafeFor(pla, loc, ploc);
    }
    return !b.wouldBeUF(pla,ploc,loc,ploc);
  }
  else if(ISP(loc))
    return 1;
  else
  {
    if(GT(ploc,loc))
    {
      if(Board::ISTRAP[loc] && b.trapGuardCounts[pla][Board::TRAPINDEX[loc]] < 1+(int)Board::isAdjacent(ploc,loc)
         && !b.canMakeTrapSafeFor(pla, loc, ploc))
        return 2;
      return 1;
    }
    return 2;
  }
}

const int Threats::ZEROS[BSIZE] = {
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
};

//Scans diagonal lines within the rectangle to the destination, summing up the min block value on each diagonal
static int blockValsPath(const Board& b, pla_t pla, loc_t ploc, loc_t dest,
    int dxTotal, int dyTotal, int dxDir, int dyDir, int maxBlock, const int* bonusVals = Threats::ZEROS)
{
  if(Board::manhattanDist(ploc,dest) == 1)
    return 0;

  int totalBlock = 0;
  for(int dist = 1; true; dist++)
  {
    int minBlock = maxBlock;
    int yi;
    for(int xi = min(dxTotal,dist); xi >= 0 && (yi = dist-xi) <= dyTotal; xi--)
    {
      loc_t loc = ploc+xi*gDXOffset(dxDir)+yi*gDYOffset(dyDir);
      int val = blockVal(b,pla,ploc,loc) + bonusVals[loc];
      if(val < minBlock)
      {
        minBlock = val;
        if(minBlock <= 0)
          break;
      }
    }

    totalBlock += minBlock;
    if(totalBlock >= maxBlock)
      return maxBlock;

    int dist2 = Board::manhattanDist(ploc,dest)-dist;
    if(dist2 <= dist)
      break;
    minBlock = maxBlock;
    for(int xi = min(dxTotal,dist2); xi >= 0 && (yi = dist2-xi) <= dyTotal; xi--)
    {
      loc_t loc = ploc+xi*gDXOffset(dxDir)+yi*gDYOffset(dyDir);
      int val = blockVal(b,pla,ploc,loc) + bonusVals[loc];
      if(val < minBlock)
      {
        minBlock = val;
        if(minBlock <= 0)
          break;
      }
    }
    totalBlock += minBlock;
    if(totalBlock >= maxBlock)
      return maxBlock;

    if(dist+1 >= dist2)
      break;
  }

  return totalBlock;
}

//A faster version of blockValsPath that only scans 1 diagonal from the source and destination unless
//the source and dest are directly in line
static int fastBlockValsPath(const Board& b, pla_t pla, loc_t ploc, loc_t dest,
    int dxTotal, int dyTotal, int dxDir, int dyDir, int maxBlock)
{
  int totalBlock = 0;
  int dxOffset = gDXOffset(dxDir);
  int dyOffset = gDYOffset(dyDir);
  if(dxTotal == 0)
  {
    for(int dy = 1; dy<dyTotal; dy++)
    {
      loc_t loc = ploc+dy*dyOffset;
      int val = blockVal(b,pla,ploc,loc);
      totalBlock += val;
      if(totalBlock >= maxBlock)
        return maxBlock;
    }
  }
  else if(dyTotal == 0)
  {
    for(int dx = 1; dx<dxTotal; dx++)
    {
      loc_t loc = ploc+dx*dxOffset;
      int val = blockVal(b,pla,ploc,loc);
      totalBlock += val;
      if(totalBlock >= maxBlock)
        return maxBlock;
    }
  }
  else
  {
    {
      loc_t loc = ploc+dxOffset;
      int val = blockVal(b,pla,ploc,loc);
      if(val > 0)
      {
        loc_t loc2 = ploc+dyOffset;
        int val2 = blockVal(b,pla,ploc,loc2);
        totalBlock += min(val,val2);
      }
    }
    if(dxTotal + dyTotal > 2)
    {
      loc_t loc = dest-dxOffset;
      int val = blockVal(b,pla,ploc,loc);
      if(val > 0)
      {
        loc_t loc2 = dest-dyOffset;
        int val2 = blockVal(b,pla,ploc,loc2);
        totalBlock += min(val,val2);
      }
    }
  }

  return totalBlock;
}

//ploc to get adjacent to dest ending UF, assumes ploc not rabbit
//From attacker persepecitve
int Threats::traverseAdjUfDist(const Board& b, pla_t pla, loc_t ploc, loc_t dest, const int ufDist[BSIZE], int maxDist)
{
  int uDist = ufDist[ploc];
  int manHatM1 = Board::manhattanDist(ploc,dest)-1;
  if(manHatM1 == 0)
    return uDist;

  int fastDistEst = uDist + manHatM1;
  if(fastDistEst >= maxDist)
    return fastDistEst;

  int best = maxDist;
  int dy = gY(dest) - gY(ploc);
  int dx = gX(dest) - gX(ploc);

  int bDist;
  //Duplication so that depending on direction, you test the spaces closer to you first
  if(dy == 0)
  {
    if(CW1(dest) && dx >= 0 && b.isRabOkay(pla,ploc,ploc,dest W) && (bDist = Board::manhattanDist(ploc,dest W)+blockVal(b,pla,ploc,dest W)) < best) {bDist += blockValsPath(b,pla,ploc,dest W, dx==0?1: dx-1,   dy<0?-dy:dy, dx==0?-1:1,  dy<0?-1:1, maxDist-bDist>2?2:maxDist-bDist);  if(bDist < best) {best = bDist; if(best <= manHatM1) {return uDist+best;}}}
    if(CE1(dest) && dx <= 0 && b.isRabOkay(pla,ploc,ploc,dest E) && (bDist = Board::manhattanDist(ploc,dest E)+blockVal(b,pla,ploc,dest E)) < best) {bDist += blockValsPath(b,pla,ploc,dest E, dx==0?1:-dx-1,   dy<0?-dy:dy, dx==0?1:-1,  dy<0?-1:1, maxDist-bDist>2?2:maxDist-bDist);  if(bDist < best) {best = bDist; if(best <= manHatM1) {return uDist+best;}}}
  }
  if(CS1(dest) && dy >= 0 && b.isRabOkay(pla,ploc,ploc,dest S) && (bDist = Board::manhattanDist(ploc,dest S)+blockVal(b,pla,ploc,dest S)) < best) {bDist += blockValsPath(b,pla,ploc,dest S,   dx<0?-dx:dx, dy==0?1: dy-1,  dx<0?-1:1, dy==0?-1:1, maxDist-bDist>2?2:maxDist-bDist);  if(bDist < best) {best = bDist; if(best <= manHatM1) {return uDist+best;}}}
  if(CN1(dest) && dy <= 0 && b.isRabOkay(pla,ploc,ploc,dest N) && (bDist = Board::manhattanDist(ploc,dest N)+blockVal(b,pla,ploc,dest N)) < best) {bDist += blockValsPath(b,pla,ploc,dest N,   dx<0?-dx:dx, dy==0?1:-dy-1,  dx<0?-1:1, dy==0?1:-1, maxDist-bDist>2?2:maxDist-bDist);  if(bDist < best) {best = bDist; if(best <= manHatM1) {return uDist+best;}}}
  if(dy != 0)
  {
    if(CW1(dest) && dx >= 0 && b.isRabOkay(pla,ploc,ploc,dest W) && (bDist = Board::manhattanDist(ploc,dest W)+blockVal(b,pla,ploc,dest W)) < best) {bDist += blockValsPath(b,pla,ploc,dest W, dx==0?1: dx-1,   dy<0?-dy:dy, dx==0?-1:1,  dy<0?-1:1, maxDist-bDist>2?2:maxDist-bDist);  if(bDist < best) {best = bDist; if(best <= manHatM1) {return uDist+best;}}}
    if(CE1(dest) && dx <= 0 && b.isRabOkay(pla,ploc,ploc,dest E) && (bDist = Board::manhattanDist(ploc,dest E)+blockVal(b,pla,ploc,dest E)) < best) {bDist += blockValsPath(b,pla,ploc,dest E, dx==0?1:-dx-1,   dy<0?-dy:dy, dx==0?1:-1,  dy<0?-1:1, maxDist-bDist>2?2:maxDist-bDist);  if(bDist < best) {best = bDist;}}
  }
  return uDist+best;
}

int Threats::traverseDist(const Board& b, loc_t ploc, loc_t dest, bool endUF,
    const int ufDist[BSIZE], int maxDist, const int* bonusVals)
{
  if(ploc == dest)
    return endUF ? ufDist[ploc] : 0;

  int dist = ufDist[ploc];
  dist += Board::manhattanDist(ploc,dest);
  if(dist >= maxDist)
    return dist;
  if(!b.isRabOkay(b.owners[ploc],ploc,ploc,dest))
    return maxDist;

  int dy = gY(dest) - gY(ploc);
  int dx = gX(dest) - gX(ploc);
  int dxDir = 1;
  int dyDir = 1;
  if(dx < 0) {dxDir = -1; dx = -dx;}
  if(dy < 0) {dyDir = -1; dy = -dy;}

  int partialDist = dist+blockVal(b,b.owners[ploc],ploc,dest) + bonusVals[dest];
  if(partialDist >= maxDist)
    return partialDist;
  int pathBlock = blockValsPath(b,b.owners[ploc],ploc,dest,dx,dy,dxDir,dyDir,maxDist-partialDist,bonusVals);
  return partialDist + pathBlock;
}

int Threats::fastTraverseAdjUFAssumeNonRabDist(const Board& b, loc_t ploc, loc_t dest, const int ufDist[BSIZE], int maxDist)
{
  int uDist = ufDist[ploc];
  int manHatM1 = Board::manhattanDist(ploc,dest)-1;
  if(manHatM1 == 0)
    return uDist;

  int fastDistEst = uDist + manHatM1;
  if(fastDistEst >= maxDist)
    return fastDistEst;

  int dy = gY(dest) - gY(ploc);
  int dx = gX(dest) - gX(ploc);
  int dxDir = 1;
  int dyDir = 1;
  if(dx < 0) {dxDir = -1; dx = -dx;}
  if(dy < 0) {dyDir = -1; dy = -dy;}

  int pathBlock = fastBlockValsPath(b,b.owners[ploc],ploc,dest,dx,dy,dxDir,dyDir,maxDist-fastDistEst);
  return fastDistEst + pathBlock;
}


int Threats::moveAdjDistUF(const Board& b, pla_t pla, loc_t loc, int maxDist,
    const int ufDist[BSIZE], const Bitmap pStrongerMaps[2][NUMTYPES],
    piece_t strongerThan, piece_t leqThan, loc_t blockApproachNotFrom)
{
  if(strongerThan == ELE || maxDist <= 0)
    return maxDist;

  Bitmap potentialPieces = pStrongerMaps == NULL ? b.pieceMaps[pla][0] :
      (pStrongerMaps[gOpp(pla)][strongerThan] & ~pStrongerMaps[gOpp(pla)][leqThan]);
  if((potentialPieces & Board::DISK[maxDist][loc]).isEmpty())
    return maxDist;

  int bestDist = maxDist;
  //Radius 1
  if(ISP(loc S) && b.pieces[loc S] > strongerThan && b.pieces[loc S] <= leqThan && b.isRabOkayN(pla,loc S)) {int dist = ufDist[loc S]; if(blockApproachNotFrom != ERRLOC && Board::manhattanDist(loc S,loc) < Board::manhattanDist(loc S,blockApproachNotFrom)) dist += 2; if(dist < bestDist) {bestDist = dist; if(bestDist <= 0) return bestDist;}}
  if(ISP(loc W) && b.pieces[loc W] > strongerThan && b.pieces[loc W] <= leqThan)                            {int dist = ufDist[loc W]; if(blockApproachNotFrom != ERRLOC && Board::manhattanDist(loc W,loc) < Board::manhattanDist(loc W,blockApproachNotFrom)) dist += 2; if(dist < bestDist) {bestDist = dist; if(bestDist <= 0) return bestDist;}}
  if(ISP(loc E) && b.pieces[loc E] > strongerThan && b.pieces[loc E] <= leqThan)                            {int dist = ufDist[loc E]; if(blockApproachNotFrom != ERRLOC && Board::manhattanDist(loc E,loc) < Board::manhattanDist(loc E,blockApproachNotFrom)) dist += 2; if(dist < bestDist) {bestDist = dist; if(bestDist <= 0) return bestDist;}}
  if(ISP(loc N) && b.pieces[loc N] > strongerThan && b.pieces[loc N] <= leqThan && b.isRabOkayS(pla,loc N)) {int dist = ufDist[loc N]; if(blockApproachNotFrom != ERRLOC && Board::manhattanDist(loc N,loc) < Board::manhattanDist(loc N,blockApproachNotFrom)) dist += 2; if(dist < bestDist) {bestDist = dist; if(bestDist <= 0) return bestDist;}}
  if(bestDist <= 1)
    return bestDist;

  //Radius 2 or more
  for(int rad = 2; rad<=maxDist; rad++)
  {
    Bitmap map = Board::RADIUS[rad][loc] & potentialPieces;
    while(map.hasBits())
    {
      loc_t ploc = map.nextBit();
      //TODO If a square has a high UFDist for an immofrozen piece, then count that in the block value.
      int dist = Threats::traverseAdjUfDist(b,pla,ploc,loc,ufDist,bestDist);
      if(blockApproachNotFrom != ERRLOC && Board::manhattanDist(ploc,loc) < Board::manhattanDist(ploc,blockApproachNotFrom))
        dist += 2;

      if(dist < bestDist)
      {
        bestDist = dist;
        if(bestDist <= rad-1)
          return bestDist;
      }
    }
    if(bestDist <= rad)
      return bestDist;
  }
  return bestDist;
}

int Threats::attackDist(const Board& b, loc_t ploc, int maxDist,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  return moveAdjDistUF(b,gOpp(b.owners[ploc]),ploc,maxDist,ufDist,pStrongerMaps,b.pieces[ploc]);
}
int Threats::attackDistNonEle(const Board& b, loc_t ploc, int maxDist,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  return moveAdjDistUF(b,gOpp(b.owners[ploc]),ploc,maxDist,ufDist,pStrongerMaps,b.pieces[ploc],CAM);
}

//Adds a special case where if the attacked piece is on a home trap, the attackdist might be increased if the attacker
//can't easily pushpull it forward
int Threats::attackDistPushpullingForward(const Board& b, loc_t ploc, int maxDist,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  loc_t blockApproachNotFrom = ERRLOC;
  pla_t owner = b.owners[ploc];
  if(Board::ISTRAP[ploc])
  {
    int trapIndex = Board::TRAPINDEX[ploc];
    if(Board::ISPLATRAP[trapIndex][owner] && (b.trapGuardCounts[owner][trapIndex] >= 2 || b.trapGuardCounts[gOpp(owner)][trapIndex] < 2))
      blockApproachNotFrom = ploc + (owner == GOLD ? N : S);
  }
  return moveAdjDistUF(b,gOpp(owner),ploc,maxDist,ufDist,pStrongerMaps,b.pieces[ploc],ELE,blockApproachNotFrom);
}

int Threats::attackDistPushpullingForwardNonEle(const Board& b, loc_t ploc, int maxDist,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  loc_t blockApproachNotFrom = ERRLOC;
  pla_t owner = b.owners[ploc];
  if(Board::ISTRAP[ploc])
  {
    int trapIndex = Board::TRAPINDEX[ploc];
    if(Board::ISPLATRAP[trapIndex][owner] && (b.trapGuardCounts[owner][trapIndex] >= 2 || b.trapGuardCounts[gOpp(owner)][trapIndex] < 2))
      blockApproachNotFrom = ploc + (owner == GOLD ? N : S);
  }
  return moveAdjDistUF(b,gOpp(owner),ploc,maxDist,ufDist,pStrongerMaps,b.pieces[ploc],CAM,blockApproachNotFrom);
}


//Dist for pla to occupy loc^M
//Not quite accurate - disregards freezing at the end
int Threats::occupyDist(const Board& b, pla_t pla, loc_t loc, int maxDist,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  if(maxDist == 0 || b.owners[loc] == pla)
    return 0;
  if(b.owners[loc] == NPLA)
    return 1+moveAdjDistUF(b,pla,loc,maxDist-1,ufDist,pStrongerMaps,EMP);
  return 2+attackDist(b,loc,maxDist-2,pStrongerMaps,ufDist);
}


//CAPTURE FINDING--------------------------------------------------------------

static int capInterfereCost(const Board& b, pla_t pla, loc_t eloc, loc_t loc, loc_t kt, const int ufDist[BSIZE])
{
  if(b.owners[loc] == NPLA)
    return 0;
  else if(b.owners[loc] == pla)
    return (int)(b.pieces[loc] <= b.pieces[eloc]) + (int)(ufDist[loc] != 0);
  else
  {
    if(Board::isAdjacent(loc,kt))
      return 0;
    return 2;
  }
}

//Extra cost of sources of interference when pla is attempting to capture eloc in kt
static int capInterferePathCost(const Board& b, pla_t pla, loc_t kt, loc_t eloc, const int ufDist[BSIZE], int maxDist)
{
  int mdist = Board::manhattanDist(kt,eloc);
  if(mdist < 2)
    return 0;

  int cost = 0;
  cost += capInterfereCost(b,pla,eloc,kt,kt,ufDist);
  if(cost >= maxDist)
    return cost;

  //TODO rewrite can optimize things like this by subtracting first, then doing the mask or divide?
  int dx = gX(eloc) - gX(kt);
  int dy = gY(eloc) - gY(kt);
  if(dx == 0 || dy == 0)
  {
    if(dy < 0)       cost += capInterfereCost(b,pla,eloc,kt S,kt,ufDist);
    else if (dx < 0) cost += capInterfereCost(b,pla,eloc,kt W,kt,ufDist);
    else if (dx > 0) cost += capInterfereCost(b,pla,eloc,kt E,kt,ufDist);
    else             cost += capInterfereCost(b,pla,eloc,kt N,kt,ufDist);
  }
  else
  {
    loc_t kty = (dy > 0) ? kt N : kt S;
    loc_t ktx = (dx > 0) ? kt E : kt W;
    int v = capInterfereCost(b,pla,eloc,kty,kt,ufDist);
    if(v > 0)
    {int v2 = capInterfereCost(b,pla,eloc,ktx,kt,ufDist); cost += v > v2 ? v2 : v;}
  }

  if(cost >= maxDist)
    return cost;

  if(mdist > 2)
  {
    if(dx == 0 || dy == 0)
    {
      if(dy < 0)       cost += capInterfereCost(b,pla,eloc,eloc N,kt,ufDist);
      else if (dx < 0) cost += capInterfereCost(b,pla,eloc,eloc E,kt,ufDist);
      else if (dx > 0) cost += capInterfereCost(b,pla,eloc,eloc W,kt,ufDist);
      else             cost += capInterfereCost(b,pla,eloc,eloc S,kt,ufDist);
    }
    else
    {
      loc_t elocy = (dy > 0) ? eloc S : eloc N;
      loc_t elocx = (dx > 0) ? eloc W : eloc E;
      int v = capInterfereCost(b,pla,eloc,elocy,kt,ufDist);
      if(v > 0)
      {int v2 = capInterfereCost(b,pla,eloc,elocx,kt,ufDist); cost += v > v2 ? v2 : v;}
    }
  }

  return cost;
}

int Threats::findCapThreats(Board& b, pla_t pla, loc_t kt, CapThreat* threats, const int ufDist[BSIZE],
    const Bitmap pStronger[2][NUMTYPES], int maxCapDist, int maxThreatLen, int& rdSteps)
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

  //Store prelim value in case we return
  rdSteps = removeDefSteps;

  //No hope of capture!
  if(rdSteps > maxCapDist || strongestPiece == ELE || defCount == 4)
    return 0;

  //Instant capture!
  if(defCount == 0 && b.owners[kt] == opp)
    threats[numThreats++] = CapThreat(0,kt,kt,ERRLOC);

  //Harder to cap when own piece on the trap
  if(defCount > 0 && b.owners[kt] == pla && !b.isDominating(kt))
    removeDefSteps++;
  //Harder to cap when opp piece on the trap.
  else if(defCount > 0 && removeDefSteps > 4 && b.owners[kt] == opp)
    removeDefSteps++;

  //Can't possibly capture within max moves
  if(removeDefSteps > maxCapDist)
    return numThreats;

  //Total steps counting pushing the strongest
  int removeDefStepsTotal = 16;
  if(defCount == 0)
    removeDefStepsTotal = 0;
  else if(defCount > 0)
  {
    //Expand outward over threateners
    int firstCapRad = 16;
    int stepAdjustmentRadG1 = b.isDominated(strongestLoc) ? 0 : 1;
    for(int rad = 1; rad-2+removeDefSteps <= maxCapDist && rad <= firstCapRad+2; rad++)
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
          attackSteps = traverseAdjUfDist(b,pla,ploc,strongestLoc,ufDist,maxCapDist+1+stepAdjustment-removeDefSteps) - stepAdjustment;
        else
          attackSteps = ufDist[ploc];

        //Additional adjustment - don't double count the cost of a weak friendly piece on a trap in both the prelim checks and by attack dist
        if(b.owners[kt] == pla && rad == 2 && Board::isAdjacent(ploc,kt) && !b.isDominating(kt))
          attackSteps--;

        //Found a cap!
        if(removeDefSteps + attackSteps <= maxCapDist)
        {
          //If shortest for trap, mark it
          if(removeDefSteps + attackSteps < removeDefStepsTotal)
            removeDefStepsTotal = removeDefSteps + attackSteps;

          //Mark closest rad so we know when to stop
          if(firstCapRad == 16)
            firstCapRad = rad;

          //Extra if opp on trap already
          int extra = 0;
          //Opp on trap
          if(b.owners[kt] == opp)
          {
            DEBUGASSERT(maxThreatLen > numThreats);
            extra = 3;
            threats[numThreats++] = CapThreat(removeDefSteps+attackSteps,kt,kt,ploc);
          }
          //Defenders
          int d = removeDefSteps+attackSteps+extra;
          if(d <= maxCapDist)
          {
            for(int j = 0; j<defCount; j++)
            {
              DEBUGASSERT(maxThreatLen > numThreats);
              threats[numThreats++] = CapThreat(d,defenders[j],kt,ploc);
            }
          }
        }
      }
    }
  }

  //Store remove def steps
  rdSteps = removeDefStepsTotal;

  //Locate captures for further pieces
  //Iterate out by radius
  for(int rad = 2; rad*2+removeDefStepsTotal <= maxCapDist; rad++)
  {
    //Compute base distance
    int basePushSteps = rad*2+removeDefStepsTotal;

    //Iterate over potential capturable pieces
    Bitmap otherPieces = b.pieceMaps[opp][0] & Board::RADIUS[rad][kt];
    while(otherPieces.hasBits())
    {
      loc_t eloc = otherPieces.nextBit();
      int totalPushSteps = basePushSteps + capInterferePathCost(b,pla,kt,eloc,ufDist,maxCapDist-basePushSteps+1);

      //Max rad to go to
      int maxThreatRad = maxCapDist + 1 - totalPushSteps > 3 ? 3 : maxCapDist + 1 - totalPushSteps;
      for(int threatRad = 1; threatRad <= maxThreatRad; threatRad++)
      {
        //Capturers
        Bitmap radThreateners = pStronger[opp][b.pieces[eloc]] & Board::RADIUS[threatRad][eloc];
        while(radThreateners.hasBits())
        {
          //Here we go!
          loc_t ploc = radThreateners.nextBit();
          int traverseDist = traverseAdjUfDist(b,pla,ploc,eloc,ufDist,maxThreatRad-1);
          int d = totalPushSteps+traverseDist;
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
  if(removeDefSteps <= 4)
  {
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
      //Handle instacaps by temp removal
      bool instaCap = false;
      piece_t oldPiece = 0;
      if(defCount == 0 && b.owners[kt] == opp)
      {
        instaCap = true;
        oldPiece = b.pieces[kt];
        b.owners[kt] = NPLA;
        b.pieces[kt] = EMP;
      }
      //Confirm that cap is actually possible!
      if(!BoardTrees::canCaps(b,pla,4,0,kt))
      {
        for(int i = 0; i<numThreats; i++)
          if(threats[i].dist <= 4 && threats[i].dist != 0)
            threats[i].dist = 5;
      }
      //Undo temp removal
      if(instaCap)
      {
        b.owners[kt] = opp;
        b.pieces[kt] = oldPiece;
      }
    }
  }

  return numThreats;
}

//The goaldist of a square is an estimate of the number of steps a rabbit would
//take to reach goal if it reached there, NOT counting the time required to make it empty
//Plas are counted less than normal because they help goals
void Threats::getGoalDistBitmapEst(const Board& b, pla_t pla, Bitmap goalDistIs[9])
{
  pla_t opp = gOpp(pla);
  Bitmap plas = b.pieceMaps[pla][0];
  Bitmap opps = b.pieceMaps[opp][0];
  Bitmap empty = ~(plas | opps);
  Bitmap open = Bitmap::adj(empty);
  Bitmap openWE = Bitmap::adjWE(empty);
  Bitmap guarded = Bitmap::adj(plas);
  Bitmap oppRab = b.pieceMaps[opp][RAB];
  Bitmap oppNonRab = opps & ~oppRab;
  Bitmap dominatey = Bitmap::BMPTRAPS | Bitmap::adj(oppNonRab);
  Bitmap freezy = ~guarded & dominatey;
  Bitmap plaRad2NonRabUF = Bitmap::adj(plas & ~b.pieceMaps[pla][RAB] & ~b.frozenMap);
  plaRad2NonRabUF |= Bitmap::adj(plaRad2NonRabUF);

  goalDistIs[0] = Board::GOALMASKS[pla];
  Bitmap reachIn1 = Bitmap::adjNS(goalDistIs[0] & (empty | (plas & openWE)));
  Bitmap reachIn2 = Bitmap::adjNS(goalDistIs[0] & (plas & openWE & ~b.dominatedMap));
  Bitmap reachIn3 = Bitmap::adjNS(goalDistIs[0] & (plas | (opps & b.dominatedMap & ~dominatey & openWE)));
  Bitmap reachIn4 = Bitmap::adjNS(goalDistIs[0] & (oppRab & plaRad2NonRabUF & openWE));

  goalDistIs[1] = goalDistIs[0] | (~freezy & reachIn1);
  reachIn2 |= Bitmap::adj(goalDistIs[1] & (empty | (plas & open)));
  reachIn3 |= Bitmap::adj(goalDistIs[1] & plas);
  reachIn4 |= Bitmap::adj(goalDistIs[1] & (opps & b.dominatedMap & ~dominatey));
  Bitmap reachIn5 = Bitmap::adj(goalDistIs[1] & (oppRab & plaRad2NonRabUF));

  goalDistIs[2] = goalDistIs[1] | (reachIn1 & plaRad2NonRabUF) | (~freezy & reachIn2);
  reachIn3 |= Bitmap::adj(goalDistIs[2] & (empty | (plas & open)));
  reachIn4 |= Bitmap::adj(goalDistIs[2] & plas);
  reachIn5 |= Bitmap::adj(goalDistIs[2] & (opps & b.dominatedMap & ~dominatey));
  Bitmap reachIn6 = Bitmap::adj(goalDistIs[2] & (oppRab & plaRad2NonRabUF));

  goalDistIs[3] = goalDistIs[2] | reachIn1 | (reachIn2 & plaRad2NonRabUF) | (~freezy & reachIn3);
  reachIn4 |= Bitmap::adj(goalDistIs[3] & (empty | (plas & open)));
  reachIn5 |= Bitmap::adj(goalDistIs[3] & plas);
  reachIn6 |= Bitmap::adj(goalDistIs[3] & (opps & b.dominatedMap & ~dominatey));
  Bitmap reachIn7 = Bitmap::adj(goalDistIs[3] & (oppRab & plaRad2NonRabUF));

  goalDistIs[4] = goalDistIs[3] | reachIn2 | (reachIn3 & plaRad2NonRabUF) | (~freezy & reachIn4);
  reachIn5 |= Bitmap::adj(goalDistIs[4] & (empty | (plas & open)));
  reachIn6 |= Bitmap::adj(goalDistIs[4] & plas);
  reachIn7 |= Bitmap::adj(goalDistIs[4] & (opps & b.dominatedMap & ~dominatey));
  Bitmap reachIn8 = Bitmap::adj(goalDistIs[4] & (oppRab & plaRad2NonRabUF));

  goalDistIs[5] = goalDistIs[4] | reachIn3 | (reachIn4 & plaRad2NonRabUF) | (~freezy & reachIn5);
  reachIn6 |= Bitmap::adj(goalDistIs[5] & (empty | (plas & open)));
  reachIn7 |= Bitmap::adj(goalDistIs[5] & plas);
  reachIn8 |= Bitmap::adj(goalDistIs[5] & (opps & b.dominatedMap & ~dominatey));

  goalDistIs[6] = goalDistIs[5] | reachIn4 | (reachIn5 & plaRad2NonRabUF) | (~freezy & reachIn6);
  reachIn7 |= Bitmap::adj(goalDistIs[6] & (empty | (plas & open)));
  reachIn8 |= Bitmap::adj(goalDistIs[7] & plas);

  goalDistIs[7] = goalDistIs[6] | reachIn5 | (reachIn6 & plaRad2NonRabUF) | (~freezy & reachIn7);
  reachIn8 |= Bitmap::adj(goalDistIs[7] & (empty | (plas & open)));

  goalDistIs[8] = goalDistIs[7] | reachIn6 | (reachIn7 & plaRad2NonRabUF) | (~freezy & reachIn8);
}


//The value filled into a square is an estimate of:
//The number of steps it would take a rabbit to reach goal if it were here
//PLUS the number of steps required to make that square empty and freezesafe/trapsafe.
void Threats::getGoalDistEst(const Board& b, pla_t pla, int goalDist[BSIZE], int max)
{
  loc_t queueBuf[64];
  loc_t queueBuf2[64];

  for(int i = 0; i<BSIZE; i++)
    goalDist[i] = max;

  int numInQueue = 0;
  loc_t* oldQueue = queueBuf;
  loc_t* newQueue = queueBuf2;
  Bitmap added;

  pla_t opp = gOpp(pla);
  Bitmap plaNonRab = b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB];

  int goalOffset = (pla == GOLD) ? gLoc(0,7) : gLoc(0,0);
  for(int x = 0; x<8; x++)
  {
    loc_t loc = goalOffset+x;

    //Blockyness for the goal line
    int extra = 0;
    if(b.owners[loc] == NPLA) extra = 0;
    else if(b.owners[loc] == pla)
    {
      if(b.isFrozen(loc))
      {
        extra++;
        if(!(b.owners[loc W] == NPLA || (b.owners[loc W] == opp && b.pieces[loc W] < b.pieces[loc] && b.isOpen(loc W))))
          extra += 2;
      }
      if(b.owners[loc W] != NPLA && b.owners[loc E] != NPLA)
        extra++;
    }
    else
    {
      if(b.isDominated(loc))
        extra = 2;
      else if(b.owners[loc W] == NPLA || b.owners[loc E] == NPLA)
        extra = 3;
      else
        extra = 4;
    }

    goalDist[loc] = extra;
    added.setOn(loc);
    oldQueue[numInQueue++] = loc;
  }

  for(int dist = 0; dist < max-1; dist++)
  {
    int newNumInQueue = 0;
    for(int i = 0; i<numInQueue; i++)
    {
      loc_t loc = oldQueue[i];
      if(goalDist[loc] != dist)
      {
        newQueue[newNumInQueue++] = loc;
        continue;
      }
      for(int dir = 0; dir<4; dir++)
      {
        if(!Board::ADJOKAY[dir][loc])
          continue;
        loc_t adj = loc + Board::ADJOFFSETS[dir];
        if(added.isOne(adj))
          continue;
        added.setOn(adj);

        int extra = 0;
        //Blockyness in general
        if(b.owners[adj] == NPLA)
        {
          if(!b.isTrapSafe2(pla,adj) || (b.wouldRabbitBeDomAt(pla,adj) && (Board::DISK[1][adj] & plaNonRab).isEmpty()))
          {
            extra++;
            if((Board::DISK[2][adj] & plaNonRab).isEmpty())
              extra++;
          }
        }
        else if(b.owners[adj] == pla) extra = 1 + (int)b.isFrozen(adj) + (int)(!b.isOpen2(adj));
        else extra = 2 + (int)(!b.isDominated(adj)) + (int)(b.pieces[adj] != RAB) + (int)(!b.isOpen2(adj));

        added.setOn(adj);
        int total = dist+extra+1;
        if(total > max)
          continue;
        goalDist[adj] = total;
        newQueue[newNumInQueue++] = adj;
      }
    }
    loc_t* temp = newQueue;
    newQueue = oldQueue;
    oldQueue = temp;
    numInQueue = newNumInQueue;
  }
}
