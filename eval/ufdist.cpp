/*
 * ufdist.cpp
 * Author: davidwu
 */

#include <cmath>
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardtreeconst.h"
#include "../eval/internal.h"

//Need to add an extra defender.
//Badloc indicates a square where a piece there is no help.
static int UF_NEED_DEFENDER = 1;
//Piece is otherwise thawed, but needs to open a space to actually move.
//Badloc indicate a square where opening it is no help.
static int UF_NEED_SPACE = 2;
//Piece needs both a new open space and a new defender
static int UF_NEED_SPACE_AND_DEFENDER = 3;

//Extra cost if no good squares
static const int IMMOFROZENCOST_N = 2;

//Obtains the status of a frozen piece
static int getFrozenUFStatus(const Board& b, pla_t pla, loc_t ploc, loc_t& badloc)
{
  badloc = ERRLOC;

  //Check if there's anywhere we could step or push out to if we were unfrozen.
  pla_t opp = gOpp(pla);
  loc_t escapeloc = ERRLOC;
  if(b.isRabOkayS(pla,ploc) && (b.owners[ploc S] == NPLA || (b.owners[ploc S] == opp && ((b.pieces[ploc S] < b.pieces[ploc] && b.isOpen(ploc S)) || b.isDominatedByUF(ploc S)))) && b.isTrapSafe2(pla,ploc S)) {if(escapeloc != ERRLOC) return UF_NEED_DEFENDER; else escapeloc = ploc S;}
  if(                          (b.owners[ploc W] == NPLA || (b.owners[ploc W] == opp && ((b.pieces[ploc W] < b.pieces[ploc] && b.isOpen(ploc W)) || b.isDominatedByUF(ploc W)))) && b.isTrapSafe2(pla,ploc W)) {if(escapeloc != ERRLOC) return UF_NEED_DEFENDER; else escapeloc = ploc W;}
  if(                          (b.owners[ploc E] == NPLA || (b.owners[ploc E] == opp && ((b.pieces[ploc E] < b.pieces[ploc] && b.isOpen(ploc E)) || b.isDominatedByUF(ploc E)))) && b.isTrapSafe2(pla,ploc E)) {if(escapeloc != ERRLOC) return UF_NEED_DEFENDER; else escapeloc = ploc E;}
  if(b.isRabOkayN(pla,ploc) && (b.owners[ploc N] == NPLA || (b.owners[ploc N] == opp && ((b.pieces[ploc N] < b.pieces[ploc] && b.isOpen(ploc N)) || b.isDominatedByUF(ploc N)))) && b.isTrapSafe2(pla,ploc N)) {if(escapeloc != ERRLOC) return UF_NEED_DEFENDER; else escapeloc = ploc N;}

  //Nowhere we can even escape to - we are hosed.
  if(escapeloc == ERRLOC)
    return UF_NEED_SPACE_AND_DEFENDER;

  //Only one space to escape to, so we can't UF from here.
  badloc = escapeloc;
  return UF_NEED_DEFENDER;
}

//TODO this can omit some of the checks because it's only called in cases where via bitmaps we've already checked most
//of the conditions
//Check if a piece is immobilized just by blockading. Includes blocking in by self pieces, but only 2 or more layers deep
static bool isImmo(const Board& b, pla_t pla, loc_t ploc)
{
  pla_t opp = gOpp(pla);
  return
  !(
    (b.isRabOkayS(pla,ploc) && (b.owners[ploc S] == NPLA || (b.owners[ploc S] == pla && b.isOpenToStep(ploc S) && b.wouldBeUF(pla,ploc,ploc,ploc S)) || (b.owners[ploc S] == opp && b.isOpen(ploc S) && b.pieces[ploc S] < b.pieces[ploc])) && b.isTrapSafe2(pla,ploc S)) ||
    (                          (b.owners[ploc W] == NPLA || (b.owners[ploc W] == pla && b.isOpenToStep(ploc W) && b.wouldBeUF(pla,ploc,ploc,ploc W)) || (b.owners[ploc W] == opp && b.isOpen(ploc W) && b.pieces[ploc W] < b.pieces[ploc])) && b.isTrapSafe2(pla,ploc W)) ||
    (                          (b.owners[ploc E] == NPLA || (b.owners[ploc E] == pla && b.isOpenToStep(ploc E) && b.wouldBeUF(pla,ploc,ploc,ploc E)) || (b.owners[ploc E] == opp && b.isOpen(ploc E) && b.pieces[ploc E] < b.pieces[ploc])) && b.isTrapSafe2(pla,ploc E)) ||
    (b.isRabOkayN(pla,ploc) && (b.owners[ploc N] == NPLA || (b.owners[ploc N] == pla && b.isOpenToStep(ploc N) && b.wouldBeUF(pla,ploc,ploc,ploc N)) || (b.owners[ploc N] == opp && b.isOpen(ploc N) && b.pieces[ploc N] < b.pieces[ploc])) && b.isTrapSafe2(pla,ploc N))
  );
}

//Obtains the status of a thawed but immo piece
static loc_t getThawedImmoUFBadLoc(const Board& b, pla_t pla, loc_t ploc)
{
  loc_t badloc = ERRLOC;

  //If nothing else happens, the only possible problem is rabbits not being able to use a cleared space behind them
  if(b.pieces[ploc] == RAB)
    badloc = (pla == GOLD && CS1(ploc)) ? ploc S : (pla == SILV && CN1(ploc)) ? ploc N : ERRLOC;

  //If not dominated, then that's the only problem
  if(!b.isDominated(ploc))
    return badloc;

  //TODO what if it's a badloc both to move that defender and rabbit can't use cleared space behind?
  //Otherwise, dominated, so if there's only a single defender, it's a badloc to move that defender.
  loc_t defloc = ERRLOC;
  if(b.owners[ploc S] == pla){if(defloc != ERRLOC) return badloc; else defloc = ploc S;}
  if(b.owners[ploc W] == pla){if(defloc != ERRLOC) return badloc; else defloc = ploc W;}
  if(b.owners[ploc E] == pla){if(defloc != ERRLOC) return badloc; else defloc = ploc E;}
  if(b.owners[ploc N] == pla){if(defloc != ERRLOC) return badloc; else defloc = ploc N;}

  return defloc;
}


//Compute best uf distance for pieces within radius 2.
//Min = min dist that we care about - return if equal or smaller
//best = best so far, return if it must be equal or larger.
//minNewUfDist - assume ufDists are >= this.
static int ufRad2(const Board& b, pla_t pla, loc_t ploc, const int ufDist[BSIZE], int min, int best, int minNewUfDist,
    loc_t badLoc, int ufStatus)
{
  if(ufStatus == UF_NEED_SPACE)
  {
    if(ploc S != badLoc && b.owners[ploc S] == pla && best > ufDist[ploc S]+1) {best = ufDist[ploc S]+1; if(best <= min) return best;}
    if(ploc W != badLoc && b.owners[ploc W] == pla && best > ufDist[ploc W]+1) {best = ufDist[ploc W]+1; if(best <= min) return best;}
    if(ploc E != badLoc && b.owners[ploc E] == pla && best > ufDist[ploc E]+1) {best = ufDist[ploc E]+1; if(best <= min) return best;}
    if(ploc N != badLoc && b.owners[ploc N] == pla && best > ufDist[ploc N]+1) {best = ufDist[ploc N]+1; if(best <= min) return best;}
  }

  Bitmap otherPlas = b.pieceMaps[pla][0] & ~Bitmap::makeLoc(ploc);
  if((otherPlas & Board::RADIUS[2][ploc]).isEmpty())
    return best;

  pla_t opp = gOpp(pla);
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][ploc])
      continue;
    loc_t loc = ploc + Board::ADJOFFSETS[i];
    if((otherPlas & Board::RADIUS[1][loc]).isEmpty())
      continue;
    int extra = loc == badLoc ? IMMOFROZENCOST_N + 1 : 1;
    if(b.owners[loc] == NPLA)
    {
      if(ufStatus == UF_NEED_SPACE_AND_DEFENDER)
        continue;
      if(ufStatus == UF_NEED_SPACE)
        extra++;
      if(minNewUfDist + extra >= best)
        continue;
      if(loc S != ploc && ISP(loc S) && b.isRabOkayN(pla,loc S)) {if(best > ufDist[loc S]+extra) {best = ufDist[loc S]+extra; if(best <= min) return best;}}
      if(loc W != ploc && ISP(loc W)                           ) {if(best > ufDist[loc W]+extra) {best = ufDist[loc W]+extra; if(best <= min) return best;}}
      if(loc E != ploc && ISP(loc E)                           ) {if(best > ufDist[loc E]+extra) {best = ufDist[loc E]+extra; if(best <= min) return best;}}
      if(loc N != ploc && ISP(loc N) && b.isRabOkayS(pla,loc N)) {if(best > ufDist[loc N]+extra) {best = ufDist[loc N]+extra; if(best <= min) return best;}}
    }
    else if(b.owners[loc] == opp)
    {
      extra++;
      if(!b.isOpen(loc) && ufStatus != UF_NEED_SPACE)
      {
        extra++;
        if(!b.isGuarded3(pla,loc))
          extra++;
      }

      if(minNewUfDist + extra >= best)
        continue;

      if(ISP(loc S) && GT(loc S, loc)) {if(best > ufDist[loc S]+extra) {best = ufDist[loc S]+extra; if(best <= min) return best;}}
      if(ISP(loc W) && GT(loc W, loc)) {if(best > ufDist[loc W]+extra) {best = ufDist[loc W]+extra; if(best <= min) return best;}}
      if(ISP(loc E) && GT(loc E, loc)) {if(best > ufDist[loc E]+extra) {best = ufDist[loc E]+extra; if(best <= min) return best;}}
      if(ISP(loc N) && GT(loc N, loc)) {if(best > ufDist[loc N]+extra) {best = ufDist[loc N]+extra; if(best <= min) return best;}}
    }
  }
  return best;
}

static int ufRad3(const Board& b, pla_t pla, loc_t ploc, const int ufDist[BSIZE], const Bitmap pStrongerMaps[2][NUMTYPES],
    int min, int best, int minNewUfDist, loc_t badLoc, int ufStatus)
{
  Bitmap otherPlas = b.pieceMaps[pla][0] & ~Bitmap::makeLoc(ploc);
  //We use disk because of possible paths like:
  //........
  //...12...
  //...pu...
  //........
  //u steps 2->1 to unfreeze p, and isn't unfreezing p right now because of some badloc or immofrozenness.
  if((otherPlas & Board::DISK[3][ploc]).isEmpty())
    return UFDist::MAX_UF_DIST;

  pla_t opp = gOpp(pla);
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][ploc])
      continue;
    loc_t loc = ploc + Board::ADJOFFSETS[i];
    if((otherPlas & Board::RADIUS[2][loc]).isEmpty())
      continue;
    int ifextra = loc == badLoc ? IMMOFROZENCOST_N : 0;

    if(b.owners[loc] == NPLA)
    {
      if(ufStatus == UF_NEED_SPACE_AND_DEFENDER)
        continue;
      if(ufStatus == UF_NEED_SPACE)
        ifextra++;

      for(int j = 0; j<4; j++)
      {
        if(!Board::ADJOKAY[j][loc])
          continue;
        loc_t loc2 = loc + Board::ADJOFFSETS[j];
        if(loc2 == ploc)
          continue;
        if((otherPlas & Board::RADIUS[1][loc2]).isEmpty())
          continue;

        if(b.owners[loc2] == NPLA)
        {
          int extra = 2;
          if(!b.isTrapSafe2(pla,loc2))
          {if(b.trapGuardCounts[pla][Board::TRAPINDEX[loc2]] == 0) {extra += 2;} else {extra += 1;}}
          if(minNewUfDist + extra + ifextra >= best)
            continue;

          bool allowRabbit = b.isRabOkay(pla,loc2,loc);
          if(loc2 S != loc && ISP(loc2 S) && b.isRabOkayN(pla,loc2 S))               {int extra2 = b.wouldBeUF(pla,loc2 S,loc2,loc2 S) ? 0 : 1; if(best > ufDist[loc2 S]+extra+extra2+ifextra) {best = ufDist[loc2 S]+extra+extra2+ifextra; if(best <= min) return best;}}
          if(loc2 W != loc && ISP(loc2 W) && (allowRabbit || b.pieces[loc2] != RAB)) {int extra2 = b.wouldBeUF(pla,loc2 W,loc2,loc2 W) ? 0 : 1; if(best > ufDist[loc2 W]+extra+extra2+ifextra) {best = ufDist[loc2 W]+extra+extra2+ifextra; if(best <= min) return best;}}
          if(loc2 E != loc && ISP(loc2 E) && (allowRabbit || b.pieces[loc2] != RAB)) {int extra2 = b.wouldBeUF(pla,loc2 E,loc2,loc2 E) ? 0 : 1; if(best > ufDist[loc2 E]+extra+extra2+ifextra) {best = ufDist[loc2 E]+extra+extra2+ifextra; if(best <= min) return best;}}
          if(loc2 N != loc && ISP(loc2 N) && b.isRabOkayS(pla,loc2 N))               {int extra2 = b.wouldBeUF(pla,loc2 N,loc2,loc2 N) ? 0 : 1; if(best > ufDist[loc2 N]+extra+extra2+ifextra) {best = ufDist[loc2 N]+extra+extra2+ifextra; if(best <= min) return best;}}
        }
        else if(b.owners[loc2] == opp && b.isTrapSafe2(pla,loc2))
        {
          if(minNewUfDist + 3 + ifextra >= best)
            continue;
          if(loc2 S != loc && ISP(loc2 S) && GT(loc2 S, loc2)) {if(best > ufDist[loc2 S]+3+ifextra) {best = ufDist[loc2 S]+3+ifextra; if(best <= min) return best;}}
          if(loc2 W != loc && ISP(loc2 W) && GT(loc2 W, loc2)) {if(best > ufDist[loc2 W]+3+ifextra) {best = ufDist[loc2 W]+3+ifextra; if(best <= min) return best;}}
          if(loc2 E != loc && ISP(loc2 E) && GT(loc2 E, loc2)) {if(best > ufDist[loc2 E]+3+ifextra) {best = ufDist[loc2 E]+3+ifextra; if(best <= min) return best;}}
          if(loc2 N != loc && ISP(loc2 N) && GT(loc2 N, loc2)) {if(best > ufDist[loc2 N]+3+ifextra) {best = ufDist[loc2 N]+3+ifextra; if(best <= min) return best;}}
        }
      }
    }
    else if(b.owners[loc] == opp)
    {
      if(!b.isOpen2(loc) && ufStatus != UF_NEED_SPACE)
        continue;

      if(minNewUfDist + 3 + ifextra >= best)
        continue;
      Bitmap strongEnoughPlas = otherPlas & pStrongerMaps[opp][b.pieces[loc]];
      for(int j = 0; j<4; j++)
      {
        if(!Board::ADJOKAY[j][loc])
          continue;
        loc_t loc2 = loc + Board::ADJOFFSETS[j];
        if(loc2 == ploc)
          continue;
        if((strongEnoughPlas & Board::RADIUS[1][loc2]).isEmpty())
          continue;
        if(b.owners[loc2] == NPLA && b.isTrapSafe2(pla,loc2))
        {
          if(loc2 S != loc && ISP(loc2 S) && GT(loc2 S, loc)) {int extra2 = b.wouldBeUF(pla,loc2 S,loc2,loc2 S) ? 0 : 1; if(best > ufDist[loc2 S]+3+extra2+ifextra) {best = ufDist[loc2 S]+3+extra2+ifextra; if(best <= min) return best;}}
          if(loc2 W != loc && ISP(loc2 W) && GT(loc2 W, loc)) {int extra2 = b.wouldBeUF(pla,loc2 W,loc2,loc2 W) ? 0 : 1; if(best > ufDist[loc2 W]+3+extra2+ifextra) {best = ufDist[loc2 W]+3+extra2+ifextra; if(best <= min) return best;}}
          if(loc2 E != loc && ISP(loc2 E) && GT(loc2 E, loc)) {int extra2 = b.wouldBeUF(pla,loc2 E,loc2,loc2 E) ? 0 : 1; if(best > ufDist[loc2 E]+3+extra2+ifextra) {best = ufDist[loc2 E]+3+extra2+ifextra; if(best <= min) return best;}}
          if(loc2 N != loc && ISP(loc2 N) && GT(loc2 N, loc)) {int extra2 = b.wouldBeUF(pla,loc2 N,loc2,loc2 N) ? 0 : 1; if(best > ufDist[loc2 N]+3+extra2+ifextra) {best = ufDist[loc2 N]+3+extra2+ifextra; if(best <= min) return best;}}
        }
      }
    }
  }
  return best;
}

static int ufRad4(const Board& b, pla_t pla, loc_t ploc, const int ufDist[BSIZE], const Bitmap pStrongerMaps[2][NUMTYPES],
    int min, int best, int minNewUfDist, loc_t badLoc, int ufStatus)
{
  Bitmap otherPlas = b.pieceMaps[pla][0] & ~Bitmap::makeLoc(ploc);
  //We use disk because of possible paths like:
  //........
  //...12...
  //...p3...
  //....u...
  //u steps 3->2->1 to unfreeze p, and it doesn't work to stop at 3 because of some badloc or immofrozenness.
  if((otherPlas & Board::DISK[4][ploc]).isEmpty())
    return UFDist::MAX_UF_DIST;

  pla_t opp = gOpp(pla);
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][ploc])
      continue;
    loc_t loc = ploc + Board::ADJOFFSETS[i];
    if((otherPlas & Board::DISK[3][loc]).isEmpty())
      continue;
    int ifextra = loc == badLoc ? IMMOFROZENCOST_N : 0;
    if(b.owners[loc] == pla) continue;
    piece_t strReq = EMP;
    int extra = 3;

    Bitmap strongEnoughPlas = otherPlas;
    if(b.owners[loc] == opp)
    {strReq = b.pieces[loc]+1; extra++; strongEnoughPlas &= pStrongerMaps[opp][b.pieces[loc]];}
    else if(ufStatus == UF_NEED_SPACE && b.owners[loc] == NPLA)
      extra++;
    else if(ufStatus == UF_NEED_SPACE_AND_DEFENDER && b.owners[loc] == NPLA)
      continue;

    if(minNewUfDist + extra + ifextra >= best)
      continue;

    for(int j = 0; j<4; j++)
    {
      if(!Board::ADJOKAY[j][loc])
        continue;
      loc_t loc2 = loc + Board::ADJOFFSETS[j];
      if(loc2 == ploc || b.owners[loc2] != NPLA || !b.isTrapSafe1(pla,loc2))
        continue;
      if((strongEnoughPlas & Board::RADIUS[2][loc2]).isEmpty())
        continue;
      bool allowRabbit = b.isRabOkay(pla,loc2,loc);
      Bitmap strongEnoughPlas2 = strongEnoughPlas;
      if(!allowRabbit)
        strongEnoughPlas2 &= ~b.pieceMaps[pla][RAB];

      for(int k = 0; k<4; k++)
      {
        if(!Board::ADJOKAY[k][loc2])
          continue;
        loc_t loc3 = loc2 + Board::ADJOFFSETS[k];
        if(loc3 == loc || b.owners[loc3] != NPLA || !b.isTrapSafe2(pla,loc3))
          continue;
        if((strongEnoughPlas2 & Board::RADIUS[1][loc3]).isEmpty())
          continue;
        bool allowRabbit2 = allowRabbit && b.isRabOkay(pla,loc3,loc2);

        //Note: allowRabbit2 is not checked on the N and S cases, but it shouldn't matter. In the case where allowRabbit2
        //is false and we have a rabbit that is okay to step N or S, that means the path to ploc must contain both an N
        //and an S step. That means it's a triviallike u-shaped path, which means the rabbit could have reached there
        //anyways with a single step, so we do no harm by counting this as a possible unfreeze.
        if(loc3 S != loc2 && ISP(loc3 S) && b.pieces[loc3 S] >= strReq && b.isRabOkayN(pla,loc3 S)                  && b.wouldBeUF(pla,loc3 S,loc3,loc3 S) && b.wouldBeUF(pla,loc3 S,loc2)) {if(best > ufDist[loc3 S]+extra+ifextra) {best = ufDist[loc3 S]+extra+ifextra; if(best <= min) return best;}}
        if(loc3 W != loc2 && ISP(loc3 W) && b.pieces[loc3 W] >= strReq && (allowRabbit2 || b.pieces[loc3 W] != RAB) && b.wouldBeUF(pla,loc3 W,loc3,loc3 W) && b.wouldBeUF(pla,loc3 W,loc2)) {if(best > ufDist[loc3 W]+extra+ifextra) {best = ufDist[loc3 W]+extra+ifextra; if(best <= min) return best;}}
        if(loc3 E != loc2 && ISP(loc3 E) && b.pieces[loc3 E] >= strReq && (allowRabbit2 || b.pieces[loc3 E] != RAB) && b.wouldBeUF(pla,loc3 E,loc3,loc3 E) && b.wouldBeUF(pla,loc3 E,loc2)) {if(best > ufDist[loc3 E]+extra+ifextra) {best = ufDist[loc3 E]+extra+ifextra; if(best <= min) return best;}}
        if(loc3 N != loc2 && ISP(loc3 N) && b.pieces[loc3 N] >= strReq && b.isRabOkayS(pla,loc3 N)                  && b.wouldBeUF(pla,loc3 N,loc3,loc3 N) && b.wouldBeUF(pla,loc3 N,loc2)) {if(best > ufDist[loc3 N]+extra+ifextra) {best = ufDist[loc3 N]+extra+ifextra; if(best <= min) return best;}}
      }
    }
  }
  return best;
}

static void solveUFDist(const Board& b, pla_t pla, int ufDist[BSIZE], const Bitmap pStrongerMaps[2][NUMTYPES])
{
  //Separate our pieces into frozen and unfrozen
  Bitmap plas = b.pieceMaps[pla][0];
  Bitmap thawedPieces = plas & (~b.frozenMap);

  //Among thawed pieces, filter down to only a subset that could possibly be immo
  pla_t opp = gOpp(pla);
  Bitmap opps = b.pieceMaps[opp][0];
  Bitmap doubleDefended = Bitmap::adj2(plas);
  Bitmap trapSafe = ~(Bitmap::BMPTRAPS & ~doubleDefended);
  Bitmap emptyTrapSafe = ~(plas | opps) & trapSafe;
  Bitmap rabbits = b.pieceMaps[pla][RAB];
  Bitmap plasSteppable = plas & (Bitmap::adjWE(emptyTrapSafe) | Bitmap::shiftF(emptyTrapSafe,pla) | (~rabbits & Bitmap::shiftG(emptyTrapSafe,pla)));
  Bitmap plasSteppableTrapSafe = plasSteppable & trapSafe;
  Bitmap plasSteppableIntoPlas = plas & trapSafe & (~b.dominatedMap | doubleDefended)
      & (Bitmap::adjWE(plasSteppableTrapSafe) | Bitmap::shiftF(plasSteppableTrapSafe,pla) | (~rabbits & Bitmap::shiftG(plasSteppableTrapSafe,pla)));

  //TODO currently isImmo and the above bitmap calc consider a piece to be immo if the only direction it can step
  //is on to a trap that currently has a pla piece that *can* step and that trap has 1 defender. This seems possibly wrong.

  Bitmap couldBeImmo = thawedPieces & ~plasSteppable & ~plasSteppableIntoPlas;
  Bitmap thawedNonImmo = thawedPieces & ~couldBeImmo;
  Bitmap frozenPieces = plas & b.frozenMap;

  //Track the pieces that are frozen or immo
  loc_t frozenLocs[16];
  loc_t badLoc[16];
  int ufStatus[16];
  int frozenCount = 0;

  //Initialize ufdist to zero for all thawed nonimmo pieces
  //All other pieces (frozen,immo) have the more detailed ufdist calculation done
  while(thawedNonImmo.hasBits())
  {
    loc_t ploc = thawedNonImmo.nextBit();
    ufDist[ploc] = 0;
  }
  while(couldBeImmo.hasBits())
  {
    loc_t ploc = couldBeImmo.nextBit();
    if(isImmo(b,pla,ploc))
    {
      frozenLocs[frozenCount] = ploc;
      ufStatus[frozenCount] = UF_NEED_SPACE;
      badLoc[frozenCount] = getThawedImmoUFBadLoc(b,pla,ploc);
      ufDist[ploc] = UFDist::MAX_UF_DIST;
      frozenCount++;
    }
    else
      ufDist[ploc] = 0;
  }
  while(frozenPieces.hasBits())
  {
    loc_t ploc = frozenPieces.nextBit();
    frozenLocs[frozenCount] = ploc;
    ufStatus[frozenCount] = getFrozenUFStatus(b,pla,ploc,badLoc[frozenCount]);
    ufDist[ploc] = UFDist::MAX_UF_DIST;
    frozenCount++;
  }

  //Iterate in rounds of unfreezing calcs
  //Round 1
  int newCount = 0;
  for(int i = 0; i<frozenCount; i++)
  {
    loc_t ploc = frozenLocs[i];
    int dist = ufRad2(b,pla,ploc,ufDist,1,UFDist::MAX_UF_DIST,0,badLoc[i],ufStatus[i]);
    ufDist[ploc] = dist;
    if(dist > 2)
    {
      frozenLocs[newCount] = ploc;
      ufStatus[newCount] = ufStatus[i];
      badLoc[newCount] = badLoc[i];
      newCount++;
    }
  }
  frozenCount = newCount;
  //Round 2
  newCount = 0;
  for(int i = 0; i<frozenCount; i++)
  {
    loc_t ploc = frozenLocs[i];
    int dist = ufDist[ploc];

    int newDist = ufRad2(b,pla,ploc,ufDist,2,dist,1,badLoc[i],ufStatus[i]);
    if(newDist < dist)
    {dist = newDist; if(dist <= 2) {ufDist[ploc] = dist; continue;}}

    newDist = ufRad3(b,pla,ploc,ufDist,pStrongerMaps,2,dist,0,badLoc[i],ufStatus[i]);
    if(newDist < dist)
    {dist = newDist;}

    ufDist[ploc] = dist;
    if(dist <= 3) continue;

    frozenLocs[newCount] = ploc;
    ufStatus[newCount] = ufStatus[i];
    badLoc[newCount] = badLoc[i];
    newCount++;
  }
  frozenCount = newCount;
  //Round 3
  for(int i = 0; i<frozenCount; i++)
  {
    loc_t ploc = frozenLocs[i];
    int dist = ufDist[ploc];

    int newDist = ufRad2(b,pla,ploc,ufDist,3,dist,2,badLoc[i],ufStatus[i]);
    if(newDist < dist)
    {dist = newDist; if(dist <= 3) {ufDist[ploc] = dist; continue;}}

    newDist = ufRad3(b,pla,ploc,ufDist,pStrongerMaps,3,dist,1,badLoc[i],ufStatus[i]);
    if(newDist < dist)
    {dist = newDist; if(dist <= 3) {ufDist[ploc] = dist; continue;}}

    newDist = ufRad4(b,pla,ploc,ufDist,pStrongerMaps,3,dist,0,badLoc[i],ufStatus[i]);
    if(newDist < dist)
    {dist = newDist;}

    ufDist[ploc] = dist;
  }
}

void UFDist::get(const Board& b, int ufDist[BSIZE], const Bitmap pStrongerMaps[2][NUMTYPES])
{
  solveUFDist(b,GOLD,ufDist,pStrongerMaps);
  solveUFDist(b,SILV,ufDist,pStrongerMaps);
}


