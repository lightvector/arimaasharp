/*
 * pthreats.cpp
 * Author: davidwu
 */
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardtreeconst.h"
#include "../eval/threats.h"
#include "../eval/internal.h"

static int eleReachDist3OrMore(const Bitmap eleCanReach[EMOB_ARR_LEN], loc_t loc)
{
  Bitmap map = Board::DISK[1][loc];
  if((eleCanReach[3] & map).hasBits())
    return 3;
  if(!(eleCanReach[EMOB_MAX-1] & map).hasBits())
    return EMOB_MAX;
  for(int i = 4; i<EMOB_MAX; i++)
    if((eleCanReach[i] & map).hasBits())
      return i;
  return EMOB_MAX;
}

//TODO this is close to a copy of H_ trap threats, mainly to preserve behavior on opp trap threats as we optimize
//home trap threats
//Ignore attacks by opp if the opp clearly has no control on trap
static const int O_MIN_OPP_TC_FOR_THREAT = -25;
//Or if the piece is simply too many steps away from pushing to the trap
static const int O_MAX_THREATDIST_FOR_THREAT = 10;
//Note that pieces at the radius limit must be dominated
static const int O_MAX_RADIUS_FROM_TRAP_FOR_THREAT = 4;
//Or if there are too many non-ele pieces already closer, or as close and unfrozen for us to feel threatened
static const int O_MAX_EQUAL_OR_CLOSER_UF_COUNT = 4;
//Or there is no nearby attacker
static const int O_MAX_ATTACKDIST_FOR_THREAT = 3;

//Base threat value = (pieceval + const) * prop
static const int O_BASE_CONST = 500;
static const int O_BASE_PROP = 18; //Out of 64

//Center of logistic that determines the contribution of opp trap control to threatening pla pieces
static const int O_TC_LOGISTIC_CENTER = 45;
//Subtract this much so that as we cross over MIN_OPP_TC_FOR_THREAT, we don't suddenly jump in value
static const int O_TC_LOGISTIC_SUB = 12; //1024.0/(1.0+exp((45+25)/16.0))

static const int O_TDIST_FACTOR[O_MAX_THREATDIST_FOR_THREAT+1] =
{64,62,61,58,53,34,30,22,19,9,6}; //Out of 64
static const int O_ADIST_FACTOR[O_MAX_ATTACKDIST_FOR_THREAT+1] =
{64,64,64,64}; //Out of 64
static const int O_UFDIST_FACTOR[UFDist::MAX_UF_DIST+1] =
{24,33,42,51,58,64,64}; //Out of 64
static const int O_EQUAL_OR_CLOSER_UF_COUNT[O_MAX_EQUAL_OR_CLOSER_UF_COUNT+1] =
{64,64,64,64,64}; //Out of 64
//{64,52,42,30,16}; //Out of 64
static const int O_ELE_REACH_DIST_3_OR_MORE[EMOB_MAX+1] =
{64,64,64,64,67,72,76,79};

void PThreats::accumOppAttack(const Board& b,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int pValues[2][NUMTYPES], const int ufDist[BSIZE],
    const int tc[2][4], const Bitmap eleCanReach[2][EMOB_ARR_LEN],
    eval_t trapThreats[2][4], eval_t pieceThreats[BSIZE])
{
  //TODO perhaps drop the memoization so that we can pass attackdist tighter maxes?
  int memoizedAttackDist[BSIZE];
  for(int i = 0; i<BSIZE; i++)
    memoizedAttackDist[i] = -1;

  //Accum for each trap
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    loc_t trapLoc = Board::TRAPLOCS[trapIndex];

    pla_t pla = trapIndex >= 2 ? SILV : GOLD;
    pla_t opp = gOpp(pla);

    //Quit immediately if the opp has no control over the trap
    int oppTC = tc[opp][trapIndex];
    if(oppTC < O_MIN_OPP_TC_FOR_THREAT)
      continue;
    int tcFactor = Logistic::prop16(oppTC-O_TC_LOGISTIC_CENTER) - O_TC_LOGISTIC_SUB;
    int eleReachDistFactor = O_ELE_REACH_DIST_3_OR_MORE[eleReachDist3OrMore(eleCanReach[pla],trapLoc)];

    int numDefendersX2 = 2 * (b.trapGuardCounts[pla][trapIndex] + (int)(b.owners[trapLoc] == pla));

    //Restrict to pla pieces that could possibly be threatened
    Bitmap plaNonEleMap = b.pieceMaps[pla][0] & ~b.pieceMaps[pla][ELE];
    int closerCount = 0; //Counts how many threatened pieces there are that are closer to the trap than us
    for(int mDist = 0; mDist <= O_MAX_RADIUS_FROM_TRAP_FOR_THREAT; mDist++)
    {
      //For updating the number of threatened pieces closer, or the number of uf pieces equally far.
      //Used to scale our threatenedness - less threatened if lots of pieces in the way
      int newCloserCount = 0;
      Bitmap radius = Board::RADIUS[mDist][trapLoc];
      Bitmap equalCloseUF = plaNonEleMap & radius & ~b.frozenMap;
      int equalCloseUFCount = equalCloseUF.countBits();

      Bitmap map = plaNonEleMap & radius;
      //Optimization - if not dominated, likely too far away to matter
      if(mDist == O_MAX_RADIUS_FROM_TRAP_FOR_THREAT) map &= b.dominatedMap;
      while(map.hasBits())
      {
        loc_t loc = map.nextBit();
        piece_t piece = b.pieces[loc];
        newCloserCount++;

        int closerOrEqualUFCount = closerCount + equalCloseUFCount + (equalCloseUF.isOne(loc) ? -1 : 0);
        if(closerOrEqualUFCount > O_MAX_EQUAL_OR_CLOSER_UF_COUNT)
          continue;

        //Base distance to push piece into trap
        int threatDist = mDist <= 1 ? (mDist * 2 + numDefendersX2 - 2) : (mDist * 2 + numDefendersX2);

        //Increase the distance if an opp piece is directly the way
        int diffIdx = Board::diffIdx(loc,trapLoc);
        if(loc != trapLoc && Board::DIRTORFAR[diffIdx] == Board::DIRTORNEAR[diffIdx])
        {
          loc_t midLoc = loc + Board::ADJOFFSETS[Board::DIRTORFAR[diffIdx]];
          if(b.owners[midLoc] == pla)
          {
            if(b.pieces[midLoc] == ELE) threatDist += 3;
            else threatDist++;

            if(threatDist > O_MAX_THREATDIST_FOR_THREAT)
              continue;
          }
        }
        //Increase the distance if the opp trap is stuffed with a weak piece
        if(b.owners[trapLoc] == opp && b.pieces[trapLoc] <= piece)
          threatDist++;
        if(threatDist > O_MAX_THREATDIST_FOR_THREAT)
          continue;

        //TODO attackdist - a piece on a trap should not be threatened by an elephant on the side unless the opp has >= 2 defs
        if(memoizedAttackDist[loc] == -1)
          memoizedAttackDist[loc] = Threats::attackDist(b,loc,O_MAX_ATTACKDIST_FOR_THREAT+1,pStrongerMaps,ufDist);
        int attackDist = memoizedAttackDist[loc];
        threatDist += attackDist + (int)(attackDist >= 3);
        if(attackDist > O_MAX_ATTACKDIST_FOR_THREAT || threatDist > O_MAX_THREATDIST_FOR_THREAT)
          continue;

        int ufd = b.isDominated(loc) ? ufDist[loc] : 0;
        int threatValue = ((pValues[pla][piece]+O_BASE_CONST) * O_BASE_PROP * O_TDIST_FACTOR[threatDist]) >> 12;
        int totalFactor = (tcFactor * O_EQUAL_OR_CLOSER_UF_COUNT[closerOrEqualUFCount] * O_ADIST_FACTOR[attackDist] * O_UFDIST_FACTOR[ufd]) >> 18;
        totalFactor = (totalFactor * eleReachDistFactor) >> 6;
        int threatScore = (totalFactor * threatValue) >> 10;

        trapThreats[pla][trapIndex] += threatScore;
        pieceThreats[loc] += threatScore;
      }

      closerCount += newCloserCount;
      if(closerCount > O_MAX_EQUAL_OR_CLOSER_UF_COUNT)
        break;
    }
  }
}


/*
//Ignore attacks by opp if the opp clearly has no control on trap
static const int OA_MIN_OPP_TC = -25;

//Center of logistic that determines the contribution of opp trap control to threatening pla pieces
static const int OA_LOGISTIC_CENTER = 45;
//Subtract this much so that as we cross over MIN_OPP_TC_FOR_THREAT, we don't suddenly jump in value
static const double OA_LOGISTIC_SUB = 12; //1024.0/(1.0+exp((45+25)/16.0))

//TODO scale these values instead and set all other scaling factors genuinely out of 64.
//Base threat value = (pieceval + const) * prop
static double OA_BASE_CONST = 500;
static double OA_BASE_PROP = 34; //Out of 64

//Various scaling factors
static double OA_CAPDIST_FACTOR[12] =
{26,26,25,24,22,14,12,9,7,4,2,1}; //Out of 64
static double OA_UFDIST_FACTOR[UFDist::MAX_UF_DIST+1] =
{41,54,61,64,64,64,64}; //Out of 64


void PThreats::accumOppAttack(const Board& b,
    const Bitmap pStrongerMaps[2][NUMTYPES], const int pValues[2][NUMTYPES], const int ufDist[BSIZE],
    const int tc[2][4], eval_t trapThreats[2][4], eval_t pieceThreats[BSIZE])
{
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    loc_t trapLoc = Board::TRAPLOCS[trapIndex];
    pla_t pla = trapIndex < 2 ? GOLD : SILV;
    pla_t opp = gOpp(pla);

    int oppTC = tc[opp][trapIndex];
    if(oppTC < OA_MIN_OPP_TC)
      continue;

    double tcFactor = Logistic::prop16(oppTC-OA_LOGISTIC_CENTER) - OA_LOGISTIC_SUB;
    int trapGuardCountX2 = (b.trapGuardCounts[pla][trapIndex] + (int)(b.owners[trapLoc] == pla)) * 2;

    int spiralIdx = 0;
    while(true)
    {
      loc_t loc = Board::SPIRAL[trapLoc][spiralIdx];
      int mDist = Board::manhattanDist(trapLoc,loc);
      if((mDist > 1 && trapGuardCountX2 >= 4) || mDist > 2)
        break;
      spiralIdx++;
      if(b.owners[loc] != pla || b.pieces[loc] == ELE)
        continue;

      piece_t piece = b.pieces[loc];
      Bitmap strongerUF = pStrongerMaps[pla][piece] & ~b.frozenMap;
      int baseCapDist = mDist > 1 ? trapGuardCountX2 + mDist * 2 : trapGuardCountX2 - 2;

      int capDist;
      //Dominated by UF directly
      if((Board::RADIUS[1][loc] & strongerUF).hasBits())
        capDist = baseCapDist;
      //Strong UF piece in radius 2 or dominating frozen piece immediately adjacent
      else if(mDist <= 1 && ((Board::RADIUS[2][loc] & strongerUF).hasBits() || b.isDominated(loc)))
        capDist = baseCapDist + 1;
      else continue;

      double pieceValue = ((pValues[pla][piece]+OA_BASE_CONST) * OA_BASE_PROP) / 64.;
      double totalFactor = tcFactor * OA_CAPDIST_FACTOR[capDist] * OA_UFDIST_FACTOR[ufDist[loc]];
      eval_t threatScore = (eval_t)(totalFactor * pieceValue / (1048576. * 4.));
      trapThreats[pla][trapIndex] += threatScore;
      pieceThreats[loc] += threatScore;
    }
  }
}
*/

//TODO check and possibly fix behavior on threats other than E attacking M.
//TODO tune values
//TODO take into account current player and steps left in turn
//TODO take into account cases where the closest path to the trap is blocked (such as by the ele) or by own frozen piece
//TODO need special handling if mdist = 0.

//Ignore attacks by opp if the opp clearly has no control on trap
static const int H_MIN_OPP_TC_FOR_THREAT_EARLY = -50; //a heuristic fast early cutoff
static const int H_MIN_OPP_TC_FOR_THREAT = -25;
//Or if the piece is simply too many steps away from pushing to the trap
static const int H_MAX_THREATDIST_FOR_THREAT = 10;
//Note that pieces at the radius limit must be dominated
static const int H_MAX_RADIUS_FROM_TRAP_FOR_THREAT = 4;
//Or if there are too many non-ele pieces already closer, or as close and unfrozen for us to feel threatened
static const int H_MAX_EQUAL_OR_CLOSER_UF_COUNT = 4;
//Or there is no nearby attacker
static const int H_MAX_ATTACKDIST_FOR_THREAT = 3;

//Base threat value = (pieceval + const) * prop
static double H_BASE_CONST = 500;
static double H_BASE_PROP = 15.5; //Out of 64

//Center of logistic that determines the contribution of opp trap control to threatening pla pieces
static const int H_TC_LOGISTIC_CENTER = 45;
//Subtract this much so that as we cross over MIN_OPP_TC_FOR_THREAT, we don't suddenly jump in value
static const double H_TC_LOGISTIC_SUB = 12; //1024.0/(1.0+exp((45+25)/16.0))

static double H_TDIST_FACTOR[H_MAX_THREATDIST_FOR_THREAT+1] =
{64,62,61,58,53,34,30,22,19,9,6}; //Out of 64
static double H_ADIST_FACTOR[4][H_MAX_ATTACKDIST_FOR_THREAT+1] =
{{66,64,62,62}, //Camel (NS = 1)
 {60,58,56,56}, //Horse (NS = 2)
 {62,60,58,58}, //NonRabbit Other (NS > 2)
 {64,64,64,64}, //Rabbit
}; //Out of 64
static double H_ADIST_NON_ELE_FACTOR[4][H_MAX_ATTACKDIST_FOR_THREAT+2] =
{{0,0,0,0,0}, //Camel (NS = 1) (UNUSED)
 {12,9,6,3,0}, //Horse (NS = 2)
 {8,6,4,2,0}, //NonRabbit Other (NS > 2)
 {0,0,0,0,0}, //Rabbit (UNUSED)
}; //Out of 64

static double H_ADIST_NON_ELE_REDUCIBILITY[4][H_MAX_ATTACKDIST_FOR_THREAT+2] =
{{ 8, 8, 8, 8, 8}, //Camel (NS = 1)
 { 8,12,17,22,27}, //Horse (NS = 2)
 { 6, 8,10,12,15}, //NonRabbit Other (NS > 2)
 {0,0,0,0,0}, //Rabbit (UNUSED)
}; //Out of 64

static double H_XPOS_REDUCIBILITY[8] =
{64,64,50,24,24,50,64,64};

//TODO for rabbits we average this value with 90, both to increase the potency of rabbit threats and to reduce
//the amount that freezing matters for them. Maybe make it vary in other cases for other pieces
static double H_UFDIST_FACTOR[UFDist::MAX_UF_DIST+1] =
{27,36,44,52,58,64,64}; //Out of 64
static double H_EQUAL_OR_CLOSER_UF_COUNT[H_MAX_EQUAL_OR_CLOSER_UF_COUNT+1] =
{74,69,64,59,54}; //Out of 64

static const double H_ELE_REACH_DIST_3_OR_MORE[EMOB_MAX+1] =
{64,64,64,64,67,72,76,79};

//TODO probably this shouldn't be done like this, rather it should adjust the threatdist
//This array should be decreasing (threat on the piece given the number of steps left by the owning player).
static double H_NUMSTEPS_LEFT[5] =
{64,64,64,64,64};

//Out of 64
static double H_LOCATION_FACTOR[2][BSIZE] = {
{
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,67,67,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,68,68,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,67,67,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
},
{
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,67,67,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,68,68,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,67,67,64,64,64,  0,0,0,0,0,0,0,0,
64,64,64,66,66,64,64,64,  0,0,0,0,0,0,0,0,
}};

//TODO control tc logistic center, width, and vertical scaling
//TODO take into account more specific information about pieces ahead (like h on A3 or r on B2)
//TODO consider the 'pushblocked' factor that heavy threats has
//TODO train on hvb games with a rating floor and downweight the bot moves a lot

void PThreats::getBasicThreatCoeffsAndBases(vector<string>& names, vector<double*>& coeffs,
    vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale)
{
  CoeffBasis::single("H_BASE_CONST",&H_BASE_CONST,200,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::single("H_BASE_PROP",&H_BASE_PROP,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_TDIST_FACTOR",H_TDIST_FACTOR,H_MAX_THREATDIST_FOR_THREAT+1,3,2,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_ADIST_FACTOR_M",H_ADIST_FACTOR[0],H_MAX_ATTACKDIST_FOR_THREAT+1,6,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_ADIST_FACTOR_H",H_ADIST_FACTOR[1],H_MAX_ATTACKDIST_FOR_THREAT+1,6,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_ADIST_FACTOR_DC",H_ADIST_FACTOR[2],H_MAX_ATTACKDIST_FOR_THREAT+1,6,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_ADIST_FACTOR_R",H_ADIST_FACTOR[3],H_MAX_ATTACKDIST_FOR_THREAT+1,6,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_ADIST_NON_ELE_FACTOR_H",H_ADIST_NON_ELE_FACTOR[1],H_MAX_ATTACKDIST_FOR_THREAT+2,6,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_ADIST_NON_ELE_FACTOR_DC",H_ADIST_NON_ELE_FACTOR[2],H_MAX_ATTACKDIST_FOR_THREAT+2,6,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_ADIST_NON_ELE_FACTOR_R",H_ADIST_NON_ELE_FACTOR[3],H_MAX_ATTACKDIST_FOR_THREAT+2,6,6,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayRightHigh("H_UFDIST_FACTOR",H_UFDIST_FACTOR,UFDist::MAX_UF_DIST+1,3,3,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::arrayLeftHigh("H_EQUAL_OR_CLOSER_UF_COUNT",H_EQUAL_OR_CLOSER_UF_COUNT,H_MAX_EQUAL_OR_CLOSER_UF_COUNT+1,5,5,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::boardArray("H_LOCATION_FACTOR",H_LOCATION_FACTOR,BSIZE,4,4,names,coeffs,basisNames,bases,scaleScale);
  CoeffBasis::linear("H_NUMSTEPS_LEFT",H_NUMSTEPS_LEFT,5,0,5,2,names,coeffs,basisNames,bases,scaleScale);

  /* Some coeffs from a first run
  405.289
  13.2228
  60.122
  58.122
  57.122
  52.062
  44.7757
  24.9118
  20.5813
  12.7541
  11.5758
  4.38812
  3.75366
  70.4334
  68.5035
  65.9249
  64.3778
  23.8399
  34.239
  44.4515
  54.1306
  60.3893
  66.061
  65.773
  73.9384
  69.3975
  67.4754
  65.8194
  65.0305
  */
}

void PThreats::accumBasicThreats(const Board& b, const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int pValues[2][NUMTYPES], const int ufDist[BSIZE],
    const int tc[2][4], const Bitmap eleCanReach[2][EMOB_ARR_LEN],
    eval_t trapThreats[2][4], eval_t reducibleTrapThreats[2][4], eval_t pieceThreats[BSIZE])
{
  //TODO perhaps drop the memoization so that we can pass attackdist tighter maxes?
  int memoizedAttackDist[BSIZE*2];
  for(int i = 0; i<BSIZE*2; i++)
    memoizedAttackDist[i] = -1;

  //Accum for each trap
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    loc_t trapLoc = Board::TRAPLOCS[trapIndex];

    pla_t pla = trapIndex < 2 ? SILV : GOLD;
    pla_t opp = gOpp(pla);

    //Quit immediately if the opp has no control over the trap
    int oppTC = tc[opp][trapIndex];
    if(oppTC < H_MIN_OPP_TC_FOR_THREAT_EARLY)
      continue;
    //double tcFactor = Logistic::prop16(oppTC-H_TC_LOGISTIC_CENTER) - H_TC_LOGISTIC_SUB;
    double eleReachDistFactor = H_ELE_REACH_DIST_3_OR_MORE[eleReachDist3OrMore(eleCanReach[pla],trapLoc)];

    int numDefendersX2 = 2 * (b.trapGuardCounts[pla][trapIndex] + (int)(b.owners[trapLoc] == pla));

    //Restrict to pla pieces that could possibly be threatened
    Bitmap plaNonEleMap = b.pieceMaps[pla][0] & ~b.pieceMaps[pla][ELE];
    int closerCount = 0; //Counts how many threatened pieces there are that are closer to the trap than us
    for(int mDist = 0; mDist <= H_MAX_RADIUS_FROM_TRAP_FOR_THREAT; mDist++)
    {
      //For updating the number of threatened pieces closer, or the number of uf pieces equally far.
      //Used to scale our threatenedness - less threatened if lots of pieces in the way
      int newCloserCount = 0;
      Bitmap radius = Board::RADIUS[mDist][trapLoc];
      Bitmap equalCloseUF = plaNonEleMap & radius & ~b.frozenMap;
      int equalCloseUFCount = equalCloseUF.countBits();

      Bitmap map = plaNonEleMap & radius;
      //Optimization - if not dominated, likely too far away to matter
      if(mDist == H_MAX_RADIUS_FROM_TRAP_FOR_THREAT) map &= b.dominatedMap;
      while(map.hasBits())
      {
        loc_t loc = map.nextBit();
        piece_t piece = b.pieces[loc];
        newCloserCount++;

        int closerOrEqualUFCount = closerCount + equalCloseUFCount + (equalCloseUF.isOne(loc) ? -1 : 0);
        if(closerOrEqualUFCount > H_MAX_EQUAL_OR_CLOSER_UF_COUNT)
          continue;

        //Base distance to push piece into trap
        int threatDist = mDist <= 1 ? (mDist * 2 + numDefendersX2 - 2) : (mDist * 2 + numDefendersX2);

        //Increase the distance if a pla piece is directly the way
        int diffIdx = Board::diffIdx(loc,trapLoc);
        if(loc != trapLoc && Board::DIRTORFAR[diffIdx] == Board::DIRTORNEAR[diffIdx])
        {
          loc_t midLoc = loc + Board::ADJOFFSETS[Board::DIRTORFAR[diffIdx]];
          if(b.owners[midLoc] == pla)
          {
            if(b.pieces[midLoc] == ELE) threatDist += 3;
            else threatDist++;

            if(threatDist > H_MAX_THREATDIST_FOR_THREAT)
              continue;
          }
        }
        //Increase the distance if the opp trap is stuffed with a weak piece
        if(b.owners[trapLoc] == opp && b.pieces[trapLoc] <= piece)
          threatDist++;
        if(threatDist > H_MAX_THREATDIST_FOR_THREAT)
          continue;

        //TODO attackdist - a piece on a trap should not be threatened by an elephant on the side unless the opp has >= 2 defs
        if(memoizedAttackDist[loc*2] == -1)
        {
          memoizedAttackDist[loc*2] = Threats::attackDistPushpullingForward(b,loc,H_MAX_ATTACKDIST_FOR_THREAT+1,pStrongerMaps,ufDist);
          if(piece == RAB || pStronger[pla][piece] <= 1)
            memoizedAttackDist[loc*2+1] = 0;
          else
          {
            memoizedAttackDist[loc*2+1] = Threats::attackDistPushpullingForwardNonEle(b,loc,H_MAX_ATTACKDIST_FOR_THREAT+1,pStrongerMaps,ufDist);
            DEBUGASSERT(memoizedAttackDist[loc*2+1] <= H_MAX_ATTACKDIST_FOR_THREAT+1);
          }
        }
        int attackDist = memoizedAttackDist[loc*2];
        int attackDistNonEle = memoizedAttackDist[loc*2+1];
        threatDist += attackDist + (int)(attackDist >= 3);
        if(attackDist > H_MAX_ATTACKDIST_FOR_THREAT || threatDist > H_MAX_THREATDIST_FOR_THREAT)
          continue;

        int nsLeft = pla == b.player ? 4-b.step : b.step;
        int pieceType = piece == RAB ? 3 : pStronger[pla][piece] <= 1 ? 0 : pStronger[pla][piece] <= 2 ? 1 : 2;

        int oppTCAdjusted =
            tc[opp][trapIndex] + TrapControl::pieceContribution(b,pStronger,pStrongerMaps,ufDist,pla,piece,loc,trapLoc);
//                             - TrapControl::pieceContributionIfThreatenedAt(b,pStronger,pStrongerMaps,ufDist,pla,piece,loc,trapLoc);
        if(oppTCAdjusted < H_MIN_OPP_TC_FOR_THREAT)
          continue;
        double tcFactorAdjusted = Logistic::prop16(oppTCAdjusted-H_TC_LOGISTIC_CENTER) - H_TC_LOGISTIC_SUB;

        int ufd = b.isDominated(loc) ? ufDist[loc] : 0;
        double ufdFactor = H_UFDIST_FACTOR[ufd];
        if(b.isRabbit(loc)) ufdFactor = (ufdFactor + 90)/2;

        int pieceValue = pValues[pla][piece];
        double baseValue = ((pieceValue+H_BASE_CONST) * H_BASE_PROP) / 64.;
        double threatValue = (baseValue * H_TDIST_FACTOR[threatDist]) / 64.;
        double totalADistFactor = H_ADIST_FACTOR[pieceType][attackDist] + H_ADIST_NON_ELE_FACTOR[pieceType][attackDistNonEle];
        double totalFactor = tcFactorAdjusted * H_EQUAL_OR_CLOSER_UF_COUNT[closerOrEqualUFCount] * totalADistFactor *
            ufdFactor * H_LOCATION_FACTOR[pla][loc] * H_NUMSTEPS_LEFT[nsLeft] * eleReachDistFactor;
        eval_t threatScore = (eval_t)(totalFactor * threatValue / (1048576. * 16384. * 4096.));

        //Make sure threat doesn't reach or exceed the value of the whole piece
        int maxValue = (pieceValue * 115)/128; //about 90%
        if(pieceThreats[loc] + threatScore > maxValue)
          threatScore = maxValue - pieceThreats[loc];

        trapThreats[pla][trapIndex] += threatScore;
        pieceThreats[loc] += threatScore;

        double reducibilityFactor =  H_ADIST_NON_ELE_REDUCIBILITY[pieceType][attackDistNonEle] * H_XPOS_REDUCIBILITY[gX(loc)];
        eval_t threatScoreReducible = (eval_t)(threatScore * reducibilityFactor / 4096.);

        //TODO this is a bit weird when we're hitting the cap on piecethreat - we might now depend on the order that
        //we evaluate threats in different traps, but hopefully this is rare and not a big deal
        reducibleTrapThreats[pla][trapIndex] += threatScoreReducible;
      }

      closerCount += newCloserCount;
      if(closerCount > H_MAX_EQUAL_OR_CLOSER_UF_COUNT)
        break;
    }
  }
}

void PThreats::printEval(pla_t pla, const eval_t trapThreats[2][4], ostream& out)
{
  pla_t opp = gOpp(pla);
  eval_t plaSum = trapThreats[pla][0] + trapThreats[pla][1] + trapThreats[pla][2] + trapThreats[pla][3];
  eval_t oppSum = trapThreats[opp][0] + trapThreats[opp][1] + trapThreats[opp][2] + trapThreats[opp][3];
  out << Global::strprintf("PSum: %4d\n", plaSum);
  out << Global::strprintf("OSum: %4d\n", oppSum);
  out << Global::strprintf("  P P   O O:  %4d  %4d    %4d  %4d\n",
      trapThreats[pla][2],
      trapThreats[pla][3],
      trapThreats[opp][2],
      trapThreats[opp][3]);
  out << Global::strprintf("  P P   O O:  %4d  %4d    %4d  %4d\n",
      trapThreats[pla][0],
      trapThreats[pla][1],
      trapThreats[opp][0],
      trapThreats[opp][1]);
}


static const int HEAVY_BASE[2][BSIZE] = {
{
 70, 85, 85, 85, 85, 85, 85, 70,  0,0,0,0,0,0,0,0,
 85, 90, 90,100,100, 90, 90, 85,  0,0,0,0,0,0,0,0,
 90, 90, 72,100,100, 72, 90, 90,  0,0,0,0,0,0,0,0,
 72, 76, 85, 87, 87, 85, 76, 72,  0,0,0,0,0,0,0,0,
 43, 46, 48, 50, 50, 48, 46, 43,  0,0,0,0,0,0,0,0,
 23, 24, 24, 28, 28, 24, 24, 23,  0,0,0,0,0,0,0,0,
  7,  5,  0,  8,  8,  0,  5,  7,  0,0,0,0,0,0,0,0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,0,0,0,0,0,0,0,
},
{
  0,  0,  0,  0,  0,  0,  0,  0,  0,0,0,0,0,0,0,0,
  7,  5,  0,  8,  8,  0,  5,  7,  0,0,0,0,0,0,0,0,
 23, 24, 24, 28, 28, 24, 24, 23,  0,0,0,0,0,0,0,0,
 43, 46, 48, 50, 50, 48, 46, 43,  0,0,0,0,0,0,0,0,
 72, 76, 85, 87, 87, 85, 76, 72,  0,0,0,0,0,0,0,0,
 90, 90, 72,100,100, 72, 90, 90,  0,0,0,0,0,0,0,0,
 85, 90, 90,100,100, 90, 90, 85,  0,0,0,0,0,0,0,0,
 70, 85, 85, 85, 85, 85, 85, 70,  0,0,0,0,0,0,0,0,
}};



static const double HEAVY_PERCENT = 0.04;
static const int HEAVY_ADIST_FACTOR[5] =
{160,100,90,50,40};

static const double HEAVY_FROZEN_FACTOR = 1.1;
static const double HEAVY_AHEAD_WEAKER_FACTOR = 0.60;
static const double HEAVY_AHEAD_WEAKER_CLOSE_FACTOR = 0.35;
static const double HEAVY_AHEAD_WEAKER_CLOSEUF_FACTOR = 0.25;
static const double HEAVY_EQUAL_WEAKER_FACTOR = 0.85;
static const double HEAVY_PUSH_BLOCKED_FACTOR = 0.5;

static const double HEAVY_SCALE = 0.89;

void PThreats::accumHeavyThreats(const Board& b, const int pStronger[2][NUMTYPES],
    const Bitmap pStrongerMaps[2][NUMTYPES], const int pValues[2][NUMTYPES], const int ufDist[BSIZE],
    const int tc[2][4], eval_t trapThreats[2][4], eval_t pieceThreats[BSIZE])
{
  //Currently only for the camel...
  for(pla_t pla = 0; pla <= 1; pla++)
  {
    for(piece_t piece = CAM; piece >= HOR; piece--)
    {
      if(pStronger[pla][piece] != 1 || b.pieceCounts[pla][piece] <= 0)
        continue;

      Bitmap map = b.pieceMaps[pla][piece];
      while(map.hasBits())
      {
        loc_t loc = map.nextBit();
        int locx = gX(loc);
        double baseValue = HEAVY_BASE[pla][loc];

        double tcFactor;
        if(locx <= 2)
          tcFactor = 0.35 - 0.012 * tc[pla][Board::PLATRAPINDICES[gOpp(pla)][0]];
        else if(locx == 3)
          tcFactor = (2.0/3.0) * (0.35 - 0.012 * tc[pla][Board::PLATRAPINDICES[gOpp(pla)][0]]) +
                     (1.0/3.0) * (0.35 - 0.012 * tc[pla][Board::PLATRAPINDICES[gOpp(pla)][1]]);
        else if(locx == 1)
          tcFactor = (1.0/3.0) * (0.35 - 0.012 * tc[pla][Board::PLATRAPINDICES[gOpp(pla)][0]]) +
                     (2.0/3.0) * (0.35 - 0.012 * tc[pla][Board::PLATRAPINDICES[gOpp(pla)][1]]);
        else
          tcFactor = 0.35 - 0.012 * tc[pla][Board::PLATRAPINDICES[gOpp(pla)][1]];

        if(tcFactor <= 0)
          continue;
        if(tcFactor >= 1)
          tcFactor = 1;

        baseValue *= tcFactor;

        int aDist = Threats::attackDist(b,loc,5,pStrongerMaps,ufDist);
        if(aDist >= 5)
          continue;
        baseValue *= HEAVY_ADIST_FACTOR[aDist];
        baseValue *= pValues[pla][piece];
        baseValue *= HEAVY_PERCENT / 10000.0;

        if(ufDist[loc] > 0)
          baseValue *= HEAVY_FROZEN_FACTOR;

        int advOff = Board::ADVANCE_OFFSET[pla];
        for(int dx = -1; dx <= 1; dx++)
        {
          if(locx + dx < 0 || locx + dx > 7)
            continue;
          int dxOff = gDXOffset(dx);
          if(b.owners[loc+dxOff+advOff*2] == pla && b.pieces[loc+dxOff+advOff*2] < piece)
            baseValue *= HEAVY_AHEAD_WEAKER_FACTOR;
          if(b.owners[loc+dxOff+advOff] == pla && b.pieces[loc+dxOff+advOff] < piece)
            baseValue *= ufDist[loc+dxOff+advOff] <= 0 ? HEAVY_AHEAD_WEAKER_CLOSEUF_FACTOR : HEAVY_AHEAD_WEAKER_CLOSE_FACTOR;
          if(dx != 0 && b.owners[loc+dxOff] == pla && b.pieces[loc+dxOff] < piece)
            baseValue *= HEAVY_EQUAL_WEAKER_FACTOR;
        }
        bool noEmptySpace = (Board::ADVANCEMENT[pla][loc] <= 2 &&
            (b.owners[loc W+advOff] != NPLA) &&
            (b.owners[loc  +advOff] != NPLA) &&
            (b.owners[loc+2*advOff] != NPLA) &&
            (b.owners[loc E+advOff] != NPLA));
        if(noEmptySpace)
          baseValue *= HEAVY_PUSH_BLOCKED_FACTOR;

        baseValue *= HEAVY_SCALE;
        int ktID = locx < 4 ? Board::PLATRAPINDICES[gOpp(pla)][0] : Board::PLATRAPINDICES[gOpp(pla)][1];
        pieceThreats[loc] += (int)baseValue;
        trapThreats[pla][ktID] += (int)baseValue;
      }
    }
  }
}

static const int REDUCTION_BY_E_DIST[16] =
{48,64,50,24,0,0,0,0,0,0,0,0,0,0,0,0};
eval_t PThreats::adjustForReducibleThreats(const Board& b, pla_t pla,
    const eval_t trapThreats[2][4], const eval_t reducibleTrapThreats[2][4],
    const int numPStrats[2], const Strat pStrats[2][Strats::numStratsMax], loc_t& bestGainAtBuf)
{
  bestGainAtBuf = ERRLOC;
  //Look for the player elephant
  loc_t eleLoc = b.findElephant(pla);
  if(eleLoc == ERRLOC)
    return 0;

  //Count up total trap threats
  eval_t totalThreat = 0;
  for(int i = 0; i<4; i++)
    totalThreat += trapThreats[pla][i];

  //Count up total opp strats
  eval_t totalOppStratValue = 0;
  pla_t opp = gOpp(pla);
  for(int i = 0; i<numPStrats[opp]; i++)
  {
    int value = pStrats[opp][i].value;
    if(value <= 0) continue;
    totalOppStratValue += value;
  }

  //Count up pla strats needing the elephant to hold
  eval_t plaEleStratValue = 0;
  for(int i = 0; i<numPStrats[pla]; i++)
  {
    if(pStrats[pla][i].plaHolders.isOne(eleLoc) && pStrats[pla][i].value > 0)
      plaEleStratValue += pStrats[pla][i].value;
  }

  //At the cost of increasing all other threats by 1.375x  and making all opp strats worth 1.375x,
  //and giving up all strats involving ele as holder, you may reduce the reducible trap threats
  //by a factor of REDUCTION_BY_E_DIST at a single trap

  eval_t bestGain = 0;
  loc_t bestGainAt = ERRLOC;
  for(int i = 0; i<4; i++)
  {
    eval_t reducibleAmount = reducibleTrapThreats[pla][i];
    if(reducibleAmount <= 0)
      continue;
    int mDist = Board::manhattanDist(eleLoc,Board::TRAPLOCS[i]);
    int reductionFactor = REDUCTION_BY_E_DIST[mDist];
    if(reductionFactor <= 0)
      continue;
    eval_t gain = (reducibleAmount * reductionFactor) >> 6;
    gain = gain - plaEleStratValue - ((totalOppStratValue*3)>>3) - (((totalThreat - trapThreats[pla][i])*3)>>3);
    if(gain > bestGain)
    {
      bestGain = gain;
      bestGainAt = Board::TRAPLOCS[i];
    }
  }
  bestGainAtBuf = bestGainAt;
  return bestGain;
}

