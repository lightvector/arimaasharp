
/*
 * boardcaptree.cpp
 * Author: davidwu
 *
 * Capture detection and generation functions
 */

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/offsets.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"

using namespace std;

//MINOR maybe add a few more 4-step captures that are 3-step captures except that
//we guard a trap to avoid saccing a piece

//MINOR try adding "const" to everything where relevant

//Cap Tree Helpers
static bool couldPushToTrap(Board& b, pla_t pla, loc_t eloc, loc_t kt, loc_t uf1, loc_t uf2);
static bool canSteps1S(Board& b, pla_t pla, loc_t k);
static int genSteps1S(Board& b, pla_t pla, loc_t k, move_t* mv, int* hm, int hmval);
static bool canPushE(Board& b, loc_t eloc, loc_t fadjloc);
static int genPushPE(Board& b, loc_t ploc, loc_t eloc, loc_t fadjloc, move_t* mv, int* hm, int hmval);
static int genPullPE(Board& b, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canPushc(Board& b, pla_t pla, loc_t eloc);
static int genPush(Board& b, pla_t pla, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canPullc(Board& b, pla_t pla, loc_t eloc);
static int genPull(Board& b, pla_t pla, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canPPPE(Board& b, loc_t ploc, loc_t eloc);
static bool canUF(Board& b, pla_t pla, loc_t ploc);
static int genUF(Board& b, pla_t pla, loc_t ploc, move_t* mv, int* hm, int hmval);
static bool canUFAt(Board& b, pla_t pla, loc_t loc);
static int genUFAt(Board& b, pla_t pla, loc_t loc, move_t* mv, int* hm, int hmval);
static bool canUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc);
static int genUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canUF2(Board& b, pla_t pla, loc_t ploc);
static int genUF2(Board& b, pla_t pla, loc_t ploc, move_t* mv, int* hm, int hmval);
static bool canPPCapAndUFc(Board& b, pla_t pla, loc_t eloc, loc_t ploc);
static int genPPCapAndUF(Board& b, pla_t pla, loc_t eloc, loc_t ploc, move_t* mv, int* hm, int hmval);
static bool canGetPlaTo2S(Board& b, pla_t pla, loc_t k, loc_t floc);
static int genGetPlaTo2S(Board& b, pla_t pla, loc_t k, loc_t floc, move_t* mv, int* hm, int hmval);
static bool canUFUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t plocdest, loc_t ploc2, loc_t eloc);
static int genUFUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t plocdest, loc_t ploc2, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canStepStepPPPE(Board& b, pla_t pla, loc_t dest, loc_t dest2, loc_t ploc, loc_t eloc);
static int genStepStepPPPE(Board& b, pla_t pla, loc_t dest, loc_t dest2, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canPushPPPE(Board& b, pla_t pla, loc_t floc, loc_t ploc, loc_t eloc);
static int genPushPPPE(Board& b, pla_t pla, loc_t floc, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canUF2PPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc);
static int genUF2PPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canGetPlaTo2SPPPE(Board& b, pla_t pla, loc_t k, loc_t ploc, loc_t eloc);
static int genGetPlaTo2SPPPE(Board& b, pla_t pla, loc_t k, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canPP(Board& b, pla_t pla, loc_t eloc);
static int genPP(Board& b, pla_t pla, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canPPTo(Board& b, pla_t pla, loc_t eloc, loc_t dest);
static int genPPTo(Board& b, pla_t pla, loc_t eloc, loc_t dest, move_t* mv, int* hm, int hmval);
static bool canMoveStrongAdjCent(Board& b, pla_t pla, loc_t dest, loc_t eloc, loc_t forbidden);
static int genMoveStrongAdjCent(Board& b, pla_t pla, loc_t dest, loc_t eloc, loc_t forbidden, move_t* mv, int* hm, int hmval);
static bool canMoveStrongTo(Board& b, pla_t pla, loc_t dest, loc_t eloc);
static int genMoveStrongToAndThen(Board& b, pla_t pla, loc_t dest, loc_t eloc, move_t move, move_t* mv, int* hm, int hmval);
static bool canAdvanceUFStep(Board& b, pla_t pla, loc_t curploc, loc_t futploc);
static int genAdvanceUFStep(Board& b, pla_t pla, loc_t curploc, loc_t futploc, move_t* mv, int* hm, int hmval);
static bool canUFStepWouldBeUF(Board& b, pla_t pla, loc_t ploc, loc_t wloc, loc_t kt);
static int genUFStepWouldBeUF(Board& b, pla_t pla, loc_t ploc, loc_t wloc, loc_t kt,move_t* mv, int* hm, int hmval);
static bool canStepStepc(Board& b, pla_t pla, loc_t dest, loc_t dest2);
static int genStepStep(Board& b, pla_t pla, loc_t dest, loc_t dest2, move_t* mv, int* hm, int hmval);
static loc_t findOpenToStepPreferEdge(const Board& b, pla_t pla, loc_t loc);
static bool movingWouldSacPiece(const Board& b, pla_t pla, loc_t ploc);


//2 STEP CAPTURES----------------------------------------------------------------------
static bool canPPIntoTrap2C(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static int genPPIntoTrap2C(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static bool canRemoveDef2C(Board& b, pla_t pla, loc_t eloc);
static int genRemoveDef2C(Board& b, pla_t pla, loc_t eloc, move_t* mv, int* hm, int hmval);

//3 STEP CAPTURES----------------------------------------------------------------------
static bool canPPIntoTrapTE3C(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static bool canPPIntoTrapTE3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static int genPPIntoTrapTE3C(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static int genPPIntoTrapTE3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static bool canPPIntoTrapTP3C(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static int genPPIntoTrapTP3C(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static bool canRemoveDef3C(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static bool canRemoveDef3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static int genRemoveDef3C(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static int genRemoveDef3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);

//4-STEP CAPTURES---------------------------------------------------------------------

//0 DEFENDERS-----------
static bool can0PieceTrail(Board& b, pla_t pla, loc_t eloc, loc_t tr, loc_t kt);
static int gen0PieceTrail(Board& b, pla_t pla, loc_t eloc, loc_t tr, loc_t kt, move_t* mv, int* hm, int hmval);

//2 DEFENDERS------------
static bool can2PieceAdj(Board& b, pla_t pla, loc_t eloc1, loc_t eloc2, loc_t kt);
static int gen2PieceAdj(Board& b, pla_t pla, loc_t eloc1, loc_t eloc2, loc_t kt, move_t* mv, int* hm, int hmval);

//4-STEP CAPTURES, 1 DEFENDER-----
static bool canMoveStrong2S(Board& b, pla_t pla, loc_t sloc, loc_t eloc);
static int genMoveStrong2S(Board& b, pla_t pla, loc_t sloc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canSwapPla(Board& b, pla_t pla, loc_t sloc, loc_t eloc);
static int genSwapPla(Board& b, pla_t pla, loc_t sloc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canSwapOpp(Board& b, pla_t pla, loc_t sloc, loc_t eloc, loc_t kt);
static int genSwapOpp(Board& b, pla_t pla, loc_t sloc, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static bool canPPIntoTrapTE4C(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static int genPPIntoTrapTE4C(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static bool canPPIntoTrapTP4C(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static int genPPIntoTrapTP4C(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static bool canRemoveDef4C(Board& b, pla_t pla, loc_t eloc, loc_t kt);
static int genRemoveDef4C(Board& b, pla_t pla, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static bool canEmpty2(Board& b, pla_t pla, loc_t loc, loc_t floc, loc_t ploc);
static int genEmpty2(Board& b, pla_t pla, loc_t loc, loc_t floc, loc_t ploc, move_t* mv, int* hm, int hmval);
static bool canUFBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc);
static int genUFBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc);
static int genBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval);
static bool canBlockedPP2(Board& b, pla_t pla, loc_t ploc, loc_t eloc, loc_t kt);
static int genBlockedPP2(Board& b, pla_t pla, loc_t ploc, loc_t eloc, loc_t kt, move_t* mv, int* hm, int hmval);
static loc_t findAdjOpp(Board& b, pla_t pla, loc_t k);
static bool canAdvancePullCap(Board& b, pla_t pla, loc_t ploc, loc_t tr);
static int genAdvancePullCap(Board& b, pla_t pla, loc_t ploc, loc_t tr, move_t* mv, int* hm, int hmval);


//TOP-LEVEL FUNCTIONS-----------------------------------------------------------------

bool BoardTrees::canCaps(Board& b, pla_t pla, int steps)
{
  return
  canCaps(b,pla,steps,2,TRAP0) ||
  canCaps(b,pla,steps,2,TRAP1) ||
  canCaps(b,pla,steps,2,TRAP2) ||
  canCaps(b,pla,steps,2,TRAP3);
}

int BoardTrees::genCaps(Board& b, pla_t pla, int steps,
move_t* mv, int* hm)
{
  int num = 0;
  num += genCaps(b,pla,steps,2,TRAP0,mv+num,hm+num);
  num += genCaps(b,pla,steps,2,TRAP1,mv+num,hm+num);
  num += genCaps(b,pla,steps,2,TRAP2,mv+num,hm+num);
  num += genCaps(b,pla,steps,2,TRAP3,mv+num,hm+num);
  return num;
}

//Same as genCapsExtended except also fills out the capturing moves so that (barring cap tree bugs)
//every move actually performs a capture (as opposed to being the first few steps of a move that captures).
int BoardTrees::genCapsFull(Board& b, pla_t pla, int steps, int minSteps, bool suicideExtend, loc_t kt, move_t* mv, int* hm)
{
  int stepsUsed = 0;
  int num = genCapsExtended(b,pla,steps,minSteps,suicideExtend,kt,mv,hm,&stepsUsed);

  if(steps == 2)
    return num;

  //Cumulate into full capture moves. First, split the full captures into complete ones and incomplete ones.
  int numMvBuf = 0;
  int goodNum = 0;
  move_t mvBuf[num];
  int hmBuf[num];
  int minStepsInBufMove = 4;
  for(int i = 0; i<num; i++)
  {
    int ns = numStepsInMove(mv[i]);
    if(ns < stepsUsed)
    {
      mvBuf[numMvBuf] = mv[i];
      hmBuf[numMvBuf] = hm[i];
      numMvBuf++;
      if(ns < minStepsInBufMove)
        minStepsInBufMove = ns;
    }
    else
    {
      mv[goodNum] = mv[i];
      hm[goodNum] = hm[i];
      goodNum++;
    }
  }

  //Okay, these are all good, so pack them away
  int totalNum = 0;
  mv += goodNum;
  hm += goodNum;
  totalNum += goodNum;

  //Now, finish off all the incomplete ones
  for(int i = 0; i<numMvBuf; i++)
  {
    move_t move = mvBuf[i];
    int ns = numStepsInMove(move);

    Board copy = b;
    copy.makeMove(move);

    if(copy.pieceCounts[gOpp(pla)][0] < b.pieceCounts[gOpp(pla)][0])
    {
      mv[0] = move;
      hm[0] = hmBuf[i];
      mv++;
      hm++;
      totalNum++;
    }
    else
    {
      DEBUGASSERT(!(stepsUsed-ns == 1 || ns >= 4));
      int newNum = genCapsFull(copy,pla,stepsUsed-ns,minStepsInBufMove-ns,false,kt,mv,hm);
      for(int k = 0; k<newNum; k++)
      {
        mv[k] = concatMoves(move,mv[k],ns);
        if(hm[k] > hmBuf[i]) //Try to carry suicide penalties through
          hm[k] = hmBuf[i];
      }
      mv += newNum;
      hm += newNum;
      totalNum += newNum;
    }
  }

  return totalNum;
}

//TODO is this code really expensive?
//Returns true all moves were suicides (and at least one of them was a suicide)
static bool filterSuicides(Board& b, pla_t pla, move_t* mv, int* hm, int& num)
{
  int newNum = 0;
  bool onlySuicides = num > 0;
  for(int i = 0; i<num; i++)
  {
    loc_t src[8];
    loc_t dest[8];
    bool moveHasSuicide = false;
    piece_t suicideStr = EMP;
    int numChanges = b.getChanges(mv[i],src,dest);
    for(int j = 0; j<numChanges; j++)
    {
      if(b.owners[src[j]] == pla && dest[j] == ERRLOC)
      {moveHasSuicide = true; suicideStr = b.pieces[src[j]];}
    }
    if(!moveHasSuicide)
    {onlySuicides = false;}

    //TODO we rely on this function to do penalization of the ordering for suicides here,
    //but genCapsFull on deeper recursions doesn't call this, so suicides don't get penalized.
    //Maybe do something better?
    if(!moveHasSuicide || suicideStr < hm[i])
    {mv[newNum] = mv[i]; hm[newNum] = hm[i] - suicideStr; newNum++;}
  }
  num = newNum;
  return onlySuicides;
}

//Goes a little further and generates captures using more steps than the minimum possible
//Generates captures with that take a number of steps in [minSteps,maxSteps], but only captures
//up to a single step more than the minimum possible actual capture, unless suicide extending.
//If suicideExtend is true, filters out suicide moves that lose as much material as gained
//if they are 4-step captures or if there are shorter captures.
//and additionally extends another step if every capture found with a given number of steps is a suicide capture.
int BoardTrees::genCapsExtended(Board& b, pla_t pla, int maxSteps, int minSteps, bool suicideExtend, loc_t kt,
move_t* mv, int* hm, int* stepsUsed)
{
  int num = 0;
  if(maxSteps <= 1)
    return num;

  if(minSteps <= 2)
  {
    int newNum = genCaps(b,pla,2,2,kt,mv+num,hm+num);
    //Filter suicides if desired
    bool wantSuicideExtension = num > 0 && suicideExtend && filterSuicides(b,pla,mv+num,hm+num,newNum);
    //If we hit the max, or there were already captures found prior to this step,
    //and we aren't suicide extending, then we're done.
    if(maxSteps <= 2 || (num > 0 && !wantSuicideExtension))
    {if(stepsUsed != NULL) *stepsUsed = 2; return num+newNum;}
    num += newNum;
  }

  if(minSteps <= 3)
  {
    int newNum = genCaps(b,pla,3,3,kt,mv+num,hm+num);
    //Filter suicides if desired
    bool wantSuicideExtension = num > 0 && suicideExtend && filterSuicides(b,pla,mv+num,hm+num,newNum);
    //If we hit the max, or there were already captures found prior to this step,
    //and we aren't suicide extending, then we're done.
    if(maxSteps <= 3 || (num > 0 && !wantSuicideExtension))
    {if(stepsUsed != NULL) *stepsUsed = 3; return num+newNum;}
    num += newNum;
  }

  int newNum = genCaps(b,pla,4,4,kt,mv+num,hm+num);

  //Filter suicides if desired
  if(suicideExtend) {filterSuicides(b,pla,mv+num,hm+num,newNum);}
  if(stepsUsed != NULL) *stepsUsed = 4;
  return num+newNum;
}

bool BoardTrees::canCaps(Board& b, pla_t pla, int steps, loc_t kt, Bitmap& capMap, int& capDist)
{
#ifdef CHECK_CAPTREE_CONSISTENCY
  assert(b.testConsistency(cout));
#endif

  pla_t opp = gOpp(pla);

  //Check some quick conditions to prove capture impossible
  if(steps < 2 || steps > 4 ||
      (Board::DISK[1 + (int)(steps == 4)][kt] & b.pieceMaps[opp][0]).isEmpty() ||
      (Board::RADIUS[1][kt] & b.pieceMaps[opp][ELE]).hasBits())
    return false;

  bool oppS = ISO(kt S);
  bool oppW = ISO(kt W);
  bool oppE = ISO(kt E);
  bool oppN = ISO(kt N);

  int defNum = oppS + oppW + oppE + oppN;
  pla_t trapOwner = b.owners[kt];

  //Check some more quick conditions to prove capture impossible
  if(defNum >= 3 || (steps < 4 && defNum != 1))
    return false;

  if(steps >= 2)
  {
    if(defNum == 1 && (Board::RADIUS[1][kt] & b.pieceMaps[opp][0] & b.dominatedMap).hasBits())
    {
      capDist = 2;
      if(trapOwner != opp)
      {
        if     (oppS) {if(canPPIntoTrap2C(b,pla,kt S,kt)) {capMap.setOn(kt S); return true;}}
        else if(oppW) {if(canPPIntoTrap2C(b,pla,kt W,kt)) {capMap.setOn(kt W); return true;}}
        else if(oppE) {if(canPPIntoTrap2C(b,pla,kt E,kt)) {capMap.setOn(kt E); return true;}}
        else          {if(canPPIntoTrap2C(b,pla,kt N,kt)) {capMap.setOn(kt N); return true;}}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {if(canRemoveDef2C(b,pla,kt S)){capMap.setOn(kt); return true;}}
        else if(oppW) {if(canRemoveDef2C(b,pla,kt W)){capMap.setOn(kt); return true;}}
        else if(oppE) {if(canRemoveDef2C(b,pla,kt E)){capMap.setOn(kt); return true;}}
        else          {if(canRemoveDef2C(b,pla,kt N)){capMap.setOn(kt); return true;}}
      }
    }
  }
  if(steps >= 3)
  {
    if(defNum == 1)
    {
      capDist = 3;
      if(trapOwner == NPLA)
      {
        if     (oppS) {if(canPPIntoTrapTE3CC(b,pla,kt S,kt)){capMap.setOn(kt S); return true;}}
        else if(oppW) {if(canPPIntoTrapTE3CC(b,pla,kt W,kt)){capMap.setOn(kt W); return true;}}
        else if(oppE) {if(canPPIntoTrapTE3CC(b,pla,kt E,kt)){capMap.setOn(kt E); return true;}}
        else          {if(canPPIntoTrapTE3CC(b,pla,kt N,kt)){capMap.setOn(kt N); return true;}}
      }
      else if(trapOwner == pla)
      {
        if     (oppS) {if(canPPIntoTrapTP3C(b,pla,kt S,kt)){capMap.setOn(kt S); return true;}}
        else if(oppW) {if(canPPIntoTrapTP3C(b,pla,kt W,kt)){capMap.setOn(kt W); return true;}}
        else if(oppE) {if(canPPIntoTrapTP3C(b,pla,kt E,kt)){capMap.setOn(kt E); return true;}}
        else          {if(canPPIntoTrapTP3C(b,pla,kt N,kt)){capMap.setOn(kt N); return true;}}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {if(canRemoveDef3CC(b,pla,kt S,kt)){capMap.setOn(kt); return true;}}
        else if(oppW) {if(canRemoveDef3CC(b,pla,kt W,kt)){capMap.setOn(kt); return true;}}
        else if(oppE) {if(canRemoveDef3CC(b,pla,kt E,kt)){capMap.setOn(kt); return true;}}
        else          {if(canRemoveDef3CC(b,pla,kt N,kt)){capMap.setOn(kt); return true;}}
      }
    }
  }
  if(steps >= 4)
  {
    capDist = 4;
    if(defNum == 0)
    {
      if((Board::RADIUS[2][kt] & b.pieceMaps[opp][0] & b.dominatedMap).isEmpty())
        return false;

      bool foundCap = false;
      if(ISO(kt SS)) {if(can0PieceTrail(b,pla,kt SS,kt S,kt)){capMap.setOn(kt SS); foundCap = true;}}
      if(ISO(kt WW)) {if(can0PieceTrail(b,pla,kt WW,kt W,kt)){capMap.setOn(kt WW); foundCap = true;}}
      if(ISO(kt EE)) {if(can0PieceTrail(b,pla,kt EE,kt E,kt)){capMap.setOn(kt EE); foundCap = true;}}
      if(ISO(kt NN)) {if(can0PieceTrail(b,pla,kt NN,kt N,kt)){capMap.setOn(kt NN); foundCap = true;}}

      if(ISO(kt SW))
      {if(can0PieceTrail(b,pla,kt SW,kt S,kt)
        ||can0PieceTrail(b,pla,kt SW,kt W,kt)){capMap.setOn(kt SW); foundCap = true;}}
      if(ISO(kt SE))
      {if(can0PieceTrail(b,pla,kt SE,kt S,kt)
        ||can0PieceTrail(b,pla,kt SE,kt E,kt)){capMap.setOn(kt SE); foundCap = true;}}
      if(ISO(kt NW))
      {if(can0PieceTrail(b,pla,kt NW,kt W,kt)
        ||can0PieceTrail(b,pla,kt NW,kt N,kt)){capMap.setOn(kt NW); foundCap = true;}}
      if(ISO(kt NE))
      {if(can0PieceTrail(b,pla,kt NE,kt E,kt)
        ||can0PieceTrail(b,pla,kt NE,kt N,kt)){capMap.setOn(kt NE); foundCap = true;}}

      if(foundCap)
        return true;
    }
    else if(defNum == 1)
    {
      if(trapOwner == NPLA)
      {
        if     (oppS) {if(canPPIntoTrapTE4C(b,pla,kt S,kt)){capMap.setOn(kt S); return true;}}
        else if(oppW) {if(canPPIntoTrapTE4C(b,pla,kt W,kt)){capMap.setOn(kt W); return true;}}
        else if(oppE) {if(canPPIntoTrapTE4C(b,pla,kt E,kt)){capMap.setOn(kt E); return true;}}
        else          {if(canPPIntoTrapTE4C(b,pla,kt N,kt)){capMap.setOn(kt N); return true;}}
      }
      else if(trapOwner == pla)
      {
        if     (oppS) {if(canPPIntoTrapTP4C(b,pla,kt S,kt)){capMap.setOn(kt S); return true;}}
        else if(oppW) {if(canPPIntoTrapTP4C(b,pla,kt W,kt)){capMap.setOn(kt W); return true;}}
        else if(oppE) {if(canPPIntoTrapTP4C(b,pla,kt E,kt)){capMap.setOn(kt E); return true;}}
        else          {if(canPPIntoTrapTP4C(b,pla,kt N,kt)){capMap.setOn(kt N); return true;}}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {if(canRemoveDef4C(b,pla,kt S,kt)){capMap.setOn(kt); return true;}}
        else if(oppW) {if(canRemoveDef4C(b,pla,kt W,kt)){capMap.setOn(kt); return true;}}
        else if(oppE) {if(canRemoveDef4C(b,pla,kt E,kt)){capMap.setOn(kt); return true;}}
        else          {if(canRemoveDef4C(b,pla,kt N,kt)){capMap.setOn(kt); return true;}}
      }
    }
    else //if(defNum == 2)
    {
      if((Board::RADIUS[1][kt] & b.pieceMaps[opp][0] & b.dominatedMap).isEmptyOrSingleBit())
        return false;
      if(((Board::RADIUS[2][kt] | Bitmap::makeLoc(kt)) & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmptyOrSingleBit())
        return false;

      if(oppS)
      {
        if(oppW)
        {if(can2PieceAdj(b,pla,kt S,kt W,kt)
          ||can2PieceAdj(b,pla,kt W,kt S,kt)){if(ISO(kt)) {capMap.setOn(kt);} else{capMap.setOn(kt S); capMap.setOn(kt W);} return true;}}
        else if(oppE)
        {if(can2PieceAdj(b,pla,kt S,kt E,kt)
          ||can2PieceAdj(b,pla,kt E,kt S,kt)){if(ISO(kt)) {capMap.setOn(kt);} else{capMap.setOn(kt S); capMap.setOn(kt E);} return true;}}
        else if(oppN)
        {if(can2PieceAdj(b,pla,kt S,kt N,kt)
          ||can2PieceAdj(b,pla,kt N,kt S,kt)){if(ISO(kt)) {capMap.setOn(kt);} else{capMap.setOn(kt S); capMap.setOn(kt N);} return true;}}
      }
      else if(oppW)
      {
        if(oppE)
        {if(can2PieceAdj(b,pla,kt W,kt E,kt)
          ||can2PieceAdj(b,pla,kt E,kt W,kt)){if(ISO(kt)) {capMap.setOn(kt);} else{capMap.setOn(kt W); capMap.setOn(kt E);} return true;}}
        else if(oppN)
        {if(can2PieceAdj(b,pla,kt W,kt N,kt)
          ||can2PieceAdj(b,pla,kt N,kt W,kt)){if(ISO(kt)) {capMap.setOn(kt);} else{capMap.setOn(kt W); capMap.setOn(kt N);} return true;}}
      }
      else
      {if(can2PieceAdj(b,pla,kt E,kt N,kt)
        ||can2PieceAdj(b,pla,kt N,kt E,kt)){if(ISO(kt)) {capMap.setOn(kt);} else{capMap.setOn(kt E); capMap.setOn(kt N);} return true;}}
    }
  }

  return false;
}

bool BoardTrees::canCaps(Board& b, pla_t pla, int steps, int minSteps, loc_t kt)
{
#ifdef CHECK_CAPTREE_CONSISTENCY
  assert(b.testConsistency(cout));
#endif

  pla_t opp = gOpp(pla);

  //Check some quick conditions to prove capture impossible
  if(steps < 2 || steps > 4 ||
      (Board::DISK[1 + (int)(steps == 4)][kt] & b.pieceMaps[opp][0]).isEmpty() ||
      (Board::RADIUS[1][kt] & b.pieceMaps[opp][ELE]).hasBits())
    return false;

  bool oppS = ISO(kt S);
  bool oppW = ISO(kt W);
  bool oppE = ISO(kt E);
  bool oppN = ISO(kt N);

  int defNum = oppS + oppW + oppE + oppN;
  pla_t trapOwner = b.owners[kt];

  //Check some more quick conditions to prove capture impossible
  if(defNum >= 3 || (steps < 4 && defNum != 1))
    return false;

  if(steps >= 2 && minSteps <= 2)
  {
    if(defNum == 1 && (Board::RADIUS[1][kt] & b.pieceMaps[opp][0] & b.dominatedMap).hasBits())
    {
      if(trapOwner != opp)
      {
        if     (oppS) {RIF(canPPIntoTrap2C(b,pla,kt S,kt))}
        else if(oppW) {RIF(canPPIntoTrap2C(b,pla,kt W,kt))}
        else if(oppE) {RIF(canPPIntoTrap2C(b,pla,kt E,kt))}
        else          {RIF(canPPIntoTrap2C(b,pla,kt N,kt))}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {RIF(canRemoveDef2C(b,pla,kt S))}
        else if(oppW) {RIF(canRemoveDef2C(b,pla,kt W))}
        else if(oppE) {RIF(canRemoveDef2C(b,pla,kt E))}
        else          {RIF(canRemoveDef2C(b,pla,kt N))}
      }
    }
  }
  if(steps >= 3 && minSteps <= 3)
  {
    if(defNum == 1)
    {
      if(trapOwner == NPLA)
      {
        if     (oppS) {RIF(canPPIntoTrapTE3CC(b,pla,kt S,kt))}
        else if(oppW) {RIF(canPPIntoTrapTE3CC(b,pla,kt W,kt))}
        else if(oppE) {RIF(canPPIntoTrapTE3CC(b,pla,kt E,kt))}
        else          {RIF(canPPIntoTrapTE3CC(b,pla,kt N,kt))}
      }
      else if(trapOwner == pla)
      {
        if     (oppS) {RIF(canPPIntoTrapTP3C(b,pla,kt S,kt))}
        else if(oppW) {RIF(canPPIntoTrapTP3C(b,pla,kt W,kt))}
        else if(oppE) {RIF(canPPIntoTrapTP3C(b,pla,kt E,kt))}
        else          {RIF(canPPIntoTrapTP3C(b,pla,kt N,kt))}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {RIF(canRemoveDef3CC(b,pla,kt S,kt))}
        else if(oppW) {RIF(canRemoveDef3CC(b,pla,kt W,kt))}
        else if(oppE) {RIF(canRemoveDef3CC(b,pla,kt E,kt))}
        else          {RIF(canRemoveDef3CC(b,pla,kt N,kt))}
      }
    }
  }
  if(steps >= 4 && minSteps <= 4)
  {
    if(defNum == 0)
    {
      if((Board::RADIUS[2][kt] & b.pieceMaps[opp][0] & b.dominatedMap).isEmpty())
        return false;

      if(ISO(kt SS))
      {RIF(can0PieceTrail(b,pla,kt SS,kt S,kt))}
      if(ISO(kt WW))
      {RIF(can0PieceTrail(b,pla,kt WW,kt W,kt))}
      if(ISO(kt EE))
      {RIF(can0PieceTrail(b,pla,kt EE,kt E,kt))}
      if(ISO(kt NN))
      {RIF(can0PieceTrail(b,pla,kt NN,kt N,kt))}

      if(ISO(kt SW))
      {RIF(can0PieceTrail(b,pla,kt SW,kt S,kt))
       RIF(can0PieceTrail(b,pla,kt SW,kt W,kt))}
      if(ISO(kt SE))
      {RIF(can0PieceTrail(b,pla,kt SE,kt S,kt))
       RIF(can0PieceTrail(b,pla,kt SE,kt E,kt))}
      if(ISO(kt NW))
      {RIF(can0PieceTrail(b,pla,kt NW,kt W,kt))
       RIF(can0PieceTrail(b,pla,kt NW,kt N,kt))}
      if(ISO(kt NE))
      {RIF(can0PieceTrail(b,pla,kt NE,kt E,kt))
       RIF(can0PieceTrail(b,pla,kt NE,kt N,kt))}
    }
    else if(defNum == 1)
    {
      if(trapOwner == NPLA)
      {
        if     (oppS) {RIF(canPPIntoTrapTE4C(b,pla,kt S,kt))}
        else if(oppW) {RIF(canPPIntoTrapTE4C(b,pla,kt W,kt))}
        else if(oppE) {RIF(canPPIntoTrapTE4C(b,pla,kt E,kt))}
        else          {RIF(canPPIntoTrapTE4C(b,pla,kt N,kt))}
      }
      else if(trapOwner == pla)
      {
        if     (oppS) {RIF(canPPIntoTrapTP4C(b,pla,kt S,kt))}
        else if(oppW) {RIF(canPPIntoTrapTP4C(b,pla,kt W,kt))}
        else if(oppE) {RIF(canPPIntoTrapTP4C(b,pla,kt E,kt))}
        else          {RIF(canPPIntoTrapTP4C(b,pla,kt N,kt))}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {RIF(canRemoveDef4C(b,pla,kt S,kt))}
        else if(oppW) {RIF(canRemoveDef4C(b,pla,kt W,kt))}
        else if(oppE) {RIF(canRemoveDef4C(b,pla,kt E,kt))}
        else          {RIF(canRemoveDef4C(b,pla,kt N,kt))}
      }
    }
    else //if(defNum == 2)
    {
      if((Board::RADIUS[1][kt] & b.pieceMaps[opp][0] & b.dominatedMap).isEmptyOrSingleBit())
        return false;
      if(((Board::RADIUS[2][kt] | Bitmap::makeLoc(kt)) & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmptyOrSingleBit())
        return false;
      if(oppS)
      {
        if(oppW)
        {RIF(can2PieceAdj(b,pla,kt S,kt W,kt))
         RIF(can2PieceAdj(b,pla,kt W,kt S,kt))}
        else if(oppE)
        {RIF(can2PieceAdj(b,pla,kt S,kt E,kt))
         RIF(can2PieceAdj(b,pla,kt E,kt S,kt))}
        else if(oppN)
        {RIF(can2PieceAdj(b,pla,kt S,kt N,kt))
         RIF(can2PieceAdj(b,pla,kt N,kt S,kt))}
      }
      else if(oppW)
      {
        if(oppE)
        {RIF(can2PieceAdj(b,pla,kt W,kt E,kt))
         RIF(can2PieceAdj(b,pla,kt E,kt W,kt))}
        else if(oppN)
        {RIF(can2PieceAdj(b,pla,kt W,kt N,kt))
         RIF(can2PieceAdj(b,pla,kt N,kt W,kt))}
      }
      else
      {RIF(can2PieceAdj(b,pla,kt E,kt N,kt))
       RIF(can2PieceAdj(b,pla,kt N,kt E,kt))}
    }
  }

  return false;
}

int BoardTrees::genCaps(Board& b, pla_t pla, int steps, int minSteps, loc_t kt,
move_t* mv, int* hm)
{
#ifdef CHECK_CAPTREE_CONSISTENCY
  assert(b.testConsistency(cout));
#endif

  int num = 0;
  pla_t opp = gOpp(pla);

  //Check some quick conditions to prove capture impossible
  if(steps < 2 || steps > 4 ||
      (Board::DISK[1 + (int)(steps == 4)][kt] & b.pieceMaps[opp][0]).isEmpty() ||
      (Board::RADIUS[1][kt] & b.pieceMaps[opp][ELE]).hasBits())
    return 0;

  bool oppS = ISO(kt S);
  bool oppW = ISO(kt W);
  bool oppE = ISO(kt E);
  bool oppN = ISO(kt N);

  int defNum = oppS + oppW + oppE + oppN;
  pla_t trapOwner = b.owners[kt];

  //Check some more quick conditions to prove capture impossible
  if(defNum >= 3 || (steps < 4 && defNum != 1))
    return 0;

  if(steps >= 2 && minSteps <= 2)
  {
    if(defNum == 1 && (Board::RADIUS[1][kt] & b.pieceMaps[opp][0] & b.dominatedMap).hasBits())
    {
      if(trapOwner != opp)
      {
        if     (oppS) {num += genPPIntoTrap2C(b,pla,kt S,kt,mv+num,hm+num,b.pieces[kt S]);}
        else if(oppW) {num += genPPIntoTrap2C(b,pla,kt W,kt,mv+num,hm+num,b.pieces[kt W]);}
        else if(oppE) {num += genPPIntoTrap2C(b,pla,kt E,kt,mv+num,hm+num,b.pieces[kt E]);}
        else          {num += genPPIntoTrap2C(b,pla,kt N,kt,mv+num,hm+num,b.pieces[kt N]);}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {num += genRemoveDef2C(b,pla,kt S,mv+num,hm+num,b.pieces[kt]);}
        else if(oppW) {num += genRemoveDef2C(b,pla,kt W,mv+num,hm+num,b.pieces[kt]);}
        else if(oppE) {num += genRemoveDef2C(b,pla,kt E,mv+num,hm+num,b.pieces[kt]);}
        else          {num += genRemoveDef2C(b,pla,kt N,mv+num,hm+num,b.pieces[kt]);}
      }
    }
    if(num > 0)
    {return num;}
  }

  if(steps >= 3 && minSteps <= 3)
  {
    if(defNum == 1)
    {
      if(trapOwner == NPLA)
      {
        if     (oppS) {num += genPPIntoTrapTE3CC(b,pla,kt S,kt,mv+num,hm+num,b.pieces[kt S]);}
        else if(oppW) {num += genPPIntoTrapTE3CC(b,pla,kt W,kt,mv+num,hm+num,b.pieces[kt W]);}
        else if(oppE) {num += genPPIntoTrapTE3CC(b,pla,kt E,kt,mv+num,hm+num,b.pieces[kt E]);}
        else          {num += genPPIntoTrapTE3CC(b,pla,kt N,kt,mv+num,hm+num,b.pieces[kt N]);}
      }
      else if(trapOwner == pla)
      {
        if     (oppS) {num += genPPIntoTrapTP3C(b,pla,kt S,kt,mv+num,hm+num,b.pieces[kt S]);}
        else if(oppW) {num += genPPIntoTrapTP3C(b,pla,kt W,kt,mv+num,hm+num,b.pieces[kt W]);}
        else if(oppE) {num += genPPIntoTrapTP3C(b,pla,kt E,kt,mv+num,hm+num,b.pieces[kt E]);}
        else          {num += genPPIntoTrapTP3C(b,pla,kt N,kt,mv+num,hm+num,b.pieces[kt N]);}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {num += genRemoveDef3CC(b,pla,kt S,kt,mv+num,hm+num,b.pieces[kt]);}
        else if(oppW) {num += genRemoveDef3CC(b,pla,kt W,kt,mv+num,hm+num,b.pieces[kt]);}
        else if(oppE) {num += genRemoveDef3CC(b,pla,kt E,kt,mv+num,hm+num,b.pieces[kt]);}
        else          {num += genRemoveDef3CC(b,pla,kt N,kt,mv+num,hm+num,b.pieces[kt]);}
      }
    }
    if(num > 0)
    {return num;}
  }

  if(steps >= 4 && minSteps <= 4)
  {
    if(defNum == 0)
    {
      if((Board::RADIUS[2][kt] & b.pieceMaps[opp][0] & b.dominatedMap).isEmpty())
        return 0;

      if(ISO(kt SS))
      {num += gen0PieceTrail(b,pla,kt SS,kt S,kt,mv+num,hm+num,b.pieces[kt SS]);}
      if(ISO(kt WW))
      {num += gen0PieceTrail(b,pla,kt WW,kt W,kt,mv+num,hm+num,b.pieces[kt WW]);}
      if(ISO(kt EE))
      {num += gen0PieceTrail(b,pla,kt EE,kt E,kt,mv+num,hm+num,b.pieces[kt EE]);}
      if(ISO(kt NN))
      {num += gen0PieceTrail(b,pla,kt NN,kt N,kt,mv+num,hm+num,b.pieces[kt NN]);}

      if(ISO(kt SW))
      {num += gen0PieceTrail(b,pla,kt SW,kt S,kt,mv+num,hm+num,b.pieces[kt SW]);
       num += gen0PieceTrail(b,pla,kt SW,kt W,kt,mv+num,hm+num,b.pieces[kt SW]);}
      if(ISO(kt SE))
      {num += gen0PieceTrail(b,pla,kt SE,kt S,kt,mv+num,hm+num,b.pieces[kt SE]);
       num += gen0PieceTrail(b,pla,kt SE,kt E,kt,mv+num,hm+num,b.pieces[kt SE]);}
      if(ISO(kt NW))
      {num += gen0PieceTrail(b,pla,kt NW,kt W,kt,mv+num,hm+num,b.pieces[kt NW]);
       num += gen0PieceTrail(b,pla,kt NW,kt N,kt,mv+num,hm+num,b.pieces[kt NW]);}
      if(ISO(kt NE))
      {num += gen0PieceTrail(b,pla,kt NE,kt E,kt,mv+num,hm+num,b.pieces[kt NE]);
       num += gen0PieceTrail(b,pla,kt NE,kt N,kt,mv+num,hm+num,b.pieces[kt NE]);}
    }
    else if(defNum == 1)
    {
      if(trapOwner == NPLA)
      {
        if     (oppS) {num += genPPIntoTrapTE4C(b,pla,kt S,kt,mv+num,hm+num,b.pieces[kt S]);}
        else if(oppW) {num += genPPIntoTrapTE4C(b,pla,kt W,kt,mv+num,hm+num,b.pieces[kt W]);}
        else if(oppE) {num += genPPIntoTrapTE4C(b,pla,kt E,kt,mv+num,hm+num,b.pieces[kt E]);}
        else          {num += genPPIntoTrapTE4C(b,pla,kt N,kt,mv+num,hm+num,b.pieces[kt N]);}
      }
      else if(trapOwner == pla)
      {
        if     (oppS) {num += genPPIntoTrapTP4C(b,pla,kt S,kt,mv+num,hm+num,b.pieces[kt S]);}
        else if(oppW) {num += genPPIntoTrapTP4C(b,pla,kt W,kt,mv+num,hm+num,b.pieces[kt W]);}
        else if(oppE) {num += genPPIntoTrapTP4C(b,pla,kt E,kt,mv+num,hm+num,b.pieces[kt E]);}
        else          {num += genPPIntoTrapTP4C(b,pla,kt N,kt,mv+num,hm+num,b.pieces[kt N]);}
      }
      else //if(trapOwner == opp)
      {
        if     (oppS) {num += genRemoveDef4C(b,pla,kt S,kt,mv+num,hm+num,b.pieces[kt]);}
        else if(oppW) {num += genRemoveDef4C(b,pla,kt W,kt,mv+num,hm+num,b.pieces[kt]);}
        else if(oppE) {num += genRemoveDef4C(b,pla,kt E,kt,mv+num,hm+num,b.pieces[kt]);}
        else          {num += genRemoveDef4C(b,pla,kt N,kt,mv+num,hm+num,b.pieces[kt]);}
      }
    }
    else //if(defNum == 2)
    {
      if((Board::RADIUS[1][kt] & b.pieceMaps[opp][0] & b.dominatedMap).isEmptyOrSingleBit())
        return 0;
      if(((Board::RADIUS[2][kt] | Bitmap::makeLoc(kt)) & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmptyOrSingleBit())
        return 0;

      if(oppS)
      {
        if(oppW)
        {num += gen2PieceAdj(b,pla,kt S,kt W,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt S]);
         num += gen2PieceAdj(b,pla,kt W,kt S,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt W]);}
        else if(oppE)
        {num += gen2PieceAdj(b,pla,kt S,kt E,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt S]);
         num += gen2PieceAdj(b,pla,kt E,kt S,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt E]);}
        else if(oppN)
        {num += gen2PieceAdj(b,pla,kt S,kt N,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt S]);
         num += gen2PieceAdj(b,pla,kt N,kt S,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt N]);}
      }
      else if(oppW)
      {
        if(oppE)
        {num += gen2PieceAdj(b,pla,kt W,kt E,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt W]);
         num += gen2PieceAdj(b,pla,kt E,kt W,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt E]);}
        else if(oppN)
        {num += gen2PieceAdj(b,pla,kt W,kt N,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt W]);
         num += gen2PieceAdj(b,pla,kt N,kt W,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt N]);}
      }
      else
      {num += gen2PieceAdj(b,pla,kt E,kt N,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt E]);
       num += gen2PieceAdj(b,pla,kt N,kt E,kt,mv+num,hm+num,ISO(kt) ? b.pieces[kt] : b.pieces[kt N]);}
    }

    if(num > 0)
      return num;
  }

  return num;
}

//BOARD UTILITY FUNCTIONS-------------------------------------------------------------

#define PREFIXMOVES(m,i) for(int aa = num; aa < newnum; aa++) {mv[aa] = ((mv[aa] << ((i)*8)) | ((m) & ((1 << ((i)*8))-1)));} num = newnum;
#define SUFFIXMOVES(m,i) for(int aa = num; aa < newnum; aa++) {mv[aa] = (((m) << ((i)*8)) | (mv[aa] & ((1 << ((i)*8))-1)));} num = newnum;

//Checks if a piece could push the enemy on to the trap, if the trap were empty, and any friendly pieces on uf1 or uf2 were unfrozen
static bool couldPushToTrap(Board& b, pla_t pla, loc_t eloc, loc_t kt, loc_t uf1, loc_t uf2)
{
  if(eloc S != kt && ISP(eloc S) && GT(eloc S, eloc) && (eloc S == uf1 || eloc S == uf2 || b.isThawedC(eloc S))) {return true;}
  if(eloc W != kt && ISP(eloc W) && GT(eloc W, eloc) && (eloc W == uf1 || eloc W == uf2 || b.isThawedC(eloc W))) {return true;}
  if(eloc E != kt && ISP(eloc E) && GT(eloc E, eloc) && (eloc E == uf1 || eloc E == uf2 || b.isThawedC(eloc E))) {return true;}
  if(eloc N != kt && ISP(eloc N) && GT(eloc N, eloc) && (eloc N == uf1 || eloc N == uf2 || b.isThawedC(eloc N))) {return true;}

  return false;
}

//Generate all steps of this piece into any open surrounding space
static bool canSteps1S(Board& b, pla_t pla, loc_t k)
{
  if(ISE(k S) && b.isRabOkayS(pla,k)) {return true;}
  if(ISE(k W))                        {return true;}
  if(ISE(k E))                        {return true;}
  if(ISE(k N) && b.isRabOkayN(pla,k)) {return true;}
  return false;
}

//Generate all steps of this piece into any open surrounding space
static int genSteps1S(Board& b, pla_t pla, loc_t k,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISE(k S) && b.isRabOkayS(pla,k)) {ADDMOVEPP(getMove(k MS),hmval)}
  if(ISE(k W))                        {ADDMOVEPP(getMove(k MW),hmval)}
  if(ISE(k E))                        {ADDMOVEPP(getMove(k ME),hmval)}
  if(ISE(k N) && b.isRabOkayN(pla,k)) {ADDMOVEPP(getMove(k MN),hmval)}
  return num;
}

//Can ploc push eloc anywhere?
//Assumes there is a ploc adjacent to eloc unfrozen and big enough.
//Does not push adjacent to to floc
static bool canPushE(Board& b, loc_t eloc, loc_t fadjloc)
{
  if(ISE(eloc S) && (fadjloc == ERRLOC || !b.isAdjacent(eloc S,fadjloc))) {return true;}
  if(ISE(eloc W) && (fadjloc == ERRLOC || !b.isAdjacent(eloc W,fadjloc))) {return true;}
  if(ISE(eloc E) && (fadjloc == ERRLOC || !b.isAdjacent(eloc E,fadjloc))) {return true;}
  if(ISE(eloc N) && (fadjloc == ERRLOC || !b.isAdjacent(eloc N,fadjloc))) {return true;}

  return false;
}

//Can ploc push eloc anywhere?
//Assumes ploc is unfrozen and big enough.
//Does not push on to floc
static int genPushPE(Board& b, loc_t ploc, loc_t eloc, loc_t fadjloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  step_t pstep = gStepSrcDest(ploc,eloc);

  if(ISE(eloc S) && (fadjloc == ERRLOC || !b.isAdjacent(eloc S,fadjloc))) {ADDMOVEPP(getMove(eloc MS,pstep),hmval)}
  if(ISE(eloc W) && (fadjloc == ERRLOC || !b.isAdjacent(eloc W,fadjloc))) {ADDMOVEPP(getMove(eloc MW,pstep),hmval)}
  if(ISE(eloc E) && (fadjloc == ERRLOC || !b.isAdjacent(eloc E,fadjloc))) {ADDMOVEPP(getMove(eloc ME,pstep),hmval)}
  if(ISE(eloc N) && (fadjloc == ERRLOC || !b.isAdjacent(eloc N,fadjloc))) {ADDMOVEPP(getMove(eloc MN,pstep),hmval)}

  return num;
}


//Can ploc pull eloc anywhere?
//Assumes ploc is unfrozen and big enough.
//The can counterpart to this is simply isOpen(ploc)
static int genPullPE(Board& b, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval)
{
  int num = 0;
  step_t estep = gStepSrcDest(eloc,ploc);

  if(ISE(ploc S)) {ADDMOVEPP(getMove(ploc MS,estep),hmval)}
  if(ISE(ploc W)) {ADDMOVEPP(getMove(ploc MW,estep),hmval)}
  if(ISE(ploc E)) {ADDMOVEPP(getMove(ploc ME,estep),hmval)}
  if(ISE(ploc N)) {ADDMOVEPP(getMove(ploc MN,estep),hmval)}

  return num;
}

//Can this piece be pushed by anthing?
static bool canPushc(Board& b, pla_t pla, loc_t eloc)
{
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc W)) {return true;}
    if(ISE(eloc E)) {return true;}
    if(ISE(eloc N)) {return true;}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc S)) {return true;}
    if(ISE(eloc E)) {return true;}
    if(ISE(eloc N)) {return true;}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc S)) {return true;}
    if(ISE(eloc W)) {return true;}
    if(ISE(eloc N)) {return true;}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc S)) {return true;}
    if(ISE(eloc W)) {return true;}
    if(ISE(eloc E)) {return true;}
  }
  return false;
}

//Can this piece be pushed by anthing?
static int genPush(Board& b, pla_t pla, loc_t eloc, move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc W)) {ADDMOVEPP(getMove(eloc MW,eloc S MN),hmval)}
    if(ISE(eloc E)) {ADDMOVEPP(getMove(eloc ME,eloc S MN),hmval)}
    if(ISE(eloc N)) {ADDMOVEPP(getMove(eloc MN,eloc S MN),hmval)}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc S)) {ADDMOVEPP(getMove(eloc MS,eloc W ME),hmval)}
    if(ISE(eloc E)) {ADDMOVEPP(getMove(eloc ME,eloc W ME),hmval)}
    if(ISE(eloc N)) {ADDMOVEPP(getMove(eloc MN,eloc W ME),hmval)}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc S)) {ADDMOVEPP(getMove(eloc MS,eloc E MW),hmval)}
    if(ISE(eloc W)) {ADDMOVEPP(getMove(eloc MW,eloc E MW),hmval)}
    if(ISE(eloc N)) {ADDMOVEPP(getMove(eloc MN,eloc E MW),hmval)}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc S)) {ADDMOVEPP(getMove(eloc MS,eloc N MS),hmval)}
    if(ISE(eloc W)) {ADDMOVEPP(getMove(eloc MW,eloc N MS),hmval)}
    if(ISE(eloc E)) {ADDMOVEPP(getMove(eloc ME,eloc N MS),hmval)}
  }
  return num;
}

//Can this piece be pulled by anthing?
static bool canPullc(Board& b, pla_t pla, loc_t eloc)
{
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc SS)) {return true;}
    if(ISE(eloc SW)) {return true;}
    if(ISE(eloc SE)) {return true;}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc SW)) {return true;}
    if(ISE(eloc WW)) {return true;}
    if(ISE(eloc NW)) {return true;}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc SE)) {return true;}
    if(ISE(eloc EE)) {return true;}
    if(ISE(eloc NE)) {return true;}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc NW)) {return true;}
    if(ISE(eloc NE)) {return true;}
    if(ISE(eloc NN)) {return true;}
  }
  return false;
}

//Can this piece be pulled by anthing?
static int genPull(Board& b, pla_t pla, loc_t eloc, move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc SS)) {ADDMOVEPP(getMove(eloc S MS,eloc MS),hmval)}
    if(ISE(eloc SW)) {ADDMOVEPP(getMove(eloc S MW,eloc MS),hmval)}
    if(ISE(eloc SE)) {ADDMOVEPP(getMove(eloc S ME,eloc MS),hmval)}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc SW)) {ADDMOVEPP(getMove(eloc W MS,eloc MW),hmval)}
    if(ISE(eloc WW)) {ADDMOVEPP(getMove(eloc W MW,eloc MW),hmval)}
    if(ISE(eloc NW)) {ADDMOVEPP(getMove(eloc W MN,eloc MW),hmval)}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc SE)) {ADDMOVEPP(getMove(eloc E MS,eloc ME),hmval)}
    if(ISE(eloc EE)) {ADDMOVEPP(getMove(eloc E ME,eloc ME),hmval)}
    if(ISE(eloc NE)) {ADDMOVEPP(getMove(eloc E MN,eloc ME),hmval)}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc NW)) {ADDMOVEPP(getMove(eloc N MW,eloc MN),hmval)}
    if(ISE(eloc NE)) {ADDMOVEPP(getMove(eloc N ME,eloc MN),hmval)}
    if(ISE(eloc NN)) {ADDMOVEPP(getMove(eloc N MN,eloc MN),hmval)}
  }
  return num;
}

//Assumes UF and big enough. Generates all pushes and pulls of eloc by ploc.
static bool canPPPE(Board& b, loc_t ploc, loc_t eloc)
{
  return b.isOpen(ploc) || b.isOpen(eloc);
}

//Does not check if the piece is already unfrozen
//Assumes a player piece at ploc
static bool canUF(Board& b, pla_t pla, loc_t ploc)
{
  if(ISE(ploc S))
  {
    if(ISP(ploc SS) && b.isThawedC(ploc SS) && b.isRabOkayN(pla,ploc SS)) {return true;}
    if(ISP(ploc SW) && b.isThawedC(ploc SW)                             ) {return true;}
    if(ISP(ploc SE) && b.isThawedC(ploc SE)                             ) {return true;}
  }
  if(ISE(ploc W))
  {
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.isRabOkayN(pla,ploc SW)) {return true;}
    if(ISP(ploc WW) && b.isThawedC(ploc WW)                             ) {return true;}
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.isRabOkayS(pla,ploc NW)) {return true;}
  }
  if(ISE(ploc E))
  {
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.isRabOkayN(pla,ploc SE)) {return true;}
    if(ISP(ploc EE) && b.isThawedC(ploc EE)                             ) {return true;}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.isRabOkayS(pla,ploc NE)) {return true;}
  }
  if(ISE(ploc N))
  {
    if(ISP(ploc NW) && b.isThawedC(ploc NW)                             ) {return true;}
    if(ISP(ploc NE) && b.isThawedC(ploc NE)                             ) {return true;}
    if(ISP(ploc NN) && b.isThawedC(ploc NN) && b.isRabOkayS(pla,ploc NN)) {return true;}
  }
  return false;
}

//Does not check if the piece is already unfrozen
//Assumes a player piece at ploc
static int genUF(Board& b, pla_t pla, loc_t ploc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISE(ploc S))
  {
    if(ISP(ploc SS) && b.isThawedC(ploc SS) && b.isRabOkayN(pla,ploc SS)) {ADDMOVEPP(getMove(ploc SS MN),hmval)}
    if(ISP(ploc SW) && b.isThawedC(ploc SW)                             ) {ADDMOVEPP(getMove(ploc SW ME),hmval)}
    if(ISP(ploc SE) && b.isThawedC(ploc SE)                             ) {ADDMOVEPP(getMove(ploc SE MW),hmval)}
  }
  if(ISE(ploc W))
  {
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.isRabOkayN(pla,ploc SW)) {ADDMOVEPP(getMove(ploc SW MN),hmval)}
    if(ISP(ploc WW) && b.isThawedC(ploc WW)                             ) {ADDMOVEPP(getMove(ploc WW ME),hmval)}
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.isRabOkayS(pla,ploc NW)) {ADDMOVEPP(getMove(ploc NW MS),hmval)}
  }
  if(ISE(ploc E))
  {
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.isRabOkayN(pla,ploc SE)) {ADDMOVEPP(getMove(ploc SE MN),hmval)}
    if(ISP(ploc EE) && b.isThawedC(ploc EE)                             ) {ADDMOVEPP(getMove(ploc EE MW),hmval)}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.isRabOkayS(pla,ploc NE)) {ADDMOVEPP(getMove(ploc NE MS),hmval)}
  }
  if(ISE(ploc N))
  {
    if(ISP(ploc NW) && b.isThawedC(ploc NW)                             ) {ADDMOVEPP(getMove(ploc NW ME),hmval)}
    if(ISP(ploc NE) && b.isThawedC(ploc NE)                             ) {ADDMOVEPP(getMove(ploc NE MW),hmval)}
    if(ISP(ploc NN) && b.isThawedC(ploc NN) && b.isRabOkayS(pla,ploc NN)) {ADDMOVEPP(getMove(ploc NN MS),hmval)}
  }
  return num;
}

static bool canUFAt(Board& b, pla_t pla, loc_t loc)
{
  if(ISP(loc S) && b.isThawedC(loc S) && b.isRabOkayN(pla,loc S)) {return true;}
  if(ISP(loc W) && b.isThawedC(loc W)                           ) {return true;}
  if(ISP(loc E) && b.isThawedC(loc E)                           ) {return true;}
  if(ISP(loc N) && b.isThawedC(loc N) && b.isRabOkayS(pla,loc N)) {return true;}
  return false;
}

static int genUFAt(Board& b, pla_t pla, loc_t loc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISP(loc S) && b.isThawedC(loc S) && b.isRabOkayN(pla,loc S)) {ADDMOVEPP(getMove(loc S MN),hmval)}
  if(ISP(loc W) && b.isThawedC(loc W)                           ) {ADDMOVEPP(getMove(loc W ME),hmval)}
  if(ISP(loc E) && b.isThawedC(loc E)                           ) {ADDMOVEPP(getMove(loc E MW),hmval)}
  if(ISP(loc N) && b.isThawedC(loc N) && b.isRabOkayS(pla,loc N)) {ADDMOVEPP(getMove(loc N MS),hmval)}
  return num;
}

//Can unfreeze and then pushpull a piece?
static bool canUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc)
{
  bool suc = false;
  if(ISE(ploc S))
  {
    if(ISP(ploc SS) && b.isThawedC(ploc SS) && b.isRabOkayN(pla,ploc SS)) {TS(ploc SS,ploc S,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc SW) && b.isThawedC(ploc SW)                             ) {TS(ploc SW,ploc S,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc SE) && b.isThawedC(ploc SE)                             ) {TS(ploc SE,ploc S,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISE(ploc W))
  {
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.isRabOkayN(pla,ploc SW)) {TS(ploc SW,ploc W,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc WW) && b.isThawedC(ploc WW)                              ) {TS(ploc WW,ploc W,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.isRabOkayS(pla,ploc NW)) {TS(ploc NW,ploc W,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISE(ploc E))
  {
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.isRabOkayN(pla,ploc SE)) {TS(ploc SE,ploc E,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc EE) && b.isThawedC(ploc EE)                             ) {TS(ploc EE,ploc E,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.isRabOkayS(pla,ploc NE)) {TS(ploc NE,ploc E,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISE(ploc N))
  {
    if(ISP(ploc NW) && b.isThawedC(ploc NW)                             ) {TS(ploc NW,ploc N,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NE) && b.isThawedC(ploc NE)                             ) {TS(ploc NE,ploc N,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NN) && b.isThawedC(ploc NN) && b.isRabOkayS(pla,ploc NN)) {TS(ploc NN,ploc N,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  return false;
}

//Can unfreeze and then pushpull a piece?
static int genUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  bool suc = false;
  int num = 0;
  if(ISE(ploc S))
  {
    if(ISP(ploc SS) && b.isThawedC(ploc SS) && b.isRabOkayN(pla,ploc SS)) {TS(ploc SS,ploc S,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SS MN),hmval)}}
    if(ISP(ploc SW) && b.isThawedC(ploc SW)                             ) {TS(ploc SW,ploc S,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SW ME),hmval)}}
    if(ISP(ploc SE) && b.isThawedC(ploc SE)                             ) {TS(ploc SE,ploc S,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SE MW),hmval)}}
  }
  if(ISE(ploc W))
  {
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.isRabOkayN(pla,ploc SW)) {TS(ploc SW,ploc W,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SW MN),hmval)}}
    if(ISP(ploc WW) && b.isThawedC(ploc WW)                             ) {TS(ploc WW,ploc W,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc WW ME),hmval)}}
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.isRabOkayS(pla,ploc NW)) {TS(ploc NW,ploc W,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NW MS),hmval)}}
  }
  if(ISE(ploc E))
  {
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.isRabOkayN(pla,ploc SE)) {TS(ploc SE,ploc E,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SE MN),hmval)}}
    if(ISP(ploc EE) && b.isThawedC(ploc EE)                             ) {TS(ploc EE,ploc E,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc EE MW),hmval)}}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.isRabOkayS(pla,ploc NE)) {TS(ploc NE,ploc E,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NE MS),hmval)}}
  }
  if(ISE(ploc N))
  {
    if(ISP(ploc NW) && b.isThawedC(ploc NW)                             ) {TS(ploc NW,ploc N,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NW ME),hmval)}}
    if(ISP(ploc NE) && b.isThawedC(ploc NE)                             ) {TS(ploc NE,ploc N,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NE MW),hmval)}}
    if(ISP(ploc NN) && b.isThawedC(ploc NN) && b.isRabOkayS(pla,ploc NN)) {TS(ploc NN,ploc N,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NN MS),hmval)}}
  }
  return num;
}

static bool canUF2(Board& b, pla_t pla, loc_t ploc)
{
  pla_t opp = gOpp(pla);
  //Safe to skip owners checks since all adjacent pieces are either 0 or controlled by enemy.
  int numStronger =
  (CS1(ploc) && GT(ploc S,ploc)) +
  (CW1(ploc) && GT(ploc W,ploc)) +
  (CE1(ploc) && GT(ploc E,ploc)) +
  (CN1(ploc) && GT(ploc N,ploc));

  if(numStronger == 1)
  {
    loc_t eloc;
         if(CS1(ploc) && GT(ploc S,ploc)) {eloc = ploc S;}
    else if(CW1(ploc) && GT(ploc W,ploc)) {eloc = ploc W;}
    else if(CE1(ploc) && GT(ploc E,ploc)) {eloc = ploc E;}
    else                                  {eloc = ploc N;}

    if(canPullc(b,pla,eloc))
    {return true;}

    if(!b.isTrapSafe2(opp,eloc))
    {
      if     (ISO(eloc S)) {RIF(canPPCapAndUFc(b,pla,eloc S,ploc))}
      else if(ISO(eloc W)) {RIF(canPPCapAndUFc(b,pla,eloc W,ploc))}
      else if(ISO(eloc E)) {RIF(canPPCapAndUFc(b,pla,eloc E,ploc))}
      else if(ISO(eloc N)) {RIF(canPPCapAndUFc(b,pla,eloc N,ploc))}
    }
  }

  if(CS1(ploc) && canGetPlaTo2S(b,pla,ploc S,ploc)) {return true;}
  if(CW1(ploc) && canGetPlaTo2S(b,pla,ploc W,ploc)) {return true;}
  if(CE1(ploc) && canGetPlaTo2S(b,pla,ploc E,ploc)) {return true;}
  if(CN1(ploc) && canGetPlaTo2S(b,pla,ploc N,ploc)) {return true;}

  return false;
}

static int genUF2(Board& b, pla_t pla, loc_t ploc, move_t* mv, int* hm, int hmval)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  //Safe to skip owners checks since all adjacent pieces are either 0 or controlled by enemy.
  int numStronger =
  (CS1(ploc) && GT(ploc S,ploc)) +
  (CW1(ploc) && GT(ploc W,ploc)) +
  (CE1(ploc) && GT(ploc E,ploc)) +
  (CN1(ploc) && GT(ploc N,ploc));

  //Try to pull the freezing piece away
  if(numStronger == 1)
  {
    loc_t eloc;
         if(CS1(ploc) && GT(ploc S,ploc)) {eloc = ploc S;}
    else if(CW1(ploc) && GT(ploc W,ploc)) {eloc = ploc W;}
    else if(CE1(ploc) && GT(ploc E,ploc)) {eloc = ploc E;}
    else                                  {eloc = ploc N;}

    num += genPull(b,pla,eloc,mv+num,hm+num,hmval);

    if(!b.isTrapSafe2(opp,eloc))
    {
      if     (ISO(eloc S)) {num += genPPCapAndUF(b,pla,eloc S,ploc,mv+num,hm+num,hmval);}
      else if(ISO(eloc W)) {num += genPPCapAndUF(b,pla,eloc W,ploc,mv+num,hm+num,hmval);}
      else if(ISO(eloc E)) {num += genPPCapAndUF(b,pla,eloc E,ploc,mv+num,hm+num,hmval);}
      else if(ISO(eloc N)) {num += genPPCapAndUF(b,pla,eloc N,ploc,mv+num,hm+num,hmval);}
    }
  }

  //Empty squares : move in a piece, which might be 2 steps away or frozen and one step away
  //Opponent squares, push.
  if(CS1(ploc)) {num += genGetPlaTo2S(b,pla,ploc S,ploc,mv+num,hm+num,hmval);}
  if(CW1(ploc)) {num += genGetPlaTo2S(b,pla,ploc W,ploc,mv+num,hm+num,hmval);}
  if(CE1(ploc)) {num += genGetPlaTo2S(b,pla,ploc E,ploc,mv+num,hm+num,hmval);}
  if(CN1(ploc)) {num += genGetPlaTo2S(b,pla,ploc N,ploc,mv+num,hm+num,hmval);}

  return num;
}

static bool canPPCapAndUFc(Board& b, pla_t pla, loc_t eloc, loc_t ploc)
{
  bool suc = false;
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc W)) {TPC(eloc S,eloc,eloc W,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc E)) {TPC(eloc S,eloc,eloc E,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc N)) {TPC(eloc S,eloc,eloc N,suc = b.isThawedC(ploc)) RIF(suc)}

    if(ISE(eloc SS)) {TPC(eloc,eloc S,eloc SS,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc SW)) {TPC(eloc,eloc S,eloc SW,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc SE)) {TPC(eloc,eloc S,eloc SE,suc = b.isThawedC(ploc)) RIF(suc)}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc S)) {TPC(eloc W,eloc,eloc S,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc E)) {TPC(eloc W,eloc,eloc E,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc N)) {TPC(eloc W,eloc,eloc N,suc = b.isThawedC(ploc)) RIF(suc)}

    if(ISE(eloc SW)) {TPC(eloc,eloc W,eloc SW,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc WW)) {TPC(eloc,eloc W,eloc WW,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc NW)) {TPC(eloc,eloc W,eloc NW,suc = b.isThawedC(ploc)) RIF(suc)}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc S)) {TPC(eloc E,eloc,eloc S,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc W)) {TPC(eloc E,eloc,eloc W,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc N)) {TPC(eloc E,eloc,eloc N,suc = b.isThawedC(ploc)) RIF(suc)}

    if(ISE(eloc SE)) {TPC(eloc,eloc E,eloc SE,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc EE)) {TPC(eloc,eloc E,eloc EE,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc NE)) {TPC(eloc,eloc E,eloc NE,suc = b.isThawedC(ploc)) RIF(suc)}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc S)) {TPC(eloc N,eloc,eloc S,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc W)) {TPC(eloc N,eloc,eloc W,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc E)) {TPC(eloc N,eloc,eloc E,suc = b.isThawedC(ploc)) RIF(suc)}

    if(ISE(eloc NW)) {TPC(eloc,eloc N,eloc NW,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc NE)) {TPC(eloc,eloc N,eloc NE,suc = b.isThawedC(ploc)) RIF(suc)}
    if(ISE(eloc NN)) {TPC(eloc,eloc N,eloc NN,suc = b.isThawedC(ploc)) RIF(suc)}
  }
  return false;
}


static int genPPCapAndUF(Board& b, pla_t pla, loc_t eloc, loc_t ploc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  bool suc = false;
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc W)) {TPC(eloc S,eloc,eloc W,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MW,eloc S MN),hmval)}}
    if(ISE(eloc E)) {TPC(eloc S,eloc,eloc E,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc ME,eloc S MN),hmval)}}
    if(ISE(eloc N)) {TPC(eloc S,eloc,eloc N,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MN,eloc S MN),hmval)}}

    if(ISE(eloc SS)) {TPC(eloc,eloc S,eloc SS,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc S MS,eloc MS),hmval)}}
    if(ISE(eloc SW)) {TPC(eloc,eloc S,eloc SW,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc S MW,eloc MS),hmval)}}
    if(ISE(eloc SE)) {TPC(eloc,eloc S,eloc SE,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc S ME,eloc MS),hmval)}}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc S)) {TPC(eloc W,eloc,eloc S,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MS,eloc W ME),hmval)}}
    if(ISE(eloc E)) {TPC(eloc W,eloc,eloc E,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc ME,eloc W ME),hmval)}}
    if(ISE(eloc N)) {TPC(eloc W,eloc,eloc N,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MN,eloc W ME),hmval)}}

    if(ISE(eloc SW)) {TPC(eloc,eloc W,eloc SW,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc W MS,eloc MW),hmval)}}
    if(ISE(eloc WW)) {TPC(eloc,eloc W,eloc WW,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc W MW,eloc MW),hmval)}}
    if(ISE(eloc NW)) {TPC(eloc,eloc W,eloc NW,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc W MN,eloc MW),hmval)}}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc S)) {TPC(eloc E,eloc,eloc S,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MS,eloc E MW),hmval)}}
    if(ISE(eloc W)) {TPC(eloc E,eloc,eloc W,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MW,eloc E MW),hmval)}}
    if(ISE(eloc N)) {TPC(eloc E,eloc,eloc N,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MN,eloc E MW),hmval)}}

    if(ISE(eloc SE)) {TPC(eloc,eloc E,eloc SE,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc E MS,eloc ME),hmval)}}
    if(ISE(eloc EE)) {TPC(eloc,eloc E,eloc EE,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc E ME,eloc ME),hmval)}}
    if(ISE(eloc NE)) {TPC(eloc,eloc E,eloc NE,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc E MN,eloc ME),hmval)}}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc S)) {TPC(eloc N,eloc,eloc S,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MS,eloc N MS),hmval)}}
    if(ISE(eloc W)) {TPC(eloc N,eloc,eloc W,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc MW,eloc N MS),hmval)}}
    if(ISE(eloc E)) {TPC(eloc N,eloc,eloc E,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc ME,eloc N MS),hmval)}}

    if(ISE(eloc NW)) {TPC(eloc,eloc N,eloc NW,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc N MW,eloc MN),hmval)}}
    if(ISE(eloc NE)) {TPC(eloc,eloc N,eloc NE,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc N ME,eloc MN),hmval)}}
    if(ISE(eloc NN)) {TPC(eloc,eloc N,eloc NN,suc = b.isThawedC(ploc)) if(suc) {ADDMOVEPP(getMove(eloc N MN,eloc MN),hmval)}}
  }

  return num;
}

static bool canGetPlaTo2S(Board& b, pla_t pla, loc_t k, loc_t floc)
{
  pla_t opp = gOpp(pla);
  if(ISE(k))
  {
    if     (k S != floc && ISP(k S) && b.isFrozen(k S) && b.isRabOkayN(pla,k S)) {if(canUF(b,pla,k S)) {return true;}}
    else if(k S != floc && ISE(k S) && b.isTrapSafe2(pla,k S))                   {if(canStepStepc(b,pla,k S,k)) {return true;}}

    if     (k W != floc && ISP(k W) && b.isFrozen(k W))                          {if(canUF(b,pla,k W)) {return true;}}
    else if(k W != floc && ISE(k W) && b.isTrapSafe2(pla,k W))                   {if(canStepStepc(b,pla,k W,k)) {return true;}}

    if     (k E != floc && ISP(k E) && b.isFrozen(k E))                          {if(canUF(b,pla,k E)) {return true;}}
    else if(k E != floc && ISE(k E) && b.isTrapSafe2(pla,k E))                   {if(canStepStepc(b,pla,k E,k)) {return true;}}

    if     (k N != floc && ISP(k N) && b.isFrozen(k N) && b.isRabOkayS(pla,k N)) {if(canUF(b,pla,k N)) {return true;}}
    else if(k N != floc && ISE(k N) && b.isTrapSafe2(pla,k N))                   {if(canStepStepc(b,pla,k N,k)) {return true;}}
  }
  else if(ISO(k)) {if(canPushc(b,pla,k)) {return true;}}

  return false;
}

static int genGetPlaTo2S(Board& b, pla_t pla, loc_t k, loc_t floc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  if(ISE(k))
  {
    if     (k S != floc && ISP(k S) && b.isFrozen(k S) && b.isRabOkayN(pla,k S)) {num += genUF(b,pla,k S,mv+num,hm+num,hmval);}
    else if(k S != floc && ISE(k S) && b.isTrapSafe2(pla,k S))                   {num += genStepStep(b,pla,k S,k,mv+num,hm+num,hmval);}

    if     (k W != floc && ISP(k W) && b.isFrozen(k W))                          {num += genUF(b,pla,k W,mv+num,hm+num,hmval);}
    else if(k W != floc && ISE(k W) && b.isTrapSafe2(pla,k W))                   {num += genStepStep(b,pla,k W,k,mv+num,hm+num,hmval);}

    if     (k E != floc && ISP(k E) && b.isFrozen(k E))                          {num += genUF(b,pla,k E,mv+num,hm+num,hmval);}
    else if(k E != floc && ISE(k E) && b.isTrapSafe2(pla,k E))                   {num += genStepStep(b,pla,k E,k,mv+num,hm+num,hmval);}

    if     (k N != floc && ISP(k N) && b.isFrozen(k N) && b.isRabOkayS(pla,k N)) {num += genUF(b,pla,k N,mv+num,hm+num,hmval);}
    else if(k N != floc && ISE(k N) && b.isTrapSafe2(pla,k N))                   {num += genStepStep(b,pla,k N,k,mv+num,hm+num,hmval);}
  }
  else if(ISO(k)) {num += genPush(b,pla,k,mv+num,hm+num,hmval);}

  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canUFUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t plocdest, loc_t ploc2, loc_t eloc)
{
  bool suc = false;

  //This includes sacrifical unfreezing (ending on a trap square, so that when the unfrozen piece moves,
  //the unfreezing piece dies).
  //Note that currently, hmval for sacs is penalized in [filterSuicides] above.
  if(ISE(ploc S) && ploc S != plocdest)
  {
    if(ISP(ploc SS) && b.isThawed(ploc SS) && b.isRabOkayN(pla,ploc SS)) {TS(ploc SS,ploc S,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc SW) && b.isThawed(ploc SW)                             ) {TS(ploc SW,ploc S,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc SE) && b.isThawed(ploc SE)                             ) {TS(ploc SE,ploc S,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
  }
  if(ISE(ploc W) && ploc W != plocdest)
  {
    if(ISP(ploc SW) && b.isThawed(ploc SW) && b.isRabOkayN(pla,ploc SW)) {TS(ploc SW,ploc W,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc WW) && b.isThawed(ploc WW)                             ) {TS(ploc WW,ploc W,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc NW) && b.isThawed(ploc NW) && b.isRabOkayS(pla,ploc NW)) {TS(ploc NW,ploc W,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
  }
  if(ISE(ploc E) && ploc E != plocdest)
  {
    if(ISP(ploc SE) && b.isThawed(ploc SE) && b.isRabOkayN(pla,ploc SE)) {TS(ploc SE,ploc E,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc EE) && b.isThawed(ploc EE)                             ) {TS(ploc EE,ploc E,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc NE) && b.isThawed(ploc NE) && b.isRabOkayS(pla,ploc NE)) {TS(ploc NE,ploc E,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
  }
  if(ISE(ploc N) && ploc N != plocdest)
  {
    if(ISP(ploc NW) && b.isThawed(ploc NW)                             ) {TS(ploc NW,ploc N,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc NE) && b.isThawed(ploc NE)                             ) {TS(ploc NE,ploc N,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
    if(ISP(ploc NN) && b.isThawed(ploc NN) && b.isRabOkayS(pla,ploc NN)) {TS(ploc NN,ploc N,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {return true;}}
  }
  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genUFUFPPPE(Board& b, pla_t pla, loc_t ploc, loc_t plocdest, loc_t ploc2, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  bool suc = false;

  //This includes sacrifical unfreezing (ending on a trap square, so that when the unfrozen piece moves,
  //the unfreezing piece dies).
  //Note that currently, hmval for sacs is penalized in [filterSuicides] above.
  if(ISE(ploc S) && ploc S != plocdest)
  {
    if(ISP(ploc SS) && b.isThawed(ploc SS) && b.isRabOkayN(pla,ploc SS)) {TS(ploc SS,ploc S,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc SS MN),hmval)}}
    if(ISP(ploc SW) && b.isThawed(ploc SW)                             ) {TS(ploc SW,ploc S,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc SW ME),hmval)}}
    if(ISP(ploc SE) && b.isThawed(ploc SE)                             ) {TS(ploc SE,ploc S,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc SE MW),hmval)}}
  }
  if(ISE(ploc W) && ploc W != plocdest)
  {
    if(ISP(ploc SW) && b.isThawed(ploc SW) && b.isRabOkayN(pla,ploc SW)) {TS(ploc SW,ploc W,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc SW MN),hmval)}}
    if(ISP(ploc WW) && b.isThawed(ploc WW)                             ) {TS(ploc WW,ploc W,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc WW ME),hmval)}}
    if(ISP(ploc NW) && b.isThawed(ploc NW) && b.isRabOkayS(pla,ploc NW)) {TS(ploc NW,ploc W,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc NW MS),hmval)}}
  }
  if(ISE(ploc E) && ploc E != plocdest)
  {
    if(ISP(ploc SE) && b.isThawed(ploc SE) && b.isRabOkayN(pla,ploc SE)) {TS(ploc SE,ploc E,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc SE MN),hmval)}}
    if(ISP(ploc EE) && b.isThawed(ploc EE)                             ) {TS(ploc EE,ploc E,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc EE MW),hmval)}}
    if(ISP(ploc NE) && b.isThawed(ploc NE) && b.isRabOkayS(pla,ploc NE)) {TS(ploc NE,ploc E,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc NE MS),hmval)}}
  }
  if(ISE(ploc N) && ploc N != plocdest)
  {
    if(ISP(ploc NW) && b.isThawed(ploc NW)                             ) {TS(ploc NW,ploc N,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc NW ME),hmval)}}
    if(ISP(ploc NE) && b.isThawed(ploc NE)                             ) {TS(ploc NE,ploc N,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc NE MW),hmval)}}
    if(ISP(ploc NN) && b.isThawed(ploc NN) && b.isRabOkayS(pla,ploc NN)) {TS(ploc NN,ploc N,TS(ploc,plocdest,suc = canPPPE(b,ploc2,eloc))) if(suc) {ADDMOVEPP(getMove(ploc NN MS),hmval)}}
  }

  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canStepStepPPPE(Board& b, pla_t pla, loc_t dest, loc_t dest2, loc_t ploc, loc_t eloc)
{
  bool suc = false;

  if((dest N == dest2 && pla == 0) || (dest S == dest2 && pla == 1))
  {
    if(ISP(dest S) && b.isThawed(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.pieces[dest S] != RAB) {TS(dest S,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
    if(ISP(dest W) && b.isThawed(dest W) && b.wouldBeUF(pla,dest W,dest,dest W) && b.pieces[dest W] != RAB) {TS(dest W,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
    if(ISP(dest E) && b.isThawed(dest E) && b.wouldBeUF(pla,dest E,dest,dest E) && b.pieces[dest E] != RAB) {TS(dest E,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
    if(ISP(dest N) && b.isThawed(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.pieces[dest N] != RAB) {TS(dest N,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
  }
  else
  {
    if(ISP(dest S) && b.isThawed(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.isRabOkayN(pla,dest S)) {TS(dest S,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
    if(ISP(dest W) && b.isThawed(dest W) && b.wouldBeUF(pla,dest W,dest,dest W))                             {TS(dest W,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
    if(ISP(dest E) && b.isThawed(dest E) && b.wouldBeUF(pla,dest E,dest,dest E))                             {TS(dest E,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
    if(ISP(dest N) && b.isThawed(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.isRabOkayS(pla,dest N)) {TS(dest N,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {return true;}}
  }
  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genStepStepPPPE(Board& b, pla_t pla, loc_t dest, loc_t dest2, loc_t ploc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  bool suc = false;

  if((dest N == dest2 && pla == 0) || (dest S == dest2 && pla == 1))
  {
    if(ISP(dest S) && b.isThawed(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.pieces[dest S] != RAB) {TS(dest S,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest S MN),hmval)}}
    if(ISP(dest W) && b.isThawed(dest W) && b.wouldBeUF(pla,dest W,dest,dest W) && b.pieces[dest W] != RAB) {TS(dest W,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest W ME),hmval)}}
    if(ISP(dest E) && b.isThawed(dest E) && b.wouldBeUF(pla,dest E,dest,dest E) && b.pieces[dest E] != RAB) {TS(dest E,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest E MW),hmval)}}
    if(ISP(dest N) && b.isThawed(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.pieces[dest N] != RAB) {TS(dest N,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest N MS),hmval)}}
  }
  else
  {
    if(ISP(dest S) && b.isThawed(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.isRabOkayN(pla,dest S)) {TS(dest S,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest S MN),hmval)}}
    if(ISP(dest W) && b.isThawed(dest W) && b.wouldBeUF(pla,dest W,dest,dest W))                             {TS(dest W,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest W ME),hmval)}}
    if(ISP(dest E) && b.isThawed(dest E) && b.wouldBeUF(pla,dest E,dest,dest E))                             {TS(dest E,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest E MW),hmval)}}
    if(ISP(dest N) && b.isThawed(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.isRabOkayS(pla,dest N)) {TS(dest N,dest, TS(dest,dest2,suc = canPPPE(b,ploc,eloc))) if(suc) {ADDMOVEPP(getMove(dest N MS),hmval)}}
  }

  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canPushPPPE(Board& b, pla_t pla, loc_t floc, loc_t ploc, loc_t eloc)
{
  bool suc = false;

  if(ISP(floc S) && GT(floc S,floc) && b.isThawed(floc S))
  {
    if(ISE(floc W)) {TP(floc S,floc,floc W,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc E)) {TP(floc S,floc,floc E,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc N)) {TP(floc S,floc,floc N,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISP(floc W) && GT(floc W,floc) && b.isThawed(floc W))
  {
    if(ISE(floc S)) {TP(floc W,floc,floc S,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc E)) {TP(floc W,floc,floc E,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc N)) {TP(floc W,floc,floc N,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISP(floc E) && GT(floc E,floc) && b.isThawed(floc E))
  {
    if(ISE(floc S)) {TP(floc E,floc,floc S,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc W)) {TP(floc E,floc,floc W,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc N)) {TP(floc E,floc,floc N,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISP(floc N) && GT(floc N,floc) && b.isThawed(floc N))
  {
    if(ISE(floc S)) {TP(floc N,floc,floc S,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc W)) {TP(floc N,floc,floc W,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
    if(ISE(floc E)) {TP(floc N,floc,floc E,suc = canPPPE(b,ploc,eloc)) if(suc) {return true;}}
  }
  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genPushPPPE(Board& b, pla_t pla, loc_t floc, loc_t ploc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  bool suc = false;

  if(ISP(floc S) && GT(floc S,floc) && b.isThawed(floc S))
  {
    if(ISE(floc W)) {TP(floc S,floc,floc W,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MW,floc S MN),hmval)}}
    if(ISE(floc E)) {TP(floc S,floc,floc E,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc ME,floc S MN),hmval)}}
    if(ISE(floc N)) {TP(floc S,floc,floc N,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MN,floc S MN),hmval)}}
  }
  if(ISP(floc W) && GT(floc W,floc) && b.isThawed(floc W))
  {
    if(ISE(floc S)) {TP(floc W,floc,floc S,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MS,floc W ME),hmval)}}
    if(ISE(floc E)) {TP(floc W,floc,floc E,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc ME,floc W ME),hmval)}}
    if(ISE(floc N)) {TP(floc W,floc,floc N,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MN,floc W ME),hmval)}}
  }
  if(ISP(floc E) && GT(floc E,floc) && b.isThawed(floc E))
  {
    if(ISE(floc S)) {TP(floc E,floc,floc S,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MS,floc E MW),hmval)}}
    if(ISE(floc W)) {TP(floc E,floc,floc W,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MW,floc E MW),hmval)}}
    if(ISE(floc N)) {TP(floc E,floc,floc N,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MN,floc E MW),hmval)}}
  }
  if(ISP(floc N) && GT(floc N,floc) && b.isThawed(floc N))
  {
    if(ISE(floc S)) {TP(floc N,floc,floc S,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MS,floc N MS),hmval)}}
    if(ISE(floc W)) {TP(floc N,floc,floc W,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc MW,floc N MS),hmval)}}
    if(ISE(floc E)) {TP(floc N,floc,floc E,suc = canPPPE(b,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(floc ME,floc N MS),hmval)}}
  }
  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canUF2PPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc)
{
  pla_t opp = gOpp(pla);
  //Safe to skip owners checks since all adjacent pieces are either 0 or controlled by enemy.
  int numStronger =
  (CS1(ploc) && GT(ploc S,ploc)) +
  (CW1(ploc) && GT(ploc W,ploc)) +
  (CE1(ploc) && GT(ploc E,ploc)) +
  (CN1(ploc) && GT(ploc N,ploc));

  //Try to pull the freezing piece away
  if(numStronger == 1)
  {
    loc_t floc;
         if(CS1(ploc) && GT(ploc S,ploc)) {floc = ploc S;}
    else if(CW1(ploc) && GT(ploc W,ploc)) {floc = ploc W;}
    else if(CE1(ploc) && GT(ploc E,ploc)) {floc = ploc E;}
    else                                  {floc = ploc N;}

    if(canPullc(b,pla,floc))
    {return true;}

    if(!b.isTrapSafe2(opp,floc))
    {
      if     (ISO(floc S)) {RIF(canPPCapAndUFc(b,pla,floc S,ploc))}
      else if(ISO(floc W)) {RIF(canPPCapAndUFc(b,pla,floc W,ploc))}
      else if(ISO(floc E)) {RIF(canPPCapAndUFc(b,pla,floc E,ploc))}
      else if(ISO(floc N)) {RIF(canPPCapAndUFc(b,pla,floc N,ploc))}
    }
  }

  //Empty squares : move in a piece, which might be 2 steps away or frozen and one step away
  //Opponent squares, push.
  if(CS1(ploc) && canGetPlaTo2SPPPE(b,pla,ploc S,ploc,eloc)) {return true;}
  if(CW1(ploc) && canGetPlaTo2SPPPE(b,pla,ploc W,ploc,eloc)) {return true;}
  if(CE1(ploc) && canGetPlaTo2SPPPE(b,pla,ploc E,ploc,eloc)) {return true;}
  if(CN1(ploc) && canGetPlaTo2SPPPE(b,pla,ploc N,ploc,eloc)) {return true;}

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genUF2PPPE(Board& b, pla_t pla, loc_t ploc, loc_t eloc, move_t* mv, int* hm, int hmval)
{
  int num = 0;
  pla_t opp = gOpp(pla);

  //Safe to skip owners checks since all adjacent pieces are either 0 or controlled by enemy.
  int numStronger =
  (CS1(ploc) && GT(ploc S,ploc)) +
  (CW1(ploc) && GT(ploc W,ploc)) +
  (CE1(ploc) && GT(ploc E,ploc)) +
  (CN1(ploc) && GT(ploc N,ploc));

  //Try to pull the freezing piece away
  if(numStronger == 1)
  {
    loc_t floc;
         if(CS1(ploc) && GT(ploc S,ploc)) {floc = ploc S;}
    else if(CW1(ploc) && GT(ploc W,ploc)) {floc = ploc W;}
    else if(CE1(ploc) && GT(ploc E,ploc)) {floc = ploc E;}
    else                                  {floc = ploc N;}

    num += genPull(b,pla,floc,mv+num,hm+num,hmval);

    if(!b.isTrapSafe2(opp,floc))
    {
      if     (ISO(floc S)) {num += genPPCapAndUF(b,pla,floc S,ploc,mv+num,hm+num,hmval);}
      else if(ISO(floc W)) {num += genPPCapAndUF(b,pla,floc W,ploc,mv+num,hm+num,hmval);}
      else if(ISO(floc E)) {num += genPPCapAndUF(b,pla,floc E,ploc,mv+num,hm+num,hmval);}
      else if(ISO(floc N)) {num += genPPCapAndUF(b,pla,floc N,ploc,mv+num,hm+num,hmval);}
    }
  }

  //Empty squares : move in a piece, which might be 2 steps away or frozen and one step away
  //Opponent squares, push.
  if(CS1(ploc)) {num += genGetPlaTo2SPPPE(b,pla,ploc S,ploc,eloc,mv+num,hm+num,hmval);}
  if(CW1(ploc)) {num += genGetPlaTo2SPPPE(b,pla,ploc W,ploc,eloc,mv+num,hm+num,hmval);}
  if(CE1(ploc)) {num += genGetPlaTo2SPPPE(b,pla,ploc E,ploc,eloc,mv+num,hm+num,hmval);}
  if(CN1(ploc)) {num += genGetPlaTo2SPPPE(b,pla,ploc N,ploc,eloc,mv+num,hm+num,hmval);}

  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canGetPlaTo2SPPPE(Board& b, pla_t pla, loc_t k, loc_t ploc, loc_t eloc)
{
  pla_t opp = gOpp(pla);
  if(ISE(k))
  {
    if     (k S != ploc && ISP(k S) && b.isFrozen(k S) && b.isRabOkayN(pla,k S)) {if(canUFUFPPPE(b,pla,k S,k,ploc,eloc)) {return true;}}
    else if(k S != ploc && ISE(k S) && b.isTrapSafe2(pla,k S))                   {if(canStepStepPPPE(b,pla,k S,k,ploc,eloc)) {return true;}}

    if     (k W != ploc && ISP(k W) && b.isFrozen(k W))                          {if(canUFUFPPPE(b,pla,k W,k,ploc,eloc)) {return true;}}
    else if(k W != ploc && ISE(k W) && b.isTrapSafe2(pla,k W))                   {if(canStepStepPPPE(b,pla,k W,k,ploc,eloc)) {return true;}}

    if     (k E != ploc && ISP(k E) && b.isFrozen(k E))                          {if(canUFUFPPPE(b,pla,k E,k,ploc,eloc)) {return true;}}
    else if(k E != ploc && ISE(k E) && b.isTrapSafe2(pla,k E))                   {if(canStepStepPPPE(b,pla,k E,k,ploc,eloc)) {return true;}}

    if     (k N != ploc && ISP(k N) && b.isFrozen(k N) && b.isRabOkayS(pla,k N)) {if(canUFUFPPPE(b,pla,k N,k,ploc,eloc)) {return true;}}
    else if(k N != ploc && ISE(k N) && b.isTrapSafe2(pla,k N))                   {if(canStepStepPPPE(b,pla,k N,k,ploc,eloc)) {return true;}}
  }
  else if(ISO(k)) {if(canPushPPPE(b,pla,k,ploc,eloc)) {return true;}}

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genGetPlaTo2SPPPE(Board& b, pla_t pla, loc_t k, loc_t ploc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  if(ISE(k))
  {
    if     (k S != ploc && ISP(k S) && b.isFrozen(k S) && b.isRabOkayN(pla,k S)) {num += genUFUFPPPE(b,pla,k S,k,ploc,eloc,mv+num,hm+num,hmval);}
    else if(k S != ploc && ISE(k S) && b.isTrapSafe2(pla,k S))                   {num += genStepStepPPPE(b,pla,k S,k,ploc,eloc,mv+num,hm+num,hmval);}

    if     (k W != ploc && ISP(k W) && b.isFrozen(k W))                          {num += genUFUFPPPE(b,pla,k W,k,ploc,eloc,mv+num,hm+num,hmval);}
    else if(k W != ploc && ISE(k W) && b.isTrapSafe2(pla,k W))                   {num += genStepStepPPPE(b,pla,k W,k,ploc,eloc,mv+num,hm+num,hmval);}

    if     (k E != ploc && ISP(k E) && b.isFrozen(k E))                          {num += genUFUFPPPE(b,pla,k E,k,ploc,eloc,mv+num,hm+num,hmval);}
    else if(k E != ploc && ISE(k E) && b.isTrapSafe2(pla,k E))                   {num += genStepStepPPPE(b,pla,k E,k,ploc,eloc,mv+num,hm+num,hmval);}

    if     (k N != ploc && ISP(k N) && b.isFrozen(k N) && b.isRabOkayS(pla,k N)) {num += genUFUFPPPE(b,pla,k N,k,ploc,eloc,mv+num,hm+num,hmval);}
    else if(k N != ploc && ISE(k N) && b.isTrapSafe2(pla,k N))                   {num += genStepStepPPPE(b,pla,k N,k,ploc,eloc,mv+num,hm+num,hmval);}
  }
  else if(ISO(k)) {num += genPushPPPE(b,pla,k,ploc,eloc,mv+num,hm+num,hmval);}

  return num;
}

//Can push eloc at all?
static bool canPP(Board& b, pla_t pla, loc_t eloc)
{
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc W)  || ISE(eloc E)  || ISE(eloc N) ||
       ISE(eloc SW) || ISE(eloc SE) || ISE(eloc SS))
    {return true;}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc S)  || ISE(eloc E)  || ISE(eloc N) ||
       ISE(eloc SW) || ISE(eloc NW) || ISE(eloc WW))
    {return true;}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc S)  || ISE(eloc W)  || ISE(eloc N) ||
       ISE(eloc SE) || ISE(eloc NE) || ISE(eloc EE))
    {return true;}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc S)  || ISE(eloc W) || ISE(eloc E) ||
       ISE(eloc NW) || ISE(eloc NE) || ISE(eloc NN))
    {return true;}
  }
  return false;
}

//Can push eloc at all?
static int genPP(Board& b, pla_t pla, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S))
  {
    if(ISE(eloc W)) {ADDMOVEPP(getMove(eloc MW, eloc S MN),hmval)}
    if(ISE(eloc E)) {ADDMOVEPP(getMove(eloc ME, eloc S MN),hmval)}
    if(ISE(eloc N)) {ADDMOVEPP(getMove(eloc MN, eloc S MN),hmval)}

    if(ISE(eloc SS)) {ADDMOVEPP(getMove(eloc S MS, eloc MS),hmval)}
    if(ISE(eloc SW)) {ADDMOVEPP(getMove(eloc S MW, eloc MS),hmval)}
    if(ISE(eloc SE)) {ADDMOVEPP(getMove(eloc S ME, eloc MS),hmval)}
  }
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W))
  {
    if(ISE(eloc S)) {ADDMOVEPP(getMove(eloc MS, eloc W ME),hmval)}
    if(ISE(eloc E)) {ADDMOVEPP(getMove(eloc ME, eloc W ME),hmval)}
    if(ISE(eloc N)) {ADDMOVEPP(getMove(eloc MN, eloc W ME),hmval)}

    if(ISE(eloc SW)) {ADDMOVEPP(getMove(eloc W MS, eloc MW),hmval)}
    if(ISE(eloc WW)) {ADDMOVEPP(getMove(eloc W MW, eloc MW),hmval)}
    if(ISE(eloc NW)) {ADDMOVEPP(getMove(eloc W MN, eloc MW),hmval)}
  }
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E))
  {
    if(ISE(eloc S)) {ADDMOVEPP(getMove(eloc MS, eloc E MW),hmval)}
    if(ISE(eloc W)) {ADDMOVEPP(getMove(eloc MW, eloc E MW),hmval)}
    if(ISE(eloc N)) {ADDMOVEPP(getMove(eloc MN, eloc E MW),hmval)}

    if(ISE(eloc SE)) {ADDMOVEPP(getMove(eloc E MS, eloc ME),hmval)}
    if(ISE(eloc EE)) {ADDMOVEPP(getMove(eloc E ME, eloc ME),hmval)}
    if(ISE(eloc NE)) {ADDMOVEPP(getMove(eloc E MN, eloc ME),hmval)}
  }
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N))
  {
    if(ISE(eloc S)) {ADDMOVEPP(getMove(eloc MS, eloc N MS),hmval)}
    if(ISE(eloc W)) {ADDMOVEPP(getMove(eloc MW, eloc N MS),hmval)}
    if(ISE(eloc E)) {ADDMOVEPP(getMove(eloc ME, eloc N MS),hmval)}

    if(ISE(eloc NW)) {ADDMOVEPP(getMove(eloc N MW, eloc MN),hmval)}
    if(ISE(eloc NE)) {ADDMOVEPP(getMove(eloc N ME, eloc MN),hmval)}
    if(ISE(eloc NN)) {ADDMOVEPP(getMove(eloc N MN, eloc MN),hmval)}
  }
  return num;
}

//Is there some stronger piece adjancent such that we can get
//eloc to dest in a pushpull?
//Dest can be occupied by a player or an enemy piece.
static bool canPPTo(Board& b, pla_t pla, loc_t eloc, loc_t dest)
{
  if(ISE(dest))
  {
    return
    (ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S)) ||
    (ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W)) ||
    (ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E)) ||
    (ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N));
  }
  else if(ISP(dest) && GT(dest,eloc) && b.isThawedC(dest))
  {
    return
    (ISE(dest S)) ||
    (ISE(dest W)) ||
    (ISE(dest E)) ||
    (ISE(dest N));
  }
  return false;
}

//Is there some stronger piece adjacent such that we can get
//eloc to dest in a pushpull?
//Dest can be occupied by a player or an enemy piece.
static int genPPTo(Board& b, pla_t pla, loc_t eloc, loc_t dest,
move_t* mv, int* hm, int hmval)
{
  step_t estep = gStepSrcDest(eloc,dest);
  int num = 0;

  //Pushing
  if(ISE(dest))
  {
    if(ISP(eloc S) && GT(eloc S, eloc) && b.isThawedC(eloc S)) {ADDMOVEPP(getMove(estep,eloc S MN),hmval)}
    if(ISP(eloc W) && GT(eloc W, eloc) && b.isThawedC(eloc W)) {ADDMOVEPP(getMove(estep,eloc W ME),hmval)}
    if(ISP(eloc E) && GT(eloc E, eloc) && b.isThawedC(eloc E)) {ADDMOVEPP(getMove(estep,eloc E MW),hmval)}
    if(ISP(eloc N) && GT(eloc N, eloc) && b.isThawedC(eloc N)) {ADDMOVEPP(getMove(estep,eloc N MS),hmval)}
  }
  //Pulling
  else if(ISP(dest) && GT(dest,eloc) && b.isThawedC(dest))
  {
    if(ISE(dest S)) {ADDMOVEPP(getMove(dest MS,estep),hmval)}
    if(ISE(dest W)) {ADDMOVEPP(getMove(dest MW,estep),hmval)}
    if(ISE(dest E)) {ADDMOVEPP(getMove(dest ME,estep),hmval)}
    if(ISE(dest N)) {ADDMOVEPP(getMove(dest MN,estep),hmval)}
  }
  return num;
}

//Can we move piece stronger than eloc adjacent to dest so that the piece would be unfrozen there, but not a move to a forbidden location?
//Assumes that dest is empty or opponent
//Assumes that dest is in the center.
static bool canMoveStrongAdjCent(Board& b, pla_t pla, loc_t dest, loc_t eloc, loc_t forbidden)
{
  if(ISE(dest S) && dest S != forbidden)
  {
    if(ISP(dest SS) && GT(dest SS,eloc) && b.isThawedC(dest SS) && b.wouldBeUF(pla,dest SS,dest S,dest SS)) {return true;}
    if(ISP(dest SW) && GT(dest SW,eloc) && b.isThawedC(dest SW) && b.wouldBeUF(pla,dest SW,dest S,dest SW)) {return true;}
    if(ISP(dest SE) && GT(dest SE,eloc) && b.isThawedC(dest SE) && b.wouldBeUF(pla,dest SE,dest S,dest SE)) {return true;}
  }
  if(ISE(dest W) && dest W != forbidden)
  {
    if(ISP(dest SW) && GT(dest SW,eloc) && b.isThawedC(dest SW) && b.wouldBeUF(pla,dest SW,dest W,dest SW)) {return true;}
    if(ISP(dest WW) && GT(dest WW,eloc) && b.isThawedC(dest WW) && b.wouldBeUF(pla,dest WW,dest W,dest WW)) {return true;}
    if(ISP(dest NW) && GT(dest NW,eloc) && b.isThawedC(dest NW) && b.wouldBeUF(pla,dest NW,dest W,dest NW)) {return true;}
  }
  if(ISE(dest E) && dest E != forbidden)
  {
    if(ISP(dest SE) && GT(dest SE,eloc) && b.isThawedC(dest SE) && b.wouldBeUF(pla,dest SE,dest E,dest SE)) {return true;}
    if(ISP(dest EE) && GT(dest EE,eloc) && b.isThawedC(dest EE) && b.wouldBeUF(pla,dest EE,dest E,dest EE)) {return true;}
    if(ISP(dest NE) && GT(dest NE,eloc) && b.isThawedC(dest NE) && b.wouldBeUF(pla,dest NE,dest E,dest NE)) {return true;}
  }
  if(ISE(dest N) && dest N != forbidden)
  {
    if(ISP(dest NW) && GT(dest NW,eloc) && b.isThawedC(dest NW) && b.wouldBeUF(pla,dest NW,dest N,dest NW)) {return true;}
    if(ISP(dest NE) && GT(dest NE,eloc) && b.isThawedC(dest NE) && b.wouldBeUF(pla,dest NE,dest N,dest NE)) {return true;}
    if(ISP(dest NN) && GT(dest NN,eloc) && b.isThawedC(dest NN) && b.wouldBeUF(pla,dest NN,dest N,dest NN)) {return true;}
  }
  return false;
}

//Can we move piece stronger than eloc adjacent to dest so that the piece would be unfrozen there, but not a move to a forbidden location?
//Assumes that dest is empty or opponent
//Assumes that dest is in the center.
static int genMoveStrongAdjCent(Board& b, pla_t pla, loc_t dest, loc_t eloc, loc_t forbidden,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISE(dest S) && dest S != forbidden)
  {
    if(ISP(dest SS) && GT(dest SS,eloc) && b.isThawedC(dest SS) && b.wouldBeUF(pla,dest SS,dest S,dest SS)) {ADDMOVE(getMove(dest SS MN),hmval)}
    if(ISP(dest SW) && GT(dest SW,eloc) && b.isThawedC(dest SW) && b.wouldBeUF(pla,dest SW,dest S,dest SW)) {ADDMOVE(getMove(dest SW ME),hmval)}
    if(ISP(dest SE) && GT(dest SE,eloc) && b.isThawedC(dest SE) && b.wouldBeUF(pla,dest SE,dest S,dest SE)) {ADDMOVE(getMove(dest SE MW),hmval)}
  }
  if(ISE(dest W) && dest W != forbidden)
  {
    if(ISP(dest SW) && GT(dest SW,eloc) && b.isThawedC(dest SW) && b.wouldBeUF(pla,dest SW,dest W,dest SW)) {ADDMOVE(getMove(dest SW MN),hmval)}
    if(ISP(dest WW) && GT(dest WW,eloc) && b.isThawedC(dest WW) && b.wouldBeUF(pla,dest WW,dest W,dest WW)) {ADDMOVE(getMove(dest WW ME),hmval)}
    if(ISP(dest NW) && GT(dest NW,eloc) && b.isThawedC(dest NW) && b.wouldBeUF(pla,dest NW,dest W,dest NW)) {ADDMOVE(getMove(dest NW MS),hmval)}
  }
  if(ISE(dest E) && dest E != forbidden)
  {
    if(ISP(dest SE) && GT(dest SE,eloc) && b.isThawedC(dest SE) && b.wouldBeUF(pla,dest SE,dest E,dest SE)) {ADDMOVE(getMove(dest SE MN),hmval)}
    if(ISP(dest EE) && GT(dest EE,eloc) && b.isThawedC(dest EE) && b.wouldBeUF(pla,dest EE,dest E,dest EE)) {ADDMOVE(getMove(dest EE MW),hmval)}
    if(ISP(dest NE) && GT(dest NE,eloc) && b.isThawedC(dest NE) && b.wouldBeUF(pla,dest NE,dest E,dest NE)) {ADDMOVE(getMove(dest NE MS),hmval)}
  }
  if(ISE(dest N) && dest N != forbidden)
  {
    if(ISP(dest NW) && GT(dest NW,eloc) && b.isThawedC(dest NW) && b.wouldBeUF(pla,dest NW,dest N,dest NW)) {ADDMOVE(getMove(dest NW ME),hmval)}
    if(ISP(dest NE) && GT(dest NE,eloc) && b.isThawedC(dest NE) && b.wouldBeUF(pla,dest NE,dest N,dest NE)) {ADDMOVE(getMove(dest NE MW),hmval)}
    if(ISP(dest NN) && GT(dest NN,eloc) && b.isThawedC(dest NN) && b.wouldBeUF(pla,dest NN,dest N,dest NN)) {ADDMOVE(getMove(dest NN MS),hmval)}
  }
  return num;
}

//Can we move piece stronger than eloc to dest so that the piece would be unfrozen there?
//Assumes that dest is empty or opponent
//Assumes that dest is in the center.
static bool canMoveStrongTo(Board& b, pla_t pla, loc_t dest, loc_t eloc)
{
  int num = 0;
  if(ISP(dest S) && GT(dest S,eloc) && b.isThawedC(dest S) && b.wouldBeUF(pla,dest S,dest,dest S)) return true;
  if(ISP(dest W) && GT(dest W,eloc) && b.isThawedC(dest W) && b.wouldBeUF(pla,dest W,dest,dest W)) return true;
  if(ISP(dest E) && GT(dest E,eloc) && b.isThawedC(dest E) && b.wouldBeUF(pla,dest E,dest,dest E)) return true;
  if(ISP(dest N) && GT(dest N,eloc) && b.isThawedC(dest N) && b.wouldBeUF(pla,dest N,dest,dest N)) return true;
  return num;
}

//Can we move piece stronger than eloc to dest so that the piece would be unfrozen there?
//Assumes that dest is empty or opponent
//Assumes that dest is in the center.
static int genMoveStrongToAndThen(Board& b, pla_t pla, loc_t dest, loc_t eloc, move_t move,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  if(ISP(dest S) && GT(dest S,eloc) && b.isThawedC(dest S) && b.wouldBeUF(pla,dest S,dest,dest S)) {ADDMOVE(preConcatMove(dest S MN,move),hmval)}
  if(ISP(dest W) && GT(dest W,eloc) && b.isThawedC(dest W) && b.wouldBeUF(pla,dest W,dest,dest W)) {ADDMOVE(preConcatMove(dest W ME,move),hmval)}
  if(ISP(dest E) && GT(dest E,eloc) && b.isThawedC(dest E) && b.wouldBeUF(pla,dest E,dest,dest E)) {ADDMOVE(preConcatMove(dest E MW,move),hmval)}
  if(ISP(dest N) && GT(dest N,eloc) && b.isThawedC(dest N) && b.wouldBeUF(pla,dest N,dest,dest N)) {ADDMOVE(preConcatMove(dest N MS,move),hmval)}
  return num;
}


static bool canAdvanceUFStep(Board& b, pla_t pla, loc_t curploc, loc_t futploc)
{
  if(!b.isTrapSafe2(pla,curploc))
  {return false;}

  if(futploc == curploc S)
  {
    if(ISP(curploc W) && ISE(futploc W) && b.isTrapSafe2(pla,futploc W) && b.wouldBeUF(pla,curploc,curploc,curploc W) && b.isRabOkayS(pla,curploc W)) {return true;}
    if(ISP(curploc E) && ISE(futploc E) && b.isTrapSafe2(pla,futploc E) && b.wouldBeUF(pla,curploc,curploc,curploc E) && b.isRabOkayS(pla,curploc E)) {return true;}
  }
  else if(futploc == curploc W)
  {
    if(ISP(curploc S) && ISE(futploc S) && b.isTrapSafe2(pla,futploc S) && b.wouldBeUF(pla,curploc,curploc,curploc S)) {return true;}
    if(ISP(curploc N) && ISE(futploc N) && b.isTrapSafe2(pla,futploc N) && b.wouldBeUF(pla,curploc,curploc,curploc N)) {return true;}
  }
  else if(futploc == curploc E)
  {
    if(ISP(curploc S) && ISE(futploc S) && b.isTrapSafe2(pla,futploc S) && b.wouldBeUF(pla,curploc,curploc,curploc S)) {return true;}
    if(ISP(curploc N) && ISE(futploc N) && b.isTrapSafe2(pla,futploc N) && b.wouldBeUF(pla,curploc,curploc,curploc N)) {return true;}
  }
  else if(futploc == curploc N)
  {
    if(ISP(curploc W) && ISE(futploc W) && b.isTrapSafe2(pla,futploc W) && b.wouldBeUF(pla,curploc,curploc,curploc W) && b.isRabOkayN(pla,curploc W)) {return true;}
    if(ISP(curploc E) && ISE(futploc E) && b.isTrapSafe2(pla,futploc E) && b.wouldBeUF(pla,curploc,curploc,curploc E) && b.isRabOkayN(pla,curploc E)) {return true;}
  }
  return false;
}

static int genAdvanceUFStep(Board& b, pla_t pla, loc_t curploc, loc_t futploc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;

  if(!b.isTrapSafe2(pla,curploc))
  {return 0;}

  if(futploc == curploc S)
  {
    if(ISP(curploc W) && ISE(futploc W) && b.isTrapSafe2(pla,futploc W) && b.wouldBeUF(pla,curploc,curploc,curploc W) && b.isRabOkayS(pla,curploc W)) {ADDMOVEPP(getMove(curploc W MS),hmval)}
    if(ISP(curploc E) && ISE(futploc E) && b.isTrapSafe2(pla,futploc E) && b.wouldBeUF(pla,curploc,curploc,curploc E) && b.isRabOkayS(pla,curploc E)) {ADDMOVEPP(getMove(curploc E MS),hmval)}
  }
  else if(futploc == curploc W)
  {
    if(ISP(curploc S) && ISE(futploc S) && b.isTrapSafe2(pla,futploc S) && b.wouldBeUF(pla,curploc,curploc,curploc S)) {ADDMOVEPP(getMove(curploc S MW),hmval)}
    if(ISP(curploc N) && ISE(futploc N) && b.isTrapSafe2(pla,futploc N) && b.wouldBeUF(pla,curploc,curploc,curploc N)) {ADDMOVEPP(getMove(curploc N MW),hmval)}
  }
  else if(futploc == curploc E)
  {
    if(ISP(curploc S) && ISE(futploc S) && b.isTrapSafe2(pla,futploc S) && b.wouldBeUF(pla,curploc,curploc,curploc S)) {ADDMOVEPP(getMove(curploc S ME),hmval)}
    if(ISP(curploc N) && ISE(futploc N) && b.isTrapSafe2(pla,futploc N) && b.wouldBeUF(pla,curploc,curploc,curploc N)) {ADDMOVEPP(getMove(curploc N ME),hmval)}
  }
  else if(futploc == curploc N)
  {
    if(ISP(curploc W) && ISE(futploc W) && b.isTrapSafe2(pla,futploc W) && b.wouldBeUF(pla,curploc,curploc,curploc W) && b.isRabOkayN(pla,curploc W)) {ADDMOVEPP(getMove(curploc W MN),hmval)}
    if(ISP(curploc E) && ISE(futploc E) && b.isTrapSafe2(pla,futploc E) && b.wouldBeUF(pla,curploc,curploc,curploc E) && b.isRabOkayN(pla,curploc E)) {ADDMOVEPP(getMove(curploc E MN),hmval)}
  }
  return num;
}

static bool canUFStepWouldBeUF(Board& b, pla_t pla, loc_t ploc, loc_t wloc, loc_t kt)
{
  int meNum = ISP(kt S) + ISP(kt W) + ISP(kt E) + ISP(kt N);

  if(ISE(ploc S) && ploc S != wloc && (ploc S != kt || meNum <= 2))
  {
    if(ISP(ploc SS) && b.isThawedC(ploc SS) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SS) && b.isRabOkayN(pla,ploc SS)) {return true;}
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SW)                             ) {return true;}
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SE)                             ) {return true;}
  }
  if(ISE(ploc W) && ploc W != wloc && (ploc W != kt || meNum <= 2))
  {
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SW) && b.isRabOkayN(pla,ploc SW)) {return true;}
    if(ISP(ploc WW) && b.isThawedC(ploc WW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc WW)                             ) {return true;}
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NW) && b.isRabOkayS(pla,ploc NW)) {return true;}
  }
  if(ISE(ploc E) && ploc E != wloc && (ploc E != kt || meNum <= 2))
  {
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SE) && b.isRabOkayN(pla,ploc SE)) {return true;}
    if(ISP(ploc EE) && b.isThawedC(ploc EE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc EE)                             ) {return true;}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NE) && b.isRabOkayS(pla,ploc NE)) {return true;}
  }
  if(ISE(ploc N) && ploc N != wloc && (ploc N != kt || meNum <= 2))
  {
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NW)                             ) {return true;}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NE)                             ) {return true;}
    if(ISP(ploc NN) && b.isThawedC(ploc NN) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NN) && b.isRabOkayS(pla,ploc NN)) {return true;}
  }
  return false;
}

static int genUFStepWouldBeUF(Board& b, pla_t pla, loc_t ploc, loc_t wloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  int meNum = ISP(kt S) + ISP(kt W) + ISP(kt E) + ISP(kt N);

  //Cannot step on to wLoc, since that would block the piece unfrozen.
  //Can step onto kT only if there is only two defenders or less of the trap, creating a suidical capture. Ex:
  /*
  .C..
  .He.
  X*M.
  .c..
  */

  if(ISE(ploc S) && ploc S != wloc && (ploc S != kt || meNum <= 2))
  {
    if(ISP(ploc SS) && b.isThawedC(ploc SS) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SS) && b.isRabOkayN(pla,ploc SS)) {ADDMOVEPP(getMove(ploc SS MN),hmval)}
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SW)                             ) {ADDMOVEPP(getMove(ploc SW ME),hmval)}
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SE)                             ) {ADDMOVEPP(getMove(ploc SE MW),hmval)}
  }
  if(ISE(ploc W) && ploc W != wloc && (ploc W != kt || meNum <= 2))
  {
    if(ISP(ploc SW) && b.isThawedC(ploc SW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SW) && b.isRabOkayN(pla,ploc SW)) {ADDMOVEPP(getMove(ploc SW MN),hmval)}
    if(ISP(ploc WW) && b.isThawedC(ploc WW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc WW)                             ) {ADDMOVEPP(getMove(ploc WW ME),hmval)}
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NW) && b.isRabOkayS(pla,ploc NW)) {ADDMOVEPP(getMove(ploc NW MS),hmval)}
  }
  if(ISE(ploc E) && ploc E != wloc && (ploc E != kt || meNum <= 2))
  {
    if(ISP(ploc SE) && b.isThawedC(ploc SE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc SE) && b.isRabOkayN(pla,ploc SE)) {ADDMOVEPP(getMove(ploc SE MN),hmval)}
    if(ISP(ploc EE) && b.isThawedC(ploc EE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc EE)                             ) {ADDMOVEPP(getMove(ploc EE MW),hmval)}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NE) && b.isRabOkayS(pla,ploc NE)) {ADDMOVEPP(getMove(ploc NE MS),hmval)}
  }
  if(ISE(ploc N) && ploc N != wloc && (ploc N != kt || meNum <= 2))
  {
    if(ISP(ploc NW) && b.isThawedC(ploc NW) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NW)                             ) {ADDMOVEPP(getMove(ploc NW ME),hmval)}
    if(ISP(ploc NE) && b.isThawedC(ploc NE) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NE)                             ) {ADDMOVEPP(getMove(ploc NE MW),hmval)}
    if(ISP(ploc NN) && b.isThawedC(ploc NN) && b.wouldBeUF(pla,ploc,wloc,ploc,ploc NN) && b.isRabOkayS(pla,ploc NN)) {ADDMOVEPP(getMove(ploc NN MS),hmval)}
  }

  return num;
}

static bool canStepStepc(Board& b, pla_t pla, loc_t dest, loc_t dest2)
{
  if((dest N == dest2 && pla == 0) || (dest S == dest2 && pla == 1))
  {
    if(ISP(dest S) && b.isThawedC(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.pieces[dest S] != RAB) {return true;}
    if(ISP(dest W) && b.isThawedC(dest W) && b.wouldBeUF(pla,dest W,dest,dest W) && b.pieces[dest W] != RAB) {return true;}
    if(ISP(dest E) && b.isThawedC(dest E) && b.wouldBeUF(pla,dest E,dest,dest E) && b.pieces[dest E] != RAB) {return true;}
    if(ISP(dest N) && b.isThawedC(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.pieces[dest N] != RAB) {return true;}
  }
  else
  {
    if(ISP(dest S) && b.isThawedC(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.isRabOkayN(pla,dest S)) {return true;}
    if(ISP(dest W) && b.isThawedC(dest W) && b.wouldBeUF(pla,dest W,dest,dest W))                             {return true;}
    if(ISP(dest E) && b.isThawedC(dest E) && b.wouldBeUF(pla,dest E,dest,dest E))                             {return true;}
    if(ISP(dest N) && b.isThawedC(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.isRabOkayS(pla,dest N)) {return true;}
  }
  return false;
}

static int genStepStep(Board& b, pla_t pla, loc_t dest, loc_t dest2,
move_t* mv, int* hm, int hmval)
{
  int num = 0;

  if((dest N == dest2 && pla == 0) || (dest S == dest2 && pla == 1))
  {
    if(ISP(dest S) && b.isThawedC(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.pieces[dest S] != RAB) {ADDMOVEPP(getMove(dest S MN),hmval)}
    if(ISP(dest W) && b.isThawedC(dest W) && b.wouldBeUF(pla,dest W,dest,dest W) && b.pieces[dest W] != RAB) {ADDMOVEPP(getMove(dest W ME),hmval)}
    if(ISP(dest E) && b.isThawedC(dest E) && b.wouldBeUF(pla,dest E,dest,dest E) && b.pieces[dest E] != RAB) {ADDMOVEPP(getMove(dest E MW),hmval)}
    if(ISP(dest N) && b.isThawedC(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.pieces[dest N] != RAB) {ADDMOVEPP(getMove(dest N MS),hmval)}
  }
  else
  {
    if(ISP(dest S) && b.isThawedC(dest S) && b.wouldBeUF(pla,dest S,dest,dest S) && b.isRabOkayN(pla,dest S)) {ADDMOVEPP(getMove(dest S MN),hmval)}
    if(ISP(dest W) && b.isThawedC(dest W) && b.wouldBeUF(pla,dest W,dest,dest W))                             {ADDMOVEPP(getMove(dest W ME),hmval)}
    if(ISP(dest E) && b.isThawedC(dest E) && b.wouldBeUF(pla,dest E,dest,dest E))                             {ADDMOVEPP(getMove(dest E MW),hmval)}
    if(ISP(dest N) && b.isThawedC(dest N) && b.wouldBeUF(pla,dest N,dest,dest N) && b.isRabOkayS(pla,dest N)) {ADDMOVEPP(getMove(dest N MS),hmval)}
  }
  return num;
}

static bool movingWouldSacPiece(const Board& b, pla_t pla, loc_t ploc)
{
  int idx = Board::ADJACENTTRAPINDEX[ploc];
  if(idx == -1)
    return false;
  return b.trapGuardCounts[pla][idx] == 1 && ISP(Board::ADJACENTTRAP[ploc]);
}

//Find empty square adjacent
static loc_t findOpenToStepPreferEdge(const Board& b, pla_t pla, loc_t loc)
{
  if(gY(loc) < 4)
  {
    if(ISE(loc S) && b.isRabOkayS(pla,loc)) {return loc S;}
    if(gX(loc) < 4)
    {if(ISE(loc W)) {return loc W;}
     if(ISE(loc E)) {return loc E;}}
    else
    {if(ISE(loc E)) {return loc E;}
     if(ISE(loc W)) {return loc W;}}
    if(ISE(loc N) && b.isRabOkayN(pla,loc)) {return loc N;}
  }
  else
  {
    if(ISE(loc N) && b.isRabOkayN(pla,loc)) {return loc N;}
    if(gX(loc) < 4)
    {if(ISE(loc W)) {return loc W;}
     if(ISE(loc E)) {return loc E;}}
    else
    {if(ISE(loc E)) {return loc E;}
     if(ISE(loc W)) {return loc W;}}
    if(ISE(loc S) && b.isRabOkayS(pla,loc)) {return loc S;}
  }
  return ERRLOC;
}

//2 STEP CAPTURES----------------------------------------------------------------------------------------------

static bool canPPIntoTrap2C(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  return canPPTo(b,pla,eloc,kt);
}

static int genPPIntoTrap2C(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  return genPPTo(b,pla,eloc,kt,mv,hm,hmval);
}

static bool canRemoveDef2C(Board& b, pla_t pla, loc_t eloc)
{
  return canPP(b,pla,eloc);
}

static int genRemoveDef2C(Board& b, pla_t pla, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  return genPP(b,pla,eloc, mv, hm, hmval);
}

//3 STEP CAPTURES-----------------------------------------------------------------------------------------------

//Top level version where the board is not tempstepped, allows use of bitmaps
static bool canPPIntoTrapTE3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  if((Board::DISK[2][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return false;
  return canPPIntoTrapTE3C(b,pla,eloc,kt);
}
static bool canPPIntoTrapTE3C(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  //Frozen but can push   //If eloc +/- X is kt, then it will not execute because no player on that trap.
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isFrozenC(eloc S) && canUF(b,pla,eloc S)) {return true;}
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isFrozenC(eloc W) && canUF(b,pla,eloc W)) {return true;}
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isFrozenC(eloc E) && canUF(b,pla,eloc E)) {return true;}
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isFrozenC(eloc N) && canUF(b,pla,eloc N)) {return true;}

  //TODO maybe also include an add-a-defender step, particularly if !wouldBeUF without the sac piece
  //Save an about-to-be-sacrified piece before capturing
  //Note that currently, hmval for sacs is penalized in [filterSuicides] above.
  if(ISP(eloc S) && GT(eloc S,eloc) && movingWouldSacPiece(b,pla,eloc S) && b.wouldBeUF(pla,eloc S,eloc S,Board::ADJACENTTRAP[eloc S])) {loc_t okt = Board::ADJACENTTRAP[eloc S]; if(b.isOpenToStep(okt)) return true;}
  if(ISP(eloc W) && GT(eloc W,eloc) && movingWouldSacPiece(b,pla,eloc W) && b.wouldBeUF(pla,eloc W,eloc W,Board::ADJACENTTRAP[eloc W])) {loc_t okt = Board::ADJACENTTRAP[eloc W]; if(b.isOpenToStep(okt)) return true;}
  if(ISP(eloc E) && GT(eloc E,eloc) && movingWouldSacPiece(b,pla,eloc E) && b.wouldBeUF(pla,eloc E,eloc E,Board::ADJACENTTRAP[eloc E])) {loc_t okt = Board::ADJACENTTRAP[eloc E]; if(b.isOpenToStep(okt)) return true;}
  if(ISP(eloc N) && GT(eloc N,eloc) && movingWouldSacPiece(b,pla,eloc N) && b.wouldBeUF(pla,eloc N,eloc N,Board::ADJACENTTRAP[eloc N])) {loc_t okt = Board::ADJACENTTRAP[eloc N]; if(b.isOpenToStep(okt)) return true;}

  //Step on the trap and pull
  int meNum = b.trapGuardCounts[pla][Board::TRAPINDEX[kt]];
  if(meNum >= 2)
  {
    if(ISP(kt S) && GT(kt S,eloc) && b.isThawedC(kt S)) return true;
    if(ISP(kt W) && GT(kt W,eloc) && b.isThawedC(kt W)) return true;
    if(ISP(kt E) && GT(kt E,eloc) && b.isThawedC(kt E)) return true;
    if(ISP(kt N) && GT(kt N,eloc) && b.isThawedC(kt N)) return true;
  }

  //Step a stronger piece next to it, unfrozen to prep a pp
  //Forbid stepping on the trap since we already handled that case
  if(eloc S != kt && ISE(eloc S) && canMoveStrongTo(b,pla,eloc S,eloc)) return true;
  if(eloc W != kt && ISE(eloc W) && canMoveStrongTo(b,pla,eloc W,eloc)) return true;
  if(eloc E != kt && ISE(eloc E) && canMoveStrongTo(b,pla,eloc E,eloc)) return true;
  if(eloc N != kt && ISE(eloc N) && canMoveStrongTo(b,pla,eloc N,eloc)) return true;

  return false;
}

//Top level version where the board is not tempstepped, allows use of bitmaps
static int genPPIntoTrapTE3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  if((Board::DISK[2][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return 0;
  return genPPIntoTrapTE3C(b,pla,eloc,kt,mv,hm,hmval);
}

static int genPPIntoTrapTE3C(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  int num = 0;

  //Frozen but can push   //If eloc +/- X is kt, then it will not execute because no player on that trap.
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isFrozenC(eloc S)) {num += genUF(b,pla,eloc S,mv+num,hm+num,hmval);}
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isFrozenC(eloc W)) {num += genUF(b,pla,eloc W,mv+num,hm+num,hmval);}
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isFrozenC(eloc E)) {num += genUF(b,pla,eloc E,mv+num,hm+num,hmval);}
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isFrozenC(eloc N)) {num += genUF(b,pla,eloc N,mv+num,hm+num,hmval);}

  //Save an about-to-be-sacrified piece before capturing
  //Note that currently, hmval for sacs is penalized in [filterSuicides] above.
  if(ISP(eloc S) && GT(eloc S,eloc) && movingWouldSacPiece(b,pla,eloc S) && b.wouldBeUF(pla,eloc S,eloc S,Board::ADJACENTTRAP[eloc S])) {loc_t okt = Board::ADJACENTTRAP[eloc S]; loc_t loc = findOpenToStepPreferEdge(b,pla,okt); if(loc != ERRLOC) {ADDMOVE(getMove(gStepSrcDest(okt,loc),gStepSrcDest(eloc,kt),eloc S MN),hmval);}}
  if(ISP(eloc W) && GT(eloc W,eloc) && movingWouldSacPiece(b,pla,eloc W) && b.wouldBeUF(pla,eloc W,eloc W,Board::ADJACENTTRAP[eloc W])) {loc_t okt = Board::ADJACENTTRAP[eloc W]; loc_t loc = findOpenToStepPreferEdge(b,pla,okt); if(loc != ERRLOC) {ADDMOVE(getMove(gStepSrcDest(okt,loc),gStepSrcDest(eloc,kt),eloc W ME),hmval);}}
  if(ISP(eloc E) && GT(eloc E,eloc) && movingWouldSacPiece(b,pla,eloc E) && b.wouldBeUF(pla,eloc E,eloc E,Board::ADJACENTTRAP[eloc E])) {loc_t okt = Board::ADJACENTTRAP[eloc E]; loc_t loc = findOpenToStepPreferEdge(b,pla,okt); if(loc != ERRLOC) {ADDMOVE(getMove(gStepSrcDest(okt,loc),gStepSrcDest(eloc,kt),eloc E MW),hmval);}}
  if(ISP(eloc N) && GT(eloc N,eloc) && movingWouldSacPiece(b,pla,eloc N) && b.wouldBeUF(pla,eloc N,eloc N,Board::ADJACENTTRAP[eloc N])) {loc_t okt = Board::ADJACENTTRAP[eloc N]; loc_t loc = findOpenToStepPreferEdge(b,pla,okt); if(loc != ERRLOC) {ADDMOVE(getMove(gStepSrcDest(okt,loc),gStepSrcDest(eloc,kt),eloc N MS),hmval);}}

  //Step on the trap and pull
  step_t estep = gStepSrcDest(eloc,kt);
  int meNum = b.trapGuardCounts[pla][Board::TRAPINDEX[kt]];
  if(meNum >= 3)
  {
    if     (ISP(kt S) && GT(kt S,eloc) && b.isThawedC(kt S)) {ADDMOVE(getMove(kt S MN, kt MS,estep),hmval);}
    else if(ISP(kt W) && GT(kt W,eloc) && b.isThawedC(kt W)) {ADDMOVE(getMove(kt W ME, kt MW,estep),hmval);}
    else if(ISP(kt E) && GT(kt E,eloc) && b.isThawedC(kt E)) {ADDMOVE(getMove(kt E MW, kt ME,estep),hmval);}
    else if(ISP(kt N) && GT(kt N,eloc) && b.isThawedC(kt N)) {ADDMOVE(getMove(kt N MS, kt MN,estep),hmval);}
  }
  else if(meNum == 2)
  {
    bool foundBase = false;
    loc_t loc = b.findOpen(kt);
    step_t openstep = gStepSrcDest(kt,loc);
    if(ISP(kt S) && GT(kt S,eloc) && b.isThawedC(kt S)) {if(!foundBase) {foundBase = true; ADDMOVE(getMove(kt S MN, kt MS,estep),hmval);} ADDMOVE(getMove(kt S MN, openstep,estep),hmval);}
    if(ISP(kt W) && GT(kt W,eloc) && b.isThawedC(kt W)) {if(!foundBase) {foundBase = true; ADDMOVE(getMove(kt W ME, kt MW,estep),hmval);} ADDMOVE(getMove(kt W ME, openstep,estep),hmval);}
    if(ISP(kt E) && GT(kt E,eloc) && b.isThawedC(kt E)) {if(!foundBase) {foundBase = true; ADDMOVE(getMove(kt E MW, kt ME,estep),hmval);} ADDMOVE(getMove(kt E MW, openstep,estep),hmval);}
    if(ISP(kt N) && GT(kt N,eloc) && b.isThawedC(kt N)) {if(!foundBase) {foundBase = true; ADDMOVE(getMove(kt N MS, kt MN,estep),hmval);} ADDMOVE(getMove(kt N MS, openstep,estep),hmval);}
  }

  //Step a stronger piece next to it, unfrozen to prep a pp
  //Forbid stepping on the trap since we already handled that case
  if(eloc S != kt && ISE(eloc S)) {num += genMoveStrongToAndThen(b,pla,eloc S,eloc,getMove(estep,eloc S MN),mv+num,hm+num,hmval);}
  if(eloc W != kt && ISE(eloc W)) {num += genMoveStrongToAndThen(b,pla,eloc W,eloc,getMove(estep,eloc W ME),mv+num,hm+num,hmval);}
  if(eloc E != kt && ISE(eloc E)) {num += genMoveStrongToAndThen(b,pla,eloc E,eloc,getMove(estep,eloc E MW),mv+num,hm+num,hmval);}
  if(eloc N != kt && ISE(eloc N)) {num += genMoveStrongToAndThen(b,pla,eloc N,eloc,getMove(estep,eloc N MS),mv+num,hm+num,hmval);}

  return num;
}

static bool canPPIntoTrapTP3C(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  //Weak piece on trap
  if(b.pieces[kt] <= b.pieces[eloc])
  {
    //Step off and push on
    if(ISE(kt S) && b.isRabOkayS(pla,kt) && couldPushToTrap(b,pla,eloc,kt,kt SW,kt SE)) {return true;}
    if(ISE(kt W)                         && couldPushToTrap(b,pla,eloc,kt,kt SW,kt NW)) {return true;}
    if(ISE(kt E)                         && couldPushToTrap(b,pla,eloc,kt,kt SE,kt NE)) {return true;}
    if(ISE(kt N) && b.isRabOkayN(pla,kt) && couldPushToTrap(b,pla,eloc,kt,kt NW,kt NE)) {return true;}

    //SUICIDAL
    if(!b.isTrapSafe2(pla,kt))
    {
      if(ISP(kt S) && GT(kt S,eloc))
      {
        if     (ISE(kt SW) && b.isAdjacent(kt SW, eloc) && b.wouldBeUF(pla,kt S,kt SW,kt S)) {return true;}
        else if(ISE(kt SE) && b.isAdjacent(kt SE, eloc) && b.wouldBeUF(pla,kt S,kt SE,kt S)) {return true;}
      }
      else if(ISP(kt W) && GT(kt W,eloc))
      {
        if     (ISE(kt SW) && b.isAdjacent(kt SW, eloc) && b.wouldBeUF(pla,kt W,kt SW,kt W)) {return true;}
        else if(ISE(kt NW) && b.isAdjacent(kt NW, eloc) && b.wouldBeUF(pla,kt W,kt NW,kt W)) {return true;}
      }
      else if(ISP(kt E) && GT(kt E,eloc))
      {
        if     (ISE(kt SE) && b.isAdjacent(kt SE, eloc) && b.wouldBeUF(pla,kt E,kt SE,kt E)) {return true;}
        else if(ISE(kt NE) && b.isAdjacent(kt NE, eloc) && b.wouldBeUF(pla,kt E,kt NE,kt E)) {return true;}
      }
      else if(ISP(kt N) && GT(kt N,eloc))
      {
        if     (ISE(kt NW) && b.isAdjacent(kt NW, eloc) && b.wouldBeUF(pla,kt N,kt NW,kt N)) {return true;}
        else if(ISE(kt NE) && b.isAdjacent(kt NE, eloc) && b.wouldBeUF(pla,kt N,kt NE,kt N)) {return true;}
      }
    }
  }

  //Strong piece, but blocked. Try stepping a piece away.
  //Note that we might be trying 3-step caps despite the existence of 2-step caps.
  //In case we are, it might be the case that there's only one supporting pla piece, so check
  //for it to make sure we aren't just saccing the piece on the trap by stepping away.
  else if(b.trapGuardCounts[pla][Board::TRAPINDEX[kt]] > 1)
  {
    if(ISP(kt S))
    {
      if(ISE(kt SS) && b.isRabOkayS(pla,kt S)) return true;
      if(ISE(kt SW))                           return true;
      if(ISE(kt SE))                           return true;
    }
    if(ISP(kt W))
    {
      if(ISE(kt SW) && b.isRabOkayS(pla,kt W)) return true;
      if(ISE(kt WW))                           return true;
      if(ISE(kt NW) && b.isRabOkayN(pla,kt W)) return true;
    }
    if(ISP(kt E))
    {
      if(ISE(kt SE) && b.isRabOkayS(pla,kt E)) return true;
      if(ISE(kt EE))                           return true;
      if(ISE(kt NE) && b.isRabOkayN(pla,kt E)) return true;
    }
    if(ISP(kt N))
    {
      if(ISE(kt NW))                           return true;
      if(ISE(kt NE))                           return true;
      if(ISE(kt NN) && b.isRabOkayN(pla,kt N)) return true;
    }
  }

  return false;
}

static int genPPIntoTrapTP3C(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  int num = 0;

  //Weak piece on trap
  if(b.pieces[kt] <= b.pieces[eloc])
  {
    //Step off and push on
    if(ISE(kt S) && b.isRabOkayS(pla,kt) && couldPushToTrap(b,pla,eloc,kt,kt SW,kt SE)) {ADDMOVE(getMove(kt MS),hmval)}
    if(ISE(kt W)                         && couldPushToTrap(b,pla,eloc,kt,kt SW,kt NW)) {ADDMOVE(getMove(kt MW),hmval)}
    if(ISE(kt E)                         && couldPushToTrap(b,pla,eloc,kt,kt SE,kt NE)) {ADDMOVE(getMove(kt ME),hmval)}
    if(ISE(kt N) && b.isRabOkayN(pla,kt) && couldPushToTrap(b,pla,eloc,kt,kt NW,kt NE)) {ADDMOVE(getMove(kt MN),hmval)}

    //SUICIDAL
    if(!b.isTrapSafe2(pla,kt))
    {
      if(ISP(kt S) && GT(kt S,eloc))
      {
        if     (ISE(kt SW) && b.isAdjacent(kt SW, eloc) && b.wouldBeUF(pla,kt S,kt SW,kt S)) {ADDMOVE(getMove(kt S MW),hmval)}
        else if(ISE(kt SE) && b.isAdjacent(kt SE, eloc) && b.wouldBeUF(pla,kt S,kt SE,kt S)) {ADDMOVE(getMove(kt S ME),hmval)}
      }
      else if(ISP(kt W) && GT(kt W,eloc))
      {
        if     (ISE(kt SW) && b.isAdjacent(kt SW, eloc) && b.wouldBeUF(pla,kt W,kt SW,kt W)) {ADDMOVE(getMove(kt W MS),hmval)}
        else if(ISE(kt NW) && b.isAdjacent(kt NW, eloc) && b.wouldBeUF(pla,kt W,kt NW,kt W)) {ADDMOVE(getMove(kt W MN),hmval)}
      }
      else if(ISP(kt E) && GT(kt E,eloc))
      {
        if     (ISE(kt SE) && b.isAdjacent(kt SE, eloc) && b.wouldBeUF(pla,kt E,kt SE,kt E)) {ADDMOVE(getMove(kt E MS),hmval)}
        else if(ISE(kt NE) && b.isAdjacent(kt NE, eloc) && b.wouldBeUF(pla,kt E,kt NE,kt E)) {ADDMOVE(getMove(kt E MN),hmval)}
      }
      else if(ISP(kt N) && GT(kt N,eloc))
      {
        if     (ISE(kt NW) && b.isAdjacent(kt NW, eloc) && b.wouldBeUF(pla,kt N,kt NW,kt N)) {ADDMOVE(getMove(kt N MW),hmval)}
        else if(ISE(kt NE) && b.isAdjacent(kt NE, eloc) && b.wouldBeUF(pla,kt N,kt NE,kt N)) {ADDMOVE(getMove(kt N ME),hmval)}
      }
    }
  }

  //Strong piece, but blocked. Try stepping a piece away.
  //Note that we might be trying 3-step caps despite the existence of 2-step caps.
  //In case we are, it might be the case that there's only one supporting pla piece, so check
  //for it to make sure we aren't just saccing the piece on the trap by stepping away.
  else if(b.trapGuardCounts[pla][Board::TRAPINDEX[kt]] > 1)
  {
    step_t estep = gStepSrcDest(eloc,kt);
    if(ISP(kt S))
    {
      if(ISE(kt SS) && b.isRabOkayS(pla,kt S)) {ADDMOVE(getMove(kt S MS,kt MS,estep),hmval)}
      if(ISE(kt SW))                           {ADDMOVE(getMove(kt S MW,kt MS,estep),hmval)}
      if(ISE(kt SE))                           {ADDMOVE(getMove(kt S ME,kt MS,estep),hmval)}
    }
    if(ISP(kt W))
    {
      if(ISE(kt SW) && b.isRabOkayS(pla,kt W)) {ADDMOVE(getMove(kt W MS,kt MW,estep),hmval)}
      if(ISE(kt WW))                           {ADDMOVE(getMove(kt W MW,kt MW,estep),hmval)}
      if(ISE(kt NW) && b.isRabOkayN(pla,kt W)) {ADDMOVE(getMove(kt W MN,kt MW,estep),hmval)}
    }
    if(ISP(kt E))
    {
      if(ISE(kt SE) && b.isRabOkayS(pla,kt E)) {ADDMOVE(getMove(kt E MS,kt ME,estep),hmval)}
      if(ISE(kt EE))                           {ADDMOVE(getMove(kt E ME,kt ME,estep),hmval)}
      if(ISE(kt NE) && b.isRabOkayN(pla,kt E)) {ADDMOVE(getMove(kt E MN,kt ME,estep),hmval)}
    }
    if(ISP(kt N))
    {
      if(ISE(kt NW))                           {ADDMOVE(getMove(kt N MW,kt MN,estep),hmval)}
      if(ISE(kt NE))                           {ADDMOVE(getMove(kt N ME,kt MN,estep),hmval)}
      if(ISE(kt NN) && b.isRabOkayN(pla,kt N)) {ADDMOVE(getMove(kt N MN,kt MN,estep),hmval)}
    }
  }

  return num;
}

//Top level version where the board is not tempstepped, allows use of bitmaps
static bool canRemoveDef3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  if((Board::DISK[2][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return false;
  return canRemoveDef3C(b,pla,eloc,kt);
}

static bool canRemoveDef3C(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  //Frozen but can pushpull. Remove the defender.   //If eloc +/- X is kt, then it will not execute because no player on that trap.
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isFrozenC(eloc S) && canUFPPPE(b,pla,eloc S,eloc)) {return true;}
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isFrozenC(eloc W) && canUFPPPE(b,pla,eloc W,eloc)) {return true;}
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isFrozenC(eloc E) && canUFPPPE(b,pla,eloc E,eloc)) {return true;}
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isFrozenC(eloc N) && canUFPPPE(b,pla,eloc N,eloc)) {return true;}

  //Unfrozen, could pushpull, but stuff is in the way.
  //Step out of the way and push the defender
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S) && canBlockedPP(b,pla,eloc S,eloc)) {return true;}
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W) && canBlockedPP(b,pla,eloc W,eloc)) {return true;}
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E) && canBlockedPP(b,pla,eloc E,eloc)) {return true;}
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N) && canBlockedPP(b,pla,eloc N,eloc)) {return true;}

  //A step away, not going through the trap
  if(canMoveStrongAdjCent(b,pla,eloc,eloc,kt))
  {return true;}

  return false;
}

//Top level version where the board is not tempstepped, allows use of bitmaps
static int genRemoveDef3CC(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  if((Board::DISK[2][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return 0;
  return genRemoveDef3C(b,pla,eloc,kt,mv,hm,hmval);
}

static int genRemoveDef3C(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  int num = 0;

  //Frozen but can pushpull. Remove the defender.   //If eloc +/- X is kt, then it will not execute because no player on that trap.
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isFrozenC(eloc S)) {num += genUFPPPE(b,pla,eloc S,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isFrozenC(eloc W)) {num += genUFPPPE(b,pla,eloc W,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isFrozenC(eloc E)) {num += genUFPPPE(b,pla,eloc E,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isFrozenC(eloc N)) {num += genUFPPPE(b,pla,eloc N,eloc,mv+num,hm+num,hmval);}

  //Unfrozen, could pushpull, but stuff is in the way.
  //Step out of the way and push the defender
  if(ISP(eloc S) && GT(eloc S,eloc) && b.isThawedC(eloc S)) {num += genBlockedPP(b,pla,eloc S,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc W) && GT(eloc W,eloc) && b.isThawedC(eloc W)) {num += genBlockedPP(b,pla,eloc W,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc E) && GT(eloc E,eloc) && b.isThawedC(eloc E)) {num += genBlockedPP(b,pla,eloc E,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc N) && GT(eloc N,eloc) && b.isThawedC(eloc N)) {num += genBlockedPP(b,pla,eloc N,eloc,mv+num,hm+num,hmval);}

  //A step away, not going through the trap
  num += genMoveStrongAdjCent(b,pla,eloc,eloc,kt,mv+num,hm+num,hmval);

  return num;
}


//4-STEP CAPTURES----------------------------------------------------------


//0 DEFENDERS--------------------------------------------------------------

//Does NOT use the C version of thawed or frozen!
static bool can0PieceTrail(Board& b, pla_t pla, loc_t eloc, loc_t tr, loc_t kt)
{
  bool suc = false;

  if(ISE(tr))
  {
    if(ISP(eloc S) && GT(eloc S, eloc) && b.isThawed(eloc S)) {TPC(eloc S,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){return true;}}
    if(ISP(eloc W) && GT(eloc W, eloc) && b.isThawed(eloc W)) {TPC(eloc W,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){return true;}}
    if(ISP(eloc E) && GT(eloc E, eloc) && b.isThawed(eloc E)) {TPC(eloc E,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){return true;}}
    if(ISP(eloc N) && GT(eloc N, eloc) && b.isThawed(eloc N)) {TPC(eloc N,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){return true;}}
  }
  //Pulling
  else if(ISP(tr) && GT(tr,eloc) && b.isThawed(tr))
  {
    loc_t emptyLocs[2];
    int numEmptyLocs = 0;
    if(tr S != kt && ISE(tr S)) {emptyLocs[numEmptyLocs++] = tr S;}
    if(tr W != kt && ISE(tr W)) {emptyLocs[numEmptyLocs++] = tr W;}
    if(tr E != kt && ISE(tr E)) {emptyLocs[numEmptyLocs++] = tr E;}
    if(tr N != kt && ISE(tr N)) {emptyLocs[numEmptyLocs++] = tr N;}

    int trapGuardCounts = b.trapGuardCounts[pla][Board::TRAPINDEX[kt]];

    //Pull and then a pla on the trap pulls
    if(ISP(kt) && trapGuardCounts > 1)
    {
      if(GT(kt,eloc) && numEmptyLocs > 0 && b.isOpen(kt))
        return true;
    }
    //Either the trap is empty or it becomes empty the moment the first pull happens
    else
    {
      //Two empty locs - the only non-through trap captures are flips
      if(numEmptyLocs == 2)
      {
        if     (b.wouldBeUF(pla,tr,emptyLocs[0],tr)) return true;
        else if(b.wouldBeUF(pla,tr,emptyLocs[1],tr)) return true;
      }
      //One empty loc
      else if(numEmptyLocs == 1)
      {
        //Flip
        if(b.wouldBeUF(pla,tr,emptyLocs[0],tr)) return true;

        //Pull and then push by an alternate friendly pla
        if     (tr S != kt && ISP(tr S) && GT(tr S,eloc) && b.wouldBeUF(pla,tr S,tr S,tr)) return true;
        else if(tr W != kt && ISP(tr W) && GT(tr W,eloc) && b.wouldBeUF(pla,tr W,tr W,tr)) return true;
        else if(tr E != kt && ISP(tr E) && GT(tr E,eloc) && b.wouldBeUF(pla,tr E,tr E,tr)) return true;
        else if(tr N != kt && ISP(tr N) && GT(tr N,eloc) && b.wouldBeUF(pla,tr N,tr N,tr)) return true;
      }

      //Pull through the trap
      if(ISE(kt))
      {
        //Pull through the trap
        if(trapGuardCounts > 1)
        {
          if(b.isOpen(kt))
            return true;
        }
        //Pull sacrificing the pulling piece
        //Note that currently, hmval for sacs is penalized in [filterSuicides] above.
        else
        {
          //Look around for alternate pieces to push
          if(tr S != kt && ISP(tr S) && GT(tr S,eloc) && b.wouldBeUF(pla,tr S,tr S,tr)) return true;
          if(tr W != kt && ISP(tr W) && GT(tr W,eloc) && b.wouldBeUF(pla,tr W,tr W,tr)) return true;
          if(tr E != kt && ISP(tr E) && GT(tr E,eloc) && b.wouldBeUF(pla,tr E,tr E,tr)) return true;
          if(tr N != kt && ISP(tr N) && GT(tr N,eloc) && b.wouldBeUF(pla,tr N,tr N,tr)) return true;
        }
      }
    }
  }

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int gen0PieceTrail(Board& b, pla_t pla, loc_t eloc, loc_t tr, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  bool suc = false;
  step_t estep = gStepSrcDest(eloc,tr);

  if(ISE(tr))
  {
    if(ISP(eloc S) && GT(eloc S, eloc) && b.isThawed(eloc S)) {TPC(eloc S,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){ADDMOVEPP(getMove(estep,eloc S MN),hmval)}}
    if(ISP(eloc W) && GT(eloc W, eloc) && b.isThawed(eloc W)) {TPC(eloc W,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){ADDMOVEPP(getMove(estep,eloc W ME),hmval)}}
    if(ISP(eloc E) && GT(eloc E, eloc) && b.isThawed(eloc E)) {TPC(eloc E,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){ADDMOVEPP(getMove(estep,eloc E MW),hmval)}}
    if(ISP(eloc N) && GT(eloc N, eloc) && b.isThawed(eloc N)) {TPC(eloc N,eloc,tr,suc = canPPTo(b,pla,tr,kt)) if(suc){ADDMOVEPP(getMove(estep,eloc N MS),hmval)}}
  }
  //Pulling
  else if(ISP(tr) && GT(tr,eloc) && b.isThawed(tr))
  {
    loc_t emptyLocs[2];
    int numEmptyLocs = 0;
    if(tr S != kt && ISE(tr S)) {emptyLocs[numEmptyLocs++] = tr S;}
    if(tr W != kt && ISE(tr W)) {emptyLocs[numEmptyLocs++] = tr W;}
    if(tr E != kt && ISE(tr E)) {emptyLocs[numEmptyLocs++] = tr E;}
    if(tr N != kt && ISE(tr N)) {emptyLocs[numEmptyLocs++] = tr N;}

    int trapGuardCounts = b.trapGuardCounts[pla][Board::TRAPINDEX[kt]];
    step_t tstep = gStepSrcDest(tr,kt);

    //Pull and then a pla on the trap pulls
    if(ISP(kt) && trapGuardCounts > 1)
    {
      if(GT(kt,eloc))
      {
        for(int i = 0; i<numEmptyLocs; i++)
        {
          loc_t loc = emptyLocs[i];
          if(ISE(kt S)) {ADDMOVEPP(getMove(gStepSrcDest(tr,loc),estep,kt MS,tstep),hmval);}
          if(ISE(kt W)) {ADDMOVEPP(getMove(gStepSrcDest(tr,loc),estep,kt MW,tstep),hmval);}
          if(ISE(kt E)) {ADDMOVEPP(getMove(gStepSrcDest(tr,loc),estep,kt ME,tstep),hmval);}
          if(ISE(kt N)) {ADDMOVEPP(getMove(gStepSrcDest(tr,loc),estep,kt MN,tstep),hmval);}
        }
      }
    }
    //Either the trap is empty or it becomes empty the moment the first pull happens
    else
    {
      //Two empty locs - the only non-through trap captures are flips
      if(numEmptyLocs == 2)
      {
        loc_t loc = ERRLOC;
        if     (b.wouldBeUF(pla,tr,emptyLocs[0],tr)) {loc = emptyLocs[0];}
        else if(b.wouldBeUF(pla,tr,emptyLocs[1],tr)) {loc = emptyLocs[1];}
        if(loc != ERRLOC)
        {ADDMOVEPP(getMove(gStepSrcDest(tr,loc),estep,tstep,gStepSrcDest(loc,tr)),hmval);}
      }
      //One empty loc
      else if(numEmptyLocs == 1)
      {
        loc_t loc = emptyLocs[0];
        step_t pullStep = gStepSrcDest(tr,loc);
        //Flip
        if(b.wouldBeUF(pla,tr,loc,tr)) {ADDMOVEPP(getMove(pullStep,estep,tstep,gStepSrcDest(loc,tr)),hmval);}

        //Pull and then push by an alternate friendly pla
        if     (tr S != kt && ISP(tr S) && GT(tr S,eloc) && b.wouldBeUF(pla,tr S,tr S,tr)) {ADDMOVEPP(getMove(pullStep,estep,tstep,tr S MN),hmval);}
        else if(tr W != kt && ISP(tr W) && GT(tr W,eloc) && b.wouldBeUF(pla,tr W,tr W,tr)) {ADDMOVEPP(getMove(pullStep,estep,tstep,tr W ME),hmval);}
        else if(tr E != kt && ISP(tr E) && GT(tr E,eloc) && b.wouldBeUF(pla,tr E,tr E,tr)) {ADDMOVEPP(getMove(pullStep,estep,tstep,tr E MW),hmval);}
        else if(tr N != kt && ISP(tr N) && GT(tr N,eloc) && b.wouldBeUF(pla,tr N,tr N,tr)) {ADDMOVEPP(getMove(pullStep,estep,tstep,tr N MS),hmval);}
      }

      if(ISE(kt))
      {
        //Pull through the trap
        if(trapGuardCounts > 1)
        {
          if(ISE(kt S)) {ADDMOVEPP(getMove(tstep,estep,kt MS,tstep),hmval);}
          if(ISE(kt W)) {ADDMOVEPP(getMove(tstep,estep,kt MW,tstep),hmval);}
          if(ISE(kt E)) {ADDMOVEPP(getMove(tstep,estep,kt ME,tstep),hmval);}
          if(ISE(kt N)) {ADDMOVEPP(getMove(tstep,estep,kt MN,tstep),hmval);}
        }
        //Pull sacrificing the pulling piece.
        //Note that currently, hmval for sacs is penalized in [filterSuicides] above.
        else
        {
          //Look around for alternate pieces to push
          if(tr S != kt && ISP(tr S) && GT(tr S,eloc) && b.wouldBeUF(pla,tr S,tr S,tr)) {ADDMOVEPP(getMove(tstep,estep,tstep,tr S MN),hmval);}
          if(tr W != kt && ISP(tr W) && GT(tr W,eloc) && b.wouldBeUF(pla,tr W,tr W,tr)) {ADDMOVEPP(getMove(tstep,estep,tstep,tr W ME),hmval);}
          if(tr E != kt && ISP(tr E) && GT(tr E,eloc) && b.wouldBeUF(pla,tr E,tr E,tr)) {ADDMOVEPP(getMove(tstep,estep,tstep,tr E MW),hmval);}
          if(tr N != kt && ISP(tr N) && GT(tr N,eloc) && b.wouldBeUF(pla,tr N,tr N,tr)) {ADDMOVEPP(getMove(tstep,estep,tstep,tr N MS),hmval);}
        }
      }
    }
  }

  return num;
}

//2 DEFENDERS--------------------------------------------------------------

//Does NOT use the C version of thawed or frozen!
static bool can2PieceAdj(Board& b, pla_t pla, loc_t eloc1, loc_t eloc2, loc_t kt)
{
  bool suc = false;

  if(ISE(kt))
  {
    //Push eloc1 into trap, then remove eloc2
    if(ISP(eloc1 S) && GT(eloc1 S, eloc1) && b.isThawed(eloc1 S)) {TP(eloc1 S,eloc1,kt,suc = canPP(b,pla,eloc2)) if(suc){return true;}}
    if(ISP(eloc1 W) && GT(eloc1 W, eloc1) && b.isThawed(eloc1 W)) {TP(eloc1 W,eloc1,kt,suc = canPP(b,pla,eloc2)) if(suc){return true;}}
    if(ISP(eloc1 E) && GT(eloc1 E, eloc1) && b.isThawed(eloc1 E)) {TP(eloc1 E,eloc1,kt,suc = canPP(b,pla,eloc2)) if(suc){return true;}}
    if(ISP(eloc1 N) && GT(eloc1 N, eloc1) && b.isThawed(eloc1 N)) {TP(eloc1 N,eloc1,kt,suc = canPP(b,pla,eloc2)) if(suc){return true;}}

    //Otherwise we need to remove eloc2, then pushpull eloc1 into trap
    //Only try this order if it's potentially necessary. Namely if removing eloc2 could possibly unfreeze
    //some player that can push eloc1. For simplicity and speed, we don't actually check for frozen pieces
    //that could push eloc1, merely just frozen pieces that aren't rabbits.
    Bitmap frozenAdjEloc1 = b.frozenMap & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB] & Board::RADIUS[1][eloc1];
    if(frozenAdjEloc1.isEmpty())
      return false;
    bool eloc2AdjFrozenAdjEloc1 = (Board::RADIUS[1][eloc2] & frozenAdjEloc1).hasBits();

    if(ISP(eloc2 S) && GT(eloc2 S,eloc2) && b.isThawed(eloc2 S))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 W) && eloc2 W != kt) {TP(eloc2 S,eloc2,eloc2 W,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 E) && eloc2 E != kt) {TP(eloc2 S,eloc2,eloc2 E,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 N) && eloc2 N != kt) {TP(eloc2 S,eloc2,eloc2 N,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      }

      if(ISE(eloc2 SS)) {TP(eloc2,eloc2 S,eloc2 SS,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 S,eloc2 SW,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 S,eloc2 SE,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
    }
    if(ISP(eloc2 W) && GT(eloc2 W,eloc2) && b.isThawed(eloc2 W))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 S) && eloc2 S != kt) {TP(eloc2 W,eloc2,eloc2 S,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 E) && eloc2 E != kt) {TP(eloc2 W,eloc2,eloc2 E,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 N) && eloc2 N != kt) {TP(eloc2 W,eloc2,eloc2 N,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      }

      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 W,eloc2 SW,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 WW)) {TP(eloc2,eloc2 W,eloc2 WW,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 W,eloc2 NW,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
    }
    if(ISP(eloc2 E) && GT(eloc2 E,eloc2) && b.isThawed(eloc2 E))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 S) && eloc2 S != kt) {TP(eloc2 E,eloc2,eloc2 S,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 W) && eloc2 W != kt) {TP(eloc2 E,eloc2,eloc2 W,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 N) && eloc2 N != kt) {TP(eloc2 E,eloc2,eloc2 N,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      }

      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 E,eloc2 SE,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 EE)) {TP(eloc2,eloc2 E,eloc2 EE,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 E,eloc2 NE,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
    }
    if(ISP(eloc2 N) && GT(eloc2 N,eloc2) && b.isThawed(eloc2 N))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 S) && eloc2 S != kt) {TP(eloc2 N,eloc2,eloc2 S,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 W) && eloc2 W != kt) {TP(eloc2 N,eloc2,eloc2 W,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
        if(ISE(eloc2 E) && eloc2 E != kt) {TP(eloc2 N,eloc2,eloc2 E,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      }

      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 N,eloc2 NW,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 N,eloc2 NE,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 NN)) {TP(eloc2,eloc2 N,eloc2 NN,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
    }
  }
  else if(ISP(kt))
  {
    //Pull eloc1 into trap, then remove eloc2
    if(GT(kt,eloc1))
    {
      if(ISE(kt S)) {TP(eloc1,kt,kt S,suc = canPP(b,pla,eloc2)) if(suc){return true;}}
      if(ISE(kt W)) {TP(eloc1,kt,kt W,suc = canPP(b,pla,eloc2)) if(suc){return true;}}
      if(ISE(kt E)) {TP(eloc1,kt,kt E,suc = canPP(b,pla,eloc2)) if(suc){return true;}}
      if(ISE(kt N)) {TP(eloc1,kt,kt N,suc = canPP(b,pla,eloc2)) if(suc){return true;}}

      //Remove eloc2 to make room, then pull into trap, TempRemove to make sure kt doesn't pull yet
      TR(kt,suc = canPullc(b,pla,eloc2))
      if(suc) {return true;}
    }
    //Push eloc 2, then push eloc 1 to trap
    if(GT(kt,eloc2))
    {
      if(ISE(eloc2 S)) {TP(kt,eloc2,eloc2 S,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 W)) {TP(kt,eloc2,eloc2 W,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 E)) {TP(kt,eloc2,eloc2 E,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
      if(ISE(eloc2 N)) {TP(kt,eloc2,eloc2 N,suc = canPPTo(b,pla,eloc1,kt)) if(suc){return true;}}
    }
  }
  else //if(ISO(kt))
  {
    //Remove eloc2, then remove eloc1
    if(ISP(eloc2 S) && GT(eloc2 S,eloc2) && b.isThawed(eloc2 S))
    {
      if(ISE(eloc2 W)) {TP(eloc2 S,eloc2,eloc2 W,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 E)) {TP(eloc2 S,eloc2,eloc2 E,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 N)) {TP(eloc2 S,eloc2,eloc2 N,suc = canPP(b,pla,eloc1)) if(suc){return true;}}

      if(ISE(eloc2 SS)) {TP(eloc2,eloc2 S,eloc2 SS,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 S,eloc2 SW,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 S,eloc2 SE,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
    }
    if(ISP(eloc2 W) && GT(eloc2 W,eloc2) && b.isThawed(eloc2 W))
    {
      if(ISE(eloc2 S)) {TP(eloc2 W,eloc2,eloc2 S,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 E)) {TP(eloc2 W,eloc2,eloc2 E,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 N)) {TP(eloc2 W,eloc2,eloc2 N,suc = canPP(b,pla,eloc1)) if(suc){return true;}}

      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 W,eloc2 SW,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 WW)) {TP(eloc2,eloc2 W,eloc2 WW,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 W,eloc2 NW,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
    }
    if(ISP(eloc2 E) && GT(eloc2 E,eloc2) && b.isThawed(eloc2 E))
    {
      if(ISE(eloc2 S)) {TP(eloc2 E,eloc2,eloc2 S,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 W)) {TP(eloc2 E,eloc2,eloc2 W,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 N)) {TP(eloc2 E,eloc2,eloc2 N,suc = canPP(b,pla,eloc1)) if(suc){return true;}}

      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 E,eloc2 SE,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 EE)) {TP(eloc2,eloc2 E,eloc2 EE,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 E,eloc2 NE,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
    }
    if(ISP(eloc2 N) && GT(eloc2 N,eloc2) && b.isThawed(eloc2 N))
    {
      if(ISE(eloc2 S)) {TP(eloc2 N,eloc2,eloc2 S,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 W)) {TP(eloc2 N,eloc2,eloc2 W,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 E)) {TP(eloc2 N,eloc2,eloc2 E,suc = canPP(b,pla,eloc1)) if(suc){return true;}}

      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 N,eloc2 NW,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 N,eloc2 NE,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
      if(ISE(eloc2 NN)) {TP(eloc2,eloc2 N,eloc2 NN,suc = canPP(b,pla,eloc1)) if(suc){return true;}}
    }
  }

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int gen2PieceAdj(Board& b, pla_t pla, loc_t eloc1, loc_t eloc2, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  step_t estep = gStepSrcDest(eloc1,kt);

  if(ISE(kt))
  {
    //Push eloc1 into trap, then remove eloc2
    if(ISP(eloc1 S) && GT(eloc1 S, eloc1) && b.isThawed(eloc1 S)) {TP(eloc1 S,eloc1,kt,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(estep,eloc1 S MN),2)}
    if(ISP(eloc1 W) && GT(eloc1 W, eloc1) && b.isThawed(eloc1 W)) {TP(eloc1 W,eloc1,kt,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(estep,eloc1 W ME),2)}
    if(ISP(eloc1 E) && GT(eloc1 E, eloc1) && b.isThawed(eloc1 E)) {TP(eloc1 E,eloc1,kt,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(estep,eloc1 E MW),2)}
    if(ISP(eloc1 N) && GT(eloc1 N, eloc1) && b.isThawed(eloc1 N)) {TP(eloc1 N,eloc1,kt,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(estep,eloc1 N MS),2)}

    //Otherwise we need to remove eloc2, then pushpull eloc1 into trap
    //Only try this order if it's potentially necessary. Namely if removing eloc2 could possibly unfreeze
    //some player that can push eloc1. For simplicity and speed, we don't actually check for frozen pieces
    //that could push eloc1, merely just frozen pieces that aren't rabbits.
    Bitmap frozenAdjEloc1 = b.frozenMap & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB] & Board::RADIUS[1][eloc1];
    if(frozenAdjEloc1.isEmpty())
      return num;
    bool eloc2AdjFrozenAdjEloc1 = (Board::RADIUS[1][eloc2] & frozenAdjEloc1).hasBits();

    if(ISP(eloc2 S) && GT(eloc2 S,eloc2) && b.isThawed(eloc2 S))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 W) && eloc2 W != kt) {TP(eloc2 S,eloc2,eloc2 W,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MW, eloc2 S MN),2)}
        if(ISE(eloc2 E) && eloc2 E != kt) {TP(eloc2 S,eloc2,eloc2 E,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 ME, eloc2 S MN),2)}
        if(ISE(eloc2 N) && eloc2 N != kt) {TP(eloc2 S,eloc2,eloc2 N,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MN, eloc2 S MN),2)}
      }

      if(ISE(eloc2 SS)) {TP(eloc2,eloc2 S,eloc2 SS,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 S MS, eloc2 MS),2)}
      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 S,eloc2 SW,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 S MW, eloc2 MS),2)}
      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 S,eloc2 SE,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 S ME, eloc2 MS),2)}
    }
    if(ISP(eloc2 W) && GT(eloc2 W,eloc2) && b.isThawed(eloc2 W))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 S) && eloc2 S != kt) {TP(eloc2 W,eloc2,eloc2 S,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MS, eloc2 W ME),2)}
        if(ISE(eloc2 E) && eloc2 E != kt) {TP(eloc2 W,eloc2,eloc2 E,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 ME, eloc2 W ME),2)}
        if(ISE(eloc2 N) && eloc2 N != kt) {TP(eloc2 W,eloc2,eloc2 N,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MN, eloc2 W ME),2)}
      }

      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 W,eloc2 SW,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 W MS, eloc2 MW),2)}
      if(ISE(eloc2 WW)) {TP(eloc2,eloc2 W,eloc2 WW,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 W MW, eloc2 MW),2)}
      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 W,eloc2 NW,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 W MN, eloc2 MW),2)}
    }
    if(ISP(eloc2 E) && GT(eloc2 E,eloc2) && b.isThawed(eloc2 E))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 S) && eloc2 S != kt) {TP(eloc2 E,eloc2,eloc2 S,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MS, eloc2 E MW),2)}
        if(ISE(eloc2 W) && eloc2 W != kt) {TP(eloc2 E,eloc2,eloc2 W,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MW, eloc2 E MW),2)}
        if(ISE(eloc2 N) && eloc2 N != kt) {TP(eloc2 E,eloc2,eloc2 N,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MN, eloc2 E MW),2)}
      }

      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 E,eloc2 SE,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 E MS, eloc2 ME),2)}
      if(ISE(eloc2 EE)) {TP(eloc2,eloc2 E,eloc2 EE,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 E ME, eloc2 ME),2)}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 E,eloc2 NE,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 E MN, eloc2 ME),2)}
    }
    if(ISP(eloc2 N) && GT(eloc2 N,eloc2) && b.isThawed(eloc2 N))
    {
      if(eloc2AdjFrozenAdjEloc1)
      {
        if(ISE(eloc2 S) && eloc2 S != kt) {TP(eloc2 N,eloc2,eloc2 S,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MS, eloc2 N MS),2)}
        if(ISE(eloc2 W) && eloc2 W != kt) {TP(eloc2 N,eloc2,eloc2 W,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MW, eloc2 N MS),2)}
        if(ISE(eloc2 E) && eloc2 E != kt) {TP(eloc2 N,eloc2,eloc2 E,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 ME, eloc2 N MS),2)}
      }

      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 N,eloc2 NW,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 N MW, eloc2 MN),2)}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 N,eloc2 NE,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 N ME, eloc2 MN),2)}
      if(ISE(eloc2 NN)) {TP(eloc2,eloc2 N,eloc2 NN,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 N MN, eloc2 MN),2)}
    }
  }
  else if(ISP(kt))
  {
    //Pull eloc1 into trap, then remove eloc2
    if(GT(kt,eloc1))
    {
      if(ISE(kt S)) {TP(eloc1,kt,kt S,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(kt MS,estep),2)}
      if(ISE(kt W)) {TP(eloc1,kt,kt W,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(kt MW,estep),2)}
      if(ISE(kt E)) {TP(eloc1,kt,kt E,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(kt ME,estep),2)}
      if(ISE(kt N)) {TP(eloc1,kt,kt N,int newnum = num + genPP(b,pla,eloc2,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(kt MN,estep),2)}

      //Remove eloc2 to make room, then pull into trap, TempRemove to make sure kt doesn't pull yet
      TR(kt,int newnum = num + genPull(b,pla,eloc2,mv+num,hm+num,hmval)) SUFFIXMOVES(getMove(gStepSrcDest(kt,eloc2),gStepSrcDest(eloc1,kt)),2)
    }
    //Push eloc 2, then push eloc 1 to trap
    if(GT(kt,eloc2))
    {
      step_t estep2 = gStepSrcDest(kt,eloc2);
      if(ISE(eloc2 S)) {TP(kt,eloc2,eloc2 S,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MS,estep2),2)}
      if(ISE(eloc2 W)) {TP(kt,eloc2,eloc2 W,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MW,estep2),2)}
      if(ISE(eloc2 E)) {TP(kt,eloc2,eloc2 E,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 ME,estep2),2)}
      if(ISE(eloc2 N)) {TP(kt,eloc2,eloc2 N,int newnum = num + genPPTo(b,pla,eloc1,kt,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MN,estep2),2)}
    }
  }
  else //if(ISO(kt))
  {
    //There's no point in removing eloc2 first and then removing eloc1 if eloc1's pushpuller is already guaranteed unfrozen.
    //Except when removing eloc2 by a pull or a push that clears space for eloc1 to be pulled or pushed
    Bitmap frozens = b.frozenMap & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB];
    bool frozenAdjEloc1 = (frozens & Board::RADIUS[1][eloc1]).hasBits();
    bool frozenAdjEloc2 = (frozens & Board::RADIUS[1][eloc2]).hasBits();
    bool pointless = !frozenAdjEloc1 && (frozenAdjEloc2 || (!frozenAdjEloc2 && eloc1 < eloc2));

    if(pointless)
    {
      //In the otherwise pointless case, we need to make sure the first pushpull clears critical space for the second.
      //Find a location adjacent to both elocs other than the trap
      Bitmap adjMap = Board::RADIUS[1][eloc1] & Board::RADIUS[1][eloc2] & ~Bitmap::BMPTRAPS;
      if(adjMap.isEmpty())
        return 0;
      loc_t adj = adjMap.nextBit();
      if(!ISP(adj))
        return 0;

      //Push-push
      if(GT(adj,eloc2))
      {
        step_t pstep = gStepSrcDest(adj,eloc2);
        if(ISE(eloc2 S)) {TP(adj,eloc2,eloc2 S,int newnum = num + genPPTo(b,pla,eloc1,adj,mv+num,hm+num,hmval)); PREFIXMOVES(getMove(eloc2 MS,pstep),2)}
        if(ISE(eloc2 W)) {TP(adj,eloc2,eloc2 W,int newnum = num + genPPTo(b,pla,eloc1,adj,mv+num,hm+num,hmval)); PREFIXMOVES(getMove(eloc2 MW,pstep),2)}
        if(ISE(eloc2 E)) {TP(adj,eloc2,eloc2 E,int newnum = num + genPPTo(b,pla,eloc1,adj,mv+num,hm+num,hmval)); PREFIXMOVES(getMove(eloc2 ME,pstep),2)}
        if(ISE(eloc2 N)) {TP(adj,eloc2,eloc2 N,int newnum = num + genPPTo(b,pla,eloc1,adj,mv+num,hm+num,hmval)); PREFIXMOVES(getMove(eloc2 MN,pstep),2)}
      }

      //Pull-pull
      if(GT(adj,eloc1))
      {
        move_t pullMove = getMove(gStepSrcDest(adj,eloc2),gStepSrcDest(eloc1,adj));
        if(eloc2 S != adj && ISP(eloc2 S) && GT(eloc2 S,eloc2) && b.isThawed(eloc2 S)) {int newnum = num + genPullPE(b,eloc2 S,eloc2,mv+num,hm+num,hmval); SUFFIXMOVES(pullMove,2)}
        if(eloc2 W != adj && ISP(eloc2 W) && GT(eloc2 W,eloc2) && b.isThawed(eloc2 W)) {int newnum = num + genPullPE(b,eloc2 W,eloc2,mv+num,hm+num,hmval); SUFFIXMOVES(pullMove,2)}
        if(eloc2 E != adj && ISP(eloc2 E) && GT(eloc2 E,eloc2) && b.isThawed(eloc2 E)) {int newnum = num + genPullPE(b,eloc2 E,eloc2,mv+num,hm+num,hmval); SUFFIXMOVES(pullMove,2)}
        if(eloc2 N != adj && ISP(eloc2 N) && GT(eloc2 N,eloc2) && b.isThawed(eloc2 N)) {int newnum = num + genPullPE(b,eloc2 N,eloc2,mv+num,hm+num,hmval); SUFFIXMOVES(pullMove,2)}
      }
      return num;
    }


    //Remove eloc2, then remove eloc1
    if(ISP(eloc2 S) && GT(eloc2 S,eloc2) && b.isThawed(eloc2 S))
    {
      if(ISE(eloc2 W)) {TP(eloc2 S,eloc2,eloc2 W,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MW, eloc2 S MN),2)}
      if(ISE(eloc2 E)) {TP(eloc2 S,eloc2,eloc2 E,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 ME, eloc2 S MN),2)}
      if(ISE(eloc2 N)) {TP(eloc2 S,eloc2,eloc2 N,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MN, eloc2 S MN),2)}

      if(ISE(eloc2 SS)) {TP(eloc2,eloc2 S,eloc2 SS,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 S MS, eloc2 MS),2)}
      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 S,eloc2 SW,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 S MW, eloc2 MS),2)}
      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 S,eloc2 SE,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 S ME, eloc2 MS),2)}
    }
    if(ISP(eloc2 W) && GT(eloc2 W,eloc2) && b.isThawed(eloc2 W))
    {
      if(ISE(eloc2 S)) {TP(eloc2 W,eloc2,eloc2 S,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MS, eloc2 W ME),2)}
      if(ISE(eloc2 E)) {TP(eloc2 W,eloc2,eloc2 E,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 ME, eloc2 W ME),2)}
      if(ISE(eloc2 N)) {TP(eloc2 W,eloc2,eloc2 N,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MN, eloc2 W ME),2)}

      if(ISE(eloc2 SW)) {TP(eloc2,eloc2 W,eloc2 SW,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 W MS, eloc2 MW),2)}
      if(ISE(eloc2 WW)) {TP(eloc2,eloc2 W,eloc2 WW,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 W MW, eloc2 MW),2)}
      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 W,eloc2 NW,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 W MN, eloc2 MW),2)}
    }
    if(ISP(eloc2 E) && GT(eloc2 E,eloc2) && b.isThawed(eloc2 E))
    {
      if(ISE(eloc2 S)) {TP(eloc2 E,eloc2,eloc2 S,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MS, eloc2 E MW),2)}
      if(ISE(eloc2 W)) {TP(eloc2 E,eloc2,eloc2 W,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MW, eloc2 E MW),2)}
      if(ISE(eloc2 N)) {TP(eloc2 E,eloc2,eloc2 N,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MN, eloc2 E MW),2)}

      if(ISE(eloc2 SE)) {TP(eloc2,eloc2 E,eloc2 SE,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 E MS, eloc2 ME),2)}
      if(ISE(eloc2 EE)) {TP(eloc2,eloc2 E,eloc2 EE,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 E ME, eloc2 ME),2)}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 E,eloc2 NE,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 E MN, eloc2 ME),2)}
    }
    if(ISP(eloc2 N) && GT(eloc2 N,eloc2) && b.isThawed(eloc2 N))
    {
      if(ISE(eloc2 S)) {TP(eloc2 N,eloc2,eloc2 S,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MS, eloc2 N MS),2)}
      if(ISE(eloc2 W)) {TP(eloc2 N,eloc2,eloc2 W,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 MW, eloc2 N MS),2)}
      if(ISE(eloc2 E)) {TP(eloc2 N,eloc2,eloc2 E,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 ME, eloc2 N MS),2)}

      if(ISE(eloc2 NW)) {TP(eloc2,eloc2 N,eloc2 NW,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 N MW, eloc2 MN),2)}
      if(ISE(eloc2 NE)) {TP(eloc2,eloc2 N,eloc2 NE,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 N ME, eloc2 MN),2)}
      if(ISE(eloc2 NN)) {TP(eloc2,eloc2 N,eloc2 NN,int newnum = num + genPP(b,pla,eloc1,mv+num,hm+num,hmval)) PREFIXMOVES(getMove(eloc2 N MN, eloc2 MN),2)}
    }
  }

  return num;
}

//4-STEP CAPTURES, 1 DEFENDER--------------------------------------------------------------

//Can we move a piece stronger than eloc to sloc in two steps? (As in, two steps away), and have it unfrozen?
//Assumes that sloc is not a trap.
//Does NOT use the C version of thawed or frozen!
static bool canMoveStrong2S(Board& b, pla_t pla, loc_t sloc, loc_t eloc)
{
  bool suc = false;

  if(ISE(sloc S) && b.isTrapSafe2(pla,sloc S))
  {
    if(ISP(sloc SS) && GT(sloc SS,eloc) && b.isThawed(sloc SS) && b.wouldBeUF(pla,sloc SS,sloc S,sloc SS)) {TSC(sloc SS,sloc S,suc=b.wouldBeUF(pla,sloc S,sloc,sloc S)) if(suc){return true;}}
    if(ISP(sloc SW) && GT(sloc SW,eloc) && b.isThawed(sloc SW) && b.wouldBeUF(pla,sloc SW,sloc S,sloc SW)) {TSC(sloc SW,sloc S,suc=b.wouldBeUF(pla,sloc S,sloc,sloc S)) if(suc){return true;}}
    if(ISP(sloc SE) && GT(sloc SE,eloc) && b.isThawed(sloc SE) && b.wouldBeUF(pla,sloc SE,sloc S,sloc SE)) {TSC(sloc SE,sloc S,suc=b.wouldBeUF(pla,sloc S,sloc,sloc S)) if(suc){return true;}}
  }
  if(ISE(sloc W) && b.isTrapSafe2(pla,sloc W))
  {
    if(ISP(sloc SW) && GT(sloc SW,eloc) && b.isThawed(sloc SW) && b.wouldBeUF(pla,sloc SW,sloc W,sloc SW)) {TSC(sloc SW,sloc W,suc=b.wouldBeUF(pla,sloc W,sloc,sloc W)) if(suc){return true;}}
    if(ISP(sloc WW) && GT(sloc WW,eloc) && b.isThawed(sloc WW) && b.wouldBeUF(pla,sloc WW,sloc W,sloc WW)) {TSC(sloc WW,sloc W,suc=b.wouldBeUF(pla,sloc W,sloc,sloc W)) if(suc){return true;}}
    if(ISP(sloc NW) && GT(sloc NW,eloc) && b.isThawed(sloc NW) && b.wouldBeUF(pla,sloc NW,sloc W,sloc NW)) {TSC(sloc NW,sloc W,suc=b.wouldBeUF(pla,sloc W,sloc,sloc W)) if(suc){return true;}}
  }
  if(ISE(sloc E) && b.isTrapSafe2(pla,sloc E))
  {
    if(ISP(sloc SE) && GT(sloc SE,eloc) && b.isThawed(sloc SE) && b.wouldBeUF(pla,sloc SE,sloc E,sloc SE)) {TSC(sloc SE,sloc E,suc=b.wouldBeUF(pla,sloc E,sloc,sloc E)) if(suc){return true;}}
    if(ISP(sloc EE) && GT(sloc EE,eloc) && b.isThawed(sloc EE) && b.wouldBeUF(pla,sloc EE,sloc E,sloc EE)) {TSC(sloc EE,sloc E,suc=b.wouldBeUF(pla,sloc E,sloc,sloc E)) if(suc){return true;}}
    if(ISP(sloc NE) && GT(sloc NE,eloc) && b.isThawed(sloc NE) && b.wouldBeUF(pla,sloc NE,sloc E,sloc NE)) {TSC(sloc NE,sloc E,suc=b.wouldBeUF(pla,sloc E,sloc,sloc E)) if(suc){return true;}}
  }
  if(ISE(sloc N) && b.isTrapSafe2(pla,sloc N))
  {
    if(ISP(sloc NW) && GT(sloc NW,eloc) && b.isThawed(sloc NW) && b.wouldBeUF(pla,sloc NW,sloc N,sloc NW)) {TSC(sloc NW,sloc N,suc=b.wouldBeUF(pla,sloc N,sloc,sloc N)) if(suc){return true;}}
    if(ISP(sloc NE) && GT(sloc NE,eloc) && b.isThawed(sloc NE) && b.wouldBeUF(pla,sloc NE,sloc N,sloc NE)) {TSC(sloc NE,sloc N,suc=b.wouldBeUF(pla,sloc N,sloc,sloc N)) if(suc){return true;}}
    if(ISP(sloc NN) && GT(sloc NN,eloc) && b.isThawed(sloc NN) && b.wouldBeUF(pla,sloc NN,sloc N,sloc NN)) {TSC(sloc NN,sloc N,suc=b.wouldBeUF(pla,sloc N,sloc,sloc N)) if(suc){return true;}}
  }

  return false;
}

//Can we move a piece stronger than eloc to sloc in two steps? (As in, two steps away), and have it unfrozen?
//Assumes that sloc is not a trap.
//Does NOT use the C version of thawed or frozen!
static int genMoveStrong2S(Board& b, pla_t pla, loc_t sloc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  bool suc = false;

  //Suicide cases cannot count as valid unfreezing, Ex: If cat steps N then W, sacrificing rabbit, then will frozen by e.
  /*
     .*.
     .r.
     e..
     .RC
  */

  if(ISE(sloc S) && b.isTrapSafe2(pla,sloc S))
  {
    if(ISP(sloc SS) && GT(sloc SS,eloc) && b.isThawed(sloc SS) && b.wouldBeUF(pla,sloc SS,sloc S,sloc SS)) {TSC(sloc SS,sloc S,suc=b.wouldBeUF(pla,sloc S,sloc,sloc S)) if(suc){ADDMOVE(getMove(sloc SS MN,sloc S MN),hmval)}}
    if(ISP(sloc SW) && GT(sloc SW,eloc) && b.isThawed(sloc SW) && b.wouldBeUF(pla,sloc SW,sloc S,sloc SW)) {TSC(sloc SW,sloc S,suc=b.wouldBeUF(pla,sloc S,sloc,sloc S)) if(suc){ADDMOVE(getMove(sloc SW ME,sloc S MN),hmval)}}
    if(ISP(sloc SE) && GT(sloc SE,eloc) && b.isThawed(sloc SE) && b.wouldBeUF(pla,sloc SE,sloc S,sloc SE)) {TSC(sloc SE,sloc S,suc=b.wouldBeUF(pla,sloc S,sloc,sloc S)) if(suc){ADDMOVE(getMove(sloc SE MW,sloc S MN),hmval)}}
  }
  if(ISE(sloc W) && b.isTrapSafe2(pla,sloc W))
  {
    if(ISP(sloc SW) && GT(sloc SW,eloc) && b.isThawed(sloc SW) && b.wouldBeUF(pla,sloc SW,sloc W,sloc SW)) {TSC(sloc SW,sloc W,suc=b.wouldBeUF(pla,sloc W,sloc,sloc W)) if(suc){ADDMOVE(getMove(sloc SW MN,sloc W ME),hmval)}}
    if(ISP(sloc WW) && GT(sloc WW,eloc) && b.isThawed(sloc WW) && b.wouldBeUF(pla,sloc WW,sloc W,sloc WW)) {TSC(sloc WW,sloc W,suc=b.wouldBeUF(pla,sloc W,sloc,sloc W)) if(suc){ADDMOVE(getMove(sloc WW ME,sloc W ME),hmval)}}
    if(ISP(sloc NW) && GT(sloc NW,eloc) && b.isThawed(sloc NW) && b.wouldBeUF(pla,sloc NW,sloc W,sloc NW)) {TSC(sloc NW,sloc W,suc=b.wouldBeUF(pla,sloc W,sloc,sloc W)) if(suc){ADDMOVE(getMove(sloc NW MS,sloc W ME),hmval)}}
  }
  if(ISE(sloc E) && b.isTrapSafe2(pla,sloc E))
  {
    if(ISP(sloc SE) && GT(sloc SE,eloc) && b.isThawed(sloc SE) && b.wouldBeUF(pla,sloc SE,sloc E,sloc SE)) {TSC(sloc SE,sloc E,suc=b.wouldBeUF(pla,sloc E,sloc,sloc E)) if(suc){ADDMOVE(getMove(sloc SE MN,sloc E MW),hmval)}}
    if(ISP(sloc EE) && GT(sloc EE,eloc) && b.isThawed(sloc EE) && b.wouldBeUF(pla,sloc EE,sloc E,sloc EE)) {TSC(sloc EE,sloc E,suc=b.wouldBeUF(pla,sloc E,sloc,sloc E)) if(suc){ADDMOVE(getMove(sloc EE MW,sloc E MW),hmval)}}
    if(ISP(sloc NE) && GT(sloc NE,eloc) && b.isThawed(sloc NE) && b.wouldBeUF(pla,sloc NE,sloc E,sloc NE)) {TSC(sloc NE,sloc E,suc=b.wouldBeUF(pla,sloc E,sloc,sloc E)) if(suc){ADDMOVE(getMove(sloc NE MS,sloc E MW),hmval)}}
  }
  if(ISE(sloc N) && b.isTrapSafe2(pla,sloc N))
  {
    if(ISP(sloc NW) && GT(sloc NW,eloc) && b.isThawed(sloc NW) && b.wouldBeUF(pla,sloc NW,sloc N,sloc NW)) {TSC(sloc NW,sloc N,suc=b.wouldBeUF(pla,sloc N,sloc,sloc N)) if(suc){ADDMOVE(getMove(sloc NW ME,sloc N MS),hmval)}}
    if(ISP(sloc NE) && GT(sloc NE,eloc) && b.isThawed(sloc NE) && b.wouldBeUF(pla,sloc NE,sloc N,sloc NE)) {TSC(sloc NE,sloc N,suc=b.wouldBeUF(pla,sloc N,sloc,sloc N)) if(suc){ADDMOVE(getMove(sloc NE MW,sloc N MS),hmval)}}
    if(ISP(sloc NN) && GT(sloc NN,eloc) && b.isThawed(sloc NN) && b.wouldBeUF(pla,sloc NN,sloc N,sloc NN)) {TSC(sloc NN,sloc N,suc=b.wouldBeUF(pla,sloc N,sloc,sloc N)) if(suc){ADDMOVE(getMove(sloc NN MS,sloc N MS),hmval)}}
  }

  return num;
}

//Can we swap out the player piece on sloc for a new piece in two steps, with the new piece UF and stronger than eloc?
static bool canSwapPla(Board& b, pla_t pla, loc_t sloc, loc_t eloc)
{
  if(ISP(sloc S) && GT(sloc S,eloc) && b.wouldBeUF(pla,sloc S,sloc S,sloc) && b.isTrapSafe2(pla,sloc S))
  {
    if(ISE(sloc W) &&                           (b.isTrapSafe2(pla,sloc W) || b.wouldBeUF(pla,sloc S,sloc,sloc S))) {return true;}
    if(ISE(sloc E) &&                           (b.isTrapSafe2(pla,sloc E) || b.wouldBeUF(pla,sloc S,sloc,sloc S))) {return true;}
    if(ISE(sloc N) && b.isRabOkayN(pla,sloc) && (b.isTrapSafe2(pla,sloc N) || b.wouldBeUF(pla,sloc S,sloc,sloc S))) {return true;}
  }
  if(ISP(sloc W) && GT(sloc W,eloc) && b.wouldBeUF(pla,sloc W,sloc W,sloc) && b.isTrapSafe2(pla,sloc W))
  {
    if(ISE(sloc S) && b.isRabOkayS(pla,sloc) && (b.isTrapSafe2(pla,sloc S) || b.wouldBeUF(pla,sloc W,sloc,sloc W))) {return true;}
    if(ISE(sloc E) &&                           (b.isTrapSafe2(pla,sloc E) || b.wouldBeUF(pla,sloc W,sloc,sloc W))) {return true;}
    if(ISE(sloc N) && b.isRabOkayN(pla,sloc) && (b.isTrapSafe2(pla,sloc N) || b.wouldBeUF(pla,sloc W,sloc,sloc W))) {return true;}
  }
  if(ISP(sloc E) && GT(sloc E,eloc) && b.wouldBeUF(pla,sloc E,sloc E,sloc) && b.isTrapSafe2(pla,sloc E))
  {
    if(ISE(sloc S) && b.isRabOkayS(pla,sloc) && (b.isTrapSafe2(pla,sloc S) || b.wouldBeUF(pla,sloc E,sloc,sloc E))) {return true;}
    if(ISE(sloc W) &&                           (b.isTrapSafe2(pla,sloc W) || b.wouldBeUF(pla,sloc E,sloc,sloc E))) {return true;}
    if(ISE(sloc N) && b.isRabOkayN(pla,sloc) && (b.isTrapSafe2(pla,sloc N) || b.wouldBeUF(pla,sloc E,sloc,sloc E))) {return true;}
  }
  if(ISP(sloc N) && GT(sloc N,eloc) && b.wouldBeUF(pla,sloc N,sloc N,sloc) && b.isTrapSafe2(pla,sloc N))
  {
    if(ISE(sloc S) && b.isRabOkayS(pla,sloc) && (b.isTrapSafe2(pla,sloc S) || b.wouldBeUF(pla,sloc N,sloc,sloc N))) {return true;}
    if(ISE(sloc W) &&                           (b.isTrapSafe2(pla,sloc W) || b.wouldBeUF(pla,sloc N,sloc,sloc N))) {return true;}
    if(ISE(sloc E) &&                           (b.isTrapSafe2(pla,sloc E) || b.wouldBeUF(pla,sloc N,sloc,sloc N))) {return true;}
  }
  return false;
}

//Can we swap out the player piece on sloc for a new piece in two steps, with the new piece UF and stronger than eloc?
static int genSwapPla(Board& b, pla_t pla, loc_t sloc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;

  if(ISP(sloc S) && GT(sloc S,eloc) && b.wouldBeUF(pla,sloc S,sloc S,sloc) && b.isTrapSafe2(pla,sloc S))
  {
    if(ISE(sloc W) &&                           (b.isTrapSafe2(pla,sloc W) || b.wouldBeUF(pla,sloc S,sloc,sloc S))) {ADDMOVE(getMove(sloc MW,sloc S MN),hmval)}
    if(ISE(sloc E) &&                           (b.isTrapSafe2(pla,sloc E) || b.wouldBeUF(pla,sloc S,sloc,sloc S))) {ADDMOVE(getMove(sloc ME,sloc S MN),hmval)}
    if(ISE(sloc N) && b.isRabOkayN(pla,sloc) && (b.isTrapSafe2(pla,sloc N) || b.wouldBeUF(pla,sloc S,sloc,sloc S))) {ADDMOVE(getMove(sloc MN,sloc S MN),hmval)}
  }
  if(ISP(sloc W) && GT(sloc W,eloc) && b.wouldBeUF(pla,sloc W,sloc W,sloc) && b.isTrapSafe2(pla,sloc W))
  {
    if(ISE(sloc S) && b.isRabOkayS(pla,sloc) && (b.isTrapSafe2(pla,sloc S) || b.wouldBeUF(pla,sloc W,sloc,sloc W))) {ADDMOVE(getMove(sloc MS,sloc W ME),hmval)}
    if(ISE(sloc E) &&                           (b.isTrapSafe2(pla,sloc E) || b.wouldBeUF(pla,sloc W,sloc,sloc W))) {ADDMOVE(getMove(sloc ME,sloc W ME),hmval)}
    if(ISE(sloc N) && b.isRabOkayN(pla,sloc) && (b.isTrapSafe2(pla,sloc N) || b.wouldBeUF(pla,sloc W,sloc,sloc W))) {ADDMOVE(getMove(sloc MN,sloc W ME),hmval)}
  }
  if(ISP(sloc E) && GT(sloc E,eloc) && b.wouldBeUF(pla,sloc E,sloc E,sloc) && b.isTrapSafe2(pla,sloc E))
  {
    if(ISE(sloc S) && b.isRabOkayS(pla,sloc) && (b.isTrapSafe2(pla,sloc S) || b.wouldBeUF(pla,sloc E,sloc,sloc E))) {ADDMOVE(getMove(sloc MS,sloc E MW),hmval)}
    if(ISE(sloc W) &&                           (b.isTrapSafe2(pla,sloc W) || b.wouldBeUF(pla,sloc E,sloc,sloc E))) {ADDMOVE(getMove(sloc MW,sloc E MW),hmval)}
    if(ISE(sloc N) && b.isRabOkayN(pla,sloc) && (b.isTrapSafe2(pla,sloc N) || b.wouldBeUF(pla,sloc E,sloc,sloc E))) {ADDMOVE(getMove(sloc MN,sloc E MW),hmval)}
  }
  if(ISP(sloc N) && GT(sloc N,eloc) && b.wouldBeUF(pla,sloc N,sloc N,sloc) && b.isTrapSafe2(pla,sloc N))
  {
    if(ISE(sloc S) && b.isRabOkayS(pla,sloc) && (b.isTrapSafe2(pla,sloc S) || b.wouldBeUF(pla,sloc N,sloc,sloc N))) {ADDMOVE(getMove(sloc MS,sloc N MS),hmval)}
    if(ISE(sloc W) &&                           (b.isTrapSafe2(pla,sloc W) || b.wouldBeUF(pla,sloc N,sloc,sloc N))) {ADDMOVE(getMove(sloc MW,sloc N MS),hmval)}
    if(ISE(sloc E) &&                           (b.isTrapSafe2(pla,sloc E) || b.wouldBeUF(pla,sloc N,sloc,sloc N))) {ADDMOVE(getMove(sloc ME,sloc N MS),hmval)}
  }
  return num;
}

//Can we swap out the opponent piece on sloc for a new pla piece in two steps, with the new piece UF and stronger than eloc?
//Assumes sloc cannot be a trap.
//Does NOT use the C version of thawed or frozen!
static bool canSwapOpp(Board& b, pla_t pla, loc_t sloc, loc_t eloc, loc_t kt)
{
  if(!b.isDominated(sloc))
    return false;
  pla_t opp = gOpp(pla);
  loc_t ignloc = ERRLOC;
  if     (ISO(sloc S) && !b.isTrapSafe2(opp,sloc S)) {ignloc = sloc S;}
  else if(ISO(sloc W) && !b.isTrapSafe2(opp,sloc W)) {ignloc = sloc W;}
  else if(ISO(sloc E) && !b.isTrapSafe2(opp,sloc E)) {ignloc = sloc E;}
  else if(ISO(sloc N) && !b.isTrapSafe2(opp,sloc N)) {ignloc = sloc N;}

  if(ISP(sloc S) && GT(sloc S,eloc) && GT(sloc S, sloc) && b.isThawed(sloc S) && b.wouldBeUF(pla,sloc S,sloc,sloc S,ignloc))
  {
    if(ISE(sloc W) && !b.isAdjacent(kt,sloc W)) {return true;}
    if(ISE(sloc E) && !b.isAdjacent(kt,sloc E)) {return true;}
    if(ISE(sloc N) && !b.isAdjacent(kt,sloc N)) {return true;}
  }
  if(ISP(sloc W) && GT(sloc W,eloc) && GT(sloc W, sloc) && b.isThawed(sloc W) && b.wouldBeUF(pla,sloc W,sloc,sloc W,ignloc))
  {
    if(ISE(sloc S) && !b.isAdjacent(kt,sloc S)) {return true;}
    if(ISE(sloc E) && !b.isAdjacent(kt,sloc E)) {return true;}
    if(ISE(sloc N) && !b.isAdjacent(kt,sloc N)) {return true;}
  }
  if(ISP(sloc E) && GT(sloc E,eloc) && GT(sloc E, sloc) && b.isThawed(sloc E) && b.wouldBeUF(pla,sloc E,sloc,sloc E,ignloc))
  {
    if(ISE(sloc S) && !b.isAdjacent(kt,sloc S)) {return true;}
    if(ISE(sloc W) && !b.isAdjacent(kt,sloc W)) {return true;}
    if(ISE(sloc N) && !b.isAdjacent(kt,sloc N)) {return true;}
  }
  if(ISP(sloc N) && GT(sloc N,eloc) && GT(sloc N, sloc) && b.isThawed(sloc N) && b.wouldBeUF(pla,sloc N,sloc,sloc N,ignloc))
  {
    if(ISE(sloc S) && !b.isAdjacent(kt,sloc S)) {return true;}
    if(ISE(sloc W) && !b.isAdjacent(kt,sloc W)) {return true;}
    if(ISE(sloc E) && !b.isAdjacent(kt,sloc E)) {return true;}
  }
  return false;
}

//Can we swap out the opponent piece on sloc for a new pla piece in two steps, with the new piece UF and stronger than eloc?
//Assumes sloc cannot be a trap.
//Does not push any opp on to a square adjacent to the given trap
//Does NOT use the C version of thawed or frozen!
static int genSwapOpp(Board& b, pla_t pla, loc_t sloc, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  if(!b.isDominated(sloc))
    return 0;
  int num = 0;
  pla_t opp = gOpp(pla);
  loc_t ignloc = ERRLOC;
  if     (ISO(sloc S) && !b.isTrapSafe2(opp,sloc S)) {ignloc = sloc S;}
  else if(ISO(sloc W) && !b.isTrapSafe2(opp,sloc W)) {ignloc = sloc W;}
  else if(ISO(sloc E) && !b.isTrapSafe2(opp,sloc E)) {ignloc = sloc E;}
  else if(ISO(sloc N) && !b.isTrapSafe2(opp,sloc N)) {ignloc = sloc N;}

  if(ISP(sloc S) && GT(sloc S,eloc) && GT(sloc S, sloc) && b.isThawed(sloc S) && b.wouldBeUF(pla,sloc S,sloc,sloc S,ignloc))
  {
    if(ISE(sloc W) && !b.isAdjacent(kt,sloc W)) {ADDMOVE(getMove(sloc MW,sloc S MN),hmval)}
    if(ISE(sloc E) && !b.isAdjacent(kt,sloc E)) {ADDMOVE(getMove(sloc ME,sloc S MN),hmval)}
    if(ISE(sloc N) && !b.isAdjacent(kt,sloc N)) {ADDMOVE(getMove(sloc MN,sloc S MN),hmval)}
  }
  if(ISP(sloc W) && GT(sloc W,eloc) && GT(sloc W, sloc) && b.isThawed(sloc W) && b.wouldBeUF(pla,sloc W,sloc,sloc W,ignloc))
  {
    if(ISE(sloc S) && !b.isAdjacent(kt,sloc S)) {ADDMOVE(getMove(sloc MS,sloc W ME),hmval)}
    if(ISE(sloc E) && !b.isAdjacent(kt,sloc E)) {ADDMOVE(getMove(sloc ME,sloc W ME),hmval)}
    if(ISE(sloc N) && !b.isAdjacent(kt,sloc N)) {ADDMOVE(getMove(sloc MN,sloc W ME),hmval)}
  }
  if(ISP(sloc E) && GT(sloc E,eloc) && GT(sloc E, sloc) && b.isThawed(sloc E) && b.wouldBeUF(pla,sloc E,sloc,sloc E,ignloc))
  {
    if(ISE(sloc S) && !b.isAdjacent(kt,sloc S)) {ADDMOVE(getMove(sloc MS,sloc E MW),hmval)}
    if(ISE(sloc W) && !b.isAdjacent(kt,sloc W)) {ADDMOVE(getMove(sloc MW,sloc E MW),hmval)}
    if(ISE(sloc N) && !b.isAdjacent(kt,sloc N)) {ADDMOVE(getMove(sloc MN,sloc E MW),hmval)}
  }
  if(ISP(sloc N) && GT(sloc N,eloc) && GT(sloc N, sloc) && b.isThawed(sloc N) && b.wouldBeUF(pla,sloc N,sloc,sloc N,ignloc))
  {
    if(ISE(sloc S) && !b.isAdjacent(kt,sloc S)) {ADDMOVE(getMove(sloc MS,sloc N MS),hmval)}
    if(ISE(sloc W) && !b.isAdjacent(kt,sloc W)) {ADDMOVE(getMove(sloc MW,sloc N MS),hmval)}
    if(ISE(sloc E) && !b.isAdjacent(kt,sloc E)) {ADDMOVE(getMove(sloc ME,sloc N MS),hmval)}
  }
  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canPPIntoTrapTE4C(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  //TODO does b.frozenMap help?
  if((Board::DISK[3][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return false;

  bool suc = false;
  pla_t opp = gOpp(pla);

  int meNum = ISP(kt S) + ISP(kt W) + ISP(kt E) + ISP(kt N);

  //Frozen but can push
  if(b.isDominated(eloc))
  {
    if(ISP(eloc S) && GT(eloc S,eloc) && b.isFrozen(eloc S) && canUF2(b,pla,eloc S)) {return true;}
    if(ISP(eloc W) && GT(eloc W,eloc) && b.isFrozen(eloc W) && canUF2(b,pla,eloc W)) {return true;}
    if(ISP(eloc E) && GT(eloc E,eloc) && b.isFrozen(eloc E) && canUF2(b,pla,eloc E)) {return true;}
    if(ISP(eloc N) && GT(eloc N,eloc) && b.isFrozen(eloc N) && canUF2(b,pla,eloc N)) {return true;}
  }

  //A step away, not going through the trap. Either must be frozen now, or be frozen after one step.
  //And if we're frozen now, the fact that we couldn't do a 3-step cap implies that we're frozen after 1 step
  if(ISE(eloc S) && eloc S != kt)
  {
    if(ISP(eloc SS) && GT(eloc SS,eloc))
    {
      if     (b.isThawed(eloc SS)) {TS(eloc SS,eloc S,suc = canUF(b,pla,eloc S)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SS,eloc S)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SS,eloc S,eloc SS) && canUFStepWouldBeUF(b,pla,eloc SS,eloc S,kt)) {return true;}
    }
    if(ISP(eloc SW) && GT(eloc SW,eloc))
    {
      if     (b.isThawed(eloc SW)) {TS(eloc SW,eloc S,suc = canUF(b,pla,eloc S)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SW,eloc S)) {return true;} if(canAdvancePullCap(b,pla,eloc SW,eloc S)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SW,eloc S,eloc SW) && canUFStepWouldBeUF(b,pla,eloc SW,eloc S,kt)) {return true;}
    }
    if(ISP(eloc SE) && GT(eloc SE,eloc))
    {
      if     (b.isThawed(eloc SE)) {TS(eloc SE,eloc S,suc = canUF(b,pla,eloc S)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SE,eloc S)) {return true;} if(canAdvancePullCap(b,pla,eloc SE,eloc S)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SE,eloc S,eloc SE) && canUFStepWouldBeUF(b,pla,eloc SE,eloc S,kt)) {return true;}
    }
  }
  if(ISE(eloc W) && eloc W != kt)
  {
    if(ISP(eloc SW) && GT(eloc SW,eloc))
    {
      if     (b.isThawed(eloc SW)) {TS(eloc SW,eloc W,suc = canUF(b,pla,eloc W)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SW,eloc W)) {return true;} if(canAdvancePullCap(b,pla,eloc SW,eloc W)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SW,eloc W,eloc SW) && canUFStepWouldBeUF(b,pla,eloc SW,eloc W,kt)) {return true;}
    }
    if(ISP(eloc WW) && GT(eloc WW,eloc))
    {
      if     (b.isThawed(eloc WW)) {TS(eloc WW,eloc W,suc = canUF(b,pla,eloc W)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc WW,eloc W)) {return true;}}
      else if(b.wouldBeUF(pla,eloc WW,eloc W,eloc WW) && canUFStepWouldBeUF(b,pla,eloc WW,eloc W,kt)) {return true;}
    }
    if(ISP(eloc NW) && GT(eloc NW,eloc))
    {
      if     (b.isThawed(eloc NW)) {TS(eloc NW,eloc W,suc = canUF(b,pla,eloc W)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NW,eloc W)) {return true;} if(canAdvancePullCap(b,pla,eloc NW,eloc W)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NW,eloc W,eloc NW) && canUFStepWouldBeUF(b,pla,eloc NW,eloc W,kt)) {return true;}
    }
  }
  if(ISE(eloc E) && eloc E != kt)
  {
    if(ISP(eloc SE) && GT(eloc SE,eloc))
    {
      if     (b.isThawed(eloc SE)) {TS(eloc SE,eloc E,suc = canUF(b,pla,eloc E)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SE,eloc E)) {return true;} if(canAdvancePullCap(b,pla,eloc SE,eloc E)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SE,eloc E,eloc SE) && canUFStepWouldBeUF(b,pla,eloc SE,eloc E,kt)) {return true;}
    }
    if(ISP(eloc EE) && GT(eloc EE,eloc))
    {
      if     (b.isThawed(eloc EE)) {TS(eloc EE,eloc E,suc = canUF(b,pla,eloc E)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc EE,eloc E)) {return true;}}
      else if(b.wouldBeUF(pla,eloc EE,eloc E,eloc EE) && canUFStepWouldBeUF(b,pla,eloc EE,eloc E,kt)) {return true;}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc))
    {
      if     (b.isThawed(eloc NE)) {TS(eloc NE,eloc E,suc = canUF(b,pla,eloc E)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NE,eloc E)) {return true;} if(canAdvancePullCap(b,pla,eloc NE,eloc E)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NE,eloc E,eloc NE) && canUFStepWouldBeUF(b,pla,eloc NE,eloc E,kt)) {return true;}
    }
  }
  if(ISE(eloc N) && eloc N != kt)
  {
    if(ISP(eloc NW) && GT(eloc NW,eloc))
    {
      if     (b.isThawed(eloc NW)) {TS(eloc NW,eloc N,suc = canUF(b,pla,eloc N)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NW,eloc N)) {return true;} if(canAdvancePullCap(b,pla,eloc NW,eloc N)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NW,eloc N,eloc NW) && canUFStepWouldBeUF(b,pla,eloc NW,eloc N,kt)) {return true;}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc))
    {
      if     (b.isThawed(eloc NE)) {TS(eloc NE,eloc N,suc = canUF(b,pla,eloc N)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NE,eloc N)) {return true;} if(canAdvancePullCap(b,pla,eloc NE,eloc N)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NE,eloc N,eloc NE) && canUFStepWouldBeUF(b,pla,eloc NE,eloc N,kt)) {return true;}
    }
    if(ISP(eloc NN) && GT(eloc NN,eloc))
    {
      if     (b.isThawed(eloc NN)) {TS(eloc NN,eloc N,suc = canUF(b,pla,eloc N)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NN,eloc N)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NN,eloc N,eloc NN) && canUFStepWouldBeUF(b,pla,eloc NN,eloc N,kt)) {return true;}
    }
  }

  //Friendly piece in the way
  if(ISP(eloc S) && canSwapPla(b,pla,eloc S,eloc)) {return true;}
  if(ISP(eloc W) && canSwapPla(b,pla,eloc W,eloc)) {return true;}
  if(ISP(eloc E) && canSwapPla(b,pla,eloc E,eloc)) {return true;}
  if(ISP(eloc N) && canSwapPla(b,pla,eloc N,eloc)) {return true;}

  //Opponent's piece in the way
  if(ISO(eloc S) && canSwapOpp(b,pla,eloc S,eloc,kt)) {return true;}
  if(ISO(eloc W) && canSwapOpp(b,pla,eloc W,eloc,kt)) {return true;}
  if(ISO(eloc E) && canSwapOpp(b,pla,eloc E,eloc,kt)) {return true;}
  if(ISO(eloc N) && canSwapOpp(b,pla,eloc N,eloc,kt)) {return true;}

  //A step away, going through the trap.
  //Trap is very safe. Can just unfreeze our piece.
  if(meNum >= 3)
  {
    b.owners[kt] = opp; b.pieces[kt] = RAB;
    if(ISP(kt S) && GT(kt S,eloc) && b.isFrozen(kt S) && canUF(b,pla,kt S)) {b.owners[kt] = NPLA; b.pieces[kt] = EMP; return true;}
    if(ISP(kt W) && GT(kt W,eloc) && b.isFrozen(kt W) && canUF(b,pla,kt W)) {b.owners[kt] = NPLA; b.pieces[kt] = EMP; return true;}
    if(ISP(kt E) && GT(kt E,eloc) && b.isFrozen(kt E) && canUF(b,pla,kt E)) {b.owners[kt] = NPLA; b.pieces[kt] = EMP; return true;}
    if(ISP(kt N) && GT(kt N,eloc) && b.isFrozen(kt N) && canUF(b,pla,kt N)) {b.owners[kt] = NPLA; b.pieces[kt] = EMP; return true;}
    b.owners[kt] = NPLA; b.pieces[kt] = EMP;
  }
  //Trap is safe. Can just unfreeze our piece, but make sure not to step away the trap-guarding piece.
  else if(meNum >= 2)
  {
    //Do some trickery; temporarily remove the piece so it isn't considrered by genUF
    if(ISP(kt S) && GT(kt S,eloc) && b.isFrozen(kt S))
    {
      if     (ISP(kt W)) {TR(kt W, suc = canUF(b,pla,kt S)) if(suc) {return true;}}
      else if(ISP(kt E)) {TR(kt E, suc = canUF(b,pla,kt S)) if(suc) {return true;}}
      else if(ISP(kt N)) {TR(kt N, suc = canUF(b,pla,kt S)) if(suc) {return true;}}
    }
    if(ISP(kt W) && GT(kt W,eloc) && b.isFrozen(kt W))
    {
      if     (ISP(kt S)) {TR(kt S, suc = canUF(b,pla,kt W)) if(suc) {return true;}}
      else if(ISP(kt E)) {TR(kt E, suc = canUF(b,pla,kt W)) if(suc) {return true;}}
      else if(ISP(kt N)) {TR(kt N, suc = canUF(b,pla,kt W)) if(suc) {return true;}}
    }
    if(ISP(kt E) && GT(kt E,eloc) && b.isFrozen(kt E))
    {
      if     (ISP(kt S)) {TR(kt S, suc = canUF(b,pla,kt E)) if(suc) {return true;}}
      else if(ISP(kt W)) {TR(kt W, suc = canUF(b,pla,kt E)) if(suc) {return true;}}
      else if(ISP(kt N)) {TR(kt N, suc = canUF(b,pla,kt E)) if(suc) {return true;}}
    }
    if(ISP(kt N) && GT(kt N,eloc) && b.isFrozen(kt N))
    {
      if     (ISP(kt S)) {TR(kt S, suc = canUF(b,pla,kt N)) if(suc) {return true;}}
      else if(ISP(kt W)) {TR(kt W, suc = canUF(b,pla,kt N)) if(suc) {return true;}}
      else if(ISP(kt E)) {TR(kt E, suc = canUF(b,pla,kt N)) if(suc) {return true;}}
    }
  }
  //The trap is unsafe, so make it safe
  else
  {
    //Locate the player next to the trap and its side-adjacent defender, if any
    int ploc = ERRLOC;
    int guardloc = ERRLOC;
    if     (ISP(kt S) && GT(kt S,eloc) && b.isThawed(kt S)) {ploc = kt S; if(ISP(kt SW)) {guardloc = kt SW;} else if(ISP(kt SE)) {guardloc = kt SE;}}
    else if(ISP(kt W) && GT(kt W,eloc) && b.isThawed(kt W)) {ploc = kt W; if(ISP(kt SW)) {guardloc = kt SW;} else if(ISP(kt NW)) {guardloc = kt NW;}}
    else if(ISP(kt E) && GT(kt E,eloc) && b.isThawed(kt E)) {ploc = kt E; if(ISP(kt SE)) {guardloc = kt SE;} else if(ISP(kt NE)) {guardloc = kt NE;}}
    else if(ISP(kt N) && GT(kt N,eloc) && b.isThawed(kt N)) {ploc = kt N; if(ISP(kt NW)) {guardloc = kt NW;} else if(ISP(kt NE)) {guardloc = kt NE;}}

    //If there's a strong enough player
    if(ploc != ERRLOC)
    {
      //Player is very unfrozen, just make the trap safe any way.
      if(!b.isDominated(ploc) || b.isGuarded2(pla, ploc))
      {if(canUF(b,pla,kt)) {return true;}}
      //Player dominated and only one defender
      else
      {
        //No side adjacent defender to worry about, make the trap safe
        if(guardloc == ERRLOC)
        {if(canUF(b,pla,kt)) {return true;}}
        //Temporarily remove the piece so genKTSafe won't use it to guard the trap.
        else
        {TR(guardloc,suc = canUF(b,pla,kt)) if(suc){return true;}}
      }
    }
  }

  //2 Steps away
  if(ISE(eloc S) && eloc S != kt && canMoveStrong2S(b,pla,eloc S,eloc)) {return true;}
  if(ISE(eloc W) && eloc W != kt && canMoveStrong2S(b,pla,eloc W,eloc)) {return true;}
  if(ISE(eloc E) && eloc E != kt && canMoveStrong2S(b,pla,eloc E,eloc)) {return true;}
  if(ISE(eloc N) && eloc N != kt && canMoveStrong2S(b,pla,eloc N,eloc)) {return true;}

  //2 Steps away, going through the trap
  //The trap is empty, but must be safe for this to work.
  if(meNum >= 1)
  {
    if(ISE(kt S) && canMoveStrongTo(b,pla,kt S,eloc)) return true;
    if(ISE(kt W) && canMoveStrongTo(b,pla,kt W,eloc)) return true;
    if(ISE(kt E) && canMoveStrongTo(b,pla,kt E,eloc)) return true;
    if(ISE(kt N) && canMoveStrongTo(b,pla,kt N,eloc)) return true;
  }

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genPPIntoTrapTE4C(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  if((Board::DISK[3][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return 0;

  bool suc = false;
  int num = 0;
  pla_t opp = gOpp(pla);
  int meNum = ISP(kt S) + ISP(kt W) + ISP(kt E) + ISP(kt N);

  //Frozen but can push
  if(b.isDominated(eloc))
  {
    if(ISP(eloc S) && GT(eloc S,eloc) && b.isFrozen(eloc S)) {num += genUF2(b,pla,eloc S,mv+num,hm+num,hmval);}
    if(ISP(eloc W) && GT(eloc W,eloc) && b.isFrozen(eloc W)) {num += genUF2(b,pla,eloc W,mv+num,hm+num,hmval);}
    if(ISP(eloc E) && GT(eloc E,eloc) && b.isFrozen(eloc E)) {num += genUF2(b,pla,eloc E,mv+num,hm+num,hmval);}
    if(ISP(eloc N) && GT(eloc N,eloc) && b.isFrozen(eloc N)) {num += genUF2(b,pla,eloc N,mv+num,hm+num,hmval);}
  }

  //A step away, not going through the trap. Either must be frozen now, or be frozen after one step if a 4-step cap is possible
  //but a 3-step cap wasn't possible.
  //We do actually go ahead and test the conditions, becomes sometimes this function will be called when a 3-step cap IS possible,
  //and it's stupid to generate unfreezes of a piece that isn't frozen.
  if(ISE(eloc S) && eloc S != kt)
  {
    if(ISP(eloc SS) && GT(eloc SS,eloc))
    {
      if     (b.wouldBeUF(pla,eloc SS,eloc S,eloc SS)) {if(b.isFrozen(eloc SS)) num += genUFStepWouldBeUF(b,pla,eloc SS,eloc S,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SS)) {TS(eloc SS,eloc S,suc = canUF(b,pla,eloc S)) if(suc) {ADDMOVE(getMove(eloc SS MN),hmval)} num += genAdvanceUFStep(b,pla,eloc SS,eloc S,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc SW) && GT(eloc SW,eloc))
    {
      if     (b.wouldBeUF(pla,eloc SW,eloc S,eloc SW)) {if(b.isFrozen(eloc SW)) num += genUFStepWouldBeUF(b,pla,eloc SW,eloc S,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SW)) {TS(eloc SW,eloc S,suc = canUF(b,pla,eloc S)) if(suc) {ADDMOVE(getMove(eloc SW ME),hmval)} num += genAdvanceUFStep(b,pla,eloc SW,eloc S,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SW,eloc S,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc SE) && GT(eloc SE,eloc))
    {
      if     (b.wouldBeUF(pla,eloc SE,eloc S,eloc SE)) {if(b.isFrozen(eloc SE)) num += genUFStepWouldBeUF(b,pla,eloc SE,eloc S,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SE)) {TS(eloc SE,eloc S,suc = canUF(b,pla,eloc S)) if(suc) {ADDMOVE(getMove(eloc SE MW),hmval)} num += genAdvanceUFStep(b,pla,eloc SE,eloc S,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SE,eloc S,mv+num,hm+num,hmval);}
    }
  }
  if(ISE(eloc W) && eloc W != kt)
  {
    if(ISP(eloc SW) && GT(eloc SW,eloc))
    {
      if     (b.wouldBeUF(pla,eloc SW,eloc W,eloc SW)) {if(b.isFrozen(eloc SW)) num += genUFStepWouldBeUF(b,pla,eloc SW,eloc W,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SW)) {TS(eloc SW,eloc W,suc = canUF(b,pla,eloc W)) if(suc) {ADDMOVE(getMove(eloc SW MN),hmval)} num += genAdvanceUFStep(b,pla,eloc SW,eloc W,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SW,eloc W,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc WW) && GT(eloc WW,eloc))
    {
      if     (b.wouldBeUF(pla,eloc WW,eloc W,eloc WW)) {if(b.isFrozen(eloc WW)) num += genUFStepWouldBeUF(b,pla,eloc WW,eloc W,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc WW)) {TS(eloc WW,eloc W,suc = canUF(b,pla,eloc W)) if(suc) {ADDMOVE(getMove(eloc WW ME),hmval)} num += genAdvanceUFStep(b,pla,eloc WW,eloc W,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NW) && GT(eloc NW,eloc))
    {
      if     (b.wouldBeUF(pla,eloc NW,eloc W,eloc NW)) {if(b.isFrozen(eloc NW)) num += genUFStepWouldBeUF(b,pla,eloc NW,eloc W,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NW)) {TS(eloc NW,eloc W,suc = canUF(b,pla,eloc W)) if(suc) {ADDMOVE(getMove(eloc NW MS),hmval)} num += genAdvanceUFStep(b,pla,eloc NW,eloc W,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NW,eloc W,mv+num,hm+num,hmval);}
    }
  }
  if(ISE(eloc E) && eloc E != kt)
  {
    if(ISP(eloc SE) && GT(eloc SE,eloc))
    {
      if     (b.wouldBeUF(pla,eloc SE,eloc E,eloc SE)) {if(b.isFrozen(eloc SE)) num += genUFStepWouldBeUF(b,pla,eloc SE,eloc E,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SE)) {TS(eloc SE,eloc E,suc = canUF(b,pla,eloc E)) if(suc) {ADDMOVE(getMove(eloc SE MN),hmval)} num += genAdvanceUFStep(b,pla,eloc SE,eloc E,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SE,eloc E,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc EE) && GT(eloc EE,eloc))
    {
      if     (b.wouldBeUF(pla,eloc EE,eloc E,eloc EE)) {if(b.isFrozen(eloc EE)) num += genUFStepWouldBeUF(b,pla,eloc EE,eloc E,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc EE)) {TS(eloc EE,eloc E,suc = canUF(b,pla,eloc E)) if(suc) {ADDMOVE(getMove(eloc EE MW),hmval)} num += genAdvanceUFStep(b,pla,eloc EE,eloc E,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc))
    {
      if     (b.wouldBeUF(pla,eloc NE,eloc E,eloc NE)) {if(b.isFrozen(eloc NE)) num += genUFStepWouldBeUF(b,pla,eloc NE,eloc E,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NE)) {TS(eloc NE,eloc E,suc = canUF(b,pla,eloc E)) if(suc) {ADDMOVE(getMove(eloc NE MS),hmval)} num += genAdvanceUFStep(b,pla,eloc NE,eloc E,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NE,eloc E,mv+num,hm+num,hmval);}
    }
  }
  if(ISE(eloc N) && eloc N != kt)
  {
    if(ISP(eloc NW) && GT(eloc NW,eloc))
    {
      if     (b.wouldBeUF(pla,eloc NW,eloc N,eloc NW)) {if(b.isFrozen(eloc NW)) num += genUFStepWouldBeUF(b,pla,eloc NW,eloc N,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NW)) {TS(eloc NW,eloc N,suc = canUF(b,pla,eloc N)) if(suc) {ADDMOVE(getMove(eloc NW ME),hmval)} num += genAdvanceUFStep(b,pla,eloc NW,eloc N,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NW,eloc N,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc))
    {
      if     (b.wouldBeUF(pla,eloc NE,eloc N,eloc NE)) {if(b.isFrozen(eloc NE)) num += genUFStepWouldBeUF(b,pla,eloc NE,eloc N,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NE)) {TS(eloc NE,eloc N,suc = canUF(b,pla,eloc N)) if(suc) {ADDMOVE(getMove(eloc NE MW),hmval)} num += genAdvanceUFStep(b,pla,eloc NE,eloc N,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NE,eloc N,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NN) && GT(eloc NN,eloc))
    {
      if     (b.wouldBeUF(pla,eloc NN,eloc N,eloc NN)) {if(b.isFrozen(eloc NN)) num += genUFStepWouldBeUF(b,pla,eloc NN,eloc N,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NN)) {TS(eloc NN,eloc N,suc = canUF(b,pla,eloc N)) if(suc) {ADDMOVE(getMove(eloc NN MS),hmval)} num += genAdvanceUFStep(b,pla,eloc NN,eloc N,mv+num,hm+num,hmval);}
    }
  }

  //Friendly piece in the way
  if(ISP(eloc S)) {num += genSwapPla(b,pla,eloc S,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc W)) {num += genSwapPla(b,pla,eloc W,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc E)) {num += genSwapPla(b,pla,eloc E,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc N)) {num += genSwapPla(b,pla,eloc N,eloc,mv+num,hm+num,hmval);}

  //Opponent's piece in the way
  if(ISO(eloc S)) {num += genSwapOpp(b,pla,eloc S,eloc,kt,mv+num,hm+num,hmval);}
  if(ISO(eloc W)) {num += genSwapOpp(b,pla,eloc W,eloc,kt,mv+num,hm+num,hmval);}
  if(ISO(eloc E)) {num += genSwapOpp(b,pla,eloc E,eloc,kt,mv+num,hm+num,hmval);}
  if(ISO(eloc N)) {num += genSwapOpp(b,pla,eloc N,eloc,kt,mv+num,hm+num,hmval);}

  //A step away, going through the trap.
  //Trap is very safe. Can just unfreeze our piece.
  //Temporarily add a rabbit to prevent unfreezes stepping on the trap.
  if(meNum >= 3)
  {
    b.owners[kt] = opp; b.pieces[kt] = RAB;
    if(ISP(kt S) && GT(kt S,eloc) && b.isFrozen(kt S)) {num += genUF(b,pla,kt S,mv+num,hm+num,hmval);}
    if(ISP(kt W) && GT(kt W,eloc) && b.isFrozen(kt W)) {num += genUF(b,pla,kt W,mv+num,hm+num,hmval);}
    if(ISP(kt E) && GT(kt E,eloc) && b.isFrozen(kt E)) {num += genUF(b,pla,kt E,mv+num,hm+num,hmval);}
    if(ISP(kt N) && GT(kt N,eloc) && b.isFrozen(kt N)) {num += genUF(b,pla,kt N,mv+num,hm+num,hmval);}
    b.owners[kt] = NPLA; b.pieces[kt] = EMP;
  }
  //Trap is safe. Can just unfreeze our piece, but make sure not to step away the trap-guarding piece.
  else if(meNum >= 2)
  {
    //Do some trickery; temporarily remove the piece so it isn't considrered by genUF
    if(ISP(kt S) && GT(kt S,eloc) && b.isFrozen(kt S))
    {
      if     (ISP(kt W)) {TR(kt W, num += genUF(b,pla,kt S,mv+num,hm+num,hmval))}
      else if(ISP(kt E)) {TR(kt E, num += genUF(b,pla,kt S,mv+num,hm+num,hmval))}
      else if(ISP(kt N)) {TR(kt N, num += genUF(b,pla,kt S,mv+num,hm+num,hmval))}
    }
    if(ISP(kt W) && GT(kt W,eloc) && b.isFrozen(kt W))
    {
      if     (ISP(kt S)) {TR(kt S, num += genUF(b,pla,kt W,mv+num,hm+num,hmval))}
      else if(ISP(kt E)) {TR(kt E, num += genUF(b,pla,kt W,mv+num,hm+num,hmval))}
      else if(ISP(kt N)) {TR(kt N, num += genUF(b,pla,kt W,mv+num,hm+num,hmval))}
    }
    if(ISP(kt E) && GT(kt E,eloc) && b.isFrozen(kt E))
    {
      if     (ISP(kt S)) {TR(kt S, num += genUF(b,pla,kt E,mv+num,hm+num,hmval))}
      else if(ISP(kt W)) {TR(kt W, num += genUF(b,pla,kt E,mv+num,hm+num,hmval))}
      else if(ISP(kt N)) {TR(kt N, num += genUF(b,pla,kt E,mv+num,hm+num,hmval))}
    }
    if(ISP(kt N) && GT(kt N,eloc) && b.isFrozen(kt N))
    {
      if     (ISP(kt S)) {TR(kt S, num += genUF(b,pla,kt N,mv+num,hm+num,hmval))}
      else if(ISP(kt W)) {TR(kt W, num += genUF(b,pla,kt N,mv+num,hm+num,hmval))}
      else if(ISP(kt E)) {TR(kt E, num += genUF(b,pla,kt N,mv+num,hm+num,hmval))}
    }
  }
  //The trap is unsafe, so make it safe
  else
  {
    //Locate the player next to the trap and its side-adjacent defender, if any
    int ploc = ERRLOC;
    int guardloc = ERRLOC;
    if     (ISP(kt S) && GT(kt S,eloc) && b.isThawed(kt S)) {ploc = kt S; if(ISP(kt SW)) {guardloc = kt SW;} else if(ISP(kt SE)) {guardloc = kt SE;}}
    else if(ISP(kt W) && GT(kt W,eloc) && b.isThawed(kt W)) {ploc = kt W; if(ISP(kt SW)) {guardloc = kt SW;} else if(ISP(kt NW)) {guardloc = kt NW;}}
    else if(ISP(kt E) && GT(kt E,eloc) && b.isThawed(kt E)) {ploc = kt E; if(ISP(kt SE)) {guardloc = kt SE;} else if(ISP(kt NE)) {guardloc = kt NE;}}
    else if(ISP(kt N) && GT(kt N,eloc) && b.isThawed(kt N)) {ploc = kt N; if(ISP(kt NW)) {guardloc = kt NW;} else if(ISP(kt NE)) {guardloc = kt NE;}}

    //If there's a strong enough player
    if(ploc != ERRLOC)
    {
      //Player is very unfrozen, just make the trap safe any way.
      if(!b.isDominated(ploc) || b.isGuarded2(pla,ploc))
      {num += genUF(b,pla,kt,mv+num,hm+num,hmval);}
      //Player dominated and only one defender
      else
      {
        //No side adjacent defender to worry about, make the trap safe
        if(guardloc == ERRLOC)
        {num += genUF(b,pla,kt,mv+num,hm+num,hmval);}
        //Temporarily remove the piece so genKTSafe won't use it to guard the trap.
        else
        {TR(guardloc,num += genUF(b,pla,kt,mv+num,hm+num,hmval))}
      }
    }
  }

  //2 Steps away
  if(ISE(eloc S) && eloc S != kt) {num += genMoveStrong2S(b,pla,eloc S,eloc,mv+num,hm+num,hmval);}
  if(ISE(eloc W) && eloc W != kt) {num += genMoveStrong2S(b,pla,eloc W,eloc,mv+num,hm+num,hmval);}
  if(ISE(eloc E) && eloc E != kt) {num += genMoveStrong2S(b,pla,eloc E,eloc,mv+num,hm+num,hmval);}
  if(ISE(eloc N) && eloc N != kt) {num += genMoveStrong2S(b,pla,eloc N,eloc,mv+num,hm+num,hmval);}

  //2 Steps away, going through the trap
  //The trap is empty, but must be safe for this to work.
  if(meNum >= 1)
  {
    if(ISE(kt S)) {num += genMoveStrongToAndThen(b,pla,kt S,eloc,getMove(kt S MN),mv+num,hm+num,hmval);}
    if(ISE(kt W)) {num += genMoveStrongToAndThen(b,pla,kt W,eloc,getMove(kt W ME),mv+num,hm+num,hmval);}
    if(ISE(kt E)) {num += genMoveStrongToAndThen(b,pla,kt E,eloc,getMove(kt E MW),mv+num,hm+num,hmval);}
    if(ISE(kt N)) {num += genMoveStrongToAndThen(b,pla,kt N,eloc,getMove(kt N MS),mv+num,hm+num,hmval);}
  }

  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canPPIntoTrapTP4C(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  bool suc = false;
  pla_t opp = gOpp(pla);

  //Weak piece on trap, must step off then push on.
  if(b.pieces[kt] <= b.pieces[eloc])
  {
    //Can step off immediately
    if(ISE(kt S) && kt S != eloc && b.isRabOkayS(pla,kt)) {TS(kt,kt S, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {return true;}}
    if(ISE(kt W) && kt W != eloc)                         {TS(kt,kt W, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {return true;}}
    if(ISE(kt E) && kt E != eloc)                         {TS(kt,kt E, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {return true;}}
    if(ISE(kt N) && kt N != eloc && b.isRabOkayN(pla,kt)) {TS(kt,kt N, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {return true;}}

    //Move each adjacent piece around and recurse
    if(ISP(kt S))
    {
      if(ISE(kt SS) && b.isRabOkayS(pla,kt S)) {TSC(kt S,kt SS,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt SW)                          ) {TSC(kt S,kt SW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt SE)                          ) {TSC(kt S,kt SE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
    }
    if(ISP(kt W))
    {
      if(ISE(kt SW) && b.isRabOkayS(pla,kt W)) {TSC(kt W,kt SW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt WW)                          ) {TSC(kt W,kt WW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt NW) && b.isRabOkayN(pla,kt W)) {TSC(kt W,kt NW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
    }
    if(ISP(kt E))
    {
      if(ISE(kt SE) && b.isRabOkayS(pla,kt E)) {TSC(kt E,kt SE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt EE)                          ) {TSC(kt E,kt EE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt NE) && b.isRabOkayN(pla,kt E)) {TSC(kt E,kt NE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
    }
    if(ISP(kt N))
    {
      if(ISE(kt NW)                          ) {TSC(kt N,kt NW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt NE)                          ) {TSC(kt N,kt NE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
      if(ISE(kt NN) && b.isRabOkayN(pla,kt N)) {TSC(kt N,kt NN,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {return true;}}
    }

    //SUICIDAL
    if(!b.isTrapSafe2(pla,kt))
    {
      if(ISP(kt S) && GT(kt S,eloc))
      {
        if(ISO(kt SW) && eloc == kt W && GT(kt S,kt SW) && b.wouldBeUF(pla,kt S,kt SW,kt S))
        {
          if(ISE(kt SSW)) {return true;}
          if(ISE(kt SWW)) {return true;}
        }
        else if(ISO(kt SE) && eloc == kt E && GT(kt S,kt SE) && b.wouldBeUF(pla,kt S,kt SE,kt S))
        {
          if(ISE(kt SSE)) {return true;}
          if(ISE(kt SEE)) {return true;}
        }
      }
      else if(ISP(kt W) && GT(kt W,eloc))
      {
        if(ISO(kt SW) && eloc == kt S && GT(kt W,kt SW) && b.wouldBeUF(pla,kt W,kt SW,kt W))
        {
          if(ISE(kt SSW)) {return true;}
          if(ISE(kt SWW)) {return true;}
        }
        else if(ISO(kt NW) && eloc == kt N && GT(kt W,kt NW) && b.wouldBeUF(pla,kt W,kt NW,kt W))
        {
          if(ISE(kt NNW)) {return true;}
          if(ISE(kt NWW)) {return true;}
        }
      }
      else if(ISP(kt E) && GT(kt E,eloc))
      {
        if(ISO(kt SE) && eloc == kt S && GT(kt E,kt SE) && b.wouldBeUF(pla,kt E,kt SE,kt E))
        {
          if(ISE(kt SSE)) {return true;}
          if(ISE(kt SEE)) {return true;}
        }
        else if(ISO(kt NE) && eloc == kt N && GT(kt E,kt NE) && b.wouldBeUF(pla,kt E,kt NE,kt E))
        {
          if(ISE(kt NNE)) {return true;}
          if(ISE(kt NEE)) {return true;}
        }
      }
      else if(ISP(kt N) && GT(kt N,eloc))
      {
        if(ISO(kt NW) && eloc == kt W && GT(kt N,kt NW) && b.wouldBeUF(pla,kt N,kt NW,kt N))
        {
          if(ISE(kt NNW)) {return true;}
          if(ISE(kt NWW)) {return true;}
        }
        else if(ISO(kt NE) && eloc == kt E && GT(kt N,kt NE) && b.wouldBeUF(pla,kt N,kt NE,kt N))
        {
          if(ISE(kt NNE)) {return true;}
          if(ISE(kt NEE)) {return true;}
        }
      }
    }
  }

  //Strong piece, but blocked. Since no 3 step captures were possible, all squares around must be doubly blocked by friendly pieces.
  else if(b.isBlocked(kt))
  {
    if(ISP(kt S) && canEmpty2(b,pla,kt S,ERRLOC,kt)) {return true;}
    if(ISP(kt W) && canEmpty2(b,pla,kt W,ERRLOC,kt)) {return true;}
    if(ISP(kt E) && canEmpty2(b,pla,kt E,ERRLOC,kt)) {return true;}
    if(ISP(kt N) && canEmpty2(b,pla,kt N,ERRLOC,kt)) {return true;}
  }

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genPPIntoTrapTP4C(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  bool suc = false;
  int num = 0;
  pla_t opp = gOpp(pla);

  //Weak piece on trap, must step off then push on.
  if(b.pieces[kt] <= b.pieces[eloc])
  {
    //Can step off immediately
    if(ISE(kt S) && kt S != eloc && b.isRabOkayS(pla,kt)) {TS(kt,kt S, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {ADDMOVE(getMove(kt MS),hmval)}}
    if(ISE(kt W) && kt W != eloc)                         {TS(kt,kt W, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {ADDMOVE(getMove(kt MW),hmval)}}
    if(ISE(kt E) && kt E != eloc)                         {TS(kt,kt E, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {ADDMOVE(getMove(kt ME),hmval)}}
    if(ISE(kt N) && kt N != eloc && b.isRabOkayN(pla,kt)) {TS(kt,kt N, suc = canPPIntoTrapTE3C(b,pla,eloc,kt)) if(suc) {ADDMOVE(getMove(kt MN),hmval)}}

    //Move each adjacent piece around and recurse
    if(ISP(kt S))
    {
      if(ISE(kt SS) && b.isRabOkayS(pla,kt S)) {TSC(kt S,kt SS,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt S MS),hmval)}}
      if(ISE(kt SW)                          ) {TSC(kt S,kt SW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt S MW),hmval)}}
      if(ISE(kt SE)                          ) {TSC(kt S,kt SE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt S ME),hmval)}}
    }
    if(ISP(kt W))
    {
      if(ISE(kt SW) && b.isRabOkayS(pla,kt W)) {TSC(kt W,kt SW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt W MS),hmval)}}
      if(ISE(kt WW)                          ) {TSC(kt W,kt WW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt W MW),hmval)}}
      if(ISE(kt NW) && b.isRabOkayN(pla,kt W)) {TSC(kt W,kt NW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt W MN),hmval)}}
    }
    if(ISP(kt E))
    {
      if(ISE(kt SE) && b.isRabOkayS(pla,kt E)) {TSC(kt E,kt SE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt E MS),hmval)}}
      if(ISE(kt EE)                          ) {TSC(kt E,kt EE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt E ME),hmval)}}
      if(ISE(kt NE) && b.isRabOkayN(pla,kt E)) {TSC(kt E,kt NE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt E MN),hmval)}}
    }
    if(ISP(kt N))
    {
      if(ISE(kt NW)                          ) {TSC(kt N,kt NW,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt N MW),hmval)}}
      if(ISE(kt NE)                          ) {TSC(kt N,kt NE,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt N ME),hmval)}}
      if(ISE(kt NN) && b.isRabOkayN(pla,kt N)) {TSC(kt N,kt NN,suc = (ISE(kt) ? canPPIntoTrapTE3C(b,pla,eloc,kt) : canPPIntoTrapTP3C(b,pla,eloc,kt))) if(suc) {ADDMOVE(getMove(kt N MN),hmval)}}
    }

    //SUICIDAL
    if(!b.isTrapSafe2(pla,kt))
    {
      if(ISP(kt S) && GT(kt S,eloc))
      {
        if     (ISO(kt SW) && eloc == kt W && GT(kt S,kt SW) && b.wouldBeUF(pla,kt S,kt SW,kt S))
        {
          if(ISE(kt SSW)) {ADDMOVE(getMove(kt SW MS,kt S MW),hmval)}
          if(ISE(kt SWW)) {ADDMOVE(getMove(kt SW MW,kt S MW),hmval)}
        }
        else if(ISO(kt SE) && eloc == kt E && GT(kt S,kt SE) && b.wouldBeUF(pla,kt S,kt SE,kt S))
        {
          if(ISE(kt SSE)) {ADDMOVE(getMove(kt SE MS,kt S ME),hmval)}
          if(ISE(kt SEE)) {ADDMOVE(getMove(kt SE ME,kt S ME),hmval)}
        }
      }
      else if(ISP(kt W) && GT(kt W,eloc))
      {
        if     (ISO(kt SW) && eloc == kt S && GT(kt W,kt SW) && b.wouldBeUF(pla,kt W,kt SW,kt W))
        {
          if(ISE(kt SSW)) {ADDMOVE(getMove(kt SW MS,kt W MS),hmval)}
          if(ISE(kt SWW)) {ADDMOVE(getMove(kt SW MW,kt W MS),hmval)}
        }
        else if(ISO(kt NW) && eloc == kt N && GT(kt W,kt NW) && b.wouldBeUF(pla,kt W,kt NW,kt W))
        {
          if(ISE(kt NNW)) {ADDMOVE(getMove(kt NW MN,kt W MN),hmval)}
          if(ISE(kt NWW)) {ADDMOVE(getMove(kt NW MW,kt W MN),hmval)}
        }
      }
      else if(ISP(kt E) && GT(kt E,eloc))
      {
        if     (ISO(kt SE) && eloc == kt S && GT(kt E,kt SE) && b.wouldBeUF(pla,kt E,kt SE,kt E))
        {
          if(ISE(kt SSE)) {ADDMOVE(getMove(kt SE MS,kt E MS),hmval)}
          if(ISE(kt SEE)) {ADDMOVE(getMove(kt SE ME,kt E MS),hmval)}
        }
        else if(ISO(kt NE) && eloc == kt N && GT(kt E,kt NE) && b.wouldBeUF(pla,kt E,kt NE,kt E))
        {
          if(ISE(kt NNE)) {ADDMOVE(getMove(kt NE MN,kt E MN),hmval)}
          if(ISE(kt NEE)) {ADDMOVE(getMove(kt NE ME,kt E MN),hmval)}
        }
      }
      else if(ISP(kt N) && GT(kt N,eloc))
      {
        if     (ISO(kt NW) && eloc == kt W && GT(kt N,kt NW) && b.wouldBeUF(pla,kt N,kt NW,kt N))
        {
          if(ISE(kt NNW)) {ADDMOVE(getMove(kt NW MN,kt N MW),hmval)}
          if(ISE(kt NWW)) {ADDMOVE(getMove(kt NW MW,kt N MW),hmval)}
        }
        else if(ISO(kt NE) && eloc == kt E && GT(kt N,kt NE) && b.wouldBeUF(pla,kt N,kt NE,kt N))
        {
          if(ISE(kt NNE)) {ADDMOVE(getMove(kt NE MN,kt N ME),hmval)}
          if(ISE(kt NEE)) {ADDMOVE(getMove(kt NE ME,kt N ME),hmval)}
        }
      }
    }
  }

  //Strong piece, but blocked. Since no 3 step captures were possible, all squares around must be doubly blocked by friendly pieces.
  else if(b.isBlocked(kt))
  {
    if(ISP(kt S)) {num += genEmpty2(b,pla,kt S,ERRLOC,kt,mv+num,hm+num,hmval);}
    if(ISP(kt W)) {num += genEmpty2(b,pla,kt W,ERRLOC,kt,mv+num,hm+num,hmval);}
    if(ISP(kt E)) {num += genEmpty2(b,pla,kt E,ERRLOC,kt,mv+num,hm+num,hmval);}
    if(ISP(kt N)) {num += genEmpty2(b,pla,kt N,ERRLOC,kt,mv+num,hm+num,hmval);}
  }
  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canRemoveDef4C(Board& b, pla_t pla, loc_t eloc, loc_t kt)
{
  if((Board::DISK[3][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return false;

  bool suc = false;
  pla_t opp = gOpp(pla);

  //Piece adjacent to eloc that can pushpull but is frozen or blocked.
  if(b.isDominated(eloc))
  {
    if(ISP(eloc S) && GT(eloc S, eloc) && b.isFrozen(eloc S))
    {
      if(canUF2PPPE(b,pla,eloc S,eloc)) {return true;}
      if(!b.isOpen2(eloc S) && canUFBlockedPP(b,pla,eloc S,eloc)) {return true;}
    }
    else if(ISP(eloc S) && GT(eloc S, eloc) && b.isThawed(eloc S) && canBlockedPP2(b,pla,eloc S,eloc,kt)) {return true;}

    if(ISP(eloc W) && GT(eloc W, eloc) && b.isFrozen(eloc W))
    {
      if(canUF2PPPE(b,pla,eloc W,eloc)) {return true;}
      if(!b.isOpen2(eloc W) && canUFBlockedPP(b,pla,eloc W,eloc)) {return true;}
    }
    else if(ISP(eloc W) && GT(eloc W, eloc) && b.isThawed(eloc W) && canBlockedPP2(b,pla,eloc W,eloc,kt)) {return true;}

    if(ISP(eloc E) && GT(eloc E, eloc) && b.isFrozen(eloc E))
    {
      if(canUF2PPPE(b,pla,eloc E,eloc)) {return true;}
      if(!b.isOpen2(eloc E) && canUFBlockedPP(b,pla,eloc E,eloc)) {return true;}
    }
    else if(ISP(eloc E) && GT(eloc E, eloc) && b.isThawed(eloc E) && canBlockedPP2(b,pla,eloc E,eloc,kt)) {return true;}

    if(ISP(eloc N) && GT(eloc N, eloc) && b.isFrozen(eloc N))
    {
      if(canUF2PPPE(b,pla,eloc N,eloc)) {return true;}
      if(!b.isOpen2(eloc N) && canUFBlockedPP(b,pla,eloc N,eloc)) {return true;}
    }
    else if(ISP(eloc N) && GT(eloc N, eloc) && b.isThawed(eloc N) && canBlockedPP2(b,pla,eloc N,eloc,kt)) {return true;}
  }

  //A step away, not going through the trap. Either must be frozen now, or be frozen after one step.
  //And if we're frozen now, the fact that we couldn't do a 3-step cap implies that we're frozen after 1 step
  if(ISE(eloc S))
  {
    if(ISP(eloc SS) && GT(eloc SS,eloc)) {
      if(b.isThawed(eloc SS)) {TS(eloc SS,eloc S,suc = canUFPPPE(b,pla,eloc S,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SS,eloc S)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SS,eloc S,eloc SS) && canUFStepWouldBeUF(b,pla,eloc SS,eloc S,kt)) {return true;}
    }
    if(ISP(eloc SW) && GT(eloc SW,eloc)) {
      if(b.isThawed(eloc SW)) {TS(eloc SW,eloc S,suc = canUFPPPE(b,pla,eloc S,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SW,eloc S)) {return true;} if(canAdvancePullCap(b,pla,eloc SW,eloc S)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SW,eloc S,eloc SW) && canUFStepWouldBeUF(b,pla,eloc SW,eloc S,kt)) {return true;}
    }
    if(ISP(eloc SE) && GT(eloc SE,eloc)) {
      if(b.isThawed(eloc SE)) {TS(eloc SE,eloc S,suc = canUFPPPE(b,pla,eloc S,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SE,eloc S)) {return true;} if(canAdvancePullCap(b,pla,eloc SE,eloc S)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SE,eloc S,eloc SE) && canUFStepWouldBeUF(b,pla,eloc SE,eloc S,kt)) {return true;}
    }
  }
  if(ISE(eloc W))
  {
    if(ISP(eloc SW) && GT(eloc SW,eloc)) {
      if(b.isThawed(eloc SW)) {TS(eloc SW,eloc W,suc = canUFPPPE(b,pla,eloc W,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SW,eloc W)) {return true;} if(canAdvancePullCap(b,pla,eloc SW,eloc W)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SW,eloc W,eloc SW) && canUFStepWouldBeUF(b,pla,eloc SW,eloc W,kt)) {return true;}
    }
    if(ISP(eloc WW) && GT(eloc WW,eloc)) {
      if(b.isThawed(eloc WW)) {TS(eloc WW,eloc W,suc = canUFPPPE(b,pla,eloc W,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc WW,eloc W)) {return true;}}
      else if(b.wouldBeUF(pla,eloc WW,eloc W,eloc WW) && canUFStepWouldBeUF(b,pla,eloc WW,eloc W,kt)) {return true;}
    }
    if(ISP(eloc NW) && GT(eloc NW,eloc)) {
      if(b.isThawed(eloc NW)) {TS(eloc NW,eloc W,suc = canUFPPPE(b,pla,eloc W,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NW,eloc W)) {return true;} if(canAdvancePullCap(b,pla,eloc NW,eloc W)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NW,eloc W,eloc NW) && canUFStepWouldBeUF(b,pla,eloc NW,eloc W,kt)) {return true;}
    }
  }
  if(ISE(eloc E))
  {
    if(ISP(eloc SE) && GT(eloc SE,eloc)) {
      if(b.isThawed(eloc SE)) {TS(eloc SE,eloc E,suc = canUFPPPE(b,pla,eloc E,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc SE,eloc E)) {return true;} if(canAdvancePullCap(b,pla,eloc SE,eloc E)) {return true;}}
      else if(b.wouldBeUF(pla,eloc SE,eloc E,eloc SE) && canUFStepWouldBeUF(b,pla,eloc SE,eloc E,kt)) {return true;}
    }
    if(ISP(eloc EE) && GT(eloc EE,eloc)) {
      if(b.isThawed(eloc EE)) {TS(eloc EE,eloc E,suc = canUFPPPE(b,pla,eloc E,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc EE,eloc E)) {return true;}}
      else if(b.wouldBeUF(pla,eloc EE,eloc E,eloc EE) && canUFStepWouldBeUF(b,pla,eloc EE,eloc E,kt)) {return true;}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc)) {
      if(b.isThawed(eloc NE)) {TS(eloc NE,eloc E,suc = canUFPPPE(b,pla,eloc E,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NE,eloc E)) {return true;} if(canAdvancePullCap(b,pla,eloc NE,eloc E)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NE,eloc E,eloc NE) && canUFStepWouldBeUF(b,pla,eloc NE,eloc E,kt)) {return true;}
    }
  }
  if(ISE(eloc N))
  {
    if(ISP(eloc NW) && GT(eloc NW,eloc)) {
      if(b.isThawed(eloc NW)) {TS(eloc NW,eloc N,suc = canUFPPPE(b,pla,eloc N,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NW,eloc N)) {return true;} if(canAdvancePullCap(b,pla,eloc NW,eloc N)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NW,eloc N,eloc NW) && canUFStepWouldBeUF(b,pla,eloc NW,eloc N,kt)) {return true;}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc)) {
      if(b.isThawed(eloc NE)) {TS(eloc NE,eloc N,suc = canUFPPPE(b,pla,eloc N,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NE,eloc N)) {return true;} if(canAdvancePullCap(b,pla,eloc NE,eloc N)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NE,eloc N,eloc NE) && canUFStepWouldBeUF(b,pla,eloc NE,eloc N,kt)) {return true;}
    }
    if(ISP(eloc NN) && GT(eloc NN,eloc)) {
      if(b.isThawed(eloc NN)) {TS(eloc NN,eloc N,suc = canUFPPPE(b,pla,eloc N,eloc)) if(suc) {return true;} if(canAdvanceUFStep(b,pla,eloc NN,eloc N)) {return true;}}
      else if(b.wouldBeUF(pla,eloc NN,eloc N,eloc NN) && canUFStepWouldBeUF(b,pla,eloc NN,eloc N,kt)) {return true;}
    }
  }

  //Friendly piece in the way
  if(ISP(eloc S) && canSwapPla(b,pla,eloc S,eloc)) {return true;}
  if(ISP(eloc W) && canSwapPla(b,pla,eloc W,eloc)) {return true;}
  if(ISP(eloc E) && canSwapPla(b,pla,eloc E,eloc)) {return true;}
  if(ISP(eloc N) && canSwapPla(b,pla,eloc N,eloc)) {return true;}

  //Opponent's piece in the way
  if(ISO(eloc S) && eloc S != kt && canSwapOpp(b,pla,eloc S,eloc,kt)) {return true;}
  if(ISO(eloc W) && eloc W != kt && canSwapOpp(b,pla,eloc W,eloc,kt)) {return true;}
  if(ISO(eloc E) && eloc E != kt && canSwapOpp(b,pla,eloc E,eloc,kt)) {return true;}
  if(ISO(eloc N) && eloc N != kt && canSwapOpp(b,pla,eloc N,eloc,kt)) {return true;}

  //2 Steps away
  if(ISE(eloc S) && canMoveStrong2S(b,pla,eloc S,eloc)) {return true;}
  if(ISE(eloc W) && canMoveStrong2S(b,pla,eloc W,eloc)) {return true;}
  if(ISE(eloc E) && canMoveStrong2S(b,pla,eloc E,eloc)) {return true;}
  if(ISE(eloc N) && canMoveStrong2S(b,pla,eloc N,eloc)) {return true;}

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genRemoveDef4C(Board& b, pla_t pla, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  if((Board::DISK[3][eloc] & b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB]).isEmpty())
    return 0;

  bool suc = false;
  int num = 0;
  pla_t opp = gOpp(pla);

  //Piece adjacent to eloc that can pushpull but is frozen or blocked.
  if(b.isDominated(eloc))
  {
    if(ISP(eloc S) && GT(eloc S, eloc))
    {
      if(b.isFrozen(eloc S))
      {
        num += genUF2PPPE(b,pla,eloc S,eloc,mv+num,hm+num,hmval);
        if(!b.isOpen2(eloc S))
        {num += genUFBlockedPP(b,pla,eloc S,eloc,mv+num,hm+num,hmval);}
      }
      else {num += genBlockedPP2(b,pla,eloc S,eloc,kt,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc W) && GT(eloc W, eloc))
    {
      if(b.isFrozen(eloc W))
      {
        num += genUF2PPPE(b,pla,eloc W,eloc,mv+num,hm+num,hmval);
        if(!b.isOpen2(eloc W))
        {num += genUFBlockedPP(b,pla,eloc W,eloc,mv+num,hm+num,hmval);}
      }
      else {num += genBlockedPP2(b,pla,eloc W,eloc,kt,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc E) && GT(eloc E, eloc))
    {
      if(b.isFrozen(eloc E))
      {
        num += genUF2PPPE(b,pla,eloc E,eloc,mv+num,hm+num,hmval);
        if(!b.isOpen2(eloc E))
        {num += genUFBlockedPP(b,pla,eloc E,eloc,mv+num,hm+num,hmval);}
      }
      else {num += genBlockedPP2(b,pla,eloc E,eloc,kt,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc N) && GT(eloc N, eloc))
    {
      if(b.isFrozen(eloc N))
      {
        num += genUF2PPPE(b,pla,eloc N,eloc,mv+num,hm+num,hmval);
        if(!b.isOpen2(eloc N))
        {num += genUFBlockedPP(b,pla,eloc N,eloc,mv+num,hm+num,hmval);}
      }
      else {num += genBlockedPP2(b,pla,eloc N,eloc,kt,mv+num,hm+num,hmval);}
    }
  }
  //A step away, not going through the trap. Either must be frozen now, or be frozen after one step if a 4-step cap is possible
  //but a 3-step cap wasn't possible.
  //We do actually go ahead and test the conditions, becomes sometimes this function will be called when a 3-step cap IS possible,
  //and it's stupid to generate unfreezes of a piece that isn't frozen.
  if(ISE(eloc S))
  {
    if(ISP(eloc SS) && GT(eloc SS,eloc)) {
      if(b.wouldBeUF(pla,eloc SS,eloc S,eloc SS)) {if(b.isFrozen(eloc SS)) num += genUFStepWouldBeUF(b,pla,eloc SS,eloc S,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SS)) {TS(eloc SS,eloc S,suc = canUFPPPE(b,pla,eloc S,eloc)) if(suc) {ADDMOVE(getMove(eloc SS MN),hmval)} num += genAdvanceUFStep(b,pla,eloc SS,eloc S,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc SW) && GT(eloc SW,eloc)) {
      if(b.wouldBeUF(pla,eloc SW,eloc S,eloc SW)) {if(b.isFrozen(eloc SW)) num += genUFStepWouldBeUF(b,pla,eloc SW,eloc S,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SW)) {TS(eloc SW,eloc S,suc = canUFPPPE(b,pla,eloc S,eloc)) if(suc) {ADDMOVE(getMove(eloc SW ME),hmval)} num += genAdvanceUFStep(b,pla,eloc SW,eloc S,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SW,eloc S,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc SE) && GT(eloc SE,eloc)) {
      if(b.wouldBeUF(pla,eloc SE,eloc S,eloc SE)) {if(b.isFrozen(eloc SE)) num += genUFStepWouldBeUF(b,pla,eloc SE,eloc S,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SE)) {TS(eloc SE,eloc S,suc = canUFPPPE(b,pla,eloc S,eloc)) if(suc) {ADDMOVE(getMove(eloc SE MW),hmval)} num += genAdvanceUFStep(b,pla,eloc SE,eloc S,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SE,eloc S,mv+num,hm+num,hmval);}
    }
  }
  if(ISE(eloc W))
  {
    if(ISP(eloc SW) && GT(eloc SW,eloc)) {
      if(b.wouldBeUF(pla,eloc SW,eloc W,eloc SW)) {if(b.isFrozen(eloc SW)) num += genUFStepWouldBeUF(b,pla,eloc SW,eloc W,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SW)) {TS(eloc SW,eloc W,suc = canUFPPPE(b,pla,eloc W,eloc)) if(suc) {ADDMOVE(getMove(eloc SW MN),hmval)} num += genAdvanceUFStep(b,pla,eloc SW,eloc W,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SW,eloc W,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc WW) && GT(eloc WW,eloc)) {
      if(b.wouldBeUF(pla,eloc WW,eloc W,eloc WW)) {if(b.isFrozen(eloc WW)) num += genUFStepWouldBeUF(b,pla,eloc WW,eloc W,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc WW)) {TS(eloc WW,eloc W,suc = canUFPPPE(b,pla,eloc W,eloc)) if(suc) {ADDMOVE(getMove(eloc WW ME),hmval)} num += genAdvanceUFStep(b,pla,eloc WW,eloc W,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NW) && GT(eloc NW,eloc)) {
      if(b.wouldBeUF(pla,eloc NW,eloc W,eloc NW)) {if(b.isFrozen(eloc NW)) num += genUFStepWouldBeUF(b,pla,eloc NW,eloc W,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NW)) {TS(eloc NW,eloc W,suc = canUFPPPE(b,pla,eloc W,eloc)) if(suc) {ADDMOVE(getMove(eloc NW MS),hmval)} num += genAdvanceUFStep(b,pla,eloc NW,eloc W,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NW,eloc W,mv+num,hm+num,hmval);}
    }
  }
  if(ISE(eloc E))
  {
    if(ISP(eloc SE) && GT(eloc SE,eloc)) {
      if(b.wouldBeUF(pla,eloc SE,eloc E,eloc SE)) {if(b.isFrozen(eloc SE)) num += genUFStepWouldBeUF(b,pla,eloc SE,eloc E,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc SE)) {TS(eloc SE,eloc E,suc = canUFPPPE(b,pla,eloc E,eloc)) if(suc) {ADDMOVE(getMove(eloc SE MN),hmval)} num += genAdvanceUFStep(b,pla,eloc SE,eloc E,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc SE,eloc E,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc EE) && GT(eloc EE,eloc)) {
      if(b.wouldBeUF(pla,eloc EE,eloc E,eloc EE)) {if(b.isFrozen(eloc EE)) num += genUFStepWouldBeUF(b,pla,eloc EE,eloc E,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc EE)) {TS(eloc EE,eloc E,suc = canUFPPPE(b,pla,eloc E,eloc)) if(suc) {ADDMOVE(getMove(eloc EE MW),hmval)} num += genAdvanceUFStep(b,pla,eloc EE,eloc E,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc)) {
      if(b.wouldBeUF(pla,eloc NE,eloc E,eloc NE)) {if(b.isFrozen(eloc NE)) num += genUFStepWouldBeUF(b,pla,eloc NE,eloc E,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NE)) {TS(eloc NE,eloc E,suc = canUFPPPE(b,pla,eloc E,eloc)) if(suc) {ADDMOVE(getMove(eloc NE MS),hmval)} num += genAdvanceUFStep(b,pla,eloc NE,eloc E,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NE,eloc E,mv+num,hm+num,hmval);}
    }
  }
  if(ISE(eloc N))
  {
    if(ISP(eloc NW) && GT(eloc NW,eloc)) {
      if(b.wouldBeUF(pla,eloc NW,eloc N,eloc NW)) {if(b.isFrozen(eloc NW)) num += genUFStepWouldBeUF(b,pla,eloc NW,eloc N,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NW)) {TS(eloc NW,eloc N,suc = canUFPPPE(b,pla,eloc N,eloc)) if(suc) {ADDMOVE(getMove(eloc NW ME),hmval)} num += genAdvanceUFStep(b,pla,eloc NW,eloc N,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NW,eloc N,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NE) && GT(eloc NE,eloc)) {
      if(b.wouldBeUF(pla,eloc NE,eloc N,eloc NE)) {if(b.isFrozen(eloc NE)) num += genUFStepWouldBeUF(b,pla,eloc NE,eloc N,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NE)) {TS(eloc NE,eloc N,suc = canUFPPPE(b,pla,eloc N,eloc)) if(suc) {ADDMOVE(getMove(eloc NE MW),hmval)} num += genAdvanceUFStep(b,pla,eloc NE,eloc N,mv+num,hm+num,hmval); num += genAdvancePullCap(b,pla,eloc NE,eloc N,mv+num,hm+num,hmval);}
    }
    if(ISP(eloc NN) && GT(eloc NN,eloc)) {
      if(b.wouldBeUF(pla,eloc NN,eloc N,eloc NN)) {if(b.isFrozen(eloc NN)) num += genUFStepWouldBeUF(b,pla,eloc NN,eloc N,kt,mv+num,hm+num,hmval);}
      else if(b.isThawed(eloc NN)) {TS(eloc NN,eloc N,suc = canUFPPPE(b,pla,eloc N,eloc)) if(suc) {ADDMOVE(getMove(eloc NN MS),hmval)} num += genAdvanceUFStep(b,pla,eloc NN,eloc N,mv+num,hm+num,hmval);}
    }
  }

  //Friendly piece in the way
  if(ISP(eloc S)) {num += genSwapPla(b,pla,eloc S,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc W)) {num += genSwapPla(b,pla,eloc W,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc E)) {num += genSwapPla(b,pla,eloc E,eloc,mv+num,hm+num,hmval);}
  if(ISP(eloc N)) {num += genSwapPla(b,pla,eloc N,eloc,mv+num,hm+num,hmval);}

  //Opponent's piece in the way
  if(ISO(eloc S) && eloc S != kt) {num += genSwapOpp(b,pla,eloc S,eloc,kt,mv+num,hm+num,hmval);}
  if(ISO(eloc W) && eloc W != kt) {num += genSwapOpp(b,pla,eloc W,eloc,kt,mv+num,hm+num,hmval);}
  if(ISO(eloc E) && eloc E != kt) {num += genSwapOpp(b,pla,eloc E,eloc,kt,mv+num,hm+num,hmval);}
  if(ISO(eloc N) && eloc N != kt) {num += genSwapOpp(b,pla,eloc N,eloc,kt,mv+num,hm+num,hmval);}

  //2 Steps away
  if(ISE(eloc S)) {num += genMoveStrong2S(b,pla,eloc S,eloc,mv+num,hm+num,hmval);}
  if(ISE(eloc W)) {num += genMoveStrong2S(b,pla,eloc W,eloc,mv+num,hm+num,hmval);}
  if(ISE(eloc E)) {num += genMoveStrong2S(b,pla,eloc E,eloc,mv+num,hm+num,hmval);}
  if(ISE(eloc N)) {num += genMoveStrong2S(b,pla,eloc N,eloc,mv+num,hm+num,hmval);}

  return num;
}

//Tries to empty the location in 2 steps.
//Assumes loc is not a trap location
//Does not push opponent pieces on floc
//Does not pull with player pieces adjacent to floc2
//Does not push opponent pieces adjacent to floc
//Ensures we do not freeze ploc when pulling an opponent, if loc is an opponent
//Does NOT use the C version of thawed or frozen!
static bool canEmpty2(Board& b, pla_t pla, loc_t loc, loc_t floc, loc_t ploc)
{
  pla_t opp = gOpp(pla);

  if(ISP(loc))
  {
    if(!b.isDominated(loc) || b.isGuarded2(pla,loc))
    {
      if(ISP(loc S) && b.isRabOkayS(pla,loc) && canSteps1S(b,pla,loc S)) {return true;}
      if(ISP(loc W)                          && canSteps1S(b,pla,loc W)) {return true;}
      if(ISP(loc E)                          && canSteps1S(b,pla,loc E)) {return true;}
      if(ISP(loc N) && b.isRabOkayN(pla,loc) && canSteps1S(b,pla,loc N)) {return true;}
    }

    if(b.isFrozen(loc))
    {
      if(ISE(loc S) && ((ISE(loc W)                         ) || (ISE(loc E)) || (ISE(loc N) && b.isRabOkayN(pla,loc)))) {if(canUFAt(b,pla,loc S)) {return true;}}
      if(ISE(loc W) && ((ISE(loc S) && b.isRabOkayS(pla,loc)) || (ISE(loc E)) || (ISE(loc N) && b.isRabOkayN(pla,loc)))) {if(canUFAt(b,pla,loc W)) {return true;}}
      if(ISE(loc E) && ((ISE(loc S) && b.isRabOkayS(pla,loc)) || (ISE(loc W)) || (ISE(loc N) && b.isRabOkayN(pla,loc)))) {if(canUFAt(b,pla,loc E)) {return true;}}
      if(ISE(loc N) && ((ISE(loc S) && b.isRabOkayS(pla,loc)) || (ISE(loc W)) || (ISE(loc E)                         ))) {if(canUFAt(b,pla,loc N)) {return true;}}
    }
    else
    {
      if(ISO(loc S) && loc S != floc && GT(loc,loc S) && canPushE(b,loc S,floc)) {return true;}
      if(ISO(loc W) && loc W != floc && GT(loc,loc W) && canPushE(b,loc W,floc)) {return true;}
      if(ISO(loc E) && loc E != floc && GT(loc,loc E) && canPushE(b,loc E,floc)) {return true;}
      if(ISO(loc N) && loc N != floc && GT(loc,loc N) && canPushE(b,loc N,floc)) {return true;}
    }
  }
  else if(ISO(loc) && b.isDominated(loc))
  {
    if(ISP(loc S) && (floc == ERRLOC || !b.isAdjacent(loc S,floc)) && GT(loc S,loc) && b.isThawed(loc S) && (!b.isAdjacent(loc S,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc)) && b.isOpen(loc S)){return true;}
    if(ISP(loc W) && (floc == ERRLOC || !b.isAdjacent(loc W,floc)) && GT(loc W,loc) && b.isThawed(loc W) && (!b.isAdjacent(loc W,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc)) && b.isOpen(loc W)){return true;}
    if(ISP(loc E) && (floc == ERRLOC || !b.isAdjacent(loc E,floc)) && GT(loc E,loc) && b.isThawed(loc E) && (!b.isAdjacent(loc E,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc)) && b.isOpen(loc E)){return true;}
    if(ISP(loc N) && (floc == ERRLOC || !b.isAdjacent(loc N,floc)) && GT(loc N,loc) && b.isThawed(loc N) && (!b.isAdjacent(loc N,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc)) && b.isOpen(loc N)){return true;}
  }

  return false;
}

//Tries to empty the location in 2 steps.
//Assumes loc is not a trap location
//Does not push opponent pieces on floc
//Does not pull with player pieces adjacent to floc2
//Does not push opponent pieces adjacent to floc
//Ensures we do not freeze ploc when pulling an opponent, if loc is an opponent
//Does NOT use the C version of thawed or frozen!
static int genEmpty2(Board& b, pla_t pla, loc_t loc, loc_t floc, loc_t ploc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  pla_t opp = gOpp(pla);

  if(ISP(loc))
  {
    if(!b.isDominated(loc) || b.isGuarded2(pla,loc))
    {
      if(ISP(loc S) && b.isRabOkayS(pla,loc)) {num += genSteps1S(b,pla,loc S,mv+num,hm+num,hmval);}
      if(ISP(loc W))                          {num += genSteps1S(b,pla,loc W,mv+num,hm+num,hmval);}
      if(ISP(loc E))                          {num += genSteps1S(b,pla,loc E,mv+num,hm+num,hmval);}
      if(ISP(loc N) && b.isRabOkayN(pla,loc)) {num += genSteps1S(b,pla,loc N,mv+num,hm+num,hmval);}
    }

    if(b.isFrozen(loc))
    {
      if(ISE(loc S) && ((ISE(loc W)                         ) || (ISE(loc E)) || (ISE(loc N) && b.isRabOkayN(pla,loc)))) {num += genUFAt(b,pla,loc S,mv+num,hm+num,hmval);}
      if(ISE(loc W) && ((ISE(loc S) && b.isRabOkayS(pla,loc)) || (ISE(loc E)) || (ISE(loc N) && b.isRabOkayN(pla,loc)))) {num += genUFAt(b,pla,loc W,mv+num,hm+num,hmval);}
      if(ISE(loc E) && ((ISE(loc S) && b.isRabOkayS(pla,loc)) || (ISE(loc W)) || (ISE(loc N) && b.isRabOkayN(pla,loc)))) {num += genUFAt(b,pla,loc E,mv+num,hm+num,hmval);}
      if(ISE(loc N) && ((ISE(loc S) && b.isRabOkayS(pla,loc)) || (ISE(loc W)) || (ISE(loc E)                         ))) {num += genUFAt(b,pla,loc N,mv+num,hm+num,hmval);}
    }
    else
    {
      if(ISO(loc S) && loc S != floc && GT(loc,loc S)) {num += genPushPE(b,loc,loc S,floc,mv+num,hm+num,hmval);}
      if(ISO(loc W) && loc W != floc && GT(loc,loc W)) {num += genPushPE(b,loc,loc W,floc,mv+num,hm+num,hmval);}
      if(ISO(loc E) && loc E != floc && GT(loc,loc E)) {num += genPushPE(b,loc,loc E,floc,mv+num,hm+num,hmval);}
      if(ISO(loc N) && loc N != floc && GT(loc,loc N)) {num += genPushPE(b,loc,loc N,floc,mv+num,hm+num,hmval);}
    }
  }
  else if(ISO(loc) && b.isDominated(loc))
  {
    if(ISP(loc S) && (floc == ERRLOC || !b.isAdjacent(loc S,floc)) && GT(loc S,loc) && b.isThawed(loc S) && (!b.isAdjacent(loc S,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc))) {num += genPullPE(b,loc S,loc,mv+num,hm+num,hmval);}
    if(ISP(loc W) && (floc == ERRLOC || !b.isAdjacent(loc W,floc)) && GT(loc W,loc) && b.isThawed(loc W) && (!b.isAdjacent(loc W,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc))) {num += genPullPE(b,loc W,loc,mv+num,hm+num,hmval);}
    if(ISP(loc E) && (floc == ERRLOC || !b.isAdjacent(loc E,floc)) && GT(loc E,loc) && b.isThawed(loc E) && (!b.isAdjacent(loc E,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc))) {num += genPullPE(b,loc E,loc,mv+num,hm+num,hmval);}
    if(ISP(loc N) && (floc == ERRLOC || !b.isAdjacent(loc N,floc)) && GT(loc N,loc) && b.isThawed(loc N) && (!b.isAdjacent(loc N,ploc) || (!b.isDominated(ploc) && !GT(loc,ploc)) || b.isGuarded2(pla,ploc))) {num += genPullPE(b,loc N,loc,mv+num,hm+num,hmval);}
  }

  return num;
}

//Does NOT use the C version of thawed or frozen!
static bool canUFBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc)
{
  bool suc = false;
  pla_t opp = gOpp(pla);

  if(ISE(ploc S))
  {
    if(ISP(ploc SS) && b.isThawed(ploc SS) && b.isRabOkayN(pla,ploc SS)) {TS(ploc SS,ploc S,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc SW) && b.isThawed(ploc SW))                              {TS(ploc SW,ploc S,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc SE) && b.isThawed(ploc SE))                              {TS(ploc SE,ploc S,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISE(ploc W))
  {
    if(ISP(ploc SW) && b.isThawed(ploc SW) && b.isRabOkayN(pla,ploc SW)) {TS(ploc SW,ploc W,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc WW) && b.isThawed(ploc WW))                              {TS(ploc WW,ploc W,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NW) && b.isThawed(ploc NW) && b.isRabOkayS(pla,ploc NW)) {TS(ploc NW,ploc W,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISE(ploc E))
  {
    if(ISP(ploc SE) && b.isThawed(ploc SE) && b.isRabOkayN(pla,ploc SE)) {TS(ploc SE,ploc E,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc EE) && b.isThawed(ploc EE))                              {TS(ploc EE,ploc E,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NE) && b.isThawed(ploc NE) && b.isRabOkayS(pla,ploc NE)) {TS(ploc NE,ploc E,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
  }
  if(ISE(ploc N))
  {
    if(ISP(ploc NW) && b.isThawed(ploc NW))                              {TS(ploc NW,ploc N,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NE) && b.isThawed(ploc NE))                              {TS(ploc NE,ploc N,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
    if(ISP(ploc NN) && b.isThawed(ploc NN) && b.isRabOkayS(pla,ploc NN)) {TS(ploc NN,ploc N,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {return true;}}
  }

  //Pull at the same time as unfreezing to unblock
  if(ISP(ploc SW) && b.isThawed(ploc SW))
  {
    if(ISO(ploc S) && GT(ploc SW,ploc S) && ISE(ploc W)) {return true;}
    if(ISO(ploc W) && GT(ploc SW,ploc W) && ISE(ploc S)) {return true;}
  }
  if(ISP(ploc SE) && b.isThawed(ploc SE))
  {
    if(ISO(ploc S) && GT(ploc SE,ploc S) && ISE(ploc E)) {return true;}
    if(ISO(ploc E) && GT(ploc SE,ploc E) && ISE(ploc S)) {return true;}
  }
  if(ISP(ploc NW) && b.isThawed(ploc NW))
  {
    if(ISO(ploc N) && GT(ploc NW,ploc N) && ISE(ploc W)) {return true;}
    if(ISO(ploc W) && GT(ploc NW,ploc W) && ISE(ploc N)) {return true;}
  }
  if(ISP(ploc NE) && b.isThawed(ploc NE))
  {
    if(ISO(ploc N) && GT(ploc NE,ploc N) && ISE(ploc E)) {return true;}
    if(ISO(ploc E) && GT(ploc NE,ploc E) && ISE(ploc N)) {return true;}
  }

  return false;
}

//Does NOT use the C version of thawed or frozen!
static int genUFBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;
  bool suc = false;
  pla_t opp = gOpp(pla);

  if(ISE(ploc S))
  {
    if(ISP(ploc SS) && b.isThawed(ploc SS) && b.isRabOkayN(pla,ploc SS)) {TS(ploc SS,ploc S,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SS MN),hmval)}}
    if(ISP(ploc SW) && b.isThawed(ploc SW))                              {TS(ploc SW,ploc S,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SW ME),hmval)}}
    if(ISP(ploc SE) && b.isThawed(ploc SE))                              {TS(ploc SE,ploc S,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SE MW),hmval)}}
  }
  if(ISE(ploc W))
  {
    if(ISP(ploc SW) && b.isThawed(ploc SW) && b.isRabOkayN(pla,ploc SW)) {TS(ploc SW,ploc W,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SW MN),hmval)}}
    if(ISP(ploc WW) && b.isThawed(ploc WW))                              {TS(ploc WW,ploc W,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc WW ME),hmval)}}
    if(ISP(ploc NW) && b.isThawed(ploc NW) && b.isRabOkayS(pla,ploc NW)) {TS(ploc NW,ploc W,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NW MS),hmval)}}
  }
  if(ISE(ploc E))
  {
    if(ISP(ploc SE) && b.isThawed(ploc SE) && b.isRabOkayN(pla,ploc SE)) {TS(ploc SE,ploc E,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc SE MN),hmval)}}
    if(ISP(ploc EE) && b.isThawed(ploc EE))                              {TS(ploc EE,ploc E,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc EE MW),hmval)}}
    if(ISP(ploc NE) && b.isThawed(ploc NE) && b.isRabOkayS(pla,ploc NE)) {TS(ploc NE,ploc E,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NE MS),hmval)}}
  }
  if(ISE(ploc N))
  {
    if(ISP(ploc NW) && b.isThawed(ploc NW))                              {TS(ploc NW,ploc N,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NW ME),hmval)}}
    if(ISP(ploc NE) && b.isThawed(ploc NE))                              {TS(ploc NE,ploc N,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NE MW),hmval)}}
    if(ISP(ploc NN) && b.isThawed(ploc NN) && b.isRabOkayS(pla,ploc NN)) {TS(ploc NN,ploc N,suc = canBlockedPP(b,pla,ploc,eloc)) if(suc) {ADDMOVEPP(getMove(ploc NN MS),hmval)}}
  }

  //Pull at the same time as unfreezing to unblock
  if(ISP(ploc SW) && b.isThawed(ploc SW))
  {
    if(ISO(ploc S) && GT(ploc SW,ploc S) && ISE(ploc W)) {ADDMOVEPP(getMove(ploc SW MN,ploc S MW),hmval)}
    if(ISO(ploc W) && GT(ploc SW,ploc W) && ISE(ploc S)) {ADDMOVEPP(getMove(ploc SW ME,ploc W MS),hmval)}
  }
  if(ISP(ploc SE) && b.isThawed(ploc SE))
  {
    if(ISO(ploc S) && GT(ploc SE,ploc S) && ISE(ploc E)) {ADDMOVEPP(getMove(ploc SE MN,ploc S ME),hmval)}
    if(ISO(ploc E) && GT(ploc SE,ploc E) && ISE(ploc S)) {ADDMOVEPP(getMove(ploc SE MW,ploc E MS),hmval)}
  }
  if(ISP(ploc NW) && b.isThawed(ploc NW))
  {
    if(ISO(ploc N) && GT(ploc NW,ploc N) && ISE(ploc W)) {ADDMOVEPP(getMove(ploc NW MS,ploc N MW),hmval)}
    if(ISO(ploc W) && GT(ploc NW,ploc W) && ISE(ploc N)) {ADDMOVEPP(getMove(ploc NW ME,ploc W MN),hmval)}
  }
  if(ISP(ploc NE) && b.isThawed(ploc NE))
  {
    if(ISO(ploc N) && GT(ploc NE,ploc N) && ISE(ploc E)) {ADDMOVEPP(getMove(ploc NE MS,ploc N ME),hmval)}
    if(ISO(ploc E) && GT(ploc NE,ploc E) && ISE(ploc N)) {ADDMOVEPP(getMove(ploc NE MW,ploc E MN),hmval)}
  }

  return num;
}

//Generates all possible 3-step pushpulls of a defender of the trap by a player piece, when blocked by other pieces.
//Assumes the player piece and enemy pieces are there, the player piece is large enough, and is currently unfrozen.
static bool canBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc)
{
  //Push
  if(eloc S != ploc && ISP(eloc S) && b.isThawedC(eloc S) && canSteps1S(b,pla,eloc S)) {return true;}
  if(eloc W != ploc && ISP(eloc W) && b.isThawedC(eloc W) && canSteps1S(b,pla,eloc W)) {return true;}
  if(eloc E != ploc && ISP(eloc E) && b.isThawedC(eloc E) && canSteps1S(b,pla,eloc E)) {return true;}
  if(eloc N != ploc && ISP(eloc N) && b.isThawedC(eloc N) && canSteps1S(b,pla,eloc N)) {return true;}

  //Pull
  if(!b.isDominatedC(ploc) || b.isGuarded2(pla,ploc))
  {
    if(ISP(ploc S) && canSteps1S(b,pla,ploc S)) {return true;}
    if(ISP(ploc W) && canSteps1S(b,pla,ploc W)) {return true;}
    if(ISP(ploc E) && canSteps1S(b,pla,ploc E)) {return true;}
    if(ISP(ploc N) && canSteps1S(b,pla,ploc N)) {return true;}
  }

  return false;
}

//Generates all possible 3-step pushpulls of a defender of the trap by a player piece, when blocked by other pieces.
//Assumes the player piece and enemy pieces are there, the player piece is large enough, and is currently unfrozen.
static int genBlockedPP(Board& b, pla_t pla, loc_t ploc, loc_t eloc,
move_t* mv, int* hm, int hmval)
{
  int num = 0;

  //Push
  step_t pushstep = gStepSrcDest(ploc,eloc);
  if(eloc S != ploc && ISP(eloc S) && b.isThawedC(eloc S)) {int newnum = num + genSteps1S(b,pla,eloc S,mv+num,hm+num,hmval); SUFFIXMOVES(getMove(eloc MS,pushstep),1)}
  if(eloc W != ploc && ISP(eloc W) && b.isThawedC(eloc W)) {int newnum = num + genSteps1S(b,pla,eloc W,mv+num,hm+num,hmval); SUFFIXMOVES(getMove(eloc MW,pushstep),1)}
  if(eloc E != ploc && ISP(eloc E) && b.isThawedC(eloc E)) {int newnum = num + genSteps1S(b,pla,eloc E,mv+num,hm+num,hmval); SUFFIXMOVES(getMove(eloc ME,pushstep),1)}
  if(eloc N != ploc && ISP(eloc N) && b.isThawedC(eloc N)) {int newnum = num + genSteps1S(b,pla,eloc N,mv+num,hm+num,hmval); SUFFIXMOVES(getMove(eloc MN,pushstep),1)}

  //Pull
  if(!b.isDominatedC(ploc) || b.isGuarded2(pla,ploc))
  {
    step_t pullstep = gStepSrcDest(eloc,ploc);
    //Normally, we call suffixmoves to append the expected pull move on to the step that clears space for that pull move.
    //However, in the case where the step was by a piece that wasn't trapsafe, go ahead and avoid suffixmoving so that
    //on a later recursion, we can append any subsequent pushpull since the clearing-space step already has the use of
    //avoiding saccing a piece.
    if(ISP(ploc S)) {int newnum = num + genSteps1S(b,pla,ploc S,mv+num,hm+num,hmval); if(!b.isTrapSafe2(pla,ploc S)){num = newnum;} else {SUFFIXMOVES(getMove(ploc MS,pullstep),1)}}
    if(ISP(ploc W)) {int newnum = num + genSteps1S(b,pla,ploc W,mv+num,hm+num,hmval); if(!b.isTrapSafe2(pla,ploc W)){num = newnum;} else {SUFFIXMOVES(getMove(ploc MW,pullstep),1)}}
    if(ISP(ploc E)) {int newnum = num + genSteps1S(b,pla,ploc E,mv+num,hm+num,hmval); if(!b.isTrapSafe2(pla,ploc E)){num = newnum;} else {SUFFIXMOVES(getMove(ploc ME,pullstep),1)}}
    if(ISP(ploc N)) {int newnum = num + genSteps1S(b,pla,ploc N,mv+num,hm+num,hmval); if(!b.isTrapSafe2(pla,ploc N)){num = newnum;} else {SUFFIXMOVES(getMove(ploc MN,pullstep),1)}}
  }

  return num;
}

//Generates all possible 4-step pushpulls of a defender of the trap by a player piece, when blocked by other pieces.
//Assumes the player piece and enemy pieces are there, the player piece is large enough, and is currently unfrozen.
//Don't need to worry about the rearrangements causing ploc to unfreeze, should be caught in genUnfreezes2
//Does NOT use the C version of thawed or frozen!
static bool canBlockedPP2(Board& b, pla_t pla, loc_t ploc, loc_t eloc, loc_t kt)
{
  pla_t opp = gOpp(pla);

  //Clear space around eloc for pushing, but not the target on the trap!
  if(CS1(eloc) && eloc S != ploc && eloc S != kt && canEmpty2(b,pla,eloc S,kt,ploc)) {return true;}
  if(CW1(eloc) && eloc W != ploc && eloc W != kt && canEmpty2(b,pla,eloc W,kt,ploc)) {return true;}
  if(CE1(eloc) && eloc E != ploc && eloc E != kt && canEmpty2(b,pla,eloc E,kt,ploc)) {return true;}
  if(CN1(eloc) && eloc N != ploc && eloc N != kt && canEmpty2(b,pla,eloc N,kt,ploc)) {return true;}

  //Clear space around ploc for pulling
  if(CS1(ploc) && ploc S != eloc && b.wouldBeUF(pla,ploc,ploc,ploc S) && canEmpty2(b,pla,ploc S,kt,ploc)) {return true;}
  if(CW1(ploc) && ploc W != eloc && b.wouldBeUF(pla,ploc,ploc,ploc W) && canEmpty2(b,pla,ploc W,kt,ploc)) {return true;}
  if(CE1(ploc) && ploc E != eloc && b.wouldBeUF(pla,ploc,ploc,ploc E) && canEmpty2(b,pla,ploc E,kt,ploc)) {return true;}
  if(CN1(ploc) && ploc N != eloc && b.wouldBeUF(pla,ploc,ploc,ploc N) && canEmpty2(b,pla,ploc N,kt,ploc)) {return true;}

  //Clear space around ploc by capture
  if(ploc S != eloc && ISO(ploc S) && !b.isTrapSafe2(opp,ploc S) && canPPCapAndUFc(b,pla,findAdjOpp(b,pla,ploc S),ploc)) {return true;}
  if(ploc W != eloc && ISO(ploc W) && !b.isTrapSafe2(opp,ploc W) && canPPCapAndUFc(b,pla,findAdjOpp(b,pla,ploc W),ploc)) {return true;}
  if(ploc E != eloc && ISO(ploc E) && !b.isTrapSafe2(opp,ploc E) && canPPCapAndUFc(b,pla,findAdjOpp(b,pla,ploc E),ploc)) {return true;}
  if(ploc N != eloc && ISO(ploc N) && !b.isTrapSafe2(opp,ploc N) && canPPCapAndUFc(b,pla,findAdjOpp(b,pla,ploc N),ploc)) {return true;}

  return false;
}

//Generates all possible 4-step pushpulls of a defender of the trap by a player piece, when blocked by other pieces.
//Assumes the player piece and enemy pieces are there, the player piece is large enough, and is currently unfrozen.
//Don't need to worry about the rearrangements causing ploc to unfreeze, should be caught in genUnfreezes2
//Does NOT use the C version of thawed or frozen!
static int genBlockedPP2(Board& b, pla_t pla, loc_t ploc, loc_t eloc, loc_t kt,
move_t* mv, int* hm, int hmval)
{
  pla_t opp = gOpp(pla);
  int num = 0;

  //Clear space around eloc for pushing, but not the target on the trap!
  if(CS1(eloc) && eloc S != ploc && eloc S != kt) {num += genEmpty2(b,pla,eloc S,kt,ploc,mv+num,hm+num,hmval);}
  if(CW1(eloc) && eloc W != ploc && eloc W != kt) {num += genEmpty2(b,pla,eloc W,kt,ploc,mv+num,hm+num,hmval);}
  if(CE1(eloc) && eloc E != ploc && eloc E != kt) {num += genEmpty2(b,pla,eloc E,kt,ploc,mv+num,hm+num,hmval);}
  if(CN1(eloc) && eloc N != ploc && eloc N != kt) {num += genEmpty2(b,pla,eloc N,kt,ploc,mv+num,hm+num,hmval);}

  //Clear space around ploc for pulling
  if(CS1(ploc) && ploc S != eloc && b.wouldBeUF(pla,ploc,ploc,ploc S)) {num += genEmpty2(b,pla,ploc S,kt,ploc,mv+num,hm+num,hmval);}
  if(CW1(ploc) && ploc W != eloc && b.wouldBeUF(pla,ploc,ploc,ploc W)) {num += genEmpty2(b,pla,ploc W,kt,ploc,mv+num,hm+num,hmval);}
  if(CE1(ploc) && ploc E != eloc && b.wouldBeUF(pla,ploc,ploc,ploc E)) {num += genEmpty2(b,pla,ploc E,kt,ploc,mv+num,hm+num,hmval);}
  if(CN1(ploc) && ploc N != eloc && b.wouldBeUF(pla,ploc,ploc,ploc N)) {num += genEmpty2(b,pla,ploc N,kt,ploc,mv+num,hm+num,hmval);}

  //Clear space around ploc by capture
  if(ploc S != eloc && ISO(ploc S) && !b.isTrapSafe2(opp,ploc S)) {num += genPPCapAndUF(b,pla,findAdjOpp(b,pla,ploc S),ploc,mv+num,hm+num,hmval);}
  if(ploc W != eloc && ISO(ploc W) && !b.isTrapSafe2(opp,ploc W)) {num += genPPCapAndUF(b,pla,findAdjOpp(b,pla,ploc W),ploc,mv+num,hm+num,hmval);}
  if(ploc E != eloc && ISO(ploc E) && !b.isTrapSafe2(opp,ploc E)) {num += genPPCapAndUF(b,pla,findAdjOpp(b,pla,ploc E),ploc,mv+num,hm+num,hmval);}
  if(ploc N != eloc && ISO(ploc N) && !b.isTrapSafe2(opp,ploc N)) {num += genPPCapAndUF(b,pla,findAdjOpp(b,pla,ploc N),ploc,mv+num,hm+num,hmval);}

  return num;
}

//Assumes there is an adjacent opponent
static loc_t findAdjOpp(Board& b, pla_t pla, loc_t k)
{
  pla_t opp = gOpp(pla);

  if     (ISO(k S)) {return k S;}
  else if(ISO(k W)) {return k W;}
  else if(ISO(k E)) {return k E;}
  else              {return k N;}

  return ERRLOC;
}

static bool canAdvancePullCap(Board& b, pla_t pla, loc_t ploc, loc_t tr)
{
  if(Board::ADJACENTTRAP[tr] == ERRLOC)
    return false;

  pla_t opp = gOpp(pla);

  int strongerCount = 0;
  loc_t eloc = ERRLOC;
  if(ISO(tr S) && GT(tr S,ploc)) {eloc = tr S; strongerCount++;}
  if(ISO(tr W) && GT(tr W,ploc)) {eloc = tr W; strongerCount++;}
  if(ISO(tr E) && GT(tr E,ploc)) {eloc = tr E; strongerCount++;}
  if(ISO(tr N) && GT(tr N,ploc)) {eloc = tr N; strongerCount++;}

  if(strongerCount == 1 && !b.isTrapSafe2(opp,eloc))
  {
    loc_t edef = findAdjOpp(b,pla,eloc);
    if(b.isAdjacent(ploc,edef) && GT(ploc,edef)) {return true;}
  }

  return false;
}

static int genAdvancePullCap(Board& b, pla_t pla, loc_t ploc, loc_t tr,
move_t* mv, int* hm, int hmval)
{
  if(Board::ADJACENTTRAP[tr] == ERRLOC)
    return 0;

  pla_t opp = gOpp(pla);
  int num = 0;

  int strongerCount = 0;
  loc_t eloc = ERRLOC;
  if(ISO(tr S) && GT(tr S,ploc)) {eloc = tr S; strongerCount++;}
  if(ISO(tr W) && GT(tr W,ploc)) {eloc = tr W; strongerCount++;}
  if(ISO(tr E) && GT(tr E,ploc)) {eloc = tr E; strongerCount++;}
  if(ISO(tr N) && GT(tr N,ploc)) {eloc = tr N; strongerCount++;}

  if(strongerCount == 1 && !b.isTrapSafe2(opp,eloc))
  {
    loc_t edef = findAdjOpp(b,pla,eloc);
    if(b.isAdjacent(ploc,edef) && GT(ploc,edef)) {ADDMOVEPP(getMove(gStepSrcDest(ploc,tr),gStepSrcDest(edef,ploc)),hmval)}
  }

  return num;
}

