
/*
 * boardmovegen.cpp
 * Author: davidwu
 *
 * Standard move generation functions
 */

#include <cmath>
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/offsets.h"
#include "../board/boardtreeconst.h"
#include "../board/boardmovegen.h"

using namespace std;

//MANHATTAN-CHAIN COMBO GENERATION--------------------------------------------

int BoardMoveGen::genLocalComboMoves(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  return genLocalComboMoves(b,pla,numSteps,mv,ERRMOVE,0,Bitmap::BMPZEROS);
}

int BoardMoveGen::genLocalComboMoves(Board& b, pla_t pla, int numSteps, move_t* mv, move_t prefix, int prefixLen, Bitmap relevantArea)
{
  if(numSteps <= 0)
    return 0;

  Bitmap rel = relevantArea;
  if(rel == Bitmap::BMPZEROS)
    rel = Bitmap::BMPONES;

  //Generate all steps and pushpulls
  int numMoves = 0;
  move_t moves[256];
  if(numSteps > 1)
    numMoves += genPushPullsInvolving(b,pla,moves+numMoves,rel);
  numMoves += genStepsInvolving(b,pla,moves+numMoves,rel);

  //Attach prefix and put them into passed in list
  int numMV = numMoves;
  for(int i = 0; i<numMoves; i++)
  {
    mv[i] = concatMoves(prefix,moves[i],prefixLen);
  }
  //No recursion needed?
  if(numSteps <= 1)
    return numMV;

  //Otherwise recurse down all possible combinations
  UndoMoveData uData;
  Bitmap oldP0Map = b.pieceMaps[0][0];
  Bitmap oldP1Map = b.pieceMaps[1][0];
  for(int i = 0; i<numMoves; i++)
  {
    int ns = numStepsInMove(moves[i]);
    if(ns < numSteps)
    {
      b.makeMove(moves[i],uData);
      Bitmap affected = (b.pieceMaps[0][0] ^ oldP0Map) | (b.pieceMaps[1][0] ^ oldP1Map);
      affected |= Bitmap::adj(affected);
      affected |= Bitmap::adj(affected & Bitmap::BMPTRAPS);
      numMV += genLocalComboMoves(b,pla,numSteps-ns,mv+numMV,mv[i],prefixLen+ns,affected);
      b.undoMove(uData);
    }
  }
  return numMV;
}

//SIMPLE-CHAIN COMBO GENERATION-----------------------------------------------

int BoardMoveGen::genSimpleChainMoves(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  return genSimpleChainMoves(b,pla,numSteps,mv,Bitmap::BMPONES);
}

int BoardMoveGen::genSimpleChainMoves(Board& b, pla_t pla, int numSteps, move_t* mv, Bitmap relPlaPieces)
{
  int num = 0;
  Bitmap plaMap = b.pieceMaps[pla][0] & (~b.frozenMap) & relPlaPieces;
  while(plaMap.hasBits())
    num += genSimpleChainMoves(b,pla,plaMap.nextBit(),numSteps,mv+num,ERRMOVE,0,ERRLOC);
  return num;
}

int BoardMoveGen::genSimpleChainMoves(Board& b, pla_t pla, loc_t ploc, int numSteps, move_t* mv, move_t prefix, int prefixLen, loc_t prohibitedLoc)
{
  if(numSteps <= 0 || b.owners[ploc] != pla || b.isFrozenC(ploc))
    return 0;

  pla_t opp = gOpp(pla);
  int num = 0;
  if(numSteps >= 1)
  {
    if(ISE(ploc S) && ploc S != prohibitedLoc && b.isRabOkayS(pla,ploc)) {mv[num++] = concatMoves(prefix,getMove(ploc MS),prefixLen);}
    if(ISE(ploc W) && ploc W != prohibitedLoc)                           {mv[num++] = concatMoves(prefix,getMove(ploc MW),prefixLen);}
    if(ISE(ploc E) && ploc E != prohibitedLoc)                           {mv[num++] = concatMoves(prefix,getMove(ploc ME),prefixLen);}
    if(ISE(ploc N) && ploc N != prohibitedLoc && b.isRabOkayN(pla,ploc)) {mv[num++] = concatMoves(prefix,getMove(ploc MN),prefixLen);}
  }
  if(numSteps >= 2 && b.pieces[ploc] > RAB)
  {
    if(ISO(ploc S) && GT(ploc, ploc S))
    {
      if(ISE(ploc W))  {mv[num++] = concatMoves(prefix,getMove(ploc MW, ploc S MN),prefixLen);}
      if(ISE(ploc E))  {mv[num++] = concatMoves(prefix,getMove(ploc ME, ploc S MN),prefixLen);}
      if(ISE(ploc N))  {mv[num++] = concatMoves(prefix,getMove(ploc MN, ploc S MN),prefixLen);}
      if(ISE(ploc SS)) {mv[num++] = concatMoves(prefix,getMove(ploc S MS, ploc MS),prefixLen);}
      if(ISE(ploc SW)) {mv[num++] = concatMoves(prefix,getMove(ploc S MW, ploc MS),prefixLen);}
      if(ISE(ploc SE)) {mv[num++] = concatMoves(prefix,getMove(ploc S ME, ploc MS),prefixLen);}
    }
    if(ISO(ploc W) && GT(ploc, ploc W))
    {
      if(ISE(ploc S)) {mv[num++] = concatMoves(prefix,getMove(ploc MS, ploc W ME),prefixLen);}
      if(ISE(ploc E)) {mv[num++] = concatMoves(prefix,getMove(ploc ME, ploc W ME),prefixLen);}
      if(ISE(ploc N)) {mv[num++] = concatMoves(prefix,getMove(ploc MN, ploc W ME),prefixLen);}
      if(ISE(ploc SW)) {mv[num++] = concatMoves(prefix,getMove(ploc W MS, ploc MW),prefixLen);}
      if(ISE(ploc WW)) {mv[num++] = concatMoves(prefix,getMove(ploc W MW, ploc MW),prefixLen);}
      if(ISE(ploc NW)) {mv[num++] = concatMoves(prefix,getMove(ploc W MN, ploc MW),prefixLen);}
    }
    if(ISO(ploc E) && GT(ploc, ploc E))
    {
      if(ISE(ploc S)) {mv[num++] = concatMoves(prefix,getMove(ploc MS, ploc E MW),prefixLen);}
      if(ISE(ploc W)) {mv[num++] = concatMoves(prefix,getMove(ploc MW, ploc E MW),prefixLen);}
      if(ISE(ploc N)) {mv[num++] = concatMoves(prefix,getMove(ploc MN, ploc E MW),prefixLen);}
      if(ISE(ploc SE)) {mv[num++] = concatMoves(prefix,getMove(ploc E MS, ploc ME),prefixLen);}
      if(ISE(ploc EE)) {mv[num++] = concatMoves(prefix,getMove(ploc E ME, ploc ME),prefixLen);}
      if(ISE(ploc NE)) {mv[num++] = concatMoves(prefix,getMove(ploc E MN, ploc ME),prefixLen);}
    }
    if(ISO(ploc N) && GT(ploc, ploc N))
    {
      if(ISE(ploc S)) {mv[num++] = concatMoves(prefix,getMove(ploc MS, ploc N MS),prefixLen);}
      if(ISE(ploc W)) {mv[num++] = concatMoves(prefix,getMove(ploc MW, ploc N MS),prefixLen);}
      if(ISE(ploc E)) {mv[num++] = concatMoves(prefix,getMove(ploc ME, ploc N MS),prefixLen);}
      if(ISE(ploc NW)) {mv[num++] = concatMoves(prefix,getMove(ploc N MW, ploc MN),prefixLen);}
      if(ISE(ploc NE)) {mv[num++] = concatMoves(prefix,getMove(ploc N ME, ploc MN),prefixLen);}
      if(ISE(ploc NN)) {mv[num++] = concatMoves(prefix,getMove(ploc N MN, ploc MN),prefixLen);}
    }
  }

  if(numSteps >= 2)
  {
    loc_t newProhibLoc = (Board::ADJACENTTRAP[ploc] != ERRLOC && b.trapGuardCounts[pla][Board::TRAPINDEX[Board::ADJACENTTRAP[ploc]]] <= 1) ? ERRLOC : ploc;
    if(ISE(ploc S) && ploc S != prohibitedLoc && b.isRabOkayS(pla,ploc)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MS),prefixLen); TSC(ploc,ploc S, num += genSimpleChainMoves(b,pla,ploc S,numSteps-1,mv+num,newPrefix,prefixLen+1,newProhibLoc))}
    if(ISE(ploc W) && ploc W != prohibitedLoc)                           {move_t newPrefix = concatMoves(prefix,getMove(ploc MW),prefixLen); TSC(ploc,ploc W, num += genSimpleChainMoves(b,pla,ploc W,numSteps-1,mv+num,newPrefix,prefixLen+1,newProhibLoc))}
    if(ISE(ploc E) && ploc E != prohibitedLoc)                           {move_t newPrefix = concatMoves(prefix,getMove(ploc ME),prefixLen); TSC(ploc,ploc E, num += genSimpleChainMoves(b,pla,ploc E,numSteps-1,mv+num,newPrefix,prefixLen+1,newProhibLoc))}
    if(ISE(ploc N) && ploc N != prohibitedLoc && b.isRabOkayN(pla,ploc)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MN),prefixLen); TSC(ploc,ploc N, num += genSimpleChainMoves(b,pla,ploc N,numSteps-1,mv+num,newPrefix,prefixLen+1,newProhibLoc))}
  }

  if(numSteps >= 3 && b.pieces[ploc] > RAB)
  {
    if(ISO(ploc S) && GT(ploc, ploc S))
    {
      if(ISE(ploc W))  {move_t newPrefix = concatMoves(prefix,getMove(ploc MW, ploc S MN),prefixLen); TPC(ploc S,ploc,ploc W, num += genSimpleChainMoves(b,pla,ploc W,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc E))  {move_t newPrefix = concatMoves(prefix,getMove(ploc ME, ploc S MN),prefixLen); TPC(ploc S,ploc,ploc E, num += genSimpleChainMoves(b,pla,ploc E,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc N))  {move_t newPrefix = concatMoves(prefix,getMove(ploc MN, ploc S MN),prefixLen); TPC(ploc S,ploc,ploc N, num += genSimpleChainMoves(b,pla,ploc N,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc SS)) {move_t newPrefix = concatMoves(prefix,getMove(ploc S MS, ploc MS),prefixLen); TPC(ploc,ploc S,ploc SS, num += genSimpleChainMoves(b,pla,ploc S,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc SW)) {move_t newPrefix = concatMoves(prefix,getMove(ploc S MW, ploc MS),prefixLen); TPC(ploc,ploc S,ploc SW, num += genSimpleChainMoves(b,pla,ploc S,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc SE)) {move_t newPrefix = concatMoves(prefix,getMove(ploc S ME, ploc MS),prefixLen); TPC(ploc,ploc S,ploc SE, num += genSimpleChainMoves(b,pla,ploc S,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
    }
    if(ISO(ploc W) && GT(ploc, ploc W))
    {
      if(ISE(ploc S)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MS, ploc W ME),prefixLen); TPC(ploc W,ploc,ploc S, num += genSimpleChainMoves(b,pla,ploc S,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc E)) {move_t newPrefix = concatMoves(prefix,getMove(ploc ME, ploc W ME),prefixLen); TPC(ploc W,ploc,ploc E, num += genSimpleChainMoves(b,pla,ploc E,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc N)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MN, ploc W ME),prefixLen); TPC(ploc W,ploc,ploc N, num += genSimpleChainMoves(b,pla,ploc N,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc SW)) {move_t newPrefix = concatMoves(prefix,getMove(ploc W MS, ploc MW),prefixLen); TPC(ploc,ploc W,ploc SW, num += genSimpleChainMoves(b,pla,ploc W,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc WW)) {move_t newPrefix = concatMoves(prefix,getMove(ploc W MW, ploc MW),prefixLen); TPC(ploc,ploc W,ploc WW, num += genSimpleChainMoves(b,pla,ploc W,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc NW)) {move_t newPrefix = concatMoves(prefix,getMove(ploc W MN, ploc MW),prefixLen); TPC(ploc,ploc W,ploc NW, num += genSimpleChainMoves(b,pla,ploc W,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
    }
    if(ISO(ploc E) && GT(ploc, ploc E))
    {
      if(ISE(ploc S)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MS, ploc E MW),prefixLen); TPC(ploc E,ploc,ploc S, num += genSimpleChainMoves(b,pla,ploc S,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc W)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MW, ploc E MW),prefixLen); TPC(ploc E,ploc,ploc W, num += genSimpleChainMoves(b,pla,ploc W,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc N)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MN, ploc E MW),prefixLen); TPC(ploc E,ploc,ploc N, num += genSimpleChainMoves(b,pla,ploc N,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc SE)) {move_t newPrefix = concatMoves(prefix,getMove(ploc E MS, ploc ME),prefixLen); TPC(ploc,ploc E,ploc SE, num += genSimpleChainMoves(b,pla,ploc E,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc EE)) {move_t newPrefix = concatMoves(prefix,getMove(ploc E ME, ploc ME),prefixLen); TPC(ploc,ploc E,ploc EE, num += genSimpleChainMoves(b,pla,ploc E,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc NE)) {move_t newPrefix = concatMoves(prefix,getMove(ploc E MN, ploc ME),prefixLen); TPC(ploc,ploc E,ploc NE, num += genSimpleChainMoves(b,pla,ploc E,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
    }
    if(ISO(ploc N) && GT(ploc, ploc N))
    {
      if(ISE(ploc S)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MS, ploc N MS),prefixLen); TPC(ploc N,ploc,ploc S, num += genSimpleChainMoves(b,pla,ploc S,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc W)) {move_t newPrefix = concatMoves(prefix,getMove(ploc MW, ploc N MS),prefixLen); TPC(ploc N,ploc,ploc W, num += genSimpleChainMoves(b,pla,ploc W,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc E)) {move_t newPrefix = concatMoves(prefix,getMove(ploc ME, ploc N MS),prefixLen); TPC(ploc N,ploc,ploc E, num += genSimpleChainMoves(b,pla,ploc E,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc NW)) {move_t newPrefix = concatMoves(prefix,getMove(ploc N MW, ploc MN),prefixLen); TPC(ploc,ploc N,ploc NW, num += genSimpleChainMoves(b,pla,ploc N,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc NE)) {move_t newPrefix = concatMoves(prefix,getMove(ploc N ME, ploc MN),prefixLen); TPC(ploc,ploc N,ploc NE, num += genSimpleChainMoves(b,pla,ploc N,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
      if(ISE(ploc NN)) {move_t newPrefix = concatMoves(prefix,getMove(ploc N MN, ploc MN),prefixLen); TPC(ploc,ploc N,ploc NN, num += genSimpleChainMoves(b,pla,ploc N,numSteps-2,mv+num,newPrefix,prefixLen+2,ERRLOC))}
    }
  }
  return num;
}

//MAIN GENERATION-------------------------------------------------------------

int BoardMoveGen::genPushPulls(const Board& b, pla_t pla, move_t* mv)
{
  pla_t opp = gOpp(pla);

  //Bitmap of stuff adjacent to empty space
  Bitmap isOpenMap = Bitmap::adj(~b.pieceMaps[pla][0] & ~b.pieceMaps[opp][0]);
  //Unfrozen nonrabbit pieces, which might be able to push or pull
  Bitmap potentialPushpullers = b.pieceMaps[pla][0] & ~b.frozenMap & ~b.pieceMaps[pla][RAB];
  //Dominated opps, which maybe can be pushed or pulled
  Bitmap dominatedOpps = b.pieceMaps[opp][0] & b.dominatedMap;

  //Pushable things are - opps that are dominated with empty space around and a potential pushpuller around
  Bitmap pushMap = Bitmap::adj(potentialPushpullers) & dominatedOpps & isOpenMap;
  //Pull-capable pieces are potential pushpullers with a dominated opp and empty space around
  Bitmap pullMap = potentialPushpullers & Bitmap::adj(dominatedOpps) & isOpenMap;

  int num = 0;
  while(pushMap.hasBits())
  {
    loc_t k = pushMap.nextBit();
    if(ISP(k S) && GT(k S,k) && b.isThawed(k S))
    {
      if(ISE(k W)){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISP(k W) && GT(k W,k) && b.isThawed(k W))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISP(k E) && GT(k E,k) && b.isThawed(k E))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W)){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISP(k N) && GT(k N,k) && b.isThawed(k N))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W)){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  while(pullMap.hasBits())
  {
    loc_t k = pullMap.nextBit();
    if(ISO(k S) && GT(k,k S))
    {
      if(ISE(k W)){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISO(k W) && GT(k,k W))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISO(k E) && GT(k,k E))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W)){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISO(k N) && GT(k,k N))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W)){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  return num;
}

bool BoardMoveGen::canPushPulls(const Board& b, pla_t pla)
{
  pla_t opp = gOpp(pla);

  //Bitmap of stuff adjacent to empty space
  Bitmap isOpenMap = Bitmap::adj(~b.pieceMaps[pla][0] & ~b.pieceMaps[opp][0]);
  //Unfrozen nonrabbit pieces, which might be able to push or pull
  Bitmap potentialPushpullers = b.pieceMaps[pla][0] & ~b.frozenMap & ~b.pieceMaps[pla][RAB];
  //Dominated opps, which maybe can be pushed or pulled
  Bitmap dominatedOpps = b.pieceMaps[opp][0] & b.dominatedMap;

  //Pushable things are - opps that are dominated with empty space around and a potential pushpuller around
  Bitmap pushMap = Bitmap::adj(potentialPushpullers) & dominatedOpps & isOpenMap;
  //Pull-capable pieces are potential pushpullers with a dominated opp and empty space around
  Bitmap pullMap = potentialPushpullers & Bitmap::adj(dominatedOpps) & isOpenMap;

  while(pushMap.hasBits())
  {
    loc_t k = pushMap.nextBit();
    //We know that around the opp isOpen, so a push must be possible
    if(ISP(k S) && GT(k S,k) && b.isThawed(k S)) return true;
    if(ISP(k W) && GT(k W,k) && b.isThawed(k W)) return true;
    if(ISP(k E) && GT(k E,k) && b.isThawed(k E)) return true;
    if(ISP(k N) && GT(k N,k) && b.isThawed(k N)) return true;
  }
  while(pullMap.hasBits())
  {
    loc_t k = pullMap.nextBit();
    //We know around the pla isOpen, so a pull must be possible
    if(ISO(k S) && GT(k,k S)) return true;
    if(ISO(k W) && GT(k,k W)) return true;
    if(ISO(k E) && GT(k,k E)) return true;
    if(ISO(k N) && GT(k,k N)) return true;
  }
  return false;
}

int BoardMoveGen::genSteps(const Board& b, pla_t pla, move_t* mv)
{
  return genStepsInto(b,pla,mv,Bitmap::BMPONES);
}

bool BoardMoveGen::canSteps(const Board& b, pla_t pla)
{
  pla_t opp = gOpp(pla);
  Bitmap pieceMap = b.pieceMaps[pla][0];
  Bitmap emptyMap = ~(b.pieceMaps[opp][0] | pieceMap);
  Bitmap plaRabMap = b.pieceMaps[pla][RAB];
  pieceMap &= ~b.frozenMap;

  if(pla == GOLD)
  {
    if((Bitmap::shiftN(pieceMap) & emptyMap).hasBits()) {return true;}
    if((Bitmap::shiftW(pieceMap) & emptyMap).hasBits()) {return true;}
    if((Bitmap::shiftE(pieceMap) & emptyMap).hasBits()) {return true;}
    if((Bitmap::shiftS(pieceMap & ~plaRabMap) & emptyMap).hasBits()) {return true;}
  }
  else //pla == SILV
  {
    if((Bitmap::shiftS(pieceMap) & emptyMap).hasBits()) {return true;}
    if((Bitmap::shiftW(pieceMap) & emptyMap).hasBits()) {return true;}
    if((Bitmap::shiftE(pieceMap) & emptyMap).hasBits()) {return true;}
    if((Bitmap::shiftN(pieceMap & ~plaRabMap) & emptyMap).hasBits()) {return true;}
  }
  return false;
}

bool BoardMoveGen::noMoves(const Board& b, pla_t pla)
{
  return !canSteps(b,pla) && !canPushPulls(b,pla);
}

int BoardMoveGen::genPushPullsInvolving(const Board& b, pla_t pla, move_t* mv, Bitmap relArea)
{
  pla_t opp = gOpp(pla);

  //Bitmap of stuff adjacent to empty space
  Bitmap isOpenMap = Bitmap::adj(~b.pieceMaps[pla][0] & ~b.pieceMaps[opp][0]);
  //Unfrozen nonrabbit pieces, which might be able to push or pull
  Bitmap potentialPushpullers = b.pieceMaps[pla][0] & ~b.frozenMap & ~b.pieceMaps[pla][RAB];
  //Dominated opps, which maybe can be pushed or pulled
  Bitmap dominatedOpps = b.pieceMaps[opp][0] & b.dominatedMap;
  //Adjacent to relevant area
  Bitmap adjRelArea = relArea | Bitmap::adj(relArea);

  //Pushable things are - opps that are dominated with empty space around and a potential pushpuller around
  Bitmap pushMap = Bitmap::adj(potentialPushpullers) & dominatedOpps & isOpenMap & adjRelArea;
  //Pull-capable pieces are potential pushpullers with a dominated opp and empty space around
  Bitmap pullMap = potentialPushpullers & Bitmap::adj(dominatedOpps) & isOpenMap & adjRelArea;

  int num = 0;
  while(pushMap.hasBits())
  {
    loc_t k = pushMap.nextBit();
    if(ISP(k S) && GT(k S,k) && b.isThawed(k S))
    {
      if(ISE(k W) && (relArea.isOne(k) || relArea.isOne(k S) || relArea.isOne(k W))){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E) && (relArea.isOne(k) || relArea.isOne(k S) || relArea.isOne(k E))){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N) && (relArea.isOne(k) || relArea.isOne(k S) || relArea.isOne(k N))){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISP(k W) && GT(k W,k) && b.isThawed(k W))
    {
      if(ISE(k S) && (relArea.isOne(k) || relArea.isOne(k W) || relArea.isOne(k S))){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E) && (relArea.isOne(k) || relArea.isOne(k W) || relArea.isOne(k E))){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N) && (relArea.isOne(k) || relArea.isOne(k W) || relArea.isOne(k N))){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISP(k E) && GT(k E,k) && b.isThawed(k E))
    {
      if(ISE(k S) && (relArea.isOne(k) || relArea.isOne(k E) || relArea.isOne(k S))){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W) && (relArea.isOne(k) || relArea.isOne(k E) || relArea.isOne(k W))){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N) && (relArea.isOne(k) || relArea.isOne(k E) || relArea.isOne(k N))){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISP(k N) && GT(k N,k) && b.isThawed(k N))
    {
      if(ISE(k S) && (relArea.isOne(k) || relArea.isOne(k N) || relArea.isOne(k S))){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W) && (relArea.isOne(k) || relArea.isOne(k N) || relArea.isOne(k W))){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E) && (relArea.isOne(k) || relArea.isOne(k N) || relArea.isOne(k E))){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  while(pullMap.hasBits())
  {
    loc_t k = pullMap.nextBit();
    if(ISO(k S) && GT(k,k S))
    {
      if(ISE(k W) && (relArea.isOne(k) || relArea.isOne(k S) || relArea.isOne(k W))){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E) && (relArea.isOne(k) || relArea.isOne(k S) || relArea.isOne(k E))){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N) && (relArea.isOne(k) || relArea.isOne(k S) || relArea.isOne(k N))){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISO(k W) && GT(k,k W))
    {
      if(ISE(k S) && (relArea.isOne(k) || relArea.isOne(k W) || relArea.isOne(k S))){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E) && (relArea.isOne(k) || relArea.isOne(k W) || relArea.isOne(k E))){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N) && (relArea.isOne(k) || relArea.isOne(k W) || relArea.isOne(k N))){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISO(k E) && GT(k,k E))
    {
      if(ISE(k S) && (relArea.isOne(k) || relArea.isOne(k E) || relArea.isOne(k S))){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W) && (relArea.isOne(k) || relArea.isOne(k E) || relArea.isOne(k W))){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N) && (relArea.isOne(k) || relArea.isOne(k E) || relArea.isOne(k N))){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISO(k N) && GT(k,k N))
    {
      if(ISE(k S) && (relArea.isOne(k) || relArea.isOne(k N) || relArea.isOne(k S))){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W) && (relArea.isOne(k) || relArea.isOne(k N) || relArea.isOne(k W))){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E) && (relArea.isOne(k) || relArea.isOne(k N) || relArea.isOne(k E))){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  return num;
}


int BoardMoveGen::genPushPullsOfOpps(const Board& b, pla_t pla, move_t* mv,
    Bitmap pushOpps, Bitmap pullOpps, Bitmap pushOppsTo, Bitmap pullOppsTo)
{
  pla_t opp = gOpp(pla);

  //Bitmap of stuff adjacent to empty space
  Bitmap emptySpace = ~b.pieceMaps[pla][0] & ~b.pieceMaps[opp][0];
  Bitmap isOpenMap = Bitmap::adj(emptySpace);
  //Unfrozen nonrabbit pieces, which might be able to push or pull
  Bitmap potentialPushpullers = b.pieceMaps[pla][0] & ~b.frozenMap & ~b.pieceMaps[pla][RAB];
  //Dominated opps, which maybe can be pushed or pulled
  Bitmap dominatedOpps = b.pieceMaps[opp][0] & b.dominatedMap;

  //Pushable things are - opps that are dominated with a potential pushpuller around, and either
  //empty space that we want to push to, or empty space around a thing we want to push
  Bitmap pushMap = (Bitmap::adj(potentialPushpullers) & dominatedOpps) &
      ((isOpenMap & pushOpps) | (Bitmap::adj(emptySpace & pushOppsTo)));

  //Pull-capable pieces are potential pushpullers with empty space around and either
  //a dominated opp that we want to pull or a dominated opp around a space we want to pull to.
  Bitmap pullMap = potentialPushpullers  & isOpenMap &
      (Bitmap::adj(dominatedOpps & pullOpps) | (Bitmap::adj(dominatedOpps) & pullOppsTo));

  int num = 0;
  while(pushMap.hasBits())
  {
    loc_t k = pushMap.nextBit();
    bool isPushOpp = pushOpps.isOne(k);
    if(ISP(k S) && GT(k S,k) && b.isThawed(k S))
    {
      if(ISE(k W) && (isPushOpp || pushOppsTo.isOne(k W))){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E) && (isPushOpp || pushOppsTo.isOne(k E))){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N) && (isPushOpp || pushOppsTo.isOne(k N))){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISP(k W) && GT(k W,k) && b.isThawed(k W))
    {
      if(ISE(k S) && (isPushOpp || pushOppsTo.isOne(k S))){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E) && (isPushOpp || pushOppsTo.isOne(k E))){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N) && (isPushOpp || pushOppsTo.isOne(k N))){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISP(k E) && GT(k E,k) && b.isThawed(k E))
    {
      if(ISE(k S) && (isPushOpp || pushOppsTo.isOne(k S))){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W) && (isPushOpp || pushOppsTo.isOne(k W))){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N) && (isPushOpp || pushOppsTo.isOne(k N))){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISP(k N) && GT(k N,k) && b.isThawed(k N))
    {
      if(ISE(k S) && (isPushOpp || pushOppsTo.isOne(k S))){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W) && (isPushOpp || pushOppsTo.isOne(k W))){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E) && (isPushOpp || pushOppsTo.isOne(k E))){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  while(pullMap.hasBits())
  {
    loc_t k = pullMap.nextBit();
    bool isPullOppTo = pullOppsTo.isOne(k);
    if(ISO(k S) && GT(k,k S) && (isPullOppTo || pullOpps.isOne(k S)))
    {
      if(ISE(k W)){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISO(k W) && GT(k,k W) && (isPullOppTo || pullOpps.isOne(k W)))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISO(k E) && GT(k,k E) && (isPullOppTo || pullOpps.isOne(k E)))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W)){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N)){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISO(k N) && GT(k,k N) && (isPullOppTo || pullOpps.isOne(k N)))
    {
      if(ISE(k S)){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W)){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E)){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  return num;
}

//Generates all pushpulls involving any square in relArea, except does not generate
//pushpulls where a piece volunatarily hops into a trap as part of the move.
int BoardMoveGen::genPushPullsOfOppsInvolvingNoSuicide(const Board& b, pla_t pla, move_t* mv, Bitmap relArea)
{
  pla_t opp = gOpp(pla);

  //Bitmap of stuff adjacent to empty space
  Bitmap isOpenMap = Bitmap::adj(~b.pieceMaps[pla][0] & ~b.pieceMaps[opp][0]);
  //Unfrozen nonrabbit pieces, which might be able to push or pull
  Bitmap potentialPushpullers = b.pieceMaps[pla][0] & ~b.frozenMap & ~b.pieceMaps[pla][RAB];
  //Dominated opps, which maybe can be pushed or pulled
  Bitmap dominatedOpps = b.pieceMaps[opp][0] & b.dominatedMap;
  //Near relevant area
  Bitmap nearRelArea = relArea | Bitmap::adj(relArea);

  //Pushable things are - opps that are dominated with empty space around and a potential pushpuller around
  Bitmap pushMap = Bitmap::adj(potentialPushpullers) & dominatedOpps & isOpenMap & nearRelArea;
  //Pull-capable pieces are potential pushpullers with a dominated opp and empty space around
  Bitmap pullMap = potentialPushpullers & Bitmap::adj(dominatedOpps & nearRelArea) & isOpenMap & nearRelArea;

  int num = 0;
  while(pushMap.hasBits())
  {
    loc_t k = pushMap.nextBit();
    if(!b.isTrapSafe2(pla,k))
      continue;
    bool inRelArea = relArea.isOne(k);
    if(ISP(k S) && GT(k S,k) && b.isThawed(k S))
    {
      if(ISE(k W) && (inRelArea || relArea.isOne(k W))){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E) && (inRelArea || relArea.isOne(k E))){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N) && (inRelArea || relArea.isOne(k N))){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISP(k W) && GT(k W,k) && b.isThawed(k W))
    {
      if(ISE(k S) && (inRelArea || relArea.isOne(k S))){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E) && (inRelArea || relArea.isOne(k E))){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N) && (inRelArea || relArea.isOne(k N))){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISP(k E) && GT(k E,k) && b.isThawed(k E))
    {
      if(ISE(k S) && (inRelArea || relArea.isOne(k S))){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W) && (inRelArea || relArea.isOne(k W))){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N) && (inRelArea || relArea.isOne(k N))){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISP(k N) && GT(k N,k) && b.isThawed(k N))
    {
      if(ISE(k S) && (inRelArea || relArea.isOne(k S))){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W) && (inRelArea || relArea.isOne(k W))){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E) && (inRelArea || relArea.isOne(k E))){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  while(pullMap.hasBits())
  {
    loc_t k = pullMap.nextBit();
    bool inRelArea = relArea.isOne(k);
    if(ISO(k S) && GT(k,k S) && (inRelArea || relArea.isOne(k S)))
    {
      if(ISE(k W) && b.isTrapSafe2(pla,k W)){mv[num++] = getMove(k MW,k S MN);}
      if(ISE(k E) && b.isTrapSafe2(pla,k E)){mv[num++] = getMove(k ME,k S MN);}
      if(ISE(k N) && b.isTrapSafe2(pla,k N)){mv[num++] = getMove(k MN,k S MN);}
    }
    if(ISO(k W) && GT(k,k W) && (inRelArea || relArea.isOne(k W)))
    {
      if(ISE(k S) && b.isTrapSafe2(pla,k S)){mv[num++] = getMove(k MS,k W ME);}
      if(ISE(k E) && b.isTrapSafe2(pla,k E)){mv[num++] = getMove(k ME,k W ME);}
      if(ISE(k N) && b.isTrapSafe2(pla,k N)){mv[num++] = getMove(k MN,k W ME);}
    }
    if(ISO(k E) && GT(k,k E) && (inRelArea || relArea.isOne(k E)))
    {
      if(ISE(k S) && b.isTrapSafe2(pla,k S)){mv[num++] = getMove(k MS,k E MW);}
      if(ISE(k W) && b.isTrapSafe2(pla,k W)){mv[num++] = getMove(k MW,k E MW);}
      if(ISE(k N) && b.isTrapSafe2(pla,k N)){mv[num++] = getMove(k MN,k E MW);}
    }
    if(ISO(k N) && GT(k,k N) && (inRelArea || relArea.isOne(k N)))
    {
      if(ISE(k S) && b.isTrapSafe2(pla,k S)){mv[num++] = getMove(k MS,k N MS);}
      if(ISE(k W) && b.isTrapSafe2(pla,k W)){mv[num++] = getMove(k MW,k N MS);}
      if(ISE(k E) && b.isTrapSafe2(pla,k E)){mv[num++] = getMove(k ME,k N MS);}
    }
  }
  return num;
}

int BoardMoveGen::genStepsIntoOutTSWF(const Board& b, pla_t pla, move_t* mv, Bitmap relArea)
{
  Bitmap pieceMap = b.pieceMaps[pla][0];
  pieceMap &= ~b.frozenMap;

  Bitmap inMap = pieceMap & relArea;
  Bitmap outMap = pieceMap & Bitmap::adj(relArea) & (~inMap);
  int num = 0;
  while(inMap.hasBits())
  {
    loc_t loc = inMap.nextBit();
    if(ISE(loc S) && b.isRabOkayS(pla,loc) && (relArea.isOne(loc S) || !b.isTrapSafe1(pla,loc) || (b.isDominated(loc) && !b.isGuarded2(pla,loc)))) mv[num++] = getMove(loc MS);
    if(ISE(loc W)                          && (relArea.isOne(loc W) || !b.isTrapSafe1(pla,loc) || (b.isDominated(loc) && !b.isGuarded2(pla,loc)))) mv[num++] = getMove(loc MW);
    if(ISE(loc E)                          && (relArea.isOne(loc E) || !b.isTrapSafe1(pla,loc) || (b.isDominated(loc) && !b.isGuarded2(pla,loc)))) mv[num++] = getMove(loc ME);
    if(ISE(loc N) && b.isRabOkayN(pla,loc) && (relArea.isOne(loc N) || !b.isTrapSafe1(pla,loc) || (b.isDominated(loc) && !b.isGuarded2(pla,loc)))) mv[num++] = getMove(loc MN);
  }
  while(outMap.hasBits())
  {
    loc_t loc = outMap.nextBit();
    if(ISE(loc S) && b.isRabOkayS(pla,loc) && relArea.isOne(loc S)) mv[num++] = getMove(loc MS);
    if(ISE(loc W)                          && relArea.isOne(loc W)) mv[num++] = getMove(loc MW);
    if(ISE(loc E)                          && relArea.isOne(loc E)) mv[num++] = getMove(loc ME);
    if(ISE(loc N) && b.isRabOkayN(pla,loc) && relArea.isOne(loc N)) mv[num++] = getMove(loc MN);
  }
  return num;
}

int BoardMoveGen::genStepsInvolving(const Board& b, pla_t pla, move_t* mv, Bitmap relArea)
{
  pla_t opp = gOpp(pla);
  Bitmap pieceMap = b.pieceMaps[pla][0];
  Bitmap emptyMap = ~(b.pieceMaps[opp][0] | pieceMap);
  Bitmap plaRabMap = b.pieceMaps[pla][RAB];
  pieceMap &= ~b.frozenMap;

  int num = 0;
  if(pla == GOLD)
  {
    Bitmap mp;
    mp = Bitmap::shiftN(pieceMap) & emptyMap & (relArea | Bitmap::shiftN(relArea));              while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
    mp = Bitmap::shiftW(pieceMap) & emptyMap & (relArea | Bitmap::shiftW(relArea));              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & emptyMap & (relArea | Bitmap::shiftE(relArea));              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftS(pieceMap & ~plaRabMap) & emptyMap & (relArea | Bitmap::shiftS(relArea)); while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
  }
  else //pla == SILV
  {
    Bitmap mp;
    mp = Bitmap::shiftS(pieceMap) & emptyMap & (relArea | Bitmap::shiftS(relArea));              while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
    mp = Bitmap::shiftW(pieceMap) & emptyMap & (relArea | Bitmap::shiftW(relArea));              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & emptyMap & (relArea | Bitmap::shiftE(relArea));              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftN(pieceMap & ~plaRabMap) & emptyMap & (relArea | Bitmap::shiftN(relArea)); while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
  }
  return num;
}

int BoardMoveGen::genStepsInto(const Board& b, pla_t pla, move_t* mv, Bitmap relArea)
{
  pla_t opp = gOpp(pla);
  Bitmap pieceMap = b.pieceMaps[pla][0];
  Bitmap empAndRel = ~(b.pieceMaps[opp][0] | pieceMap) & relArea;
  Bitmap plaRabMap = b.pieceMaps[pla][RAB];
  pieceMap &= ~b.frozenMap;

  int num = 0;
  if(pla == GOLD)
  {
    Bitmap mp;
    mp = Bitmap::shiftN(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
    mp = Bitmap::shiftW(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftS(pieceMap & ~plaRabMap) & empAndRel; while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
  }
  else //pla == SILV
  {
    Bitmap mp;
    mp = Bitmap::shiftS(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
    mp = Bitmap::shiftW(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftN(pieceMap & ~plaRabMap) & empAndRel; while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
  }
  return num;
}

int BoardMoveGen::genStepsForwardInto(const Board& b, pla_t pla, move_t* mv, Bitmap relArea)
{
  pla_t opp = gOpp(pla);
  Bitmap pieceMap = b.pieceMaps[pla][0];
  Bitmap empAndRel = ~(b.pieceMaps[opp][0] | pieceMap) & relArea;
  pieceMap &= ~b.frozenMap;

  int num = 0;
  if(pla == GOLD)
  {
    Bitmap mp;
    mp = Bitmap::shiftN(pieceMap) & empAndRel; while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
  }
  else //pla == SILV
  {
    Bitmap mp;
    mp = Bitmap::shiftS(pieceMap) & empAndRel; while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
  }
  return num;
}

int BoardMoveGen::genStepsIntoAndOutof(const Board& b, pla_t pla, move_t* mv, Bitmap intoArea, Bitmap outofArea)
{
  pla_t opp = gOpp(pla);
  Bitmap pieceMap = b.pieceMaps[pla][0];
  Bitmap empAndRel = ~(b.pieceMaps[opp][0] | pieceMap) & intoArea;
  Bitmap plaRabMap = b.pieceMaps[pla][RAB];
  pieceMap &= ~b.frozenMap & outofArea;

  int num = 0;
  if(pla == GOLD)
  {
    Bitmap mp;
    mp = Bitmap::shiftN(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
    mp = Bitmap::shiftW(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftS(pieceMap & ~plaRabMap) & empAndRel; while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
  }
  else //pla == SILV
  {
    Bitmap mp;
    mp = Bitmap::shiftS(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
    mp = Bitmap::shiftW(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & empAndRel;              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftN(pieceMap & ~plaRabMap) & empAndRel; while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
  }
  return num;
}

int BoardMoveGen::genStepsIntoOrOutof(const Board& b, pla_t pla, move_t* mv, Bitmap intoArea, Bitmap outofArea)
{
  pla_t opp = gOpp(pla);
  Bitmap pieceMap = b.pieceMaps[pla][0];
  Bitmap empAndRel = ~(b.pieceMaps[opp][0] | pieceMap);
  Bitmap plaRabMap = b.pieceMaps[pla][RAB];
  pieceMap &= ~b.frozenMap;

  int num = 0;
  if(pla == GOLD)
  {
    Bitmap mp;
    mp = Bitmap::shiftN(pieceMap) & empAndRel & (Bitmap::shiftN(outofArea) | intoArea);              while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
    mp = Bitmap::shiftW(pieceMap) & empAndRel & (Bitmap::shiftW(outofArea) | intoArea);              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & empAndRel & (Bitmap::shiftE(outofArea) | intoArea);              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftS(pieceMap & ~plaRabMap) & empAndRel & (Bitmap::shiftS(outofArea) | intoArea); while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
  }
  else //pla == SILV
  {
    Bitmap mp;
    mp = Bitmap::shiftS(pieceMap) & empAndRel & (Bitmap::shiftS(outofArea) | intoArea);              while(mp.hasBits()) {loc_t loc = mp.nextBit() N; mv[num++] = getMove(loc MS);}
    mp = Bitmap::shiftW(pieceMap) & empAndRel & (Bitmap::shiftW(outofArea) | intoArea);              while(mp.hasBits()) {loc_t loc = mp.nextBit() E; mv[num++] = getMove(loc MW);}
    mp = Bitmap::shiftE(pieceMap) & empAndRel & (Bitmap::shiftE(outofArea) | intoArea);              while(mp.hasBits()) {loc_t loc = mp.nextBit() W; mv[num++] = getMove(loc ME);}
    mp = Bitmap::shiftN(pieceMap & ~plaRabMap) & empAndRel & (Bitmap::shiftN(outofArea) | intoArea); while(mp.hasBits()) {loc_t loc = mp.nextBit() S; mv[num++] = getMove(loc MN);}
  }
  return num;
}
