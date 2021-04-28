/*
 * trapcontrol.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../eval/internal.h"

#include "../board/boardtreeconst.h"

//Base contributions for a piece depending on the number of opps stronger than it, globally and uf radius 3
static const int TC_PIECE_STR[9] =
{21,16,14,12,11,10,9,9,9};
static const int TC_NS3_STR[9] =
{11,6,3,1,0,0,0,0,0};

//Scales the contribution of a piece based on distance from trap
static const int TC_PIECE_RAD[16] =
{61,68,36,14,4,0,0,0,0,0,0,0,0,0,0,0};

//Scales the contribution of a piece based on whether it's dominated by UF.
static const int TC_NS1_FACTOR[2] =
{16,13};
//Scales the contribution of a piece based on its ufdist
static const int TC_FREEZE_FACTOR[UFDist::MAX_UF_DIST+5] =
{40,36,32,28,25,21,20,20,20,20,20};

//Power of 2 divisor for all of this stuff
static const int TC_DIVSHIFT = 15;
//Straight up add this much control based on the number of defenders
static const int TC_TRAP_NUM_DEFS[5] =
{0,2,11,20,21};

//FIXME test if this actually helps or hurts
static const int TC_ATTACKER_BONUS = 35; //Out of 32

//Bonuses for taking key squares with non-ELE
static const int TC_ATTACK_KEY_SQUARE = 7;
//Bonuses for threatening these squares with non-ELE
static const int TC_THREAT_KEY_SQUARE = 5;

//Bonus for having the strongest nonelephant piece in various radii of the trap
static const int TC_STRONGEST_NONELE_RAD2_TRAP = 5;
static const int TC_STRONGEST_NONELE_RAD3_TRAP = 3;

//Bonus for having a key square not pushable into
static const int TC_PUSHSAFER_KEY_SQUARE = 5;
static const int TC_STUFF_TRAP_CENTER = 50;
static const int TC_STUFF_TRAP_SCALEDIV = 3;
static const int TC_STUFF_TRAP_MAX = 20;


static int keySquareAttackBonus(const Board& b, pla_t pla, loc_t keyloc, const Bitmap pStrongerMaps[2][NUMTYPES])
{
  int tcVal = 0;
  if(b.owners[keyloc] == pla && b.pieces[keyloc] != ELE && !b.isDominated(keyloc))
    tcVal += TC_ATTACK_KEY_SQUARE;
  else if(b.owners[keyloc] != pla && (Board::RADIUS[1][keyloc] & pStrongerMaps[gOpp(pla)][b.pieces[keyloc]] & ~b.pieceMaps[pla][ELE] & ~b.dominatedMap).hasBits())
    tcVal += TC_THREAT_KEY_SQUARE;
  return tcVal;
}

static bool isPushSafer(const Board& b, pla_t pla, loc_t loc, const Bitmap pStrongerMaps[2][NUMTYPES])
{
  int plaCount = ISP(loc S) + ISP(loc W) + ISP(loc E) + ISP(loc N);
  if(plaCount >= 3)
    return true;
  return (pStrongerMaps[pla][b.pieces[loc]] & ~b.pieceMaps[gOpp(pla)][ELE] & Board::DISK[2][loc] & ~b.frozenMap).isEmpty();
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

static bool canPushOffTrap(const Board& b, pla_t pla, loc_t trapLoc)
{
  pla_t opp = gOpp(pla);
  if(ISO(trapLoc S) && GT(trapLoc, trapLoc S) && b.isOpen(trapLoc S)) return true;
  if(ISO(trapLoc W) && GT(trapLoc, trapLoc W) && b.isOpen(trapLoc W)) return true;
  if(ISO(trapLoc E) && GT(trapLoc, trapLoc E) && b.isOpen(trapLoc E)) return true;
  if(ISO(trapLoc N) && GT(trapLoc, trapLoc N) && b.isOpen(trapLoc N)) return true;
  return false;
}

int TrapControl::pieceContribution(const Board& b,
    const int pStronger[2][NUMTYPES], const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDists[BSIZE],
    pla_t owner, piece_t piece, loc_t loc, loc_t trapLoc)
{
  int mDist = Board::manhattanDist(loc,trapLoc);
  if(mDist <= 4)
  {
    int numStronger = pStronger[owner][piece];
    Bitmap strongerUF = pStrongerMaps[owner][piece] & ~b.frozenMap;
    int strScore = TC_PIECE_STR[numStronger] + TC_NS3_STR[(Board::DISK[3][loc] & strongerUF).countBits()];
    int freezeNS1Factor = TC_FREEZE_FACTOR[ufDists[loc]] * TC_NS1_FACTOR[(Board::RADIUS[1][loc] & strongerUF).hasBits()];
    strScore *= freezeNS1Factor;

    //An extra fudge factor to take into account the fact that strong pieces will tend to weaken the contribution of opp pieces.
    static const int EXTRA_CONTRIBUTION_SCALING[9] =
    {8,6,5,4,4,4,4,4,4}; //out of 4
    return (strScore * TC_PIECE_RAD[mDist] * EXTRA_CONTRIBUTION_SCALING[numStronger]) >> (TC_DIVSHIFT+2);
  }
  return 0;
}

//TODO tc should use restrict keyword?
void TrapControl::get(const Board& b, const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDists[BSIZE], int tc[2][4], int tcEle[2][4])
{
  for(int i = 0; i<4; i++)
  {
    tc[0][i] = 0;
    tc[1][i] = 0;
    tcEle[0][i] = 0;
    tcEle[1][i] = 0;
  }

  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      pla_t owner = b.owners[loc];
      if(owner == NPLA)
        continue;

      pla_t piece = b.pieces[loc];
      Bitmap strongerUF = pStrongerMaps[owner][piece] & ~b.frozenMap;

      int strScore = TC_PIECE_STR[pStronger[owner][piece]] + TC_NS3_STR[(Board::DISK[3][loc] & strongerUF).countBits()];
      int freezeNS1Factor = TC_FREEZE_FACTOR[ufDists[loc]] * TC_NS1_FACTOR[(Board::RADIUS[1][loc] & strongerUF).hasBits()];
      strScore *= freezeNS1Factor;

      for(int j = 0; j<4; j++)
      {
        int mDist = Board::manhattanDist(loc,Board::TRAPLOCS[j]);
        if(mDist <= 4)
        {
          tc[owner][j] += strScore * TC_PIECE_RAD[mDist];
          if(piece == ELE)
            tcEle[owner][j] += strScore * TC_PIECE_RAD[mDist];
        }
      }
    }
  }

  for(int i = 0; i<4; i++)
  {
    if(Board::ISPLATRAP[i][SILV]) tc[GOLD][i] = (tc[GOLD][i] * TC_ATTACKER_BONUS) >> 5;
    else                          tc[SILV][i] = (tc[SILV][i] * TC_ATTACKER_BONUS) >> 5;

    int tcVal = (tc[GOLD][i] >> TC_DIVSHIFT) - (tc[SILV][i] >> TC_DIVSHIFT);

    //Number of defenders
    tcVal += TC_TRAP_NUM_DEFS[b.trapGuardCounts[GOLD][i]];
    tcVal -= TC_TRAP_NUM_DEFS[b.trapGuardCounts[SILV][i]];

    //Attacks on key squares
    if(i <= 1)
    {
      tcVal -= keySquareAttackBonus(b,SILV,Board::TRAPBACKLOCS[i], pStrongerMaps);
      tcVal -= keySquareAttackBonus(b,SILV,Board::TRAPOUTSIDELOCS[i], pStrongerMaps);
    }
    else
    {
      tcVal += keySquareAttackBonus(b,GOLD,Board::TRAPBACKLOCS[i], pStrongerMaps);
      tcVal += keySquareAttackBonus(b,GOLD,Board::TRAPOUTSIDELOCS[i], pStrongerMaps);
    }

    //Strongest non-elephant piece
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

    tcEle[GOLD][i] = tcEle[GOLD][i] >> TC_DIVSHIFT;
    tcEle[SILV][i] = tcEle[SILV][i] >> TC_DIVSHIFT;
  }
}

static const int TC_SCORE_VALUE = 680;
static const int TC_SCORE_CENTER = 25;

//TODO decrease the bonus for controlling two opp traps? - controlling two traps a little is not as good as one trap a lot
//particularly if own traps are weak? The idea is to prevent overextension
eval_t TrapControl::getEval(pla_t pla, const int tc[2][4])
{
  eval_t silvAttackScore =
      Logistic::prop20(tc[SILV][0]-TC_SCORE_CENTER) +
      Logistic::prop20(tc[SILV][1]-TC_SCORE_CENTER);
  eval_t goldAttackScore =
      Logistic::prop20(tc[GOLD][2]-TC_SCORE_CENTER) +
      Logistic::prop20(tc[GOLD][3]-TC_SCORE_CENTER);
  silvAttackScore = (silvAttackScore * TC_SCORE_VALUE) >> Logistic::PROP_SHIFT;
  goldAttackScore = (goldAttackScore * TC_SCORE_VALUE) >> Logistic::PROP_SHIFT;

  if(pla == GOLD)
    return goldAttackScore - silvAttackScore;
  else
    return silvAttackScore - goldAttackScore;
}

static eval_t singleEval(pla_t pla, int idx, const int tc[2][4])
{
  if((idx < 2) == (pla == GOLD)) return -((Logistic::prop20(-tc[pla][idx]-TC_SCORE_CENTER) * TC_SCORE_VALUE) >> Logistic::PROP_SHIFT);
  else                           return  ((Logistic::prop20( tc[pla][idx]-TC_SCORE_CENTER) * TC_SCORE_VALUE) >> Logistic::PROP_SHIFT);
}

void TrapControl::printEval(pla_t pla, const int tc[2][4], ostream& out)
{
  out << Global::strprintf("  TC TC  Eval Eval: %4d %4d   %4d %4d\n",
      tc[pla][2],
      tc[pla][3],
      singleEval(pla,2,tc),
      singleEval(pla,3,tc));
  out << Global::strprintf("  TC TC  Eval Eval: %4d %4d   %4d %4d\n",
      tc[pla][0],
      tc[pla][1],
      singleEval(pla,0,tc),
      singleEval(pla,1,tc));
}

//Simple trap defense bonuses - having pieces next to traps

static const int TRAP_DEF_SCORE[5] = {0,0,75,95,95};
static const int TRAP_DEF_SCORE_BACK_BONUS = 60;
static const int TRAP_DEF_SCORE_SIDE_BONUS = 30;

eval_t TrapControl::getSimple(const Board& b)
{
  eval_t goldScore = 0;
  for(int i = 0; i<=1; i++)
  {
    goldScore += TRAP_DEF_SCORE[b.trapGuardCounts[GOLD][i]];
    if(b.owners[Board::TRAPBACKLOCS[i]] == GOLD)
      goldScore += TRAP_DEF_SCORE_BACK_BONUS;
    if(b.owners[Board::TRAPOUTSIDELOCS[i]] == GOLD)
      goldScore += TRAP_DEF_SCORE_SIDE_BONUS;
  }
  eval_t silvScore = 0;
  for(int i = 2; i<=3; i++)
  {
    silvScore += TRAP_DEF_SCORE[b.trapGuardCounts[SILV][i]];
    if(b.owners[Board::TRAPBACKLOCS[i]] == SILV)
      silvScore += TRAP_DEF_SCORE_BACK_BONUS;
    if(b.owners[Board::TRAPOUTSIDELOCS[i]] == SILV)
      silvScore += TRAP_DEF_SCORE_SIDE_BONUS;
  }
  return b.player == GOLD ? (goldScore-silvScore) : (silvScore-goldScore);
}
