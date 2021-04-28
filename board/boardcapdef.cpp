
/*
 * boardcapdef.cpp
 * Author: davidwu
 *
 * Capture defense move generation functions
 */

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/offsets.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"
#include "../eval/threats.h"

using namespace std;

#include "../board/locations.h"

//Generate moves that step to a target location using a direct path.
//simpleWalkLocs is a buffer that tracks whether we've reached this location without pushing or pulling.
//We never revisit a spot if we've done so, which gets rid of a lot of duplicate moves.
//ASSUMES rabbitokayness.
static int genWalkTosUsingRec(Board& b, pla_t pla, loc_t ploc, loc_t targ, int numSteps, move_t* mv,
    move_t moveSoFar, int stepsSoFar, bool simpleWalk, Bitmap& simpleWalkLocs, bool simpleWalkIsGood)
{
  int num = 0;
  pla_t opp = gOpp(pla);

  int plocTargMDist = Board::manhattanDist(ploc,targ);
  if(plocTargMDist <= 0)
    return 0;

  if(simpleWalk)
    simpleWalkLocs.setOn(ploc);

  //If right next to the target and UF, directly gen the moves to step to
  if(plocTargMDist <= 1 && b.isThawedC(ploc))
  {
    step_t step = gStepSrcDest(ploc,targ);
    //Step directly?
    if(ISE(targ))
    {
      if(simpleWalk && (simpleWalkLocs.isOne(targ) || !simpleWalkIsGood))
        return 0;
      if(!b.isTrapSafe2(pla,targ))
        return 0;
      mv[num++] = concatMoves(moveSoFar,getMove(step),stepsSoFar);
      if(simpleWalk)
        simpleWalkLocs.setOn(targ);
    }
    //TODO test these conditions earlier for an early provable numSteps cutoff?
    //Player in the way - only step if all safe and we are stronger than the piece we're replacing and we're not saccing the piece.
    else if(ISP(targ) && numSteps >= 2 && GT(ploc,targ) && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,targ))
    {
      if(ISE(targ S) && b.isTrapSafe2(pla,targ S) && b.isRabOkayS(pla,targ)) mv[num++] = concatMoves(moveSoFar,getMove(targ MS,step),stepsSoFar);
      if(ISE(targ W) && b.isTrapSafe2(pla,targ W))                           mv[num++] = concatMoves(moveSoFar,getMove(targ MW,step),stepsSoFar);
      if(ISE(targ E) && b.isTrapSafe2(pla,targ E))                           mv[num++] = concatMoves(moveSoFar,getMove(targ ME,step),stepsSoFar);
      if(ISE(targ N) && b.isTrapSafe2(pla,targ N) && b.isRabOkayN(pla,targ)) mv[num++] = concatMoves(moveSoFar,getMove(targ MN,step),stepsSoFar);
    }
    //Opp in the way
    else if(ISO(targ) && numSteps >= 2 && GT(ploc,targ))
    {
      if(!b.isTrapSafe2(pla,targ))
        return 0;
      if(ISE(targ S)) mv[num++] = concatMoves(moveSoFar,getMove(targ MS,step),stepsSoFar);
      if(ISE(targ W)) mv[num++] = concatMoves(moveSoFar,getMove(targ MW,step),stepsSoFar);
      if(ISE(targ E)) mv[num++] = concatMoves(moveSoFar,getMove(targ ME,step),stepsSoFar);
      if(ISE(targ N)) mv[num++] = concatMoves(moveSoFar,getMove(targ MN,step),stepsSoFar);
    }

    //Don't try longer step sequences if we're next to the target
    return num;
  }
  //If we are frozen, try 1-step unfreezes
  else if(b.isFrozenC(ploc))
  {
    if(numSteps < plocTargMDist+1)
      return num;

    //For each direction
    for(int dir = 0; dir < 4; dir++)
    {
      if(!Board::ADJOKAY[dir][ploc])
        continue;

      //If it's empty, we can step someone in
      loc_t hloc = ploc + Board::ADJOFFSETS[dir];
      if(!ISE(hloc))
        continue;

      //Look for pieces to step in
      for(int dir2 = 0; dir2 < 4; dir2++)
      {
        if(!Board::ADJOKAY[dir2][hloc])
          continue;
        loc_t hloc2 = hloc + Board::ADJOFFSETS[dir2];
        if(hloc2 == ploc) continue;
        if(ISP(hloc2) && b.isThawedC(hloc2) && b.isRabOkay(pla,hloc2,hloc))
        {
          TSC(hloc2,hloc,num += genWalkTosUsingRec(b,pla,ploc,targ,numSteps-1,mv+num,
              concatMoves(moveSoFar,getMove(gStepSrcDest(hloc2,hloc)),stepsSoFar),stepsSoFar+1,
              false,simpleWalkLocs,simpleWalkIsGood))
        }
      }
    }

    return num;
  }

  if(numSteps < plocTargMDist)
    return num;

  //Try to step in each profitable direction.
  int dirs[2];
  int numDirs = 0;
  int dx = gX(targ)-gX(ploc);
  int dy = gY(targ)-gY(ploc);
  if(dy > 0) dirs[numDirs++] = 3;
  if(dy < 0) dirs[numDirs++] = 0;
  if(dx > 0) dirs[numDirs++] = 2;
  if(dx < 0) dirs[numDirs++] = 1;

  for(int i = 0; i < numDirs; i++)
  {
    int dir = dirs[i];
    loc_t next = ploc + Board::ADJOFFSETS[dir];
    step_t step = ploc + Board::STEPOFFSETS[dir];

    //Next loc is empty...
    if(ISE(next))
    {
      if(simpleWalk && simpleWalkLocs.isOne(next))
        continue;

      //Here we go!
      if(b.isTrapSafe2(pla,next))
      {
        TSC(ploc,next,num += genWalkTosUsingRec(b,pla,next,targ,numSteps-1,mv+num,
                          concatMoves(moveSoFar,getMove(step),stepsSoFar),stepsSoFar+1,
                          simpleWalk,simpleWalkLocs,simpleWalkIsGood))
      }
      //Trapunsafe? Defend trap!
      else
      {
        if(numSteps < plocTargMDist+1)
          continue;

        //For each direction look for defenders
        for(int dir1 = 0; dir1 < 4; dir1++)
        {
          if(!Board::ADJOKAY[dir1][next])
            continue;

          //If it's empty, we can step someone in
          loc_t hloc = next + Board::ADJOFFSETS[dir1];
          if(!ISE(hloc))
            continue;

          //Look for pieces to step in
          for(int dir2 = 0; dir2 < 4; dir2++)
          {
            if(!Board::ADJOKAY[dir2][hloc])
              continue;
            loc_t hloc2 = hloc + Board::ADJOFFSETS[dir2];
            if(hloc2 == next) continue;
            if(ISP(hloc2) && b.isThawedC(hloc2) && b.isRabOkay(pla,hloc2,hloc))
            {
              TSC(hloc2,hloc,num += genWalkTosUsingRec(b,pla,ploc,targ,numSteps-1,mv+num,
                  concatMoves(moveSoFar,getMove(gStepSrcDest(hloc2,hloc)),stepsSoFar),stepsSoFar+1,
                  false,simpleWalkLocs,simpleWalkIsGood))
            }
          }
        }
      }
    }
    //Player in the way
    else if(ISP(next) && numSteps >= plocTargMDist+1 && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,next))
    {
      if(ISE(next S) && b.isRabOkayS(pla,next)) {TPC(ploc,next,next S, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MS,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next W))                           {TPC(ploc,next,next W, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MW,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next E))                           {TPC(ploc,next,next E, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next ME,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next N) && b.isRabOkayN(pla,next)) {TPC(ploc,next,next N, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MN,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
    }
    //Opp in the way
    else if(ISO(next) && numSteps >= plocTargMDist+1 && GT(ploc,next) && b.isTrapSafe2(pla,next))
    {
      if(ISE(next S)) {TPC(ploc,next,next S, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MS,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next W)) {TPC(ploc,next,next W, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MW,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next E)) {TPC(ploc,next,next E, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next ME,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next N)) {TPC(ploc,next,next N, num += genWalkTosUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MN,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
    }
  }
  return num;
}

//Generate moves that defend a target location
//simpleWalkLocs is a buffer that tracks whether we've reached this location without pushing or pulling.
//We never revisit a spot if we've done so, which gets rid of a lot of duplicate moves.
static int genDefsUsingRec(Board& b, pla_t pla, loc_t ploc, loc_t targ, int numSteps, move_t* mv,
    move_t moveSoFar, int stepsSoFar, bool simpleWalk, Bitmap& simpleWalkLocs, bool simpleWalkIsGood)
{
  int num = 0;
  pla_t opp = gOpp(pla);

  int plocTargMDist = Board::manhattanDist(ploc,targ);
  if(plocTargMDist == 1)
    return 0;

  if(simpleWalk)
    simpleWalkLocs.setOn(ploc);

  //If right next to the target and UF, directly gen the moves to defend
  if(plocTargMDist <= 2 && b.isThawedC(ploc))
  {
    //Try each direction
    for(int dir = 0; dir < 4; dir++)
    {
      if(!Board::ADJOKAY[dir][ploc])
        continue;

      loc_t dest = ploc + Board::ADJOFFSETS[dir];
      if(Board::manhattanDist(targ,dest) != 1 || !b.isRabOkay(pla,ploc,dest))
        continue;

      step_t step = ploc + Board::STEPOFFSETS[dir];

      //Step directly?
      if(ISE(dest))
      {
        if(simpleWalk && (simpleWalkLocs.isOne(dest) || !simpleWalkIsGood))
          continue;
        if(!b.isTrapSafe2(pla,dest))
          continue;
        mv[num++] = concatMoves(moveSoFar,getMove(step),stepsSoFar);
        if(simpleWalk)
          simpleWalkLocs.setOn(dest);
      }
      //Player in the way - only defend if all safe and we are stronger than the piece we're replacing and we're not saccing the piece.
      else if(ISP(dest) && numSteps >= 2 && GT(ploc,dest) && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,dest))
      {
        if(ISE(dest S) && b.isTrapSafe2(pla,dest S) && b.isRabOkayS(pla,dest)) mv[num++] = concatMoves(moveSoFar,getMove(dest MS,step),stepsSoFar);
        if(ISE(dest W) && b.isTrapSafe2(pla,dest W))                           mv[num++] = concatMoves(moveSoFar,getMove(dest MW,step),stepsSoFar);
        if(ISE(dest E) && b.isTrapSafe2(pla,dest E))                           mv[num++] = concatMoves(moveSoFar,getMove(dest ME,step),stepsSoFar);
        if(ISE(dest N) && b.isTrapSafe2(pla,dest N) && b.isRabOkayN(pla,dest)) mv[num++] = concatMoves(moveSoFar,getMove(dest MN,step),stepsSoFar);
      }
      //Opp in the way
      else if(ISO(dest) && numSteps >= 2 && GT(ploc,dest))
      {
        if(!b.isTrapSafe2(pla,dest))
          continue;
        if(ISE(dest S)) mv[num++] = concatMoves(moveSoFar,getMove(dest MS,step),stepsSoFar);
        if(ISE(dest W)) mv[num++] = concatMoves(moveSoFar,getMove(dest MW,step),stepsSoFar);
        if(ISE(dest E)) mv[num++] = concatMoves(moveSoFar,getMove(dest ME,step),stepsSoFar);
        if(ISE(dest N)) mv[num++] = concatMoves(moveSoFar,getMove(dest MN,step),stepsSoFar);
      }
    }

    //These are all of the ways to consider defending if we only have 2 steps.
    if(numSteps <= 2)
      return num;
  }
  //If we are frozen, try 1-step unfreezes
  else if(b.isFrozenC(ploc))
  {
    if(plocTargMDist == 0 || numSteps < plocTargMDist)
      return num;

    //For each direction
    for(int dir = 0; dir < 4; dir++)
    {
      if(!Board::ADJOKAY[dir][ploc])
        continue;

      //If it's empty, we can step someone in
      loc_t hloc = ploc + Board::ADJOFFSETS[dir];
      if(!ISE(hloc))
        continue;

      //Look for pieces to step in
      for(int dir2 = 0; dir2 < 4; dir2++)
      {
        if(!Board::ADJOKAY[dir2][hloc])
          continue;
        loc_t hloc2 = hloc + Board::ADJOFFSETS[dir2];
        if(hloc2 == ploc) continue;
        if(ISP(hloc2) && b.isThawedC(hloc2) && b.isRabOkay(pla,hloc2,hloc))
        {
          TSC(hloc2,hloc,num += genDefsUsingRec(b,pla,ploc,targ,numSteps-1,mv+num,
              concatMoves(moveSoFar,getMove(gStepSrcDest(hloc2,hloc)),stepsSoFar),stepsSoFar+1,
              false,simpleWalkLocs,simpleWalkIsGood))
        }
      }
    }

    return num;
  }

  if(numSteps < plocTargMDist-1 || plocTargMDist == 0)
    return num;

  //Try to step in each profitable direction.
  int bestDir = 0;
  int worstDir1 = -1;
  int worstDir2 = -1;
  if(targ != ploc)
  {bestDir = Board::getApproachDir(ploc,targ); Board::getRetreatDirs(ploc,targ,worstDir1,worstDir2);}

  int dirs[4] = {0,1,2,3};
  dirs[bestDir] = 0;
  dirs[0] = bestDir;
  for(int i = 0; i < 4; i++)
  {
    int dir = dirs[i];
    if(!Board::ADJOKAY[dir][ploc] || dir == worstDir1 || dir == worstDir2)
      continue;
    loc_t next = ploc + Board::ADJOFFSETS[dir];
    if(Board::manhattanDist(targ,next) == 1 || !b.isRabOkay(pla,ploc,next))
      continue;
    step_t step = ploc + Board::STEPOFFSETS[dir];

    //Next loc is empty...
    if(ISE(next))
    {
      if(simpleWalk && simpleWalkLocs.isOne(next))
        continue;

      //Here we go!
      if(b.isTrapSafe2(pla,next))
      {
        TSC(ploc,next,num += genDefsUsingRec(b,pla,next,targ,numSteps-1,mv+num,
                          concatMoves(moveSoFar,getMove(step),stepsSoFar),stepsSoFar+1,
                          simpleWalk,simpleWalkLocs,simpleWalkIsGood))
      }
      //Trapunsafe? Defend trap!
      else
      {
        if(numSteps < plocTargMDist)
          continue;

        //For each direction look for defenders
        for(int dir1 = 0; dir1 < 4; dir1++)
        {
          if(!Board::ADJOKAY[dir1][next])
            continue;

          //If it's empty, we can step someone in
          loc_t hloc = next + Board::ADJOFFSETS[dir1];
          if(!ISE(hloc))
            continue;

          //Look for pieces to step in
          for(int dir2 = 0; dir2 < 4; dir2++)
          {
            if(!Board::ADJOKAY[dir2][hloc])
              continue;
            loc_t hloc2 = hloc + Board::ADJOFFSETS[dir2];
            if(hloc2 == next) continue;
            if(ISP(hloc2) && b.isThawedC(hloc2) && b.isRabOkay(pla,hloc2,hloc))
            {
              TSC(hloc2,hloc,num += genDefsUsingRec(b,pla,ploc,targ,numSteps-1,mv+num,
                  concatMoves(moveSoFar,getMove(gStepSrcDest(hloc2,hloc)),stepsSoFar),stepsSoFar+1,
                  false,simpleWalkLocs,simpleWalkIsGood))
            }
          }
        }
      }
    }
    //Player in the way
    else if(ISP(next) && numSteps >= plocTargMDist && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,next))
    {
      if(ISE(next S) && b.isRabOkayS(pla,next)) {TPC(ploc,next,next S, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MS,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next W))                           {TPC(ploc,next,next W, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MW,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next E))                           {TPC(ploc,next,next E, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next ME,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next N) && b.isRabOkayN(pla,next)) {TPC(ploc,next,next N, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MN,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
    }
    //Opp in the way
    else if(ISO(next) && numSteps >= plocTargMDist && GT(ploc,next) && b.isTrapSafe2(pla,next))
    {
      if(ISE(next S)) {TPC(ploc,next,next S, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MS,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next W)) {TPC(ploc,next,next W, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MW,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next E)) {TPC(ploc,next,next E, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next ME,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
      if(ISE(next N)) {TPC(ploc,next,next N, num += genDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MN,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood));}
    }
  }
  return num;
}

//Same as genDefsUsingRec, but iterates with the given function f for each defense.
template <typename Func>
#define IDUCALLF(move,ns) num += f(b,concatMoves(moveSoFar,move,stepsSoFar),stepsSoFar+ns,mv+num)
static int iterDefsUsingRec(Board& b, pla_t pla, loc_t ploc, loc_t targ, int numSteps, move_t* mv,
    move_t moveSoFar, int stepsSoFar, bool simpleWalk, Bitmap& simpleWalkLocs, bool simpleWalkIsGood, Func f)
{
  int num = 0;
  pla_t opp = gOpp(pla);

  int plocTargMDist = Board::manhattanDist(ploc,targ);
  if(plocTargMDist == 1)
    return 0;

  if(simpleWalk)
    simpleWalkLocs.setOn(ploc);

  //If right next to the target and UF, directly gen the moves to defend
  if(plocTargMDist <= 2 && b.isThawedC(ploc))
  {
    //Try each direction
    for(int dir = 0; dir < 4; dir++)
    {
      if(!Board::ADJOKAY[dir][ploc])
        continue;

      loc_t dest = ploc + Board::ADJOFFSETS[dir];
      if(Board::manhattanDist(targ,dest) != 1 || !b.isRabOkay(pla,ploc,dest))
        continue;

      step_t step = ploc + Board::STEPOFFSETS[dir];

      //Step directly?
      if(ISE(dest))
      {
        if(simpleWalk && (simpleWalkLocs.isOne(dest) || !simpleWalkIsGood))
          continue;
        if(!b.isTrapSafe2(pla,dest))
          continue;
        TSC(ploc,dest,IDUCALLF(getMove(step),1));
        if(simpleWalk)
          simpleWalkLocs.setOn(dest);
      }
      //Player in the way - only defend if all safe and we are stronger than the piece we're replacing and we're not saccing the piece.
      else if(ISP(dest) && numSteps >= 2 && GT(ploc,dest) && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,dest))
      {
        if(ISE(dest S) && b.isTrapSafe2(pla,dest S) && b.isRabOkayS(pla,dest)) {TPC(ploc,dest,dest S,IDUCALLF(getMove(dest MS,step),2));}
        if(ISE(dest W) && b.isTrapSafe2(pla,dest W))                           {TPC(ploc,dest,dest W,IDUCALLF(getMove(dest MW,step),2));}
        if(ISE(dest E) && b.isTrapSafe2(pla,dest E))                           {TPC(ploc,dest,dest E,IDUCALLF(getMove(dest ME,step),2));}
        if(ISE(dest N) && b.isTrapSafe2(pla,dest N) && b.isRabOkayN(pla,dest)) {TPC(ploc,dest,dest N,IDUCALLF(getMove(dest MN,step),2));}
      }
      //Opp in the way
      else if(ISO(dest) && numSteps >= 2 && GT(ploc,dest))
      {
        if(!b.isTrapSafe2(pla,dest))
          continue;
        if(ISE(dest S)) {TPC(ploc,dest,dest S,IDUCALLF(getMove(dest MS,step),2));}
        if(ISE(dest W)) {TPC(ploc,dest,dest W,IDUCALLF(getMove(dest MW,step),2));}
        if(ISE(dest E)) {TPC(ploc,dest,dest E,IDUCALLF(getMove(dest ME,step),2));}
        if(ISE(dest N)) {TPC(ploc,dest,dest N,IDUCALLF(getMove(dest MN,step),2));}
      }
    }

    //These are all of the ways to consider defending if we only have 2 steps.
    if(numSteps <= 2)
      return num;
  }
  //If we are frozen, try 1-step unfreezes
  else if(b.isFrozenC(ploc))
  {
    if(plocTargMDist == 0 || numSteps < plocTargMDist)
      return num;

    //For each direction
    for(int dir = 0; dir < 4; dir++)
    {
      if(!Board::ADJOKAY[dir][ploc])
        continue;

      //If it's empty, we can step someone in
      loc_t hloc = ploc + Board::ADJOFFSETS[dir];
      if(!ISE(hloc))
        continue;

      //Look for pieces to step in
      for(int dir2 = 0; dir2 < 4; dir2++)
      {
        if(!Board::ADJOKAY[dir2][hloc])
          continue;
        loc_t hloc2 = hloc + Board::ADJOFFSETS[dir2];
        if(hloc2 == ploc) continue;
        if(ISP(hloc2) && b.isThawedC(hloc2) && b.isRabOkay(pla,hloc2,hloc))
        {
          TSC(hloc2,hloc,num += iterDefsUsingRec(b,pla,ploc,targ,numSteps-1,mv+num,
              concatMoves(moveSoFar,getMove(gStepSrcDest(hloc2,hloc)),stepsSoFar),stepsSoFar+1,
              false,simpleWalkLocs,simpleWalkIsGood,f))
        }
      }
    }

    return num;
  }

  if(numSteps < plocTargMDist-1 || plocTargMDist == 0)
    return num;

  //Try to step in each profitable direction.
  int bestDir = 0;
  int worstDir1 = -1;
  int worstDir2 = -1;
  if(targ != ploc)
  {bestDir = Board::getApproachDir(ploc,targ); Board::getRetreatDirs(ploc,targ,worstDir1,worstDir2);}

  int dirs[4] = {0,1,2,3};
  dirs[bestDir] = 0;
  dirs[0] = bestDir;
  for(int i = 0; i < 4; i++)
  {
    int dir = dirs[i];
    if(!Board::ADJOKAY[dir][ploc] || dir == worstDir1 || dir == worstDir2)
      continue;
    loc_t next = ploc + Board::ADJOFFSETS[dir];
    if(Board::manhattanDist(targ,next) == 1 || !b.isRabOkay(pla,ploc,next))
      continue;
    step_t step = ploc + Board::STEPOFFSETS[dir];

    //Next loc is empty...
    if(ISE(next))
    {
      if(simpleWalk && simpleWalkLocs.isOne(next))
        continue;

      //Here we go!
      if(b.isTrapSafe2(pla,next))
      {
        TSC(ploc,next,num += iterDefsUsingRec(b,pla,next,targ,numSteps-1,mv+num,
                          concatMoves(moveSoFar,getMove(step),stepsSoFar),stepsSoFar+1,
                          simpleWalk,simpleWalkLocs,simpleWalkIsGood,f))
      }
      //Trapunsafe? Defend trap!
      else
      {
        if(numSteps < plocTargMDist)
          continue;

        //For each direction look for defenders
        for(int dir1 = 0; dir1 < 4; dir1++)
        {
          if(!Board::ADJOKAY[dir1][next])
            continue;

          //If it's empty, we can step someone in
          loc_t hloc = next + Board::ADJOFFSETS[dir1];
          if(!ISE(hloc))
            continue;

          //Look for pieces to step in
          for(int dir2 = 0; dir2 < 4; dir2++)
          {
            if(!Board::ADJOKAY[dir2][hloc])
              continue;
            loc_t hloc2 = hloc + Board::ADJOFFSETS[dir2];
            if(hloc2 == next) continue;
            if(ISP(hloc2) && b.isThawedC(hloc2) && b.isRabOkay(pla,hloc2,hloc))
            {
              TSC(hloc2,hloc,num += iterDefsUsingRec(b,pla,ploc,targ,numSteps-1,mv+num,
                  concatMoves(moveSoFar,getMove(gStepSrcDest(hloc2,hloc)),stepsSoFar),stepsSoFar+1,
                  false,simpleWalkLocs,simpleWalkIsGood,f))
            }
          }
        }
      }
    }
    //Player in the way
    else if(ISP(next) && numSteps >= plocTargMDist && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,next))
    {
      if(ISE(next S) && b.isRabOkayS(pla,next)) {TPC(ploc,next,next S, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MS,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
      if(ISE(next W))                           {TPC(ploc,next,next W, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MW,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
      if(ISE(next E))                           {TPC(ploc,next,next E, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next ME,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
      if(ISE(next N) && b.isRabOkayN(pla,next)) {TPC(ploc,next,next N, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MN,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
    }
    //Opp in the way
    else if(ISO(next) && numSteps >= plocTargMDist && GT(ploc,next) && b.isTrapSafe2(pla,next))
    {
      if(ISE(next S)) {TPC(ploc,next,next S, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MS,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
      if(ISE(next W)) {TPC(ploc,next,next W, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MW,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
      if(ISE(next E)) {TPC(ploc,next,next E, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next ME,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
      if(ISE(next N)) {TPC(ploc,next,next N, num += iterDefsUsingRec(b,pla,next,targ,numSteps-2,mv+num, concatMoves(moveSoFar,getMove(next MN,step),stepsSoFar),stepsSoFar+2,false,simpleWalkLocs,simpleWalkIsGood,f));}
    }
  }
  return num;
}

static int genWalkTosUsing(Board& b, pla_t pla, loc_t ploc, loc_t targ, int numSteps, move_t* mv, Bitmap& simpleWalkLocs)
{
  return genWalkTosUsingRec(b,pla,ploc,targ,numSteps,mv,ERRMOVE,0,true,simpleWalkLocs,true);
}

static int genDefsUsing(Board& b, pla_t pla, loc_t ploc, loc_t targ, int numSteps, move_t* mv, Bitmap& simpleWalkLocs)
{
  return genDefsUsingRec(b,pla,ploc,targ,numSteps,mv,ERRMOVE,0,true,simpleWalkLocs,true);
}

template <typename Func>
static int iterDefsUsing(Board& b, pla_t pla, loc_t ploc, loc_t targ, int numSteps, move_t* mv, Bitmap& simpleWalkLocs, Func f)
{
  return iterDefsUsingRec(b,pla,ploc,targ,numSteps,mv,ERRMOVE,0,true,simpleWalkLocs,true,f);
}


//Step each possible direction that isn't suicide, push through if blocked
//NOTE: THIS FUNCTION MUST WORK INSIDE A TEMPSTEP!
static int stepOrPushAwayNoSuicide(Board& b, pla_t pla, loc_t ploc, int numSteps, move_t* mv)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  //Step each possible direction that isn't suicide, push through if blocked
  if(CS1(ploc) && b.isRabOkayS(pla,ploc))
  {
    if(ISE(ploc S) && b.isTrapSafe2(pla,ploc S)) {mv[num++] = getMove(ploc MS);}
    else if(ISP(ploc S) && numSteps >= 2 && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,ploc S))
    {
      loc_t loc = ploc S;
      if(ISE(loc S) && b.isRabOkayS(pla,loc)) {mv[num++] = getMove(loc MS, ploc MS);}
      if(ISE(loc W))                          {mv[num++] = getMove(loc MW, ploc MS);}
      if(ISE(loc E))                          {mv[num++] = getMove(loc ME, ploc MS);}
      if(ISE(loc N) && b.isRabOkayN(pla,loc)) {mv[num++] = getMove(loc MN, ploc MS);}
    }
    else if(ISO(ploc S) && numSteps >= 2 && GT(ploc,ploc S) && b.isTrapSafe2(pla,ploc S))
    {
      loc_t loc = ploc S;
      if(ISE(loc S)) {mv[num++] = getMove(loc MS, ploc MS);}
      if(ISE(loc W)) {mv[num++] = getMove(loc MW, ploc MS);}
      if(ISE(loc E)) {mv[num++] = getMove(loc ME, ploc MS);}
      if(ISE(loc N)) {mv[num++] = getMove(loc MN, ploc MS);}
    }
  }
  if(CW1(ploc))
  {
    if(ISE(ploc W) && b.isTrapSafe2(pla,ploc W)) {mv[num++] = getMove(ploc MW);}
    else if(ISP(ploc W) && numSteps >= 2 && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,ploc W))
    {
      loc_t loc = ploc W;
      if(ISE(loc S) && b.isRabOkayS(pla,loc)) {mv[num++] = getMove(loc MS, ploc MW);}
      if(ISE(loc W))                          {mv[num++] = getMove(loc MW, ploc MW);}
      if(ISE(loc E))                          {mv[num++] = getMove(loc ME, ploc MW);}
      if(ISE(loc N) && b.isRabOkayN(pla,loc)) {mv[num++] = getMove(loc MN, ploc MW);}
    }
    else if(ISO(ploc W) && numSteps >= 2 && GT(ploc,ploc W) && b.isTrapSafe2(pla,ploc W))
    {
      loc_t loc = ploc W;
      if(ISE(loc S)) {mv[num++] = getMove(loc MS, ploc MW);}
      if(ISE(loc W)) {mv[num++] = getMove(loc MW, ploc MW);}
      if(ISE(loc E)) {mv[num++] = getMove(loc ME, ploc MW);}
      if(ISE(loc N)) {mv[num++] = getMove(loc MN, ploc MW);}
    }
  }
  if(CE1(ploc))
  {
    if(ISE(ploc E) && b.isTrapSafe2(pla,ploc E)) {mv[num++] = getMove(ploc ME);}
    else if(ISP(ploc E) && numSteps >= 2 && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,ploc E))
    {
      loc_t loc = ploc E;
      if(ISE(loc S) && b.isRabOkayS(pla,loc)) {mv[num++] = getMove(loc MS, ploc ME);}
      if(ISE(loc W))                          {mv[num++] = getMove(loc MW, ploc ME);}
      if(ISE(loc E))                          {mv[num++] = getMove(loc ME, ploc ME);}
      if(ISE(loc N) && b.isRabOkayN(pla,loc)) {mv[num++] = getMove(loc MN, ploc ME);}
    }
    else if(ISO(ploc E) && numSteps >= 2 && GT(ploc,ploc E) && b.isTrapSafe2(pla,ploc E))
    {
      loc_t loc = ploc E;
      if(ISE(loc S)) {mv[num++] = getMove(loc MS, ploc ME);}
      if(ISE(loc W)) {mv[num++] = getMove(loc MW, ploc ME);}
      if(ISE(loc E)) {mv[num++] = getMove(loc ME, ploc ME);}
      if(ISE(loc N)) {mv[num++] = getMove(loc MN, ploc ME);}
    }
  }
  if(CN1(ploc) && b.isRabOkayN(pla,ploc))
  {
    if(ISE(ploc N) && b.isTrapSafe2(pla,ploc N)) {mv[num++] = getMove(ploc MN);}
    else if(ISP(ploc N) && numSteps >= 2 && b.isTrapSafe2(pla,ploc) && b.wouldBeUF(pla,ploc,ploc,ploc N))
    {
      loc_t loc = ploc N;
      if(ISE(loc S) && b.isRabOkayS(pla,loc)) {mv[num++] = getMove(loc MS, ploc MN);}
      if(ISE(loc W))                          {mv[num++] = getMove(loc MW, ploc MN);}
      if(ISE(loc E))                          {mv[num++] = getMove(loc ME, ploc MN);}
      if(ISE(loc N) && b.isRabOkayN(pla,loc)) {mv[num++] = getMove(loc MN, ploc MN);}
    }
    else if(ISO(ploc N) && numSteps >= 2 && GT(ploc,ploc N) && b.isTrapSafe2(pla,ploc N))
    {
      loc_t loc = ploc N;
      if(ISE(loc S)) {mv[num++] = getMove(loc MS, ploc MN);}
      if(ISE(loc W)) {mv[num++] = getMove(loc MW, ploc MN);}
      if(ISE(loc E)) {mv[num++] = getMove(loc ME, ploc MN);}
      if(ISE(loc N)) {mv[num++] = getMove(loc MN, ploc MN);}
    }
  }
  return num;
}

//Generates some simple runaways by the given piece in the number of steps
//chainUFRunaways - if the piece is frozen, then rather than generating unfreeze steps alone and leaving
//the rest to a later search depth, generate runaway steps chained on to the unfreeze steps.
int BoardTrees::genRunawayDefs(Board& b, pla_t pla, loc_t ploc, int numSteps, bool chainUFRunaways, move_t* mv)
{
  int num = 0;
  //Unfrozen - directly push away
  if(b.isThawed(ploc))
    num += stepOrPushAwayNoSuicide(b,pla,ploc,numSteps,mv+num);
  //Frozen - defend first
  else if(numSteps > 1)
  {
    Bitmap map = b.pieceMaps[pla][0] & (Board::DISK[numSteps-1][ploc] | (Board::DISK[numSteps][ploc] & ~b.frozenMap));
    map.setOff(ploc);
    while(map.hasBits())
    {
      loc_t hloc = map.nextBit();
      Bitmap simpleWalkLocs;
      if(!chainUFRunaways)
        num += genDefsUsing(b,pla,hloc,ploc,numSteps-1,mv+num,simpleWalkLocs);
      else
      {
        num += iterDefsUsing(b,pla,hloc,ploc,numSteps-1,mv+num,simpleWalkLocs,
          [=](Board& b,move_t moveSoFar, int stepsSoFar, move_t* mv) {
          int num = stepOrPushAwayNoSuicide(b,b.player,ploc,numSteps-stepsSoFar,mv);
          for(int i = 0; i<num; i++)
            mv[i] = concatMoves(moveSoFar,mv[i],stepsSoFar);
          return num;
        });
      }
    }
  }

  return num;
}

int BoardTrees::shortestGoodTrapDef(Board& b, pla_t pla, loc_t kt, move_t* mv, int num, const Bitmap& currentWholeCapMap)
{
  pla_t opp = gOpp(pla);
  int shortestLen = 5;
  UndoMoveData uData;
  for(int i = 0; i<num; i++)
  {
    move_t move = mv[i];

    //Make sure the move is shorter than the best
    int ns = numStepsInMove(move);
    if(ns >= shortestLen)
      continue;

    //Try the move
    int oldPieceCount = b.pieceCounts[pla][0];
    b.makeMove(move,uData);

    //Make we didn't commit suicide
    if(b.pieceCounts[pla][0] < oldPieceCount)
    {b.undoMove(uData); continue;}

    //Make sure nothing here is captureable now and no goal is possible at all
    if(BoardTrees::canCaps(b,opp,4,2,kt) || BoardTrees::goalDist(b,opp,4) < 5)
    {b.undoMove(uData); continue;}

    //Make sure nothing *new* is captureable elsewhere
    bool newCapture = false;
    for(int j = 0; j<4; j++)
    {
      loc_t otherkt = Board::TRAPLOCS[j];
      if(otherkt == kt)
        continue;
      Bitmap capMap;
      int capDist;
      if(BoardTrees::canCaps(b,opp,4,kt,capMap,capDist))
      {
        if((capMap & ~currentWholeCapMap).hasBits())
        {newCapture = true; break;}
      }
    }
    if(!newCapture)
      shortestLen = ns;

    b.undoMove(uData);
  }
  return shortestLen;
}

//NOTE: Should NOT be called with numSteps <= 0, will do an out-of-bounds array access if so
int BoardTrees::genTrapDefs(Board& b, pla_t pla, loc_t kt, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  int numGuards = b.trapGuardCounts[pla][Board::TRAPINDEX[kt]];
  if(numGuards == 0)
  {
    //Find how strong opp is near this trap
    int oppStrRad2 = ELE;
    while(oppStrRad2 > RAB && (b.pieceMaps[opp][oppStrRad2] & Board::DISK[3][kt]).isEmpty())
      oppStrRad2--;

    //Find defenders
    Bitmap strongMap = pStrongerMaps[opp][oppStrRad2-1] & (Board::DISK[numSteps][kt] | (Board::DISK[numSteps+1][kt] & ~b.frozenMap));
    Bitmap weakMap = b.pieceMaps[pla][0] & (Board::DISK[numSteps-1][kt] | (Board::DISK[numSteps][kt] & ~b.frozenMap));
    weakMap &= ~strongMap;

    //Defense by strong pieces
    while(strongMap.hasBits())
    {
      loc_t ploc = strongMap.nextBit();
      Bitmap simpleWalkLocs;
      num += genDefsUsing(b,pla,ploc,kt,numSteps,mv+num,simpleWalkLocs);
    }
    //Defense by weak pieces
    while(weakMap.hasBits())
    {
      loc_t ploc = weakMap.nextBit();
      Bitmap simpleWalkLocs;
      move_t tempmv[32];
      int tempNum = genDefsUsing(b,pla,ploc,kt,numSteps-1,tempmv,simpleWalkLocs);

      //Try each defense and see if it works by itself
      UndoMoveData uData;
      for(int i = 0; i<tempNum; i++)
      {
        b.makeMove(tempmv[i],uData);
        if(!BoardTrees::canCaps(b,opp,4,2,kt))
          mv[num++] = tempmv[i];
        //It failed. So let's see if we can add more defense to this trap
        else
        {
          int nsInMove =  numStepsInMove(tempmv[i]);
          int numStepsLeft = numSteps - nsInMove;
          if(numStepsLeft <= 0)
          {b.undoMove(uData); continue;}
          int tempTempNum = genTrapDefs(b,pla,kt,numStepsLeft,pStrongerMaps,mv+num);
          for(int j = 0; j<tempTempNum; j++)
          {
            mv[num+j] = concatMoves(tempmv[i],mv[num+j],nsInMove);
          }
          num += tempTempNum;
        }
        b.undoMove(uData);
      }
    }

  }
  //NOTE THAT THE BELOW CANNOT USE pStrongerMaps without invalidating the recursion a few lines above.
  else if(numGuards >= 1)
  {
    Bitmap map = b.pieceMaps[pla][0] & ~Board::RADIUS[1][kt] & (Board::DISK[numSteps][kt] | (Board::DISK[numSteps+1][kt] & ~b.frozenMap)) ;
    while(map.hasBits())
    {
      loc_t ploc = map.nextBit();
      Bitmap simpleWalkLocs;
      num += genDefsUsing(b,pla,ploc,kt,numSteps,mv+num,simpleWalkLocs);
    }
  }
  return num;
}





static const int DYDIR[2] =
{MS,MN};

//deltax is gX(rloc) - gX(loc)
//deltay is the y distance from loc, assumed always positive
//dy is N or S indicating which direction pla's rabbits want to advance
//Returns a move if successful, else ERRMOVE
//Assumes rloc is not loc
//Assumes rloc is not frozen
//Assumes loc is empty
static move_t genRabbitWalkTo(Board& b, pla_t pla, loc_t rloc, loc_t loc, int deltax, int deltay, int dy)
{
  if(deltax == 0)
  {
    switch(deltay)
    {
    case 1:
      return gMove(rloc MG);
    case 2:
      if(ISE(rloc G) && b.wouldBeUF(pla,rloc,rloc G,rloc) && b.isTrapSafe2(pla,rloc G))
        return gMove(rloc MG, rloc G MG);
      break;
    case 3:
      if(ISE(rloc G) && ISE(rloc GG) &&
         b.wouldBeUF(pla,rloc,rloc GG) && b.isTrapSafe1(pla,rloc GG) &&
         b.wouldBeUF(pla,rloc,rloc G,rloc) && b.isTrapSafe2(pla,rloc G))
        return gMove(rloc MG, rloc G MG, rloc GG MG);
      break;
    default:
      break;
    }
    return ERRMOVE;
  }

  if(deltay <= 0)
  {
    if(deltax > 0)
    {
      switch(deltax)
      {
      case 1:
        return gMove(rloc MW);
      case 2:
        if(ISE(rloc W) && b.wouldBeUF(pla,rloc,rloc W,rloc) && b.isTrapSafe2(pla,rloc W))
          return gMove(rloc MW, rloc W MW);
        break;
      case 3:
        if(ISE(rloc W) && ISE(rloc WW) &&
           b.wouldBeUF(pla,rloc,rloc WW) && b.isTrapSafe1(pla,rloc WW) &&
           b.wouldBeUF(pla,rloc,rloc W,rloc) && b.isTrapSafe2(pla,rloc W))
          return gMove(rloc MW, rloc W MW, rloc WW MW);
        break;
      default:
        break;
      }
    }
    else
    {
      switch(-deltax)
      {
      case 1:
        return gMove(rloc ME);
      case 2:
        if(ISE(rloc E) && b.wouldBeUF(pla,rloc,rloc E,rloc) && b.isTrapSafe2(pla,rloc E))
          return gMove(rloc ME, rloc E ME);
        break;
      case 3:
        if(ISE(rloc E) && ISE(rloc EE) &&
           b.wouldBeUF(pla,rloc,rloc EE) && b.isTrapSafe1(pla,rloc EE) &&
           b.wouldBeUF(pla,rloc,rloc E,rloc) && b.isTrapSafe2(pla,rloc E))
          return gMove(rloc ME, rloc E ME, rloc EE ME);
        break;
      default:
        break;
      }
    }
  }

  if(deltax > 0 && ISE(rloc W) && b.isTrapSafe2(pla,rloc W) && b.wouldBeUF(pla,rloc,rloc W,rloc))
  {
    move_t move;
    if(deltax+deltay <= 2)
    {TS(rloc,rloc W,move = genRabbitWalkTo(b,pla,rloc W,loc,deltax-1,deltay,dy))}
    else
    {TSC(rloc,rloc W,move = genRabbitWalkTo(b,pla,rloc W,loc,deltax-1,deltay,dy))}
    if(move != ERRMOVE)
      return preConcatMove(rloc MW,move);
  }
  if(deltax < 0 && ISE(rloc E) && b.isTrapSafe2(pla,rloc E) && b.wouldBeUF(pla,rloc,rloc E,rloc))
  {
    move_t move;
    if(-deltax+deltay <= 2)
    {TS(rloc,rloc E,move = genRabbitWalkTo(b,pla,rloc E,loc,deltax+1,deltay,dy))}
    else
    {TSC(rloc,rloc E,move = genRabbitWalkTo(b,pla,rloc E,loc,deltax+1,deltay,dy))}
    if(move != ERRMOVE)
      return preConcatMove(rloc ME,move);
  }
  if(deltay > 0 && ISE(rloc G) && b.isTrapSafe2(pla,rloc G) && b.wouldBeUF(pla,rloc,rloc G,rloc))
  {
    move_t move;
    if((deltax > 0 ? deltax : -deltax)+deltay <= 2)
    {TS(rloc,rloc G,move = genRabbitWalkTo(b,pla,rloc G,loc,deltax,deltay-1,dy))}
    else
    {TSC(rloc,rloc G,move = genRabbitWalkTo(b,pla,rloc G,loc,deltax,deltay-1,dy))}
    if(move != ERRMOVE)
      return preConcatMove(rloc MG,move);
  }
  return ERRMOVE;
}

static int genRabbitWalksToBy(Board& b, pla_t pla, loc_t loc, Bitmap rlocs, move_t* mv)
{
  int num = 0;
  int dy = Board::GOALLOCINC[pla];
  while(rlocs.hasBits())
  {
    loc_t rloc = rlocs.nextBit();
    int deltax = gX(rloc) - gX(loc);
    int deltay = gY(rloc) - gY(loc);
    if(pla == GOLD)
      deltay = -deltay;
    if(deltay < 0)
      continue;
    move_t move = genRabbitWalkTo(b, pla, rloc, loc, deltax, deltay, dy);
    if(move != ERRMOVE)
      mv[num++] = move;
  }
  return num;
}

static int genRabbitUFAndStepTo(Board& b, pla_t pla, loc_t loc, Bitmap rlocs, move_t* mv)
{
  int num = 0;
  while(rlocs.hasBits())
  {
    loc_t rloc = rlocs.nextBit();
    //Check that there is the possiblity to unfreeze the rabbit
    if((Board::RADIUS[2][rloc] & b.pieceMaps[pla][0] & ~b.frozenMap).isEmpty())
      continue;

    step_t rstep = gStepSrcDest(rloc,loc);
    if(loc != rloc S && ISE(rloc S))
    {
      if(ISP(rloc SS) && b.isThawed(rloc SS) && b.isRabOkayN(pla,rloc SS)) {mv[num++] = getMove(rloc SS MN,rstep);}
      if(ISP(rloc SW) && b.isThawed(rloc SW)                             ) {mv[num++] = getMove(rloc SW ME,rstep);}
      if(ISP(rloc SE) && b.isThawed(rloc SE)                             ) {mv[num++] = getMove(rloc SE MW,rstep);}
    }
    if(loc != rloc W && ISE(rloc W))
    {
      if(ISP(rloc SW) && b.isThawed(rloc SW) && b.isRabOkayN(pla,rloc SW)) {mv[num++] = getMove(rloc SW MN,rstep);}
      if(ISP(rloc WW) && b.isThawed(rloc WW)                             ) {mv[num++] = getMove(rloc WW ME,rstep);}
      if(ISP(rloc NW) && b.isThawed(rloc NW) && b.isRabOkayS(pla,rloc NW)) {mv[num++] = getMove(rloc NW MS,rstep);}
    }
    if(loc != rloc E && ISE(rloc E))
    {
      if(ISP(rloc SE) && b.isThawed(rloc SE) && b.isRabOkayN(pla,rloc SE)) {mv[num++] = getMove(rloc SE MN,rstep);}
      if(ISP(rloc EE) && b.isThawed(rloc EE)                             ) {mv[num++] = getMove(rloc EE MW,rstep);}
      if(ISP(rloc NE) && b.isThawed(rloc NE) && b.isRabOkayS(pla,rloc NE)) {mv[num++] = getMove(rloc NE MS,rstep);}
    }
    if(loc != rloc N && ISE(rloc N))
    {
      if(ISP(rloc NW) && b.isThawed(rloc NW)                             ) {mv[num++] = getMove(rloc NW ME,rstep);}
      if(ISP(rloc NE) && b.isThawed(rloc NE)                             ) {mv[num++] = getMove(rloc NE MW,rstep);}
      if(ISP(rloc NN) && b.isThawed(rloc NN) && b.isRabOkayS(pla,rloc NN)) {mv[num++] = getMove(rloc NN MS,rstep);}
    }
  }
  return num;
}

static int genVacateAndRabStepTo(Board& b, pla_t pla, loc_t ploc, loc_t rloc, step_t rstep, move_t* mv)
{
  if(b.isTrapSafe2(pla,rloc) && b.wouldBeUF(pla,rloc,rloc,ploc))
  {
    int num = 0;
    if(ISE(ploc S) && b.isRabOkay(pla,ploc,ploc S)) mv[num++] = getMove(ploc MS,rstep);
    if(ISE(ploc W))                                 mv[num++] = getMove(ploc MW,rstep);
    if(ISE(ploc E))                                 mv[num++] = getMove(ploc ME,rstep);
    if(ISE(ploc N) && b.isRabOkay(pla,ploc,ploc N)) mv[num++] = getMove(ploc MN,rstep);
    return num;
  }
  return 0;
}

static int genPushesOf(Board& b, pla_t pla, loc_t oloc, move_t* mv)
{
  int num = 0;
  if(ISP(oloc S) && GT(oloc S,oloc) && b.isThawed(oloc S))
  {
    if(ISE(oloc W)) mv[num++] = getMove(oloc MW,oloc S MN);
    if(ISE(oloc E)) mv[num++] = getMove(oloc ME,oloc S MN);
    if(ISE(oloc N)) mv[num++] = getMove(oloc MN,oloc S MN);
  }
  if(ISP(oloc W) && GT(oloc W,oloc) && b.isThawed(oloc W))
  {
    if(ISE(oloc S)) mv[num++] = getMove(oloc MS,oloc W ME);
    if(ISE(oloc E)) mv[num++] = getMove(oloc ME,oloc W ME);
    if(ISE(oloc N)) mv[num++] = getMove(oloc MN,oloc W ME);
  }
  if(ISP(oloc E) && GT(oloc E,oloc) && b.isThawed(oloc E))
  {
    if(ISE(oloc S)) mv[num++] = getMove(oloc MS,oloc E MW);
    if(ISE(oloc W)) mv[num++] = getMove(oloc MW,oloc E MW);
    if(ISE(oloc N)) mv[num++] = getMove(oloc MN,oloc E MW);
  }
  if(ISP(oloc N) && GT(oloc N,oloc) && b.isThawed(oloc N))
  {
    if(ISE(oloc S)) mv[num++] = getMove(oloc MS,oloc N MS);
    if(ISE(oloc W)) mv[num++] = getMove(oloc MW,oloc N MS);
    if(ISE(oloc E)) mv[num++] = getMove(oloc ME,oloc N MS);
  }
  return num;
}

static int genAddRabbitDefender(Board& b, pla_t pla, int numSteps, Bitmap map, move_t* mv)
{
  int num = 0;
  while(map.hasBits())
  {
    loc_t rloc = map.nextBit();
    //Check that there is the possiblity to add a defender
    if((Board::RADIUS[2][rloc] & b.pieceMaps[pla][0] & ~b.frozenMap).isEmpty())
      continue;

    //Regular steps to add a rabbit defender
    if(ISE(rloc S))
    {
      if(ISP(rloc SS) && b.isThawed(rloc SS) && b.isRabOkayN(pla,rloc SS)) {mv[num++] = getMove(rloc SS MN);}
      if(ISP(rloc SW) && b.isThawed(rloc SW)                             ) {mv[num++] = getMove(rloc SW ME);}
      if(ISP(rloc SE) && b.isThawed(rloc SE)                             ) {mv[num++] = getMove(rloc SE MW);}
    }
    if(ISE(rloc W))
    {
      if(ISP(rloc SW) && b.isThawed(rloc SW) && b.isRabOkayN(pla,rloc SW)) {mv[num++] = getMove(rloc SW MN);}
      if(ISP(rloc WW) && b.isThawed(rloc WW)                             ) {mv[num++] = getMove(rloc WW ME);}
      if(ISP(rloc NW) && b.isThawed(rloc NW) && b.isRabOkayS(pla,rloc NW)) {mv[num++] = getMove(rloc NW MS);}
    }
    if(ISE(rloc E))
    {
      if(ISP(rloc SE) && b.isThawed(rloc SE) && b.isRabOkayN(pla,rloc SE)) {mv[num++] = getMove(rloc SE MN);}
      if(ISP(rloc EE) && b.isThawed(rloc EE)                             ) {mv[num++] = getMove(rloc EE MW);}
      if(ISP(rloc NE) && b.isThawed(rloc NE) && b.isRabOkayS(pla,rloc NE)) {mv[num++] = getMove(rloc NE MS);}
    }
    if(ISE(rloc N))
    {
      if(ISP(rloc NW) && b.isThawed(rloc NW)                             ) {mv[num++] = getMove(rloc NW ME);}
      if(ISP(rloc NE) && b.isThawed(rloc NE)                             ) {mv[num++] = getMove(rloc NE MW);}
      if(ISP(rloc NN) && b.isThawed(rloc NN) && b.isRabOkayS(pla,rloc NN)) {mv[num++] = getMove(rloc NN MS);}
    }

    //Pushes to add a rabbit defender
    if(numSteps >= 2)
    {
      pla_t opp = gOpp(pla);
      if(ISO(rloc S)) num += genPushesOf(b,pla,rloc S,mv+num);
      if(ISO(rloc W)) num += genPushesOf(b,pla,rloc W,mv+num);
      if(ISO(rloc E)) num += genPushesOf(b,pla,rloc E,mv+num);
      if(ISO(rloc N)) num += genPushesOf(b,pla,rloc N,mv+num);
    }
  }
  return num;
}

//Generate pulls of olocs by pieces W and E of it.
static int genPullsByWE(Board& b, pla_t pla, Bitmap olocs, move_t* mv)
{
  int num = 0;
  while(olocs.hasBits())
  {
    loc_t oloc = olocs.nextBit();
    //Pull each way if possible
    if(ISP(oloc W) && GT(oloc W,oloc) && b.isThawed(oloc W))
    {
      if(ISE(oloc SW)) mv[num++] = getMove(oloc W MS,oloc MW);
      if(ISE(oloc NW)) mv[num++] = getMove(oloc W MN,oloc MW);
      if(ISE(oloc WW)) mv[num++] = getMove(oloc W MW,oloc MW);
    }
    if(ISP(oloc E) && GT(oloc E,oloc) && b.isThawed(oloc E))
    {
      if(ISE(oloc SE)) mv[num++] = getMove(oloc E MS,oloc ME);
      if(ISE(oloc NE)) mv[num++] = getMove(oloc E MN,oloc ME);
      if(ISE(oloc EE)) mv[num++] = getMove(oloc E ME,oloc ME);
    }
  }
  return num;
}

//Expects a map of dominated opp pieces at gydist 2 or 3 and an adjacent rabbit
//Expects numSteps >= 2
static int genPullsAndMaybeRabStep(Board& b, pla_t pla, int numSteps, Bitmap olocs, move_t* mv)
{
  int num = 0;
  int dy = Board::GOALLOCINC[pla];
  pla_t opp = gOpp(pla);
  while(olocs.hasBits())
  {
    loc_t oloc = olocs.nextBit();

    //Look for a rabbit
    loc_t rloc;
    if(b.owners[oloc W] == pla && b.pieces[oloc W] == RAB) rloc = oloc W;
    else if(b.owners[oloc E] == pla && b.pieces[oloc E] == RAB) rloc = oloc E;
    else if(b.owners[oloc F] == pla && b.pieces[oloc F] == RAB) rloc = oloc F;
    else continue;

    bool wuf = b.wouldBeUF(pla,rloc,rloc,oloc);
    //If the opp isn't a rabbit, then make sure it won't immediately be able to push the pla rabbit back.
    bool isOppRab = b.pieces[oloc] == RAB;

    int startNum = num;
    //Look around for a dominating unfrozen pla
    //Pull each way if possible
    if(ISP(oloc W) && GT(oloc W,oloc) && b.isThawed(oloc W) && (isOppRab || (b.pieceMaps[opp][0] & Board::RADIUS[1][oloc W]).isEmptyOrSingleBit()))
    {
      if(ISE(oloc GW) && (wuf || b.isAdjacent(oloc GW,rloc))) mv[num++] = getMove(oloc W MG,oloc MW);
      if(ISE(oloc WW) && (wuf                              )) mv[num++] = getMove(oloc W MW,oloc MW);
      if(ISE(oloc FW) && (wuf || b.isAdjacent(oloc FW,rloc))) mv[num++] = getMove(oloc W MF,oloc MW);
    }
    if(ISP(oloc E) && GT(oloc E,oloc) && b.isThawed(oloc E) && (isOppRab || (b.pieceMaps[opp][0] & Board::RADIUS[1][oloc E]).isEmptyOrSingleBit()))
    {
      if(ISE(oloc GE) && (wuf || b.isAdjacent(oloc GE,rloc))) mv[num++] = getMove(oloc E MG,oloc ME);
      if(ISE(oloc EE) && (wuf                              )) mv[num++] = getMove(oloc E ME,oloc ME);
      if(ISE(oloc FE) && (wuf || b.isAdjacent(oloc FE,rloc))) mv[num++] = getMove(oloc E MF,oloc ME);
    }
    if(ISP(oloc F) && GT(oloc F,oloc) && b.isThawed(oloc F) && (isOppRab || (b.pieceMaps[opp][0] & Board::RADIUS[1][oloc F]).isEmptyOrSingleBit()))
    {
      if(ISE(oloc FW) && (wuf || b.isAdjacent(oloc FW,rloc))) mv[num++] = getMove(oloc F MW,oloc MF);
      if(ISE(oloc FF) && (wuf                              )) mv[num++] = getMove(oloc F MF,oloc MF);
      if(ISE(oloc FE) && (wuf || b.isAdjacent(oloc FE,rloc))) mv[num++] = getMove(oloc F ME,oloc MF);
    }

    //Duplicate each move, one having a terminal rstep and one not
    if(numSteps >= 3)
    {
      int endNum = num;
      step_t rstep = gStepSrcDest(rloc,oloc);
      for(int i = startNum; i<endNum; i++)
      {
        move_t move = mv[i];
        mv[num++] = move;
        mv[i] = setStep(move,rstep,2);
      }
    }
  }
  return num;
}

//Expects a pla piece on the 7th rank that isn't frozen
static int genSplinter7thCreates(Board& b, int numSteps, pla_t pla, loc_t ploc, move_t* mv)
{
  int num = 0;
  int dy = Board::GOALLOCINC[pla];
  pla_t opp = gOpp(pla);

  //Make sure that we won't be dominated on the target square.
  if(b.wouldBeDom(pla,ploc,ploc G))
    return 0;
  //Make sure that if behind is a rabbit on a trap, we don't sac it by moving.
  if(b.owners[ploc F] == pla && b.pieces[ploc F] == RAB && !b.isTrapSafe2(pla,ploc F))
    return 0;

  if(ISE(ploc G))
  {
    mv[num++] = getMove(ploc MG);
  }
  else if(numSteps >= 2 && (ISP(ploc G) || (ISO(ploc G) && GT(ploc,ploc G))))
  {
    if(ISE(ploc GW)) mv[num++] = getMove(ploc G MW, ploc MG);
    if(ISE(ploc GE)) mv[num++] = getMove(ploc G ME, ploc MG);
  }
  return num;
}

static int genStepToPullingAdjRabbits(Board& b, pla_t opp, loc_t src, step_t step0, move_t* mv)
{
  int num = 0;
  if(ISO(src S) && b.pieces[src S] == RAB) mv[num++] = getMove(step0,src S MN);
  if(ISO(src W) && b.pieces[src W] == RAB) mv[num++] = getMove(step0,src W ME);
  if(ISO(src E) && b.pieces[src E] == RAB) mv[num++] = getMove(step0,src E MW);
  if(ISO(src N) && b.pieces[src N] == RAB) mv[num++] = getMove(step0,src N MS);
  return num;
}

//Expects a map of opponents that are blocking unfrozen rabbits
static int genUndermines(Board& b, pla_t pla, Bitmap olocs, Bitmap plaNonRab, int numSteps, move_t* mv)
{
  int num = 0;
  int dy = Board::GOALLOCINC[pla];
  pla_t opp = gOpp(pla);
  while(olocs.hasBits())
  {
    loc_t oloc = olocs.nextBit();
    if((plaNonRab & ~b.frozenMap & Board::RADIUS[2][oloc]).hasBits())
    {
      //Look around for a pla to step or push in
      if(ISE(oloc W))
      {
        if(ISP(oloc FW) && GT(oloc FW,oloc) && b.isThawed(oloc FW)) mv[num++] = getMove(oloc FW MG);
        if(ISP(oloc WW) && GT(oloc WW,oloc) && b.isThawed(oloc WW)) mv[num++] = getMove(oloc WW ME);
        if(ISP(oloc GW) && GT(oloc GW,oloc) && b.isThawed(oloc GW)) mv[num++] = getMove(oloc GW MF);
      }
      if(ISE(oloc E))
      {
        if(ISP(oloc FE) && GT(oloc FE,oloc) && b.isThawed(oloc FE)) mv[num++] = getMove(oloc FE MG);
        if(ISP(oloc EE) && GT(oloc EE,oloc) && b.isThawed(oloc EE)) mv[num++] = getMove(oloc EE MW);
        if(ISP(oloc GE) && GT(oloc GE,oloc) && b.isThawed(oloc GE)) mv[num++] = getMove(oloc GE MF);
      }
      if(ISE(oloc F))
      {
        if(ISP(oloc FW) && GT(oloc FW,oloc) && b.isThawed(oloc FW)) mv[num++] = getMove(oloc FW ME);
        if(ISP(oloc FF) && GT(oloc FF,oloc) && b.isThawed(oloc FF)) mv[num++] = getMove(oloc FF MG);
        if(ISP(oloc FE) && GT(oloc FE,oloc) && b.isThawed(oloc FE)) mv[num++] = getMove(oloc FE MW);
      }
      if(numSteps >= 2)
      {
        if(ISO(oloc W))
        {
          if(ISP(oloc FW) && GT(oloc FW,oloc) && GT(oloc FW,oloc W) && b.isThawed(oloc FW)) {if(ISE(oloc WW)) mv[num++] = getMove(oloc W MW,oloc FW MG); if(ISE(oloc GW)) mv[num++] = getMove(oloc W MG,oloc FW MG);}
          if(ISP(oloc WW) && GT(oloc WW,oloc) && GT(oloc WW,oloc W) && b.isThawed(oloc WW)) {if(ISE(oloc FW)) mv[num++] = getMove(oloc W MF,oloc WW ME); if(ISE(oloc GW)) mv[num++] = getMove(oloc W MG,oloc WW ME);}
          if(ISP(oloc GW) && GT(oloc GW,oloc) && GT(oloc GW,oloc W) && b.isThawed(oloc GW)) {if(ISE(oloc FW)) mv[num++] = getMove(oloc W MF,oloc GW MF); if(ISE(oloc WW)) mv[num++] = getMove(oloc W MW,oloc GW MF);}
        }
        if(ISO(oloc E))
        {
          if(ISP(oloc FE) && GT(oloc FE,oloc) && GT(oloc FE,oloc E) && b.isThawed(oloc FE)) {if(ISE(oloc EE)) mv[num++] = getMove(oloc E ME,oloc FE MG); if(ISE(oloc GE)) mv[num++] = getMove(oloc E MG,oloc FE MG);}
          if(ISP(oloc EE) && GT(oloc EE,oloc) && GT(oloc EE,oloc E) && b.isThawed(oloc EE)) {if(ISE(oloc FE)) mv[num++] = getMove(oloc E MF,oloc EE MW); if(ISE(oloc GE)) mv[num++] = getMove(oloc E MG,oloc EE MW);}
          if(ISP(oloc GE) && GT(oloc GE,oloc) && GT(oloc GE,oloc E) && b.isThawed(oloc GE)) {if(ISE(oloc FE)) mv[num++] = getMove(oloc E MF,oloc GE MF); if(ISE(oloc EE)) mv[num++] = getMove(oloc E ME,oloc GE MF);}
        }
        if(ISO(oloc F))
        {
          if(ISP(oloc FW) && GT(oloc FW,oloc) && GT(oloc FW,oloc F) && b.isThawed(oloc FW)) {if(ISE(oloc FF)) mv[num++] = getMove(oloc F MF,oloc FW ME); if(ISE(oloc FE)) mv[num++] = getMove(oloc F ME,oloc FW ME);}
          if(ISP(oloc FF) && GT(oloc FF,oloc) && GT(oloc FF,oloc F) && b.isThawed(oloc FF)) {if(ISE(oloc FW)) mv[num++] = getMove(oloc F MW,oloc FF MG); if(ISE(oloc FE)) mv[num++] = getMove(oloc F ME,oloc FF MG);}
          if(ISP(oloc FE) && GT(oloc FE,oloc) && GT(oloc FE,oloc F) && b.isThawed(oloc FE)) {if(ISE(oloc FF)) mv[num++] = getMove(oloc F MF,oloc FE MW); if(ISE(oloc FW)) mv[num++] = getMove(oloc F MW,oloc FE MW);}
        }

        //Pull opp rabbit while undermining - the pulled rabbit might block opp defenders
        if(ISE(oloc W))
        {
          if(ISP(oloc FW) && GT(oloc FW,oloc) && b.isThawed(oloc FW)) num += genStepToPullingAdjRabbits(b,opp,oloc FW,oloc FW MG,mv+num);
          if(ISP(oloc WW) && GT(oloc WW,oloc) && b.isThawed(oloc WW)) num += genStepToPullingAdjRabbits(b,opp,oloc WW,oloc WW ME,mv+num);
        }
        if(ISE(oloc E))
        {
          if(ISP(oloc FE) && GT(oloc FE,oloc) && b.isThawed(oloc FE)) num += genStepToPullingAdjRabbits(b,opp,oloc FE,oloc FE MG,mv+num);
          if(ISP(oloc EE) && GT(oloc EE,oloc) && b.isThawed(oloc EE)) num += genStepToPullingAdjRabbits(b,opp,oloc EE,oloc EE MW,mv+num);
        }
      }
    }

    if(numSteps >= 3)
    {
      if((plaNonRab & ~b.frozenMap & Board::RADIUS[3][oloc]).hasBits())
      {
        if(ISO(oloc W)) num += BoardTrees::genStepAndPush3Step(b,mv+num,oloc W);
        if(ISO(oloc E)) num += BoardTrees::genStepAndPush3Step(b,mv+num,oloc E);
      }
    }
  }
  return num;
}

//Expects a map of opp pieces on the 8th rank that are undermined
static int genMakeSpaceAroundUndermines(Board& b, pla_t pla, int numSteps, Bitmap olocs, move_t* mv)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  int dy = Board::GOALLOCINC[pla];
  while(olocs.hasBits())
  {
    loc_t oloc = olocs.nextBit();

    if(numSteps <= 1)
    {
      if(ISP(oloc W) && GT(oloc W,oloc) && b.isThawed(oloc W) && ISE(oloc WW))
        mv[num++] = getMove(oloc W MW);
      if(ISP(oloc E) && GT(oloc E,oloc) && b.isThawed(oloc E) && ISE(oloc EE))
        mv[num++] = getMove(oloc E ME);
    }
    else
    {
      if(ISP(oloc W) && GT(oloc W,oloc) && b.isThawed(oloc W) && ISO(oloc WW) && GT(oloc W,oloc WW))
      {
        if(ISE(oloc WWW)) mv[num++] = getMove(oloc WW MW,oloc W MW);
        if(ISE(oloc FWW)) mv[num++] = getMove(oloc WW MF,oloc W MW);
      }
      if(ISP(oloc E) && GT(oloc E,oloc) && b.isThawed(oloc E) && ISO(oloc EE) && GT(oloc E,oloc EE))
      {
        if(ISE(oloc EEE)) mv[num++] = getMove(oloc EE ME,oloc E ME);
        if(ISE(oloc FEE)) mv[num++] = getMove(oloc EE MF,oloc E ME);
      }
    }
  }
  return num;
}

//Expects a map of opp pieces on the 8th rank that are blocking a rabbit
static int genDistantUndermines(Board& b, pla_t pla, int numSteps, Bitmap olocs, move_t* mv)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  int dy = Board::GOALLOCINC[pla];
  while(olocs.hasBits())
  {
    loc_t oloc = olocs.nextBit();
    if(b.isFrozen(oloc F))
    {
      //Make sure the rabbit would be uf if we weren't here
      if(!b.wouldBeUF(pla,oloc F,oloc F,oloc))
        continue;
      //A distant undermine is no good if the rabbit can just be pushed back
      if(ISE(oloc FF))
        continue;
    }

    if(ISE(oloc W))
    {
      if(ISE(oloc WW))
      {
        if(ISP(oloc WWW) && b.isThawed(oloc WWW)) mv[num++] = getMove(oloc WWW ME);
        if(ISP(oloc FWW) && b.isThawed(oloc FWW)) mv[num++] = getMove(oloc FWW MG);
      }
      else if(ISO(oloc WW) && numSteps >= 2)
      {
        if(ISP(oloc WWW) && ISE(oloc FWW) && GT(oloc WWW,oloc WW) && b.isThawed(oloc WWW)) mv[num++] = getMove(oloc WW MF,oloc WWW ME);
        if(ISP(oloc FWW) && ISE(oloc WWW) && GT(oloc FWW,oloc WW) && b.isThawed(oloc FWW)) mv[num++] = getMove(oloc WW MW,oloc FWW MG);
      }
    }
    if(ISE(oloc E))
    {
      if(ISE(oloc EE))
      {
        if(ISP(oloc EEE) && b.isThawed(oloc EEE)) mv[num++] = getMove(oloc EEE MW);
        if(ISP(oloc FEE) && b.isThawed(oloc FEE)) mv[num++] = getMove(oloc FEE MG);
      }
      else if(ISO(oloc EE) && numSteps >= 2)
      {
        if(ISP(oloc EEE) && ISE(oloc FEE) && GT(oloc EEE,oloc EE) && b.isThawed(oloc EEE)) mv[num++] = getMove(oloc EE MF,oloc EEE MW);
        if(ISP(oloc FEE) && ISE(oloc EEE) && GT(oloc FEE,oloc EE) && b.isThawed(oloc FEE)) mv[num++] = getMove(oloc EE ME,oloc FEE MG);
      }
    }
  }
  return num;
}

//Corner should be the corner of the board. dyLoc and dxLoc should be the loc offsets to point inward
static int genGTSquashedCorner1(Board& b, pla_t pla, loc_t corner, int dxLoc, int dyLoc, int numSteps, move_t* mv)
{
  //SQUASHED CORNER #1
  //|..*
  //|rcC
  //|RRe
  //+---
  pla_t opp = gOpp(pla);
  //Check basic conditions
  if(!(ISO(corner) &&
       ISP(corner+dyLoc) && b.pieces[corner+dyLoc] == RAB &&
       ISP(corner+dyLoc+dxLoc) && b.pieces[corner+dyLoc+dxLoc] > RAB &&
       ISO(corner+dxLoc) && b.pieces[corner+dxLoc] == RAB))
    return 0;
  //Check no opps at important spots
  if(ISO(corner+dxLoc+dxLoc) || ISO(corner+dyLoc+dyLoc+dxLoc) || (ISO(corner+dyLoc+dyLoc) && b.pieces[corner+dyLoc+dyLoc] != RAB))
    return 0;

  //Check that the new spot is not likely to be refillable
  if(ISO(corner+dxLoc+dxLoc))
    return 0;

  //Pull, vacating if needed
  int num = 0;
  loc_t toPull = corner+dxLoc;
  loc_t src = corner+dyLoc+dxLoc;
  loc_t dest = corner+dyLoc+dyLoc+dxLoc;
  if(ISE(dest))
  {
    mv[num++] = getMove(gStepSrcDest(src,dest),gStepSrcDest(toPull,src));
  }
  else if(numSteps >= 3)
  {
    if(ISE(dest-dxLoc))
      mv[num++] = getMove(gStepSrcDest(dest,dest-dxLoc),gStepSrcDest(src,dest),gStepSrcDest(toPull,src));
    if(ISE(dest+dxLoc))
      mv[num++] = getMove(gStepSrcDest(dest,dest+dxLoc),gStepSrcDest(src,dest),gStepSrcDest(toPull,src));
    else if(ISE(dest+dyLoc) && !ISE(dest-dxLoc) && b.pieces[dest] != RAB)
      mv[num++] = getMove(gStepSrcDest(dest,dest+dyLoc),gStepSrcDest(src,dest),gStepSrcDest(toPull,src));
  }

  if(ISE(corner+dyLoc+dxLoc+dxLoc))
  {
    mv[num++] = getMove(gStepSrcDest(src,corner+dyLoc+dxLoc+dxLoc),gStepSrcDest(toPull,src));
  }

  return num;
}

int BoardTrees::genSevereGoalThreats(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  DEBUGASSERT(numSteps > 0);

  pla_t opp = gOpp(pla);
  Bitmap empty = ~b.pieceMaps[GOLD][0] & ~b.pieceMaps[SILV][0];
  Bitmap plaRab = b.pieceMaps[pla][RAB];
  Bitmap oppAny = b.pieceMaps[opp][0];
  Bitmap oppAnyUF = b.pieceMaps[opp][0] & ~b.frozenMap;
  Bitmap oppRab = b.pieceMaps[opp][RAB];
  Bitmap oppNonRab = b.pieceMaps[opp][0] & ~b.pieceMaps[opp][RAB];
  Bitmap plaNonRab = b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB];

  //WALKING RABBIT TO 7TH EMPTY------------------------------------------------------------------------
  //Directly walking a rabbit to 7th rank where 8th rank is empty and rabbit won't be dominated by uf.
  //1-step unfreeze of rabbit that then steps to such a location.

  //Find locations one step away from the goal row that are empty with the following row also empty
  Bitmap oneStepThreatLocs;
  //TODO check if it's a gain to hardcode BMPY[0] and BMPY[7] - does the compiler do a lookup or does it inline the constant?
  //maybe use constexpr
  if(pla == GOLD)
    oneStepThreatLocs = Bitmap::shiftS(empty & Bitmap::BMPY[7]);
  else
    oneStepThreatLocs = Bitmap::shiftN(empty & Bitmap::BMPY[0]);

  int num = 0;
  if(numSteps <= 1) oneStepThreatLocs &= empty;
  while(oneStepThreatLocs.hasBits())
  {
    loc_t loc = oneStepThreatLocs.nextBit();
    //Check that a rabbit would not be dominated there by an unfrozen piece.
    if((Board::RADIUS[1][loc] & oppNonRab & ~b.frozenMap).hasBits())
      continue;

    if(ISE(loc))
    {
      //In the case where the square would have the rabbit definitely unfrozen, then we can
      //look for rabbits at the exact right distance (if they were any closer, then if they could make it
      //there then they could also probably just goal. The only exception is for certain rabbit steps
      //that sac pieces on traps causing the rabbit to be later frozen, but we're okay not generating those).
      Bitmap possibleRabs;
      if((Board::RADIUS[1][loc] & oppNonRab).isEmpty() || (Board::RADIUS[1][loc] & plaNonRab).hasBits())
        possibleRabs = Board::RADIUS[numSteps][loc] & plaRab & ~b.frozenMap;
      else
        possibleRabs = Board::DISK[numSteps][loc] & plaRab & ~b.frozenMap;

      //Here we go!
      num += genRabbitWalksToBy(b,pla,loc,possibleRabs,mv+num);

      //Look also for rabbits that are frozen exactly at radius 1 that we can 1-step unfreeze and step.
      if(numSteps >= 2)
      {
        possibleRabs = Board::RADIUS[1][loc] & plaRab & b.frozenMap;
        num += genRabbitUFAndStepTo(b,pla,loc,possibleRabs,mv+num);
      }
    }
    //Step pla out of the way
    else if(ISP(loc))
    {
      int dy = Board::GOALLOCINC[pla];
      if(ISP(loc F) && b.pieces[loc F] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc F,loc F MG,mv+num);
      if(ISP(loc W) && b.pieces[loc W] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc W,loc W ME,mv+num);
      if(ISP(loc E) && b.pieces[loc E] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc E,loc E MW,mv+num);
    }
  }

  //WALKING RABBIT TO 7TH BLOCKED------------------------------------------------------------------------
  //Directly walking a rabbit to 7th rank where 8th rank has an opp rabbit or pla, and various patterns based off this.
  Bitmap blocked7ThreatLocs;
  if(pla == GOLD)
  {
    //Opp rabbit, or pla
    Bitmap weakBlockers = ~oppNonRab & ~empty & Bitmap::BMPY[7];

    //Blocked with a pla
    blocked7ThreatLocs = Bitmap::shiftS(plaNonRab & Bitmap::BMPY[7]);
    //Weakly blocked, adjacent undominated friendly piece with the square in front of that piece being empty.
    blocked7ThreatLocs |= Bitmap::shiftS(weakBlockers) & Bitmap::adjWE(Bitmap::shiftS(empty & Bitmap::BMPY[7]) & plaNonRab & ~b.dominatedMap);
    //Piece in front is weak with an undominated pla piece adjacent
    blocked7ThreatLocs |= Bitmap::shiftS(weakBlockers & Bitmap::adjWE(plaNonRab & Bitmap::BMPY[7] & ~b.dominatedMap));
  }
  else
  {
    //Opp rabbit, or pla
    Bitmap weakBlockers = ~oppNonRab & ~empty & Bitmap::BMPY[0];
    //Blocked with a pla
    blocked7ThreatLocs = Bitmap::shiftN(plaNonRab & Bitmap::BMPY[0]);
    //Weakly blocked, adjacent undominated friendly piece with the square in front of that piece being empty.
    blocked7ThreatLocs |= Bitmap::shiftN(weakBlockers) & Bitmap::adjWE(Bitmap::shiftN(empty & Bitmap::BMPY[0]) & plaNonRab & ~b.dominatedMap);
    //Piece in front is weak with an undominated pla piece adjacent
    blocked7ThreatLocs |= Bitmap::shiftN(weakBlockers & Bitmap::adjWE(plaNonRab & Bitmap::BMPY[0] & ~b.dominatedMap));
  }
  if(numSteps <= 1) blocked7ThreatLocs &= empty;
  while(blocked7ThreatLocs.hasBits())
  {
    loc_t loc = blocked7ThreatLocs.nextBit();
    //Check that a rabbit would be undominated
    if((Board::RADIUS[1][loc] & oppNonRab).hasBits())
      continue;
    if(ISE(loc))
    {
      //Look for rabbits within the right distance.
      Bitmap possibleRabs = Board::DISK[numSteps][loc] & plaRab & ~b.frozenMap;
      num += genRabbitWalksToBy(b,pla,loc,possibleRabs,mv+num);
    }
    //Step pla out of the way
    else if(ISP(loc))
    {
      int dy = Board::GOALLOCINC[pla];
      if(ISP(loc F) && b.pieces[loc F] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc F,loc F MG,mv+num);
      if(ISP(loc W) && b.pieces[loc W] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc W,loc W ME,mv+num);
      if(ISP(loc E) && b.pieces[loc E] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc E,loc E MW,mv+num);
    }
  }

  //WALKING RABBIT TO 6TH VERY EMPTY------------------------------------------------------------------------
  //Find locations that are two steps away from goal where a whole 2x2 block of stuff in front is not an opp, or is a frozen opp
  //Allow a single opp rabbit if the square in front is protected
  Bitmap twoStepThreatLocs;
  if(pla == GOLD)
  {
    Bitmap doubleNotOppAny = Bitmap::shiftS(~oppAnyUF & Bitmap::BMPY[7]) & ~oppAnyUF;
    Bitmap doubleOnlyOneRab = (Bitmap::shiftS(oppRab & Bitmap::BMPY[7]) & ~oppAnyUF) | (Bitmap::shiftS(~oppAnyUF & Bitmap::BMPY[7]) & oppRab);
    twoStepThreatLocs = doubleNotOppAny & Bitmap::adjWE(doubleNotOppAny);
    twoStepThreatLocs |= Bitmap::adjWE(plaNonRab) & ((doubleOnlyOneRab & Bitmap::adjWE(doubleNotOppAny)) | (doubleNotOppAny & Bitmap::adjWE(doubleOnlyOneRab)));
    twoStepThreatLocs = Bitmap::shiftS(twoStepThreatLocs);
  }
  else
  {
    Bitmap doubleNotOppAny = Bitmap::shiftN(~oppAnyUF & Bitmap::BMPY[0]) & ~oppAnyUF;
    Bitmap doubleOnlyOneRab = (Bitmap::shiftN(oppRab & Bitmap::BMPY[0]) & ~oppAnyUF) | (Bitmap::shiftN(~oppAnyUF & Bitmap::BMPY[0]) & oppRab);
    twoStepThreatLocs = doubleNotOppAny & Bitmap::adjWE(doubleNotOppAny);
    twoStepThreatLocs |= Bitmap::adjWE(plaNonRab) & ((doubleOnlyOneRab & Bitmap::adjWE(doubleNotOppAny)) | (doubleNotOppAny & Bitmap::adjWE(doubleOnlyOneRab)));
    twoStepThreatLocs = Bitmap::shiftN(twoStepThreatLocs);
  }
  if(numSteps <= 1) twoStepThreatLocs &= empty;
  while(twoStepThreatLocs.hasBits())
  {
    loc_t loc = twoStepThreatLocs.nextBit();
    //Check that a rabbit would be undominated and trapsafe
    if((Board::RADIUS[1][loc] & oppNonRab).hasBits())
      continue;

    if(ISE(loc))
    {
      //Look for rabbits within the right distance.
      if(b.isTrapSafe2(pla,loc))
      {
        Bitmap possibleRabs = Board::RADIUS[1][loc] & plaRab & ~b.frozenMap;
        num += genRabbitWalksToBy(b,pla,loc,possibleRabs,mv+num);
      }
      if(numSteps >= 2 && b.isTrapSafe1(pla,loc))
      {
        Bitmap possibleRabs = Board::RADIUS[numSteps][loc] & plaRab & ~b.frozenMap;
        num += genRabbitWalksToBy(b,pla,loc,possibleRabs,mv+num);
      }
    }
    //Step pla out of the way
    else if(ISP(loc))
    {
      int dy = Board::GOALLOCINC[pla];
      if(ISP(loc F) && b.pieces[loc F] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc F,loc F MG,mv+num);
      if(ISP(loc W) && b.pieces[loc W] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc W,loc W ME,mv+num);
      if(ISP(loc E) && b.pieces[loc E] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc E,loc E MW,mv+num);
    }
  }

  //WALKING RABBIT TO 5TH VERY EMPTY------------------------------------------------------------------------
  //Find locations that are three steps away from goal where in front is very empty
  Bitmap threeStepThreatLocs;
  if(pla == GOLD)
  {
    Bitmap oppRabCols = Bitmap::shiftS(oppRab & Bitmap::BMPY[7]);
    Bitmap noOppCols = Bitmap::shiftS(~oppAny & Bitmap::BMPY[7]);

    oppRabCols = Bitmap::shiftS((oppRab & noOppCols) | (~oppAny & oppRabCols));
    noOppCols = Bitmap::shiftS(~oppAny & noOppCols);

    oppRabCols = Bitmap::shiftS((oppRab & noOppCols) | (~oppAny & oppRabCols));
    noOppCols = Bitmap::shiftS(~oppAny & noOppCols);

    threeStepThreatLocs = ~Bitmap::adj(oppNonRab) & (
        (oppRabCols & Bitmap::adjWE(noOppCols)) | (noOppCols & Bitmap::adjWE(oppRabCols)));
  }
  else
  {
    Bitmap oppRabCols = Bitmap::shiftN(oppRab & Bitmap::BMPY[0]);
    Bitmap noOppCols = Bitmap::shiftN(~oppAny & Bitmap::BMPY[0]);

    oppRabCols = Bitmap::shiftN((oppRab & noOppCols) | (~oppAny & oppRabCols));
    noOppCols = Bitmap::shiftN(~oppAny & noOppCols);

    oppRabCols = Bitmap::shiftN((oppRab & noOppCols) | (~oppAny & oppRabCols));
    noOppCols = Bitmap::shiftN(~oppAny & noOppCols);

    threeStepThreatLocs = ~Bitmap::adj(oppNonRab) & (
        (oppRabCols & Bitmap::adjWE(noOppCols)) | (noOppCols & Bitmap::adjWE(oppRabCols)));
  }
  if(numSteps <= 1) threeStepThreatLocs &= empty;
  while(threeStepThreatLocs.hasBits())
  {
    loc_t loc = threeStepThreatLocs.nextBit();

    if(ISE(loc))
    {
      //Look for rabbits within the right distance.
      Bitmap possibleRabs = Board::RADIUS[numSteps][loc] & plaRab & ~b.frozenMap;
      num += genRabbitWalksToBy(b,pla,loc,possibleRabs,mv+num);
    }
    //Step pla out of the way
    else if(ISP(loc))
    {
      int dy = Board::GOALLOCINC[pla];
      if(ISP(loc F) && b.pieces[loc F] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc F,loc F MG,mv+num);
      if(ISP(loc W) && b.pieces[loc W] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc W,loc W ME,mv+num);
      if(ISP(loc E) && b.pieces[loc E] == RAB) num += genVacateAndRabStepTo(b,pla,loc,loc E,loc E MW,mv+num);
    }
  }

  //PULL 8TH --------------------------------------------------------------------
  //Pull a piece on the 8th rank in front of a 7th rank rabbit, or a 6th rank rabbit where the 7th rank space is empty and there is no
  //unfrozen opp nonrabbit piece adjacent
  if(numSteps >= 2)
  {
    Bitmap pullTargets;
    if(pla == GOLD)
    {
      Bitmap rank7Rabs = plaRab;
      Bitmap rank7InFrontOfRank6Rabs = Bitmap::shiftN(plaRab) & empty & ~Bitmap::adjWE(oppNonRab & ~b.frozenMap);
      pullTargets = oppAny & b.dominatedMap & Bitmap::BMPY[7] & Bitmap::shiftN(rank7Rabs | rank7InFrontOfRank6Rabs);
    }
    else
    {
      Bitmap rank7Rabs = plaRab;
      Bitmap rank7InFrontOfRank6Rabs = Bitmap::shiftS(plaRab) & empty & ~Bitmap::adjWE(oppNonRab & ~b.frozenMap);
      pullTargets = oppAny & b.dominatedMap & Bitmap::BMPY[0] & Bitmap::shiftS(rank7Rabs | rank7InFrontOfRank6Rabs);
    }
    num += genPullsByWE(b,pla,pullTargets,mv+num);
  }

  //PULL AND REPLACE 7TH 6TH ----------------------------------------------------
  //Pull a piece on the 6th or 7th rank and replace it with own rabbit in 3 steps.
  //Or do the pull and don't replace - just pull.
  //New rabbit at location cannot be dominated by a currently nonfrozen opp, and also the square in front must not be an opp nonrab.
  if(numSteps >= 2)
  {
    Bitmap pullStepThreatLocs;
    Bitmap y1;
    Bitmap y2;
    if(pla == GOLD)
    {
      pullStepThreatLocs = Bitmap::shiftS(~oppNonRab) & oppAny & b.dominatedMap & Bitmap::adj(plaRab) & ~Bitmap::adj(oppNonRab & ~b.frozenMap);
      y1 = Bitmap::BMPY[6];
      y2 = Bitmap::shiftS((empty | (oppRab & b.dominatedMap)) & Bitmap::shiftS(Bitmap::BMPY[7] & empty));
    }
    else
    {
      pullStepThreatLocs = Bitmap::shiftN(~oppNonRab) & oppAny & b.dominatedMap & Bitmap::adj(plaRab) & ~Bitmap::adj(oppNonRab & ~b.frozenMap);
      y1 = Bitmap::BMPY[1];
      y2 = Bitmap::shiftN((empty | (oppRab & b.dominatedMap)) & Bitmap::shiftN(Bitmap::BMPY[0] & empty));
    }
    num += genPullsAndMaybeRabStep(b,pla,numSteps,pullStepThreatLocs & y1,mv+num);
    num += genPullsAndMaybeRabStep(b,pla,numSteps,pullStepThreatLocs & y2,mv+num);
  }

  //MAKE SPLINTER 7TH--------------------------------------------------------------------------
  //Near where an advanced rabbit is, step or push a strong piece from the 7th rank to the 8th rank to create a splinter.
  //Look for a pla nonrab piece on the 7th rank nondominated and adjacent to a pla rabbit and the 8th rank is not totally blocked.
  Bitmap splinter7Locs;
  if(pla == GOLD)
    splinter7Locs = Bitmap::adj(plaRab) & Bitmap::BMPY[6] & plaNonRab & ~b.dominatedMap &
    Bitmap::shiftS(empty | Bitmap::adjWE(empty));
  else
    splinter7Locs = Bitmap::adj(plaRab) & Bitmap::BMPY[1] & plaNonRab & ~b.dominatedMap &
    Bitmap::shiftN(empty | Bitmap::adjWE(empty));
  while(splinter7Locs.hasBits())
  {
    loc_t ploc = splinter7Locs.nextBit();
    num += genSplinter7thCreates(b,numSteps,pla,ploc,mv+num);
  }

  //SUPPORT RABBIT 67TH---------------------------------------------------------------------------
  //Support a 7-th rank rabbit behind a splinter or behind an undermined blocker.
  //Support a 6-th rank rabbit similarly if the squares in front are empty enough
  Bitmap supportLocs;
  if(pla == GOLD)
  {
    Bitmap weaklyBlocked = empty | plaNonRab | ((oppRab | (oppAny & b.frozenMap)) & b.dominatedMap);
    supportLocs = Bitmap::shiftS(Bitmap::BMPY[7] & weaklyBlocked);
    supportLocs |= Bitmap::shiftS(Bitmap::shiftS(Bitmap::BMPY[7] & empty & Bitmap::adjWE(empty)) & weaklyBlocked);
    supportLocs &= plaRab;
  }
  else
  {
    Bitmap weaklyBlocked = empty | plaNonRab | ((oppRab | (oppAny & b.frozenMap)) & b.dominatedMap);
    supportLocs = Bitmap::shiftN(Bitmap::BMPY[0] & weaklyBlocked);
    supportLocs |= Bitmap::shiftN(Bitmap::shiftN(Bitmap::BMPY[0] & empty & Bitmap::adjWE(empty)) & weaklyBlocked);
    supportLocs &= plaRab;
  }
  num += genAddRabbitDefender(b,pla,numSteps,supportLocs,mv+num);

  //UNDERMINE BLOCKER 678TH------------------------------------------------------------------------
  //Step so as to undermine a blocker piece or rabbit on the last three rows
  //These are opponent pieces that we are attempting to place a strong piece next to
  Bitmap undermineLocs;
  if(pla == GOLD)
  {
    Bitmap undermineLocs8th = oppAny & Bitmap::shiftN(Bitmap::BMPY[6] & plaRab & ~b.frozenMap);
    Bitmap undermineLocs7th = (oppRab | (oppAny & Bitmap::adjWE(plaNonRab)))
                              & Bitmap::shiftN(Bitmap::BMPY[5] & plaRab & ~b.frozenMap)
                              & Bitmap::shiftS(Bitmap::BMPY[7] & empty);
    Bitmap undermineLocs6th = oppRab & Bitmap::shiftN(Bitmap::BMPY[4] & plaRab & ~b.frozenMap)
                              & Bitmap::shiftS(Bitmap::BMPY[6] & empty)
                              & Bitmap::shiftS(Bitmap::BMPY[7] & empty);
    Bitmap sideways7th      = oppRab & Bitmap::adjWE(Bitmap::BMPY[6] & plaRab & ~b.frozenMap)
                              & Bitmap::shiftS(Bitmap::BMPY[7] & empty);
    undermineLocs = undermineLocs8th | undermineLocs7th | undermineLocs6th | sideways7th;
  }
  if(pla == SILV)
  {
    Bitmap undermineLocs8th = oppAny & Bitmap::shiftS(Bitmap::BMPY[1] & plaRab & ~b.frozenMap);
    Bitmap undermineLocs7th = (oppRab | (oppAny & Bitmap::adjWE(plaNonRab)))
                              & Bitmap::shiftS(Bitmap::BMPY[2] & plaRab & ~b.frozenMap)
                              & Bitmap::shiftN(Bitmap::BMPY[0] & empty);
    Bitmap undermineLocs6th = oppRab & Bitmap::shiftS(Bitmap::BMPY[3] & plaRab & ~b.frozenMap)
                              & Bitmap::shiftN(Bitmap::BMPY[1] & empty)
                              & Bitmap::shiftN(Bitmap::BMPY[0] & empty);
    Bitmap sideways7th      = oppRab & Bitmap::adjWE(Bitmap::BMPY[1] & plaRab & ~b.frozenMap)
                              & Bitmap::shiftN(Bitmap::BMPY[0] & empty);
    undermineLocs = undermineLocs8th | undermineLocs7th | undermineLocs6th | sideways7th;
  }
  num += genUndermines(b,pla,undermineLocs,plaNonRab,numSteps,mv+num);

  //MAKE SPACE AROUND UNDERMINE-----------------------------------------------------------------
  //Push W or E away from an undermined piece on the 8th rank to make space for a step and pull next turn.
  Bitmap makeSpaceAroundUndermineLocs;
  if(pla == GOLD)
    makeSpaceAroundUndermineLocs = oppAny & Bitmap::BMPY[7] & b.frozenMap & Bitmap::shiftN(plaRab & ~b.frozenMap);
  else
    makeSpaceAroundUndermineLocs = oppAny & Bitmap::BMPY[0] & b.frozenMap & Bitmap::shiftS(plaRab & ~b.frozenMap);
  num += genMakeSpaceAroundUndermines(b,pla,numSteps,makeSpaceAroundUndermineLocs,mv+num);

  //DISTANT UNDERMINE---------------------------------------------------------------------------
  //Step to a square on the last rank, one space away from a blocker, to prepare for a step and pull next turn.
  Bitmap distantUndermineLocs;
  if(pla == GOLD)
    distantUndermineLocs = oppAny & Bitmap::BMPY[7] & Bitmap::shiftN(plaRab) & Bitmap::adjWE(empty);
  else
    distantUndermineLocs = oppAny & Bitmap::BMPY[0] & Bitmap::shiftS(plaRab) & Bitmap::adjWE(empty);
  num += genDistantUndermines(b,pla,numSteps,distantUndermineLocs,mv+num);

  //SQUASHED CORNER #1--------------------------------------------------------------------------
  if(numSteps >= 2)
  {
    if(pla == GOLD)
    {
      num += genGTSquashedCorner1(b,pla,A8,E,S,numSteps,mv+num);
      num += genGTSquashedCorner1(b,pla,H8,W,S,numSteps,mv+num);
    }
    else
    {
      num += genGTSquashedCorner1(b,pla,A1,E,N,numSteps,mv+num);
      num += genGTSquashedCorner1(b,pla,H1,W,N,numSteps,mv+num);
    }
  }

  return num;
}

//Currently, generates 1-step defenses of the trap where the defending piece will not be dominated
//by an unfrozen opp and the trap has an opp adjacent and the piece will be adjacent to an opp.
int BoardTrees::genContestedDefenses(Board& b, pla_t pla, loc_t kt, int numSteps, move_t* mv)
{
  pla_t opp = gOpp(pla);
  int trapIndex = Board::TRAPINDEX[kt];
  if(b.trapGuardCounts[opp][trapIndex] <= 0)
    return 0;

  DEBUGASSERT(numSteps > 0);
  int num = 0;
  Bitmap oppMap = b.pieceMaps[opp][0];
  for(int i = 0; i<4; i++)
  {
    loc_t adj = kt + Board::ADJOFFSETS[i];
    if(!ISE(adj) || (oppMap & Board::RADIUS[1][adj]).isEmpty())
      continue;
    piece_t reqStr = b.strongestAdjacentUFPla(opp,adj);
    num += BoardTrees::genStepGeqTo1S(b,mv+num,adj,ERRLOC,reqStr);
  }
  return num;
}

static const int DY[2] =
{S,N};

#define RABRECURSE(NEWRLOC,NS,MOVE) num += genRabbitWalks(b,pla,NEWRLOC,rloc,numStepsLeft-(NS),mv+num,concatMoves(moveSoFar,MOVE,numStepsSoFar),numStepsSoFar+NS,targetLocs);

static int genRabbitWalks(Board& b, pla_t pla, loc_t rloc, loc_t prev, int numStepsLeft, move_t* mv,
    move_t moveSoFar, int numStepsSoFar, Bitmap targetLocs)
{
  int num = 0;
  if(numStepsLeft <= 0)
  {
    if(targetLocs.isOne(rloc))
      mv[num++] = moveSoFar;
    return num;
  }
  int dy = DY[pla];
  //Frozen rabbit?
  if(b.isFrozenC(rloc))
  {
    if(moveSoFar != ERRMOVE && targetLocs.isOne(rloc))
      mv[num++] = moveSoFar;

    if(numStepsLeft <= 1)
    {
      //Stuff a piece behind the rabbit if possible
      if(ISE(rloc F) && targetLocs.isOne(rloc))
      {
        if(ISP(rloc FW) && b.isThawedC(rloc FW)) mv[num++] = concatMoves(moveSoFar,gMove(rloc FW ME),numStepsSoFar);
        if(ISP(rloc FE) && b.isThawedC(rloc FE)) mv[num++] = concatMoves(moveSoFar,gMove(rloc FE MW),numStepsSoFar);
        if(ISP(rloc FF) && b.isThawedC(rloc FF)) mv[num++] = concatMoves(moveSoFar,gMove(rloc FF MG),numStepsSoFar);
      }
      return num;
    }

    //Unfreeze it and recurse
    loc_t loc = rloc F;
    if(ISE(loc))
    {
      if(ISP(loc F) && b.isThawedC(loc F)) {TSC(loc F,loc,RABRECURSE(rloc,1,getMove(loc F MG)))}
      if(ISP(loc W) && b.isThawedC(loc W)) {TSC(loc W,loc,RABRECURSE(rloc,1,getMove(loc W ME)))}
      if(ISP(loc E) && b.isThawedC(loc E)) {TSC(loc E,loc,RABRECURSE(rloc,1,getMove(loc E MW)))}
    }
    loc = rloc W;
    if(ISE(loc))
    {
      if(ISP(loc F) && b.isThawedC(loc F))                       {TSC(loc F,loc,RABRECURSE(rloc,1,getMove(loc F MG)))}
      if(ISP(loc W) && b.isThawedC(loc W))                       {TSC(loc W,loc,RABRECURSE(rloc,1,getMove(loc W ME)))}
      if(ISP(loc G) && b.isThawedC(loc G) && !b.isRabbit(loc G)) {TSC(loc G,loc,RABRECURSE(rloc,1,getMove(loc G MF)))}
    }
    loc = rloc E;
    if(ISE(loc))
    {
      if(ISP(loc F) && b.isThawedC(loc F))                       {TSC(loc F,loc,RABRECURSE(rloc,1,getMove(loc F MG)))}
      if(ISP(loc E) && b.isThawedC(loc E))                       {TSC(loc E,loc,RABRECURSE(rloc,1,getMove(loc E MW)))}
      if(ISP(loc G) && b.isThawedC(loc G) && !b.isRabbit(loc G)) {TSC(loc G,loc,RABRECURSE(rloc,1,getMove(loc G MF)))}
    }
    return num;
  }

  //Always step forward if possible
  if(ISE(rloc G) && b.isTrapSafe2(pla,rloc G))
  {
    //Save progress if it would be dominated or we've made many steps, else force a forward step
    if(moveSoFar != ERRMOVE && targetLocs.isOne(rloc) && (b.wouldBeDom(pla,rloc,rloc G) || numStepsSoFar >= 3))
      mv[num++] = moveSoFar;
    TSC(rloc,rloc G, RABRECURSE(rloc G,1,gMove(rloc MG)));
  }
  else
  {
    //Hit a stopping point - save progress
    if(moveSoFar != ERRMOVE && targetLocs.isOne(rloc))
      mv[num++] = moveSoFar;
    //Recurse sideways
    if(rloc W != prev && ISE(rloc W) && b.isTrapSafe2(pla,rloc W))
    {TSC(rloc,rloc W, RABRECURSE(rloc W,1,gMove(rloc MW)));}
    if(rloc E != prev && ISE(rloc E) && b.isTrapSafe2(pla,rloc E))
    {TSC(rloc,rloc E, RABRECURSE(rloc E,1,gMove(rloc ME)));}
  }

  return num;
}

int BoardTrees::genGoalAttacks(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  Bitmap goalDistIs[9];
  Threats::getGoalDistBitmapEst(b,pla,goalDistIs);

  pla_t opp = gOpp(pla);

  //Things next to a very advanced rabbit, and things next to the space in front of a very advanced rabbit
  Bitmap veryAdvancedRabMap = b.pieceMaps[pla][RAB] & goalDistIs[4];
  Bitmap inFrontOfVeryAdvancedRabMap = Bitmap::shiftG(veryAdvancedRabMap,pla);
  Bitmap genStepsInvolvingMap = Bitmap::adj(veryAdvancedRabMap | inFrontOfVeryAdvancedRabMap);

  //Opp pieces that are very low goal dist that are next to the space in front of a rabbit, with looser tolerance on distance
  Bitmap advancedRabMap = b.pieceMaps[pla][RAB] & goalDistIs[7];
  Bitmap inFrontOfAdvancedRabMap = Bitmap::shiftG(advancedRabMap,pla) & (~b.pieceMaps[opp][0] | goalDistIs[3]);
  genStepsInvolvingMap |= goalDistIs[1] & b.pieceMaps[opp][0] & b.dominatedMap
      & (inFrontOfAdvancedRabMap | Bitmap::adj(inFrontOfAdvancedRabMap));

  //Generate pushpulls and steps of things near rabbits very close to goal
  int num = 0;
  if(numSteps >= 2)
    num += BoardMoveGen::genPushPullsInvolving(b,pla,mv+num,genStepsInvolvingMap);
  num += BoardMoveGen::genStepsInvolving(b,pla,mv+num,genStepsInvolvingMap);

  //Generate advances of rabbits that aren't too far away from goal forward
  Bitmap rabMap = b.pieceMaps[pla][RAB] & goalDistIs[numSteps + 4];
  while(rabMap.hasBits())
  {
    loc_t rloc = rabMap.nextBit();
    num += genRabbitWalks(b,pla,rloc,ERRLOC,numSteps,mv+num,ERRMOVE,0,goalDistIs[5]);
  }

  return num;
}

int BoardTrees::genFreezeAttacks(Board& b, pla_t pla, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv)
{
  int num = 0;
  //Identify pieces that are within radius 2 of traps that have at most 1 opp defender
  //and that are not guarded and not already dominated, and where we have at least one defender also
  pla_t opp = gOpp(pla);
  Bitmap opps = b.pieceMaps[opp][0];
  Bitmap adjOpps = Bitmap::adj(opps);

  Bitmap rad2WeakOppTraps;
  if(b.trapGuardCounts[opp][0] <= 1 && b.trapGuardCounts[pla][0] >= 1) rad2WeakOppTraps |= Board::DISK[2][TRAP0];
  if(b.trapGuardCounts[opp][1] <= 1 && b.trapGuardCounts[pla][1] >= 1) rad2WeakOppTraps |= Board::DISK[2][TRAP1];
  if(b.trapGuardCounts[opp][2] <= 1 && b.trapGuardCounts[pla][2] >= 1) rad2WeakOppTraps |= Board::DISK[2][TRAP2];
  if(b.trapGuardCounts[opp][3] <= 1 && b.trapGuardCounts[pla][3] >= 1) rad2WeakOppTraps |= Board::DISK[2][TRAP3];

  Bitmap attackableOpps = rad2WeakOppTraps & opps & ~adjOpps & ~b.dominatedMap;
  while(attackableOpps.hasBits())
  {
    loc_t oloc = attackableOpps.nextBit();
    int maxRad = numSteps+1;
    for(int rad = 2; rad <= maxRad; rad++)
    {
      Bitmap attackers = pStrongerMaps[opp][b.pieces[oloc]] &
          ((Board::RADIUS[rad-1][oloc] & b.frozenMap) | (Board::RADIUS[rad][oloc] & ~b.frozenMap));
      if(attackers.isEmpty())
        continue;
      //Only look for things a single step further out than the minimum found
      if(maxRad > rad+1)
        maxRad = rad+1;

      int startSteps = rad > numSteps ? numSteps : rad;
      while(attackers.hasBits())
      {
        loc_t ploc = attackers.nextBit();
        Bitmap simpleWalkLocs;
        //Take the shortest attacks with one step of leeway at the start
        for(int i = startSteps; i <= numSteps; i++)
        {
          int newNum = genDefsUsing(b,pla,ploc,oloc,i,mv+num,simpleWalkLocs);
          if(newNum > 0)
          {num += newNum; break;}
        }
      }
    }
  }

  return num;
}

//Generates distant (more than 1 space away) walks of own pieces to empty opponent trap defense squares
//Does NOT consider elephant walks
int BoardTrees::genDistantTrapAttacks(Board& b, pla_t pla, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv)
{
  DEBUGASSERT(numSteps >= 2);

  //For each opponent trap, if the opp has only 1 defender next to it, try adding an attacker if the added
  //attacker would not be dominated by any nearby nonele piece.
  pla_t opp = gOpp(pla);
  int num = 0;
  for(int i = 0; i<2; i++)
  {
    int trapIndex = Board::PLATRAPINDICES[opp][i];
    int numOppGuards = b.trapGuardCounts[opp][trapIndex];
    if(numOppGuards <= 2)
    {
      loc_t kt = Board::TRAPLOCS[trapIndex];
      for(int j = 0; j<4; j++)
      {
        loc_t adj = kt + Board::ADJOFFSETS[j];
        //Don't generate for the inside squares
        //TODO also don't generate for the top squares unless one or more defenders?
        if(gX(adj) == 3 || gX(adj) == 4)
          continue;
        if(numOppGuards >= 2 && (gY(adj) == 3 || gY(adj) == 4))
          continue;
        if(!ISE(adj))
          continue;
        //Find the weakest piece that we'd want to add as a defender
        //If trap has at least one defender of our own, then require no non-elephant dominators in rad 3
        //If trap does not, require no dominators at all in rad 3
        piece_t piece = ELE;
        Bitmap disk3 = Board::DISK[3][adj];
        if(b.trapGuardCounts[pla][trapIndex] > 0)
        {
          Bitmap oppEleMap = b.pieceMaps[opp][ELE];
          while(piece > RAB && (pStrongerMaps[pla][piece-1] & disk3 & ~oppEleMap).isEmpty())
            piece--;
        }
        else
        {
          while(piece > RAB && (pStrongerMaps[pla][piece-1] & disk3).isEmpty())
            piece--;
        }
        if(piece >= ELE)
          continue;

        //Scan out and try to reach this location from distant locations with nonele pieces
        Bitmap map = pStrongerMaps[opp][piece-1] & ~b.pieceMaps[pla][ELE] & Board::DISK[numSteps][adj] & ~Board::DISK[1][adj];
        while(map.hasBits())
        {
          loc_t ploc = map.nextBit();
          if(!b.isRabOkay(pla,ploc,adj))
            continue;
          Bitmap simpleWalkLocs;
          num += genWalkTosUsing(b,pla,ploc,adj,numSteps,mv+num,simpleWalkLocs);
        }
      }
    }
  }
  return num;
}

//TODO can we use this function elsewhere profitably?
static bool isElephantBusy(Board& b, pla_t pla, loc_t ploc)
{
  int trapIndex = Board::ADJACENTTRAPINDEX[ploc];
  if(trapIndex == ERRLOC)
    return false;
  loc_t kt = Board::TRAPLOCS[trapIndex];

  //Frame
  if(ISP(kt) && b.trapGuardCounts[pla][trapIndex] <= 1)
    return true;

  //Transient tactical threat if elephant leaves, in the common case of the other opp ele there.
  pla_t opp = gOpp(pla);
  if(ISE(kt) && b.trapGuardCounts[opp][trapIndex] >= 2 && b.trapGuardCounts[pla][trapIndex] == 2
      && (Board::RADIUS[1][kt] & b.pieceMaps[opp][ELE]).hasBits())
    return true;

  //Hostage or similar tactical threat if elephant leaves
  if(ISE(kt) && b.trapGuardCounts[opp][trapIndex] >= 2 && b.trapGuardCounts[pla][trapIndex] <= 1
      && (Board::RADIUS[2][kt] & b.pieceMaps[pla][0] & b.dominatedMap &
          (b.frozenMap | ~Bitmap::adj(~(b.pieceMaps[SILV][0] | b.pieceMaps[GOLD][0])))).hasBits())
    return true;

  return false;
}

//Companion to genDistantTrapAttacks
//Generates distant (more than 1 space away) walks of own pieces to empty opponent's side of the board
//when conditions are promising.
//Does NOT consider elephant walks
int BoardTrees::genDistantAdvances(Board& b, pla_t pla, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES], move_t* mv)
{
  if(numSteps <= 1)
    return 0;
  pla_t opp = gOpp(pla);
  int num = 0;

  //Attack a certain set of target squares on the opp side of the board.
  //Excludes most trap defense squares to limit overlap with other functions
  //+--------+
  //|........|
  //|.x.xx.x.|
  //|x.xxxx.x|
  //|.x....x.|
  Bitmap targetBase = pla == GOLD ? Bitmap(0x005ABD4200000000ULL) : Bitmap(0x0000000042BD5A00ULL);

  //Don't generate attacks using pieces in this region - they're already advanced
  //+--------+
  //|........|
  //|.xxxxxx.|
  //|xxxxxxxx|
  //|.x....x.|
  Bitmap excludeBase = pla == GOLD ? Bitmap(0x007EFF4200000000ULL) : Bitmap(0x0000000042FF7E00ULL);

  //Attack squares that are very far from the opp elephant
  //Attack squares that are not quite as far from the opp elephant if the opp elephant is busy
  loc_t oppEleLoc = b.findElephant(opp);
  Bitmap farFromOppEle;
  if(oppEleLoc == ERRLOC)                  farFromOppEle = Bitmap::BMPONES;
  else if(isElephantBusy(b,opp,oppEleLoc)) farFromOppEle = ~Board::DISK[3][oppEleLoc];
  else                                     farFromOppEle = ~Board::DISK[5][oppEleLoc];

  //Lastly, require adjacency to an opp or a pushpull of an opp
  Bitmap targets = targetBase & farFromOppEle & (b.pieceMaps[opp][0] | Bitmap::adj(b.pieceMaps[opp][0]));
  while(targets.hasBits())
  {
    loc_t loc = targets.nextBit();
    //Require strength enough to dominate at least one opp piece and not be scared of anything in radius 3
    Bitmap disk3 = Board::DISK[3][loc];
    Bitmap disk1 = Board::DISK[1][loc];
    Bitmap oppMap = b.pieceMaps[opp][0];
    piece_t piece = ELE;
    while(piece > CAT && (pStrongerMaps[pla][piece-1] & disk3).isEmpty() && (oppMap & ~pStrongerMaps[pla][piece-2] & disk1).hasBits())
      piece--;
    if(piece >= ELE)
      continue;

    //Scan out and try to reach this location from distant locations with nonele pieces with radius at least 2
    Bitmap potentialPlas = pStrongerMaps[opp][piece-1] & ~b.pieceMaps[pla][ELE] & ~Board::DISK[1][loc] & ~excludeBase;
    potentialPlas &= Board::DISK[numSteps][loc];

    while(potentialPlas.hasBits())
    {
      loc_t ploc = potentialPlas.nextBit();
      Bitmap simpleWalkLocs;
      num += genWalkTosUsing(b,pla,ploc,loc,numSteps,mv+num,simpleWalkLocs);
    }
  }

  //Attack near traps if we have a framed piece
  Bitmap oppDomTargets;
  for(int i = 0; i<4; i++)
  {
    loc_t kt = Board::TRAPLOCS[i];
    if(!(ISP(kt) && b.trapGuardCounts[pla][i] == 1 && b.trapGuardCounts[opp][i] == 3))
      continue;
    bool notFrame = false;
    Bitmap holders;
    for(int j = 0; j<4; j++)
    {
      loc_t adj = kt + Board::ADJOFFSETS[j];
      if(ISP(adj))
        continue;
      if(GT(kt,adj) && b.isOpen(adj))
      {notFrame = true; break;}
      if(GT(kt,adj))
        holders |= Board::DISK[1][adj];
      else
        holders.setOn(adj);
    }
    if(notFrame)
      continue;
    oppDomTargets |= holders;
  }
  oppDomTargets &= b.pieceMaps[opp][0];

  //Attack defenders of traps that we have contested on the opponent's side
  for(int i = 0; i<4; i++)
  {
    loc_t kt = Board::TRAPLOCS[i];
    if(b.trapGuardCounts[pla][i] < 2 || b.trapGuardCounts[opp][i] < 2)
      continue;
    oppDomTargets |= Board::DISK[1][kt] & b.pieceMaps[opp][0] & ~b.pieceMaps[opp][ELE] & ~b.dominatedMap;
  }

  targets = Bitmap::adj(oppDomTargets);
  while(targets.hasBits())
  {
    loc_t loc = targets.nextBit();
    //Require strength enough to dominate at least one opp piece in the targets
    Bitmap disk1 = Board::DISK[1][loc];
    piece_t piece = ELE;
    while(piece > CAT && (oppDomTargets & ~pStrongerMaps[pla][piece-2] & disk1).hasBits())
      piece--;
    if(piece >= ELE)
      continue;

    //Scan out and try to reach this location from distant locations with nonele pieces with radius at least 2
    Bitmap potentialPlas = pStrongerMaps[opp][piece-1] & ~b.pieceMaps[pla][ELE] & ~Board::DISK[1][loc];
    potentialPlas &= Board::DISK[numSteps][loc];

    while(potentialPlas.hasBits())
    {
      loc_t ploc = potentialPlas.nextBit();
      Bitmap simpleWalkLocs;
      num += genWalkTosUsing(b,pla,ploc,loc,numSteps,mv+num,simpleWalkLocs);
    }
  }

  return num;
}

//TODO Consider generating trap fight swarms in more cases
int BoardTrees::genDistantTrapFightSwarms(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  if(numSteps < 2)
    return 0;
  pla_t opp = gOpp(pla);
  loc_t plaEleLoc = b.findElephant(pla);
  if(plaEleLoc == ERRLOC)
    return 0;
  int trapIndex = -1;
  //Elephant must be on one of the center two attack squares
  if(pla == GOLD && plaEleLoc == gLoc(3,5)) {trapIndex = 2;}
  else if(pla == GOLD && plaEleLoc == gLoc(4,5)) {trapIndex = 3;}
  else if(pla == SILV && plaEleLoc == gLoc(3,2)) {trapIndex = 0;}
  else if(pla == SILV && plaEleLoc == gLoc(4,2)) {trapIndex = 1;}
  else return 0;

  if(b.trapGuardCounts[pla][trapIndex] < 2)
    return 0;

  //Require either a hostage situation or the opp elephant fighting for the trap
  loc_t kt = Board::TRAPLOCS[trapIndex];
  loc_t outsideLoc = Board::TRAPOUTSIDELOCS[trapIndex];
  if((b.pieceMaps[opp][ELE] & Board::DISK[1][kt]).isEmpty()
     && !(ISO(outsideLoc) && ISP(2*outsideLoc-kt) && GT(outsideLoc,2*outsideLoc-kt)))
    return 0;

  //Generate some swarmy steps by distant pieces
  int num = 0;

  //Swarm to next to elephant from locations with at least radius
  Bitmap swarmers = b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB] & ~b.pieceMaps[pla][CAT] &
      Board::DISK[numSteps+1][plaEleLoc] & ~Board::DISK[2][plaEleLoc];
  while(swarmers.hasBits())
  {
    loc_t ploc = swarmers.nextBit();
    Bitmap simpleWalkLocs;
    num += genDefsUsing(b,pla,ploc,plaEleLoc,numSteps,mv+num,simpleWalkLocs);
  }

  //Swarm next to outsideLoc from at least radius 2, with a distance cutoff if there are nearby pieces.
  if(ISP(outsideLoc))
  {
    swarmers = b.pieceMaps[pla][0] & ~Board::DISK[2][outsideLoc];
    if((swarmers & Board::DISK[3][outsideLoc]).hasBits())
      swarmers &= Board::DISK[numSteps][outsideLoc];
    else
      swarmers &= Board::DISK[numSteps+1][outsideLoc];

    while(swarmers.hasBits())
    {
      loc_t ploc = swarmers.nextBit();
      Bitmap simpleWalkLocs;
      num += genDefsUsing(b,pla,ploc,outsideLoc,numSteps,mv+num,simpleWalkLocs);
    }
  }
  return num;
}

//TODO Currently we don't support 2-step phalanxes
//Generates phalanxes at [loc] against (assumed adjacent) location [from]
//[from] should NOT be ERRLOC.
int BoardTrees::genPhalanxesIfNeeded(Board& b, pla_t pla, int numSteps, loc_t loc, loc_t from, move_t* mv)
{
  if(ISE(loc))
  {
    if(numSteps <= 1 || b.isOpen(loc,from))
      return 0;
    return genStuffings2S(b,mv,pla,loc);
  }

  int numOpen = b.countOpen(loc) - (ISE(from) ? 1 : 0);
  if(numOpen != 1)
    return 0;

  loc_t openLoc = b.findOpenIgnoring(loc,from);

  //First try 1-step phalanxes
  int num = BoardTrees::genStepGeqTo1S(b,mv,openLoc,loc,EMP);
  bool anyOneStepPhalanxes = num > 0;

  //Try longer phalanxes
  if(numSteps >= 2)
  {
    //Cap the distance we consider
    if(numSteps > 3)
      numSteps = 3;

    int maxRadius = anyOneStepPhalanxes ? 2 : numSteps;
    Bitmap map = b.pieceMaps[pla][0] & Board::DISK[maxRadius][openLoc] & ~Board::DISK[1][loc] & ~(Board::DISK[1][openLoc] & ~b.frozenMap);
    while(map.hasBits())
    {
      loc_t ploc = map.nextBit();
      Bitmap simpleWalkLocs;
      if(b.isRabOkay(pla,ploc,openLoc))
        num += genWalkTosUsing(b,pla,ploc,openLoc,numSteps,mv+num,simpleWalkLocs);
    }
  }

  return num;
}

int BoardTrees::genFramePhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  for(int i = 0; i<4; i++)
  {
    loc_t kt = Board::TRAPLOCS[i];
    if(ISO(kt) && !b.isRabbit(kt) && b.trapGuardCounts[pla][i] >= 2)
    {
      for(int j = 0; j<4; j++)
      {
        loc_t adj = kt + Board::ADJOFFSETS[j];
        if(ISP(adj) && GT(kt,adj))
          num += genPhalanxesIfNeeded(b,pla,numSteps,adj,kt,mv+num);
      }
    }
  }
  return num;
}

int BoardTrees::genTrapBackPhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  int num = 0;
  pla_t opp = gOpp(pla);
  //Phalanxing the backs of traps to prevent push through
  for(int i = 0; i<2; i++)
  {
    int trapIndex = Board::PLATRAPINDICES[pla][i];
    loc_t kt = Board::TRAPLOCS[trapIndex];
    loc_t backLoc = Board::TRAPBACKLOCS[trapIndex];
    if(ISE(kt) && b.trapGuardCounts[pla][trapIndex] == 2 && b.trapGuardCounts[opp][trapIndex] >= 1 && ISP(backLoc))
    {
      //We only defend against the weakest opp pushing through
      piece_t weakestOpp = ELE;
      if(ISO(kt S) && LTP(kt S,weakestOpp)) weakestOpp = b.pieces[kt S];
      if(ISO(kt W) && LTP(kt W,weakestOpp)) weakestOpp = b.pieces[kt W];
      if(ISO(kt E) && LTP(kt E,weakestOpp)) weakestOpp = b.pieces[kt E];
      if(ISO(kt N) && LTP(kt N,weakestOpp)) weakestOpp = b.pieces[kt N];
      if(b.trapGuardCounts[opp][trapIndex] == 1)
      {
        //Find the strongest piece able to push through
        loc_t open = b.findOpen(kt);
        piece_t pusher = b.strongestAdjacentPla(opp,open);
        if(pusher < weakestOpp)
          weakestOpp = pusher;
      }
      if(weakestOpp <= b.pieces[backLoc])
        continue;
      num += genPhalanxesIfNeeded(b,pla,numSteps,backLoc,kt,mv+num);
    }
  }
  return num;
}

static const int DX[2] =
{W,E};

int BoardTrees::genBackRowMiscPhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  int num = 0;
  int dy = DY[pla];
  pla_t opp = gOpp(pla);

  for(int xSide = 0; xSide<2; xSide++)
  {
    int trapIndex = Board::PLATRAPINDICES[pla][xSide];
    int dx = DX[xSide];
    loc_t kt = Board::TRAPLOCS[trapIndex];
    loc_t backLoc = kt F;

    //A2 phalanx to hold in a hostage
    loc_t a2Loc = backLoc + dx + dx;
    loc_t a3Loc = a2Loc G;
    if(ISP(a2Loc) && ISO(a3Loc) && b.isDominated(a3Loc))
      num += genPhalanxesIfNeeded(b,pla,numSteps,a2Loc,a3Loc,mv+num);

    //B2 phalanx to hold on to trap control
    loc_t b2Loc = backLoc + dx;
    loc_t b3Loc = b2Loc G;
    if(ISP(b2Loc) && ISO(b3Loc) && GT(b3Loc,b2Loc) && ISP(backLoc))
      num += genPhalanxesIfNeeded(b,pla,numSteps,b2Loc,b3Loc,mv+num);

    //D2 phalanx to threaten a piece
    loc_t d2Loc = backLoc - dx;
    loc_t d3Loc = d2Loc G;
    if(ISP(d2Loc) && ISO(d3Loc) && GT(d3Loc,d2Loc) && ISP(backLoc))
    {
      //TODO maybe (b.pieceMaps[opp][0] & Board::DISK[2][kt]).isEmptyOrSingleBit() as a condition to catch
      //the case where you blockade the elephant from d2

      //Build blockade against threatened piece
      if(b.isDominated(d3Loc))
        num += genPhalanxesIfNeeded(b,pla,numSteps,d2Loc,d3Loc,mv+num);
    }
  }

  return num;
}

//TODO add phalanxes where we pushpull an opp rabbit to block things?
int BoardTrees::genOppElePhalanxes(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  int num = 0;
  pla_t opp = gOpp(pla);

  //Generate any phalanxes of the opponent's elephant in any central direction if opp ele is off-center
  loc_t oppEleLoc = b.findElephant(opp);
  int oppEleX = gX(oppEleLoc);
  int oppEleY = gY(oppEleLoc);
  Bitmap emptyMap = ~(b.pieceMaps[SILV][0] | b.pieceMaps[GOLD][0]);
  if(oppEleY <= 2 && (Board::DISK[1][oppEleLoc N] & emptyMap).isEmptyOrSingleBit())
    num += genPhalanxesIfNeeded(b,pla,numSteps,oppEleLoc N,oppEleLoc,mv+num);
  if(oppEleY >= 5 && (Board::DISK[1][oppEleLoc S] & emptyMap).isEmptyOrSingleBit())
    num += genPhalanxesIfNeeded(b,pla,numSteps,oppEleLoc S,oppEleLoc,mv+num);
  if(oppEleX <= 2 && (Board::DISK[1][oppEleLoc E] & emptyMap).isEmptyOrSingleBit())
    num += genPhalanxesIfNeeded(b,pla,numSteps,oppEleLoc E,oppEleLoc,mv+num);
  if(oppEleX >= 5 && (Board::DISK[1][oppEleLoc W] & emptyMap).isEmptyOrSingleBit())
    num += genPhalanxesIfNeeded(b,pla,numSteps,oppEleLoc W,oppEleLoc,mv+num);

  return num;
}
//Generates all steps and pushpulls and unblocks that move a pla nonrab piece from ploc to dest as long as doing so
//does not sacrifice the piece, in 2 steps, assuming that it is not dominated.
//NOTE: This should remain safe to operate on a temp-stepped board
//NOTE: If this ever changes to be capable of producing more than 8 moves, need to edit buf size in
//genUsefulElephantStepPushPullsRec below.
static int genUndomPieceMovesTo2S(Board& b, pla_t pla, loc_t ploc, loc_t dest, step_t pStep, move_t* mv)
{
  int num = 0;
  if(ISE(dest))
  {
    if(!b.isTrapSafe2(pla,dest))
      return 0;
    //Pull as we step
    pla_t opp = gOpp(pla);
    if(ploc S != dest && ISO(ploc S) && GT(ploc,ploc S)) mv[num++] = getMove(pStep,ploc S MN);
    if(ploc W != dest && ISO(ploc W) && GT(ploc,ploc W)) mv[num++] = getMove(pStep,ploc W ME);
    if(ploc E != dest && ISO(ploc E) && GT(ploc,ploc E)) mv[num++] = getMove(pStep,ploc E MW);
    if(ploc N != dest && ISO(ploc N) && GT(ploc,ploc N)) mv[num++] = getMove(pStep,ploc N MS);
  }
  //Player in the way - only step if all safe and we're not saccing the piece.
  else if(ISP(dest))
  {
    if(!b.isTrapSafe2(pla,ploc))
      return 0;
    if(ISE(dest S) && b.isTrapSafe2(pla,dest S) && b.isRabOkayS(pla,dest)) mv[num++] = getMove(dest MS,pStep);
    if(ISE(dest W) && b.isTrapSafe2(pla,dest W))                           mv[num++] = getMove(dest MW,pStep);
    if(ISE(dest E) && b.isTrapSafe2(pla,dest E))                           mv[num++] = getMove(dest ME,pStep);
    if(ISE(dest N) && b.isTrapSafe2(pla,dest N) && b.isRabOkayN(pla,dest)) mv[num++] = getMove(dest MN,pStep);
  }
  //Opp in the way, push
  else if(GT(ploc,dest))
  {
    if(!b.isTrapSafe2(pla,dest))
      return 0;
    if(ISE(dest S)) mv[num++] = getMove(dest MS,pStep);
    if(ISE(dest W)) mv[num++] = getMove(dest MW,pStep);
    if(ISE(dest E)) mv[num++] = getMove(dest ME,pStep);
    if(ISE(dest N)) mv[num++] = getMove(dest MN,pStep);
  }
  return num;
}

//0 = no
//1 = always
//2 = if the opp ele is adjacent to you and more central
//OR there is an opp rab in front of that and you have at least 2 steps left after
//OR the nearest trap has at least 2 other of your pieces within radius 2
static const int ELEFORWARDMODE[BSIZE] =
{
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,2,0,2,2,0,2,0,  0,0,0,0,0,0,0,0,
2,2,1,1,1,1,2,2,  0,0,0,0,0,0,0,0,
2,1,1,1,1,1,1,2,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
};

//0 = no
//1 = always
//2 = if there is any opp piece to the right of your ele and at most one square forward from it,
//OR there is any pla piece to the right of your ele strictly forward from it.
//NOTE - if modifying this, note that it's specialized to assuming that 2s don't appear at the top of the board.
//3 = if there is any opp piece to the right of your ele and at most one square forward from it,
//OR the square in front of the ele is phalanxed or blocked by opp ele
static const int ELEEASTMODE[BSIZE] =
{
1,1,1,1,0,0,0,0,  0,0,0,0,0,0,0,0,
1,1,1,1,0,0,0,0,  0,0,0,0,0,0,0,0,
1,1,1,1,1,0,0,0,  0,0,0,0,0,0,0,0,
1,1,1,1,1,2,0,0,  0,0,0,0,0,0,0,0,
1,1,1,1,1,2,0,0,  0,0,0,0,0,0,0,0,
1,1,1,3,3,3,0,0,  0,0,0,0,0,0,0,0,
1,3,1,3,3,3,0,0,  0,0,0,0,0,0,0,0,
1,3,3,3,3,3,0,0,  0,0,0,0,0,0,0,0,
};

//0 = no
//1 = always
//2 = if adjacent to an opp ele more central than us
//OR an opp is below within 2 spaces to the left or right excluding the square directly beneath
//3 = if adjacent to an opp ele more central than us
//NOTE - if modifying this, note that it's specialized to assuming no 2 is on the left right or bottom edge of the board.
static const int ELEBACKMODE[BSIZE] =
{
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
3,2,2,2,2,2,2,3,  0,0,0,0,0,0,0,0,
3,2,2,2,2,2,2,3,  0,0,0,0,0,0,0,0,
0,2,2,2,2,2,2,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
};

static int genUsefulElephantStepPushPullsRec(Board& b, pla_t pla, loc_t eloc, loc_t origLoc, loc_t oppEleLoc, int numStepsLeft,
    int chainThreshold, move_t* mv);
static int gUESPPRecurseOnMove1S(Board& b, pla_t pla, loc_t oldEloc, loc_t newEloc, step_t step, loc_t origLoc, loc_t oppEleLoc,
    int numStepsLeft, int chainThreshold, move_t* mv)
{
  int num = 0;
  TSC(oldEloc,newEloc, num = genUsefulElephantStepPushPullsRec(b,pla,newEloc,origLoc,oppEleLoc,numStepsLeft-1,chainThreshold-1,mv));
  for(int i = 0; i<num; i++)
    mv[i] = preConcatMove(step,mv[i]);
  return num;
}
static int gUESPPRecurseOnMove2S(Board& b, pla_t pla, loc_t newEloc, move_t move, loc_t origLoc, loc_t oppEleLoc,
    int numStepsLeft, int chainThreshold, move_t* mv)
{
  step_t step0 = getStep(move,0);
  loc_t src0 = gSrc(step0);
  loc_t dest0 = gDest(step0);
  step_t step1 = getStep(move,1);
  loc_t src1 = gSrc(step1);
  loc_t dest1 = gDest(step1);
  int num = 0;
  TSC(src0,dest0,TSC2(src1,dest1,num = genUsefulElephantStepPushPullsRec(b,pla,newEloc,origLoc,oppEleLoc,numStepsLeft-2,chainThreshold-2,mv)));
  for(int i = 0; i<num; i++)
    mv[i] = concatMoves(move,mv[i],2);
  return num;
}

static int gUESPPRecurseInDir(Board& b, pla_t pla, loc_t eloc, loc_t dest, step_t step, loc_t origLoc, loc_t oppEleLoc, int numStepsLeft, int chainThreshold, move_t* mv)
{
  int num = 0;

  if(ISE(dest) && b.isTrapSafe2(pla,dest) && b.manhattanDist(origLoc,dest) > b.manhattanDist(origLoc,eloc))
  {
    if(chainThreshold <= 1 || numStepsLeft <= 1)
      mv[num++] = getMove(step);
    if(chainThreshold >= 1 && numStepsLeft > 1)
      num += gUESPPRecurseOnMove1S(b,pla,eloc,dest,step,origLoc,oppEleLoc,numStepsLeft,chainThreshold,mv+num);
  }

  if(numStepsLeft >= 2)
  {
    if(chainThreshold <= 2 || numStepsLeft <= 2)
      num += genUndomPieceMovesTo2S(b,pla,eloc,dest,step,mv+num);
    if(chainThreshold >= 2 && numStepsLeft > 2)
    {
      int subNum = 0;
      move_t subMv[8];
      subNum += genUndomPieceMovesTo2S(b,pla,eloc,dest,step,subMv);
      for(int i = 0; i<subNum; i++)
        num += gUESPPRecurseOnMove2S(b,pla,dest,subMv[i],origLoc,oppEleLoc,numStepsLeft,chainThreshold,mv+num);
    }
  }
  return num;
}

//Generates moves with a length of at least chainThreshold, chaining on more steps if <= chainThreshold.
//NOTE: This should be safe to operate on a tempstepped board.
//HOWEVER, we do use bitmaps, but only in an indicative way they might be stale from prior to a tempstep, and that's okay.
//ADDITIONALLY oppEleLoc might be stale prior to a tempstep, that's intentional
static int genUsefulElephantStepPushPullsRec(Board& b, pla_t pla, loc_t eloc, loc_t origLoc, loc_t oppEleLoc, int numStepsLeft,
    int chainThreshold, move_t* mv)
{
  int num = 0;
  int dy = Board::GOALLOCINC[pla];
  pla_t opp = gOpp(pla);
  Bitmap pEleMap = b.pieceMaps[pla][ELE];
  Bitmap pNonEleMap = b.pieceMaps[pla][0] & ~pEleMap;
  Bitmap oppMap = b.pieceMaps[opp][0];
  bool oppEleAdjMoreCentral = false;
  if(oppEleLoc != ERRLOC && b.isAdjacent(eloc,oppEleLoc) && Board::CENTERDIST[oppEleLoc] < Board::CENTERDIST[eloc])
    oppEleAdjMoreCentral = true;

  //Elephant stepping forward?
  int forwardMode = ELEFORWARDMODE[pla == GOLD ? gRevLoc(eloc) : eloc];
  if(forwardMode == 1 ||
     (forwardMode == 2 && (
         oppEleAdjMoreCentral ||
         (ISO(eloc GG) && b.isRabbit(eloc GG) && (numStepsLeft >= 3 + !ISE(eloc G))) ||
         !(Board::DISK[2][Board::CLOSEST_TRAP[eloc]] & pNonEleMap).isEmptyOrSingleBit()
  )))
  {
    num += gUESPPRecurseInDir(b,pla,eloc,eloc G,eloc MG,origLoc,oppEleLoc,numStepsLeft,chainThreshold,mv+num);
  }
  int eX = gX(eloc);
  int eY = gY(eloc);
  int westMode = ELEEASTMODE[gLoc(7-eX,pla == GOLD ? 7-eY : eY)];
  int eastMode = ELEEASTMODE[gLoc(eX,pla == GOLD ? 7-eY : eY)];

  if(westMode == 1 ||
     (westMode == 2 && (Board::XINTERVAL[eX+1][0] & Board::YINTERVAL[gY(eloc G)][Board::GOALY[pla]] & pNonEleMap).hasBits()) ||
     ((westMode == 2 || westMode == 3) && (Board::XINTERVAL[eX+1][0] & Board::YINTERVAL[gY(eloc G)][Board::STARTY[pla]] & oppMap).hasBits()) ||
     (westMode == 3 && (b.pieces[eloc G] == ELE || b.isBlocked(eloc G)))
  )
  {
    num += gUESPPRecurseInDir(b,pla,eloc,eloc W,eloc MW,origLoc,oppEleLoc,numStepsLeft,chainThreshold,mv+num);
  }

  if(eastMode == 1 ||
     (eastMode == 2 && (Board::XINTERVAL[eX+1][7] & Board::YINTERVAL[gY(eloc G)][Board::GOALY[pla]] & pNonEleMap).hasBits()) ||
     ((eastMode == 2 || eastMode == 3) && (Board::XINTERVAL[eX+1][7] & Board::YINTERVAL[gY(eloc G)][Board::STARTY[pla]] & oppMap).hasBits()) ||
     (eastMode == 3 && (b.pieces[eloc G] == ELE || b.isBlocked(eloc G)))
  )
  {
    num += gUESPPRecurseInDir(b,pla,eloc,eloc E,eloc ME,origLoc,oppEleLoc,numStepsLeft,chainThreshold,mv+num);
  }

  int backMode = ELEBACKMODE[pla == GOLD ? gRevLoc(eloc) : eloc];
  if(backMode == 1 || (backMode == 3 && oppEleAdjMoreCentral) || (backMode == 2 && (
      oppEleAdjMoreCentral ||
      (Board::XINTERVAL[eX-1][eX+1] & Board::YINTERVAL[gY(eloc F)][Board::STARTY[pla]] & oppMap & ~Bitmap::makeLoc(eloc F)).hasBits()
  )))
  {
    num += gUESPPRecurseInDir(b,pla,eloc,eloc F,eloc MF,origLoc,oppEleLoc,numStepsLeft,chainThreshold,mv+num);
  }

  return num;
}

int BoardTrees::genUsefulElephantMoves(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  if(numSteps <= 1)
    return 0;

  loc_t eloc = b.findElephant(pla);
  if(eloc == ERRLOC)
    return 0;
  loc_t oppEleLoc = b.findElephant(gOpp(pla));

  if(numSteps > 3)
    numSteps = 3;
  return genUsefulElephantStepPushPullsRec(b,pla,eloc,eloc,oppEleLoc,numSteps,2,mv);
}

static int genUndomPieceMovesTo1Or2S(Board& b, pla_t pla, loc_t ploc, loc_t dest, step_t pStep, int numSteps, move_t* mv)
{
  int num = 0;
  if(ISE(dest) && b.isTrapSafe2(pla,dest))
    mv[num++] = getMove(pStep);
  if(numSteps >= 2)
    num += genUndomPieceMovesTo2S(b,pla,ploc,dest,pStep,mv+num);
  return num;
}

static int numNonCamelDefs(Board& b, pla_t pla, loc_t ploc, int trapIndex)
{
  return b.trapGuardCounts[pla][trapIndex] - (int)(Board::ADJACENTTRAPINDEX[ploc] == trapIndex);
}

int BoardTrees::genCamelHostageRunaways(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  pla_t opp = gOpp(pla);
  loc_t oppEleLoc = b.findElephant(opp);
  if(oppEleLoc == ERRLOC)
    return 0;

  Bitmap undomCamelMap = b.pieceMaps[pla][CAM] & ~b.dominatedMap;
  if(undomCamelMap.isEmpty())
    return 0;
  loc_t ploc = undomCamelMap.nextBit();
  int gydist = Board::GOALYDIST[pla][ploc];
  if(gydist >= 5)
    return 0;
  int eleManhattanDist = b.manhattanDist(oppEleLoc,ploc);
  if(eleManhattanDist > 3)
    return 0;
  if(b.isTrap(ploc))
    return 0;
  int y1 = (pla == SILV ? 1 : 6);
  int x = gX(ploc);
  int y = gY(ploc);

  int trapIndex0 = Board::PLATRAPINDICES[opp][0];
  int trapIndex1 = Board::PLATRAPINDICES[opp][1];
  if(x <= 3 && numNonCamelDefs(b,pla,ploc,trapIndex0) >= 2 && (x <= 2 || b.trapGuardCounts[pla][trapIndex0] > 0)) {return 0;}
  if(x >= 4 && numNonCamelDefs(b,pla,ploc,trapIndex1) >= 2 && (x >= 5 || b.trapGuardCounts[pla][trapIndex1] > 0)) {return 0;}
  if(x <= 2 && b.trapGuardCounts[pla][trapIndex0] >= 2) {loc_t a2 = gLoc(0,y1); if(ploc != a2 && ISP(a2)) {return 0;}}
  if(x >= 5 && b.trapGuardCounts[pla][trapIndex1] >= 2) {loc_t h2 = gLoc(7,y1); if(ploc != h2 && ISP(h2)) {return 0;}}
  //TODO this condition seems not quite right - a c2 piece won't stop an a3 hostage.
  if(x <= 2) {loc_t c2 = gLoc(2,y1); if(ploc != c2 && ISP(c2) && !b.isDominated(c2)) {return 0;}}
  if(x >= 5) {loc_t f2 = gLoc(5,y1); if(ploc != f2 && ISP(f2) && !b.isDominated(f2)) {return 0;}}

  int dy = Board::GOALLOCINC[pla];
  if(gydist >= 3 && !(
      b.pieceMaps[pla][0] &
      ~b.pieceMaps[pla][ELE] &
      Board::XINTERVAL[max(x-2,0)][min(x+2,7)] &
      Board::YINTERVAL[y+Board::GOALYINC[pla]][Board::GOALY[pla]]
     ).isEmptyOrSingleBit())
    return 0;

  int num = 0;

  //Step each direction that either is back home or away from the opp
  if(CR1(pla,ploc) && gydist >= 2) num += genUndomPieceMovesTo1Or2S(b,pla,ploc,ploc F,ploc MF,numSteps,mv+num);
  if(CW1(ploc) && b.manhattanDist(oppEleLoc,ploc W) > eleManhattanDist) num += genUndomPieceMovesTo1Or2S(b,pla,ploc,ploc W,ploc MW,numSteps,mv+num);
  if(CE1(ploc) && b.manhattanDist(oppEleLoc,ploc E) > eleManhattanDist) num += genUndomPieceMovesTo1Or2S(b,pla,ploc,ploc E,ploc ME,numSteps,mv+num);
  if(CA1(pla,ploc) && gydist <= 3 && b.manhattanDist(oppEleLoc,ploc G) > eleManhattanDist) num += genUndomPieceMovesTo1Or2S(b,pla,ploc,ploc G,ploc MG,numSteps,mv+num);

  return num;
}

static int genRabSideAdvancesOnSide(Board& b, pla_t pla, int numSteps, int edgeX, Bitmap sideRows, move_t* mv)
{
  pla_t opp = gOpp(pla);

  int starty = Board::STARTY[pla];
  int yinc = Board::GOALYINC[pla];
  int dy = Board::GOALLOCINC[pla];
  loc_t loc;
  for(int i = 3; i<6; i++)
  {
    loc = gLoc(edgeX,starty + yinc * i);
    if(ISO(loc))
      break;
  }
  if(!(ISO(loc) && b.isRabbit(loc)))
    return 0;
  loc_t dest = loc F;
  if(!ISE(dest) || b.wouldRabbitBeDomAt(pla,dest))
    return 0;

  Bitmap plaRabs = b.pieceMaps[pla][RAB];
  int num = 0;
  for(int rad = 1; rad <= numSteps; rad++)
  {
    Bitmap map = plaRabs & Board::RADIUS[rad][dest] & sideRows & Board::YINTERVAL[starty][gY(dest)];
    bool foundAny = false;
    while(map.hasBits())
    {
      loc_t rloc = map.nextBit();
      if(!ISP(rloc G) && b.isRabbit(rloc G))
        continue;
      Bitmap simpleWalkLocs;
      int newNum = genWalkTosUsing(b,pla,rloc,dest,numSteps,mv+num,simpleWalkLocs);
      if(newNum > 0)
        foundAny = true;
      num += newNum;
    }
    if(foundAny && numSteps > rad+1)
      numSteps = rad+1;
  }
  return num;
}

//Generates rabbit advances up the side of the board that block opp rabbits
int BoardTrees::genRabSideAdvances(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  int num = 0;
  num += genRabSideAdvancesOnSide(b,pla,numSteps,0,Board::XINTERVAL[0][1],mv+num);
  num += genRabSideAdvancesOnSide(b,pla,numSteps,7,Board::XINTERVAL[6][7],mv+num);
  return num;
}

//------------------------------------------------------------------------------------------------------

//Generate a push, preferring to push the piece on pushLoc away from awayFromLoc, and away from the center
static loc_t pushPreferAwayFromLoc(Board& b, loc_t pushLoc, loc_t awayFromLoc)
{
  int best = 0;
  loc_t bestLoc = ERRLOC;
  loc_t loc;
  loc = pushLoc S;
  if(ISE(loc)) {int val = Board::manhattanDist(loc,awayFromLoc) * 10 + Board::CENTERDIST[loc]; if(val > best) {best = val; bestLoc = loc;}}
  loc = pushLoc N;
  if(ISE(loc)) {int val = Board::manhattanDist(loc,awayFromLoc) * 10 + Board::CENTERDIST[loc]; if(val > best) {best = val; bestLoc = loc;}}
  loc = pushLoc W;
  if(ISE(loc)) {int val = Board::manhattanDist(loc,awayFromLoc) * 10 + Board::CENTERDIST[loc]; if(val > best) {best = val; bestLoc = loc;}}
  loc = pushLoc E;
  if(ISE(loc)) {int val = Board::manhattanDist(loc,awayFromLoc) * 10 + Board::CENTERDIST[loc]; if(val > best) {best = val; bestLoc = loc;}}

  return bestLoc;
}
static int genPushPreferAwayFrom(Board& b, step_t pStep, loc_t pushLoc, loc_t awayFromLoc, move_t* mv, move_t moveSoFar, int ns)
{
  loc_t loc = pushPreferAwayFromLoc(b,pushLoc,awayFromLoc);
  if(loc != ERRLOC)
  {mv[0] = concatMoves(moveSoFar,getMove(gStepSrcDest(pushLoc,loc),pStep),ns); return 1;}
  return 0;
}

//Looks for ways to get the elephant to the desired trap.
//Except for possibly at the end, takes the first available way
//Expects that we've already checked mDist <= numSteps+1 and mDist >= 2
static int genElephantWalksAdjTo(Board& b, pla_t pla, loc_t eleLoc, loc_t trapLoc, int mDist, int numSteps, move_t* mv, move_t moveSoFar, int ns, Bitmap& visited)
{
  if(visited.isOne(eleLoc))
    return 0;
  visited.setOn(eleLoc);

  int num = 0;
  int excess = numSteps-mDist+1;
  pla_t opp = gOpp(pla);

  //Super-close to trap
  if(mDist == 2)
  {
    if(ISE(eleLoc W) && Board::isAdjacent(eleLoc W,trapLoc)) mv[num++] = concatMoves(moveSoFar,eleLoc MW,ns);
    if(ISE(eleLoc E) && Board::isAdjacent(eleLoc E,trapLoc)) mv[num++] = concatMoves(moveSoFar,eleLoc ME,ns);
    if(ISE(eleLoc S) && Board::isAdjacent(eleLoc S,trapLoc)) mv[num++] = concatMoves(moveSoFar,eleLoc MS,ns);
    if(ISE(eleLoc N) && Board::isAdjacent(eleLoc N,trapLoc)) mv[num++] = concatMoves(moveSoFar,eleLoc MN,ns);

    if(excess > 0)
    {
      if(ISO(eleLoc W) && GT(eleLoc,eleLoc W) && Board::isAdjacent(eleLoc W,trapLoc)) num += genPushPreferAwayFrom(b,eleLoc MW,eleLoc W,trapLoc,mv+num,moveSoFar,ns);
      if(ISO(eleLoc E) && GT(eleLoc,eleLoc E) && Board::isAdjacent(eleLoc E,trapLoc)) num += genPushPreferAwayFrom(b,eleLoc ME,eleLoc E,trapLoc,mv+num,moveSoFar,ns);
      if(ISO(eleLoc S) && GT(eleLoc,eleLoc S) && Board::isAdjacent(eleLoc S,trapLoc)) num += genPushPreferAwayFrom(b,eleLoc MS,eleLoc S,trapLoc,mv+num,moveSoFar,ns);
      if(ISO(eleLoc N) && GT(eleLoc,eleLoc N) && Board::isAdjacent(eleLoc N,trapLoc)) num += genPushPreferAwayFrom(b,eleLoc MN,eleLoc N,trapLoc,mv+num,moveSoFar,ns);
    }

    //Found no way in, so try walking around
    if(num == 0 && excess >= 2)
    {
      loc_t aroundLoc = ERRLOC;
      if(gX(eleLoc) == gX(trapLoc))      aroundLoc = (gY(eleLoc) >= 4) ? eleLoc S : eleLoc N;
      else if(gY(eleLoc) == gY(trapLoc)) aroundLoc = (gX(eleLoc) >= 4) ? eleLoc W : eleLoc E;
      if(aroundLoc != ERRLOC)
      {
        if(ISE(aroundLoc))
        {
          move_t nextMove = concatMoves(moveSoFar,gStepSrcDest(eleLoc,aroundLoc),ns);
          TSC(eleLoc,aroundLoc,num += genElephantWalksAdjTo(b,pla,aroundLoc,trapLoc,mDist+1,numSteps-1,mv+num,nextMove,ns+1,visited));
        }
        else if(excess >= 3 && ISO(aroundLoc) && GT(eleLoc,aroundLoc))
        {
          loc_t pushToLoc = pushPreferAwayFromLoc(b,aroundLoc,trapLoc);
          if(pushToLoc != ERRLOC)
          {
            move_t nextMove = concatMoves(moveSoFar,gMove(gStepSrcDest(aroundLoc,pushToLoc),gStepSrcDest(eleLoc,aroundLoc)),ns);
            TPC(eleLoc,aroundLoc,pushToLoc,num += genElephantWalksAdjTo(b,pla,aroundLoc,trapLoc,mDist+1,numSteps-2,mv+num,nextMove,ns+2,visited));
          }
        }
      }
    }
    return num;
  }

  //Otherwise, not super close to trap.
  //Walk towards the trap in each possible direction, stopping at the first success
  if(gX(eleLoc) > gX(trapLoc))
  {loc_t loc = eleLoc W; if(ISE(loc) && b.isTrapSafe2(pla,loc)) {TSC(eleLoc,loc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-1,mv+num,concatMoves(moveSoFar,gStepSrcDest(eleLoc,loc),ns),ns+1,visited)); if(num > 0) return num;}}
  else if(gX(eleLoc) < gX(trapLoc))
  {loc_t loc = eleLoc E; if(ISE(loc) && b.isTrapSafe2(pla,loc)) {TSC(eleLoc,loc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-1,mv+num,concatMoves(moveSoFar,gStepSrcDest(eleLoc,loc),ns),ns+1,visited)); if(num > 0) return num;}}
  if(gY(eleLoc) > gY(trapLoc))
  {loc_t loc = eleLoc S; if(ISE(loc) && b.isTrapSafe2(pla,loc)) {TSC(eleLoc,loc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-1,mv+num,concatMoves(moveSoFar,gStepSrcDest(eleLoc,loc),ns),ns+1,visited)); if(num > 0) return num;}}
  else if(gY(eleLoc) < gY(trapLoc))
  {loc_t loc = eleLoc N; if(ISE(loc) && b.isTrapSafe2(pla,loc)) {TSC(eleLoc,loc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-1,mv+num,concatMoves(moveSoFar,gStepSrcDest(eleLoc,loc),ns),ns+1,visited)); if(num > 0) return num;}}

  if(excess <= 0)
    return num;

  //Same, but pushing
  bool aligned = false;
  if(gX(eleLoc) > gX(trapLoc))
  {loc_t loc = eleLoc W; if(ISO(loc) && GT(eleLoc,loc) && b.isTrapSafe2(pla,loc)) {loc_t pushToLoc = pushPreferAwayFromLoc(b,loc,trapLoc); if(pushToLoc != ERRLOC) {TPC(eleLoc,loc,pushToLoc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-2,mv+num,concatMoves(moveSoFar,gMove(gStepSrcDest(loc,pushToLoc),gStepSrcDest(eleLoc,loc)),ns),ns+2,visited)); if(num > 0) return num;}}}
  else if(gX(eleLoc) < gX(trapLoc))
  {loc_t loc = eleLoc E; if(ISO(loc) && GT(eleLoc,loc) && b.isTrapSafe2(pla,loc)) {loc_t pushToLoc = pushPreferAwayFromLoc(b,loc,trapLoc); if(pushToLoc != ERRLOC) {TPC(eleLoc,loc,pushToLoc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-2,mv+num,concatMoves(moveSoFar,gMove(gStepSrcDest(loc,pushToLoc),gStepSrcDest(eleLoc,loc)),ns),ns+2,visited)); if(num > 0) return num;}}}
  else {aligned = true;}
  if(gY(eleLoc) > gY(trapLoc))
  {loc_t loc = eleLoc S; if(ISO(loc) && GT(eleLoc,loc) && b.isTrapSafe2(pla,loc)) {loc_t pushToLoc = pushPreferAwayFromLoc(b,loc,trapLoc); if(pushToLoc != ERRLOC) {TPC(eleLoc,loc,pushToLoc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-2,mv+num,concatMoves(moveSoFar,gMove(gStepSrcDest(loc,pushToLoc),gStepSrcDest(eleLoc,loc)),ns),ns+2,visited)); if(num > 0) return num;}}}
  else if(gY(eleLoc) < gY(trapLoc))
  {loc_t loc = eleLoc N; if(ISO(loc) && GT(eleLoc,loc) && b.isTrapSafe2(pla,loc)) {loc_t pushToLoc = pushPreferAwayFromLoc(b,loc,trapLoc); if(pushToLoc != ERRLOC) {TPC(eleLoc,loc,pushToLoc,num += genElephantWalksAdjTo(b,pla,loc,trapLoc,mDist-1,numSteps-2,mv+num,concatMoves(moveSoFar,gMove(gStepSrcDest(loc,pushToLoc),gStepSrcDest(eleLoc,loc)),ns),ns+2,visited)); if(num > 0) return num;}}}
  else {aligned = true;}

  //Found no way in, so try walking around
  if(aligned && num == 0 && excess >= 2)
  {
    loc_t aroundLoc = ERRLOC;
    if(gX(eleLoc) == gX(trapLoc))      aroundLoc = (gY(eleLoc) >= 4) ? eleLoc S : eleLoc N;
    else if(gY(eleLoc) == gY(trapLoc)) aroundLoc = (gX(eleLoc) >= 4) ? eleLoc W : eleLoc E;
    if(aroundLoc != ERRLOC)
    {
      if(ISE(aroundLoc))
      {
        move_t nextMove = concatMoves(moveSoFar,gStepSrcDest(eleLoc,aroundLoc),ns);
        TSC(eleLoc,aroundLoc,num += genElephantWalksAdjTo(b,pla,aroundLoc,trapLoc,mDist+1,numSteps-1,mv+num,nextMove,ns+1,visited));
      }
    }
  }
  return num;
}

//Generate simple defenses of a trap at loc (assumed to be adjacent), preferring to push pieces away
static int genSimpleDefenses(Board& b, pla_t pla, loc_t loc, loc_t trapLoc, int numSteps, move_t* mv)
{
  int dy = Board::GOALLOCINC[pla];
  int num = 0;
  pla_t opp = gOpp(pla);
  if(ISE(loc))
  {
    if(ISP(loc F) && b.isThawed(loc F)) {mv[num++] = loc F MG; return num;}
    if(gX(trapLoc) >= 4)
    {
      if(ISP(loc W) && b.isThawed(loc W)) {mv[num++] = loc W ME; return num;}
      if(ISP(loc E) && b.isThawed(loc E)) {mv[num++] = loc E MW; return num;}
    }
    else
    {
      if(ISP(loc E) && b.isThawed(loc E)) {mv[num++] = loc E MW; return num;}
      if(ISP(loc W) && b.isThawed(loc W)) {mv[num++] = loc W ME; return num;}
    }
    if(ISP(loc G) && b.isThawed(loc G) && !b.isRabbit(loc G)) {mv[num++] = loc G MF; return num;}
  }
  else if(ISO(loc) && numSteps >= 2)
  {
    loc_t firstStrongerPla = ERRLOC;
    do{
      if(ISP(loc F) && b.isThawed(loc F) && GT(loc F,loc)) {firstStrongerPla = loc F; break;}
      if(gX(trapLoc) >= 4)
      {
        if(ISP(loc W) && b.isThawed(loc W) && GT(loc W,loc)) {firstStrongerPla = loc W; break;}
        if(ISP(loc E) && b.isThawed(loc E) && GT(loc E,loc)) {firstStrongerPla = loc E; break;}
      }
      else
      {
        if(ISP(loc E) && b.isThawed(loc E) && GT(loc E,loc)) {firstStrongerPla = loc E; break;}
        if(ISP(loc W) && b.isThawed(loc W) && GT(loc W,loc)) {firstStrongerPla = loc W; break;}
      }
      if(ISP(loc G) && b.isThawed(loc G) && GT(loc G,loc)) {firstStrongerPla = loc G; break;}
    } while(false);
    if(firstStrongerPla != ERRLOC)
    {
      loc_t pushToLoc = pushPreferAwayFromLoc(b,loc,trapLoc);
      if(pushToLoc != ERRLOC)
      {mv[num++] = getMove(gStepSrcDest(loc,pushToLoc),gStepSrcDest(firstStrongerPla,loc)); return num;}
    }
  }
  return num;
}

int BoardTrees::genImportantTrapDefenses(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  pla_t opp = gOpp(pla);
  loc_t eleLoc = b.findElephant(pla);
  loc_t oppEleLoc = b.findElephant(opp);
  if(eleLoc == ERRLOC || oppEleLoc == ERRLOC)
    return 0;

  int num = 0;
  for(int i = 0; i<4; i++)
  {
    loc_t trapLoc = Board::TRAPLOCS[i];
    int plaCount = b.trapGuardCounts[pla][i];

    //Filter down to just traps that are likely to benefit from defense
    if(plaCount >= 2)
      continue;
    if(Board::isAdjacent(eleLoc,trapLoc))
      continue;
    if(plaCount <= 0 && (Board::RADIUS[2][trapLoc] & b.dominatedMap & b.pieceMaps[pla][0]).isEmpty())
      continue;
    int oppCount = b.trapGuardCounts[opp][i];
    if(oppCount <= 0)
      continue;

    //If there's only one pla, look for ways to defend the trap in very simple ways
    if(plaCount == 1)
    {
      num += genSimpleDefenses(b,pla,trapLoc S,trapLoc,numSteps,mv+num);
      num += genSimpleDefenses(b,pla,trapLoc W,trapLoc,numSteps,mv+num);
      num += genSimpleDefenses(b,pla,trapLoc E,trapLoc,numSteps,mv+num);
      num += genSimpleDefenses(b,pla,trapLoc N,trapLoc,numSteps,mv+num);
    }

    //Look for ways to walk the elephant over to the trap
    int mDist = Board::manhattanDist(eleLoc,trapLoc);
    if(mDist <= numSteps+1 && mDist >= 2)
    {
      //If the elephant is super-close, don't bother unless the opposing elephant is in the way, so as not to overlap with the
      //above simple trap defenses
      if(mDist > 2 || (b.isAdjacent(eleLoc,oppEleLoc) && trapLoc == eleLoc + (oppEleLoc-eleLoc)))
      {
        Bitmap visited = Bitmap();
        num += genElephantWalksAdjTo(b,pla,eleLoc,trapLoc,mDist,numSteps,mv+num,ERRMOVE,0,visited);
      }
    }
  }
  return num;
}

/*
int BoardTrees::genGoalThreats(Board& b, pla_t pla, int numSteps, move_t* mv)
{
  int goalDistEst[BSIZE];
  Threats::getGoalDistEst(b,pla,goalDistEst,numSteps+4);

  Bitmap relevant;
  for(int x = 0; x<8; x++)
    for(int y = 0; y<8; y++)
    {
      int i = gLoc(x,y);
      if(goalDistEst[i] <= 7)
        relevant.setOn(i);
    }
  relevant |= Bitmap::adj(relevant);

  Bitmap rabRadMap;
  Bitmap rabMap = b.pieceMaps[pla][RAB];
  while(rabMap.hasBits())
  {
    loc_t rabLoc = rabMap.nextBit();
    if(Board::GOALYDIST[rabLoc] <= 0)
      continue;
    if(goalDistEst[rabLoc] + (int)(b.isFrozen(rabLoc)) > numSteps+4)
      continue;
    loc_t inFront = Board::ADVANCE_OFFSET[pla] + rabLoc;
    rabRadMap |= Board::DISK[numSteps+1][inFront];
  }
  relevant &= rabRadMap;

  return genGoalThreatsRec(b,pla,relevant,numSteps,mv,ERRMOVE,0);
}

int BoardTrees::genGoalThreatsRec(Board& b, pla_t pla, Bitmap relevant, int numStepsLeft, move_t* mv,
    move_t moveSoFar, int numStepsSoFar)
{
  int num = 0;
  if(numStepsLeft == 0)
  {
    if(BoardTrees::goalDist(b,pla,4) <= 4)
      mv[num++] = moveSoFar;
    return num;
  }

  if(numStepsSoFar > 0 && BoardTrees::goalDist(b,pla,4) <= 4)
    mv[num++] = moveSoFar;

  int numMoves = 0;
  move_t moves[256];
  if(numStepsLeft >= 1)
    numMoves += BoardMoveGen::genStepsInvolving(b,pla,moves+numMoves,relevant);
  if(numStepsLeft >= 2)
    numMoves += BoardMoveGen::genPushPullsInvolving(b,pla,moves+numMoves,relevant);

  for(int i = 0; i<numMoves; i++)
  {
    move_t move = moves[i];
    int ns = numStepsInMove(move);

    step_t lastStep = getStep(move,ns-1);

    Bitmap newRelevant = Board::RADIUS[1][gSrc(lastStep)] | Board::RADIUS[1][gDest(lastStep)];
    if(numStepsLeft-ns > 0)
      newRelevant |= Bitmap::adj(newRelevant & Bitmap::BMPTRAPS);

    UndoMoveData uData;
    b.makeMove(move,uData);
    num += genGoalThreatsRec(b,pla,newRelevant,numStepsLeft-ns,mv+num,concatMoves(moveSoFar,move,numStepsSoFar),numStepsSoFar+ns);
    b.undoMove(uData);
  }

  return num;
}
*/






