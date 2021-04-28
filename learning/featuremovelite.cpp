
/*
 * featuremove.cpp
 * Author: davidwu
 */

#include <boost/static_assert.hpp>
#include <algorithm>
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../eval/eval.h"
#include "../eval/threats.h"
#include "../eval/internal.h"
#include "../learning/feature.h"
#include "../learning/featuremove.h"
#include "../learning/featurearimaa.h"
#include "../search/searchutils.h"

using namespace std;

//FEATURESET-----------------------------------------------------------------------------------------

static FeatureSet fset;

//PARAMS----------------------------------------------------------------------------------------------
static const int NUMPIECEINDICES = 8;

//BASE------------------------------------------------------------------------------------------------
//Prior feature
static fgrpindex_t PRIOR;
//Pass feature
static fgrpindex_t PASS;

//SRCS AND DESTS--------------------------------------------------------------------------------------
static const bool ENABLE_SRCDEST = true;

//Source locations of step sequence for pla/opp. (pieceindex 0-7)(symloc 0-31)
static fgrpindex_t SRC;
static fgrpindex_t SRC_O;
//Destination locations of step sequence for pla/opp. (pieceindex 0-7)(symloc 0-32)
static fgrpindex_t DEST;
static fgrpindex_t DEST_O;

//SRCSS AND DESTS GIVEN OPP RADII-------------------------------------------------------------------
static const bool ENABLE_SRCDEST_GIVEN_OPPRAD = true;

//Steps of pla pieces given various radii from opponent pieces
static fgrpindex_t SRC_WHILE_OPP_RAD;  //(symloc 0-31)(opprad 1-5 (-1))
static fgrpindex_t DEST_WHILE_OPP_RAD; //(symloc 0-31)(opprad 1-5 (-1))

static const bool ENABLE_STRATS = true;

//SPECIAL CASE FRAME SAC-------------------------------------------------------------------------------
//Sacrifices a framed pla piece. (pieceindex 0-7)(iseleele 0-1)
static fgrpindex_t FRAMESAC;

//TRAP STATE-----------------------------------------------------------------------------------------
static const bool ENABLE_TRAPSTATE = true;

//Changes the pla/opp trapstate at a trap. (isopptrap 0-1)(oldstate 0-8)(newstate 0-8)
static fgrpindex_t TRAP_STATE_P;
static fgrpindex_t TRAP_STATE_O;

//LIKELY CAPTURES-----------------------------------------------------------------------------------
static const bool ENABLE_LIKELY_CAP = false;

//(isopptrap 0-1)(prev_numtrapdefs 0-4)(numtrapdefs 0-2)
static fgrpindex_t LIKELY_PLACAPTHREAT;
//(isopptrap 0-1)(prev_actually_threatened 0-1)(prev_numtrapdefs 0-4)(numtrapdefs 0-2)
static fgrpindex_t LIKELY_OPPCAPTHREAT;
//(isopptrap 0-1)(oldstate 0-8)(newstate 0-8)
static fgrpindex_t NO_LONGER_LIKELY_THREATENED;


//CAPTURE THREATENING-----------------------------------------------------------------------------------
static const bool ENABLE_CAP_THREAT = true;

//Threatens to capture a formerly safe piece of the opponent. (pieceindex 0-7)(dist 0-4)(isopptrap 0-1)
static fgrpindex_t THREATENS_CAP;
//Allows an opponent to capture a formerly safe piece that we just moved. (pieceindex 0-7)(dist 0-4)(isopptrap 0-1)
static fgrpindex_t INVITES_CAP_MOVED;
//Allows an opponent to capture a formerly safe piece that we didn't just move. (pieceindex 0-7)(dist 0-4)(isopptrap 0-1)
static fgrpindex_t INVITES_CAP_UNMOVED;
//Prevents the opponent from capturing a piece (without sacrificing it) (pieceindex 0-7)(piecesymloc 0-31)
static fgrpindex_t PREVENTS_CAP;

//CAPTURE DEFENSE---------------------------------------------------------------------------------------
static const bool ENABLE_CAP_DEF = true;

//Adds an elephant defender to a trap that the opponent could capture at. (isopptrap 0-1)(threat 0-4)
static fgrpindex_t CAP_DEF_ELEDEF;
//Adds a non-elephant defender to a trap that the opponent could capture at. (isopptrap 0-1)(threat 0-4)
static fgrpindex_t CAP_DEF_TRAPDEF;
//Moves a pla piece that was threatened with capture. (isopptrap 0-1)(threat 0-4)
static fgrpindex_t CAP_DEF_RUNAWAY;
//Moves or freezes an opp piece that was threatening to capture. (isopptrap 0-1)(threat 0-4)
static fgrpindex_t CAP_DEF_INTERFERE;


//FREEZING-----------------------------------------------------------------------------------------------
static const bool ENABLE_FREEZE = true;

//Freezes/thaws opponent/pla piece of the given type (ispla 0-1)(pieceindex 0-7)
static fgrpindex_t FREEZES_STR;
static fgrpindex_t THAWS_STR;

//Freezes/thaws opponent/pla piece at the given loc (ispla 0-1)(symloc 0-31)
static fgrpindex_t FREEZES_AT;
static fgrpindex_t THAWS_AT;


//DOMINATION-----------------------------------------------------------------------------------------------
static const bool ENABLE_DOMINATE = true;

//Dominates/undominates opponent/pla piece of the given type (ispla 0-1)(pieceindex 0-7)
static fgrpindex_t DOMS_STR;
static fgrpindex_t UNDOMS_STR;

//Dominates/undominates opponent/pla piece at the given loc (ispla 0-1)(symloc 0-31)
static fgrpindex_t DOMS_AT;
static fgrpindex_t UNDOMS_AT;


//DOMINATION FAR-----------------------------------------------------------------------------------------------
static const bool ENABLE_DOMFAR = true;

//Dominates/undominates at radius 2 pla piece of the given type (pieceindex 0-7)
static fgrpindex_t DOMRAD2_STR;
static fgrpindex_t UNDOMRAD2_STR;

//Dominates/undominates at radius 2 pla piece at the given loc (symloc 0-31)
static fgrpindex_t DOMRAD2_AT;
static fgrpindex_t UNDOMRAD2_AT;



//ADVANCEMENT AND INFLUENCE-----------------------------------------------------------------------------
static const bool ENABLE_INFLUENCE = true;

//Moves rabbit. (same/advance 0-1)(finaldistance 0-7)(goalinfluence 0-8 (+4))
static fgrpindex_t RABBIT_ADVANCE;
//Moves piece or rabbit. (retreat/same/advance 0-2)(pieceindex 0-7)(opptrapinfluence 0-8 (+4))
static fgrpindex_t PIECE_ADVANCE;

/*

//CAPTHREATENED-----------------------------------------------------------------------------------
static const bool ENABLE_CAPTHREATENED = true;

//Moves frozen pla piece (isplatrap 0-1) (plapieceindex 0-7)  or (isplatrap 0-1) (symloc 0-31)
static fgrpindex_t MOVES_CAPTHREATENED_STR;
static fgrpindex_t MOVES_CAPTHREATENED_AT;

//Defends capthreated trap (isplatrap 0-1) (newdefcount 0-2) (opptrapstate 0-7) (plapieceindex 0-7) or (isplatrap 0-1) (symloc 0-31)
static fgrpindex_t DEFENDS_CAPTHREATED_TRAP_STR;
static fgrpindex_t DEFENDS_CAPTHREATED_TRAP_AT;

//LIKELY CAPTURES-------------------------------------------------------------------------------
static const bool ENABLE_LIKELY_CAPTHREAT = true;

//(isplatrap 0-1)
static fgrpindex_t LIKELY_CAPDANGER_ADJ;
static fgrpindex_t LIKELY_CAPDANGER_ADJ2;
static fgrpindex_t LIKELY_CAPTRAPDEF;

static fgrpindex_t LIKELY_CAPHANG;

static fgrpindex_t LIKELY_CAPTHREAT_STATIC;
static fgrpindex_t LIKELY_CAPTHREAT_LOOSE;
static fgrpindex_t LIKELY_CAPTHREAT_PUSHPULL;
static fgrpindex_t LIKELY_CAPTHREAT_PUSHPULL_LOOSE;

*/

//PHALANXES---------------------------------------------------------------------------------------------
static const bool ENABLE_PHALANX = false;
static const bool ENABLE_SIMPLE_PHALANX = false;

//This move creates a phalanx against a piece (pieceindex 0-7) (issingle 0-1)
static fgrpindex_t CREATES_PHALANX_VS;
//This move creates a phalanx against a piece at (symloc 0-31) (dir 0-2) (issingle 0-1)
static fgrpindex_t CREATES_PHALANX_AT;

//This move releases a phalanx against a piece (pieceindex 0-7) (issingle 0-1)
static fgrpindex_t RELEASES_PHALANX_VS;
//This move releases a phalanx against a piece at (symloc 0-31) (dir 0-2) (issingle 0-1)
static fgrpindex_t RELEASES_PHALANX_AT;



//RABBIT GOAL DIST-------------------------------------------------------------------------------
//static const bool ENABLE_RABGDIST = false;

//Advances rabbit (distestimate 0-9)
//static fgrpindex_t RAB_GOAL_DIST_SRC;
//static fgrpindex_t RAB_GOAL_DIST_DEST;


//MOVE DEPENDENCY--------------------------------------------------------------------------------
static const bool ENABLE_DEPENDENCY = true;

//T1 - Steps are amalgamated if...
//     Src of one is within radius 1 of the dest of the other
//T2 - Steps are amalgamated if...
//     Srcdests are adjacent to srcdests
//     Srcdests are adjacent to the same trap
//T3 - Steps are amalgamated if...
//     Srcdests are within radius 2 of srcdests

//(numSteps 0-3 (-1))(numindepT1 0-3 (-1))(numindepT2 0-3 (-1))(numindepT3 0-3 (-1))
static fgrpindex_t STEP_INDEPENDENCE;

//GOAL THREATENING--------------------------------------------------------------------------------------
//These are costly to compute during an actual search, but are included for the purpose of reducing
//noise in the training.
static const bool ENABLE_GOAL_THREAT = true;
//Wins the game
static fgrpindex_t WINS_GAME;
//Allows the opponent to goal
static fgrpindex_t ALLOWS_GOAL;


//These are actually computed in the search
static const bool ENABLE_REAL_GOAL_THREAT = true;
//Threatens goal when we couldn't goal previously. (threat 1-4)
static fgrpindex_t THREATENS_GOAL;


//Helpers---------------------------------------------------------------------------------------
static void initPriors();

static bool IS_INITIALIZED = false;
void MoveFeatureLite::initFeatureSet()
{
  if(IS_INITIALIZED)
    return;

  PRIOR = fset.add("Prior");
  PASS = fset.add("Pass");

  if(ENABLE_SRCDEST)
  {
    SRC = fset.add("Src",NUMPIECEINDICES,Board::NUMSYMLOCS);
    SRC_O = fset.add("SrcO",NUMPIECEINDICES,Board::NUMSYMLOCS);
    DEST = fset.add("Dest",NUMPIECEINDICES,Board::NUMSYMLOCS+1);
    DEST_O = fset.add("DestO",NUMPIECEINDICES,Board::NUMSYMLOCS+1);
  }

  if(ENABLE_SRCDEST_GIVEN_OPPRAD)
  {
    SRC_WHILE_OPP_RAD = fset.add("SrcWhileOppRad",Board::NUMSYMLOCS,5);
    DEST_WHILE_OPP_RAD = fset.add("DestWhileOppRad",Board::NUMSYMLOCS,5);
  }

  if(ENABLE_STRATS)
  {
    FRAMESAC = fset.add("FrameSac",NUMPIECEINDICES, 2);
  }

  if(ENABLE_TRAPSTATE)
  {
    TRAP_STATE_P = fset.add("TrapStateP",2,9,9);
    TRAP_STATE_O = fset.add("TrapStateO",2,9,9);
  }

  if(ENABLE_LIKELY_CAP)
  {
    LIKELY_PLACAPTHREAT = fset.add("LikelyPlaCapThreat",2,5,3);
    LIKELY_OPPCAPTHREAT = fset.add("LikelyOppCapThreat",2,2,5,3);
    NO_LONGER_LIKELY_THREATENED = fset.add("NoLongerLikelyThreatened",2,9,9);
  }

  if(ENABLE_CAP_THREAT)
  {
    THREATENS_CAP = fset.add("ThreatensCap",NUMPIECEINDICES,5,2);
    INVITES_CAP_MOVED = fset.add("InvitesCapMoved",NUMPIECEINDICES,5,2);
    INVITES_CAP_UNMOVED = fset.add("InvitesCapUnmoved",NUMPIECEINDICES,5,2);
    PREVENTS_CAP = fset.add("PreventsCap",NUMPIECEINDICES,Board::NUMSYMLOCS);
  }

  if(ENABLE_CAP_DEF)
  {
    CAP_DEF_ELEDEF = fset.add("CapDefEleDef",2,5);
    CAP_DEF_TRAPDEF = fset.add("CapDefTrapDef",2,5);
    CAP_DEF_RUNAWAY = fset.add("CapDefRunaway",2,5);
    CAP_DEF_INTERFERE = fset.add("CapDefInterfere",2,5);
  }

  if(ENABLE_FREEZE)
  {
    FREEZES_STR = fset.add("FreezesStr",2,NUMPIECEINDICES);
    THAWS_STR = fset.add("ThawsStr",2,NUMPIECEINDICES);
    FREEZES_AT = fset.add("FreezesAt",2,Board::NUMSYMLOCS);
    THAWS_AT = fset.add("ThawsAt",2,Board::NUMSYMLOCS);
  }
  if(ENABLE_DOMINATE)
  {
    DOMS_STR = fset.add("DomsStr",2,NUMPIECEINDICES);
    UNDOMS_STR = fset.add("UndomsStr",2,NUMPIECEINDICES);
    DOMS_AT = fset.add("DomsAt",2,Board::NUMSYMLOCS);
    UNDOMS_AT = fset.add("UndomsAt",2,Board::NUMSYMLOCS);
  }
  if(ENABLE_DOMFAR)
  {
    DOMRAD2_STR = fset.add("DomRad2Str",NUMPIECEINDICES);
    UNDOMRAD2_STR = fset.add("UndomRad2SStr",NUMPIECEINDICES);
    DOMRAD2_AT = fset.add("DomRad2SAt",Board::NUMSYMLOCS);
    UNDOMRAD2_AT = fset.add("UndomRad2SAt",Board::NUMSYMLOCS);
  }
  if(ENABLE_INFLUENCE)
  {
    RABBIT_ADVANCE = fset.add("RabbitAdvance",2,8,9);
    PIECE_ADVANCE = fset.add("PieceAdvance",3,NUMPIECEINDICES,9);
  }
  if(ENABLE_PHALANX)
  {
    CREATES_PHALANX_VS = fset.add("CreatesPhalanxVs",NUMPIECEINDICES,2);
    CREATES_PHALANX_AT = fset.add("CreatesPhalanxAtLoc",32,Board::NUMSYMDIR,2);
    RELEASES_PHALANX_VS = fset.add("ReleasesPhalanxVs",NUMPIECEINDICES,2);
    RELEASES_PHALANX_AT = fset.add("ReleasesPhalanxAtLoc",32,Board::NUMSYMDIR,2);
  }

  /*
  if(ENABLE_CAPTHREATENED)
  {
    MOVES_CAPTHREATENED_STR = fset.add("MovesCapthreatedStr",2,NUMPIECEINDICES);
    MOVES_CAPTHREATENED_AT = fset.add("MovesCapthreatedAt",2,Board::NUMSYMLOCS);
    DEFENDS_CAPTHREATED_TRAP_STR = fset.add("DefendsCapThreatedTrapStr",2,3,8,NUMPIECEINDICES);
    DEFENDS_CAPTHREATED_TRAP_AT = fset.add("DefendsCapThreatedTrapAt",2,Board::NUMSYMLOCS);
  }

  if(ENABLE_LIKELY_CAPTHREAT)
  {
    LIKELY_CAPDANGER_ADJ = fset.add("LikelyCapDangerAdj",2);
    LIKELY_CAPDANGER_ADJ2 = fset.add("LikelyCapDangerAdj2",2);
    LIKELY_CAPTRAPDEF = fset.add("LikelyCapTrapdef",2);

    LIKELY_CAPHANG = fset.add("LikelyCapHang",2);

    LIKELY_CAPTHREAT_STATIC = fset.add("LikelyCapthreatStatic");
    LIKELY_CAPTHREAT_LOOSE = fset.add("LikelyCapthreatLoose");
    LIKELY_CAPTHREAT_PUSHPULL = fset.add("LikelyCapthreatPushpull");
    LIKELY_CAPTHREAT_PUSHPULL_LOOSE = fset.add("LikelyCapthreatPushpullLoose");
  }

  */

  if(ENABLE_DEPENDENCY)
  {
    const string names[4] = {string("1"),string("2"),string("3"),string("4")};
    STEP_INDEPENDENCE = fset.add("StepIndependence",4,4,4,4,names,names,names,names);
  }

  if(ENABLE_GOAL_THREAT)
  {
    WINS_GAME = fset.add("WinsGame");
    ALLOWS_GOAL = fset.add("AllowsGoal");
  }

  if(ENABLE_REAL_GOAL_THREAT)
  {
    THREATENS_GOAL = fset.add("ThreatensGoal",5);
  }

/*
  if(ENABLE_RABGDIST)
  {
    RAB_GOAL_DIST_SRC = fset.add("RabGoalDistSrc",10);
    RAB_GOAL_DIST_DEST = fset.add("RabGoalDistDest",10);
  }
*/
  initPriors();

  IS_INITIALIZED = true;
}

static const int NUM_PARALLEL_WEIGHTS = 1;

static vector<double> lossPriorWeight;
static vector<double> winPriorWeight;

static void addWLPriorFor(fgrpindex_t group, double weightWin, double weightLoss)
{
  for(int i = 0; i<fset.groups[group].size; i++)
  {
    findex_t feature = i + fset.groups[group].baseIdx;
    lossPriorWeight[feature] += weightLoss;
    winPriorWeight[feature] += weightWin;
  }
}

static void initPriors()
{
  //Add a prior over everything
  lossPriorWeight = vector<double>(fset.numFeatures,3.0);
  winPriorWeight = vector<double>(fset.numFeatures,3.0);

  if(ENABLE_SRCDEST_GIVEN_OPPRAD)
  {
    addWLPriorFor(SRC_WHILE_OPP_RAD,3.0,3.0);
    addWLPriorFor(DEST_WHILE_OPP_RAD,3.0,3.0);
  }
}

static double computeLogProbOfWin(double coeff1, double coeff2)
{
  double diff = coeff1 - coeff2;
  if(diff < -40)
    return diff;
  return -log(1.0 + exp(-diff));
}

static double getPriorLogProb(const vector<vector<double>>& coeffsForFolds)
{
  double logProb = 0;

  int numFolds = coeffsForFolds.size();
  DEBUGASSERT(numFolds == NUM_PARALLEL_WEIGHTS);
  for(int parallelFold = 0; parallelFold < (int)coeffsForFolds.size(); parallelFold++)
  {
    const vector<double>& coeffs = coeffsForFolds[parallelFold];
    int size = coeffs.size();
    DEBUGASSERT(size == fset.numFeatures);
    for(int i = 0; i<size; i++)
    {
      logProb += winPriorWeight[i] * computeLogProbOfWin(coeffs[i],0);
      logProb += lossPriorWeight[i] * computeLogProbOfWin(0,coeffs[i]);
    }
  }
  return logProb;
}

static const int TRAPSTATEEXTENDED_ADD_ELE_TRANSITION[9] =
{2,4,4,6,6,8,8,8,8};
static const int TRAPSTATEEXTENDED_ADD_PIECE_TRANSITION[9] =
{1,3,4,5,6,7,8,7,8};
static const int TRAPSTATEEXTENDED_REMOVE_ELE_TRANSITION[9] =
{0,0,0,1,1,3,3,5,5};
static const int TRAPSTATEEXTENDED_REMOVE_PIECE_TRANSITION[9] =
{0,0,0,1,2,3,4,5,6};
static const bool TRAPSTATEEXTENDED_IS_SAFE[9] =
{false,false,true,false,true,true,true,true,true};

struct MoveFeatureLitePosData
{
  bool forTraining;

  Board board;

  int pStronger[2][NUMTYPES];
  int pieceIndex[2][NUMTYPES];
  Bitmap pStrongerMaps[2][NUMTYPES];

  //Array of the maximum strength opponent piece around each spot, EMP if none
  //piece_t oppMaxStrAdj[BSIZE];
  //Array of the minimum strength opponent piece around each spot, ELE+1 if none
  //piece_t oppMinStrAdj[BSIZE];

  //Is the opponent threatening a goal?
  bool goalThreatened;

  //Own frozen pieces
  Bitmap plaFrozen;

  //Own pieces threatened by opponent with capture
  Bitmap plaCapThreatenedPlaTrap;
  Bitmap plaCapThreatenedOppTrap;

  //Threatened traps - can lose piece here
  bool isTrapCapThreatened[4];

  int ufDist[BSIZE];
  CapThreat oppCapThreats[Threats::maxCapsPerPla];
  int numOppCapThreats;

  int oldTrapStates[2][4];

  bool oppHoldsFrameAtTrap[4];
  bool oppFrameIsPartial[4];
  bool oppFrameIsEleEle[4];

  Bitmap plaCapMap;
  Bitmap oppCapMap;

  int plaGoalDist;

  int influence[2][BSIZE];

  //Squares where we are likely threatened with capture in the adjacent trap if the trap has 1 defenders
  //and we are leq the specified strength
  //piece_t likelyDangerAdjTrapStr[BSIZE];
  //Squares where we are likely threatened with capture in the trap 2 steps away if the trap has 0 defenders
  //and we are leq the specified strength
  //piece_t likelyDangerAdj2TrapStr[BSIZE];
  //Traps whose #defenders could be increased to defend against capture
  //bool trapNeedsMoreDefs[4];
  //Number of defenders next to trap needed for us to be likely capsafe there
  //int minDefendersToBeLikelySafe[4];

  //Squares where we will likely threaten an opp with capture if we put a piece geq the specified strength
  //piece_t likelyThreatStr[BSIZE];

  //Squares where we will likely loosely threaten an opp with capture if we put a piece geq the specified strength
  //piece_t likelyLooseThreatStr[BSIZE];

  //Squares where we will likely threaten an opp with capture if we pushpull an opp piece there
  //Bitmap likelyThreatPushPullLocs;
  //Bitmap likelyThreatPushPullLocsLoose;

  //Estimated steps to goal if a rabbit steps here.
  //int rabbitGoalDist[BSIZE];

  //Opponent pieces threatening ours with cap
  //Bitmap oppThreateners;
  //Squares where we would likely threaten goal if we moved a rabbit there
  //Bitmap likelyGoalThreat;

};

static void getFeatures(Board& bAfter, const void* dataBuf, pla_t pla, move_t move, const BoardHistory& hist,
    void (*handleFeature)(findex_t,void*), void* handleFeatureData)
{
  (void)hist;
  //Handle Pass Move
  if(move == PASSMOVE || move == QPASSMOVE)
  {
    (*handleFeature)(fset.get(PASS),handleFeatureData);
    return;
  }

  const MoveFeatureLitePosData& data = *((const MoveFeatureLitePosData*)(dataBuf));
  const Board& b = data.board;
  DEBUGASSERT(bAfter.posStartHash == b.posCurrentHash);

  Bitmap pStrongerMaps[2][NUMTYPES];
  bAfter.initializeStrongerMaps(pStrongerMaps);

  //Source and destination piece movement features
  pla_t opp = gOpp(pla);
  loc_t src[8];
  loc_t dest[8];
  int8_t newTrapGuardCounts[2][4];
  int numChanges = b.getChanges(move,src,dest,newTrapGuardCounts);
  Bitmap oppMap = b.pieceMaps[opp][0];

  int newTrapStates[2][4];
  if(ENABLE_TRAPSTATE)
  {
    for(int i = 0; i<4; i++)
    {
      newTrapStates[SILV][i] = data.oldTrapStates[SILV][i];
      newTrapStates[GOLD][i] = data.oldTrapStates[GOLD][i];
    }
  }

  for(int i = 0; i<numChanges; i++)
  {
    //Retrieve piece stats
    pla_t owner = b.owners[src[i]];
    piece_t piece = b.pieces[src[i]];
    int pieceIndex = data.pieceIndex[owner][piece];

    if(ENABLE_SRCDEST)
    {
      findex_t srcFeature;
      findex_t destFeature;
      if(owner == pla) {srcFeature = SRC; destFeature = DEST;}
      else             {srcFeature = SRC_O; destFeature = DEST_O;}

      (*handleFeature)(fset.get(srcFeature,pieceIndex,Board::SYMLOC[pla][src[i]]),handleFeatureData);
      (*handleFeature)(fset.get(destFeature,pieceIndex,(dest[i] == ERRLOC) ? 32 : Board::SYMLOC[pla][dest[i]]),handleFeatureData);
    }

    if(ENABLE_SRCDEST_GIVEN_OPPRAD && owner == pla && dest[i] != ERRLOC)
    {
      int srcRad = 1;
      while(srcRad < 5)
      {
        if((oppMap & Board::DISK[srcRad][src[i]]).hasBits())
          break;
        srcRad++;
      }
      int destRad = 1;
      while(destRad < 5)
      {
        if((oppMap & Board::DISK[destRad][dest[i]]).hasBits())
          break;
        destRad++;
      }
      (*handleFeature)(fset.get(SRC_WHILE_OPP_RAD,Board::SYMLOC[pla][src[i]],srcRad-1),handleFeatureData);
      (*handleFeature)(fset.get(DEST_WHILE_OPP_RAD,Board::SYMLOC[pla][src[i]],destRad-1),handleFeatureData);
    }

    if(ENABLE_STRATS)
    {
      if(dest[i] == ERRLOC && owner == pla && Board::ISTRAP[src[i]] && data.oppHoldsFrameAtTrap[Board::TRAPINDEX[src[i]]])
      {
        bool isEleEle = data.oppFrameIsEleEle[Board::TRAPINDEX[src[i]]];
        (*handleFeature)(fset.get(FRAMESAC,pieceIndex,isEleEle),handleFeatureData);
      }
    }

    if(ENABLE_TRAPSTATE)
    {
      int srcAdjTrapIndex = Board::ADJACENTTRAPINDEX[src[i]];
      if(srcAdjTrapIndex != -1)
      {
        newTrapStates[owner][srcAdjTrapIndex] = piece == ELE ?
            TRAPSTATEEXTENDED_REMOVE_ELE_TRANSITION  [newTrapStates[owner][srcAdjTrapIndex]] :
            TRAPSTATEEXTENDED_REMOVE_PIECE_TRANSITION[newTrapStates[owner][srcAdjTrapIndex]];
      }
      if(dest[i] != ERRLOC)
      {
        int destAdjTrapIndex = Board::ADJACENTTRAPINDEX[dest[i]];
        if(destAdjTrapIndex != -1)
        {
          newTrapStates[owner][destAdjTrapIndex] = piece == ELE ?
              TRAPSTATEEXTENDED_ADD_ELE_TRANSITION  [newTrapStates[owner][destAdjTrapIndex]] :
              TRAPSTATEEXTENDED_ADD_PIECE_TRANSITION[newTrapStates[owner][destAdjTrapIndex]];
        }
      }
    }

    if(ENABLE_INFLUENCE && owner == pla && dest[i] != ERRLOC)
    {
      if(piece == RAB)
      {
        int srcDist = Board::GOALYDIST[pla][src[i]];
        int destDist = Board::GOALYDIST[pla][dest[i]];
        loc_t goalTarget = dest[i] + destDist * (pla == GOLD ? N : S);
        int influence = data.influence[pla][goalTarget];
        (*handleFeature)(fset.get(RABBIT_ADVANCE,destDist<srcDist,destDist,influence),handleFeatureData);
      }

      int advdest = Board::ADVANCEMENT[pla][dest[i]];
      int advsrc = Board::ADVANCEMENT[pla][src[i]];

      int influence;
      int file = gX(dest[i]);
      if(file <= 2) influence = data.influence[pla][Board::PLATRAPLOCS[opp][0]];
      else if(file >= 5) influence = data.influence[pla][Board::PLATRAPLOCS[opp][1]];
      else influence = (data.influence[pla][Board::PLATRAPLOCS[opp][0]] + data.influence[pla][Board::PLATRAPLOCS[opp][1]])/2;

      int retreatSideAdvance = advsrc > advdest ? 0 : advsrc == advdest ? 1 : 2;
      (*handleFeature)(fset.get(
          PIECE_ADVANCE,retreatSideAdvance,data.pieceIndex[pla][piece],influence),handleFeatureData);
    }

    /*
    if(ENABLE_RABGDIST && owner == pla && piece == RAB && dest[i] != ERRLOC)
    {
      (*handleFeature)(fset.get(RAB_GOAL_DIST_SRC,data.rabbitGoalDist[src[i]]),handleFeatureData);
      (*handleFeature)(fset.get(RAB_GOAL_DIST_DEST,data.rabbitGoalDist[dest[i]]),handleFeatureData);
    }*/

    if(ENABLE_DOMFAR && owner == pla && dest[i] != ERRLOC)
    {
      bool wasDomRad2 = (data.pStrongerMaps[pla][piece] & ~b.frozenMap & Board::DISK[2][src[i]]).hasBits();
      bool nowDomRad2 = (pStrongerMaps[pla][piece] & ~bAfter.frozenMap & Board::DISK[2][dest[i]]).hasBits();
      if(wasDomRad2 && !nowDomRad2)
      {
        (*handleFeature)(fset.get(UNDOMRAD2_STR,data.pieceIndex[pla][piece]),handleFeatureData);
        (*handleFeature)(fset.get(UNDOMRAD2_AT,Board::SYMLOC[pla][src[i]]),handleFeatureData);
      }
      else if(!wasDomRad2 && nowDomRad2)
      {
        (*handleFeature)(fset.get(DOMRAD2_STR,data.pieceIndex[pla][piece]),handleFeatureData);
        (*handleFeature)(fset.get(DOMRAD2_AT,Board::SYMLOC[pla][dest[i]]),handleFeatureData);
      }
    }
  }

  //Trap State Features
  if(ENABLE_TRAPSTATE)
  {
    for(int trapIndex = 0; trapIndex < 4; trapIndex++)
    {
      int isOppTrap = !Board::ISPLATRAP[trapIndex][pla];
      int oldPlaTrapState = data.oldTrapStates[pla][trapIndex];
      int newPlaTrapState = newTrapStates[pla][trapIndex];
      int oldOppTrapState = data.oldTrapStates[opp][trapIndex];
      int newOppTrapState = newTrapStates[opp][trapIndex];

      if(oldPlaTrapState != newPlaTrapState)
        (*handleFeature)(fset.get(TRAP_STATE_P,isOppTrap,oldPlaTrapState,newPlaTrapState),handleFeatureData);
      if(oldOppTrapState != newOppTrapState)
        (*handleFeature)(fset.get(TRAP_STATE_O,isOppTrap,oldOppTrapState,newOppTrapState),handleFeatureData);
    }
  }

  //Likely capture features
  if(ENABLE_LIKELY_CAP)
  {
    for(int trapIndex = 0; trapIndex < 4; trapIndex++)
    {
      int isOppTrap = !Board::ISPLATRAP[trapIndex][pla];
      int oldPlaTrapState = data.oldTrapStates[pla][trapIndex];
      int newPlaTrapState = newTrapStates[pla][trapIndex];
      int newOppTrapState = newTrapStates[opp][trapIndex];
      bool notThreatened = false;
      if(TRAPSTATEEXTENDED_IS_SAFE[newPlaTrapState])
        notThreatened = true;
      else
      {
        loc_t trapLoc = Board::TRAPLOCS[trapIndex];
        if(newPlaTrapState == ArimaaFeature::TRAPSTATE_0)
        {
          Bitmap potentialThreatened = bAfter.pieceMaps[pla][0] & bAfter.dominatedMap & Board::RADIUS[2][trapLoc];
          if(potentialThreatened.hasBits())
            (*handleFeature)(fset.get(LIKELY_OPPCAPTHREAT,isOppTrap,
                data.isTrapCapThreatened[trapIndex],
                b.trapGuardCounts[pla][trapIndex],0),handleFeatureData);
          else
            notThreatened = true;
        }
        else if(newPlaTrapState == ArimaaFeature::TRAPSTATE_1)
        {
          Bitmap defender = bAfter.pieceMaps[pla][0] & Board::RADIUS[2][trapLoc];
          loc_t defenderLoc = defender.nextBit();
          Bitmap stronger = pStrongerMaps[pla][bAfter.pieces[defenderLoc]];
          if((stronger & (Board::DISK[2][trapLoc] | (Board::DISK[3][trapLoc] & ~bAfter.frozenMap))).hasBits())
            (*handleFeature)(fset.get(LIKELY_OPPCAPTHREAT,isOppTrap,
                data.isTrapCapThreatened[trapIndex],
                b.trapGuardCounts[pla][trapIndex],1),handleFeatureData);
          else
            notThreatened = true;
        }
        else
        {
          Bitmap potentialThreatened = bAfter.pieceMaps[pla][0] & bAfter.dominatedMap & Board::RADIUS[1][trapLoc];
          if(!potentialThreatened.isEmptyOrSingleBit())
            (*handleFeature)(fset.get(LIKELY_OPPCAPTHREAT,isOppTrap,
                data.isTrapCapThreatened[trapIndex],
                b.trapGuardCounts[pla][trapIndex],2),handleFeatureData);
          else
            notThreatened = true;
        }
      }

      if(notThreatened && data.isTrapCapThreatened[trapIndex])
        (*handleFeature)(fset.get(NO_LONGER_LIKELY_THREATENED,isOppTrap,oldPlaTrapState,newPlaTrapState),handleFeatureData);


      if(!TRAPSTATEEXTENDED_IS_SAFE[newOppTrapState])
      {
        loc_t trapLoc = Board::TRAPLOCS[trapIndex];
        if(newOppTrapState == ArimaaFeature::TRAPSTATE_0)
        {
          Bitmap potentialThreatened = bAfter.pieceMaps[opp][0] & bAfter.dominatedMap & Board::RADIUS[2][trapLoc];
          if(potentialThreatened.hasBits())
            (*handleFeature)(fset.get(LIKELY_PLACAPTHREAT,isOppTrap,
                b.trapGuardCounts[opp][trapIndex],0),handleFeatureData);
        }
        else if(newOppTrapState == ArimaaFeature::TRAPSTATE_1)
        {
          Bitmap defender = bAfter.pieceMaps[opp][0] & Board::RADIUS[2][trapLoc];
          loc_t defenderLoc = defender.nextBit();
          Bitmap stronger = pStrongerMaps[opp][bAfter.pieces[defenderLoc]];
          if((stronger & (Board::DISK[2][trapLoc] | (Board::DISK[3][trapLoc] & ~bAfter.frozenMap))).hasBits())
            (*handleFeature)(fset.get(LIKELY_PLACAPTHREAT,isOppTrap,
                b.trapGuardCounts[opp][trapIndex],1),handleFeatureData);
        }
        else
        {
          Bitmap potentialThreatened = bAfter.pieceMaps[opp][0] & bAfter.dominatedMap & Board::RADIUS[1][trapLoc];
          if(!potentialThreatened.isEmptyOrSingleBit())
            (*handleFeature)(fset.get(LIKELY_PLACAPTHREAT,isOppTrap,
                b.trapGuardCounts[opp][trapIndex],2),handleFeatureData);
        }
      }
    }
  }

  if(ENABLE_CAP_THREAT)
  {
    Bitmap capThreatMap;
    //Where was the piece at this location prior to this move?
    loc_t priorLocation[BSIZE];
    for(int i = 0; i<BSIZE; i++)
      priorLocation[i] = i;
    for(int i = 0; i<numChanges; i++)
    {
      if(dest[i] != ERRLOC)
        priorLocation[dest[i]] = src[i];
    }

    //Check pla capture threats around each trap
    for(int i = 0; i<4; i++)
    {
      int capDist;
      Bitmap map;
      //If we can capture...
      if(BoardTrees::canCaps(bAfter,pla,4,Board::TRAPLOCS[i],map,capDist))
      {
        //Record capture threats outside for other stuff to use
        capThreatMap |= map;

        //Locate the biggest piece capturable
        loc_t biggestLoc = ERRLOC;
        piece_t biggestPiece = EMP;
        while(map.hasBits())
        {
          loc_t loc = map.nextBit();
          if(bAfter.pieces[loc] > biggestPiece)
          {
            biggestLoc = loc;
            biggestPiece = bAfter.pieces[loc];
          }
        }

        int isOppTrap = !Board::ISPLATRAP[i][pla];

        //If it wasn't already been threatened, add it!
        if(!data.plaCapMap.isOne(priorLocation[biggestLoc]))
          (*handleFeature)(fset.get(THREATENS_CAP,data.pieceIndex[opp][biggestPiece],capDist,isOppTrap),handleFeatureData);
      }
    }
    //Check for opponent capture threats around each trap
    Bitmap preventedMap = data.oppCapMap;
    for(int i = 0; i<4; i++)
    {
      int capDist;
      Bitmap map;
      //If opp can capture...
      if(BoardTrees::canCaps(bAfter,opp,4,Board::TRAPLOCS[i],map,capDist))
      {
        //Record capture threats outside for other stuff to use
        capThreatMap |= map;

        //Locate the biggest piece capturable
        loc_t biggestLoc = ERRLOC;
        piece_t biggestPiece = EMP;
        while(map.hasBits())
        {
          loc_t loc = map.nextBit();
          preventedMap.setOff(priorLocation[loc]);
          if(bAfter.pieces[loc] > biggestPiece)
          {
            biggestLoc = loc;
            biggestPiece = bAfter.pieces[loc];
          }
        }

        //If it wasn't already been threatened, it's an invitiation to capture
        if(!data.oppCapMap.isOne(priorLocation[biggestLoc]))
        {
          int isOppTrap = !Board::ISPLATRAP[i][pla];

          //Was not moved
          if(priorLocation[biggestLoc] == biggestLoc)
            (*handleFeature)(fset.get(INVITES_CAP_UNMOVED,data.pieceIndex[pla][biggestPiece],capDist,isOppTrap),handleFeatureData);
          else
            (*handleFeature)(fset.get(INVITES_CAP_MOVED,data.pieceIndex[pla][biggestPiece],capDist,isOppTrap),handleFeatureData);
        }
      }
    }
    for(int i = 0; i<numChanges; i++)
    {
      if(dest[i] == ERRLOC)
        preventedMap.setOff(src[i]);
    }
    //Did we successfully prevent any captures? Add them all in!
    while(preventedMap.hasBits())
    {
      loc_t loc = preventedMap.nextBit();
      (*handleFeature)(fset.get(PREVENTS_CAP,data.pieceIndex[pla][b.pieces[loc]],Board::SYMLOC[pla][loc]),handleFeatureData);
    }
  }

  if(ENABLE_CAP_DEF)
  {
    loc_t plaEleLoc = bAfter.findElephant(pla);
    for(int j = 0; j<data.numOppCapThreats; j++)
    {
      int dist = data.oppCapThreats[j].dist;
      if(dist > 4)
        continue;
      loc_t kt = data.oppCapThreats[j].kt;
      int trapIndex = Board::TRAPINDEX[kt];
      int isOppTrap = !Board::ISPLATRAP[trapIndex][pla];

      if(plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt))
        (*handleFeature)(fset.get(CAP_DEF_ELEDEF,isOppTrap,dist),handleFeatureData);
      else if(bAfter.trapGuardCounts[pla][trapIndex] > b.trapGuardCounts[pla][trapIndex])
        (*handleFeature)(fset.get(CAP_DEF_TRAPDEF,isOppTrap,dist),handleFeatureData);
      else
      {
        for(int i = 0; i<numChanges; i++)
        {
          if(src[i] == data.oppCapThreats[j].oloc && dest[i] != ERRLOC)
            (*handleFeature)(fset.get(CAP_DEF_RUNAWAY,isOppTrap,dist),handleFeatureData);
          else if(data.oppCapThreats[j].ploc != ERRLOC && src[i] == data.oppCapThreats[j].ploc && dest[i] != ERRLOC)
            (*handleFeature)(fset.get(CAP_DEF_INTERFERE,isOppTrap,dist),handleFeatureData);
          else if(data.oppCapThreats[j].ploc != ERRLOC && b.isThawed(data.oppCapThreats[j].ploc) && bAfter.isFrozen(data.oppCapThreats[j].ploc))
            (*handleFeature)(fset.get(CAP_DEF_INTERFERE,isOppTrap,dist),handleFeatureData);
        }
      }
    }
  }

  if(ENABLE_FREEZE)
  {
    for(int isPla = 0; isPla <= 1; isPla++)
    {
      pla_t p = isPla ? pla : opp;
      Bitmap wasFrozen = b.pieceMaps[p][0] & b.frozenMap;
      Bitmap nowFrozen = bAfter.pieceMaps[p][0] & bAfter.frozenMap;
      Bitmap changed = wasFrozen ^ nowFrozen;
      while(changed.hasBits())
      {
        loc_t loc = changed.nextBit();
        if(wasFrozen.isOne(loc))
        {
          (*handleFeature)(fset.get(THAWS_STR,isPla,data.pieceIndex[opp][b.pieces[loc]]),handleFeatureData);
          (*handleFeature)(fset.get(THAWS_AT,isPla,Board::SYMLOC[opp][loc]),handleFeatureData);
        }
        else
        {
          (*handleFeature)(fset.get(FREEZES_STR,isPla,data.pieceIndex[opp][bAfter.pieces[loc]]),handleFeatureData);
          (*handleFeature)(fset.get(FREEZES_AT,isPla,Board::SYMLOC[opp][loc]),handleFeatureData);
        }
      }
    }
  }

  if(ENABLE_DOMINATE)
  {
    for(int isPla = 0; isPla <= 1; isPla++)
    {
      pla_t p = isPla ? pla : opp;
      Bitmap wasDom = b.pieceMaps[p][0] & b.dominatedMap;
      Bitmap nowDom = bAfter.pieceMaps[p][0] & bAfter.dominatedMap;
      Bitmap changed = wasDom ^ nowDom;
      while(changed.hasBits())
      {
        loc_t loc = changed.nextBit();
        if(wasDom.isOne(loc))
        {
          (*handleFeature)(fset.get(UNDOMS_STR,isPla,data.pieceIndex[opp][b.pieces[loc]]),handleFeatureData);
          (*handleFeature)(fset.get(UNDOMS_AT,isPla,Board::SYMLOC[opp][loc]),handleFeatureData);
        }
        else
        {
          (*handleFeature)(fset.get(DOMS_STR,isPla,data.pieceIndex[opp][bAfter.pieces[loc]]),handleFeatureData);
          (*handleFeature)(fset.get(DOMS_AT,isPla,Board::SYMLOC[opp][loc]),handleFeatureData);
        }
      }
    }
  }

  if(ENABLE_PHALANX)
  {
    //Phalanx can only be released if it was within radius 3 of a space we affected
    Bitmap rad3AffectMap;
    for(int i = 0; i<numChanges; i++)
    {
      rad3AffectMap |= Board::DISK[3][src[i]];
      if(dest[i] != ERRLOC)
        rad3AffectMap |= Board::DISK[3][dest[i]];
    }
    Bitmap oppMapR3D = b.pieceMaps[opp][0] & bAfter.pieceMaps[opp][0] & rad3AffectMap;
    while(oppMapR3D.hasBits())
    {
      loc_t oloc = oppMapR3D.nextBit();
      for(int dir = 0; dir < 4; dir++)
      {
        if(!Board::ADJOKAY[dir][oloc])
          continue;
        loc_t adj = oloc + Board::ADJOFFSETS[dir];
        if(ArimaaFeature::isSinglePhalanx(b,pla,oloc,adj) && !ArimaaFeature::isSinglePhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(RELEASES_PHALANX_VS,data.pieceIndex[opp][b.pieces[oloc]],1),handleFeatureData);
          (*handleFeature)(fset.get(RELEASES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],1),handleFeatureData);
        }
        else if(ArimaaFeature::isMultiPhalanx(b,pla,oloc,adj) && !ArimaaFeature::isMultiPhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(RELEASES_PHALANX_VS,data.pieceIndex[opp][b.pieces[oloc]],0),handleFeatureData);
          (*handleFeature)(fset.get(RELEASES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],0),handleFeatureData);
        }
      }
    }

    Bitmap oppMapR3C = bAfter.pieceMaps[opp][0] & rad3AffectMap;
    while(oppMapR3C.hasBits())
    {
      loc_t oloc = oppMapR3C.nextBit();
      for(int dir = 0; dir < 4; dir++)
      {
        if(!Board::ADJOKAY[dir][oloc])
          continue;
        loc_t adj = oloc + Board::ADJOFFSETS[dir];
        if(!ArimaaFeature::isSinglePhalanx(b,pla,oloc,adj) && ArimaaFeature::isSinglePhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(CREATES_PHALANX_VS,data.pieceIndex[opp][bAfter.pieces[oloc]],1),handleFeatureData);
          (*handleFeature)(fset.get(CREATES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],1),handleFeatureData);
        }
        else if(!ArimaaFeature::isMultiPhalanx(b,pla,oloc,adj) && ArimaaFeature::isMultiPhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(CREATES_PHALANX_VS,data.pieceIndex[opp][bAfter.pieces[oloc]],0),handleFeatureData);
          (*handleFeature)(fset.get(CREATES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],0),handleFeatureData);
        }
      }
    }
  }

  if(ENABLE_SIMPLE_PHALANX)
  {
    //Phalanx can only be released if it was within radius 2 of a space we affected
    Bitmap src2Map;
    Bitmap dest2Map;
    for(int i = 0; i<numChanges; i++)
    {
      src2Map |= Board::DISK[2][src[i]];
      if(dest[i] != ERRLOC)
        dest2Map |= Board::DISK[2][dest[i]];
    }
    //Released phalanxes
    Bitmap oppMapS2 = b.pieceMaps[opp][0] & bAfter.pieceMaps[opp][0] & src2Map;
    while(oppMapS2.hasBits())
    {
      loc_t oloc = oppMapS2.nextBit();
      for(int dir = 0; dir < 4; dir++)
      {
        if(!Board::ADJOKAY[dir][oloc])
          continue;
        loc_t adj = oloc + Board::ADJOFFSETS[dir];
        if(ArimaaFeature::isSimpleSinglePhalanx(b,pla,oloc,adj) && !ArimaaFeature::isSimpleSinglePhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(RELEASES_PHALANX_VS,data.pieceIndex[opp][b.pieces[oloc]],1),handleFeatureData);
          (*handleFeature)(fset.get(RELEASES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],1),handleFeatureData);
        }
        else if(ArimaaFeature::isSimpleMultiPhalanx(b,pla,oloc,adj) && !ArimaaFeature::isSimpleMultiPhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(RELEASES_PHALANX_VS,data.pieceIndex[opp][b.pieces[oloc]],0),handleFeatureData);
          (*handleFeature)(fset.get(RELEASES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],0),handleFeatureData);
        }
      }
    }

    Bitmap oppMapD2 = bAfter.pieceMaps[opp][0] & dest2Map;
    while(oppMapD2.hasBits())
    {
      loc_t oloc = oppMapD2.nextBit();
      for(int dir = 0; dir < 4; dir++)
      {
        if(!Board::ADJOKAY[dir][oloc])
          continue;
        loc_t adj = oloc + Board::ADJOFFSETS[dir];
        if(!ArimaaFeature::isSimpleSinglePhalanx(b,pla,oloc,adj) && ArimaaFeature::isSimpleSinglePhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(CREATES_PHALANX_VS,data.pieceIndex[opp][bAfter.pieces[oloc]],1),handleFeatureData);
          (*handleFeature)(fset.get(CREATES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],1),handleFeatureData);
        }
        else if(!ArimaaFeature::isSimpleMultiPhalanx(b,pla,oloc,adj) && ArimaaFeature::isSimpleMultiPhalanx(bAfter,pla,oloc,adj))
        {
          (*handleFeature)(fset.get(CREATES_PHALANX_VS,data.pieceIndex[opp][bAfter.pieces[oloc]],0),handleFeatureData);
          (*handleFeature)(fset.get(CREATES_PHALANX_AT,Board::SYMLOC[pla][oloc],Board::SYMDIR[opp][3-dir],0),handleFeatureData);
        }
      }
    }
  }

  /*
  Bitmap moved;
  for(int i = 0; i<numChanges; i++)
  {
    pla_t owner = b.owners[src[i]];
    piece_t piece = b.pieces[src[i]];
    int pieceIndex = data.pieceIndex[owner][piece];
    moved.setOn(src[i]);

    if(owner == pla && dest[i] != ERRLOC)
    {
      if(ENABLE_CAPTHREATENED)
      {
        if(data.plaCapThreatenedPlaTrap.isOne(src[i]))
        {
          bool isPlaTrap = true;
          (*handleFeature)(fset.get(MOVES_CAPTHREATENED_STR,isPlaTrap,pieceIndex),handleFeatureData);
          (*handleFeature)(fset.get(MOVES_CAPTHREATENED_AT,isPlaTrap,Board::SYMLOC[pla][src[i]]),handleFeatureData);
        }
        if(data.plaCapThreatenedOppTrap.isOne(src[i]))
        {
          bool isPlaTrap = false;
          (*handleFeature)(fset.get(MOVES_CAPTHREATENED_STR,isPlaTrap,pieceIndex),handleFeatureData);
          (*handleFeature)(fset.get(MOVES_CAPTHREATENED_AT,isPlaTrap,Board::SYMLOC[pla][src[i]]),handleFeatureData);
        }
        loc_t adjTrap = Board::ADJACENTTRAP[dest[i]];
        if(adjTrap != ERRLOC)
        {
          int trapIndex = Board::TRAPINDEX[adjTrap];
          if(data.isTrapCapThreatened[trapIndex])
          {
            bool isPlaTrap = Board::ISPLATRAP[trapIndex][pla];
            int defCount = newTrapGuardCounts[pla][trapIndex];
            (*handleFeature)(fset.get(DEFENDS_CAPTHREATED_TRAP_STR,isPlaTrap,defCount,data.oppTrapState[trapIndex],pieceIndex),handleFeatureData);
            (*handleFeature)(fset.get(DEFENDS_CAPTHREATED_TRAP_AT,isPlaTrap,Board::SYMLOC[pla][dest[i]]),handleFeatureData);
          }
        }
      }

      if(ENABLE_LIKELY_CAPTHREAT)
      {
        loc_t adjTrap = Board::ADJACENTTRAP[dest[i]];
        if(adjTrap != ERRLOC)
        {
          int trapIndex = Board::TRAPINDEX[adjTrap];
          if(newTrapGuardCounts[pla][trapIndex] == 1 && piece <= data.likelyDangerAdjTrapStr[dest[i]])
            (*handleFeature)(fset.get(LIKELY_CAPDANGER_ADJ,Board::ISPLATRAP[trapIndex][pla]),handleFeatureData);
        }
        loc_t adjTrap2 = Board::RAD2TRAP[dest[i]];
        if(adjTrap2 != ERRLOC)
        {
          int trapIndex2 = Board::TRAPINDEX[adjTrap2];
          if(newTrapGuardCounts[pla][trapIndex2] == 0 && piece <= data.likelyDangerAdj2TrapStr[dest[i]])
            (*handleFeature)(fset.get(LIKELY_CAPDANGER_ADJ2,Board::ISPLATRAP[trapIndex2][pla]),handleFeatureData);
        }

        if(piece >= data.likelyThreatStr[dest[i]])
          (*handleFeature)(fset.get(LIKELY_CAPTHREAT_STATIC),handleFeatureData);
        else if(piece >= data.likelyLooseThreatStr[dest[i]])
          (*handleFeature)(fset.get(LIKELY_CAPTHREAT_LOOSE),handleFeatureData);
      }



    }

    else if(owner == opp && dest[i] != ERRLOC)
    {
      if(ENABLE_LIKELY_CAPTHREAT)
      {
        if(data.likelyThreatPushPullLocs.isOne(dest[i]))
          (*handleFeature)(fset.get(LIKELY_CAPTHREAT_PUSHPULL),handleFeatureData);
        else if(data.likelyThreatPushPullLocsLoose.isOne(dest[i]))
          (*handleFeature)(fset.get(LIKELY_CAPTHREAT_PUSHPULL_LOOSE),handleFeatureData);
      }
    }
  }

  if(ENABLE_LIKELY_CAPTHREAT)
  {
    for(int trapIndex = 0; trapIndex < 4; trapIndex++)
    {
      if(data.trapNeedsMoreDefs[trapIndex] && newTrapGuardCounts[pla][trapIndex] > b.trapGuardCounts[pla][trapIndex])
        (*handleFeature)(fset.get(LIKELY_CAPTRAPDEF,Board::ISPLATRAP[trapIndex][pla]),handleFeatureData);
      else if(!data.trapNeedsMoreDefs[trapIndex] && newTrapGuardCounts[pla][trapIndex] < b.trapGuardCounts[pla][trapIndex] &&
              newTrapGuardCounts[pla][trapIndex] < data.minDefendersToBeLikelySafe[trapIndex])
      {
        loc_t kt = Board::TRAPLOCS[trapIndex];
        bool foundHanging = false;

        if(newTrapGuardCounts[pla][trapIndex] == 1)
        {
          for(int dir = 0; dir < 4; dir++)
          {
            loc_t loc = kt + Board::ADJOFFSETS[dir];
            if(b.owners[loc] == pla && !moved.isOne(loc) && b.pieces[loc] <= data.likelyDangerAdjTrapStr[loc])
            {foundHanging = true; break;}
          }
        }
        else if(newTrapGuardCounts[pla][trapIndex] == 0)
        {
          for(int i = 5; i<13; i++)
          {
            loc_t loc = Board::SPIRAL[kt][i];
            if(b.owners[loc] == pla && !moved.isOne(loc) && b.pieces[loc] <= data.likelyDangerAdj2TrapStr[loc])
            {foundHanging = true; break;}
          }
        }

        if(foundHanging)
          (*handleFeature)(fset.get(LIKELY_CAPHANG,Board::ISPLATRAP[trapIndex][pla]),handleFeatureData);
      }
    }
  }
  */

  if(ENABLE_DEPENDENCY)
  {
    int numCombos = 0;
    Bitmap comboSrcs[4];
    Bitmap comboDests[4];
    int comboStepCount[4];

    for(int stepIdx = 0; stepIdx<4; stepIdx++)
    {
      step_t s = getStep(move,stepIdx);
      if(s == PASSSTEP || s == ERRSTEP)
        break;
      loc_t src = gSrc(s);
      loc_t dest = gDest(s);

      //Accumulate into combos by the T1 condition:
      //  Src of one is within radius 1 of the dest of the other
      int matchingCombo = -1;
      Bitmap srcD1 = Board::DISK[1][src];
      Bitmap destD1 = Board::DISK[1][dest];
      int comboIdx;
      for(comboIdx = 0; comboIdx<numCombos; comboIdx++)
      {
        if((srcD1 & comboDests[comboIdx]).hasBits() ||
           (destD1 & comboSrcs[comboIdx]).hasBits())
        {
          matchingCombo = comboIdx;
          comboIdx++;
          break;
        }
      }

      //No match, so add a new combo
      if(matchingCombo == -1)
      {
        comboSrcs[numCombos].setOn(src);
        comboDests[numCombos].setOn(dest);
        comboStepCount[numCombos] = 1;
        numCombos++;
        continue;
      }

      //Otherwise, we have a match. Keep iterating to see if anything else needs to be merged
      int newNumCombos = comboIdx;
      for(; comboIdx<numCombos; comboIdx++)
      {
        if((srcD1 & comboDests[comboIdx]).hasBits() ||
           (destD1 & comboSrcs[comboIdx]).hasBits())
        {
          //Another match, merge it into the first match
          comboSrcs[matchingCombo] |= comboSrcs[comboIdx];
          comboDests[matchingCombo] |= comboDests[comboIdx];
          comboStepCount[matchingCombo] += comboStepCount[comboIdx];
        }
        else
        {
          //No match, so simply copy it downward
          comboSrcs[newNumCombos] = comboSrcs[comboIdx];
          comboDests[newNumCombos] = comboDests[comboIdx];
          comboStepCount[newNumCombos] = comboStepCount[comboIdx];
          newNumCombos++;
        }
      }
      numCombos = newNumCombos;

      //And finally accumulate the new step as well
      comboSrcs[matchingCombo].setOn(src);
      comboDests[matchingCombo].setOn(dest);
      comboStepCount[matchingCombo]++;
      continue;
    }

    //Find all squares adjacent to any accumulated changes
    Bitmap adjSrcChange;
    for(int i = 0; i<numChanges; i++)
      adjSrcChange.setOn(src[i]);
    adjSrcChange |= Bitmap::adj(adjSrcChange);

    //Filter out all combos whose srcs are not adjacent to any such change
    //This tries to eliminate obvious useless step-unstep combos that affect nothing
    //and are equivalent to passing those steps, although it might not eliminate all such cases.
    //In the process, we also transform comboSrcs to be comboSrcDests, and count total number of steps
    int newNumCombos = 0;
    int numTotal = 0;
    for(int comboIdx = 0; comboIdx<numCombos; comboIdx++)
    {
      if((comboSrcs[comboIdx] & adjSrcChange).isEmpty())
        continue;
      numTotal += comboStepCount[comboIdx];
      comboSrcs[newNumCombos] = comboSrcs[comboIdx] | comboDests[comboIdx];
      newNumCombos++;
    }
    numCombos = newNumCombos;

    int numIndepT1 = numCombos;
    numCombos = 0; //Now counts number of T2 combos

    //Now fold over srcs and dests into srcdests and accumulate for T2:
    //  Srcdests are adjacent to srcdests
    //  Srcdests are adjacent to the same trap
    for(int outerIdx = 0; outerIdx<numIndepT1; outerIdx++)
    {
      Bitmap merged = comboSrcs[outerIdx];

      //Accumulate into combos by the T2 condition:
      int matchingCombo = -1;
      Bitmap expanded = Bitmap::adj(merged);
      expanded |= Bitmap::adj(expanded & Bitmap::BMPTRAPS);

      int comboIdx;
      for(comboIdx = 0; comboIdx<numCombos; comboIdx++)
      {
        if((expanded & comboSrcs[comboIdx]).hasBits())
        {
          matchingCombo = comboIdx;
          comboIdx++;
          break;
        }
      }

      //No match, so add a new combo
      if(matchingCombo == -1)
      {
        comboSrcs[numCombos] = merged;
        numCombos++;
        continue;
      }

      //Otherwise, we have a match. Keep iterating to see if anything else needs to be merged
      int newNumCombos = comboIdx;
      for(; comboIdx<numCombos; comboIdx++)
      {
        if((expanded & comboSrcs[comboIdx]).hasBits())
        {
          //Another match, merge it into the first match
          comboSrcs[matchingCombo] |= comboSrcs[comboIdx];
        }
        else
        {
          //No match, so simply copy it downward
          comboSrcs[newNumCombos] = comboSrcs[comboIdx];
          newNumCombos++;
        }
      }
      numCombos = newNumCombos;

      //And finally accumulate the new combo as well
      comboSrcs[matchingCombo] |= merged;
      continue;
    }

    int numIndepT2 = numCombos;
    numCombos = 0; //Now counts number of T3 combos

    //Now fold over srcs and dests into srcdests and accumulate for T3:
    //  Srcdests are within radius 2 of srcdests
    for(int outerIdx = 0; outerIdx<numIndepT1; outerIdx++)
    {
      Bitmap merged = comboSrcs[outerIdx];

      //Accumulate into combos by the T3 condition:
      int matchingCombo = -1;
      Bitmap expanded = Bitmap::adj(merged);
      expanded |= Bitmap::adj(expanded);

      int comboIdx;
      for(comboIdx = 0; comboIdx<numCombos; comboIdx++)
      {
        if((expanded & comboSrcs[comboIdx]).hasBits())
        {
          matchingCombo = comboIdx;
          comboIdx++;
          break;
        }
      }

      //No match, so add a new combo
      if(matchingCombo == -1)
      {
        comboSrcs[numCombos] = merged;
        numCombos++;
        continue;
      }

      //Otherwise, we have a match. Keep iterating to see if anything else needs to be merged
      int newNumCombos = comboIdx;
      for(; comboIdx<numCombos; comboIdx++)
      {
        if((expanded & comboSrcs[comboIdx]).hasBits())
        {
          //Another match, merge it into the first match
          comboSrcs[matchingCombo] |= comboSrcs[comboIdx];
        }
        else
        {
          //No match, so simply copy it downward
          comboSrcs[newNumCombos] = comboSrcs[comboIdx];
          newNumCombos++;
        }
      }
      numCombos = newNumCombos;

      //And finally accumulate the new combo as well
      comboSrcs[matchingCombo] |= merged;
      continue;
    }

    int numIndepT3 = numCombos;

    DEBUGASSERT(numTotal > 0 && numTotal <= 4);
    (*handleFeature)(fset.get(STEP_INDEPENDENCE,numTotal-1,numIndepT1-1,numIndepT2-1,numIndepT3-1),handleFeatureData);
  }

  if(ENABLE_GOAL_THREAT && data.forTraining)
  {
    if(bAfter.getWinner() == pla)
      (*handleFeature)(fset.get(WINS_GAME),handleFeatureData);
    else
    {
      int newOppGoalDist = BoardTrees::goalDist(bAfter,opp,4);
      if(newOppGoalDist <= 4)
        (*handleFeature)(fset.get(ALLOWS_GOAL),handleFeatureData);
    }
  }

  if(ENABLE_REAL_GOAL_THREAT)
  {
    int newPlaGoalDist = BoardTrees::goalDist(bAfter,pla,4);
    if(newPlaGoalDist < data.plaGoalDist && newPlaGoalDist <= 4 && newPlaGoalDist > 0)
      (*handleFeature)(fset.get(THREATENS_GOAL,newPlaGoalDist),handleFeatureData);
  }


}

static void* getPosDataHelper(Board& b, const BoardHistory& hist, pla_t pla, bool forTraining)
{
  (void)hist;
  MoveFeatureLitePosData* dataBuf = new MoveFeatureLitePosData();
  MoveFeatureLitePosData& data = *dataBuf;

  data.forTraining = forTraining;
  data.board = b;
  pla_t opp = gOpp(pla);

  b.initializeStronger(data.pStronger);
  b.initializeStrongerMaps(data.pStrongerMaps);

  for(piece_t piece = RAB; piece <= ELE; piece++)
  {
    data.pieceIndex[GOLD][piece] = ArimaaFeature::getPieceIndexApprox(GOLD,piece,data.pStronger);
    data.pieceIndex[SILV][piece] = ArimaaFeature::getPieceIndexApprox(SILV,piece,data.pStronger);
  }

  /*
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      int i = gLoc(x,y);
      data.oppMaxStrAdj[i] = EMP;
      data.oppMinStrAdj[i] = ELE+1;
    }
  }

  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      int i = gLoc(x,y);
      if(b.owners[i] != opp)
        continue;

      int str = b.pieces[i];
      if(CS1(i) && data.oppMaxStrAdj[i S] < str) {data.oppMaxStrAdj[i S] = str;}
      if(CW1(i) && data.oppMaxStrAdj[i W] < str) {data.oppMaxStrAdj[i W] = str;}
      if(CE1(i) && data.oppMaxStrAdj[i E] < str) {data.oppMaxStrAdj[i E] = str;}
      if(CN1(i) && data.oppMaxStrAdj[i N] < str) {data.oppMaxStrAdj[i N] = str;}

      if(CS1(i) && data.oppMinStrAdj[i S] > str) {data.oppMinStrAdj[i S] = str;}
      if(CW1(i) && data.oppMinStrAdj[i W] > str) {data.oppMinStrAdj[i W] = str;}
      if(CE1(i) && data.oppMinStrAdj[i E] > str) {data.oppMinStrAdj[i E] = str;}
      if(CN1(i) && data.oppMinStrAdj[i N] > str) {data.oppMinStrAdj[i N] = str;}
    }
  }
  */

  data.plaFrozen = b.frozenMap & b.pieceMaps[pla][0];

  data.plaCapThreatenedPlaTrap = Bitmap();
  data.plaCapThreatenedOppTrap = Bitmap();
  for(int i = 0; i<4; i++)
  {
    data.isTrapCapThreatened[i] = false;

    int capDist;
    Bitmap map;
    loc_t kt = Board::TRAPLOCS[i];
    if(BoardTrees::canCaps(b,opp,4,kt,map,capDist))
    {
      data.isTrapCapThreatened[i] = true;
      if(Board::ISPLATRAP[i][pla])
        data.plaCapThreatenedPlaTrap |= map;
      else
        data.plaCapThreatenedOppTrap |= map;
    }
  }

  data.goalThreatened = BoardTrees::goalDist(b,opp,4) <= 4;

  if(ENABLE_CAP_DEF)
  {
    UFDist::get(b,data.ufDist,data.pStrongerMaps);

    data.numOppCapThreats = 0;
    int oppRDSteps[4] = {16,16,16,16};
    for(int j = 0; j<4; j++)
    {
      loc_t kt = Board::TRAPLOCS[j];
      data.numOppCapThreats += Threats::findCapThreats(b,opp,kt,data.oppCapThreats+data.numOppCapThreats,data.ufDist,data.pStrongerMaps,
          4,Threats::maxCapsPerPla-data.numOppCapThreats,oppRDSteps[j]);
    }
  }

  if(ENABLE_STRATS)
  {
    for(int i = 0; i<4; i++)
    {
      FrameThreat ft;
      data.oppHoldsFrameAtTrap[i] = false;

      loc_t trapLoc = Board::TRAPLOCS[i];
      if(Strats::findFrame(b,gOpp(pla),trapLoc,&ft))
      {
        data.oppHoldsFrameAtTrap[i] = true;
        data.oppFrameIsPartial[i] = ft.isPartial;
        data.oppFrameIsEleEle[i] =
            (Board::RADIUS[1][trapLoc] & b.pieceMaps[0][ELE]).hasBits() &&
            (Board::RADIUS[1][trapLoc] & b.pieceMaps[1][ELE]).hasBits();
      }
    }
  }

  if(ENABLE_TRAPSTATE)
  {
    for(int i = 0; i<4; i++)
    {
      data.oldTrapStates[SILV][i] = ArimaaFeature::getTrapStateExtended(b,SILV,Board::TRAPLOCS[i]);
      data.oldTrapStates[GOLD][i] = ArimaaFeature::getTrapStateExtended(b,GOLD,Board::TRAPLOCS[i]);
    }
  }

  if(ENABLE_CAP_THREAT)
  {
    data.plaCapMap = Bitmap();
    data.oppCapMap = Bitmap();
    for(int i = 0; i<4; i++)
    {
      int capDist;
      BoardTrees::canCaps(b,pla,4,Board::TRAPLOCS[i],data.plaCapMap,capDist);
      BoardTrees::canCaps(b,opp,4,Board::TRAPLOCS[i],data.oppCapMap,capDist);
    }
  }

  if(ENABLE_REAL_GOAL_THREAT)
  {
    data.plaGoalDist = BoardTrees::goalDist(b,pla,4);
  }

  //Scale to 0-8 range
  if(ENABLE_INFLUENCE)
  {
    Eval::getInfluence(b, data.pStronger, data.influence);
    for(int y = 0; y<8; y++)
    {
      for(int x = 0; x<8; x++)
      {
        int i = gLoc(x,y);
        data.influence[0][i] /= 50;
        data.influence[0][i] += 4;
        if(data.influence[0][i] < 0)
          data.influence[0][i] = 0;
        else if(data.influence[0][i] > 8)
          data.influence[0][i] = 8;

        data.influence[1][i] /= 50;
        data.influence[1][i] += 4;
        if(data.influence[1][i] < 0)
          data.influence[1][i] = 0;
        else if(data.influence[1][i] > 8)
          data.influence[1][i] = 8;
      }
    }
  }

  /*
  Bitmap pStrongerMaps[2][NUMTYPES];
  b.initializeStrongerMaps(pStrongerMaps);

  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      int i = gLoc(x,y);
      data.likelyDangerAdjTrapStr[i] = EMP;
      data.likelyDangerAdj2TrapStr[i] = EMP;
    }
  }

  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    loc_t kt = Board::TRAPLOCS[trapIndex];
    bool haveDangerPieceRad1 = false;
    for(int dir = 0; dir < 4; dir++)
    {
      loc_t loc = kt + Board::ADJOFFSETS[dir];
      data.likelyDangerAdjTrapStr[loc] = EMP;

      //If we are the only defender, then we are likely threatened with capture in the adjacent trap if
      //there is a stronger piece within radius 3 of us unfrozen, or a stronger piece within radius 2 of us at all
      //So find the strongest piece in such a set
      Bitmap relevantSet = Board::RADIUS[2][loc] | (Board::RADIUS[3][loc] & ~b.frozenMap);
      for(piece_t piece = ELE; piece >= CAT; piece--)
      {
        if((pStrongerMaps[pla][piece-1] & relevantSet).hasBits())
        {
          data.likelyDangerAdjTrapStr[loc] = piece-1;

          //If we already have a piece here, then this means that we are in trouble if the number of defenders
          //goes to 1
          if(b.owners[loc] == pla && b.pieces[loc] <= piece-1)
            haveDangerPieceRad1 = true;

          break;
        }
      }
    }

    //Radius 2 squares around trap
    bool haveDangerPieceRad2 = false;
    for(int i = 5; i<13; i++)
    {
      loc_t loc = Board::SPIRAL[kt][i];
      //If there are no trap defenders, then we are likely threatened with capture only if there is an adjacent
      //unfrozen piece
      Bitmap relevantSet = Board::RADIUS[1][loc] & ~b.frozenMap;
      for(piece_t piece = ELE; piece >= CAT; piece--)
      {
        if((pStrongerMaps[pla][piece-1] & relevantSet).hasBits())
        {
          data.likelyDangerAdj2TrapStr[loc] = piece-1;

          //If we already have a piece here, then this means that we are in trouble if the number of defenders
          //goes to zero
          if(b.owners[loc] == pla && b.pieces[loc] <= piece-1)
            haveDangerPieceRad2 = true;

          break;
        }
      }
    }

    if(haveDangerPieceRad1)
      data.minDefendersToBeLikelySafe[trapIndex] = 2;
    else if(haveDangerPieceRad2)
      data.minDefendersToBeLikelySafe[trapIndex] = 1;
    else
      data.minDefendersToBeLikelySafe[trapIndex] = 0;
  }

  for(int i = 0; i<4; i++)
    data.trapNeedsMoreDefs[i] = data.isTrapCapThreatened[i];

  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      int i = gLoc(x,y);
      data.likelyThreatStr[i] = ELE+1;
      data.likelyLooseThreatStr[i] = ELE+1;
    }
  }

  data.likelyThreatPushPullLocs = Bitmap();
  data.likelyThreatPushPullLocsLoose = Bitmap();

  for(int trapIndex = 0; trapIndex<4; trapIndex++)
  {
    loc_t kt = Board::TRAPLOCS[trapIndex];

    //Relevant pieces to threaten are those within radius 2
    if(b.trapGuardCounts[opp][trapIndex] == 0)
    {
      //While we're at it, if we pushpull an opp adjacent to an undefended trap, it's a likely threat
      data.likelyThreatPushPullLocs |= Board::RADIUS[1][kt];

      //And it's a loose threat if we do it at a farther radius
      data.likelyThreatPushPullLocsLoose |= Board::RADIUS[2][kt];

      Bitmap oppMap = b.pieceMaps[opp][0] & Board::RADIUS[2][kt];
      while(oppMap.hasBits())
      {
        loc_t oloc = oppMap.nextBit();
        piece_t neededStr = b.pieces[oloc] + 1;
        for(int dir = 0; dir<4; dir++)
        {
          if(!Board::ADJOKAY[dir][oloc])
            continue;
          loc_t adj = oloc + Board::ADJOFFSETS[dir];
          if(neededStr < data.likelyThreatStr[adj])
            data.likelyThreatStr[adj] = neededStr;
        }
      }
    }

    //Relevant pieces to threaten are the single defenders
    else if(b.trapGuardCounts[opp][trapIndex] == 1)
    {
      loc_t oloc = b.findGuard(opp,kt);
      piece_t neededStr = b.pieces[oloc] + 1;
      //Let's say that we need to be within radius 1 to really be threatening
      //Even though radius 3 would sometimes suffice
      for(int dir = 0; dir<4; dir++)
      {
        loc_t adj = oloc + Board::ADJOFFSETS[dir];
        if(neededStr < data.likelyThreatStr[adj])
          data.likelyThreatStr[adj] = neededStr;
      }

      //But radius 2 would threaten loosely
      for(int i = 5; i<13; i++)
      {
        loc_t adj = Board::SPIRAL[oloc][i];
        if(neededStr < data.likelyLooseThreatStr[adj])
          data.likelyLooseThreatStr[adj] = neededStr;
      }
    }

    //Relevant pieces to threaten are defenders
    //To do so, we need to actually put a piece greater than both strengths on one of the
    //defenders current squares (indicating that we pushed one of them).
    else if(b.trapGuardCounts[opp][trapIndex] == 2)
    {
      int numDefenderLocs = 0;
      loc_t defenderLocs[2];
      for(int dir = 0; dir<4; dir++)
      {
        loc_t adj = kt + Board::ADJOFFSETS[dir];
        if(b.owners[adj] == opp)
          defenderLocs[numDefenderLocs++] = adj;
      }

      piece_t maxNeededStr;
      if(b.pieces[defenderLocs[0]] < b.pieces[defenderLocs[1]])
        maxNeededStr = b.pieces[defenderLocs[1]];
      else
        maxNeededStr = b.pieces[defenderLocs[0]];

      //If the other piece is dominated, then putting a piece here at all is a strong threat
      if((Board::RADIUS[1][defenderLocs[1]] & pStrongerMaps[opp][b.pieces[defenderLocs[1]]]).hasBits())
        data.likelyThreatStr[defenderLocs[0]] = RAB;
      //Else if it has a radius 2 dominator, then it's a weak threat
      else if((Board::RADIUS[2][defenderLocs[1]] & pStrongerMaps[opp][b.pieces[defenderLocs[1]]]).hasBits())
        data.likelyLooseThreatStr[defenderLocs[0]] = RAB;
      //Else we can make a loose threat simply by putting a new piece here >= both defenders
      else if(maxNeededStr < data.likelyLooseThreatStr[defenderLocs[0]])
        data.likelyLooseThreatStr[defenderLocs[0]] = maxNeededStr;

      //Repeat for the other piece
      if((Board::RADIUS[1][defenderLocs[0]] & pStrongerMaps[opp][b.pieces[defenderLocs[0]]]).hasBits())
        data.likelyThreatStr[defenderLocs[1]] = RAB;
      else if((Board::RADIUS[2][defenderLocs[0]] & pStrongerMaps[opp][b.pieces[defenderLocs[0]]]).hasBits())
        data.likelyLooseThreatStr[defenderLocs[1]] = RAB;
      else if(maxNeededStr < data.likelyLooseThreatStr[defenderLocs[1]])
        data.likelyLooseThreatStr[defenderLocs[1]] = maxNeededStr;
    }
  }
  */

  /*
  if(ENABLE_RABGDIST)
  {
    Threats::getGoalDistEst(b,pla,data.rabbitGoalDist,9);
  }
  */
  return dataBuf;
}

static void* getPosData(Board& b, const BoardHistory& hist, pla_t pla)
{
  return getPosDataHelper(b,hist,pla,false);
}
static void* getPosDataForTraining(Board& b, const BoardHistory& hist, pla_t pla)
{
  return getPosDataHelper(b,hist,pla,true);
}

static void freePosData(void* dataBuf)
{
  MoveFeatureLitePosData* dataPtr = (MoveFeatureLitePosData*)(dataBuf);
  delete dataPtr;
}

static void getParallelWeights(const void* dataBuf, vector<double>& buf)
{
  (void)dataBuf;
  DEBUGASSERT(NUM_PARALLEL_WEIGHTS == 1);
  if(buf.size() != NUM_PARALLEL_WEIGHTS)
    buf.resize(NUM_PARALLEL_WEIGHTS);
  buf[0] = 1.0;
}

static bool doesCoeffMatterForPrior(findex_t feature)
{
  (void)feature;
  return false;
}

const FeatureSet& MoveFeatureLite::getFeatureSet()
{
  return fset;
}

ArimaaFeatureSet MoveFeatureLite::getArimaaFeatureSet()
{
  return ArimaaFeatureSet(&fset,getFeatures,getPosData,freePosData,
      NUM_PARALLEL_WEIGHTS,getParallelWeights,getPriorLogProb,doesCoeffMatterForPrior);
}
ArimaaFeatureSet MoveFeatureLite::getArimaaFeatureSetForTraining()
{
  return ArimaaFeatureSet(&fset,getFeatures,getPosDataForTraining,freePosData,
      NUM_PARALLEL_WEIGHTS,getParallelWeights,getPriorLogProb,doesCoeffMatterForPrior);
}

