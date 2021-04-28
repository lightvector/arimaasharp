/*
 * placement.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../eval/internal.h"
#include "../eval/threats.h"

#include "../board/boardtreeconst.h"

static int getLocs(const Board& b, pla_t pla, piece_t piece, loc_t locs[2])
{
  Bitmap pieceMap = b.pieceMaps[pla][piece];
  if(pieceMap.isEmpty())
    return 0;
  locs[0] = pieceMap.nextBit();
  if(pieceMap.isEmpty())
    return 1;
  locs[1] = pieceMap.nextBit();
  if(pieceMap.isEmpty())
    return 2;
  return 3;
}

//CAMEL ADVANCEMENT--------------------------------------------------------------------
//Some slightly hacky extra code to reason about when and when not it is good to advance a camel
// (or generally the second strongest piece)
//It's good to advance when...
// Other pieces are in front of you.
// You have a piece either at A7 or C7
// You control the diagonal trap (or it's contested, where your tc is good and especially if your noeletc is good.
// The opponent's elephant is not near.
//
//It's not necessarily so great to advance if
//  The same-side trap is deadlocked Ele-Ele
//
//It's bad to advance when...
// You have no friends nearby
// You're the most advanced non-ele piece.

//[bigpiece][smallpiece]
static const int HDIST[8][8] =
{
    {0,0,2,3,5,6,7,7},
    {0,0,2,3,5,6,7,7},
    {1,1,0,2,4,5,6,6},
    {3,3,1,0,2,4,5,5},
    {5,5,4,2,0,1,3,3},
    {6,6,5,4,2,0,1,1},
    {7,7,6,5,3,2,0,0},
    {7,7,6,5,3,2,0,0},
};

static int hdist(loc_t bigLoc, loc_t smallLoc)
{
  return HDIST[gX(bigLoc)][gX(smallLoc)];
}

static const int HDIST_PENALTY[8] = {120,100,80,60,45,20,5,0};
static const int MDIST_PENALTY[16] = {190,190,170,150,110,75,40,20,10,0,0,0,0,0,0,0};

//Out of 64
static const int SHERIFF_PENALTY_MOD[BSIZE] = {
  0,  0,  0,  0,  0,  0,  0,  0,   0,0,0,0,0,0,0,0,
  0,  0,  0,  0,  0,  0,  0,  0,   0,0,0,0,0,0,0,0,
  6,  8,  8, 12, 12,  8,  8,  6,   0,0,0,0,0,0,0,0,
 20, 22, 28, 26, 26, 28, 22, 20,   0,0,0,0,0,0,0,0,
 40, 46, 52, 54, 54, 52, 46, 40,   0,0,0,0,0,0,0,0,
 68, 72, 64, 80, 80, 64, 72, 68,   0,0,0,0,0,0,0,0,
 54, 66, 60, 78, 78, 60, 66, 54,   0,0,0,0,0,0,0,0,
 16, 36, 42, 56, 56, 42, 36, 16,   0,0,0,0,0,0,0,0,
};

//Assuming camel is on the left and the trap is on the right
//How much does the diagonal trap "count" for reducing the penalty?
static const int DIAGONAL_TRAP_MOD[BSIZE] = {
   64, 64, 64, 50, 38, 22, 20, 20,   0,0,0,0,0,0,0,0,
   64, 64, 64, 50, 38, 22, 20, 20,   0,0,0,0,0,0,0,0,
   64, 64, 60, 50, 38, 22, 20, 20,   0,0,0,0,0,0,0,0,
   64, 64, 56, 40, 32, 22, 14, 12,   0,0,0,0,0,0,0,0,
   64, 64, 56, 40, 20, 24, 10, 10,   0,0,0,0,0,0,0,0,
   64, 64, 64, 30, 10,  0, 16,  8,   0,0,0,0,0,0,0,0,
   64, 64, 64, 40, 24, 30,  8,  8,   0,0,0,0,0,0,0,0,
   64, 64, 64, 40, 24, 20, 10,  8,   0,0,0,0,0,0,0,0,
};

//Incrementing by 5s, from -40 to 60
static const int TC_MOD[21] = {
 64,64,64,64,62, 59,55,51,47,43, 39, 35,31,27,23,20, 16,12,9,7,5,
};
//Incrementing by 5s, from -20 to 80
static const int TC_NOE_MOD[21] = {
 64,64,64,64,62, 59,55,51,47,43, 39, 35,31,27,23,20, 16,12,9,7,5,
};

//Number of pieces in front, piece in front roughly scores 6, side scores 4, behind scores 2
//Elephant also counts as a piece in front
static const int IN_FRONT_MOD[26] = {
  64,61,58,55,52,49,46,43,41,38,36,33,30,27,24,21,19,17,15,14,13,12,12,12,12
};
static const int PELE_MDIST_MOD[16] = {48,48,50,52,54,58,62,64,67,68,69,70,71,72,72,72};

static const int FROZEN_MOD = 72;


int Placement::getSheriffAdvancementThreat(const Board& b, pla_t pla, const int tc[2][4], const int tcEle[2][4], ostream* out)
{
  piece_t piece = CAM;
  while(piece > RAB && b.pieceCounts[pla][piece] == 0)
    piece--;
  if(piece <= RAB)
    return 0;

  loc_t locs[2];
  int numLocs = getLocs(b,pla,piece,locs);
  if(numLocs >= 2 || numLocs <= 0) return 0;
  loc_t loc = locs[0];

  pla_t opp = gOpp(pla);
  loc_t plaEleLoc = b.findElephant(pla);
  if(plaEleLoc == ERRLOC)
    return 0;
  loc_t oppEleLoc = b.findElephant(opp);
  if(oppEleLoc == ERRLOC)
    return 0;

  int gydist = Board::GOALYDIST[pla][loc];
  int spmod = SHERIFF_PENALTY_MOD[pla == GOLD ? loc : gRevLoc(loc)];
  if(spmod <= 0 || gydist >= 7)
    return 0;
  int base = HDIST_PENALTY[hdist(oppEleLoc,loc)] + MDIST_PENALTY[Board::manhattanDist(oppEleLoc,loc)];

  int infront = 0;
  int gy = (pla == GOLD) ? N : S;
  int dyEnd = gydist;
  if(b.owners[loc W-gy] == pla)
    infront += 2;
  if(b.owners[loc-gy] == pla)
    infront += 2;
  if(b.owners[loc E-gy] == pla)
    infront += 2;

  if(b.owners[loc WW] == pla)
    infront += 2;
  if(b.owners[loc W] == pla)
    infront += 4;
  if(b.owners[loc E] == pla)
    infront += 4;
  if(b.owners[loc EE] == pla)
    infront += 2;

  for(int dy = 1; dy <= dyEnd; dy++)
  {
    int infrontTemp = 0;
    if(b.owners[loc WW+dy*gy] == pla)
      infrontTemp += 3;
    if(b.owners[loc W+dy*gy] == pla)
      infrontTemp += 6;
    if(b.owners[loc+dy*gy] == pla)
      infrontTemp += 6;
    if(b.owners[loc E+dy*gy] == pla)
      infrontTemp += 6;
    if(b.owners[loc EE+dy*gy] == pla)
      infrontTemp += 3;

    //Score extra in-frontness for pieces on the second line
    if(dy == dyEnd-1)
      infrontTemp = infrontTemp*3/2;
    infront += infrontTemp;
  }

  int ifmod = IN_FRONT_MOD[min(infront,25)];
  int pemod = PELE_MDIST_MOD[Board::manhattanDist(plaEleLoc,loc)];

  int x = gX(loc);
  int tcmod0;
  {
    int tcidx = Board::PLATRAPINDICES[opp][0];
    int tcontrol = tc[pla][tcidx];
    int tcnoele = tcontrol + tcEle[opp][tcidx];
    int idx = tcontrol/5+8;
    if(idx < 0) idx = 0;
    if(idx > 20) idx = 20;
    int idx2 = tcnoele/5+4;
    if(idx2 < 0) idx2 = 0;
    if(idx2 > 20) idx2 = 20;
    int tcmod = (TC_MOD[idx] + TC_NOE_MOD[idx2])/2;
    tcmod0 = 64-((64-tcmod) * DIAGONAL_TRAP_MOD[gLoc(7-x,7-gydist)])/64;
  }
  int tcmod1;
  {
    int tcidx = Board::PLATRAPINDICES[opp][1];
    int tcontrol = tc[pla][tcidx];
    int tcnoele = tcontrol + tcEle[opp][tcidx];
    int idx = tcontrol/5+8;
    if(idx < 0) idx = 0;
    if(idx > 20) idx = 20;
    int idx2 = tcnoele/5+4;
    if(idx2 < 0) idx2 = 0;
    if(idx2 > 20) idx2 = 20;
    int tcmod = (TC_MOD[idx] + TC_NOE_MOD[idx2])/2;
    tcmod1 = 64-((64-tcmod) * DIAGONAL_TRAP_MOD[gLoc(x,7-gydist)])/64;
  }
  int frozenMod = b.isFrozen(loc) ? FROZEN_MOD : 64;
  int score = (base * spmod * ifmod * pemod)/4096 * tcmod0 * tcmod1 / 4096 * frozenMod / 4096 * 4/3;

  ONLYINDEBUG(if(out)
  {
    (*out) << "base " << -base << endl;
    (*out) << "spmod " << spmod << endl;
    (*out) << "ifmod " << ifmod << endl;
    (*out) << "pemod " << pemod << endl;
    (*out) << "tcmod0 " << tcmod0 << endl;
    (*out) << "tcmod1 " << tcmod1 << endl;
    (*out) << "score " << -score << endl;
    (*out) << endl;
  })

  return -score;
}



//Returns a measure of distance, whose order of magnitude is about 2x the actual number of steps needed.
static int alignmentDistanceFromTarget(const Board& b, const int ufDist[BSIZE], loc_t ploc, loc_t target,
    int oppEleX, int oppEleY)
{
  int mDist = Board::manhattanDist(ploc,target);
  int max = min(9,mDist+3);
  int dist = Threats::fastTraverseAdjUFAssumeNonRabDist(b,ploc,target,ufDist,max);
  if(dist > max)
    dist = max;

  //Double to get finer granularity
  dist *= 2;

  int pX = gX(ploc);
  int pY = gY(ploc);
  int kX = gX(target);
  int kY = gY(target);

  //The elephant does a better than normal job of blocking pieces smaller than itself
  if(b.pieces[ploc] != ELE)
  {
    if(pY >= oppEleY - 1 && pY <= oppEleY + 1 && (oppEleX - pX) * (oppEleX - kX) < 0)
      dist+=2;
    if(pX == oppEleX && (oppEleY - pY) * (oppEleY - kY) < 0)
      dist+=1;

    //Count horizontal distance slightly more, and also count it more when you're forward on the board
    static const int HDIST_FACTOR[8] = {3,2,1,1,2,4,4,4}; //out of 8
    dist += abs(pX-kX) * HDIST_FACTOR[b.owners[ploc] == GOLD ? pY : 7-pY] / 4;
  }
  else
  {
    //A different array for the elephant
    static const int ELE_HDIST_FACTOR[8] = {3,3,2,1,1,2,3,3}; //out of 8
    dist += abs(pX-kX) * ELE_HDIST_FACTOR[b.owners[ploc] == GOLD ? pY : 7-pY] / 4;
  }
  return dist;
}

static void getLocsShort(const Board& b, pla_t pla, piece_t piece, loc_t locs[2])
{
  Bitmap pieceMap = b.pieceMaps[pla][piece];
  if(pieceMap.isEmpty()) return;
  locs[0] = pieceMap.nextBit();
  if(pieceMap.isEmpty()) return;
  locs[1] = pieceMap.nextBit();
}

//Scale factor for value based on strength, 0 being the Elephant (skipping empty piece ranks)
static const int ALIGN_STRENGTH_VALUES[5] =
{380,640,300,0,0}; //Last two indices aren't used

//Alignment to attack a piece matters more when that piece is advanced - scale by this array
static const int ALIGN_OPP_GDIST_FACTOR[8] =
{58,64,64,52,40,28,20,16};

//Factor for distance.
//Less than 6 - attacking quite closely, we don't want to overlap too much with threat eval here
//Around 10 - Aligned
//More than 15 - Not so aligned
//More than 20 - very not aligned
static const int ALIGN_DISTANCE_FACTOR[32] =
{64,62,60,58,56,53,50,47,44,40,
 35,30,26,22,19,16,13,11, 9, 7,
  5, 4, 3, 2, 1, 0, 0, 0, 0, 0,
  0, 0,
};

//For shifting the alignment bonus to be closer to 0, so as to make the bot not simply reluctant to
//advance pieces (unless we zero-center, advanced pieces will get penalized more so whoever has
//more advanced pieces will get penalized more)
static const int ALIGN_DISTANCE_SHIFT = 24;



void Placement::getAlignmentScore(const Board& b, const int ufDist[BSIZE], eval_t evals[2], ostream* out)
{
  evals[SILV] = 0;
  evals[GOLD] = 0;

  //[pla][piece][whichpiece]
  loc_t pieceLocs[2][7][2];

  //Make sure elephants exist
  if(b.pieceCounts[SILV][ELE] <= 0 || b.pieceCounts[GOLD][ELE] <= 0)
    return;

  //We don't fill in rabbits
  for(pla_t pla = SILV; pla <= GOLD; pla++)
    for(piece_t piece = ELE; piece > RAB; piece--)
      getLocsShort(b,pla,piece,pieceLocs[pla][piece]);

  //For each piece, it scores points for being close to the pieces that it barely dominates
  for(pla_t pla = SILV; pla <= GOLD; pla++)
  {
    pla_t opp = gOpp(pla);
    int strength = 0; //0 = ELE, 1 = Next after ELE, etc. Skips over a rank if there are no piece of that rank

    int oppEleLoc = pieceLocs[opp][ELE][0];
    int oppEleX = gX(oppEleLoc);
    int oppEleY = gY(oppEleLoc);

    for(piece_t piece = ELE; piece > CAT; piece--)
    {
      int plaCount = b.pieceCounts[pla][piece];
      if(plaCount <= 0)
        continue; //Continue without incrementing strength to skip over rank with no pieces

      //Find the strongest opp that it dominates
      piece_t oppPiece = piece-1;
      while(oppPiece > RAB && b.pieceCounts[opp][oppPiece] <= 0)
        oppPiece--;
      if(oppPiece <= RAB)
        break;

      //Additional factor for the importance of the alignment bonus purely based on piece counts
      int baseValue = ALIGN_STRENGTH_VALUES[strength];

      //Is there a weaker rank of piece we have that still dominates?
      {
        piece_t p = piece-1;
        while(p > oppPiece && b.pieceCounts[pla][p] <= 0)
          p--;
        if(p > oppPiece)
          baseValue = baseValue * 3/5;
      }
      //Do we have no pieces of the opponent's strength?
      if(b.pieceCounts[pla][oppPiece] == 0)
        baseValue = baseValue * 3/2;

      //Do we have many pieces of this type, and/or the opponent many pieces of this type?
      if(b.pieceCounts[pla][piece] >= 2)
        baseValue = baseValue * 2/3;

      //Do we have many pieces of this type, and/or the opponent many pieces of this type?
      if(b.pieceCounts[opp][oppPiece] >= 2)
        baseValue = baseValue * 2/3;

      int oppCount = b.pieceCounts[opp][oppPiece];
      for(int i = 0; i<plaCount; i++)
      {
        loc_t plaLoc = pieceLocs[pla][piece][i];
        for(int j = 0; j<oppCount; j++)
        {
          loc_t oppLoc = pieceLocs[opp][oppPiece][j];
          int alignDist = alignmentDistanceFromTarget(b,ufDist,plaLoc,oppLoc,oppEleX,oppEleY);
          if(alignDist >= 32) alignDist = 32;
          eval_t eval =
            ( baseValue
            * ALIGN_OPP_GDIST_FACTOR[Board::GOALYDIST[opp][oppLoc]]
            * (ALIGN_DISTANCE_FACTOR[alignDist] - ALIGN_DISTANCE_SHIFT)
            ) / 4096;

          if(out)
            (*out) << Board::writePiece(pla,piece) << Board::writePiece(opp,oppPiece) << " dist " << alignDist << " eval " << eval << endl;
          evals[pla] += eval;
        }
      }

      strength++;
      if(strength > 2)
        break;
    }

  }

}

static const int imbalanceDistX[8][8] = {
{ 1, 2, 5, 7,10,12,13,13},
{ 1, 2, 5, 7,10,12,13,13},
{ 3, 3, 4, 7, 9,11,12,12},
{ 5, 5, 5, 5, 7, 9, 9, 9},
{ 9, 9, 9, 7, 5, 5, 5, 5},
{12,12,11, 7, 7, 4, 3, 3},
{13,13,12,10, 7, 5, 2, 1},
{13,13,12,10, 7, 5, 2, 1},
};

static const int imbalanceDistY_ELE_CAM[8][8] = {
{ 0, 1, 2, 4, 5, 7, 8, 8},
{ 0, 1, 2, 4, 5, 7, 8, 8},
{ 0, 1, 2, 4, 5, 7, 8, 8},
{ 2, 3, 4, 3, 4, 5, 6, 6},
{ 4, 5, 6, 5, 4, 3, 3, 3},
{ 5, 6, 7, 6, 5, 3, 3, 3},
{ 5, 6, 7, 6, 5, 3, 2, 2},
{ 5, 6, 7, 6, 5, 3, 2, 1},
};

//Factor based on distance
static const int EM_IMBALANCE_DIST_FACTOR[25] = {
69,69,68,66,64,62,57,52,46,40,
34,28,22,16,11, 7, 3, 0, 0, 0,
0, 0, 0, 0, 0,
};

//Base value based on location of M
static const int EM_IMBALANCE_PENALTY_BY_M[128] = {
220,210,210,210,210,210,210,220,  0,0,0,0,0,0,0,0,
220,220,220,230,230,220,220,220,  0,0,0,0,0,0,0,0,
220,210,190,210,210,190,210,220,  0,0,0,0,0,0,0,0,
180,170,150,150,150,150,170,180,  0,0,0,0,0,0,0,0,
150,140,120,110,110,120,140,150,  0,0,0,0,0,0,0,0,
110,100, 90, 80, 80, 90,100,110,  0,0,0,0,0,0,0,0,
110,100, 90, 80, 80, 90,100,110,  0,0,0,0,0,0,0,0,
110,100, 90, 80, 80, 90,100,110,  0,0,0,0,0,0,0,0,
};

//TODO first index is for when the opp E or your own pieces block you from the other side
static const int SIDE_CONTRIBUTION_X_FACTOR[5][8] = {
{64,64,58,44,28,16,10, 4},
{64,64,57,42,26,13, 6, 0},
{64,64,56,40,24,10, 2, 0},
{64,64,55,38,21, 8, 0, 0},
{64,64,54,36,18, 6, 0, 0},
};
static const int SIDE_CONTRIBUTION_X_DIFFICULTY_BY_ADV[8] = {
2,1,0,0,0,2,2,3 //NOTE: This gets possibly ++, so needs to be <= 3 since SIDE_CONTRIBUTION_X_FACTOR is len 5
};

static const int SIDE_CONTRIBUTION_STR[9][4] = {
{44,0,0,0}, //0M 0H
{30,30,0,0}, //1M 0H
{16,30,0,0}, //2M 0H
{0,0,0,0},   //0M 1H impossible
{7,28,23,2}, //1M 1H
{6,28,10,0}, //2M 1H
{0,0,0,0},   //0M 2H impossible
{6,28,22,0}, //1M 2H
{6,28, 5,0}, //2M 2H
};
static const int SIDE_CONTRIBUTION_SCALE[9] = {
32, //0M 0H
32, //1M 0H
32, //2M 0H
0,   //0M 1H impossible
64, //1M 1H
64, //2M 1H
0,   //0M 2H impossible
64, //1M 2H
64, //2M 2H
};


static const int SIDE_SCORE_LEN = 60;
static const int SIDE_SCORE[SIDE_SCORE_LEN] = {
480,470,460,450,440,430,425,410,395,380,
355,330,305,280,255,230,205,180,160,140,
120,100, 85, 70, 55, 40, 30, 20, 15, 10,
  5,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static int getSideContributionXFactor(const Board& b, pla_t pla, loc_t loc, int oppEleX, int oppEleY, int side)
{
  int x = gX(loc);
  int y = gY(loc);
  int adv = pla == GOLD ? y : 7-y;

  int difficulty = SIDE_CONTRIBUTION_X_DIFFICULTY_BY_ADV[adv];
  int targetX = side*5+1;
  bool eleInBetween = (oppEleX - targetX)*(oppEleX - x) < 0;
  if(eleInBetween && y == oppEleY)
    difficulty = 4;
  else if(b.isFrozen(loc) || (eleInBetween && y <= oppEleY+1 && y >= oppEleY-1))
    difficulty++;
  return SIDE_CONTRIBUTION_X_FACTOR[difficulty][side != 0 ? 7-x : x];
}


void Placement::getImbalanceScore(const Board& b, const int ufDist[BSIZE], eval_t evals[2], ostream* out)
{
  (void)ufDist;
  evals[SILV] = 0;
  evals[GOLD] = 0;

  //Make sure elephants exist
  if(b.pieceCounts[SILV][ELE] <= 0 || b.pieceCounts[GOLD][ELE] <= 0)
    return;
  loc_t eleLocs[2];
  eleLocs[SILV] = b.findElephant(SILV);
  eleLocs[GOLD] = b.findElephant(GOLD);

  //We fill in only the top three levels of piece strengths for a player
  for(pla_t pla = SILV; pla <= GOLD; pla++)
  {
    //Index 0 is supposed to be the elephant, but we actually don't use it, and dont bother filling it
    //[strength][whichpiece]
    loc_t pieceLocs[4][2];
    int pieceCounts[4];

    int str = 1;
    for(piece_t piece = CAM; piece > RAB; piece--)
    {
      int count = b.pieceCounts[pla][piece];
      if(count == 0) continue;
      pieceCounts[str] = count;
      getLocsShort(b,pla,piece,pieceLocs[str]);
      str++;
      if(str >= 4) break;
    }
    while(str < 4) {pieceCounts[str++] = 0;}

    //Penalize based on max distance between E and M (or equivalent)
    loc_t eleLoc = eleLocs[pla];

    eval_t emImbalanceEval = 0;
    for(int i = 0; i<pieceCounts[1]; i++)
    {
      loc_t camLoc = pieceLocs[1][i];
      int yEMDist = (pla==SILV ? imbalanceDistY_ELE_CAM[7-gY(eleLoc)][7-gY(camLoc)] : imbalanceDistY_ELE_CAM[gY(eleLoc)][gY(camLoc)]);
      int emImbalanceDist = imbalanceDistX[gX(eleLoc)][gX(camLoc)] + yEMDist;
      int base = EM_IMBALANCE_PENALTY_BY_M[pla == GOLD ? gRevLoc(camLoc) : camLoc];

      //This normally would be >> 6, but we multiply by 3 and >> 8 to effectively scale to 75%.
      eval_t eval = (base * EM_IMBALANCE_DIST_FACTOR[emImbalanceDist] * 3) >> 8;
      if(out) (*out) << "EM Imbalance: " << (int)pla << " Base " << base << " Dist " << emImbalanceDist << " Eval " << eval << endl;
      emImbalanceEval += eval;
    }
    if(pieceCounts[1] >= 2)
      emImbalanceEval = emImbalanceEval / 2;
    evals[pla] -= emImbalanceEval;

    //Penalize based on same-sidedness of major pieces
    int mode = pieceCounts[1] + pieceCounts[2]*3;

    //Consider the opp ele blocking things off
    loc_t oppEleLoc = eleLocs[gOpp(pla)];
    int oppEleX = gX(oppEleLoc);
    int oppEleY = gY(oppEleLoc);

    int side0Str = SIDE_CONTRIBUTION_STR[mode][0] * SIDE_CONTRIBUTION_X_FACTOR[0][gX(eleLoc)];
    int side1Str = SIDE_CONTRIBUTION_STR[mode][0] * SIDE_CONTRIBUTION_X_FACTOR[0][7-gX(eleLoc)];
    for(int str = 1; str <= 3; str++)
    {
      int strFactor = SIDE_CONTRIBUTION_STR[mode][str];
      if(strFactor == 0) continue;
      for(int i = 0; i<pieceCounts[str]; i++)
      {
        loc_t loc = pieceLocs[str][i];
        int side0Factor = getSideContributionXFactor(b,pla,loc,oppEleX,oppEleY,0);
        int side1Factor = getSideContributionXFactor(b,pla,loc,oppEleX,oppEleY,1);
        side0Str += strFactor * side0Factor;
        side1Str += strFactor * side1Factor;
      }
    }

    side0Str = side0Str >> 6;
    side1Str = side1Str >> 6;
    if(side0Str >= SIDE_SCORE_LEN) side0Str = SIDE_SCORE_LEN-1;
    if(side1Str >= SIDE_SCORE_LEN) side1Str = SIDE_SCORE_LEN-1;
    eval_t score = ((SIDE_SCORE[side0Str] + SIDE_SCORE[side1Str]) * SIDE_CONTRIBUTION_SCALE[mode]) >> 6;
    evals[pla] -= score;
    if(out) (*out) << "Side Imbal Pla: " << (int)pla << " Str0 " << side0Str << " Str1 " << side1Str
                   << " Mode " << mode << " Eval " << score << endl;
  }

}
