
/*
 * evalthreats.cpp
 * Author: davidwu
 */

#include <cmath>
#include <cstdio>
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtreeconst.h"
#include "../eval/internal.h"
#include "../eval/evalparams.h"
#include "../eval/eval.h"
#include "../program/arimaaio.h"

//TRAP CONTROL--------------------------------------------------------------

static const int TC_PIECE_RAD[16] =
{18,20,9,3,1,0,0,0,0,0,0,0,0,0,0,0}; //Out of 20
static const int TC_NS3_PIECE_RAD[16] =
{18,20,13,5,2,0,0,0,0,0,0,0,0,0,0,0}; //Out of 20

static const int TC_PIECE_STR[9] =
{21,16,14,12,11,10,9,9,9};
static const int TC_NS3_STR[9] =
{11,6,3,1,0,0,0,0,0};

static const int TC_ATTACKER_BONUS = 110;

static const int TC_NS1_FACTOR[2] =
{16,13}; //Out of 16
static const int TC_FREEZE_FACTOR[UFDist::MAX_UF_DIST+5] =
{40,36,32,28,25,21,20,20,20,20,20}; //Out of 40

static const int TC_DIV = 15*16*40;
static const int TC_TRAP_NUM_DEFS[5] =
{0,2,11,20,21};

static const int TC_ATTACK_KEY_SQUARE = 7; //Bonuses for taking these squares with non-ELE
static const int TC_THREAT_KEY_SQUARE = 5; //Bonuses for threatening these squares with non-ELE
static const int TC_PUSHSAFER_KEY_SQUARE = 5; //Bonus for having a pushsafer key square
static const int TC_STUFF_TRAP_CENTER = 50;
static const int TC_STUFF_TRAP_SCALEDIV = 3;
static const int TC_STUFF_TRAP_MAX = 20;

static const int TC_STRONGEST_NONELE_RAD2_TRAP = 5;
static const int TC_STRONGEST_NONELE_RAD3_TRAP = 3;

static int LOGISTIC_ARR20[255];
static int LOGISTIC_ARR16[255];
static int LOGISTIC_INTEGRAL_ARR[255];

void Eval::initLogistic()
{
  for(int i = 0; i<255; i++)
  {
    int x = i-127;
    LOGISTIC_ARR20[i] = (int)(1000.0/(1.0+exp(-x/20.0)));
    LOGISTIC_ARR16[i] = (int)(1000.0/(1.0+exp(-x/16.0)));
    LOGISTIC_INTEGRAL_ARR[i] = (int)(1000.0*log(1.0+exp(x/20.0)));
  }
}

//Out of 1000,streched by 20
static int logisticProp20(int x)
{
  int i = x+127;
  if(i < 0)
    return LOGISTIC_ARR20[0];
  if(i > 254)
    return LOGISTIC_ARR20[254];
  return LOGISTIC_ARR20[i];
}

//Assumes not edge, assumes pla piece at loc!
static bool isPushSafer(const Board& b, pla_t pla, loc_t loc, const Bitmap pStrongerMaps[2][NUMTYPES])
{
  int plaCount = ISP(loc S) + ISP(loc W) + ISP(loc E) + ISP(loc N);
  if(plaCount >= 3)
    return true;
  return (pStrongerMaps[pla][b.pieces[loc]] & ~b.pieceMaps[gOpp(pla)][ELE] & Board::DISK[2][loc] & ~b.frozenMap).isEmpty();
}

static bool canPushOffTrap(const Board& b, pla_t pla, loc_t trapLoc)
{
  pla_t opp = gOpp(pla);
  if(ISO(trapLoc S) && GT(trapLoc, trapLoc S) && b.isOpen(trapLoc S)) return true;
  if(ISO(trapLoc W) && GT(trapLoc, trapLoc W) && b.isOpen(trapLoc W)) return true;
  if(ISO(trapLoc E) && GT(trapLoc, trapLoc E) && b.isOpen(trapLoc E)) return true;
  if(ISO(trapLoc N) && GT(trapLoc, trapLoc N) && b.isOpen(trapLoc N)) return true;
  return false;
}

static int numNonUndermined(const Board& b, pla_t pla, loc_t trapLoc)
{
  int num = 0;
  if(ISP(trapLoc S) && (!b.isDominatedByUF(trapLoc S) || !b.isOpen(trapLoc S))) num++;
  if(ISP(trapLoc W) && (!b.isDominatedByUF(trapLoc W) || !b.isOpen(trapLoc W))) num++;
  if(ISP(trapLoc E) && (!b.isDominatedByUF(trapLoc E) || !b.isOpen(trapLoc E))) num++;
  if(ISP(trapLoc N) && (!b.isDominatedByUF(trapLoc N) || !b.isOpen(trapLoc N))) num++;
  return num;
}

int Eval::elephantTrapControlContribution(const Board& b, pla_t pla, const int ufDists[BSIZE], int trapIndex)
{
  loc_t loc = b.findElephant(pla);
  if(loc == ERRLOC)
    return 0;

  int mDist = Board::manhattanDist(loc,Board::TRAPLOCS[trapIndex]);
  if(mDist > 4)
    return 0;

  int strScore = TC_PIECE_STR[0];
  int strScoreNs3 = TC_NS3_STR[0];
  bool isNS1 = false;
  int freezeNS1Factor = TC_FREEZE_FACTOR[ufDists[loc]] * TC_NS1_FACTOR[isNS1];
  int tc = 0;
  tc += strScore * TC_PIECE_RAD[mDist];
  tc += strScoreNs3 * TC_NS3_PIECE_RAD[mDist];
  tc *= freezeNS1Factor;
  if(Board::ISPLATRAP[trapIndex][gOpp(pla)])
    tc = tc * TC_ATTACKER_BONUS / 100;

  int tcVal = tc/TC_DIV;
  return tcVal;
}

void Eval::getBasicTrapControls(const Board& b, const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDists[BSIZE], int tc[2][4])
{
  for(int i = 0; i<4; i++)
  {
    tc[0][i] = 0;
    tc[1][i] = 0;
  }

  //Piece strength and influence, numstronger, freezing
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      pla_t owner = b.owners[loc];
      if(owner == NPLA)
        continue;
      Bitmap strongerUF = pStrongerMaps[owner][b.pieces[loc]] & ~b.frozenMap;
      int strScore = TC_PIECE_STR[pStronger[owner][b.pieces[loc]]];
      int strScoreNs3 = TC_NS3_STR[(Board::DISK[3][loc] & strongerUF).countBits()];
      int freezeNS1Factor = TC_FREEZE_FACTOR[ufDists[loc]] * TC_NS1_FACTOR[(Board::RADIUS[1][loc] & strongerUF).hasBits()];

      strScore *= freezeNS1Factor;
      strScoreNs3 *= freezeNS1Factor;
      for(int j = 0; j<4; j++)
      {
        int mDist = Board::manhattanDist(loc,Board::TRAPLOCS[j]);
        if(mDist <= 4)
        {
          tc[owner][j] += strScore * TC_PIECE_RAD[mDist];
          tc[owner][j] += strScoreNs3 * TC_NS3_PIECE_RAD[mDist];
        }
      }
    }
  }

  for(int i = 0; i<4; i++)
  {
    if(Board::ISPLATRAP[i][SILV]) tc[GOLD][i] = tc[GOLD][i] * TC_ATTACKER_BONUS / 100;
    else                          tc[SILV][i] = tc[SILV][i] * TC_ATTACKER_BONUS / 100;

    int tcVal = (tc[GOLD][i]-tc[SILV][i])/TC_DIV;
    loc_t trapLoc = Board::TRAPLOCS[i];

    for(piece_t piece = CAM; piece >= DOG; piece--)
    {
      bool g = (b.pieceMaps[GOLD][piece] & Board::DISK[2][trapLoc] & ~b.frozenMap).hasBits();
      bool s = (b.pieceMaps[SILV][piece] & Board::DISK[2][trapLoc] & ~b.frozenMap).hasBits();
      if(g && !s) tcVal += TC_STRONGEST_NONELE_RAD2_TRAP;
      else if (s && !g) tcVal -= TC_STRONGEST_NONELE_RAD2_TRAP;
      if(g || s)
        break;
    }
    for(piece_t piece = CAM; piece >= DOG; piece--)
    {
      bool g = (b.pieceMaps[GOLD][piece] & Board::DISK[3][trapLoc]).hasBits();
      bool s = (b.pieceMaps[SILV][piece] & Board::DISK[3][trapLoc]).hasBits();
      if(g && !s) tcVal += TC_STRONGEST_NONELE_RAD3_TRAP;
      else if (s && !g) tcVal -= TC_STRONGEST_NONELE_RAD3_TRAP;
      if(g || s)
        break;
    }

    //Key squares and phalanxes
    if(i <= 1)
    {
      loc_t keyloc = Board::TRAPBACKLOCS[i];
      if(b.owners[keyloc] == SILV && b.pieces[keyloc] != ELE)
        tcVal -= TC_ATTACK_KEY_SQUARE;
      else if(b.owners[keyloc] != SILV && (Board::RADIUS[1][keyloc] & pStrongerMaps[GOLD][b.pieces[keyloc]] & ~b.pieceMaps[SILV][ELE] & ~b.frozenMap).hasBits())
        tcVal -= TC_THREAT_KEY_SQUARE;

      keyloc = Board::TRAPOUTSIDELOCS[i];
      if(b.owners[keyloc] == SILV && b.pieces[keyloc] != ELE)
        tcVal -= TC_ATTACK_KEY_SQUARE;
      else if(b.owners[keyloc] != SILV && (Board::RADIUS[1][keyloc] & pStrongerMaps[GOLD][b.pieces[keyloc]] & ~b.pieceMaps[SILV][ELE] & ~b.frozenMap).hasBits())
        tcVal -= TC_THREAT_KEY_SQUARE;
    }
    else
    {
      loc_t keyloc = Board::TRAPBACKLOCS[i];
      if(b.owners[keyloc] == GOLD && b.pieces[keyloc] != ELE)
        tcVal += TC_ATTACK_KEY_SQUARE;
      else if(b.owners[keyloc] != GOLD && (Board::RADIUS[1][keyloc] & pStrongerMaps[SILV][b.pieces[keyloc]] & ~b.pieceMaps[GOLD][ELE] & ~b.frozenMap).hasBits())
        tcVal += TC_THREAT_KEY_SQUARE;

      keyloc = Board::TRAPOUTSIDELOCS[i];
      if(b.owners[keyloc] == GOLD && b.pieces[keyloc] != ELE)
        tcVal += TC_ATTACK_KEY_SQUARE;
      else if(b.owners[keyloc] != GOLD && (Board::RADIUS[1][keyloc] & pStrongerMaps[SILV][b.pieces[keyloc]] & ~b.pieceMaps[GOLD][ELE] & ~b.frozenMap).hasBits())
        tcVal += TC_THREAT_KEY_SQUARE;
    }

    //Number of defenders
    tcVal += TC_TRAP_NUM_DEFS[b.trapGuardCounts[GOLD][i]];
    tcVal -= TC_TRAP_NUM_DEFS[b.trapGuardCounts[SILV][i]];

    //Piece on trap
    int trapTcVal = 0;
    if(b.owners[trapLoc] == GOLD)
    {
      //Defense bonus for stuffing the trap, unless defenders are undermined
      if(b.trapGuardCounts[GOLD][i] >= 4 || numNonUndermined(b,GOLD,trapLoc) >= 2) {
        int v = tcVal < TC_STUFF_TRAP_CENTER ? (TC_STUFF_TRAP_CENTER - tcVal)/TC_STUFF_TRAP_SCALEDIV : 0;
        trapTcVal += v > TC_STUFF_TRAP_MAX ? TC_STUFF_TRAP_MAX : v;
      }
      //Attack penalty for stuffing the trap
      if((trapLoc == TRAP2 || trapLoc == TRAP3) && (b.trapGuardCounts[SILV][i] >= 2 || b.pieces[trapLoc] == RAB) && !canPushOffTrap(b,GOLD,trapLoc)) {
        int v = tcVal > TC_STUFF_TRAP_CENTER ? (TC_STUFF_TRAP_CENTER - tcVal)/TC_STUFF_TRAP_SCALEDIV : 0;
        trapTcVal += v < -TC_STUFF_TRAP_MAX ? -TC_STUFF_TRAP_MAX : v;
      }
      //Phalanx or pushsafe
      if(trapLoc == TRAP0 || trapLoc == TRAP1)
      {
        if(b.owners[Board::TRAPBACKLOCS[i]] == GOLD && b.trapGuardCounts[GOLD][i] >= 2 && isPushSafer(b,GOLD,Board::TRAPBACKLOCS[i],pStrongerMaps))
          trapTcVal += TC_PUSHSAFER_KEY_SQUARE;

        if(b.owners[Board::TRAPOUTSIDELOCS[i]] == GOLD && b.trapGuardCounts[GOLD][i] >= 2 && b.owners[trapLoc] == GOLD && isPushSafer(b,GOLD,Board::TRAPOUTSIDELOCS[i],pStrongerMaps))
          trapTcVal += TC_PUSHSAFER_KEY_SQUARE;
      }
    }
    else if(b.owners[trapLoc] == SILV)
    {
      //Defense bonus for stuffing the trap
      if(b.trapGuardCounts[SILV][i] >= 4 || numNonUndermined(b,SILV,trapLoc) >= 2) {
        int v = tcVal > -TC_STUFF_TRAP_CENTER ? (TC_STUFF_TRAP_CENTER + tcVal)/TC_STUFF_TRAP_SCALEDIV : 0;
        trapTcVal -= v > TC_STUFF_TRAP_MAX ? TC_STUFF_TRAP_MAX : v;
      }
      //Attack penalty for stuffing the trap
      if((trapLoc == TRAP0 || trapLoc == TRAP1) && (b.trapGuardCounts[GOLD][i] >= 2 || b.pieces[trapLoc] == RAB) && !canPushOffTrap(b,SILV,trapLoc)) {
        int v = tcVal < -TC_STUFF_TRAP_CENTER ? (TC_STUFF_TRAP_CENTER + tcVal)/TC_STUFF_TRAP_SCALEDIV : 0;
        trapTcVal -= v < -TC_STUFF_TRAP_MAX ? -TC_STUFF_TRAP_MAX : v;
      }

      if(trapLoc == TRAP2 || trapLoc == TRAP3)
      {
        //Phalanx or pushsafe
        if(b.owners[Board::TRAPBACKLOCS[i]] == SILV && b.trapGuardCounts[SILV][i] >= 2 && isPushSafer(b,SILV,Board::TRAPBACKLOCS[i],pStrongerMaps))
          trapTcVal -= TC_PUSHSAFER_KEY_SQUARE;

        //Phalanx or pushsafe
        if(b.owners[Board::TRAPOUTSIDELOCS[i]] == SILV && b.trapGuardCounts[SILV][i] >= 2 && b.owners[trapLoc] == SILV && isPushSafer(b,SILV,Board::TRAPOUTSIDELOCS[i],pStrongerMaps))
          trapTcVal -= TC_PUSHSAFER_KEY_SQUARE;
      }
    }
    int finalTrapVal = tcVal+trapTcVal;
    tc[GOLD][i] = finalTrapVal;
    tc[SILV][i] = -finalTrapVal;
  }
}


static const int TC_SCORE_VALUE = 850;
static const int TC_SCORE_CENTER = 25;


eval_t Eval::getTrapControlScore(pla_t pla, int trapIndex, int control)
{
  if((trapIndex < 2) == (pla == GOLD))
    return -logisticProp20(-control-TC_SCORE_CENTER) * TC_SCORE_VALUE / 1000;
  else
    return  logisticProp20(control-TC_SCORE_CENTER) * TC_SCORE_VALUE / 1000;
}

//CAPTHREATS----------------------------------------------------------------------

static const int CAPTHREATDIV = 1000;
static const int capOccurPropLen = 24;
static const int capOccurProp[capOccurPropLen] =
{800,760,750,740,730,520,500,450,400,300,230,170,90,20,12,6,3,1,0,0,0,0,0,0};

int Eval::evalCapOccur(const Board& b, const eval_t values[NUMTYPES], loc_t loc, int dist)
{
  if(dist < 0) dist = 0;
  return dist >= capOccurPropLen ? 0 : (capOccurProp[dist]*values[b.pieces[loc]])/CAPTHREATDIV;
}

/*
static void computeGoalDistances(const Board& b, const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE], int goalDist[2][BSIZE], int max)
{
  loc_t queueBuf[32];
  loc_t queueBuf2[32];
  int extraSteps[BSIZE];

  for(int i = 0; i<BSIZE; i++)
    goalDist[0][i] = max;
  for(int i = 0; i<BSIZE; i++)
    goalDist[1][i] = max;

  for(int pla = 0; pla <= 1; pla++)
  {
    for(int i = 0; i<BSIZE; i++)
      extraSteps[i] = -1;

    int goalLocInc = Board::GOALLOCINC[pla];

    int numInQueue = 0;
    loc_t* oldQueue = queueBuf;
    loc_t* newQueue = queueBuf2;

    int goalOffset = (pla == GOLD) ? gLoc(0,7) : gLoc(0,0);
    for(int x = 0; x<8; x++)
    {
      loc_t loc = goalOffset+gDXOffset(x);
      pla_t owner = b.owners[loc];

      int extra = 0;
      if(owner == pla)
      {
        if(ufDist[loc] > 0 && !b.isOpen2(loc))
          extra = 3;// + 2*(!b.isOpenToPush(loc));
        //else if(ufDist[loc] == 0 && !b.isOpen(loc) && !b.isOpenToPush(loc))
        //  extra = 4;
        else
          extra = 1 + (!b.isOpenToStep(loc));
      }
      else if(owner == gOpp(pla))
        extra = 2
        + (pStrongerMaps[gOpp(pla)][b.pieces[loc]] & Board::RADIUS[1][loc] & ~b.frozenMap).isEmpty()
        + (pStrongerMaps[gOpp(pla)][b.pieces[loc]] & (Board::RADIUS[1][loc] | (Board::RADIUS[2][loc] & ~b.frozenMap))).isEmpty()
        + !((b.owners[loc W] == NPLA) || (b.owners[loc E] == NPLA));

      goalDist[pla][loc] = 0;
      extraSteps[loc] = extra;
      oldQueue[numInQueue++] = loc;
    }

    for(int dist = 0; dist < max-1; dist++)
    {
      int newNumInQueue = 0;
      for(int i = 0; i<numInQueue; i++)
      {
        loc_t oldLoc = oldQueue[i];
        int oldLocDist = goalDist[pla][oldLoc] + extraSteps[oldLoc];
        if(goalDist[pla][oldLoc] + extraSteps[oldLoc] != dist)
        {
          if(oldLocDist < max-1) //MINOR omit this check for speed?
            newQueue[newNumInQueue++] = oldLoc;
          continue;
        }

        for(int dir = 0; dir<4; dir++)
        {
          if(!Board::ADJOKAY[dir][oldLoc]) //MINOR make player specific directions only including forward but not back? Or vice versa?
            continue;
          loc_t loc = oldLoc + Board::ADJOFFSETS[dir];
          if(extraSteps[loc] >= 0)
            continue;

          goalDist[pla][loc] = dist+1;

          if(dist < max-2)
          {
            pla_t owner = b.owners[loc];
            int extra = 0;
            if(owner == NPLA)
              extra = (!b.isTrapSafe2(pla,loc) || (b.wouldRabbitBeDomAt(pla,loc) && !b.isGuarded2(pla,loc))) ? 1 : 0;
            else if(owner == pla)
            {
              if(ufDist[loc] > 0 && !b.isOpen2(loc))
                extra = 3;// + 2*(!b.isOpenToPush(loc));
              //else if(ufDist[loc] == 0 && !b.isOpen(loc) && !b.isOpenToPush(loc))
              //  extra = 4;
              else
                extra = 1 + (!b.isOpenToStep(loc,loc+goalLocInc));
            }
            else
              extra = 2
              + (pStrongerMaps[gOpp(pla)][b.pieces[loc]] & Board::RADIUS[1][loc] & ~b.frozenMap).isEmpty()
              + (pStrongerMaps[gOpp(pla)][b.pieces[loc]] & (Board::RADIUS[1][loc] | (Board::RADIUS[2][loc] & ~b.frozenMap))).isEmpty()
              + (b.pieces[loc] != RAB);

            extraSteps[loc] = extra;
            newQueue[newNumInQueue++] = loc;
          }
        }
      }
      loc_t* temp = newQueue;
      newQueue = oldQueue;
      oldQueue = temp;
      numInQueue = newNumInQueue;
    }
  }

}*/









