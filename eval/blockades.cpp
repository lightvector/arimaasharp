/*
 * blockades.cpp
 * Author: davidwu
 */
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtreeconst.h"
#include "../eval/threats.h"
#include "../eval/internal.h"

//ROTATION----------------------------------------------------------------------------
static void fillBlockadeRotationBonusSteps(Bitmap heldMap, Bitmap holderMap, int singleBonusSteps[BSIZE], int multiBonusSteps[BSIZE])
{
  for(int i = 0; i<BSIZE; i++)
  {singleBonusSteps[i] = 0; multiBonusSteps[i] = 0;}
  while(heldMap.hasBits())
  {loc_t loc = heldMap.nextBit(); singleBonusSteps[loc] = 8; multiBonusSteps[loc] = 8;}
  while(holderMap.hasBits())
  {multiBonusSteps[holderMap.nextBit()] = -1;}
}

static BlockadeRotation getBlockadeSingleRotation(const Board& b, pla_t pla, loc_t ploc, Bitmap heldMap, Bitmap holderMap,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], int singleBonusSteps[BSIZE], int multiBonusSteps[BSIZE],
    bool& tryMultiSwarm, piece_t& minStrNeeded)
{
  pla_t opp = gOpp(pla);
  int bestBlockerDist = 10;
  loc_t bestBlockerLoc = ERRLOC;
  bool bestIsSwarm = false;
  tryMultiSwarm = false;
  minStrNeeded = RAB;

  //Special case - on a trap
  if(Board::ISTRAP[ploc] && b.isOpenToMove(ploc) && (!b.isTrapSafe2(opp,ploc) || b.isBlocked(ploc)))
    return BlockadeRotation(bestBlockerLoc,false,2);

  tryMultiSwarm = true;

  //Try rotation of a single piece. If a weaker piece could be put here, attempt to find such a weaker piece and put it here.
  //Meanwhile finding how strong a piece we need if we are doing it nonswarming
  piece_t minStr = RAB;
  Bitmap map = heldMap & ~b.frozenMap;
  bool isBlocked = b.isBlocked(ploc);
  bool canSwarm = isBlocked;
  while(map.hasBits())
  {
    loc_t loc = map.nextBit();
    if(!Board::isAdjacent(loc,ploc)) continue;
    if(b.pieces[loc] > minStr) minStr = b.pieces[loc];
  }
  map = heldMap & b.frozenMap;
  while(map.hasBits())
  {
    loc_t loc = map.nextBit();
    if(!Board::isAdjacent(loc,ploc)) continue;
    if(b.wouldBeDom(opp,loc,loc,ploc)) continue;
    if(b.pieces[loc] == RAB)
    {
      if(!b.isRabOkay(opp,loc,ploc)) continue;
      if(b.isOpenToStep(loc,ploc)) {canSwarm = false; tryMultiSwarm = false; if(CAT > minStr) {minStr = CAT;}}
    }
    else
    {
      canSwarm = false; tryMultiSwarm = false;
      if(b.pieces[loc]+1 > minStr) {minStr = b.pieces[loc]+1;}
    }
  }
  minStrNeeded = minStr;
  //Locate all weaker pieces strong enough to handle it singly
  Bitmap possibleBlockers = minStr <= RAB ? b.pieceMaps[pla][0] : pStrongerMaps[opp][minStr-1];
  possibleBlockers &= ~holderMap & ~pStrongerMaps[opp][b.pieces[ploc]-1];

  //Find closest, iterating out by radius
  {
    const int maxRad = 6;
    //An extra 1 if blocked for radius 1, an extra 1 if blocked for all higher radii
    int extraSteps = isBlocked;
    for(int rad = 1; rad < maxRad && rad < bestBlockerDist - extraSteps; rad++)
    {
      Bitmap radBlockers = possibleBlockers & Board::RADIUS[rad][ploc];
      while(radBlockers.hasBits())
      {
        loc_t loc = radBlockers.nextBit();
        bool bonused[4] = {false,false,false,false};
        for(int i = 0; i < 4; i++)
        {
          if(!Board::ADJOKAY[i][ploc]) continue;
          loc_t adjloc =  ploc + Board::ADJOFFSETS[i];
          if(!b.wouldBeUF(pla,loc,adjloc,ploc)) {bonused[i] = true; singleBonusSteps[adjloc] += 1;}
        }

        int tdist = Threats::traverseDist(b,loc,ploc,false,ufDist,bestBlockerDist-extraSteps,singleBonusSteps);
        for(int i = 0; i < 4; i++)
          if(bonused[i])
          {loc_t adjloc =  ploc + Board::ADJOFFSETS[i]; singleBonusSteps[adjloc] -= 1;}

        int dist = tdist + extraSteps;
        if(dist < bestBlockerDist || (dist == bestBlockerDist && bestBlockerLoc != ERRLOC && b.pieces[loc] < b.pieces[bestBlockerLoc]))
        {
          bestBlockerLoc = loc;
          bestBlockerDist = dist;

          if(rad >= bestBlockerDist - extraSteps)
            break;
        }
      }
      //1 if blocked for radius 1, 2 if blocked for all higher radii
      if(rad == 1 && isBlocked)
        extraSteps++;
    }
  }

  //Handle the case where we can swarm block, but do it with only one piece rotated in
  //because the rest of the swarm is already there.

  if(canSwarm && minStr > RAB)
  {
    tryMultiSwarm = false;
    Bitmap swarmPossibleBlockers = b.pieceMaps[pla][0] & ~holderMap & ~possibleBlockers;
    const int maxRad = 6;
    int extraSteps = 4; //We're caught in the middle, so we have to do a lot of shuffling.
    for(int rad = 2; rad < maxRad && rad < bestBlockerDist - extraSteps; rad++)
    {
      Bitmap radBlockers = swarmPossibleBlockers & Board::RADIUS[rad][ploc];
      while(radBlockers.hasBits())
      {
        loc_t loc = radBlockers.nextBit();
        int tdist = Threats::traverseDist(b,loc,ploc,false,ufDist,bestBlockerDist-extraSteps,multiBonusSteps);

        int dist = tdist + extraSteps;
        if(dist < bestBlockerDist)
        {
          bestBlockerLoc = loc;
          bestBlockerDist = dist;
          bestIsSwarm = true;

          if(rad < bestBlockerDist - extraSteps)
            break;
        }
      }
    }
  }

  if(minStr <= RAB)
    tryMultiSwarm = false;

  return BlockadeRotation(bestBlockerLoc,bestIsSwarm,bestBlockerDist < 10 ? bestBlockerDist : 16);
}

static BlockadeRotation getBlockadeMultiRotation(const Board& b, pla_t pla, loc_t ploc, Bitmap holderMap,
    const Bitmap pStrongerMaps[2][NUMTYPES])
{
  pla_t opp = gOpp(pla);
  //Count how many new blockers we need
  int numBlockersNeeded = 1;
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][ploc]) continue;
    loc_t loc = ploc + Board::ADJOFFSETS[i];
    if(b.owners[loc] == NPLA)
      numBlockersNeeded += 1;
  }
  if(numBlockersNeeded == 1)
    return BlockadeRotation(ERRLOC,false,16);

  //Iterate out, looking for blockers
  int bestBlockerDist = 11;
  loc_t bestBlockerLoc = ERRLOC;
  int numBlockersFound = 0;
  int totalDist = 0;
  //For general inefficiency and confusion in shuffling around
  int extraDist = 5;
  Bitmap possibleBlockers = b.pieceMaps[pla][0] & ~holderMap & ~pStrongerMaps[opp][b.pieces[ploc]-1];
  const int maxRad = 4;
  for(int rad = 2; rad <= maxRad && totalDist + (numBlockersNeeded - numBlockersFound)*(rad-1) + extraDist < bestBlockerDist; rad++)
  {
    Bitmap radBlockers = possibleBlockers & Board::RADIUS[rad][ploc];
    int numNewBlockers = radBlockers.countBits();
    if(numNewBlockers > numBlockersNeeded-numBlockersFound)
      numNewBlockers = numBlockersNeeded-numBlockersFound;
    numBlockersFound += numNewBlockers;
    totalDist += numNewBlockers*(rad-1);
    if(numBlockersFound >= numBlockersNeeded)
    {
      bestBlockerLoc = radBlockers.nextBit();
      break;
    }
  }
  if(numBlockersFound >= numBlockersNeeded)
    bestBlockerDist = totalDist + extraDist;
  return BlockadeRotation(bestBlockerLoc,true,bestBlockerDist < 11 ? bestBlockerDist : 16);
}


//TODO Use restrict keyword?
//holderRotBlockers can sometimes be errorsquare
int Blockades::fillBlockadeRotationArrays(const Board& b, pla_t pla, loc_t* holderRotLocs, int* holderRotDists,
    loc_t* holderRotBlockers, bool* holderIsSwarm, int* minStrNeededArr, Bitmap heldMap, Bitmap holderMap, Bitmap rotatableMap,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  //Simulate blocks
  int singleBonusSteps[BSIZE];
  int multiBonusSteps[BSIZE];
  fillBlockadeRotationBonusSteps(heldMap,holderMap,singleBonusSteps,multiBonusSteps);
  int n = 0;
  Bitmap map = rotatableMap & b.pieceMaps[pla][0];
  while(map.hasBits())
  {
    loc_t loc = map.nextBit();
    bool tryMultiSwarm = false;
    int minStrNeeded = RAB;
    BlockadeRotation rot = getBlockadeSingleRotation(b, pla, loc, heldMap, holderMap,
        pStrongerMaps, ufDist, singleBonusSteps, multiBonusSteps, tryMultiSwarm, minStrNeeded);

    holderRotLocs[n] = loc;
    holderRotDists[n] = rot.dist;
    holderRotBlockers[n] = rot.blockerLoc;
    holderIsSwarm[n] = rot.isSwarm;
    minStrNeededArr[n] = minStrNeeded;
    n++;

    if(tryMultiSwarm)
    {
      BlockadeRotation rot2 = getBlockadeMultiRotation(b,pla,loc,holderMap,pStrongerMaps);
      holderRotLocs[n] = loc;
      holderRotDists[n] = rot2.dist;
      holderRotBlockers[n] = rot2.blockerLoc;
      holderIsSwarm[n] = rot2.isSwarm;
      minStrNeededArr[n] = RAB;
      n++;
    }

  }
  int numRotatable = n;

  //Find multistep rotations - can i be replaced by j after j is replaced by some other piece?
  for(int i = 0; i<numRotatable; i++)
  {
    loc_t loc = holderRotLocs[i];
    int minStrNeeded = minStrNeededArr[i];
    if(minStrNeeded <= RAB)
      continue;
    for(int j = 0; j<numRotatable; j++)
    {
      loc_t loc2 = holderRotLocs[j];
      if(j == i || b.pieces[loc2] < minStrNeeded || b.pieces[loc2] >= b.pieces[loc] || holderRotDists[j] > holderRotDists[i]-2)
        continue;
      int dist = Threats::traverseDist(b,loc2,loc,false,ufDist,holderRotDists[i]-holderRotDists[j]-1,singleBonusSteps);
      int totalDist = dist + 1 + holderRotDists[j];
      if(totalDist < holderRotDists[i])
      {
        holderRotDists[i] = totalDist;
        holderRotBlockers[i] = holderRotBlockers[j];
        holderIsSwarm[i] = holderIsSwarm[j];
      }
    }
  }
  return numRotatable;
}

static int getBlockadeBreakDistance(const Board& b, pla_t pla, loc_t bloc, Bitmap defenseMap,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  pla_t opp = gOpp(pla);

  loc_t ktLoc = Board::ADJACENTTRAP[bloc];

  //For each square, compute ability to break, and take the min
  int minBreakDist = 16;
  while(defenseMap.hasBits())
  {
    loc_t loc = defenseMap.nextBit();
    int bs = 16;
    if(b.owners[loc] == opp)
    {
      if(b.isOpenToStep(loc))
        bs = ufDist[loc]+3;
      else if(b.isOpenToMove(loc))
        bs = ufDist[loc]+4;

      if(ktLoc != ERRLOC && Board::isAdjacent(ktLoc,loc))
      {
        int numAdjBloc = 0;
        for(int i = 0; i<4; i++)
        {
          loc_t loc2 = loc + Board::ADJOFFSETS[i];
          if(Board::isAdjacent(loc2,bloc))
            numAdjBloc++;
        }
        if(numAdjBloc == 1)
          bs = 7;
      }
    }
    else if(b.owners[loc] == pla)
    {
      //Don't bother attacking pieces that are completely interior
      if(b.isBlocked(loc) && !b.isDominated(loc))
        continue;
      bs = Threats::attackDist(b,loc,min(5,minBreakDist-3),pStrongerMaps,ufDist)+3;
      if(Board::ISTRAP[loc] && (!b.isTrapSafe2(opp,loc) || b.isBlocked(loc)))
        bs += 2;
      if(ktLoc != ERRLOC && Board::isAdjacent(ktLoc,loc))
      {
        int numAdjBloc = 0;
        for(int i = 0; i<4; i++)
        {
          loc_t loc2 = loc + Board::ADJOFFSETS[i];
          if(Board::isAdjacent(loc2,bloc))
            numAdjBloc++;
        }
        if(numAdjBloc == 1)
          bs = 16;
      }
    }
    else
    {
      //Probably a square in the middle of a blockade that's actually quite okay to be open.
      if(!Board::ISTRAP[loc])
        continue;

      if(b.isTrapSafe2(opp,loc) && !b.isBlocked(loc))
        bs = 2;
      else
        bs = Threats::moveAdjDistUF(b,opp,loc,min(5,minBreakDist-3),ufDist,pStrongerMaps,RAB)+3; //TODO questionable - do we need the piece to be unfrozen?
    }

    if(bs < minBreakDist)
      minBreakDist = bs;
  }

  if(minBreakDist < 0)
    minBreakDist = 0;

  return minBreakDist;
}




//Value for holding an elephant
//ImmoTypes: Immo, Almost immo, CentralBlocked
//BreakDist small: -
//Opp advancement of non-rabbit pieces: -
//Opp advancement of rabbit pieces: +
//[immotype][loc]
//Top side is holder's side.
static const int E_IMMO_VAL[BSIZE] =
{
  4800,4200,3700,3800,3800,3700,4200,4800,  0,0,0,0,0,0,0,0,
  4000,3250,3600,2900,2900,3600,3250,4000,  0,0,0,0,0,0,0,0,
  3000,2200,1500,1000,1000,1500,2200,3000,  0,0,0,0,0,0,0,0,
  2100,1300,1300, 800, 800,1300,1300,2100,  0,0,0,0,0,0,0,0,
  2100,1300,1000, 800, 800,1000,1300,2100,  0,0,0,0,0,0,0,0,
  2200,1500,1000, 700, 700,1000,1500,2200,  0,0,0,0,0,0,0,0,
  2700,1900,1700,1200,1200,1700,1900,2700,  0,0,0,0,0,0,0,0,
  3200,2500,2000,1800,1800,2000,2500,3200,  0,0,0,0,0,0,0,0,
};

//Out of 100
//[isholderturn*4 + steps][breaksteps]
static const int IMMO_BREAKSTEPS_FACTOR[8][9] =
{
    {50,55,62,59,76,87,94,102,107},
    {52,57,63,69,79,88,95,102,107},
    {56,63,66,73,81,89,96,102,107},
    {58,63,69,76,83,90,96,102,107},
    {62,67,74,82,86,91,96,102,107},
    {61,65,72,80,85,91,96,102,107},
    {59,64,70,78,84,90,96,102,107},
    {58,63,69,74,82,90,96,102,107},
};

//[goalydist], from perspective of opp, counts rabbits recursed on
static const int OPP_RAB_IN_BLOCKADE_BONUS[8] =
{0,100,200,130,110,110,150,50};

//[mdist+ydist+ADV_RABBIT_XDIST_BONUS[xdist] from ele]
static const int RELEVANT_ELE_ADVANCE_RADIUS = 7;
//[x1-x2+7]
static const int ADV_RABBIT_XDIST_BONUS[15] =
{5,4,3,2,1,0,0,0,0,0,1,2,3,4,5};
static const int ADV_RABBIT_YDIST_BONUS[15] =
{7,6,5,4,3,2,1,0,1,2,3,4,5,6,7};
static const int BLOCKADE_ADVANCE_RABBIT_BONUS[32] =
{144,144,144,144,140, 130,120,100,80, 64,48,34,22, 11,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0, 0,0,0};
static const int BLOCKADE_ADVANCE_MHH_PENALTY[32] =
{200,200,200,200,194, 182,168,140,118, 96,74,52,32, 16,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0, 0,0,0};

//TODO expand to account for inner?
//TODO expand to account for location?
//Basic cost per pla holder in blockade, by numstronger, 7 = rabbit
static const int PLA_HOLDER_COST[8] = {160,90,65,50,45,40,35,30};

//Factor for having few leftover pieces [numpieces*2]
static const int NUM_LEFTOVER_PIECES_FACTOR[34] =
{0,0, 0,0,10,16, 22,36,50,60, 70,76,82,89, 95,97,99,100, 100,100,100,100, 100,100,100,100, 100,100,100,100, 100,100,100,100};


//[E][M][m]
static const int EMm_BLOCKADE_SFP_FACTOR[2][4][4] =
{
 //E not free
 {{30,15,5,5},{100,30,15,15},{120,50,30,30},{120,50,30,30}},
 //E free
 {{110,98,85,85},{120,101,87,87},{130,110,90,90},{130,110,90,90}},
};

//[Rotationsteps * 2, with some bonus values added for weak breakdist, whose turn it is etc]
static const int ROTATION_INTERP[33] =
{99, 99,99,98,97, 95,92,89,86, 84,81,78,75, 71,67,63,60, 56,52,48,45, 41,37,33,30, 26,22,18,14, 10,6,2,0};
static const int NUM_LEFTOVER_PIECES_ROTATE_MULTI_ADD[34] =
{33,33, 33,26,19,15, 11,9,7,5, 4,3,2,1, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};

//Make advancing be "farther" from the blockade to EDISTANCE and MDISTANCE bonuses
static const int DISTANCE_ADJ[2][BSIZE] =
{{
 3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
 3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
 1,2,3,4,4,3,2,1,  0,0,0,0,0,0,0,0,
 1,1,2,3,3,2,1,1,  0,0,0,0,0,0,0,0,
 0,1,2,2,2,2,1,0,  0,0,0,0,0,0,0,0,
 0,1,2,3,3,2,1,0,  0,0,0,0,0,0,0,0,
 0,0,1,1,1,1,0,0,  0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
},
{
 0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
 0,0,1,1,1,1,0,0,  0,0,0,0,0,0,0,0,
 0,1,2,3,3,2,1,0,  0,0,0,0,0,0,0,0,
 0,1,2,2,2,2,1,0,  0,0,0,0,0,0,0,0,
 1,1,2,3,3,2,1,1,  0,0,0,0,0,0,0,0,
 1,2,3,4,4,3,2,1,  0,0,0,0,0,0,0,0,
 3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
 3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
}};


static const int EDISTANCE_FACTOR[21] =
{0, 84,84,84,86, 90,95,100,103, 105,106,106,106, 106,106,106,106, 106,106,106,106};
static const int EDISTANCE_BONUS[21] =
{0, 0,0,0,60,  105,145,185,200, 205,205,205,205, 205,205,205,205, 205,205,205,205,};
static const int MDISTANCE_BONUS[21] =
{0, 0,0,25,45, 65,85,95,100, 100,100,100,100, 100,100,100,100, 100,100,100,100};


//b is not const because the blockade detection code needs to plop down fake elephants to detect the tightness
//of various loose blockades
int Blockades::evalEleBlockade(Board& b, pla_t pla, const int pStronger[2][NUMTYPES], const Bitmap pStrongerMaps[2][NUMTYPES],
    const int tc[2][4], int ufDist[BSIZE], pla_t mainPla, eval_t earlyBlockadePenalty, ostream* out)
{
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  loc_t oppEleLoc;
  {
    Bitmap freezeHeldMap;
    oppEleLoc = Strats::findEBlockade(b,pla,immoType,recursedMap,holderHeldMap,freezeHeldMap);
    if(oppEleLoc == ERRLOC)
      return 0;
    while(freezeHeldMap.hasBits())
    {
      loc_t loc = freezeHeldMap.nextBit();
      loc_t bestHolder = ERRLOC;
      if(b.owners[loc S] == pla && b.pieces[loc S] > b.pieces[loc])
      {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc S) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc S] ) bestHolder = loc S;}
      if(b.owners[loc W] == pla && b.pieces[loc W] > b.pieces[loc])
      {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc W) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc W] ) bestHolder = loc W;}
      if(b.owners[loc E] == pla && b.pieces[loc E] > b.pieces[loc])
      {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc E) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc E] ) bestHolder = loc E;}
      if(b.owners[loc N] == pla && b.pieces[loc N] > b.pieces[loc])
      {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc N) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc N] ) bestHolder = loc N;}
      holderHeldMap.setOn(bestHolder);
    }
  }
  pla_t opp = gOpp(pla);

  //Pieces that are threatening to break the blockade but are frozen
  /*
  Bitmap specialFreezeHolderMap;
  {
    Bitmap specialFreezeHeldMap = b.frozenMap & pStrongerMaps[pla][RAB] & Bitmap::adj(holderHeldMap) & ~holderHeldMap;
    if(specialFreezeHeldMap.hasBits())
    {
      while(specialFreezeHeldMap.hasBits())
      {
        //Make sure it's actually dominating something in the blockade
        loc_t loc = fhmap.nextBit();
        if((holderHeldMap & b.pieceMaps[pla][0] & ~pStrongerMaps[opp][b.pieces[loc]-1] & Board::RADIUS[1][loc]).isEmpty())
        {specialFreezeHeldMap.setOff(loc); continue;}

        loc_t bestHolder = ERRLOC;
        if(b.owners[loc S] == pla && b.pieces[loc S] > b.pieces[loc])
        {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc S) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc S] ) bestHolder = loc S;}
        if(b.owners[loc W] == pla && b.pieces[loc W] > b.pieces[loc])
        {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc W) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc W] ) bestHolder = loc W;}
        if(b.owners[loc E] == pla && b.pieces[loc E] > b.pieces[loc])
        {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc E) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc E] ) bestHolder = loc E;}
        if(b.owners[loc N] == pla && b.pieces[loc N] > b.pieces[loc])
        {if(bestHolder == ERRLOC || (holderHeldMap.isOne(loc N) && !holderHeldMap.isOne(bestHolder)) || b.pieces[bestHolder] > b.pieces[loc N] ) bestHolder = loc N;}
        specialFreezeHolderMap.setOn(bestHolder);
      }
      holderHeldMap |= specialFreezeHolderMap | specialFreezeHeldMap;
    }
  }*/

  int baseValue = E_IMMO_VAL[pla == GOLD ? oppEleLoc : gRevLoc(oppEleLoc)];

  //Adjust by breakSteps
  Bitmap heldImmoMap = holderHeldMap & b.pieceMaps[opp][0] & ~b.frozenMap;
  while(heldImmoMap.hasBits())
    ufDist[heldImmoMap.nextBit()] += 8;
  int breakSteps = getBlockadeBreakDistance(b, pla, oppEleLoc, holderHeldMap | recursedMap, pStrongerMaps, ufDist);
  heldImmoMap = holderHeldMap & b.pieceMaps[opp][0] & ~b.frozenMap;
  while(heldImmoMap.hasBits())
    ufDist[heldImmoMap.nextBit()] -= 8;
  int breakStepsFactor = IMMO_BREAKSTEPS_FACTOR[(b.player == pla)*4 + b.step][breakSteps > 8 ? 8 : breakSteps];


  int plaHolderCost = 0;
  Bitmap plaHolderMap = holderHeldMap & b.pieceMaps[pla][0] & ~Bitmap::BMPTRAPS;;
  int numUsedPiecesX2 = plaHolderMap.countBits() * 2;
  //Add half the pieces around the trap, if there is one.
  Bitmap aroundTrapMap = Board::ADJACENTTRAP[oppEleLoc] == ERRLOC || b.owners[Board::ADJACENTTRAP[oppEleLoc]] == opp ? Bitmap::BMPZEROS
      : (Board::DISK[1][Board::ADJACENTTRAP[oppEleLoc]] & b.pieceMaps[pla][0] & ~plaHolderMap);
  numUsedPiecesX2 += aroundTrapMap.countBitsIterative();
  int numLeftoverPiecesX2 = b.pieceCounts[pla][0]*2 - numUsedPiecesX2;
  int leftoverFactor = NUM_LEFTOVER_PIECES_FACTOR[numLeftoverPiecesX2];
  while(plaHolderMap.hasBits())
  {
    loc_t loc = plaHolderMap.nextBit();
    int idx = b.pieces[loc] == RAB ? 7 : pStronger[pla][b.pieces[loc]];
    plaHolderCost += PLA_HOLDER_COST[idx];
  }
  while(aroundTrapMap.hasBits())
  {
    loc_t loc = aroundTrapMap.nextBit();
    int idx = b.pieces[loc] == RAB ? 7 : pStronger[pla][b.pieces[loc]];
    plaHolderCost += PLA_HOLDER_COST[idx]/2;
  }

  int oppRabInBlockadeBonus = 0;
  Bitmap oppRabInBlockadeMap = b.pieceMaps[opp][RAB] & recursedMap;
  while(oppRabInBlockadeMap.hasBits())
    oppRabInBlockadeBonus += OPP_RAB_IN_BLOCKADE_BONUS[7-Board::GOALYDIST[pla][oppRabInBlockadeMap.nextBit()]];

  int pieceAdvancementPenalty = 0;
  int rabbitAdvancementBonus = 0;
  loc_t center;
  if(Board::GOALYDIST[pla][oppEleLoc] < 4)
    center = oppEleLoc - Board::GOALLOCINC[pla];
  else
    center = oppEleLoc;
  Bitmap advancementMap =
      (Board::DISK[RELEVANT_ELE_ADVANCE_RADIUS][center] & ((pStrongerMaps[pla][CAT] & ~holderHeldMap) | b.pieceMaps[opp][RAB]));
  while(advancementMap.hasBits())
  {
    loc_t loc = advancementMap.nextBit();
    int dist = Board::manhattanDist(loc,center) + ADV_RABBIT_XDIST_BONUS[gX(loc) - gX(center) + 7] + ADV_RABBIT_YDIST_BONUS[gY(loc) - gY(center) + 7];
    if(b.pieces[loc] == RAB)
    {
      //Squares behind traps
      if(Bitmap::BMPBEHINDTRAPS.isOne(loc))
        dist += 2;
      if(out)
        (*out) << dist << endl;
      rabbitAdvancementBonus += BLOCKADE_ADVANCE_RABBIT_BONUS[dist];
      if(Board::ISTRAP[loc] && (pla == GOLD) == (gY(loc) < 4) && holderHeldMap.isOne(loc))
        rabbitAdvancementBonus += 40;
    }
    else if(pStronger[opp][b.pieces[loc]] == 1)
      pieceAdvancementPenalty += BLOCKADE_ADVANCE_MHH_PENALTY[dist];
    else if(pStronger[opp][b.pieces[loc]] == 2)
      pieceAdvancementPenalty += BLOCKADE_ADVANCE_MHH_PENALTY[dist] * 2 / 3;
    else if(pStronger[opp][b.pieces[loc]] <= 4)
      pieceAdvancementPenalty += BLOCKADE_ADVANCE_MHH_PENALTY[dist] * 1 / 3;
  }
  //Advancing rabbits is worse when the opponent really needs to make those replacements.
  rabbitAdvancementBonus = rabbitAdvancementBonus * 3 * (80+100-leftoverFactor) / 160;

  //Don't overvalue rabbit advancement bonus when the ele is not so advanced.
  const int RAB_ADV_BONUS_FACTOR_OUT_OF_16[8] =
  {16,16,14,10,7,6,5,2};
  rabbitAdvancementBonus = rabbitAdvancementBonus * RAB_ADV_BONUS_FACTOR_OUT_OF_16[Board::GOALYDIST[opp][oppEleLoc]] / 16;

  //Compute rotations
  Bitmap rotatableMap = holderHeldMap;
  if(immoType >= Strats::CENTRAL_BLOCK) rotatableMap &= (b.pieceMaps[pla][ELE] | b.pieceMaps[pla][CAM]);
  else rotatableMap &= b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB] & ~b.pieceMaps[pla][CAT];
  int numRotatableMax = rotatableMap.countBits()*2;
  int numRotatable;
  loc_t holderRotLocs[numRotatableMax];
  int holderRotDists[numRotatableMax];
  loc_t holderRotBlockers[numRotatableMax];
  bool holderIsSwarm[numRotatableMax];
  int minStrNeededArr[numRotatableMax];

  int sfpBaseFactor;
  int sfpRotatedFactor;
  int bestRotateLoc = 0;
  int bestRotateyness = 0;
  int mPiece = ELE;
  int hPiece = ELE;
  {
    int numEs = b.pieceCounts[pla][ELE] - (holderHeldMap & b.pieceMaps[pla][ELE]).hasBits();
    int numPlaMs = 0;
    int numOppMs = 0;
    {
      int piece;
      for(piece = CAM; piece > RAB; piece--)
      {
        if(b.pieceCounts[pla][piece] > 0 || b.pieceCounts[opp][piece] > 0)
        {
          numPlaMs = b.pieceCounts[pla][piece];
          numOppMs = b.pieceCounts[opp][piece];
          piece--;
          if(b.pieceCounts[opp][piece+1] == 0)
          {
            while(b.pieceCounts[opp][piece] == 0 && piece > RAB)
            {numPlaMs += b.pieceCounts[pla][piece]; piece--;}
          }
          else if(b.pieceCounts[pla][piece+1] == 0)
          {
            while(b.pieceCounts[pla][piece] == 0 && piece > RAB)
            {numOppMs += b.pieceCounts[opp][piece]; piece--;}
          }
          break;
        }
      }
      mPiece = piece+1;

      //int numPlaHs = 0;
      //int numOppHs = 0;
      for(; piece > RAB; piece--)
      {
        if(b.pieceCounts[pla][piece] > 0 || b.pieceCounts[opp][piece] > 0)
        {
          //numPlaHs = b.pieceCounts[pla][piece];
          //numOppHs = b.pieceCounts[opp][piece];
          piece--;
          if(b.pieceCounts[opp][piece+1] == 0)
          {
            while(b.pieceCounts[opp][piece] == 0 && piece > RAB)
            {
              //numPlaHs += b.pieceCounts[pla][piece];
              piece--;
            }
          }
          else if(b.pieceCounts[pla][piece+1] == 0)
          {
            while(b.pieceCounts[pla][piece] == 0 && piece > RAB)
            {
              //numOppHs += b.pieceCounts[opp][piece];
              piece--;
            }
          }
          break;
        }
      }
      hPiece = piece+1;
    }

    numPlaMs -= (holderHeldMap & pStrongerMaps[opp][mPiece-1] & ~b.pieceMaps[pla][ELE]).countBitsIterative();
    numOppMs -= (holderHeldMap & pStrongerMaps[pla][mPiece-1] & ~b.pieceMaps[opp][ELE]).countBitsIterative();
    //numPlaHs -= (holderHeldMap & pStrongerMaps[opp][hPiece-1] & ~pStrongerMaps[opp][mPiece-1]).countBitsIterative();
    //numOppHs -= (holderHeldMap & pStrongerMaps[pla][hPiece-1] & ~pStrongerMaps[pla][mPiece-1]).countBitsIterative();

    if(numPlaMs < 0) numPlaMs = 0;
    if(numOppMs < 0) numOppMs = 0;
    if(numPlaMs > 3) numPlaMs = 3;
    if(numOppMs > 3) numOppMs = 3;

    sfpBaseFactor = EMm_BLOCKADE_SFP_FACTOR[numEs][numPlaMs][numOppMs];
    sfpRotatedFactor = sfpBaseFactor;

    numRotatable = fillBlockadeRotationArrays(b,pla,holderRotLocs,holderRotDists,holderRotBlockers,holderIsSwarm,minStrNeededArr,
        holderHeldMap & b.pieceMaps[opp][0], holderHeldMap & b.pieceMaps[pla][0],rotatableMap & pStrongerMaps[opp][hPiece-1], pStrongerMaps, ufDist);

    for(int i = 0; i<numRotatable; i++)
    {
      loc_t rotLoc = holderRotLocs[i];
      int otherSFPFactor = sfpBaseFactor;
      piece_t piece = b.pieces[rotLoc];
      piece_t blockerPiece = holderRotBlockers[i] == ERRLOC ? EMP : b.pieces[holderRotBlockers[i]];
      if(holderIsSwarm[i])
      {
        piece_t strongestAdj = b.strongestAdjacentPla(pla,rotLoc);
        if(strongestAdj > blockerPiece)
          blockerPiece = strongestAdj;
      }
      if(piece == ELE)
      {
        if(blockerPiece >= mPiece)
          otherSFPFactor = EMm_BLOCKADE_SFP_FACTOR[numEs+1][max(numPlaMs-1,0)][numOppMs];
        else
          otherSFPFactor = EMm_BLOCKADE_SFP_FACTOR[numEs+1][numPlaMs][numOppMs];
      }
      else if(piece >= mPiece)
      {
        if(blockerPiece < mPiece)
          otherSFPFactor = EMm_BLOCKADE_SFP_FACTOR[numEs][min(numPlaMs+1,2)][numOppMs];
      }
      else continue;

      int rotateDist = holderRotDists[i] * 2;
      //if((pStrongerMaps[pla][blockerPiece] & ~holderHeldMap & Board::DISK[1][rotLoc]).hasBits())
        rotateDist += 2;
      //if((pStrongerMaps[pla][blockerPiece] & ~holderHeldMap & Board::DISK[3][rotLoc]).hasBits())
      //  rotateDist += 1;
      if(holderIsSwarm[i] && numLeftoverPiecesX2 <= 0)
        continue;
      else if(holderIsSwarm[i])
        rotateDist += NUM_LEFTOVER_PIECES_ROTATE_MULTI_ADD[numLeftoverPiecesX2];

      if(b.player == pla)
        rotateDist -= (3-b.step);
      if(rotateDist < 0)
        rotateDist = 0;
      if(rotateDist > 32)
        rotateDist = 32;

      int lambda = ROTATION_INTERP[rotateDist];
      int fctr = (otherSFPFactor * lambda + sfpBaseFactor * (100-lambda))/100;

      if(holderIsSwarm[i])
        fctr = fctr * NUM_LEFTOVER_PIECES_FACTOR[numLeftoverPiecesX2-2] / (NUM_LEFTOVER_PIECES_FACTOR[numLeftoverPiecesX2] + 1);
      if(fctr > sfpRotatedFactor)
      {
        sfpRotatedFactor = fctr;
        bestRotateLoc = rotLoc;
        bestRotateyness = rotateDist;
      }
    }
  }

  int eDistFactor = 100;
  int distBonus = 0;
  Bitmap eleMap = b.pieceMaps[pla][ELE];
  while(eleMap.hasBits())
  {
    loc_t loc = eleMap.nextBit();
    int dist = DISTANCE_ADJ[pla][loc] + Board::manhattanDist(loc,oppEleLoc);
    distBonus += EDISTANCE_BONUS[dist];
    eDistFactor = EDISTANCE_FACTOR[dist];
  }
  for(int piece = CAM; piece >= mPiece && piece >= RAB; piece--)
  {
    Bitmap map = b.pieceMaps[pla][piece];
    while(map.hasBits())
    {
      loc_t loc = map.nextBit();
      distBonus += MDISTANCE_BONUS[DISTANCE_ADJ[pla][loc] + Board::manhattanDist(loc,oppEleLoc)];
    }
  }

  int tc0 = tc[pla][Board::PLATRAPINDICES[opp][0]];
  int tc1 = tc[pla][Board::PLATRAPINDICES[opp][1]];
  int tc2 = tc[pla][Board::PLATRAPINDICES[pla][0]];
  int tc3 = tc[pla][Board::PLATRAPINDICES[pla][1]];
  int tcBonus = 60 + (tc0+tc1+tc2/2+tc3/2) +
      ((tc0 <= 0 ? 0 : tc0 * tc0) +
       (tc1 <= 0 ? 0 : tc1 * tc1) -
       (tc2 >= 0 ? 0 : tc2 * tc2) -
       (tc3 >= 0 ? 0 : tc3 * tc3))/30;

  //Asymmetric blockade eval to be scared of bait and tackle
  if(gOpp(pla) == mainPla && (immoType == Strats::TOTAL_IMMO || immoType == Strats::ALMOST_IMMO)
     && Board::GOALYDIST[gOpp(pla)][oppEleLoc] <= 3)
  {
    if(Board::GOALYDIST[gOpp(pla)][oppEleLoc] <= 1)
    {
      baseValue = baseValue * 5 / 3 + earlyBlockadePenalty;
      oppRabInBlockadeBonus = oppRabInBlockadeBonus * 5 / 3;
      rabbitAdvancementBonus = rabbitAdvancementBonus * 5 / 3;
    }
    else
      baseValue = baseValue * 5 / 4;
  }

  int value = (baseValue + oppRabInBlockadeBonus - pieceAdvancementPenalty + rabbitAdvancementBonus - plaHolderCost + distBonus + tcBonus)
  * breakStepsFactor * leftoverFactor / 10000 * eDistFactor * max(sfpRotatedFactor,0) / 10000;

  if(immoType == Strats::ALMOST_IMMO)
    value = (value - 200) * 6/7;
  else if(immoType == Strats::CENTRAL_BLOCK)
    value = (value - 500) / 9 + distBonus/3 + tcBonus/2;
  else if(immoType == Strats::CENTRAL_BLOCK_WEAK)
    value = (value - 500) / 15 + distBonus/4 + tcBonus/2;

  if(value < 0)
    value = 0;
  if(value > 4200) //Reduce super-high-valued blockades
    value = 4200 + (value - 4200)/2;

  ONLYINDEBUG(
  if(out)
  {
    (*out) << "--Blockade Type " << immoType << "--" << endl;
    (*out) << "Base value: " << baseValue << endl;
    (*out) << "SFP: " << sfpBaseFactor << endl;
    (*out) << "SFP Rotated: " << sfpRotatedFactor << " best loc " << bestRotateLoc << " " << bestRotateyness << endl;
    (*out) << "BreakSteps: " << breakSteps << " " << breakStepsFactor << endl;
    (*out) << "RabInBlockadeBonus: " << oppRabInBlockadeBonus << endl;
    (*out) << "RabAdvBonus: " << rabbitAdvancementBonus << endl;
    (*out) << "PieceAdvPenalty: " << pieceAdvancementPenalty << endl;
    (*out) << "PlaHolderCost: " << plaHolderCost << endl;
    (*out) << "NumLeftoverFactor: " << leftoverFactor << endl;
    (*out) << "Distbonus: " << distBonus << endl;
    (*out) << "EDistFactor: " << eDistFactor << endl;
    (*out) << "TCbonus: " << tcBonus << endl;
    (*out) << "Final: " << value << endl;

    for(int i = 0; i<numRotatable; i++)
    {
      loc_t loc = holderRotLocs[i];
      int rotDist = holderRotDists[i];
      loc_t blockerLoc = holderRotBlockers[i];
      if(rotDist >= 16)
        continue;
      (*out) << "Rotate " << (int)loc << " in " << rotDist  << " with " << (int)blockerLoc << endl;
    }
  }
  )
  return value;
}
