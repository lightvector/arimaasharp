/*
 * strats.cpp
 * Author: davidwu
 */

#include <cmath>
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"
#include "../eval/strats.h"



//FRAMES --------------------------------------------------------------

//Attempts to detect whether pla holds a frame around kt, and if so, stores the relevant info in the given FrameThreat reference.
//Detects partial frames - that is, frames that are open but such that a piece stepping off could immediately be pushed back on.
static Bitmap getFrameHolderMap(const Board& b, pla_t pla, loc_t kt)
{
  pla_t opp = gOpp(pla);
  piece_t piece = b.pieces[kt];
  Bitmap plaGeq;
  for(int i = piece; i<=ELE; i++)
    plaGeq |= b.pieceMaps[pla][i];

  Bitmap map = Bitmap::adj(Bitmap::makeLoc(kt));
  map &= ~b.pieceMaps[opp][0];
  map |= Bitmap::adj(map & b.pieceMaps[pla][0] & ~plaGeq);
  map.setOff(kt);
  return map;
}

int Strats::findFrame(const Board& b, pla_t pla, loc_t kt, FrameThreat* threats)
{
  //Ensure that there is an opp on the trap!
  pla_t opp = gOpp(pla);
  if(b.owners[kt] != opp)
  {return 0;}

  //Check each direction. At most one partial allowed, at most one opponent allowed.
  FrameThreat threat;
  threat.pinnedLoc = ERRLOC;
  int partialcount = 0;
  int oppCount = 0;
  if     (b.owners[kt S] == NPLA) {if(b.isRabOkayS(opp,kt)) {return 0;}}
  else if(b.owners[kt S] == opp)  {oppCount++; threat.pinnedLoc = kt S; if(oppCount > 1) return 0;}
  else if(b.pieces[kt S] < b.pieces[kt] && b.isOpen(kt S)) {partialcount++; if(partialcount > 1 || b.wouldBeUF(opp,kt,kt S,kt)) return 0;}

  if     (b.owners[kt W] == NPLA) {return 0;}
  else if(b.owners[kt W] == opp)  {oppCount++; threat.pinnedLoc = kt W; if(oppCount > 1) return 0;}
  else if(b.pieces[kt W] < b.pieces[kt] && b.isOpen(kt W)) {partialcount++; if(partialcount > 1 || b.wouldBeUF(opp,kt,kt W,kt)) return 0;}

  if     (b.owners[kt E] == NPLA) {return 0;}
  else if(b.owners[kt E] == opp)  {oppCount++; threat.pinnedLoc = kt E; if(oppCount > 1) return 0;}
  else if(b.pieces[kt E] < b.pieces[kt] && b.isOpen(kt E)) {partialcount++; if(partialcount > 1 || b.wouldBeUF(opp,kt,kt E,kt)) return 0;}

  if     (b.owners[kt N] == NPLA) {if(b.isRabOkayN(opp,kt)) {return 0;}}
  else if(b.owners[kt N] == opp)  {oppCount++; threat.pinnedLoc = kt N; if(oppCount > 1) return 0;}
  else if(b.pieces[kt N] < b.pieces[kt] && b.isOpen(kt N)) {partialcount++; if(partialcount > 1 || b.wouldBeUF(opp,kt,kt N,kt)) return 0;}

  if(partialcount >= 1)
    threat.isPartial = true;
  else
    threat.isPartial = false;

  threat.kt = kt;
  threat.holderMap = getFrameHolderMap(b,pla,kt);
  threats[0] = threat;
  return 1;
}


static const int AUTOBLOCKADE[BSIZE] =
{
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
2,3,3,3,3,3,3,2,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
2,3,3,3,3,3,3,2,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
};

//loc contains opp. Pla is blocking
static Bitmap stepBlockMap(const Board& b, pla_t pla, loc_t oloc, loc_t loc)
{
  pla_t opp = gOpp(pla);
  Bitmap map = Board::DISK[1][loc];
  if(CS1(loc) && !b.isRabOkayS(opp,loc))
    map.setOff(loc S);
  else if(CN1(loc) && !b.isRabOkayN(opp,loc))
    map.setOff(loc N);
  map.setOff(oloc);
  return map;
}

//loc contains opp. Pla is blocking
static Bitmap stepPushBlockMap(const Board& b, pla_t pla, loc_t oloc, loc_t loc)
{
  if(b.pieces[loc] == RAB) {return stepBlockMap(b,pla,oloc,loc);}
  pla_t opp = gOpp(pla);
  Bitmap bmap;
  if(CS1(loc) && loc != oloc)
  {
    if     (b.owners[loc S] == opp) {bmap |= stepBlockMap(b,pla,loc,loc S);}
    else if(b.owners[loc S] == pla && b.pieces[loc S] >= b.pieces[loc]) {bmap.setOn(loc S);}
    else if(b.owners[loc S] == pla) {bmap |= Board::DISK[1][loc S] & ~Bitmap::makeLoc(loc);}
  }
  if(CW1(loc) && loc != oloc)
  {
    if     (b.owners[loc W] == opp) {bmap |= stepBlockMap(b,pla,loc,loc W);}
    else if(b.owners[loc W] == pla && b.pieces[loc W] >= b.pieces[loc]) {bmap.setOn(loc W);}
    else if(b.owners[loc W] == pla) {bmap |= Board::DISK[1][loc W] & ~Bitmap::makeLoc(loc);}
  }
  if(CE1(loc) && loc != oloc)
  {
    if     (b.owners[loc E] == opp) {bmap |= stepBlockMap(b,pla,loc,loc E);}
    else if(b.owners[loc E] == pla && b.pieces[loc E] >= b.pieces[loc]) {bmap.setOn(loc E);}
    else if(b.owners[loc E] == pla) {bmap |= Board::DISK[1][loc E] & ~Bitmap::makeLoc(loc);}
  }
  if(CN1(loc) && loc != oloc)
  {
    if     (b.owners[loc N] == opp) {bmap |= stepBlockMap(b,pla,loc,loc N);}
    else if(b.owners[loc N] == pla && b.pieces[loc N] >= b.pieces[loc]) {bmap.setOn(loc N);}
    else if(b.owners[loc N] == pla) {bmap |= Board::DISK[1][loc N] & ~Bitmap::makeLoc(loc);}
  }
  return bmap;
}

//Helper checking if pla is blockading oloc. Can oloc escape through loc? Mark the relevant blockers. Assume oloc is not frozen.
static int getBlockadeStr(Board& b, pla_t pla, loc_t oloc, loc_t loc, Bitmap& defMap, int max)
{
  pla_t opp = gOpp(pla);
  if(b.owners[loc] == pla)
  {
    if(b.pieces[loc] >= b.pieces[oloc]) {defMap.setOn(loc); return BlockadeThreat::FULLBLOCKADE;}
    if(!b.isTrapSafe2(opp,loc)) {defMap.setOn(loc); return BlockadeThreat::FULLBLOCKADE;}
    if(!b.isOpen(loc)) {defMap |= Board::DISK[1][loc] & ~Bitmap::makeLoc(oloc); return BlockadeThreat::FULLBLOCKADE;}
    if(max <= BlockadeThreat::FULLBLOCKADE) return BlockadeThreat::OPENBLOCKADE;

    if(AUTOBLOCKADE[loc] <= BlockadeThreat::LOOSEBLOCKADE) {defMap.setOn(loc); return BlockadeThreat::LOOSEBLOCKADE;}
    if(!b.isOpen2(loc))
    {
      loc_t openloc = b.findOpen(loc);
      b.tempPP(oloc,loc,openloc);
      Bitmap tempMap;
      bool good =
        (!(CS1(loc) && loc S != oloc && getBlockadeStr(b,pla,loc,loc S,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
         !(CW1(loc) && loc W != oloc && getBlockadeStr(b,pla,loc,loc W,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
         !(CE1(loc) && loc E != oloc && getBlockadeStr(b,pla,loc,loc E,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
         !(CN1(loc) && loc N != oloc && getBlockadeStr(b,pla,loc,loc N,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE));
      b.tempPP(openloc,loc,oloc);
      if(good) {tempMap.setOff(openloc); tempMap.setOn(loc); defMap |= tempMap; return BlockadeThreat::LOOSEBLOCKADE;}
    }
    if(max <= BlockadeThreat::LOOSEBLOCKADE) return BlockadeThreat::OPENBLOCKADE;

    if(AUTOBLOCKADE[loc] <= BlockadeThreat::LEAKYBLOCKADE) {defMap.setOn(loc); return BlockadeThreat::LEAKYBLOCKADE;}
    if(b.isTrap(loc)) {defMap.setOn(loc); return BlockadeThreat::LEAKYBLOCKADE;}
  }
  else if(b.owners[loc] == opp)
  {
    if(!b.isOpenToStep(loc) && !b.isOpenToMove(loc)) {defMap |= stepPushBlockMap(b,pla,oloc,loc); return BlockadeThreat::FULLBLOCKADE;}
    if(max <= BlockadeThreat::FULLBLOCKADE) return BlockadeThreat::OPENBLOCKADE;

    if(AUTOBLOCKADE[loc] <= BlockadeThreat::LOOSEBLOCKADE) {defMap.setOn(loc); return BlockadeThreat::LOOSEBLOCKADE;}
    if(b.isOpenToStep(loc))
    {
      loc_t openloc = b.findOpenToStep(loc);
      b.tempPP(oloc,loc,openloc);
      Bitmap tempMap;
      bool good =
        (!(CS1(loc) && loc S != oloc && getBlockadeStr(b,pla,loc,loc S,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
         !(CW1(loc) && loc W != oloc && getBlockadeStr(b,pla,loc,loc W,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
         !(CE1(loc) && loc E != oloc && getBlockadeStr(b,pla,loc,loc E,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
         !(CN1(loc) && loc N != oloc && getBlockadeStr(b,pla,loc,loc N,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE));
      b.tempPP(openloc,loc,oloc);
      if(good) {tempMap.setOff(openloc); defMap |= tempMap; return BlockadeThreat::LOOSEBLOCKADE;}
    }
    if(max <= BlockadeThreat::LOOSEBLOCKADE) return BlockadeThreat::OPENBLOCKADE;

    if(AUTOBLOCKADE[loc] <= BlockadeThreat::LEAKYBLOCKADE) {defMap.setOn(loc); return BlockadeThreat::LEAKYBLOCKADE;}
    if(b.isTrap(loc) && !b.isOpenToStep(loc)) {defMap |= stepBlockMap(b,pla,oloc,loc); return BlockadeThreat::LEAKYBLOCKADE;}
  }
  else //if empty
  {
    if(!b.isTrapSafe2(opp,loc)) {defMap.setOn(loc); return BlockadeThreat::FULLBLOCKADE;}
    if(max <= BlockadeThreat::FULLBLOCKADE) return BlockadeThreat::OPENBLOCKADE;

    if(AUTOBLOCKADE[loc] <= BlockadeThreat::LOOSEBLOCKADE) return BlockadeThreat::LOOSEBLOCKADE;
    b.tempStep(oloc,loc);
    Bitmap tempMap;
    bool good =
      (!(CS1(loc) && loc S != oloc && getBlockadeStr(b,pla,loc,loc S,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
       !(CW1(loc) && loc W != oloc && getBlockadeStr(b,pla,loc,loc W,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
       !(CE1(loc) && loc E != oloc && getBlockadeStr(b,pla,loc,loc E,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE) &&
       !(CN1(loc) && loc N != oloc && getBlockadeStr(b,pla,loc,loc N,tempMap,BlockadeThreat::FULLBLOCKADE) > BlockadeThreat::FULLBLOCKADE));
    b.tempStep(loc,oloc);

    if(good) {tempMap.setOff(loc); defMap |= tempMap; return BlockadeThreat::LOOSEBLOCKADE;}
    if(max <= BlockadeThreat::LOOSEBLOCKADE) return BlockadeThreat::OPENBLOCKADE;

    if(AUTOBLOCKADE[loc] <= BlockadeThreat::LEAKYBLOCKADE) return BlockadeThreat::LEAKYBLOCKADE;
    if(b.isTrap(loc) && !b.isTrapSafe3(opp,loc)) {defMap.setOn(loc); return BlockadeThreat::LEAKYBLOCKADE;}
  }
  return BlockadeThreat::OPENBLOCKADE;
}

//Is pla blockading oloc?
static bool isBlockade(Board& b, pla_t pla, loc_t oloc, BlockadeThreat& threat)
{
  if(!b.isFrozen(oloc))
  {
    Bitmap defMap;
    int strsum = 0;
    if(CS1(oloc)) {strsum += getBlockadeStr(b,pla,oloc,oloc S,defMap,BlockadeThreat::OPENBLOCKADE); if(strsum >= 3) return false;}
    if(CW1(oloc)) {strsum += getBlockadeStr(b,pla,oloc,oloc W,defMap,BlockadeThreat::OPENBLOCKADE); if(strsum >= 3) return false;}
    if(CE1(oloc)) {strsum += getBlockadeStr(b,pla,oloc,oloc E,defMap,BlockadeThreat::OPENBLOCKADE); if(strsum >= 3) return false;}
    if(CN1(oloc)) {strsum += getBlockadeStr(b,pla,oloc,oloc N,defMap,BlockadeThreat::OPENBLOCKADE); if(strsum >= 3) return false;}
    threat.holderMap = defMap;
    threat.tightness = strsum;
    threat.pinnedLoc = oloc;
    return true;
  }
  else
  {
    Bitmap map[4] = {Bitmap(),Bitmap(),Bitmap(),Bitmap()};
    int strsum = 0;
    int strmaxIndex = 0;
    int strmax = -1;
    if(CS1(oloc)) {int str = getBlockadeStr(b,pla,oloc,oloc S,map[0],BlockadeThreat::OPENBLOCKADE); strsum += str; if(str > strmax) {strmax = str; strmaxIndex = 0;}}
    if(CW1(oloc)) {int str = getBlockadeStr(b,pla,oloc,oloc W,map[1],BlockadeThreat::OPENBLOCKADE); strsum += str; if(str > strmax) {strmax = str; strmaxIndex = 1;}}
    if(CE1(oloc)) {int str = getBlockadeStr(b,pla,oloc,oloc E,map[2],BlockadeThreat::OPENBLOCKADE); strsum += str; if(str > strmax) {strmax = str; strmaxIndex = 2;}}
    if(CN1(oloc)) {int str = getBlockadeStr(b,pla,oloc,oloc N,map[3],BlockadeThreat::OPENBLOCKADE); strsum += str; if(str > strmax) {strmax = str; strmaxIndex = 3;}}

    strsum -= strmax;
    if(strsum >= 3) {return false;}

    //TODO This is buggy if all are full-blockaded. It could eliminate one necessary to the blockade, like the freezing piece!
    Bitmap defMap;
    for(int i = 0; i<4; i++)
       if(i != strmaxIndex)
         defMap |= map[i];

    threat.holderMap = defMap;
    threat.tightness = strsum;
    threat.pinnedLoc = oloc;
    return true;
  }
}

//Pla blockades opp
int Strats::findBlockades(Board& b, pla_t pla, BlockadeThreat* threats)
{
  int numThreats = 0;
  pla_t opp = gOpp(pla);
  Bitmap eMap = b.pieceMaps[opp][ELE];
  if(eMap.hasBits())
  {
    loc_t loc = eMap.nextBit();
    if(isBlockade(b,pla,loc,threats[numThreats]))
      numThreats++;
  }
  return numThreats;
}











//NEW BLOCKADE--------------------------------------------------------------------------

static const int TOTAL_IMMO = Strats::TOTAL_IMMO;
static const int ALMOST_IMMO = Strats::ALMOST_IMMO;
static const int CENTRAL_BLOCK = Strats::CENTRAL_BLOCK;
static const int CENTRAL_BLOCK_WEAK = Strats::CENTRAL_BLOCK_WEAK;

//Most important location for blockading
static const int CENTRAL_ADJ_1[BSIZE] =
{
 gLoc(0,0) N,gLoc(1,0) N,gLoc(2,0) N,gLoc(3,0) N,gLoc(4,0) N,gLoc(5,0) N,gLoc(6,0) N,gLoc(7,0) N,  0,0,0,0,0,0,0,0,
 gLoc(0,1) N,gLoc(1,1) N,gLoc(2,1) N,gLoc(3,1) N,gLoc(4,1) N,gLoc(5,1) N,gLoc(6,1) N,gLoc(7,1) N,  0,0,0,0,0,0,0,0,
 gLoc(0,2) N,gLoc(1,2) N,gLoc(2,2) N,gLoc(3,2) N,gLoc(4,2) N,gLoc(5,2) N,gLoc(6,2) N,gLoc(7,2) N,  0,0,0,0,0,0,0,0,
 gLoc(0,3) E,gLoc(1,3) E,gLoc(2,3) N,gLoc(3,3) N,gLoc(4,3) N,gLoc(5,3) N,gLoc(6,3) W,gLoc(7,3) W,  0,0,0,0,0,0,0,0,
 gLoc(0,4) E,gLoc(1,4) E,gLoc(2,4) S,gLoc(3,4) S,gLoc(4,4) S,gLoc(5,4) S,gLoc(6,4) W,gLoc(7,4) W,  0,0,0,0,0,0,0,0,
 gLoc(0,5) S,gLoc(1,5) S,gLoc(2,5) S,gLoc(3,5) S,gLoc(4,5) S,gLoc(5,5) S,gLoc(6,5) S,gLoc(7,5) S,  0,0,0,0,0,0,0,0,
 gLoc(0,6) S,gLoc(1,6) S,gLoc(2,6) S,gLoc(3,6) S,gLoc(4,6) S,gLoc(5,6) S,gLoc(6,6) S,gLoc(7,6) S,  0,0,0,0,0,0,0,0,
 gLoc(0,7) S,gLoc(1,7) S,gLoc(2,7) S,gLoc(3,7) S,gLoc(4,7) S,gLoc(5,7) S,gLoc(6,7) S,gLoc(7,7) S,  0,0,0,0,0,0,0,0,
};
//Second important location for blockading
static const int CENTRAL_ADJ_2[BSIZE] =
{
 gLoc(0,0) E,gLoc(1,0) E,gLoc(2,0) E,gLoc(3,0) E,gLoc(4,0) W,gLoc(5,0) W,gLoc(6,0) W,gLoc(7,0) W,  0,0,0,0,0,0,0,0,
 gLoc(0,1) E,gLoc(1,1) E,gLoc(2,1) E,gLoc(3,1) E,gLoc(4,1) W,gLoc(5,1) W,gLoc(6,1) W,gLoc(7,1) W,  0,0,0,0,0,0,0,0,
 gLoc(0,2) E,gLoc(1,2) E,gLoc(2,2) E,gLoc(3,2) E,gLoc(4,2) W,gLoc(5,2) W,gLoc(6,2) W,gLoc(7,2) W,  0,0,0,0,0,0,0,0,
 gLoc(0,3) N,gLoc(1,3) N,gLoc(2,3) E,gLoc(3,3) E,gLoc(4,3) W,gLoc(5,3) W,gLoc(6,3) N,gLoc(7,3) N,  0,0,0,0,0,0,0,0,
 gLoc(0,4) S,gLoc(1,4) S,gLoc(2,4) E,gLoc(3,4) E,gLoc(4,4) W,gLoc(5,4) W,gLoc(6,4) S,gLoc(7,4) S,  0,0,0,0,0,0,0,0,
 gLoc(0,5) E,gLoc(1,5) E,gLoc(2,5) E,gLoc(3,5) E,gLoc(4,5) W,gLoc(5,5) W,gLoc(6,5) W,gLoc(7,5) W,  0,0,0,0,0,0,0,0,
 gLoc(0,6) E,gLoc(1,6) E,gLoc(2,6) E,gLoc(3,6) E,gLoc(4,6) W,gLoc(5,6) W,gLoc(6,6) W,gLoc(7,6) W,  0,0,0,0,0,0,0,0,
 gLoc(0,7) E,gLoc(1,7) E,gLoc(2,7) E,gLoc(3,7) E,gLoc(4,7) W,gLoc(5,7) W,gLoc(6,7) W,gLoc(7,7) W,  0,0,0,0,0,0,0,0,
};

//Third important location for blockading
static const int EDGE_ADJ_1[BSIZE] =
{
 gLoc(0,0)  ,gLoc(1,0) W,gLoc(2,0) W,gLoc(3,0) W,gLoc(4,0) E,gLoc(5,0) E,gLoc(6,0) E,gLoc(7,0)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,1)  ,gLoc(1,1) W,gLoc(2,1) W,gLoc(3,1) W,gLoc(4,1) E,gLoc(5,1) E,gLoc(6,1) E,gLoc(7,1)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,2)  ,gLoc(1,2) W,gLoc(2,2) W,gLoc(3,2) W,gLoc(4,2) E,gLoc(5,2) E,gLoc(6,2) E,gLoc(7,2)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,3)  ,gLoc(1,3) W,gLoc(2,3) W,gLoc(3,3) W,gLoc(4,3) E,gLoc(5,3) E,gLoc(6,3) E,gLoc(7,3)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,4)  ,gLoc(1,4) W,gLoc(2,4) W,gLoc(3,4) W,gLoc(4,4) E,gLoc(5,4) E,gLoc(6,4) E,gLoc(7,4)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,5)  ,gLoc(1,5) W,gLoc(2,5) W,gLoc(3,5) W,gLoc(4,5) E,gLoc(5,5) E,gLoc(6,5) E,gLoc(7,5)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,6)  ,gLoc(1,6) W,gLoc(2,6) W,gLoc(3,6) W,gLoc(4,6) E,gLoc(5,6) E,gLoc(6,6) E,gLoc(7,6)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,7)  ,gLoc(1,7) W,gLoc(2,7) W,gLoc(3,7) W,gLoc(4,7) E,gLoc(5,7) E,gLoc(6,7) E,gLoc(7,7)  ,  0,0,0,0,0,0,0,0,
};

//Least important location for blockading
static const int EDGE_ADJ_2[BSIZE] =
{
 gLoc(0,0)  ,gLoc(1,0)  ,gLoc(2,0)  ,gLoc(3,0)  ,gLoc(4,0)  ,gLoc(5,0)  ,gLoc(6,0)  ,gLoc(7,0)  ,  0,0,0,0,0,0,0,0,
 gLoc(0,1) S,gLoc(1,1) S,gLoc(2,1) S,gLoc(3,1) S,gLoc(4,1) S,gLoc(5,1) S,gLoc(6,1) S,gLoc(7,1) S,  0,0,0,0,0,0,0,0,
 gLoc(0,2) S,gLoc(1,2) S,gLoc(2,2) S,gLoc(3,2) S,gLoc(4,2) S,gLoc(5,2) S,gLoc(6,2) S,gLoc(7,2) S,  0,0,0,0,0,0,0,0,
 gLoc(0,3) S,gLoc(1,3) S,gLoc(2,3) S,gLoc(3,3) S,gLoc(4,3) S,gLoc(5,3) S,gLoc(6,3) S,gLoc(7,3) S,  0,0,0,0,0,0,0,0,
 gLoc(0,4) N,gLoc(1,4) N,gLoc(2,4) N,gLoc(3,4) N,gLoc(4,4) N,gLoc(5,4) N,gLoc(6,4) N,gLoc(7,4) N,  0,0,0,0,0,0,0,0,
 gLoc(0,5) N,gLoc(1,5) N,gLoc(2,5) N,gLoc(3,5) N,gLoc(4,5) N,gLoc(5,5) N,gLoc(6,5) N,gLoc(7,5) N,  0,0,0,0,0,0,0,0,
 gLoc(0,6) N,gLoc(1,6) N,gLoc(2,6) N,gLoc(3,6) N,gLoc(4,6) N,gLoc(5,6) N,gLoc(6,6) N,gLoc(7,6) N,  0,0,0,0,0,0,0,0,
 gLoc(0,7)  ,gLoc(1,7)  ,gLoc(2,7)  ,gLoc(3,7)  ,gLoc(4,7)  ,gLoc(5,7)  ,gLoc(6,7)  ,gLoc(7,7)  ,  0,0,0,0,0,0,0,0,
};

static const int WEAK_CENTRAL_BLOCK_OK[BSIZE] =
{
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  1,1,0,0,0,0,1,1,  0,0,0,0,0,0,0,0,
  1,1,0,0,0,0,1,1,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
};

//Pla blockading opp
//Recursive on each surrounding piece when it encounters an opp piece.
//geqThan - piece must be geq this strength opp piece, or else phalanxed. MUST NOT BE EMP because it's used to set a fake piece on board
//prevloc - previous loc in the recursion
//AllowLoose - if it's okay for loc to be empty or push-in-able, so long it's still blockadey
//WasLoose - whether it was loose or not. MUST BE INITED TO FALSE.
//Recursedmap - All pieces or trap squares recursed on, whether part of the blockade or not. These include unsafe traps and any interior blockers
//HolderHeldMap - All pieces involved, but excludes the case where a pla piece is on a trap if the trap is unsafe for the opp. Might have empty squares, but can filter them out
//FreezeHeldMap - All pieces that need to be physically frozen in order to hold the blockade
static bool isBlockadeRec(Board& b, pla_t pla, piece_t geqThan, loc_t prevloc, loc_t loc,
    Bitmap& allowRecheck, Bitmap& recursedMap, Bitmap& holderHeldMap, Bitmap& freezeHeldMap, bool allowLoose, bool& wasLoose)
{
  //Don't recurse on squares already handled
  //There's one glitch however, where a piece can be found to be strong ehough to block one piece, but there's a
  //SECOND piece in the blockade it's not strong neough to block.
  //So we use "allowRecheck" to track this and allow rechecks in this case.
  if((recursedMap & ~allowRecheck).isOne(loc))
    return true;

  recursedMap.setOn(loc);
  pla_t opp = gOpp(pla);

  //Unsafe trap
  if(b.owners[loc] != opp && !b.isTrapSafe2(opp,loc))
    return true;

  //Opp? Check surrounding squares
  if(b.owners[loc] == opp)
  {
    holderHeldMap.setOn(loc);
    bool singleFailuresOkay = false;
    bool singleOppFailuresOkay = false;
    if(b.isDominatedC(loc))
    {
      if(!b.isGuarded(opp,loc)) singleFailuresOkay = true;
      else if(!b.isGuarded2(opp,loc)) singleOppFailuresOkay = true;
    }

    //If a piece is frozen, then failure of blockage on a single adjacent square doesn't matter.
    //Except for rabbits where the nonrabokay square still counts as a failure of blockage for this purpose.
    //So we special case check this
    if(singleFailuresOkay && b.isRabbit(loc))
    {
      if(!b.isOpenToStep(loc))
      {
        Bitmap map = Board::RADIUS[1][loc] & b.pieceMaps[pla][0];
        if(Board::GOALYDIST[pla][loc] > 0)
          map.setOff(loc + Board::ADVANCE_OFFSET[pla]);
        holderHeldMap |= map;
        return true;
      }

      if(b.isOpen2(loc))
        return false;
      holderHeldMap |= Board::RADIUS[1][loc] & b.pieceMaps[pla][0];
      return true;
    }
    if(singleOppFailuresOkay && b.isRabbit(loc))
    {
      if(b.isOpenToStep(loc))
        return false;
      Bitmap map = Board::RADIUS[1][loc] & b.pieceMaps[pla][0];
      if(Board::GOALYDIST[pla][loc] > 0)
        map.setOff(loc + Board::ADVANCE_OFFSET[pla]);
      holderHeldMap |= map;
      return true;
    }

    piece_t newGeqThan = b.pieces[loc];

    //Single failures not okay
    if(!singleFailuresOkay && !singleOppFailuresOkay)
    {
      loc_t nextloc;
      nextloc = CENTRAL_ADJ_1[loc]; if(nextloc != prevloc                   && b.isRabOkay(opp,loc,nextloc) && !isBlockadeRec(b,pla,newGeqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {return false;}
      nextloc = CENTRAL_ADJ_2[loc]; if(nextloc != prevloc                   && b.isRabOkay(opp,loc,nextloc) && !isBlockadeRec(b,pla,newGeqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {return false;}
      nextloc = EDGE_ADJ_1[loc];    if(nextloc != prevloc && nextloc != loc && b.isRabOkay(opp,loc,nextloc) && !isBlockadeRec(b,pla,newGeqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {return false;}
      nextloc = EDGE_ADJ_2[loc];    if(nextloc != prevloc && nextloc != loc && b.isRabOkay(opp,loc,nextloc) && !isBlockadeRec(b,pla,newGeqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {return false;}
      return true;

    }
    //Single failures okay
    else
    {
      //Create temporaries so that we dont add fake holders to isBlockadeRecs that return false
      Bitmap allowRecheckTemp = allowRecheck;
      Bitmap recursedMapTemp = recursedMap;
      Bitmap holderHeldMapTemp = holderHeldMap;
      Bitmap freezeHeldMapTemp = freezeHeldMap;
      bool hasFailed = false;
      //Reverse order so that we preferentially assign the allowed failures to the central squares
      loc_t locs[4] = {EDGE_ADJ_2[loc], EDGE_ADJ_1[loc], CENTRAL_ADJ_2[loc], CENTRAL_ADJ_1[loc], };
      for(int i = 0; i<4; i++)
      {
        loc_t nextloc = locs[i];
        if(nextloc == prevloc || nextloc == loc || !b.isRabOkay(opp,loc,nextloc))
          continue;
        if(i == 3 && !hasFailed && (singleFailuresOkay || b.owners[nextloc] == opp))
        {hasFailed = true;}
        else if(b.owners[nextloc] == NPLA)
        {if(!hasFailed && singleFailuresOkay) {hasFailed = true;} else return false;}
        else if(isBlockadeRec(b,pla,newGeqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose))
          {allowRecheckTemp = allowRecheck; recursedMapTemp = recursedMap; holderHeldMapTemp = holderHeldMap; freezeHeldMapTemp = freezeHeldMap;}
        else if(!hasFailed && (singleFailuresOkay || b.owners[nextloc] == opp)) {hasFailed = true; allowRecheck = allowRecheckTemp; recursedMap = recursedMapTemp; holderHeldMap = holderHeldMapTemp; freezeHeldMap = freezeHeldMapTemp;}
        else return false;
      }
      return true;
    }
  }
  //Empty? Allow only if allowing loose, and make sure surrounding squares are tight.
  else if(b.owners[loc] == NPLA)
  {
    if(!allowLoose)
      return false;

    //Temporarily fill the square with an equally strong piece, and see if the blockade still works.
    b.owners[loc] = opp;
    b.pieces[loc] = geqThan;
    wasLoose = true;
    loc_t nextloc;
    nextloc = CENTRAL_ADJ_1[loc]; if(nextloc != prevloc                   && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {b.owners[loc] = NPLA; b.pieces[loc] = EMP; return false;}
    nextloc = CENTRAL_ADJ_2[loc]; if(nextloc != prevloc                   && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {b.owners[loc] = NPLA; b.pieces[loc] = EMP; return false;}
    nextloc = EDGE_ADJ_1[loc];    if(nextloc != prevloc && nextloc != loc && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {b.owners[loc] = NPLA; b.pieces[loc] = EMP; return false;}
    nextloc = EDGE_ADJ_2[loc];    if(nextloc != prevloc && nextloc != loc && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) {b.owners[loc] = NPLA; b.pieces[loc] = EMP; return false;}
    b.owners[loc] = NPLA; b.pieces[loc] = EMP;
    return true;
  }
  //Pla
  else
  {
    holderHeldMap.setOn(loc);
    //Unpushable?
    if(b.pieces[loc] >= geqThan)
    {return true;}

    //We cannot infinite loop because of the rechecking allowed by this map. Because allowRecheck is set ONLY on pla pieces initially,
    //and in any case where we allowed a recheck, the check either must terminate on the above "unpushable" line without recursing, or it
    //must remove another additional bit from this map.
    allowRecheck.setOff(loc);

    int numOpen = b.countOpen(loc);
    if(numOpen > 0)
    {
      if(!allowLoose || numOpen >= 2)
        return false;

      loc_t openSq = b.findOpen(loc);
      //Pretend we've pushed and insert a new piece to determine blockedness.
      b.owners[openSq] = b.owners[loc];
      b.pieces[openSq] = b.pieces[loc];
      b.owners[loc] = opp;
      b.pieces[loc] = geqThan;
      wasLoose = true;

      bool isBlock = true;
      holderHeldMap.setOn(loc);
      loc_t nextloc;
      if(isBlock) {nextloc = CENTRAL_ADJ_1[loc]; if(nextloc != prevloc                   && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) isBlock = false;}
      if(isBlock) {nextloc = CENTRAL_ADJ_2[loc]; if(nextloc != prevloc                   && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) isBlock = false;}
      if(isBlock) {nextloc = EDGE_ADJ_1[loc];    if(nextloc != prevloc && nextloc != loc && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) isBlock = false;}
      if(isBlock) {nextloc = EDGE_ADJ_2[loc];    if(nextloc != prevloc && nextloc != loc && !isBlockadeRec(b,pla,geqThan,loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose)) isBlock = false;}

      b.owners[loc] = b.owners[openSq]; b.pieces[loc] = b.pieces[openSq];
      b.owners[openSq] = NPLA; b.pieces[openSq] = EMP;
      return isBlock;
    }

    //If there's a pla piece on the trap, then if the opponent has not enough defenders, it's still blockaed.
    //We don't have to check pla defenders, because opptrapunsafe3 && blocked ==> pla trapsafe2.
    if(!b.isTrapSafe3(opp,loc))
    {
      holderHeldMap |= (Board::RADIUS[1][loc] & b.pieceMaps[pla][0]);
      return true;
    }

    //Otherwise, check for a phalanx
    loc_t locs[4] = {CENTRAL_ADJ_1[loc], CENTRAL_ADJ_2[loc], EDGE_ADJ_1[loc], EDGE_ADJ_2[loc]};
    for(int i = 0; i<4; i++)
    {
      loc_t nextloc = locs[i];
      if(nextloc == prevloc || nextloc == loc)
        continue;
      //if(b.owners[nextloc] == NPLA) return false; //Don't need to check because we checked b.isOpen above
      if(b.owners[nextloc] == opp)
      {
        Bitmap allowRecheckTemp = allowRecheck;
        Bitmap recursedMapTemp = recursedMap;
        Bitmap holderHeldMapTemp = holderHeldMap;
        Bitmap freezeHeldMapTemp = freezeHeldMap;
        if(isBlockadeRec(b,pla,b.pieces[nextloc],loc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,false,wasLoose))
          holderHeldMap.setOn(nextloc);
        else
        {
          allowRecheck = allowRecheckTemp; recursedMap = recursedMapTemp; holderHeldMap = holderHeldMapTemp; freezeHeldMap = freezeHeldMapTemp;
          if(!wasLoose && b.isFrozenC(nextloc)) {holderHeldMap.setOn(nextloc); freezeHeldMap.setOn(nextloc);}
          return false;
        }
      }
      else //pla
        holderHeldMap.setOn(nextloc);
    }
    return true;
  }
}

//TODO Use restrict keyword?

//Pla is holder
static bool isEleBlockade(Board& b, pla_t pla, loc_t oppEleLoc,
    Bitmap& recursedMap, Bitmap& holderHeldMap, Bitmap& freezeHeldMap, int& immoType)
{
  Bitmap allowRecheck = b.pieceMaps[pla][0];
  recursedMap.setOn(oppEleLoc);
  holderHeldMap.setOn(oppEleLoc);
  bool isEverLoose = false;
  bool wasLoose;

  immoType = TOTAL_IMMO;

  loc_t nextloc = CENTRAL_ADJ_1[oppEleLoc];
  wasLoose = false;
  if(!isBlockadeRec(b,pla,ELE,oppEleLoc,nextloc,allowRecheck,recursedMap,holderHeldMap,freezeHeldMap,true,wasLoose)) return false;
  isEverLoose |= wasLoose;

  //Create temporaries so that we dont add fake holders to isBlockadeRecs that return false
  Bitmap allowRecheckTemp = allowRecheck;
  Bitmap recursedMapTemp = recursedMap;
  Bitmap holderHeldMapTemp = holderHeldMap;
  Bitmap freezeHeldMapTemp = freezeHeldMap;

  nextloc = CENTRAL_ADJ_2[oppEleLoc];
  wasLoose = false;
  if(!isBlockadeRec(b,pla,ELE,oppEleLoc,nextloc,allowRecheckTemp,recursedMapTemp,holderHeldMapTemp,freezeHeldMapTemp,true,wasLoose))
  {
    allowRecheckTemp = allowRecheck; recursedMapTemp = recursedMap; holderHeldMapTemp = holderHeldMap; freezeHeldMapTemp = freezeHeldMap; //Restore
    immoType = CENTRAL_BLOCK;
  }
  else
  {allowRecheck = allowRecheckTemp; recursedMap = recursedMapTemp; holderHeldMap = holderHeldMapTemp; freezeHeldMap = freezeHeldMapTemp; isEverLoose |= wasLoose;} //Save

  nextloc = EDGE_ADJ_1[oppEleLoc];
  wasLoose = false;
  if(nextloc != oppEleLoc)
  {
    if(!isBlockadeRec(b,pla,ELE,oppEleLoc,nextloc,allowRecheckTemp,recursedMapTemp,holderHeldMapTemp,freezeHeldMapTemp,true,wasLoose))
    {
      //Both left and right were unblocked, so we can't have any sort of immo.
      if(immoType == CENTRAL_BLOCK)
      {
        //Actually, let's still catch a weak block-ish situation
        //At least one of these surrounding locs is blocked, and also only in certain locations
        if(WEAK_CENTRAL_BLOCK_OK[oppEleLoc] != 0 &&
            ((b.owners[CENTRAL_ADJ_2[oppEleLoc]] != NPLA && b.isBlocked(CENTRAL_ADJ_2[oppEleLoc])) ||
             (b.owners[nextloc] != NPLA && b.isBlocked(nextloc))))
        {
          immoType = CENTRAL_BLOCK_WEAK;
          return true;
        }
        //allowRecheckTemp = allowRecheck; recursedMapTemp = recursedMap; holderHeldMapTemp = holderHeldMap; freezeHeldMapTemp = freezeHeldMap; //Restore
        return false;
      }
      immoType = CENTRAL_BLOCK;
      return true;
    }
    else
    {allowRecheck = allowRecheckTemp; recursedMap = recursedMapTemp; holderHeldMap = holderHeldMapTemp; freezeHeldMap = freezeHeldMapTemp;  isEverLoose |= wasLoose;} //Save
  }

  if(immoType == CENTRAL_BLOCK)
    return true;

  nextloc = EDGE_ADJ_2[oppEleLoc];
  wasLoose = false;
  if(nextloc != oppEleLoc)
  {
    if(!isBlockadeRec(b,pla,ELE,oppEleLoc,nextloc,allowRecheckTemp,recursedMapTemp,holderHeldMapTemp,freezeHeldMapTemp,true,wasLoose))
    {
      //allowRecheckTemp = allowRecheck; recursedMapTemp = recursedMap; holderHeldMapTemp = holderHeldMap; freezeHeldMapTemp = freezeHeldMap; //Restore
      immoType = ALMOST_IMMO;
      return true;
    }
    else
    {/*allowRecheck = allowRecheckTemp;*/ recursedMap = recursedMapTemp; holderHeldMap = holderHeldMapTemp; freezeHeldMap = freezeHeldMapTemp; isEverLoose |= wasLoose;} //Save
  }

  if(immoType == TOTAL_IMMO && isEverLoose)
    immoType = ALMOST_IMMO;
  return true;
}

loc_t Strats::findEBlockade(Board& b, pla_t pla, int& immoType, Bitmap& recursedMap, Bitmap& holderHeldMap, Bitmap& freezeHeldMap)
{
  pla_t opp = gOpp(pla);
  Bitmap eMap = b.pieceMaps[opp][ELE];
  if(eMap.hasBits())
  {
    loc_t loc = eMap.nextBit();
    if(isEleBlockade(b, pla, loc, recursedMap, holderHeldMap, freezeHeldMap, immoType))
    {
      Bitmap anyMap = b.pieceMaps[SILV][0] | b.pieceMaps[GOLD][0];
      holderHeldMap &= anyMap; //Remove empty squares
      return loc;
    }
  }
  return ERRLOC;
}



















