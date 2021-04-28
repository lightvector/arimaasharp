/*
 * hostages.cpp
 * Author: davidwu
 */
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtreeconst.h"
#include "../eval/eval.h"
#include "../eval/threats.h"
#include "../eval/internal.h"

class HostageThreat
{
  public:
  loc_t hostageLoc;          //Location of the hostage
  loc_t holderLoc;           //Location of a piece holding the hostage
  loc_t holderLoc2;          //Second location of a piece holding the hostage, or ERRLOC
  loc_t kt;                  //The trap the hostage is being threatened at

  inline friend ostream& operator<<(ostream& out, const HostageThreat& t)
  {
    out << "Hostage: Loc " << Board::writeLoc(t.hostageLoc)
        << " Holders " << Board::writeLoc(t.holderLoc) << " " << Board::writeLoc(t.holderLoc2)
        << " Trap " << Board::writeLoc(t.kt);
    return out;
  }
};

//Possible hostage locations around TRAP0.
//111100000
//110100000
//100000000
//010000000
//000000000
//000000000
//000000000
//000000000

static const int hostageLocs[4][Strats::maxHostagesPerKT] = {
{gLoc(1,3),gLoc(0,2),gLoc(2,0),gLoc(1,1),gLoc(3,1),gLoc(0,1),gLoc(1,0),gLoc(0,0),gLoc(3,0)}, //TRAP0
{gLoc(6,3),gLoc(7,2),gLoc(5,0),gLoc(6,1),gLoc(4,1),gLoc(7,1),gLoc(6,0),gLoc(7,0),gLoc(4,0)}, //TRAP1
{gLoc(1,4),gLoc(0,5),gLoc(2,7),gLoc(1,6),gLoc(3,6),gLoc(0,6),gLoc(1,7),gLoc(0,7),gLoc(3,7)}, //TRAP2
{gLoc(6,4),gLoc(7,5),gLoc(5,7),gLoc(6,6),gLoc(4,6),gLoc(7,6),gLoc(6,7),gLoc(7,7),gLoc(4,7)}, //TRAP3
};
static const int allowRadius2Holder[Strats::maxHostagesPerKT] =
{false,false,false,false,false,true,true,true,false};

static const int centrality1D[8] = {0,1,2,3,3,2,1,0};
static bool isBetterHolder(const Board& b, loc_t holder, loc_t than)
{
  return
      than == ERRLOC ||
      b.pieces[holder] < b.pieces[than] || (b.pieces[holder] == b.pieces[than] && (
      (centrality1D[gY(holder)] > centrality1D[gY(than)] || (centrality1D[gY(holder)] == centrality1D[gY(than)] &&
      (centrality1D[gX(holder)] > centrality1D[gX(than)]
      )))));
}
static void findBestHolders(const Board& b, loc_t& holderLoc, loc_t& holderLoc2, Bitmap possibleHolders)
{
  while(possibleHolders.hasBits())
  {
    loc_t hloc = possibleHolders.nextBit();
    if(isBetterHolder(b,hloc,holderLoc))
    {
      holderLoc2 = holderLoc;
      holderLoc = hloc;
    }
    else if(isBetterHolder(b,hloc,holderLoc2))
    {
      holderLoc2 = hloc;
    }
  }
}

static bool hostageCanStepTo(const Board& b, pla_t pla, pla_t opp, loc_t loc, loc_t adj)
{
  if(ISE(adj)) return true;
  else if(ISP(adj) && LT(adj,loc) && b.isOpen(adj)) return true;
  else if(ISO(adj) && b.wouldBeUF(opp,loc,loc,adj) && (b.isOpenToStep(adj) || b.isOpenToPush(adj))) return true;
  return false;
}

static int findHostages(const Board& b, pla_t pla, loc_t kt, HostageThreat* threats,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  int numThreats = 0;
  pla_t opp = gOpp(pla);
  int trapIndex = Board::TRAPINDEX[kt];
  for(int i = 0; i<Strats::maxHostagesPerKT; i++)
  {
    loc_t loc = hostageLocs[trapIndex][i];
    if(b.owners[loc] == opp)
    {
      //If there is a stronger adjacent piece, the weakest such is the holder. The next strongest if any is the holder2.
      loc_t holderLoc = ERRLOC;
      loc_t holderLoc2 = ERRLOC;
      piece_t oppPiece = b.pieces[loc];
      findBestHolders(b,holderLoc,holderLoc2,pStrongerMaps[opp][oppPiece] & Board::RADIUS[1][loc]);
      //No holder found, so consider radius 2
      if(holderLoc == ERRLOC && allowRadius2Holder[i])
        //At radius 2, the piece cannot be on the goal line and count as a holder.
        findBestHolders(b,holderLoc,holderLoc2,pStrongerMaps[opp][oppPiece] & Board::RADIUS[2][loc] & Board::YINTERVAL[1][6]);

      if(holderLoc == ERRLOC)
        continue;

      //If frozen, we're definitely okay. But if not..
      if(ufDist[loc] <= 0)
      {
        //Any square the piece can step to should either be closer to the corner OR closer to a holder
        //Closer to the corner is equivalent to further from the diag opposite trap.
        //Being able to push into c2 is also okay
        bool canEscape = false;
        for(int j = 0; j<4; j++)
        {
          if(!Board::ADJOKAY[j][loc])
            continue;
          loc_t adj = loc + Board::ADJOFFSETS[j];
          if(!b.isRabOkay(opp,loc,adj))
            continue;
          if(Board::manhattanDist(adj,holderLoc) < Board::manhattanDist(loc,holderLoc))
            continue;
          if(holderLoc2 != ERRLOC && Board::manhattanDist(adj,holderLoc2) < Board::manhattanDist(loc,holderLoc2))
            continue;
          if(Bitmap::BMPBEHINDTRAPS.isOne(adj))
            continue;
          loc_t oppositeTrap = Board::TRAPLOCS[3-trapIndex];
          if(Board::manhattanDist(adj,oppositeTrap) > Board::manhattanDist(loc,oppositeTrap))
            continue;

          if(hostageCanStepTo(b,pla,opp,loc,adj))
          {canEscape = true; break;}
        }
        if(canEscape)
          continue;
      }

      threats[numThreats].kt = kt;
      threats[numThreats].hostageLoc = loc;
      threats[numThreats].holderLoc = holderLoc;
      threats[numThreats].holderLoc2 = holderLoc2;
      numThreats++;
    }
  }

  return numThreats;
}



static int capInterfereCost(const Board& b, pla_t pla, loc_t eloc, loc_t loc)
{
  if(b.owners[loc] == pla)
    return (int)(b.pieces[loc] <= b.pieces[eloc]) + (int)(b.isFrozen(loc));
  return 0;
}

static int capInterferePathCost(const Board& b, pla_t pla, loc_t kt, loc_t eloc)
{
  int dx = gX(eloc)-gX(kt);
  int dy = gY(eloc)-gY(kt);
  int mdist = Board::manhattanDist(kt,eloc);
  if(mdist < 2)
    return 0;

  int cost = 0;
  cost += capInterfereCost(b,pla,eloc,kt);

  if(dx == 0 || dy == 0)
  {
    if(dy < 0)       cost += capInterfereCost(b,pla,eloc,kt S);
    else if (dx < 0) cost += capInterfereCost(b,pla,eloc,kt W);
    else if (dx > 0) cost += capInterfereCost(b,pla,eloc,kt E);
    else             cost += capInterfereCost(b,pla,eloc,kt N);
  }
  else
  {
    if(dy < 0 && dx < 0)      cost += min(capInterfereCost(b,pla,eloc,kt S),capInterfereCost(b,pla,eloc,kt W));
    else if(dy < 0 && dx > 0) cost += min(capInterfereCost(b,pla,eloc,kt S),capInterfereCost(b,pla,eloc,kt E));
    else if(dy > 0 && dx < 0) cost += min(capInterfereCost(b,pla,eloc,kt N),capInterfereCost(b,pla,eloc,kt W));
    else                      cost += min(capInterfereCost(b,pla,eloc,kt N),capInterfereCost(b,pla,eloc,kt E));
  }

  if(mdist > 2)
  {
    if(dx == 0 || dy == 0)
    {
      if(dy < 0)       cost += capInterfereCost(b,pla,eloc,eloc N);
      else if (dx < 0) cost += capInterfereCost(b,pla,eloc,eloc E);
      else if (dx > 0) cost += capInterfereCost(b,pla,eloc,eloc W);
      else             cost += capInterfereCost(b,pla,eloc,eloc S);
    }
    else
    {
      if(dy < 0 && dx < 0)      cost += min(capInterfereCost(b,pla,eloc,eloc N),capInterfereCost(b,pla,eloc,eloc E));
      else if(dy < 0 && dx > 0) cost += min(capInterfereCost(b,pla,eloc,eloc N),capInterfereCost(b,pla,eloc,eloc W));
      else if(dy > 0 && dx < 0) cost += min(capInterfereCost(b,pla,eloc,eloc S),capInterfereCost(b,pla,eloc,eloc E));
      else                      cost += min(capInterfereCost(b,pla,eloc,eloc S),capInterfereCost(b,pla,eloc,eloc W));
    }
  }

  return cost;
}

static int getHostageThreatDist(const Board& b, pla_t pla, HostageThreat& threat, const int ufDist[BSIZE])
{
  pla_t opp = gOpp(pla);
  loc_t kt = threat.kt;
  loc_t hostageLoc = threat.hostageLoc;
  //Basic capture distance
  int trapMDist = Board::manhattanDist(hostageLoc,kt);
  int threatDist = trapMDist*2 + capInterferePathCost(b,pla,kt,hostageLoc);
  //Extra if the trap is occupied by a pla piece too weak to cap the hostage
  if(b.owners[kt] == pla && b.pieces[kt] <= b.pieces[hostageLoc])
    threatDist += 1;

  //Extra if the holder is frozen
  loc_t holderLoc = threat.holderLoc;
  int ufD = ufDist[holderLoc];
  if(threat.holderLoc2 != ERRLOC && ufD > 0)
  {
    int ufD2 = ufDist[threat.holderLoc2];
    if(ufD2 < ufD)
      ufD = ufD2;
  }
  threatDist += ufD;

  //Extra if the holder is radius 2
  if(Board::manhattanDist(hostageLoc,holderLoc) >= 2)
  {
    threatDist++;
    //And based on squares where we might pull closer...
    Bitmap adjBoth = Board::RADIUS[1][hostageLoc] & Board::RADIUS[1][holderLoc];
    int minExtraDist = 6;
    while(adjBoth.hasBits())
    {
      loc_t adjBothLoc = adjBoth.nextBit();
      int extraDist = 0;
      //More if the squares are blocked
      if(!ISE(adjBothLoc))
      {
        extraDist++;
        if(!b.isOpen(adjBothLoc))
          extraDist++;
        if(ISO(adjBothLoc))
          extraDist+=2;
      }
      //Or the piece would be unfrozen if pulled closer
      if(b.wouldBeUF(opp,hostageLoc,adjBothLoc,hostageLoc))
        extraDist += 3;
      if(extraDist < minExtraDist)
        minExtraDist = extraDist;
    }
    threatDist += minExtraDist;
  }
  //Radius 1
  else
  {
    //Pulling closer to trap would leave piece unfrozen
    if(trapMDist >= 3 && b.wouldBeG(opp,holderLoc,hostageLoc))
      threatDist += 3;
  }

  //Extra if the hostage can walk any way easily
  if(ufDist[hostageLoc] <= 1)
  {
    int maxEscapeExtra = 0;
    for(int j = 0; j<4; j++)
    {
      if(!Board::ADJOKAY[j][hostageLoc])
        continue;
      loc_t adj = hostageLoc + Board::ADJOFFSETS[j];
      if(!b.isRabOkay(opp,hostageLoc,adj))
        continue;
      if(Board::manhattanDist(adj,holderLoc) < Board::manhattanDist(hostageLoc,holderLoc))
        continue;
      int escapeExtra = 0;
      if(hostageCanStepTo(b,pla,opp,hostageLoc,adj))
      {
        escapeExtra += 2;
        //If the hostage is guarded now, it will be very hard to pull it back, unless it's actually going closer to the trap
        //in which case who cares
        if(b.isGuarded(opp,hostageLoc) && !b.isAdjacent(adj,kt))
          escapeExtra += 2;
      }
      if(escapeExtra > maxEscapeExtra)
        maxEscapeExtra = escapeExtra;
    }
    threatDist += maxEscapeExtra;
  }

  //Lastly, subtract off some distance if the opp's elephant has buried itself
  if(b.pieces[holderLoc] == ELE && (b.pieceMaps[opp][ELE] & Board::DISK[3][hostageLoc]).hasBits())
  {
    //Opp elephant is closer to the corner than D3 would be - remove one threatDist for each amount this is true
    loc_t oppEle = b.findElephant(opp);
    int trapIndex = Board::TRAPINDEX[kt];
    loc_t oppositeTrap = Board::TRAPLOCS[3-trapIndex];
    int oppositeTrapDist = Board::manhattanDist(oppEle,oppositeTrap);
    if(oppositeTrapDist > 5)
    {
      threatDist -= oppositeTrapDist - 5;
      if(threatDist <= 0)
        threatDist = 0;
    }
  }

  return threatDist;
}

static const int HOSTAGE_THREAT_SHIFT = 10;
static const int HOSTAGE_THREAT_LEN = 24;
static const int HOSTAGE_THREAT_FACTOR[HOSTAGE_THREAT_LEN] =
{800, 760,750,740,730, 590,560,500,460, 390,340,280,210, 150,100,60,20, 0,0,0,0,0,0,0};

static int hostageThreatValue(const Board& b, const eval_t values[NUMTYPES], loc_t loc, int dist)
{
  if(dist < 0) dist = 0;
  return dist >= HOSTAGE_THREAT_LEN ? 0 : (HOSTAGE_THREAT_FACTOR[dist]*values[b.pieces[loc]]) >> HOSTAGE_THREAT_SHIFT;
}


//Scaling of hostage value by number of steps to make the hostage runaway!
static const int HOSTAGE_RUNAWAY_BS_LEN = 7;
static const int NORMAL_HOSTAGE_RUNAWAY_BS_FACTOR[HOSTAGE_RUNAWAY_BS_LEN] =
{52,65,77,88,94,98,100};
static const int HIGH_HOSTAGE_RUNAWAY_BS_FACTOR[HOSTAGE_RUNAWAY_BS_LEN] =
{32,40,58,75,88,96,100};

//Scaling of hostage value by number of steps to break the hostage by attacking the holder
static const int HOSTAGE_HOLDER_BS_LEN = 12;
static const int HOSTAGE_HOLDER_BS_FACTOR[HOSTAGE_HOLDER_BS_LEN] =
{30,34,45,55,67,80,92,96,100,100,100,100};

//Scaling of hostage value by trap control
static const int HOSTAGE_OPP_TSTAB_LEN = 40;
static const int HOSTAGE_OPP_TSTAB_RATIO[HOSTAGE_OPP_TSTAB_LEN] =
{100,99,98,97,95,94,92,90,88,86, //100 to 65
  84,81,78,75,72,69,66,63,59,55, //65 to 30
  51,47,43,39,36,33,30,27,25,22, //30 to (-5)
  20,18,16,15,13,12,11,10, 9, 8, //(-5) to(-40)
};

//Scaling of hostage value by the progression of a swarm
static const int HOSTAGE_SWARMY_LEN = 13;
static const int HOSTAGE_SWARMY_FACTOR[HOSTAGE_SWARMY_LEN] =
{100,97,93,88,83,79,75,72,69,67,65,64,63};

//Penalizing the strongest free piece for being close to the hostage
static const int HOSTAGE_SFP_DIST_LEN = 6;
static const int HOSTAGE_SFP_DIST_FACTOR[HOSTAGE_SFP_DIST_LEN] =
{60,75,85,92,100,100};
static const int HOSTAGE_SECONDARY_SFP_DIST_LEN = 6;
static const int HOSTAGE_SECONDARY_SFP_DIST_FACTOR[HOSTAGE_SECONDARY_SFP_DIST_LEN] =
{85,90,96,98,100,100};

static const int HOSTAGE_SFP_LOGISTIC_DIV = 280;
//Mobility loss for the hostaged piece, for SFP
static const double HOSTAGEHELDMOBLOSS = 70;
//Mobility loss for a holder, by location, for SFP
static const double HOSTAGEMOBLOSS[BSIZE] = {
 65, 65, 65, 64, 64, 65, 65, 65,  0,0,0,0,0,0,0,0,
 64, 63, 61, 59, 59, 61, 63, 64,  0,0,0,0,0,0,0,0,
 62, 58, 54, 52, 52, 54, 58, 62,  0,0,0,0,0,0,0,0,
 61, 49, 48, 42, 42, 48, 49, 61,  0,0,0,0,0,0,0,0,
 58, 49, 48, 39, 39, 48, 49, 58,  0,0,0,0,0,0,0,0,
 58, 55, 42, 41, 41, 42, 55, 58,  0,0,0,0,0,0,0,0,
 61, 60, 60, 51, 51, 60, 60, 61,  0,0,0,0,0,0,0,0,
 64, 63, 62, 61, 61, 62, 63, 64,  0,0,0,0,0,0,0,0,
};

//Mobility loss factor for non-rabbit buried pieces, as a fraction of HOSTAGEMOBLOSS, out of 64, by numstronger
static const double BURIED_MOB_LOSS_NS_FACTOR[7] = {56,54,48,36,24,16,8};
//Mobility loss factor by location for non-rabbit buried pieces
static const double BURIED_MOB_LOSS_LOC_FACTOR[BSIZE] = {
 64, 64, 56, 20, 20, 56, 64, 64,  0,0,0,0,0,0,0,0,
 64, 58, 24, 10, 10, 24, 58, 64,  0,0,0,0,0,0,0,0,
 50, 26, 16,  0,  0, 16, 26, 50,  0,0,0,0,0,0,0,0,
 24, 16,  0,  0,  0,  0, 16, 24,  0,0,0,0,0,0,0,0,
  8,  8,  0,  0,  0,  0,  8,  8,  0,0,0,0,0,0,0,0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,0,0,0,0,0,0,0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,0,0,0,0,0,0,0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,0,0,0,0,0,0,0,
};
//By trap index, locations where we check for buried pieces
static const Bitmap BURIED_MAP[4] = {
Bitmap(0x0000000303070F0FULL),
Bitmap(0x000000C0C0E0F0F0ULL),
Bitmap(0x0F0F070303000000ULL),
Bitmap(0xF0F0E0C0C0000000ULL),
};


//[ktindex][loc]
static const int SWARMYFACTOR[4][BSIZE] = {
{
3,3,4,3,1,0,0,0,  0,0,0,0,0,0,0,0,
3,4,5,2,1,0,0,0,  0,0,0,0,0,0,0,0,
4,5,5,4,2,0,0,0,  0,0,0,0,0,0,0,0,
3,4,4,3,1,0,0,0,  0,0,0,0,0,0,0,0,
2,3,3,2,0,0,0,0,  0,0,0,0,0,0,0,0,
1,2,2,1,0,0,0,0,  0,0,0,0,0,0,0,0,
0,1,1,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
},
{
0,0,0,1,3,4,3,3,  0,0,0,0,0,0,0,0,
0,0,0,1,2,5,4,3,  0,0,0,0,0,0,0,0,
0,0,0,2,4,5,5,4,  0,0,0,0,0,0,0,0,
0,0,0,1,3,4,4,3,  0,0,0,0,0,0,0,0,
0,0,0,0,2,3,3,2,  0,0,0,0,0,0,0,0,
0,0,0,0,1,2,2,1,  0,0,0,0,0,0,0,0,
0,0,0,0,0,1,1,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
},
{
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,1,1,0,0,0,0,0,  0,0,0,0,0,0,0,0,
1,2,2,1,0,0,0,0,  0,0,0,0,0,0,0,0,
2,3,3,2,0,0,0,0,  0,0,0,0,0,0,0,0,
3,4,4,3,1,0,0,0,  0,0,0,0,0,0,0,0,
4,5,5,4,2,0,0,0,  0,0,0,0,0,0,0,0,
3,4,5,2,1,0,0,0,  0,0,0,0,0,0,0,0,
3,3,4,3,1,0,0,0,  0,0,0,0,0,0,0,0,
},
{
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,1,1,0,  0,0,0,0,0,0,0,0,
0,0,0,0,1,2,2,1,  0,0,0,0,0,0,0,0,
0,0,0,0,2,3,3,2,  0,0,0,0,0,0,0,0,
0,0,0,1,3,4,4,3,  0,0,0,0,0,0,0,0,
0,0,0,2,4,5,5,4,  0,0,0,0,0,0,0,0,
0,0,0,1,2,5,4,3,  0,0,0,0,0,0,0,0,
0,0,0,1,3,4,3,3,  0,0,0,0,0,0,0,0,
},
};

//Swarm is good for opponent if holder is ELE and HOSTAGE_SWARM_GOOD[holderLoc] == pla
static const int HOSTAGE_SWARM_GOOD[BSIZE] = {
GOLD,GOLD,GOLD,GOLD,GOLD,GOLD,GOLD,GOLD,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
GOLD,GOLD,GOLD,GOLD,GOLD,GOLD,GOLD,GOLD,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
GOLD,GOLD,NPLA,NPLA,NPLA,NPLA,GOLD,GOLD,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
GOLD,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,GOLD,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
SILV,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,SILV,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
SILV,SILV,NPLA,NPLA,NPLA,NPLA,SILV,SILV,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
SILV,SILV,SILV,SILV,SILV,SILV,SILV,SILV,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
SILV,SILV,SILV,SILV,SILV,SILV,SILV,SILV,  NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,NPLA,
};

static double getHostageMobLoss(pla_t pla, loc_t loc)
{
  if(pla == GOLD)
    return HOSTAGEMOBLOSS[loc]/100.0;
  else
    return HOSTAGEMOBLOSS[gRevLoc(loc)]/100.0;
}

static bool isHostageIndefensible(const Board& b, pla_t pla, HostageThreat& threat, int runawayBS, int holderAttackDist, int threatSteps,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE])
{
  return false; //TODO should work, but is a bit fragile right now, so disabled

  pla_t opp = gOpp(pla);
  loc_t kt = threat.kt;
  int trapIndex = Board::TRAPINDEX[kt];
  //Must be unable to runaway or break the hostage, and must have no opp defenders and at least 2 pla defenders
  if(!(runawayBS > 4 && holderAttackDist > 2 && (b.isGuarded(pla,threat.holderLoc) || holderAttackDist > 4)
      && b.trapGuardCounts[opp][trapIndex] == 0 && b.trapGuardCounts[pla][trapIndex] >= 2 && threatSteps <= 4))
    return false;
  //Find the strongest piece within rad 2 of the trap
  piece_t strongest = ELE;
  while(strongest > RAB && (b.pieceMaps[pla][strongest] & Board::DISK[2][kt]).isEmpty())
    strongest--;
  if(strongest <= RAB)
    return false;

  //Check if any opponent pieces at least that strong could possibly reach in time
  Bitmap oppMap = pStrongerMaps[pla][strongest-1];
  if((Board::DISK[1][kt] & oppMap).hasBits())
    return false;
  int moveAdjDist = Threats::moveAdjDistUF(b,opp,kt,5,ufDist,pStrongerMaps,strongest-1);
  if(moveAdjDist < 5)
    return false;

  //Check that the total time to move multi pieces in to defend everything is too large
  //and that none of pla's current defenders can be pushed.
  int totalDist = 0;
  int maxDistToTest = 5;
  for(int i = 0; i<4; i++)
  {
    loc_t loc = Board::TRAPDEFLOCS[trapIndex][i];
    if(b.owners[loc] == pla)
    {
      if(Threats::attackDist(b,loc,3,pStrongerMaps,ufDist) + 2 < 5)
        return false;
    }
    else if(b.owners[loc] == NPLA)
    {
      int dist = Threats::occupyDist(b,opp,loc,maxDistToTest,pStrongerMaps,ufDist);
      totalDist += maxDistToTest;
      if(dist < 2)
        return false;
      maxDistToTest -= dist;
      if(maxDistToTest < 2) maxDistToTest = 2;
    }
  }
  if(totalDist < 5)
    return false;

  return true;
}

static int getHostageRunawayBreakSteps(const Board& b, pla_t pla, loc_t hostageLoc, loc_t holderLoc, const int ufDist[BSIZE])
{
  //Basic value is the ufdist for the hostage plus one
  int dist = ufDist[hostageLoc]+1;

  //If its a rabbit, then it's extra hard to run away since there's often nowhere safe to run to
  if(b.pieces[hostageLoc] == RAB)
    dist++;
  //Subtract one if the hostage is actually literally unfrozen and not a rabbit
  else if(b.isThawed(hostageLoc))
    dist--;

  //For each surrounding square, look at how escapable that square is
  pla_t opp = gOpp(pla);
  int minBlockedSteps = 6;
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][hostageLoc])
      continue;
    loc_t adj = hostageLoc + Board::ADJOFFSETS[i];
    if(!b.isRabOkay(opp,hostageLoc,hostageLoc,adj))
      continue;

    int blockedSteps = 0;
    //If the direction is towards the holder, then blocksteps is at least 4
    if(Board::manhattanDist(adj,holderLoc) < Board::manhattanDist(hostageLoc,holderLoc))
      blockedSteps = 4;
    //If the direction is adjacent to a trap, then blocksteps is at least 1
    else if(Board::ADJACENTTRAP[adj] != ERRLOC)
      blockedSteps += 1;

    if(!ISE(adj))
      blockedSteps++;
    if(ISO(adj) && !b.wouldBeUF(opp,hostageLoc,hostageLoc,adj))
      blockedSteps += 2;
    if(ISP(adj) && GEQ(adj,hostageLoc))
      blockedSteps += 2;
    if(!b.isOpen(adj))
      blockedSteps += 3;

    if(blockedSteps < minBlockedSteps)
      minBlockedSteps = blockedSteps;
  }

  dist += min(minBlockedSteps,2);
  if(dist < minBlockedSteps)
    dist = minBlockedSteps;

  return dist;
}

static const int HOSTAGE_PULL_GUARD_OFFSETS[BSIZE] = {
  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
  N,  N,  E,  N,  N,  W,  N,  N,    0,  0,  0,  0,  0,  0,  0,  0,
  N,  N,  0,  0,  0,  0,  N,  N,    0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
  S,  S,  0,  0,  0,  0,  S,  S,    0,  0,  0,  0,  0,  0,  0,  0,
  S,  S,  E,  S,  S,  W,  S,  S,    0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
};
static const int HOSTAGE_PULL_GUARD_BONUS[BSIZE] = {
 2, 2, 2, 2, 2, 2, 2, 2,  0,0,0,0,0,0,0,0,
 2, 2, 2, 2, 2, 2, 2, 2,  0,0,0,0,0,0,0,0,
 2, 2, 0,-1,-1, 0, 2, 2,  0,0,0,0,0,0,0,0,
-1,-1,-1, 0, 0,-1,-1,-1,  0,0,0,0,0,0,0,0,
-1,-1,-1, 0, 0,-1,-1,-1,  0,0,0,0,0,0,0,0,
 2, 2, 0,-1,-1, 0, 2, 2,  0,0,0,0,0,0,0,0,
 2, 2, 2, 2, 2, 2, 2, 2,  0,0,0,0,0,0,0,0,
 2, 2, 2, 2, 2, 2, 2, 2,  0,0,0,0,0,0,0,0,
};

static int getHostageHolderBreakSteps(const Board& b, pla_t pla, loc_t holderLoc, int holderAttackDist)
{
  if(b.pieces[holderLoc] == ELE)
    return 16;

  //Base attack distance, plus 2 is the break distance.
  int dist = holderAttackDist+2;

  //Check for pull guarding and add the appropriate amount of protection
  int pullGuardOffset = HOSTAGE_PULL_GUARD_OFFSETS[holderLoc];
  if(pullGuardOffset == 0 || b.owners[holderLoc+pullGuardOffset] == pla || b.wouldBeG(pla,holderLoc+pullGuardOffset,holderLoc))
    dist += HOSTAGE_PULL_GUARD_BONUS[holderLoc];

  //Also add a bonus if the piece is guarded
  if(b.isGuarded(pla,holderLoc))
    dist++;

  if(dist < 0)
    dist = 0;

  return dist;
}


static int getHostageNoETrapControl(const Board& b, pla_t pla, loc_t kt, const int ufDist[BSIZE], const int tc[2][4])
{
  pla_t opp = gOpp(pla);
  int trapIndex = Board::TRAPINDEX[kt];
  //adding the contribution of opponent ele removes its contribution since the opponent is negative
  return tc[pla][trapIndex] + Eval::elephantTrapControlContribution(b,opp,ufDist,trapIndex);
}

static void evalHostageSFP(const Board& b, pla_t pla, loc_t kt, piece_t biggestHostage,
    const int pStronger[2][NUMTYPES], const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE],
    const HostageThreat* threats, int numThreats, double* advancementGood,
    double& baseValueBuf, double& defendedValueBuf,
    double* plaPieceCounts, double* oppPieceCounts, ostream* out)
{
  //Copy over counts
  pla_t opp = gOpp(pla);
  for(int piece = ELE; piece > EMP; piece--)
  {
    plaPieceCounts[piece] = b.pieceCounts[pla][piece];
    oppPieceCounts[piece] = b.pieceCounts[opp][piece];
  }

  //Subtract off mobility of involved hostages and holders
  Bitmap counted = Bitmap();
  piece_t strongestHostage = EMP;
  for(int i = 0; i<numThreats; i++)
  {
    //Hostaged piece
    loc_t hostageLoc = threats[i].hostageLoc;
    double prop = HOSTAGEHELDMOBLOSS/100.0;
    oppPieceCounts[b.pieces[hostageLoc]] -= prop;
    oppPieceCounts[0] -= prop;

    if(b.pieces[hostageLoc] > strongestHostage)
      strongestHostage = b.pieces[hostageLoc];

    loc_t holderLoc = threats[i].holderLoc;
    loc_t holderLoc2 = threats[i].holderLoc2;
    if(counted.isOne(holderLoc) || (holderLoc2 != ERRLOC && counted.isOne(holderLoc2)))
      continue;

    //Holding piece
    prop = getHostageMobLoss(pla,holderLoc);
    plaPieceCounts[b.pieces[holderLoc]] -= prop;
    plaPieceCounts[0] -= prop;
    counted.setOn(holderLoc);
  }

  //Subtract off mobility of buried pieces
  //Only pieces less than or equal to the strongest hostage can be buried - anything stronger than the hostage should be able to get out
  int trapIndex = Board::TRAPINDEX[kt];
  Bitmap buried = b.pieceMaps[pla][0] & ~b.pieceMaps[pla][RAB] & BURIED_MAP[trapIndex] & ~counted & ~pStrongerMaps[opp][strongestHostage];
  int dy = pla == GOLD ? N : S;
  while(buried.hasBits())
  {
    loc_t loc = buried.nextBit();
    double locFactor;
    if(ufDist[loc] >= 2)
      locFactor = min(64.0,BURIED_MOB_LOSS_LOC_FACTOR[loc] * 1.5 + 8);
    else if(ufDist[loc] >= 1)
      locFactor = BURIED_MOB_LOSS_LOC_FACTOR[loc];
    else
    {
      if(ISE(loc+dy)) locFactor = BURIED_MOB_LOSS_LOC_FACTOR[loc+dy];
      else if(ISP(loc+dy) && b.isOpenToStep(loc+dy)) locFactor = 0.5*(BURIED_MOB_LOSS_LOC_FACTOR[loc] + BURIED_MOB_LOSS_LOC_FACTOR[loc+dy]);
      else if(ISO(loc+dy) && GT(loc,loc+dy) && b.isOpen(loc+dy)) locFactor = 0.5*(BURIED_MOB_LOSS_LOC_FACTOR[loc] + BURIED_MOB_LOSS_LOC_FACTOR[loc+dy]);
      else locFactor = BURIED_MOB_LOSS_LOC_FACTOR[loc];
    }

    int nStronger = pStronger[pla][b.pieces[loc]];
    double prop = BURIED_MOB_LOSS_NS_FACTOR[nStronger] * locFactor * getHostageMobLoss(pla,loc) / 4096.0;
    plaPieceCounts[b.pieces[loc]] -= prop;
    plaPieceCounts[0] -= prop;
  }

  //Mobility for all defenders
  double oppKTMobilityProp = getHostageMobLoss(opp,kt);

  //Mark off strongest piece (usually ELE) and compute its SFP, then undo it
  int baseSFPVal = 0;
  piece_t strongestOppPiece = ELE;
  for(int piece = ELE; piece >= RAB; piece--)
  {
    if(b.pieceCounts[opp][piece] > 0)
    {
      if(oppPieceCounts[piece] >= oppKTMobilityProp)
      {
        oppPieceCounts[piece] -= oppKTMobilityProp;
        oppPieceCounts[0] -= oppKTMobilityProp;
        baseSFPVal = EvalShared::computeSFPScore(plaPieceCounts,oppPieceCounts,advancementGood,biggestHostage);
        oppPieceCounts[piece] += oppKTMobilityProp;
        oppPieceCounts[0] += oppKTMobilityProp;
        strongestOppPiece = piece;
        break;
      }
    }
  }

  //Mark out all weaker defenders
  int numOtherDefendersFound = 0;
  for(int i = 0; i<4; i++)
  {
    loc_t loc = kt + Board::ADJOFFSETS[i];
    if(b.owners[loc] == opp && b.pieces[loc] < strongestOppPiece)
    {
      numOtherDefendersFound++;
      oppPieceCounts[b.pieces[loc]] -= oppKTMobilityProp;
      oppPieceCounts[0] -= oppKTMobilityProp;
    }
  }

  //If too few defenders, mark out the next highest ones possible till we have enough
  int numDefendersNeeded = 2-numOtherDefendersFound;
  for(int piece = strongestOppPiece-1; piece >= RAB && numDefendersNeeded > 0; piece--)
  {
    while(oppPieceCounts[piece] >= oppKTMobilityProp && numDefendersNeeded > 0)
    {
      oppPieceCounts[piece] -= oppKTMobilityProp;
      oppPieceCounts[0] -= oppKTMobilityProp;
      numDefendersNeeded--;
    }
  }

  int defendedSFPVal = EvalShared::computeSFPScore(plaPieceCounts,oppPieceCounts);

  //Compute values
  double baseValue = EvalShared::computeLogistic(baseSFPVal,HOSTAGE_SFP_LOGISTIC_DIV,10000)/10000.0;
  double defendedValue = EvalShared::computeLogistic(defendedSFPVal,HOSTAGE_SFP_LOGISTIC_DIV,10000)/10000.0;
  if(baseValue <= 0)
    baseValue = 0;
  if(defendedValue <= 0)
    defendedValue = 0;
  if(defendedValue > baseValue)
    defendedValue = baseValue;

  baseValueBuf = baseValue;
  defendedValueBuf = defendedValue;

  ONLYINDEBUG(if(out)
  {
    (*out) << "Hostages at trap " << Board::writeLoc(kt);
    (*out) << " sfpbase " << baseValue << " sfpdef " << defendedValue << endl;
  })
}

static double evalHostageProp(const Board& b, pla_t pla, HostageThreat threat, int threatSteps,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], const int tc[2][4], bool& indefensible,
    double baseSFPValue, double defendedSFPValue, const double* plaPieceCounts, const double* oppPieceCounts, ostream* out)
{
  pla_t opp = gOpp(pla);
  loc_t kt = threat.kt;
  loc_t hostageLoc = threat.hostageLoc;
  loc_t holderLoc = threat.holderLoc;

  //Adjust ratio by trap stability
  int oppTrapStab = (100-getHostageNoETrapControl(b,pla,kt,ufDist,tc))*HOSTAGE_OPP_TSTAB_LEN/140;
  double ratio;
  if(oppTrapStab >= HOSTAGE_OPP_TSTAB_LEN)
    ratio = HOSTAGE_OPP_TSTAB_RATIO[HOSTAGE_OPP_TSTAB_LEN-1]/100.0;
  else if(oppTrapStab < 0)
    ratio = 1;
  else
    ratio = HOSTAGE_OPP_TSTAB_RATIO[oppTrapStab]/100.0;

  //Adjust ratio by piece swarming for defender
  int swarmyness = 0;
  if(b.pieces[holderLoc] == ELE && HOSTAGE_SWARM_GOOD[holderLoc] == pla)
  {
    int ktIndex = Board::TRAPINDEX[kt];
    Bitmap map = b.pieceMaps[opp][0];
    while(map.hasBits())
    {
      loc_t loc = map.nextBit();
      if(loc != hostageLoc && b.pieces[loc] != ELE)
        swarmyness += (b.pieces[loc]+3) * SWARMYFACTOR[ktIndex][loc];
    }
    swarmyness /= 13;
    ratio *= HOSTAGE_SWARMY_FACTOR[swarmyness >= HOSTAGE_SWARMY_LEN ? HOSTAGE_SWARMY_LEN-1 : swarmyness]/100.0;
  }

  double value = baseSFPValue*ratio + defendedSFPValue*(1-ratio);

  //Adjust by breakSteps
  int holderAttackDist = Threats::attackDist(b,holderLoc,5,pStrongerMaps,ufDist);
  int runawayBS = getHostageRunawayBreakSteps(b, pla, hostageLoc, holderLoc, ufDist);
  int holderBS = getHostageHolderBreakSteps(b, pla, holderLoc, holderAttackDist);

  bool isHighHostage = Board::GOALYDIST[opp][hostageLoc] >= 3;

  runawayBS = runawayBS >= HOSTAGE_RUNAWAY_BS_LEN ? HOSTAGE_RUNAWAY_BS_LEN-1 : runawayBS;
  holderBS = holderBS >= HOSTAGE_HOLDER_BS_LEN ? HOSTAGE_HOLDER_BS_LEN-1 : holderBS;
  int runawayBSFactor = isHighHostage ? HIGH_HOSTAGE_RUNAWAY_BS_FACTOR[runawayBS] : NORMAL_HOSTAGE_RUNAWAY_BS_FACTOR[runawayBS];
  int holderBSFactor = HOSTAGE_HOLDER_BS_FACTOR[holderBS];

  //Adjust further for high hostage - high hostage requires blocking pieces weaker than the hostage itself
  int blockCount = 0;
  if(isHighHostage && b.pieces[hostageLoc] > RAB)
  {
    int hx = gX(hostageLoc);
    int hy = gY(hostageLoc);
    int dy = Board::GOALYINC[pla];
    piece_t hostagePiece = b.pieces[hostageLoc];
    DEBUGASSERT(hx > 0 && hx < 7);
    DEBUGASSERT(hy > 2 && hy < 5);
    Bitmap blockArea = Board::XINTERVAL[hx-1][hx+1] & Board::YINTERVAL[hy][hy+dy*3];
    Bitmap blockingPieces = b.pieceMaps[pla][0] & ~pStrongerMaps[opp][hostagePiece-1] & blockArea;
    Bitmap oppEleAndHostages = b.pieceMaps[opp][ELE] | Bitmap::makeLoc(hostageLoc);
    while(blockingPieces.hasBits())
    {
      loc_t loc = blockingPieces.nextBit();
      blockCount += 2;
      if(gY(loc) == hy)
        blockCount++;
      Bitmap threateners = pStrongerMaps[pla][b.pieces[loc]] & ~oppEleAndHostages;
      if((Board::DISK[3][loc] & threateners).isEmpty())
        blockCount++;
      if((Board::DISK[4][loc] & threateners).isEmpty())
        blockCount++;
      if(threateners.isEmpty())
        blockCount += 5;
    }

    static const int BLOCKCOUNT_SCALE[12] = {16,20,24,28,32,36,40,44,48,56,60,64};
    if(blockCount >= 12)
      blockCount = 11;
    runawayBSFactor = (runawayBSFactor * BLOCKCOUNT_SCALE[blockCount]) >> 6;
  }

  //Compute final breaksteps factor
  int bsFactor;
  if(runawayBSFactor < holderBSFactor)   bsFactor = runawayBSFactor * (holderBSFactor+100)/2;
  else                                   bsFactor = holderBSFactor  * (runawayBSFactor+100)/2;
  value *= bsFactor / 10000.0;



  //Penalize close strongest free piece
  piece_t sfpStr = EMP;
  piece_t sfpStr2 = EMP;
  for(piece_t p = ELE; p >= DOG; p--)
  {
    if(plaPieceCounts[p] > oppPieceCounts[p])
    {sfpStr = p; break;}
  }
  for(piece_t p = sfpStr-1; p >= DOG; p--)
  {
    if(plaPieceCounts[p] > oppPieceCounts[p])
    {sfpStr2 = p; break;}
  }

  if(sfpStr != EMP)
  {
    Bitmap map = b.pieceMaps[pla][sfpStr];
    int totalDist = 0;
    int count = 0;
    //TODO make this corner-sensitive
    while(map.hasBits())
    {int d = Board::manhattanDist(holderLoc,map.nextBit()); totalDist += d > 6 ? 6 : d; count++;}
    if(count > 0)
    {
      int avgDist = totalDist/count;
      value *= HOSTAGE_SFP_DIST_FACTOR[avgDist >= HOSTAGE_SFP_DIST_LEN ? HOSTAGE_SFP_DIST_LEN-1 : avgDist] / 100.0;
    }
  }

  if(sfpStr2 != EMP)
  {
    Bitmap map = b.pieceMaps[pla][sfpStr2];
    int totalDist = 0;
    int count = 0;
    while(map.hasBits())
    {int d = Board::manhattanDist(holderLoc,map.nextBit()); totalDist += d > 6 ? 6 : d; count++;}
    if(count > 0)
    {
      int avgDist = totalDist/count;
      value *= HOSTAGE_SECONDARY_SFP_DIST_FACTOR[avgDist >= HOSTAGE_SECONDARY_SFP_DIST_LEN ? HOSTAGE_SECONDARY_SFP_DIST_LEN-1 : avgDist] / 100.0;
    }
  }
  indefensible = isHostageIndefensible(b,pla,threat,runawayBS,holderAttackDist,threatSteps,pStrongerMaps,ufDist);

  ONLYINDEBUG(if(out)
  {
    (*out) << threat
    << " baseval " << baseSFPValue
    << " oppTrapStab " << oppTrapStab
    << " swarmyness " << swarmyness
    << " ratioedval " << (baseSFPValue*ratio + defendedSFPValue*(1.0-ratio)) << endl
    << " blockCount " << blockCount
    << " runawayBS " << runawayBS
    << " holderBS " << holderBS
    << " indef " << indefensible
    << " propvalue " << value << endl;
  })

  return value;
}


static Strat getSingleEval(const Board& b, pla_t pla, HostageThreat threat, const eval_t pValues[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], const int tc[2][4],
    double baseSFPValue, double defendedSFPValue, const double* plaPieceCounts, const double* oppPieceCounts,
    const double* advancementGood,
    ostream* out)
{
  pla_t opp = gOpp(pla);
  loc_t kt = threat.kt;
  loc_t hostageLoc = threat.hostageLoc;
  loc_t holderLoc = threat.holderLoc;

  bool indefensible = false;
  int threatSteps = getHostageThreatDist(b,pla,threat,ufDist);
  int hostageValue = hostageThreatValue(b,pValues[opp],hostageLoc,threatSteps);
  double prop = evalHostageProp(b,pla,threat,threatSteps,pStrongerMaps,ufDist,tc,indefensible,
      baseSFPValue, defendedSFPValue, plaPieceCounts, oppPieceCounts, out);

  double sfpAdvGoodFactor = EvalShared::computeSFPAdvGoodFactor(b,pla,kt,advancementGood);
  prop *= sfpAdvGoodFactor;

  int value = (int)(hostageValue * prop);

  //Check if it looks outright indefensible
  if(value < hostageValue && indefensible)
    value = (hostageValue*2 + value)/3;

  Strat strat(value,hostageLoc,true,Bitmap::makeLoc(holderLoc));

  ONLYINDEBUG(if(out)
  {
    (*out) << " sfpAdvGoodFactor " << sfpAdvGoodFactor;
    (*out) << " threatSteps " << threatSteps << endl;
    (*out) << "Hostage final " << value << endl;
  })

  return strat;
}

int Hostages::getStrats(const Board& b, pla_t pla, const eval_t pValues[2][NUMTYPES], const int pStronger[2][NUMTYPES],
  const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], const int tc[2][4],
  Strat* strats, ostream* out)
{
  int numStrats = 0;
  HostageThreat hostages[Strats::maxHostagesPerKT];
  for(int i = 0; i<2; i++)
  {
    loc_t kt = Board::PLATRAPLOCS[pla][i];
    int numHostages = findHostages(b,pla,kt,hostages,pStrongerMaps,ufDist);
    if(numHostages <= 0)
      continue;

    double baseSFPValue = 0;
    double defendedSFPValue = 0;
    double plaPieceCounts[NUMTYPES];
    double oppPieceCounts[NUMTYPES];

    double advancementGood[NUMTYPES];

    piece_t biggestHostage = RAB;
    for(int j = 0; j<numHostages; j++)
    {
      piece_t piece = b.pieces[hostages[j].hostageLoc];
      if(piece > biggestHostage)
        biggestHostage = piece;
    }
    evalHostageSFP(b,pla,kt,biggestHostage,pStronger,pStrongerMaps,ufDist,hostages,numHostages,advancementGood,baseSFPValue,defendedSFPValue,
        plaPieceCounts,oppPieceCounts,out);
    for(int j = 0; j<numHostages; j++)
      strats[j+numStrats] = getSingleEval(b,pla,hostages[j],pValues,pStrongerMaps,ufDist,tc,
          baseSFPValue,defendedSFPValue,plaPieceCounts,oppPieceCounts,advancementGood,out);

    numStrats += numHostages;
  }
  return numStrats;
}


