/*
 * retro.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/offsets.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"

using namespace std;

#define APPENDCALL(n,x) f(concatMoves(sofar,x,ns),ns+(n));

//P = pla
//O = opp
//iterT* = iterate over each possible way of doing * with tempsteps, calling f on each one.
//iterS* = iterate over each possible way of doing * statically, calling f on each one.
//nS = exactly n steps
//nL = leq n steps
//step = step there regardless of trapsafety
//place = actually establish a piece there

//Iterate over single steps to [loc], ignoring steps from [ign] to [loc].
//Does not check trapsafety at [loc]
template <typename Func>
static void iterTStepTo1S(Board& b, move_t sofar, int ns, loc_t loc, loc_t ign, const Func& f)
{
  pla_t pla = b.player;
  if(loc S != ign && ISP(loc S) && b.isThawedC(loc S) && b.isRabOkayN(pla,loc S)) {TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))}
  if(loc W != ign && ISP(loc W) && b.isThawedC(loc W))                            {TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))}
  if(loc E != ign && ISP(loc E) && b.isThawedC(loc E))                            {TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))}
  if(loc N != ign && ISP(loc N) && b.isThawedC(loc N) && b.isRabOkayS(pla,loc N)) {TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))}
}

//Iterate over single steps to [loc], ignoring steps from [ign] to [loc], requiring a piece greater than [piece].
//Assumes [loc] is empty.
//Checks that the step will be trapsafe and the piece will be uf afterwards.
template <typename Func>
static void iterTPlaceGtToUF1S(Board& b, move_t sofar, int ns, loc_t loc, loc_t ign, piece_t piece, const Func& f)
{
  pla_t pla = b.player;
  if(b.isTrapSafe2(pla,loc))
  {
    if(loc S != ign && ISP(loc S) && GTP(loc S,piece) && b.isThawedC(loc S) && b.wouldBeUF(pla,loc S,loc,loc S) && b.isRabOkayN(pla,loc S)) {TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))}
    if(loc W != ign && ISP(loc W) && GTP(loc W,piece) && b.isThawedC(loc W) && b.wouldBeUF(pla,loc W,loc,loc W))                            {TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))}
    if(loc E != ign && ISP(loc E) && GTP(loc E,piece) && b.isThawedC(loc E) && b.wouldBeUF(pla,loc E,loc,loc E))                            {TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))}
    if(loc N != ign && ISP(loc N) && GTP(loc N,piece) && b.isThawedC(loc N) && b.wouldBeUF(pla,loc N,loc,loc N) && b.isRabOkayS(pla,loc N)) {TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))}
  }
}

//Iterate over single step defenses of [loc], ignoring steps using any piece at [ign] OR any
//steps stepping TO [ign].
template <typename Func>
static void iterTDefend1S(Board& b, move_t sofar, int ns, loc_t loc, loc_t ign, const Func& f)
{
  if(loc S != ign && ISE(loc S)) iterTStepTo1S(b,sofar,ns,loc S,ign,f);
  if(loc W != ign && ISE(loc W)) iterTStepTo1S(b,sofar,ns,loc W,ign,f);
  if(loc E != ign && ISE(loc E)) iterTStepTo1S(b,sofar,ns,loc E,ign,f);
  if(loc N != ign && ISE(loc N)) iterTStepTo1S(b,sofar,ns,loc N,ign,f);
}
template <typename Func>
static void iterTDefend1S(Board& b, move_t sofar, int ns, loc_t loc, loc_t igndefat, loc_t ignusepla, const Func& f)
{
  if(loc S != igndefat && ISE(loc S)) iterTStepTo1S(b,sofar,ns,loc S,ignusepla,f);
  if(loc W != igndefat && ISE(loc W)) iterTStepTo1S(b,sofar,ns,loc W,ignusepla,f);
  if(loc E != igndefat && ISE(loc E)) iterTStepTo1S(b,sofar,ns,loc E,ignusepla,f);
  if(loc N != igndefat && ISE(loc N)) iterTStepTo1S(b,sofar,ns,loc N,ignusepla,f);
}

//Iterate over single steps by loc in each direction, assuming it's an unfrozen pla piece.
template <typename Func>
static void iterSStepAway1S(Board& b, move_t sofar, int ns, loc_t loc, const Func& f)
{
  pla_t pla = b.player;
  if(ISE(loc S) && b.isRabOkayS(pla,loc)) APPENDCALL(1,getMove(loc MS));
  if(ISE(loc W))                          APPENDCALL(1,getMove(loc MW));
  if(ISE(loc E))                          APPENDCALL(1,getMove(loc ME));
  if(ISE(loc N) && b.isRabOkayN(pla,loc)) APPENDCALL(1,getMove(loc MN));
}

//Iterate over single steps by loc in each direction, assuming it's an unfrozen pla piece.
template <typename Func>
static void iterTStepAway1S(Board& b, move_t sofar, int ns, loc_t loc, const Func& f)
{
  pla_t pla = b.player;
  if(ISE(loc S) && b.isRabOkayS(pla,loc)) {TSC(loc,loc S,APPENDCALL(1,getMove(loc MS)));}
  if(ISE(loc W))                          {TSC(loc,loc W,APPENDCALL(1,getMove(loc MW)));}
  if(ISE(loc E))                          {TSC(loc,loc E,APPENDCALL(1,getMove(loc ME)));}
  if(ISE(loc N) && b.isRabOkayN(pla,loc)) {TSC(loc,loc N,APPENDCALL(1,getMove(loc MN)));}
}

//Iterate over ways of getting a piece to [loc] in 2 steps, ignoring direct use of [ign] itself.
//Assumes [loc] is empty.
//Does not check trapsafety at [loc]
template <typename Func>
static void iterTPrepStepToFrom1S(Board& b, move_t sofar, int ns, loc_t loc, loc_t from, bool nonRab, const Func& f)
{
  pla_t pla = b.player;
  if(ISE(from))
    iterTPlaceGtToUF1S(b,sofar,ns,from,loc,nonRab ? RAB : EMP,f);
  else if(ISP(from))
  {
    if(!(nonRab && b.isRabbit(from)))
    {
      //TODO generate 1s step-tos that sac a piece, but with an extra sac defense.
      if(b.isFrozenC(from))
        iterTDefend1S(b,sofar,ns,from,loc,f);
    }
  }
}

//Iterate over ways of [ploc] pushing [oloc], assuming ploc is unfrozen and large enough
template <typename Func>
static void iterTPushPO2S(Board& b, move_t sofar, int ns, loc_t ploc, loc_t oloc, const Func& f)
{
  step_t pstep = gStepSrcDest(ploc,oloc);
  if(ISE(oloc S)) {TPC(ploc,oloc,oloc S,APPENDCALL(2,getMove(oloc MS,pstep)))}
  if(ISE(oloc W)) {TPC(ploc,oloc,oloc W,APPENDCALL(2,getMove(oloc MW,pstep)))}
  if(ISE(oloc E)) {TPC(ploc,oloc,oloc E,APPENDCALL(2,getMove(oloc ME,pstep)))}
  if(ISE(oloc N)) {TPC(ploc,oloc,oloc N,APPENDCALL(2,getMove(oloc MN,pstep)))}
}

//Iterate over ways of [ploc] pulling [oloc], assuming ploc is unfrozen and large enough
template <typename Func>
static void iterTPullPO2S(Board& b, move_t sofar, int ns, loc_t ploc, loc_t oloc, const Func& f)
{
  step_t ostep = gStepSrcDest(oloc,ploc);
  if(ISE(ploc S)) {TPC(oloc,ploc,ploc S,APPENDCALL(2,getMove(ploc MS,ostep)))}
  if(ISE(ploc W)) {TPC(oloc,ploc,ploc W,APPENDCALL(2,getMove(ploc MW,ostep)))}
  if(ISE(ploc E)) {TPC(oloc,ploc,ploc E,APPENDCALL(2,getMove(ploc ME,ostep)))}
  if(ISE(ploc N)) {TPC(oloc,ploc,ploc N,APPENDCALL(2,getMove(ploc MN,ostep)))}
}

//Iterate over ways of anything pulling [oloc]
template <typename Func>
static void iterTPull2S(Board& b, move_t sofar, int ns, loc_t oloc, const Func& f)
{
  pla_t pla = b.player;
  if(ISP(oloc S) && GT(oloc S,oloc) && b.isThawedC(oloc S)) iterTPullPO2S(b,sofar,ns,oloc S,oloc,f);
  if(ISP(oloc W) && GT(oloc W,oloc) && b.isThawedC(oloc W)) iterTPullPO2S(b,sofar,ns,oloc W,oloc,f);
  if(ISP(oloc E) && GT(oloc E,oloc) && b.isThawedC(oloc E)) iterTPullPO2S(b,sofar,ns,oloc E,oloc,f);
  if(ISP(oloc N) && GT(oloc N,oloc) && b.isThawedC(oloc N)) iterTPullPO2S(b,sofar,ns,oloc N,oloc,f);
}

//Iterate over ways of getting a piece to [loc] in 2 steps, ignoring direct use of [ign] itself.
//Does not check trapsafety at [loc]
template <typename Func>
static void iterTStepTo2S(Board& b, move_t sofar, int ns, loc_t loc, loc_t ign, const Func& f)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  if(ISE(loc))
  {
    if(loc S != ign) iterTPrepStepToFrom1S(b,sofar,ns,loc,loc S,pla == SILV,[=,&b,&f](move_t sofar, int ns){TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))});
    if(loc W != ign) iterTPrepStepToFrom1S(b,sofar,ns,loc,loc W,false      ,[=,&b,&f](move_t sofar, int ns){TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))});
    if(loc E != ign) iterTPrepStepToFrom1S(b,sofar,ns,loc,loc E,false      ,[=,&b,&f](move_t sofar, int ns){TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))});
    if(loc N != ign) iterTPrepStepToFrom1S(b,sofar,ns,loc,loc N,pla == GOLD,[=,&b,&f](move_t sofar, int ns){TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))});
  }
  else if(ISO(loc))
  {
    if(loc S != ign && ISP(loc S) && GT(loc S,loc) && b.isThawedC(loc S)) iterTPushPO2S(b,sofar,ns,loc S,loc,f);
    if(loc W != ign && ISP(loc W) && GT(loc W,loc) && b.isThawedC(loc W)) iterTPushPO2S(b,sofar,ns,loc W,loc,f);
    if(loc E != ign && ISP(loc E) && GT(loc E,loc) && b.isThawedC(loc E)) iterTPushPO2S(b,sofar,ns,loc E,loc,f);
    if(loc N != ign && ISP(loc N) && GT(loc N,loc) && b.isThawedC(loc N)) iterTPushPO2S(b,sofar,ns,loc N,loc,f);
  }
}

//Iterate over two step defenses of [loc], ignoring any ways that involve the square at [ign]
//as the direct defense square and ignoring defenses that go through [loc] itself
template <typename Func>
static void iterTDefend2S(Board& b, move_t sofar, int ns, loc_t loc, loc_t ign, const Func& f)
{
  if(loc S != ign) iterTStepTo2S(b,sofar,ns,loc S,loc,f);
  if(loc W != ign) iterTStepTo2S(b,sofar,ns,loc W,loc,f);
  if(loc E != ign) iterTStepTo2S(b,sofar,ns,loc E,loc,f);
  if(loc N != ign) iterTStepTo2S(b,sofar,ns,loc N,loc,f);
}

//Iterate over ways to get rid of a pla piece at [ploc] in 2 steps, ignoring paths going through [ign]
//AND unfreeze-steps that use a piece at [ign]
//NOT COMPLETE - doesn't generate sacrifical unblocks
template <typename Func>
static void iterTStepAway2S(Board& b, move_t sofar, int ns, loc_t ploc, loc_t ign, const Func& f)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  if(b.isThawedC(ploc))
  {
    //Unblock unblock
    if(b.isTrapSafe2(pla,ploc) && (!b.isDominatedC(ploc) || b.isGuarded2(pla,ploc)))
    {
      if(ploc S != ign && ISP(ploc S) && b.isRabOkayS(pla,ploc)) iterTStepAway1S(b,sofar,ns,ploc S,[=,&b,&f](move_t sofar, int ns){TSC(ploc,ploc S,APPENDCALL(1,getMove(ploc MS)))});
      if(ploc W != ign && ISP(ploc W))                           iterTStepAway1S(b,sofar,ns,ploc W,[=,&b,&f](move_t sofar, int ns){TSC(ploc,ploc W,APPENDCALL(1,getMove(ploc MW)))});
      if(ploc E != ign && ISP(ploc E))                           iterTStepAway1S(b,sofar,ns,ploc E,[=,&b,&f](move_t sofar, int ns){TSC(ploc,ploc E,APPENDCALL(1,getMove(ploc ME)))});
      if(ploc N != ign && ISP(ploc N) && b.isRabOkayN(pla,ploc)) iterTStepAway1S(b,sofar,ns,ploc N,[=,&b,&f](move_t sofar, int ns){TSC(ploc,ploc N,APPENDCALL(1,getMove(ploc MN)))});
    }
    //Push away
    if(ploc S != ign && ISO(ploc S) && GT(ploc,ploc S)) iterTPushPO2S(b,sofar,ns,ploc,ploc S,f);
    if(ploc W != ign && ISO(ploc W) && GT(ploc,ploc W)) iterTPushPO2S(b,sofar,ns,ploc,ploc W,f);
    if(ploc E != ign && ISO(ploc E) && GT(ploc,ploc E)) iterTPushPO2S(b,sofar,ns,ploc,ploc E,f);
    if(ploc N != ign && ISO(ploc N) && GT(ploc,ploc N)) iterTPushPO2S(b,sofar,ns,ploc,ploc N,f);
  }
  else if(b.isOpen2(ploc))
  {
    //Unfreeze and step
    if(ploc S != ign && ISE(ploc S) && b.isRabOkayS(pla,ploc)) iterTDefend1S(b,sofar,ns,ploc,ploc S,ign,[=,&f](move_t sofar, int ns){APPENDCALL(1,getMove(ploc MS))});
    if(ploc W != ign && ISE(ploc W))                           iterTDefend1S(b,sofar,ns,ploc,ploc W,ign,[=,&f](move_t sofar, int ns){APPENDCALL(1,getMove(ploc MW))});
    if(ploc E != ign && ISE(ploc E))                           iterTDefend1S(b,sofar,ns,ploc,ploc E,ign,[=,&f](move_t sofar, int ns){APPENDCALL(1,getMove(ploc ME))});
    if(ploc N != ign && ISE(ploc N) && b.isRabOkayN(pla,ploc)) iterTDefend1S(b,sofar,ns,ploc,ploc N,ign,[=,&f](move_t sofar, int ns){APPENDCALL(1,getMove(ploc MN))});
  }
}


//Covers the cases of iterTPlaceGtToNRUF2S from one of the 4 possible adj locs that apply when the location is empty and trapsafe2, namely:
//Step and then unfreeze
//Unfreeze and then step
//Advance unfreeze
//Assumes the piece at [from] is a player that is strong enough
template <typename Func>
static void iterTPlaceGtToNRUFHelper2S(Board& b, move_t sofar, int ns, loc_t loc, loc_t from, const Func& f)
{
  pla_t pla = b.player;
  step_t pstep = gStepSrcDest(from,loc);
  if(b.isFrozenC(from)) {
    //Unfreeze and then step
    iterTDefend1S(b,sofar,ns,from,loc,[=,&b,&f](move_t sofar, int ns){if(b.isTrapSafe2(pla,loc) && b.wouldBeUF(pla,from,loc,from)) {TSC(from,loc,APPENDCALL(1,getMove(pstep)))}});
  }
  else if(!b.wouldBeUF(pla,from,loc,from))
  {
    //Step and then unfreeze
    {TSC(from,loc,iterTDefend1S(b,concatMoves(sofar,pstep,ns),ns+1,loc,ERRLOC,f));}
    //Advance uf
    if(b.isTrapSafe2(pla,from) && (!b.isDominatedC(from) || b.isGuarded2(pla,from)))
    {
      if(gY(loc) != gY(from))
      {
        if(ISE(loc W) && ISP(from W) && b.isRabOkay(pla,from W,loc W) && b.isTrapSafe2(pla,loc W) && (!b.isTrapSafe1(pla,from W) || !b.wouldBeUF(pla,from W,from W,from)))
        {TSC(from W,loc W,TSC2(from,loc,APPENDCALL(2,getMove(gStepSrcDest(from W,loc W),pstep))))}
        if(ISE(loc E) && ISP(from E) && b.isRabOkay(pla,from E,loc E) && b.isTrapSafe2(pla,loc E) && (!b.isTrapSafe1(pla,from E) || !b.wouldBeUF(pla,from E,from E,from)))
        {TSC(from E,loc E,TSC2(from,loc,APPENDCALL(2,getMove(gStepSrcDest(from E,loc E),pstep))))}
      }
      else
      {
        if(ISE(loc S) && ISP(from S) && b.isTrapSafe2(pla,loc S) && (!b.isTrapSafe1(pla,from S) || !b.wouldBeUF(pla,from S,from S,from)))
        {TSC(from S,loc S,TSC2(from,loc,APPENDCALL(2,getMove(gStepSrcDest(from S,loc S),pstep))))}
        if(ISE(loc N) && ISP(from N) && b.isTrapSafe2(pla,loc N) && (!b.isTrapSafe1(pla,from N) || !b.wouldBeUF(pla,from N,from N,from)))
        {TSC(from N,loc N,TSC2(from,loc,APPENDCALL(2,getMove(gStepSrcDest(from N,loc N),pstep))))}
      }
    }
  }
}

//Step a piece in 2 steps to [loc] that is greater than the specified piece, ending UF.
//Assumes [loc] is empty.
//Ignores paths immediately through [ign]
//Assumes [piece] >= rab, so that we can not consider rabbits among our desired pieces.
//Does check trapsafety of the destination as well.
template <typename Func>
static void iterTPlaceGtToNRUF2S(Board& b, move_t sofar, int ns, loc_t loc, loc_t ign, piece_t piece, const Func& f)
{
  pla_t pla = b.player;
  if(b.isTrapSafe1(pla,loc))
  {
    //Step twice
    if(loc S != ign && ISE(loc S)) iterTPlaceGtToUF1S(b,sofar,ns,loc S,ERRLOC,piece,[=,&b,&f](move_t sofar, int ns){if(b.wouldBeUF(pla,loc S,loc,loc S)) {TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))}});
    if(loc W != ign && ISE(loc W)) iterTPlaceGtToUF1S(b,sofar,ns,loc W,ERRLOC,piece,[=,&b,&f](move_t sofar, int ns){if(b.wouldBeUF(pla,loc W,loc,loc W)) {TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))}});
    if(loc E != ign && ISE(loc E)) iterTPlaceGtToUF1S(b,sofar,ns,loc E,ERRLOC,piece,[=,&b,&f](move_t sofar, int ns){if(b.wouldBeUF(pla,loc E,loc,loc E)) {TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))}});
    if(loc N != ign && ISE(loc N)) iterTPlaceGtToUF1S(b,sofar,ns,loc N,ERRLOC,piece,[=,&b,&f](move_t sofar, int ns){if(b.wouldBeUF(pla,loc N,loc,loc N)) {TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))}});

    //Step and then unfreeze
    //Unfreeze and then step
    //Advance unfreeze
    if(b.isTrapSafe2(pla,loc))
    {
      if(loc S != ign && ISP(loc S) && GTP(loc S,piece)) iterTPlaceGtToNRUFHelper2S(b,sofar,ns,loc,loc S,f);
      if(loc W != ign && ISP(loc W) && GTP(loc W,piece)) iterTPlaceGtToNRUFHelper2S(b,sofar,ns,loc,loc W,f);
      if(loc E != ign && ISP(loc E) && GTP(loc E,piece)) iterTPlaceGtToNRUFHelper2S(b,sofar,ns,loc,loc E,f);
      if(loc N != ign && ISP(loc N) && GTP(loc N,piece)) iterTPlaceGtToNRUFHelper2S(b,sofar,ns,loc,loc N,f);
    }
    //Defend the trap then step
    else
    {
      if(loc S != ign && ISP(loc S) && GTP(loc S,piece)) iterTDefend1S(b,sofar,ns,loc,loc S,[=,&b,&f](move_t sofar, int ns){if(b.isThawedC(loc S)) {TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))}});
      if(loc W != ign && ISP(loc W) && GTP(loc W,piece)) iterTDefend1S(b,sofar,ns,loc,loc W,[=,&b,&f](move_t sofar, int ns){if(b.isThawedC(loc W)) {TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))}});
      if(loc E != ign && ISP(loc E) && GTP(loc E,piece)) iterTDefend1S(b,sofar,ns,loc,loc E,[=,&b,&f](move_t sofar, int ns){if(b.isThawedC(loc E)) {TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))}});
      if(loc N != ign && ISP(loc N) && GTP(loc N,piece)) iterTDefend1S(b,sofar,ns,loc,loc N,[=,&b,&f](move_t sofar, int ns){if(b.isThawedC(loc N)) {TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))}});
    }
  }
}


#define APPENDMV(x) mv[num++] = concatMoves(sofar,x,ns);

//Generate pushpulls of [ploc] on [oloc] in 2 steps, assuming both exist and [ploc] is unfrozen and gt [oloc]
static int genPPPO2S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t oloc)
{
  int num = 0;
  step_t pstep = gStepSrcDest(ploc,oloc);
  step_t ostep = gStepSrcDest(oloc,ploc);
  if(ISE(ploc S)) {APPENDMV(getMove(ploc MS,ostep))}
  if(ISE(ploc W)) {APPENDMV(getMove(ploc MW,ostep))}
  if(ISE(ploc E)) {APPENDMV(getMove(ploc ME,ostep))}
  if(ISE(ploc N)) {APPENDMV(getMove(ploc MN,ostep))}
  if(ISE(oloc S)) {APPENDMV(getMove(oloc MS,pstep))}
  if(ISE(oloc W)) {APPENDMV(getMove(oloc MW,pstep))}
  if(ISE(oloc E)) {APPENDMV(getMove(oloc ME,pstep))}
  if(ISE(oloc N)) {APPENDMV(getMove(oloc MN,pstep))}
  return num;
}

//Generate pulls of [ploc] on [oloc] in 2 steps, assuming both exist and [ploc] is unfrozen and gt [oloc]
static int genPullsPO2S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t oloc)
{
  int num = 0;
  step_t ostep = gStepSrcDest(oloc,ploc);
  if(ISE(ploc S)) {APPENDMV(getMove(ploc MS,ostep))}
  if(ISE(ploc W)) {APPENDMV(getMove(ploc MW,ostep))}
  if(ISE(ploc E)) {APPENDMV(getMove(ploc ME,ostep))}
  if(ISE(ploc N)) {APPENDMV(getMove(ploc MN,ostep))}
  return num;
}

//Generate pushes of [ploc] on [oloc] in 2 steps, assuming both exist and [ploc] is unfrozen and gt [oloc]
static int genPushesPO2S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t oloc)
{
  int num = 0;
  step_t pstep = gStepSrcDest(ploc,oloc);
  if(ISE(oloc S)) {APPENDMV(getMove(oloc MS,pstep))}
  if(ISE(oloc W)) {APPENDMV(getMove(oloc MW,pstep))}
  if(ISE(oloc E)) {APPENDMV(getMove(oloc ME,pstep))}
  if(ISE(oloc N)) {APPENDMV(getMove(oloc MN,pstep))}
  return num;
}

//Generate all possible pushes of [oloc] to [loc] assuming [loc] is empty
static int genPushTo2S(Board& b, move_t sofar, int ns, move_t* mv, loc_t oloc, loc_t loc)
{
  int num = 0;
  pla_t pla = b.player;
  step_t ostep = gStepSrcDest(oloc,loc);
  if(ISP(oloc S) && GT(oloc S,oloc) && b.isThawedC(oloc S)) {APPENDMV(getMove(ostep,oloc S MN))}
  if(ISP(oloc W) && GT(oloc W,oloc) && b.isThawedC(oloc W)) {APPENDMV(getMove(ostep,oloc W ME))}
  if(ISP(oloc E) && GT(oloc E,oloc) && b.isThawedC(oloc E)) {APPENDMV(getMove(ostep,oloc E MW))}
  if(ISP(oloc N) && GT(oloc N,oloc) && b.isThawedC(oloc N)) {APPENDMV(getMove(ostep,oloc N MS))}
  return num;
}

//Generate pushpulls of [ploc] on [oloc] that require unblocking the push dest, in 3 steps, assuming
//both exist and [ploc] is unfrozen and gt [oloc]
//NOT complete - doesn't generate sacrificial unblocks of a square
static int genUnblockPushPO3S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t oloc)
{
  int num = 0;
  pla_t pla = b.player;
  step_t pstep = gStepSrcDest(ploc,oloc);
  if(oloc S != ploc && ISP(oloc S) && b.isThawedC(oloc S)) {iterSStepAway1S(b,sofar,ns,oloc S,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(oloc MS,pstep));});}
  if(oloc W != ploc && ISP(oloc W) && b.isThawedC(oloc W)) {iterSStepAway1S(b,sofar,ns,oloc W,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(oloc MW,pstep));});}
  if(oloc E != ploc && ISP(oloc E) && b.isThawedC(oloc E)) {iterSStepAway1S(b,sofar,ns,oloc E,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(oloc ME,pstep));});}
  if(oloc N != ploc && ISP(oloc N) && b.isThawedC(oloc N)) {iterSStepAway1S(b,sofar,ns,oloc N,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(oloc MN,pstep));});}
  return num;
}

//Generate pushpulls of [ploc] on [oloc] that require unblocking the pull dest, in 3 steps, assuming
//both exist and [ploc] is unfrozen and gt [oloc]
static int genUnblockPullPO3S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t oloc)
{
  int num = 0;
  pla_t pla = b.player;
  if(b.isGuarded2(pla,ploc) || (!b.isTrap(ploc) && !b.isDominatedC(ploc)))
  {
    step_t ostep = gStepSrcDest(oloc,ploc);
    if(ISP(ploc S)) {iterSStepAway1S(b,sofar,ns,ploc S,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc MS,ostep));});}
    if(ISP(ploc W)) {iterSStepAway1S(b,sofar,ns,ploc W,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc MW,ostep));});}
    if(ISP(ploc E)) {iterSStepAway1S(b,sofar,ns,ploc E,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc ME,ostep));});}
    if(ISP(ploc N)) {iterSStepAway1S(b,sofar,ns,ploc N,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc MN,ostep));});}
  }
  return num;
}

//Generate pushpulls of [ploc] on [oloc] to [loc] that require unblocking [loc], in 3 steps, assuming
//both exist and [ploc] is unfrozen and gt [oloc]
//NOT complete - doesn't generate sacrificial unblocks of a square
static int genUnblockPushTo3S(Board& b, move_t sofar, int ns, move_t* mv, loc_t loc, move_t pushmove)
{
  int num = 0;
  pla_t pla = b.player;
  if(ISP(loc) && b.isThawedC(loc))
    iterSStepAway1S(b,sofar,ns,loc,[=,&num] (move_t sofar, int ns) {APPENDMV(pushmove);});
  return num;
}

//Generates pushpulls for [oloc] from a pla piece at [loc] that may or may not be there yet.
//[f] should be genPPPO2S specialized to [loc] pushpulling [oloc]
template <typename Func>
static void genPPFrom3S(Board& b, move_t sofar, int ns, move_t* mv, int& num, loc_t loc, loc_t oloc, Func& f)
{
  pla_t pla = b.player;
  if(ISE(loc))
    iterTPlaceGtToUF1S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f);
  else if(ISP(loc))
  {
    if(!GT(loc,oloc)) return;
    if(b.isThawedC(loc)) {
      //genPPPO2S(b,sofar,ns); //uncomment this to turn into genPPFrom3L
      num += genUnblockPushPO3S(b,sofar,ns,mv+num,loc,oloc);
      num += genUnblockPullPO3S(b,sofar,ns,mv+num,loc,oloc);
      return;
    }
    iterTDefend1S(b,sofar,ns,loc,loc,f);
  }
}

//Generate pushes of [oloc] in 3 steps where the first move is a step of a piece closer
static int genStepAndPush3S(Board& b, move_t sofar, int ns, move_t* mv, loc_t oloc)
{
  int num = 0;
  int fromloc = ERRLOC;
  auto func = [=,&b,&num,&fromloc] (move_t sofar, int ns) {num += genPushesPO2S(b,sofar,ns,mv+num,fromloc,oloc);};
  fromloc = oloc S; if(ISE(fromloc)) iterTPlaceGtToUF1S(b,sofar,ns,fromloc,ERRLOC,b.pieces[oloc],func);
  fromloc = oloc W; if(ISE(fromloc)) iterTPlaceGtToUF1S(b,sofar,ns,fromloc,ERRLOC,b.pieces[oloc],func);
  fromloc = oloc E; if(ISE(fromloc)) iterTPlaceGtToUF1S(b,sofar,ns,fromloc,ERRLOC,b.pieces[oloc],func);
  fromloc = oloc N; if(ISE(fromloc)) iterTPlaceGtToUF1S(b,sofar,ns,fromloc,ERRLOC,b.pieces[oloc],func);
  return num;
}

//Generate pushpulls of [oloc] in 3 steps
static int genPP3S(Board& b, move_t sofar, int ns, move_t* mv, loc_t oloc)
{
  int num = 0;
  int fromloc = ERRLOC;
  auto func = [=,&b,&num,&fromloc] (move_t sofar, int ns) {num += genPPPO2S(b,sofar,ns,mv+num,fromloc,oloc);};

  //Iterate over ways of establishing an unfrozen piece >= [piece] at [loc] in <= 1 step,
  //ignoring steps by [ign] in the case where the square is empty
  fromloc = oloc S; genPPFrom3S(b,sofar,ns,mv,num,fromloc,oloc,func);
  fromloc = oloc W; genPPFrom3S(b,sofar,ns,mv,num,fromloc,oloc,func);
  fromloc = oloc E; genPPFrom3S(b,sofar,ns,mv,num,fromloc,oloc,func);
  fromloc = oloc N; genPPFrom3S(b,sofar,ns,mv,num,fromloc,oloc,func);
  return num;
}

//Generates pushpulls for [oloc] from a pla piece at [loc] that may or may not be there yet to [toLoc]
static void genPushFromTo3S(Board& b, move_t sofar, int ns, move_t* mv, int& num, loc_t loc, loc_t oloc, loc_t toLoc, move_t pushmove)
{
  pla_t pla = b.player;
  auto f = [=,&b,&num] (move_t sofar, int ns) {if(ISE(toLoc)) {APPENDMV(pushmove);}};
  if(ISE(loc))
    iterTPlaceGtToUF1S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f);
  else if(ISP(loc))
  {
    if(!GT(loc,oloc)) return;
    if(b.isThawedC(loc)) {
      num += genUnblockPushTo3S(b,sofar,ns,mv+num,toLoc,pushmove);
      return;
    }
    iterTDefend1S(b,sofar,ns,loc,loc,f);
  }
}

//Generates pushpulls for [oloc] from a pla piece at [loc] that may or may not be there yet
static void genPullFrom3S(Board& b, move_t sofar, int ns, move_t* mv, int& num, loc_t loc, loc_t oloc)
{
  pla_t pla = b.player;
  auto f = [=,&b,&num] (move_t sofar, int ns) {num += genPullsPO2S(b,sofar,ns,mv+num,loc,oloc);};
  if(ISE(loc))
    iterTPlaceGtToUF1S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f);
  else if(ISP(loc))
  {
    if(GT(loc,oloc))
    {
      if(b.isThawedC(loc))
        num += genUnblockPullPO3S(b,sofar,ns,mv+num,loc,oloc);
      else
        iterTDefend1S(b,sofar,ns,loc,loc,f);
    }
  }
}

//Generate pushpulls of [oloc] to [loc] in 3 steps
static int genPPTo3S(Board& b, move_t sofar, int ns, move_t* mv, loc_t oloc, loc_t loc)
{
  int num = 0;

  //Push
  step_t ostep = gStepSrcDest(oloc,loc);
  if(oloc S != loc) genPushFromTo3S(b,sofar,ns,mv,num,oloc S,oloc,loc,getMove(ostep,oloc S MN));
  if(oloc W != loc) genPushFromTo3S(b,sofar,ns,mv,num,oloc W,oloc,loc,getMove(ostep,oloc W ME));
  if(oloc E != loc) genPushFromTo3S(b,sofar,ns,mv,num,oloc E,oloc,loc,getMove(ostep,oloc E MW));
  if(oloc N != loc) genPushFromTo3S(b,sofar,ns,mv,num,oloc N,oloc,loc,getMove(ostep,oloc N MS));

  //Pull
  genPullFrom3S(b,sofar,ns,mv,num,loc,oloc);
  return num;
}

//Generate pushpulls of [ploc] on [oloc] that require unblocking the push dest, in 4 steps, assuming
//both exist and [ploc] is unfrozen and gt [oloc]
//NOT complete - doesn't generate sacrificial unblocks of a square
static int genUnblockPushPO4S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t oloc)
{
  int num = 0;
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  step_t pstep = gStepSrcDest(ploc,oloc);
  //It's possible the act of double-stepping away could sac ploc, so we carefully check for this.
  if(oloc S != ploc && ISP(oloc S)) {iterTStepAway2S(b,sofar,ns,oloc S,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc)) APPENDMV(getMove(oloc MS,pstep));});}
  if(oloc W != ploc && ISP(oloc W)) {iterTStepAway2S(b,sofar,ns,oloc W,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc)) APPENDMV(getMove(oloc MW,pstep));});}
  if(oloc E != ploc && ISP(oloc E)) {iterTStepAway2S(b,sofar,ns,oloc E,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc)) APPENDMV(getMove(oloc ME,pstep));});}
  if(oloc N != ploc && ISP(oloc N)) {iterTStepAway2S(b,sofar,ns,oloc N,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc)) APPENDMV(getMove(oloc MN,pstep));});}

  if(ISO(oloc S)) iterTPull2S(b,sofar,ns,oloc S,[=,&b,&num](move_t sofar, int ns){if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(oloc MS,pstep));});
  if(ISO(oloc W)) iterTPull2S(b,sofar,ns,oloc W,[=,&b,&num](move_t sofar, int ns){if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(oloc MW,pstep));});
  if(ISO(oloc E)) iterTPull2S(b,sofar,ns,oloc E,[=,&b,&num](move_t sofar, int ns){if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(oloc ME,pstep));});
  if(ISO(oloc N)) iterTPull2S(b,sofar,ns,oloc N,[=,&b,&num](move_t sofar, int ns){if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(oloc MN,pstep));});
  return num;
}

//Generate pushpulls of [ploc] on [oloc] that require unblocking the pull dest, in 4 steps, assuming
//both exist and [ploc] is unfrozen and gt [oloc]
static int genUnblockPullPO4S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t oloc)
{
  int num = 0;
  pla_t pla = b.player;
  step_t ostep = gStepSrcDest(oloc,ploc);
  if(b.isTrapSafe2(pla,ploc))
  {
    //Standard 2-step unblock
    if(ISP(ploc S)) {iterTStepAway2S(b,sofar,ns,ploc S,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(ploc MS,ostep));});}
    if(ISP(ploc W)) {iterTStepAway2S(b,sofar,ns,ploc W,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(ploc MW,ostep));});}
    if(ISP(ploc E)) {iterTStepAway2S(b,sofar,ns,ploc E,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(ploc ME,ostep));});}
    if(ISP(ploc N)) {iterTStepAway2S(b,sofar,ns,ploc N,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(getMove(ploc MN,ostep));});}

    //Unblock - unfreeze - pull
    if(b.isDominatedC(ploc) && !b.isGuarded2(pla,ploc))
    {
      if(ISP(ploc S)) {iterTStepAway1S(b,sofar,ns,ploc S,[=,&b,&num] (move_t sofar, int ns) {iterTDefend1S(b,sofar,ns,ploc,ploc S,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc MS,ostep));});});}
      if(ISP(ploc W)) {iterTStepAway1S(b,sofar,ns,ploc W,[=,&b,&num] (move_t sofar, int ns) {iterTDefend1S(b,sofar,ns,ploc,ploc W,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc MW,ostep));});});}
      if(ISP(ploc E)) {iterTStepAway1S(b,sofar,ns,ploc E,[=,&b,&num] (move_t sofar, int ns) {iterTDefend1S(b,sofar,ns,ploc,ploc E,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc ME,ostep));});});}
      if(ISP(ploc N)) {iterTStepAway1S(b,sofar,ns,ploc N,[=,&b,&num] (move_t sofar, int ns) {iterTDefend1S(b,sofar,ns,ploc,ploc N,[=,&num] (move_t sofar, int ns) {APPENDMV(getMove(ploc MN,ostep));});});}
    }
  }
  else
  {
    //Pre-defend trap - unblock - pull
    loc_t guardLoc = b.findGuard(pla,ploc);
    iterTDefend1S(b,sofar,ns,ploc,ploc,[=,&b,&num](move_t sofar, int ns) {iterSStepAway1S(b,sofar,ns,guardLoc,[=,&num](move_t sofar, int ns) {APPENDMV(getMove(gStepSrcDest(ploc,guardLoc),ostep))});});
  }
  return num;
}

//Generate pushpulls of [ploc] on [oloc] to [loc] that require unblocking [loc], in 4 steps, assuming
//both exist and [ploc] is unfrozen and gt [oloc]
//NOT complete - doesn't generate sacrificial unblocks of a square
static int genUnblockPushTo4S(Board& b, move_t sofar, int ns, move_t* mv, loc_t ploc, loc_t loc, move_t pushmove)
{
  int num = 0;
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  //It's possible the act of double-stepping away could sac ploc, so we carefully check for this.
  if(ISP(loc)) {iterTStepAway2S(b,sofar,ns,loc,ploc,[=,&b,&num] (move_t sofar, int ns) {if(ISP(ploc)) APPENDMV(pushmove);});}
  else if(ISO(loc)) iterTPull2S(b,sofar,ns,loc,[=,&b,&num](move_t sofar, int ns){if(ISP(ploc) && b.isThawedC(ploc)) APPENDMV(pushmove);});
  return num;
}

//Generates pushpulls for [oloc] from a pla piece at [loc] that may or may not be there yet.
//[f] should be genPPPO2S specialized to [loc] pushpulling [oloc]
//[f2] should be genUnblockPushPO3S specialized to [loc] pushing [oloc]
template <typename Func1, typename Func2>
static void genPPFrom4S(Board& b, move_t sofar, int ns, move_t* mv, int& num, loc_t loc, loc_t oloc, Func1& f, Func2& f2)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  if(ISE(loc))
  {
    //Attack in 1 step and unblock the push dest
    iterTPlaceGtToUF1S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f2);
    //Attack in 2 steps
    iterTPlaceGtToNRUF2S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f);
  }
  else if(ISP(loc))
  {
    if(GT(loc,oloc))
    {
      if(b.isFrozenC(loc))
      {
        //Unfreeze in 1 step and unblock the push dest
        iterTDefend1S(b,sofar,ns,loc,oloc,f2);

        //Unfreeze with defender
        iterTDefend2S(b,sofar,ns,loc,oloc,f);

        //Pull dominator
        loc_t domLoc = b.findSingleDominator(loc);
        if(domLoc != ERRLOC)
        {
          if(ISP(domLoc S) && GT(domLoc S,domLoc) && b.isThawedC(domLoc S)) iterTPullPO2S(b,sofar,ns,domLoc S,domLoc,f);
          if(ISP(domLoc W) && GT(domLoc W,domLoc) && b.isThawedC(domLoc W)) iterTPullPO2S(b,sofar,ns,domLoc W,domLoc,f);
          if(ISP(domLoc E) && GT(domLoc E,domLoc) && b.isThawedC(domLoc E)) iterTPullPO2S(b,sofar,ns,domLoc E,domLoc,f);
          if(ISP(domLoc N) && GT(domLoc N,domLoc) && b.isThawedC(domLoc N)) iterTPullPO2S(b,sofar,ns,domLoc N,domLoc,f);
        }
      }
      else
      {
        num += genUnblockPushPO4S(b,sofar,ns,mv+num,loc,oloc);
        num += genUnblockPullPO4S(b,sofar,ns,mv+num,loc,oloc);
      }
    }
    else
    {
      //Swap for a stronger piece
      if(ISP(loc S) && GT(loc S,oloc) && b.isTrapSafe2(pla,loc S) && b.wouldBeUF(pla,loc S,loc S,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc S,loc,loc S)) {TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))}});
      if(ISP(loc W) && GT(loc W,oloc) && b.isTrapSafe2(pla,loc W) && b.wouldBeUF(pla,loc W,loc W,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc W,loc,loc W)) {TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))}});
      if(ISP(loc E) && GT(loc E,oloc) && b.isTrapSafe2(pla,loc E) && b.wouldBeUF(pla,loc E,loc E,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc E,loc,loc E)) {TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))}});
      if(ISP(loc N) && GT(loc N,oloc) && b.isTrapSafe2(pla,loc N) && b.wouldBeUF(pla,loc N,loc N,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc N,loc,loc N)) {TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))}});
    }
  }
  else if(ISO(loc))
  {
    //Push into the square
    if(b.isTrapSafe2(pla,loc))
    {
      if(ISP(loc S) && GT(loc S,loc) && GT(loc S,oloc) && b.isThawedC(loc S) && b.wouldBeUF(pla,loc S,loc,loc S)) iterTPushPO2S(b,sofar,ns,loc S,loc,f);
      if(ISP(loc W) && GT(loc W,loc) && GT(loc W,oloc) && b.isThawedC(loc W) && b.wouldBeUF(pla,loc W,loc,loc W)) iterTPushPO2S(b,sofar,ns,loc W,loc,f);
      if(ISP(loc E) && GT(loc E,loc) && GT(loc E,oloc) && b.isThawedC(loc E) && b.wouldBeUF(pla,loc E,loc,loc E)) iterTPushPO2S(b,sofar,ns,loc E,loc,f);
      if(ISP(loc N) && GT(loc N,loc) && GT(loc N,oloc) && b.isThawedC(loc N) && b.wouldBeUF(pla,loc N,loc,loc N)) iterTPushPO2S(b,sofar,ns,loc N,loc,f);
    }
  }
}

//Generate pushpulls of [oloc] in 4 steps
static int genPP4S(Board& b, move_t sofar, int ns, move_t* mv, loc_t oloc)
{
  int num = 0;
  int fromloc = ERRLOC;
  auto func = [=,&b,&num,&fromloc] (move_t sofar, int ns) {num += genPPPO2S(b,sofar,ns,mv+num,fromloc,oloc);};
  auto func2 = [=,&b,&num,&fromloc] (move_t sofar, int ns) {num += genUnblockPushPO3S(b,sofar,ns,mv+num,fromloc,oloc);};

  //Iterate over ways of establishing an unfrozen piece >= [piece] at [loc] in <= 2 step,
  //ignoring steps by [ign] in the case where the square is empty
  fromloc = oloc S; genPPFrom4S(b,sofar,ns,mv,num,fromloc,oloc,func,func2);
  fromloc = oloc W; genPPFrom4S(b,sofar,ns,mv,num,fromloc,oloc,func,func2);
  fromloc = oloc E; genPPFrom4S(b,sofar,ns,mv,num,fromloc,oloc,func,func2);
  fromloc = oloc N; genPPFrom4S(b,sofar,ns,mv,num,fromloc,oloc,func,func2);
  return num;
}


//Generates pushes for [oloc] from a pla piece at [loc] that may or may not be there yet to [toLoc]
//[pushmove] should be the pushmove loc -> oloc -> toLoc
static void genPushFromTo4S(Board& b, move_t sofar, int ns, move_t* mv, int& num, loc_t loc, loc_t oloc, loc_t toLoc, move_t pushmove)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  auto f = [=,&b,&num] (move_t sofar, int ns) {if(ISE(toLoc)) {APPENDMV(pushmove);}};
  auto f2 = [=,&b,&num] (move_t sofar, int ns) {num += genUnblockPushTo3S(b,sofar,ns,mv+num,toLoc,pushmove);};
  if(ISE(loc))
  {
    //Attack in 1 step and unblock the push dest
    iterTPlaceGtToUF1S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f2);
    //Attack in 2 steps
    iterTPlaceGtToNRUF2S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f);
  }
  else if(ISP(loc))
  {
    if(GT(loc,oloc))
    {
      if(b.isFrozenC(loc))
      {
        //Unfreeze in 1 step and unblock the push dest
        iterTDefend1S(b,sofar,ns,loc,oloc,f2);

        //Unfreeze with defender
        iterTDefend2S(b,sofar,ns,loc,oloc,f);

        //Pull dominator
        loc_t domLoc = b.findSingleDominator(loc);
        if(domLoc != ERRLOC)
        {
          if(ISP(domLoc S) && GT(domLoc S,domLoc) && b.isThawedC(domLoc S)) iterTPullPO2S(b,sofar,ns,domLoc S,domLoc,f);
          if(ISP(domLoc W) && GT(domLoc W,domLoc) && b.isThawedC(domLoc W)) iterTPullPO2S(b,sofar,ns,domLoc W,domLoc,f);
          if(ISP(domLoc E) && GT(domLoc E,domLoc) && b.isThawedC(domLoc E)) iterTPullPO2S(b,sofar,ns,domLoc E,domLoc,f);
          if(ISP(domLoc N) && GT(domLoc N,domLoc) && b.isThawedC(domLoc N)) iterTPullPO2S(b,sofar,ns,domLoc N,domLoc,f);
        }
      }
      else
      {
        num += genUnblockPushTo4S(b,sofar,ns,mv+num,loc,toLoc,pushmove);
      }
    }
    else
    {
      //Swap for a stronger piece
      if(ISP(loc S) && GT(loc S,oloc) && b.isTrapSafe2(pla,loc S) && b.wouldBeUF(pla,loc S,loc S,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc S,loc,loc S)) {TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))}});
      if(ISP(loc W) && GT(loc W,oloc) && b.isTrapSafe2(pla,loc W) && b.wouldBeUF(pla,loc W,loc W,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc W,loc,loc W)) {TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))}});
      if(ISP(loc E) && GT(loc E,oloc) && b.isTrapSafe2(pla,loc E) && b.wouldBeUF(pla,loc E,loc E,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc E,loc,loc E)) {TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))}});
      if(ISP(loc N) && GT(loc N,oloc) && b.isTrapSafe2(pla,loc N) && b.wouldBeUF(pla,loc N,loc N,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc N,loc,loc N)) {TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))}});
    }
  }
  else if(ISO(loc))
  {
    //Push into the square
    if(b.isTrapSafe2(pla,loc))
    {
      if(ISP(loc S) && GT(loc S,loc) && GT(loc S,oloc) && b.isThawedC(loc S) && b.wouldBeUF(pla,loc S,loc,loc S)) iterTPushPO2S(b,sofar,ns,loc S,loc,f);
      if(ISP(loc W) && GT(loc W,loc) && GT(loc W,oloc) && b.isThawedC(loc W) && b.wouldBeUF(pla,loc W,loc,loc W)) iterTPushPO2S(b,sofar,ns,loc W,loc,f);
      if(ISP(loc E) && GT(loc E,loc) && GT(loc E,oloc) && b.isThawedC(loc E) && b.wouldBeUF(pla,loc E,loc,loc E)) iterTPushPO2S(b,sofar,ns,loc E,loc,f);
      if(ISP(loc N) && GT(loc N,loc) && GT(loc N,oloc) && b.isThawedC(loc N) && b.wouldBeUF(pla,loc N,loc,loc N)) iterTPushPO2S(b,sofar,ns,loc N,loc,f);
    }
  }
}

//Generates pushpulls for [oloc] from a pla piece at [loc] that may or may not be there yet.
//[f] should be genPPPO2S specialized to [loc] pushpulling [oloc]
//[f2] should be genUnblockPushPO3S specialized to [loc] pushing [oloc]
static void genPullFrom4S(Board& b, move_t sofar, int ns, move_t* mv, int& num, loc_t loc, loc_t oloc)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  auto f = [=,&b,&num] (move_t sofar, int ns) {num += genPullsPO2S(b,sofar,ns,mv+num,loc,oloc);};
  if(ISE(loc))
  {
    //Attack in 2 steps
    iterTPlaceGtToNRUF2S(b,sofar,ns,loc,ERRLOC,b.pieces[oloc],f);
  }
  else if(ISP(loc))
  {
    if(GT(loc,oloc))
    {
      if(b.isFrozenC(loc))
      {
        //Unfreeze with defender
        iterTDefend2S(b,sofar,ns,loc,oloc,f);

        //Pull dominator
        loc_t domLoc = b.findSingleDominator(loc);
        if(domLoc != ERRLOC)
        {
          if(ISP(domLoc S) && GT(domLoc S,domLoc) && b.isThawedC(domLoc S)) iterTPullPO2S(b,sofar,ns,domLoc S,domLoc,f);
          if(ISP(domLoc W) && GT(domLoc W,domLoc) && b.isThawedC(domLoc W)) iterTPullPO2S(b,sofar,ns,domLoc W,domLoc,f);
          if(ISP(domLoc E) && GT(domLoc E,domLoc) && b.isThawedC(domLoc E)) iterTPullPO2S(b,sofar,ns,domLoc E,domLoc,f);
          if(ISP(domLoc N) && GT(domLoc N,domLoc) && b.isThawedC(domLoc N)) iterTPullPO2S(b,sofar,ns,domLoc N,domLoc,f);
        }
      }
      else
      {
        num += genUnblockPullPO4S(b,sofar,ns,mv+num,loc,oloc);
      }
    }
    else
    {
      //Swap for a stronger piece
      if(ISP(loc S) && GT(loc S,oloc) && b.isTrapSafe2(pla,loc S) && b.wouldBeUF(pla,loc S,loc S,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc S,loc,loc S)) {TSC(loc S,loc,APPENDCALL(1,getMove(loc S MN)))}});
      if(ISP(loc W) && GT(loc W,oloc) && b.isTrapSafe2(pla,loc W) && b.wouldBeUF(pla,loc W,loc W,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc W,loc,loc W)) {TSC(loc W,loc,APPENDCALL(1,getMove(loc W ME)))}});
      if(ISP(loc E) && GT(loc E,oloc) && b.isTrapSafe2(pla,loc E) && b.wouldBeUF(pla,loc E,loc E,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc E,loc,loc E)) {TSC(loc E,loc,APPENDCALL(1,getMove(loc E MW)))}});
      if(ISP(loc N) && GT(loc N,oloc) && b.isTrapSafe2(pla,loc N) && b.wouldBeUF(pla,loc N,loc N,loc)) iterTStepAway1S(b,sofar,ns,loc,[=,&b](move_t sofar, int ns){if(b.wouldBeUF(pla,loc N,loc,loc N)) {TSC(loc N,loc,APPENDCALL(1,getMove(loc N MS)))}});
    }
  }
  else if(ISO(loc))
  {
    //Push into the square
    if(b.isTrapSafe2(pla,loc))
    {
      if(ISP(loc S) && GT(loc S,loc) && GT(loc S,oloc) && b.isThawedC(loc S) && b.wouldBeUF(pla,loc S,loc,loc S)) iterTPushPO2S(b,sofar,ns,loc S,loc,f);
      if(ISP(loc W) && GT(loc W,loc) && GT(loc W,oloc) && b.isThawedC(loc W) && b.wouldBeUF(pla,loc W,loc,loc W)) iterTPushPO2S(b,sofar,ns,loc W,loc,f);
      if(ISP(loc E) && GT(loc E,loc) && GT(loc E,oloc) && b.isThawedC(loc E) && b.wouldBeUF(pla,loc E,loc,loc E)) iterTPushPO2S(b,sofar,ns,loc E,loc,f);
      if(ISP(loc N) && GT(loc N,loc) && GT(loc N,oloc) && b.isThawedC(loc N) && b.wouldBeUF(pla,loc N,loc,loc N)) iterTPushPO2S(b,sofar,ns,loc N,loc,f);
    }
  }
}

//Generate pushpulls of [oloc] to [loc] in 3 steps
static int genPPTo4S(Board& b, move_t sofar, int ns, move_t* mv, loc_t oloc, loc_t loc)
{
  int num = 0;

  //Push
  step_t ostep = gStepSrcDest(oloc,loc);
  if(oloc S != loc) genPushFromTo4S(b,sofar,ns,mv,num,oloc S,oloc,loc,getMove(ostep,oloc S MN));
  if(oloc W != loc) genPushFromTo4S(b,sofar,ns,mv,num,oloc W,oloc,loc,getMove(ostep,oloc W ME));
  if(oloc E != loc) genPushFromTo4S(b,sofar,ns,mv,num,oloc E,oloc,loc,getMove(ostep,oloc E MW));
  if(oloc N != loc) genPushFromTo4S(b,sofar,ns,mv,num,oloc N,oloc,loc,getMove(ostep,oloc N MS));

  //Pull
  genPullFrom4S(b,sofar,ns,mv,num,loc,oloc);
  return num;
}

//----------------------------------------------------------------------------------------------

int BoardTrees::genPPTo2Step(Board& b, move_t* mv, loc_t oloc, loc_t loc)
{
  int num = 0;
  pla_t pla = b.player;
  if(ISE(loc))
    num += genPushTo2S(b,ERRMOVE,0,mv+num,oloc,loc);
  else if(ISP(loc) && GT(loc,oloc) && b.isThawed(loc))
    num += genPullsPO2S(b,ERRMOVE,0,mv+num,loc,oloc);
  return num;
}

int BoardTrees::genPPTo3Step(Board& b, move_t* mv, loc_t oloc, loc_t loc)
{
  return genPPTo3S(b,ERRMOVE,0,mv,oloc,loc);
}

int BoardTrees::genPPTo4Step(Board& b, move_t* mv, loc_t oloc, loc_t loc)
{
  return genPPTo4S(b,ERRMOVE,0,mv,oloc,loc);
}

int BoardTrees::genPP3Step(Board& b, move_t* mv, loc_t oloc)
{
  return genPP3S(b,ERRMOVE,0,mv,oloc);
}

int BoardTrees::genPP4Step(Board& b, move_t* mv, loc_t oloc)
{
  return genPP4S(b,ERRMOVE,0,mv,oloc);
}

int BoardTrees::genStepAndPush3Step(Board& b, move_t* mv, loc_t oloc)
{
  return genStepAndPush3S(b,ERRMOVE,0,mv,oloc);
}

//---------------------------------------------------------------------------------

//Generates steps of pieces >= [piece] to [loc], except for pieces from [ign]
int BoardTrees::genStepGeqTo1S(Board& b, move_t* mv, loc_t loc, loc_t ign, piece_t piece)
{
  int num = 0;
  pla_t pla = b.player;
  if(loc S != ign && ISP(loc S) && GEQP(loc S,piece) && b.isThawedC(loc S) && b.isRabOkayN(pla,loc S)) {mv[num++] = getMove(loc S MN);}
  if(loc W != ign && ISP(loc W) && GEQP(loc W,piece) && b.isThawedC(loc W))                            {mv[num++] = getMove(loc W ME);}
  if(loc E != ign && ISP(loc E) && GEQP(loc E,piece) && b.isThawedC(loc E))                            {mv[num++] = getMove(loc E MW);}
  if(loc N != ign && ISP(loc N) && GEQP(loc N,piece) && b.isThawedC(loc N) && b.isRabOkayS(pla,loc N)) {mv[num++] = getMove(loc N MS);}
  return num;
}

//Generates steps of pieces >= [piece] to squares adjacent to [loc], except not to location [ign]
//Excludes any piece at [loc] itself from stepping.
int BoardTrees::genStepGeqAround1S(Board& b, move_t* mv, loc_t loc, loc_t ign, piece_t piece)
{
  int num = 0;
  if(loc S != ign && ISE(loc S)) num += genStepGeqTo1S(b,mv+num,loc S,loc,piece);
  if(loc W != ign && ISE(loc W)) num += genStepGeqTo1S(b,mv+num,loc W,loc,piece);
  if(loc E != ign && ISE(loc E)) num += genStepGeqTo1S(b,mv+num,loc E,loc,piece);
  if(loc N != ign && ISE(loc N)) num += genStepGeqTo1S(b,mv+num,loc N,loc,piece);
  return num;
}

//Generates steps of pieces to [loc] followed by steps of pieces into the square just vacated
//Assumes [loc] is empty
int BoardTrees::genStuffings2S(Board& b, move_t* mv, pla_t pla, loc_t loc)
{
  int num = 0;
  if(ISP(loc S) && b.isRabOkayN(pla,loc S))
  {
    if(ISP(loc SS) && b.wouldBeUF(pla,loc SS,loc SS,loc S) && b.isTrapSafe2(pla,loc SS) && b.isRabOkayN(pla,loc SS)) {mv[num++] = getMove(loc S MN,loc SS MN);}
    if(ISP(loc SW) && b.wouldBeUF(pla,loc SW,loc SW,loc S) && b.isTrapSafe2(pla,loc SW))                             {mv[num++] = getMove(loc S MN,loc SW ME);}
    if(ISP(loc SE) && b.wouldBeUF(pla,loc SE,loc SE,loc S) && b.isTrapSafe2(pla,loc SE))                             {mv[num++] = getMove(loc S MN,loc SE MW);}
  }
  if(ISP(loc W))
  {
    if(ISP(loc SW) && b.wouldBeUF(pla,loc SW,loc SW,loc W) && b.isTrapSafe2(pla,loc SW) && b.isRabOkayN(pla,loc SW)) {mv[num++] = getMove(loc W ME,loc SW MN);}
    if(ISP(loc WW) && b.wouldBeUF(pla,loc WW,loc WW,loc W) && b.isTrapSafe2(pla,loc WW))                             {mv[num++] = getMove(loc W ME,loc WW ME);}
    if(ISP(loc NW) && b.wouldBeUF(pla,loc NW,loc NW,loc W) && b.isTrapSafe2(pla,loc NW) && b.isRabOkayS(pla,loc NW)) {mv[num++] = getMove(loc W ME,loc NW MS);}
  }
  if(ISP(loc E))
  {
    if(ISP(loc SE) && b.wouldBeUF(pla,loc SE,loc SE,loc E) && b.isTrapSafe2(pla,loc SE) && b.isRabOkayN(pla,loc SE)) {mv[num++] = getMove(loc E MW,loc SE MN);}
    if(ISP(loc EE) && b.wouldBeUF(pla,loc EE,loc EE,loc E) && b.isTrapSafe2(pla,loc EE))                             {mv[num++] = getMove(loc E MW,loc EE MW);}
    if(ISP(loc NE) && b.wouldBeUF(pla,loc NE,loc NE,loc E) && b.isTrapSafe2(pla,loc NE) && b.isRabOkayS(pla,loc NE)) {mv[num++] = getMove(loc E MW,loc NE MS);}
  }
  if(ISP(loc N) && b.isRabOkayS(pla,loc N))
  {
    if(ISP(loc NW) && b.wouldBeUF(pla,loc NW,loc NW,loc N) && b.isTrapSafe2(pla,loc NW))                             {mv[num++] = getMove(loc N MS,loc NW ME);}
    if(ISP(loc NE) && b.wouldBeUF(pla,loc NE,loc NE,loc N) && b.isTrapSafe2(pla,loc NE))                             {mv[num++] = getMove(loc N MS,loc NE MW);}
    if(ISP(loc NN) && b.wouldBeUF(pla,loc NN,loc NN,loc N) && b.isTrapSafe2(pla,loc NN) && b.isRabOkayS(pla,loc NN)) {mv[num++] = getMove(loc N MS,loc NN MS);}
  }
  return num;
}


