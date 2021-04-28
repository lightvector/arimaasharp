/*
 * searchprune.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtrees.h"
#include "../search/searchprune.h"

#include "../board/boardtreeconst.h"
#include "../board/locations.h"

static const uint64_t BN567 = 0x1F1F1F1F1F1F1F1FULL;
static const uint64_t BN67  = 0x3F3F3F3F3F3F3F3FULL;
static const uint64_t BN01  = 0xFCFCFCFCFCFCFCFCULL;
static const uint64_t BN012 = 0xF8F8F8F8F8F8F8F8ULL;

static const uint64_t BACKTWOROWS[2] =
{
0xFFFF000000000000ULL, //silv
0x000000000000FFFFULL, //gold
};

static const uint64_t BACKTWOROWS_C2[2] =
{
0x3F3F000000000000ULL, //silv
0x0000000000003F3FULL, //gold
};
static const uint64_t BACKTWOROWS_F2[2] =
{
0xFCFC000000000000ULL, //silv
0x000000000000FCFCULL, //gold
};

void SearchPrune::findStartMatches(Board& b, vector<ID>& buf)
{
  buf.clear();
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);

  int pStronger[2][NUMTYPES];
  Bitmap pStrongerMaps[2][NUMTYPES];
  b.initializeStronger(pStronger);
  b.initializeStrongerMaps(pStrongerMaps);
  Bitmap oppGoalThreatRabs = BoardTrees::goalThreatRabsMap(b,gOpp(pla),4);

  //For vertically flipping the locations depending on player
  int yflip = pla == GOLD ? 0x00 : 0x70;

  //Sac piece in empty home trap
  //No dominated pla pieces around trap
  loc_t c3 = C3^yflip;
  if(ISE(c3) && (oppGoalThreatRabs & Bitmap(BN67)).isEmpty())
  {
    if((Board::DISK[1][c3] & b.dominatedMap & b.pieceMaps[pla][0]).isEmpty())
      buf.push_back(ID::C3_SAC_E);
  }
  loc_t f3 = F3^yflip;
  if(ISE(f3) && (oppGoalThreatRabs & Bitmap(BN01)).isEmpty())
  {
    if((Board::DISK[1][f3] & b.dominatedMap & b.pieceMaps[pla][0]).isEmpty())
      buf.push_back(ID::F3_SAC_E);
  }

  //Sac piece in empty opp trap with initially 1 defender
  //No dominated pla pieces around trap
  loc_t c6 = C6^yflip;
  if(ISE(c6) && (oppGoalThreatRabs & Bitmap(BN567)).isEmpty())
  {
    if((Board::DISK[1][c6] & b.dominatedMap & b.pieceMaps[pla][0]).isEmpty())
      buf.push_back(ID::C6_SAC_E);
  }
  loc_t f6 = F6^yflip;
  if(ISE(f6) && (oppGoalThreatRabs & Bitmap(BN012)).isEmpty())
  {
    if((Board::DISK[1][f6] & b.dominatedMap & b.pieceMaps[pla][0]).isEmpty())
      buf.push_back(ID::F6_SAC_E);
  }

  //Retreat piece needlessly from behind own trap
  //Require that the retreated piece is not dominated and that there is no opp at all nearby on the back two rows
  //Require that it's not a rabbit, since rabbits can't retreat
  //Require that own elephant is not at that trap (sometimes you want to retreat to open space for hostaging,
  //and retreating is totally okay because your elephant is guarding
  loc_t c2 = C2^yflip;
  if(ISP(c2) && b.pieces[c2] != RAB && b.trapGuardCounts[pla][Board::PLATRAPINDICES[pla][0]] >= 2 && oppGoalThreatRabs.isEmpty())
  {
    if(!b.isDominated(c2) && (Bitmap(BACKTWOROWS_C2[pla]) & b.pieceMaps[opp][0]).isEmpty()
        && (Board::DISK[1][c3] & b.pieceMaps[pla][ELE]).isEmpty())
      buf.push_back(ID::C2_RETREAT);
  }
  loc_t f2 = F2^yflip;
  if(ISP(f2) && b.pieces[c2] != RAB && b.trapGuardCounts[pla][Board::PLATRAPINDICES[pla][1]] >= 2 && oppGoalThreatRabs.isEmpty())
  {
    if(!b.isDominated(f2) && (Bitmap(BACKTWOROWS_F2[pla]) & b.pieceMaps[opp][0]).isEmpty()
        && (Board::DISK[1][f3] & b.pieceMaps[pla][ELE]).isEmpty())
      buf.push_back(ID::F2_RETREAT);
  }

  //Retreat piece needlessly to back row in various places
  //Require that there are no opps nearby, the backrow is not infiltrated anywhere near, no opp goal threats.
  //Require that it's not a rabbit, since rabbits can't retreat
  loc_t a2 = A2^yflip;
  if(ISP(a2) && b.pieces[a2] != RAB && oppGoalThreatRabs.isEmpty())
  {
    if(((Board::DISK[3][a2] | Bitmap(BACKTWOROWS_C2[pla])) & b.pieceMaps[opp][0]).isEmpty())
      buf.push_back(ID::A2_RETREAT);
  }
  loc_t b2 = B2^yflip;
  if(ISP(b2) && b.pieces[b2] != RAB && oppGoalThreatRabs.isEmpty())
  {
    if(((Board::DISK[3][b2] | Bitmap(BACKTWOROWS_C2[pla])) & b.pieceMaps[opp][0]).isEmpty())
      buf.push_back(ID::B2_RETREAT);
  }
  loc_t d2 = D2^yflip;
  if(ISP(d2) && b.pieces[d2] != RAB && oppGoalThreatRabs.isEmpty())
  {
    if(((Board::DISK[3][d2] | Bitmap(BACKTWOROWS[pla])) & b.pieceMaps[opp][0]).isEmpty())
      buf.push_back(ID::D2_RETREAT);
  }
  loc_t e2 = E2^yflip;
  if(ISP(e2) && b.pieces[e2] != RAB && oppGoalThreatRabs.isEmpty())
  {
    if(((Board::DISK[3][e2] | Bitmap(BACKTWOROWS[pla])) & b.pieceMaps[opp][0]).isEmpty())
      buf.push_back(ID::E2_RETREAT);
  }
  loc_t g2 = G2^yflip;
  if(ISP(g2) && b.pieces[g2] != RAB && oppGoalThreatRabs.isEmpty())
  {
    if(((Board::DISK[3][g2] | Bitmap(BACKTWOROWS_F2[pla])) & b.pieceMaps[opp][0]).isEmpty())
      buf.push_back(ID::G2_RETREAT);
  }
  loc_t h2 = H2^yflip;
  if(ISP(h2) && b.pieces[h2] != RAB && oppGoalThreatRabs.isEmpty())
  {
    if(((Board::DISK[3][h2] | Bitmap(BACKTWOROWS_F2[pla])) & b.pieceMaps[opp][0]).isEmpty())
      buf.push_back(ID::H2_RETREAT);
  }

}


static bool sacETest(Board& b, pla_t pla, loc_t kt, int ns, const loc_t srcs[4], const loc_t dests[4],
    bool requireCreatesNoNearbyGoalThreat)
{
  //Require:
  //At least one step on to the trap
  //No steps off from the trap
  //No steps onto any piece stepping on to the trap.
  //The final board has the trap empty
  //Optionally - there is no goal threat created in the vicinity of the trap by the move
  if(!ISE(kt))
    return false;
  Bitmap srcOnToTrap;
  for(int i = 0; i<ns; i++)
  {
    if(dests[i] == kt)
      srcOnToTrap.setOn(srcs[i]);
    else if(srcs[i] == kt)
      return false;
    else if(srcOnToTrap.isOne(dests[i]))
      return false;
  }
  if(srcOnToTrap.isEmpty())
    return false;

  if(!requireCreatesNoNearbyGoalThreat)
    return true;

  return BoardTrees::goalDistForRabs(b,pla,4,Board::DISK[4][kt]) >= 5;
}

static bool c2f2RetreatTest(Board& b, pla_t pla, loc_t loc, loc_t kt, int ns, const loc_t srcs[4], const loc_t dests[4])
{
  //Require:
  //The square ends empty
  //At least one step to C1 or F1
  //The trap now has 0 or 1 defenders
  //No steps involving any squares adjacent to C2 or F2 or C3 or F3.
  //There is no goal threat created anywhere on the board by the move
  if(!ISE(loc) || b.trapGuardCounts[pla][Board::TRAPINDEX[kt]] >= 2)
    return false;
  loc_t c1f1 = loc + (loc - kt);
  bool found = false;
  for(int i = 0; i<ns; i++)
  {
    if(srcs[i] == loc && dests[i] == c1f1)
      found = true;
    else if(Board::isAdjacent(srcs[i],loc) ||
            Board::isAdjacent(dests[i],loc) ||
            Board::isAdjacent(srcs[i],kt) ||
            Board::isAdjacent(dests[i],kt))
      return false;
  }
  if(!found)
    return false;
  return BoardTrees::goalDistForRabs(b,pla,4,Bitmap::BMPONES) >= 5;
}

static bool genericRetreatTest(Board& b, pla_t pla, loc_t loc, loc_t back, int ns, const loc_t srcs[4], const loc_t dests[4])
{
  //Require:
  //The square ends empty
  //At least one step back
  //No other steps within radius 2
  //There is no goal threat created anywhere on the board by the move
  if(!ISE(loc))
    return false;
  bool found = false;
  for(int i = 0; i<ns; i++)
  {
    if(srcs[i] == loc && dests[i] == back)
      found = true;
    else if(Board::manhattanDist(srcs[i],loc) <= 2 ||
            Board::manhattanDist(dests[i],loc) <= 2)
      return false;
  }
  if(!found)
    return false;
  return BoardTrees::goalDistForRabs(b,pla,4,Bitmap::BMPONES) >= 5;

}

static bool matchesSingle(Board& b, pla_t pla, int id, int yflip, int ns, const loc_t srcs[4], const loc_t dests[4])
{
  using namespace SearchPrune;
  switch(id)
  {
  case ID::C3_SAC_E: return sacETest(b,pla,C3^yflip,ns,srcs,dests,false);
  case ID::F3_SAC_E: return sacETest(b,pla,F3^yflip,ns,srcs,dests,false);
  case ID::C6_SAC_E: return sacETest(b,pla,C6^yflip,ns,srcs,dests,true);
  case ID::F6_SAC_E: return sacETest(b,pla,F6^yflip,ns,srcs,dests,true);

  case ID::C2_RETREAT: return c2f2RetreatTest(b,pla,C2^yflip,C3^yflip,ns,srcs,dests);
  case ID::F2_RETREAT: return c2f2RetreatTest(b,pla,F2^yflip,F3^yflip,ns,srcs,dests);

  case ID::A2_RETREAT: return genericRetreatTest(b,pla,A2^yflip,A1^yflip,ns,srcs,dests);
  case ID::B2_RETREAT: return genericRetreatTest(b,pla,B2^yflip,B1^yflip,ns,srcs,dests);
  case ID::D2_RETREAT: return genericRetreatTest(b,pla,D2^yflip,D1^yflip,ns,srcs,dests);
  case ID::E2_RETREAT: return genericRetreatTest(b,pla,E2^yflip,E1^yflip,ns,srcs,dests);
  case ID::G2_RETREAT: return genericRetreatTest(b,pla,G2^yflip,G1^yflip,ns,srcs,dests);
  case ID::H2_RETREAT: return genericRetreatTest(b,pla,H2^yflip,H1^yflip,ns,srcs,dests);
  default: DEBUGASSERT(false);
  }
  return false;
}

bool SearchPrune::matchesMove(Board& endBoard, pla_t pla, move_t m, const vector<ID>& ids)
{
  size_t numIds = ids.size();
  if(numIds <= 0)
    return false;

  int ns = 0;
  loc_t srcs[4];
  loc_t dests[4];
  for(int i = 0; i<4; i++)
  {
    step_t step = getStep(m,i);
    if(step == ERRSTEP || step == PASSSTEP || step == QPASSSTEP)
      break;
    srcs[ns] = gSrc(step);
    dests[ns] = gDest(step);
    ns++;
  }

  int yflip = pla == GOLD ? 0x00 : 0x70;

  for(size_t i = 0; i<numIds; i++)
  {
    if(matchesSingle(endBoard,pla,ids[i],yflip,ns,srcs,dests))
      return true;
  }
  return false;
}

static bool isDependent(const Board& origBoard, pla_t pla, step_t a, step_t b)
{
  loc_t asrc = gSrc(a);
  loc_t adest = gDest(a);
  loc_t bsrc = gSrc(b);
  loc_t bdest = gDest(b);

  if(asrc == bdest || bsrc == adest)
    return true;

  //TODO Tried to add these before, but got testing evidence that it made things worse!
  //TODO A steps away prior to B stepping because A would be frozen without B.
  //TODO A steps away off a trap prior to B stepping because A would be saced without B.
  //TODO add a defender of a trap with a pla piece on it prior to removing a defender

  //Unfreeze
  //It wasn't necessarily step a that unfroze b, but the fact that we moved
  //a frozen piece at all indicates that a dependency happened, so we leave
  //off the check that step a was the step that did it as a performance hack
  if(origBoard.isFrozen(bsrc))
    return true;

  //Pushpull
  pla_t opp = gOpp(pla);
  if(origBoard.owners[asrc] == opp || origBoard.owners[bsrc] == opp)
    return true;
  //Trap-related - add or remove a defender of a trap prior to stepping on it
  if(Board::ISTRAP[bdest] && Board::ADJACENTTRAP[adest] == bdest)
    return true;
  //Adding a defender to a trap after stepping on it.
  if(Board::ISTRAP[adest] && Board::ADJACENTTRAP[bdest] == adest)
    return true;

  return false;
}

bool SearchPrune::canHaizhiPrune(const Board& origBoard, pla_t pla, move_t m)
{
  step_t s0 = getStep(m,0);
  step_t s1 = getStep(m,1);
  if(s0 == ERRSTEP || s0 == PASSSTEP || s0 == QPASSSTEP)
    return false;
  if(s1 == ERRSTEP || s1 == PASSSTEP || s1 == QPASSSTEP)
    return false;
  if(isDependent(origBoard,pla,s0,s1))
    return false;
  step_t s2 = getStep(m,2);
  if(s2 == ERRSTEP || s2 == PASSSTEP || s2 == QPASSSTEP)
    return false;
  if(isDependent(origBoard,pla,s0,s2) || isDependent(origBoard,pla,s1,s2))
    return false;

  return true;
}

/*
{
MovePattern testPattern;
testPattern.pla = GOLD;
testPattern.move = Board::readMove("b3s");
testPattern.comboSrcRad = 1;
testPattern.comboDestRad = 1;
testPattern.comboSepExpandOnTrap = true;
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("b3"),GOLD));
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("b2"),NPLA));
testPattern.startConds.push_back(!MoveCond::makeIsDomWithinOf(Board::readLoc("b3"),2,Board::readLoc("b4")));
testPattern.startConds.push_back(!MoveCond::makeIsDomWithinOf(Board::readLoc("b3"),3,Board::readLoc("b2")));
testPattern.startConds.push_back(MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),GOLD,2));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),GOLD,4));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("b2"),SILV,1));
testPattern.startConds.push_back(!MoveCond::makeIsOppGoalThreatRabs((~Bitmap::BMPX[6] & ~Bitmap::BMPX[7]).bits));
testPattern.recomputeMaps();
patterns.push_back(testPattern);
}

{
MovePattern testPattern;
testPattern.pla = GOLD;
testPattern.move = Board::readMove("d2s");
testPattern.comboSrcRad = 1;
testPattern.comboDestRad = 1;
testPattern.comboSepExpandOnTrap = false;
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("d2"),GOLD));
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("d1"),NPLA));
testPattern.startConds.push_back(!MoveCond::makeIsDomWithinOf(Board::readLoc("d2"),4,Board::readLoc("d1")));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("d1"),SILV,1));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),SILV,1));
testPattern.startConds.push_back(MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),GOLD,1));
testPattern.startConds.push_back(!MoveCond::makeIsOppGoalThreatRabs((~Bitmap::BMPX[7]).bits));
testPattern.recomputeMaps();
patterns.push_back(testPattern);
}

{
MovePattern testPattern;
testPattern.pla = GOLD;
testPattern.move = Board::readMove("b2s");
testPattern.comboSrcRad = 1;
testPattern.comboDestRad = 1;
testPattern.comboSepExpandOnTrap = false;
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("b2"),GOLD));
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("b1"),NPLA));
testPattern.startConds.push_back(!MoveCond::makeIsDomWithinOf(Board::readLoc("b2"),4,Board::readLoc("b1")));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("b1"),SILV,1));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),SILV,1));
testPattern.startConds.push_back(MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),GOLD,1));
testPattern.startConds.push_back(!MoveCond::makeIsOppGoalThreatRabs((~Bitmap::BMPX[6] & ~Bitmap::BMPX[7]).bits));
testPattern.recomputeMaps();
patterns.push_back(testPattern);
}
{
MovePattern testPattern;
testPattern.pla = GOLD;
testPattern.move = Board::readMove("c2s");
testPattern.comboSrcRad = 1;
testPattern.comboDestRad = 1;
testPattern.comboSepExpandOnTrap = true;
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("c2"),GOLD));
testPattern.startConds.push_back(MoveCond::makeIsPla(Board::readLoc("c1"),NPLA));
testPattern.startConds.push_back(!MoveCond::makeIsDomWithinOf(Board::readLoc("c2"),4,Board::readLoc("c1")));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("c1"),SILV,1));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),SILV,1));
testPattern.startConds.push_back(MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),GOLD,2));
testPattern.startConds.push_back(!MoveCond::makeIsNPlasAdjacent(Board::readLoc("c3"),GOLD,4));
testPattern.startConds.push_back(!MoveCond::makeIsOppGoalThreatRabs((~Bitmap::BMPX[7]).bits));
testPattern.recomputeMaps();
patterns.push_back(testPattern);
}

*/

