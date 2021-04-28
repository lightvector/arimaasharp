
/*
 * featuremove.cpp
 * Author: davidwu
 */

#include <algorithm>
#include <set>
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../eval/internal.h"
#include "../eval/eval.h"
#include "../eval/threats.h"
#include "../learning/feature.h"
#include "../learning/featuremove.h"
#include "../learning/featurearimaa.h"
#include "../search/searchutils.h"
#include "../search/searchmovegen.h"

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

//Wins the game
static fgrpindex_t WINS_GAME;

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


//ACTUAL STEPS---------------------------------------------------------------------------------------
static const bool ENABLE_STEPS = false;

//Actual step made (pieceindex 0-7)(src * 4 + symdir 0-127)
static fgrpindex_t ACTUAL_STEP;
static fgrpindex_t ACTUAL_STEP_O;


static const bool ENABLE_CAPTURED = false;

//Piece captured (pieceindex 0-7)
static fgrpindex_t CAPTURED;
static fgrpindex_t CAPTURED_O;

//SPECIAL CASE FRAME SAC-------------------------------------------------------------------------------
static const bool ENABLE_STRATS = true;

//Sacrifices a framed pla piece. (pieceindex 0-7)(iseleele 0-1)
static fgrpindex_t FRAMESAC;
//Sacrifices a hostaged pla piece. (pieceindex 0-7)(iseleele 0-1)  MINOR NOT IMPLEMENTED
static fgrpindex_t HOSTAGESAC;
//After move, there is a pla framing a piece (pieceindex 0-7)(iseleele 0-1)
static fgrpindex_t FRAME_P;
//After move, there is an opp framing a piece (pieceindex 0-7)(iseleele 0-1)
static fgrpindex_t FRAME_O;
//After move, there is a hostage held by pla (pieceindex 0-7)(holderindex 0-7)  MINOR NOT IMPLEMENTED
static fgrpindex_t HOSTAGE_P;
//After move, there is a hostage held by opp (pieceindex 0-7)(holderindex 0-7)  MINOR NOT IMPLEMENTED
static fgrpindex_t HOSTAGE_O;
//Elephant blockade (centrality 0-6)(usesele 0-1)
static fgrpindex_t EBLOCKADE_CENT_P;
//Elephant blockade (centrality 0-6)(usesele 0-1)
static fgrpindex_t EBLOCKADE_CENT_O;
//Elephant blockade (gydist 0-7)(usesele 0-1)
static fgrpindex_t EBLOCKADE_GYDIST_P;
//Elephant blockade (gydist 0-7)(usesele 0-1)
static fgrpindex_t EBLOCKADE_GYDIST_O;

//TRAP STATE-----------------------------------------------------------------------------------------
static const bool ENABLE_TRAPSTATE = true;

//Changes the pla/opp trapstate at a trap. (isopptrap 0-1)(oldstate 0-7)(newstate 0-7)
static fgrpindex_t TRAP_STATE_P;
static fgrpindex_t TRAP_STATE_O;

//TRAP CONTROL---------------------------------------------------------------------------------------
static const bool ENABLE_TRAPCONTROL = true;

//Move results in the specified trap control for a trap (isopptrap 0-1)(tc 0-16 (/10 +8))
static fgrpindex_t TRAP_CONTROL;

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

//GOAL THREATENING--------------------------------------------------------------------------------------
static const bool ENABLE_GOAL_THREAT = true;
//Threatens goal when we couldn't goal previously. (threat 1-4) (defsteps 1-4 (-1))
static fgrpindex_t THREATENS_GOAL;
//Allows the opponent to goal
static fgrpindex_t ALLOWS_GOAL;

static const bool ENABLE_GOAL_DIST = true;
//Moves a rabbit from approx goal dist X to approx goal dist Y (gdistX 0-9) (gdistY 0-9)
static fgrpindex_t MOVES_RAB_GOAL_DIST;
//Moves pla pieces to or from location in front of advanced rabbit (from/to 0-1) (gdist 0-7) (rad 0-3)
static fgrpindex_t PLA_SRCDEST_NEAR_ADVANCED_PLA_RAB;
static fgrpindex_t PLA_SRCDEST_NEAR_ADVANCED_OPP_RAB;
//Moves opp pieces to or from location in front of advanced rabbit (from/to 0-1) (gdist 0-7) (rad 0-3)
static fgrpindex_t OPP_SRCDEST_NEAR_ADVANCED_PLA_RAB;
static fgrpindex_t OPP_SRCDEST_NEAR_ADVANCED_OPP_RAB;

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

//ADVANCEMENT AND INFLUENCE-----------------------------------------------------------------------------
static const bool ENABLE_INFLUENCE = true;

//Moves rabbit. (finaldistance 0-7) (influence 0-8 (+4))
static fgrpindex_t RABBIT_ADVANCE;
//Advances piece (pieceindex 0-7) (influencetrap 0-8 (+4))
static fgrpindex_t PIECE_ADVANCE;
//Retreats piece (pieceindex 0-7) (influencetrap 0-8 (+4))
static fgrpindex_t PIECE_RETREAT;

//Increases distance from nearest dominating piece (pieceindex 0-7) (influencesrc 0-8 (+4)) (rad 0-4)
static fgrpindex_t ESCAPE_DOMINATOR;
//Decreases distance from nearest dominating piece (pieceindex 0-7) (influencesrc 0-8 (+4)) (rad 0-4)
static fgrpindex_t APPROACH_DOMINATOR;
//Dominates a piece by stepping adjacent (pieceindex 0-7) (influencedom 0-8 (+4))
static fgrpindex_t DOMINATES_ADJ;

//Steps on to a trap with 2 or more defenders or the ele or not (isopptrap 0-1)(pieceindex 0-7)
static fgrpindex_t SAFE_STEP_ON_TRAP;
static fgrpindex_t UNSAFE_STEP_ON_TRAP;


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

//PLACEMENT AND BALANCE---------------------------------------------------------------------------------
static const bool ENABLE_BALANCE = true;

//Distance of elephant from camel or other pstronger1 after move (Eispla 0-1) (Mispla 0-1) (rad 1-7 (-1)) (weaker y 0-7)
static fgrpindex_t EM_DIST;
//Distance of elephant from horse or other pstronger2 after move (Eispla 0-1) (Hispla 0-1) (rad 1-7 (-1)) (weaker y 0-7)
static fgrpindex_t EH_DIST;
//Distance of camel from horse or other pstronger2 after move (Mispla 0-1) (Hispla 0-1) (rad 1-7 (-1)) (weaker y 0-7)
static fgrpindex_t MH_DIST;

//PHALANXES---------------------------------------------------------------------------------------------
static const bool ENABLE_PHALANX = true;

//This move creates a phalanx against a piece (pieceindex 0-7) (issingle 0-1)
static fgrpindex_t CREATES_PHALANX_VS;
//This move creates a phalanx against a piece at (symloc 0-31) (dir 0-2) (issingle 0-1)
static fgrpindex_t CREATES_PHALANX_AT;

//This move releases a phalanx against a piece (pieceindex 0-7) (issingle 0-1)
static fgrpindex_t RELEASES_PHALANX_VS;
//This move releases a phalanx against a piece at (symloc 0-31) (dir 0-2) (issingle 0-1)
static fgrpindex_t RELEASES_PHALANX_AT;

//BLOCKING UFS/ESCAPES----------------------------------------------------------------------------------
static const bool ENABLE_BLOCKING = false;

//Steps a piece adjacent to one frozen (pieceindex 0-7)(blockerweaker 0-1)(sidefrontback 0-3)
static fgrpindex_t BLOCKS_FROZEN;
//Steps a piece adjacent to one just pushed/pulled (pieceindex 0-7)(blockerweaker 0-1)(sidefrontback 0-3)
static fgrpindex_t BLOCKS_PUSHPULLED;
//Steps a piece adjacent to one under capthreat (pieceindex 0-7)(blockerweaker 0-1)(sidefrontback 0-3)
static fgrpindex_t BLOCKS_CAPTHREATED;

//REVERSIBLES and FREECAPS------------------------------------------------------------------------------
static const bool ENABLE_REVFREECAPS = true;

//4-2 reversible, 3-1 reversible, and pulls/pushes a piece that can immediately step back.
static fgrpindex_t IS_FULL_REVERSIBLE;
static fgrpindex_t IS_MOSTLY_REVERSIBLE;
static fgrpindex_t PUSHPULL_OPP_REVERSIBLE;

//Only-moved piece can immediately be captured for free, or only 1 step for other pieces, and taking different
//numbers of steps (see searchutils.h) (1-6 (-1))
static fgrpindex_t IS_FREE_CAPTURABLE;

//Remakes an earlier move in the same position that occured earlier, presumably as part of a rep fight
static fgrpindex_t REMAKES_EARLIER_MOVE;


//LAST MOVE-------------------------------------------------------------------------------------------
static const bool ENABLE_LAST = true;

//Moves a piece that the opponent just pushpulled
static fgrpindex_t MOVES_LAST_PUSHED;
//Moves near the opponent's last move (nearness 0-63)
static fgrpindex_t MOVES_NEAR_LAST;
//Moves near the pla's move his last turn (nearness 0-63)
static fgrpindex_t MOVES_NEAR_LAST_LAST;

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

//RELTACTICS------------------------------------------------------------------------------------
static const bool ENABLE_RELTACTICS = true;

//0 = reltactic, 1 = reltactic with an extra step
static fgrpindex_t RELTACTICSDIST;

//Helpers---------------------------------------------------------------------------------------
static void initPriors();
static int getSymDir(pla_t pla, loc_t loc, loc_t adj);

static bool IS_INITIALIZED = false;
void MoveFeature::initFeatureSet()
{
  if(IS_INITIALIZED)
    return;

  PRIOR = fset.add("Prior");
  PASS = fset.add("Pass");
  WINS_GAME = fset.add("WinsGame");

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

  if(ENABLE_STEPS)
  {
    ACTUAL_STEP = fset.add("Step",NUMPIECEINDICES,128);
    ACTUAL_STEP_O = fset.add("StepO",NUMPIECEINDICES,128);
  }

  if(ENABLE_CAPTURED)
  {
    CAPTURED = fset.add("Captured",NUMPIECEINDICES);
    CAPTURED_O = fset.add("CapturedO",NUMPIECEINDICES);
  }

  if(ENABLE_STRATS)
  {
    FRAMESAC = fset.add("FrameSac",NUMPIECEINDICES, 2);
    HOSTAGESAC = fset.add("HostageSac",NUMPIECEINDICES, 2);

    FRAME_P = fset.add("Frame_P",NUMPIECEINDICES, 2);
    FRAME_O = fset.add("Frame_O",NUMPIECEINDICES, 2);

    HOSTAGE_P = fset.add("Hostage_P",NUMPIECEINDICES,NUMPIECEINDICES);
    HOSTAGE_O = fset.add("Hostage_O",NUMPIECEINDICES,NUMPIECEINDICES);

    EBLOCKADE_CENT_P = fset.add("EBlockCent_P",7, 2);
    EBLOCKADE_CENT_O = fset.add("EBlockCent_O",7, 2);
    EBLOCKADE_GYDIST_P = fset.add("EBlockGYDist_P",8, 2);
    EBLOCKADE_GYDIST_O = fset.add("EBlockGYDist_O",8, 2);
  }

  if(ENABLE_TRAPSTATE)
  {
    TRAP_STATE_P = fset.add("TrapStateP",2,8,8);
    TRAP_STATE_O = fset.add("TrapStateO",2,8,8);
  }

  if(ENABLE_TRAPCONTROL)
  {
    TRAP_CONTROL = fset.add("TrapControl",2,17);
  }

  if(ENABLE_CAP_DEF)
  {
    CAP_DEF_ELEDEF = fset.add("CapDefEleDef",2,5);
    CAP_DEF_TRAPDEF = fset.add("CapDefTrapDef",2,5);
    CAP_DEF_RUNAWAY = fset.add("CapDefRunaway",2,5);
    CAP_DEF_INTERFERE = fset.add("CapDefInterfere",2,5);
  }

  if(ENABLE_GOAL_THREAT)
  {
    THREATENS_GOAL = fset.add("ThreatensGoal",5,4);
    ALLOWS_GOAL = fset.add("InvitesGoal");
  }

  if(ENABLE_GOAL_DIST)
  {
    MOVES_RAB_GOAL_DIST = fset.add("MovesRabGoalDist",10,10);
    PLA_SRCDEST_NEAR_ADVANCED_PLA_RAB = fset.add("PlaSrcdestNearAdvancedPlaRab",2,8,4);
    PLA_SRCDEST_NEAR_ADVANCED_OPP_RAB = fset.add("PlaSrcdestNearAdvancedOppRab",2,8,4);
    OPP_SRCDEST_NEAR_ADVANCED_PLA_RAB = fset.add("OppSrcdestNearAdvancedPlaRab",2,8,4);
    OPP_SRCDEST_NEAR_ADVANCED_OPP_RAB = fset.add("OppSrcdestNearAdvancedOppRab",2,8,4);
  }

  if(ENABLE_CAP_THREAT)
  {
    THREATENS_CAP = fset.add("ThreatensCap",NUMPIECEINDICES,5,2);
    INVITES_CAP_MOVED = fset.add("InvitesCapMoved",NUMPIECEINDICES,5,2);
    INVITES_CAP_UNMOVED = fset.add("InvitesCapUnmoved",NUMPIECEINDICES,5,2);
    PREVENTS_CAP = fset.add("PreventsCap",NUMPIECEINDICES,Board::NUMSYMLOCS);
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

  if(ENABLE_BALANCE)
  {
    EM_DIST = fset.add("EMDist",2,2,7,8);
    EH_DIST = fset.add("EHDist",2,2,7,8);
    MH_DIST = fset.add("MHDist",2,2,7,8);
  }

  if(ENABLE_PHALANX)
  {
    CREATES_PHALANX_VS = fset.add("CreatesPhalanxVs",NUMPIECEINDICES,2);
    CREATES_PHALANX_AT = fset.add("CreatesPhalanxAtLoc",32,Board::NUMSYMDIR,2);
    RELEASES_PHALANX_VS = fset.add("ReleasesPhalanxVs",NUMPIECEINDICES,2);
    RELEASES_PHALANX_AT = fset.add("ReleasesPhalanxAtLoc",32,Board::NUMSYMDIR,2);
  }

  if(ENABLE_INFLUENCE)
  {
    RABBIT_ADVANCE = fset.add("RabbitAdvance",8,9);
    PIECE_ADVANCE = fset.add("PieceAdvance",NUMPIECEINDICES,9);
    PIECE_RETREAT = fset.add("PieceRetreat",NUMPIECEINDICES,9);
    ESCAPE_DOMINATOR = fset.add("EscapeDominator",NUMPIECEINDICES,9,5);
    APPROACH_DOMINATOR = fset.add("ApproachDominator",NUMPIECEINDICES,9,5);
    DOMINATES_ADJ = fset.add("DominatesAdj",NUMPIECEINDICES,9);
    SAFE_STEP_ON_TRAP = fset.add("SafeStepOnTrap",2,NUMPIECEINDICES);
    UNSAFE_STEP_ON_TRAP = fset.add("UnafeStepOnTrap",2,NUMPIECEINDICES);
  }

  if(ENABLE_BLOCKING)
  {
    BLOCKS_FROZEN = fset.add("BlocksFrozen",NUMPIECEINDICES,2,4);
    BLOCKS_PUSHPULLED = fset.add("BlocksPushpulled",NUMPIECEINDICES,2,4);
    BLOCKS_CAPTHREATED = fset.add("BlocksCapthreated",NUMPIECEINDICES,2,4);
  }

  if(ENABLE_REVFREECAPS)
  {
    IS_FULL_REVERSIBLE = fset.add("IsFullReversible");
    IS_MOSTLY_REVERSIBLE = fset.add("IsMostlyReversible");
    PUSHPULL_OPP_REVERSIBLE = fset.add("PushpullOppReversible");
    IS_FREE_CAPTURABLE = fset.add("IsFreeCapturable",6);
    REMAKES_EARLIER_MOVE = fset.add("RemakesEarlierMove");
  }

  if(ENABLE_LAST)
  {
    MOVES_LAST_PUSHED = fset.add("MovesLastPushed");
    MOVES_NEAR_LAST = fset.add("MovesNearLast",64);
    MOVES_NEAR_LAST_LAST = fset.add("MovesNearLastLast",64);
  }

  if(ENABLE_DEPENDENCY)
  {
    const string names[4] = {string("1"),string("2"),string("3"),string("4")};
    STEP_INDEPENDENCE = fset.add("StepIndependence",4,4,4,4,names,names,names,names);
  }

  if(ENABLE_RELTACTICS)
  {
    RELTACTICSDIST = fset.add("RTDist",2);
  }

  initPriors();

  IS_INITIALIZED = true;
}

//0 - basic average feature value (midgame, pieces not super advanced)
//1 - if it's the opening, ArimaaFeature::advancementProperty is small
//2 - if it's the endgame, ArimaaFeature::advancementProperty is large
//3 - if ArimaaFeature::advancementNoCapsProperty is large. For fixed advancement, larger this means more pieces on board.

//Make sure to adjust the necessary code below (prior computation, etc) if this changes
static const int NUM_PARALLEL_WEIGHTS = 4;

static vector<double> lossPriorWeight;
static vector<double> winPriorWeight;
static vector<bool> coeffMattersForPrior;

static void addWLPriorFor(fgrpindex_t group, double weightWin, double weightLoss)
{
  for(int i = 0; i<fset.groups[group].size; i++)
  {
    findex_t feature = i + fset.groups[group].baseIdx;
    lossPriorWeight[feature] += weightLoss;
    winPriorWeight[feature] += weightWin;
  }
}

static void setCoeffMatteringForPrior(fgrpindex_t group)
{
  for(int i = 0; i<fset.groups[group].size; i++)
  {
    findex_t feature = i + fset.groups[group].baseIdx;
    coeffMattersForPrior[feature] = true;
  }
}

static bool doesCoeffMatterForPrior(findex_t feature)
{
  return coeffMattersForPrior[feature];
}

static void initPriors()
{
  //Add a weak 1W1L prior over everything
  lossPriorWeight = vector<double>(fset.numFeatures,1.0);
  winPriorWeight = vector<double>(fset.numFeatures,1.0);
  coeffMattersForPrior = vector<bool>(fset.numFeatures,false);

  lossPriorWeight[fset.get(PRIOR)] = 0.0;
  winPriorWeight[fset.get(PRIOR)] = 0.0;

  //Special priors - setting this makes it so that the optimization will compute coeffs for
  //these even if these features never occur in a board position, which is important because
  //they affect the prior's penalties for adjacent values of these features that do occur
  setCoeffMatteringForPrior(TRAP_CONTROL);
  setCoeffMatteringForPrior(MOVES_NEAR_LAST);
  setCoeffMatteringForPrior(MOVES_NEAR_LAST_LAST);

  //Add additional prior strength for most things
  double epr = 1.5;

  addWLPriorFor(PASS,epr,epr);

  if(ENABLE_SRCDEST)
  {
    addWLPriorFor(SRC,epr,epr);
    addWLPriorFor(SRC_O,epr,epr);
    addWLPriorFor(DEST,epr,epr);
    addWLPriorFor(DEST_O,epr,epr);
  }
  if(ENABLE_SRCDEST_GIVEN_OPPRAD)
  {
    addWLPriorFor(SRC_WHILE_OPP_RAD,epr,epr);
    addWLPriorFor(DEST_WHILE_OPP_RAD,epr,epr);
  }

  if(ENABLE_STEPS)
  {
    addWLPriorFor(ACTUAL_STEP,epr,epr);
    addWLPriorFor(ACTUAL_STEP_O,epr,epr);
  }

  if(ENABLE_CAPTURED)
  {
    addWLPriorFor(CAPTURED,epr,epr);
    addWLPriorFor(CAPTURED_O,epr,epr);
  }

  if(ENABLE_STRATS)
  {
    addWLPriorFor(FRAMESAC,epr,epr);
    addWLPriorFor(HOSTAGESAC,epr,epr);
    addWLPriorFor(FRAME_P,epr,epr);
    addWLPriorFor(FRAME_O,epr,epr);
    addWLPriorFor(HOSTAGE_P,epr,epr);
    addWLPriorFor(HOSTAGE_O,epr,epr);
    addWLPriorFor(EBLOCKADE_CENT_P,epr,epr);
    addWLPriorFor(EBLOCKADE_CENT_O,epr,epr);
    addWLPriorFor(EBLOCKADE_GYDIST_P,epr,epr);
    addWLPriorFor(EBLOCKADE_GYDIST_O,epr,epr);
  }

  if(ENABLE_TRAPSTATE)
  {
    addWLPriorFor(TRAP_STATE_P,epr,epr);
    addWLPriorFor(TRAP_STATE_O,epr,epr);
  }

  if(ENABLE_CAP_DEF)
  {
    addWLPriorFor(CAP_DEF_ELEDEF,epr,epr);
    addWLPriorFor(CAP_DEF_TRAPDEF,epr,epr);
    addWLPriorFor(CAP_DEF_TRAPDEF,epr,epr);
    addWLPriorFor(CAP_DEF_INTERFERE,epr,epr);
  }

  if(ENABLE_GOAL_THREAT)
  {
    addWLPriorFor(WINS_GAME,epr,epr);
    addWLPriorFor(THREATENS_GOAL,epr,epr);
    addWLPriorFor(ALLOWS_GOAL,epr,epr);
  }

  if(ENABLE_GOAL_DIST)
  {
    addWLPriorFor(MOVES_RAB_GOAL_DIST,epr,epr);
    addWLPriorFor(PLA_SRCDEST_NEAR_ADVANCED_PLA_RAB,epr,epr);
    addWLPriorFor(PLA_SRCDEST_NEAR_ADVANCED_OPP_RAB,epr,epr);
    addWLPriorFor(OPP_SRCDEST_NEAR_ADVANCED_PLA_RAB,epr,epr);
    addWLPriorFor(OPP_SRCDEST_NEAR_ADVANCED_OPP_RAB,epr,epr);
  }

  if(ENABLE_CAP_THREAT)
  {
    addWLPriorFor(THREATENS_CAP,epr,epr);
    addWLPriorFor(INVITES_CAP_MOVED,epr,epr);
    addWLPriorFor(INVITES_CAP_UNMOVED,epr,epr);
    addWLPriorFor(PREVENTS_CAP,epr,epr);
  }

  if(ENABLE_FREEZE)
  {
    addWLPriorFor(FREEZES_STR,epr,epr);
    addWLPriorFor(THAWS_STR,epr,epr);
    addWLPriorFor(FREEZES_AT,epr,epr);
    addWLPriorFor(THAWS_AT,epr,epr);
  }
  if(ENABLE_DOMINATE)
  {
    addWLPriorFor(DOMS_STR,epr,epr);
    addWLPriorFor(UNDOMS_STR,epr,epr);
    addWLPriorFor(DOMS_AT,epr,epr);
    addWLPriorFor(UNDOMS_AT,epr,epr);
  }
  if(ENABLE_DOMFAR)
  {
    addWLPriorFor(DOMRAD2_STR,epr,epr);
    addWLPriorFor(UNDOMRAD2_STR,epr,epr);
    addWLPriorFor(DOMRAD2_AT,epr,epr);
    addWLPriorFor(UNDOMRAD2_AT,epr,epr);
  }
  if(ENABLE_BALANCE)
  {
    addWLPriorFor(EM_DIST,epr,epr);
    addWLPriorFor(EH_DIST,epr,epr);
    addWLPriorFor(MH_DIST,epr,epr);
  }

  if(ENABLE_PHALANX)
  {
    addWLPriorFor(CREATES_PHALANX_VS,epr,epr);
    addWLPriorFor(CREATES_PHALANX_AT,epr,epr);
    addWLPriorFor(RELEASES_PHALANX_VS,epr,epr);
    addWLPriorFor(RELEASES_PHALANX_AT,epr,epr);
  }

  if(ENABLE_INFLUENCE)
  {
    addWLPriorFor(RABBIT_ADVANCE,epr,epr);
    addWLPriorFor(PIECE_ADVANCE,epr,epr);
    addWLPriorFor(PIECE_RETREAT,epr,epr);
    addWLPriorFor(ESCAPE_DOMINATOR,epr,epr);
    addWLPriorFor(APPROACH_DOMINATOR,epr,epr);
    addWLPriorFor(DOMINATES_ADJ,epr,epr);
    addWLPriorFor(SAFE_STEP_ON_TRAP,epr,epr);
    addWLPriorFor(UNSAFE_STEP_ON_TRAP,epr,epr);
  }

  if(ENABLE_BLOCKING)
  {
    addWLPriorFor(BLOCKS_FROZEN,epr,epr);
    addWLPriorFor(BLOCKS_PUSHPULLED,epr,epr);
    addWLPriorFor(BLOCKS_CAPTHREATED,epr,epr);
  }

  if(ENABLE_REVFREECAPS)
  {
    addWLPriorFor(IS_FULL_REVERSIBLE,epr,epr);
    addWLPriorFor(IS_MOSTLY_REVERSIBLE,epr,epr);
    addWLPriorFor(PUSHPULL_OPP_REVERSIBLE,epr,epr);
    addWLPriorFor(IS_FREE_CAPTURABLE,epr,epr);
    addWLPriorFor(REMAKES_EARLIER_MOVE,epr,epr);
  }

  if(ENABLE_LAST)
  {
    addWLPriorFor(MOVES_LAST_PUSHED,epr,epr);
  }

  if(ENABLE_DEPENDENCY)
  {
    addWLPriorFor(STEP_INDEPENDENCE,epr,epr);
  }

  if(ENABLE_RELTACTICS)
  {
    addWLPriorFor(RELTACTICSDIST,epr,epr);
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
  //NOTE: Locally near 0, 2W2L behaves like a penalty function of the standard normal, which is like squared error -x^2/2

  double logProb = 0;

  int numFolds = coeffsForFolds.size();
  DEBUGASSERT(numFolds == 1 || numFolds == NUM_PARALLEL_WEIGHTS);

  const vector<double>& coeffs = coeffsForFolds[0];
  int size = coeffs.size();
  DEBUGASSERT(size == fset.numFeatures);
  for(int i = 0; i<size; i++)
  {
    logProb += winPriorWeight[i] * computeLogProbOfWin(coeffs[i],0);
    logProb += lossPriorWeight[i] * computeLogProbOfWin(0,coeffs[i]);
  }

  if(ENABLE_TRAPCONTROL)
  {
    //Higher trap controls are better - give each one a 5W4L against the previous
    for(int j = 0; j<16; j++)
    {
      int f00 = fset.get(TRAP_CONTROL,0,j);
      int f01 = fset.get(TRAP_CONTROL,0,j+1);
      int f10 = fset.get(TRAP_CONTROL,1,j);
      int f11 = fset.get(TRAP_CONTROL,1,j+1);
      logProb += 4 * computeLogProbOfWin(coeffs[f00],coeffs[f01]);
      logProb += 5 * computeLogProbOfWin(coeffs[f01],coeffs[f00]);
      logProb += 4 * computeLogProbOfWin(coeffs[f10],coeffs[f11]);
      logProb += 5 * computeLogProbOfWin(coeffs[f11],coeffs[f10]);
    }
  }

  if(ENABLE_LAST)
  {
    //Each step is similar - give each one a 4.2W 4L against the previous
    for(int j = 0; j<63; j++)
    {
      int f00 = fset.get(MOVES_NEAR_LAST,j);
      int f01 = fset.get(MOVES_NEAR_LAST,j+1);
      int f10 = fset.get(MOVES_NEAR_LAST_LAST,j);
      int f11 = fset.get(MOVES_NEAR_LAST_LAST,j+1);
      logProb += 4 * computeLogProbOfWin(coeffs[f00],coeffs[f01]);
      logProb += 4.2 * computeLogProbOfWin(coeffs[f01],coeffs[f00]);
      logProb += 4 * computeLogProbOfWin(coeffs[f10],coeffs[f11]);
      logProb += 4.2 * computeLogProbOfWin(coeffs[f11],coeffs[f10]);
    }
  }

  //Add prior that adjustments based on game phase are zero
  if(numFolds > 1)
  {
    DEBUGASSERT(numFolds == NUM_PARALLEL_WEIGHTS);
    DEBUGASSERT(NUM_PARALLEL_WEIGHTS == 4);
    for(int parallelFold = 1; parallelFold <= 2; parallelFold++)
    {
      for(int f = 0; f<fset.numFeatures; f++)
      {
        //Like 8W8L, or normal with precision 4, 2x^2
        double diff = coeffsForFolds[parallelFold][f];
        logProb -= diff * diff * 2;
      }
    }
    for(int parallelFold = 3; parallelFold <= 3; parallelFold++)
    {
      for(int f = 0; f<fset.numFeatures; f++)
      {
        //Like 6W6L, or normal with precision 3, 1.5x^2
        double diff = coeffsForFolds[parallelFold][f];
        logProb -= diff * diff * 1.5;
      }
    }
  }

  return logProb;
}

struct MoveFeaturePosData
{
  Board board;

  int ufDist[BSIZE];
  CapThreat oppCapThreats[Threats::maxCapsPerPla];
  int numOppCapThreats;

  Bitmap plaCapMap;
  Bitmap oppCapMap;

  int plaGoalDist;
  int oppGoalDist;
  Bitmap goalDistIs[2][9];
  int goalDist[2][BSIZE];
  int nearAdvancedRabLoc[2][BSIZE];

  int pStronger[2][NUMTYPES];
  int pieceIndex[2][NUMTYPES];
  Bitmap pStrongerMaps[2][NUMTYPES];

  int influence[2][BSIZE];

  int oldTrapStates[2][4];

  int numLastPushed;
  loc_t lastPushed[2];
  int lastMoveSum[BSIZE];
  int lastLastMoveSum[BSIZE];

  bool oppHoldsFrameAtTrap[4];
  bool oppFrameIsPartial[4];
  bool oppFrameIsEleEle[4];

  set<hash_t> relTactic0SHashes;
  set<hash_t> relTactic1SHashes;

  vector<double> parallelWeights;
};


static void getFeatures(Board& bAfter, const void* dataBuf, pla_t pla, move_t move, const BoardHistory& hist,
    void (*handleFeature)(findex_t,void*), void* handleFeatureData)
{
  //Handle Pass Move
  if(move == PASSMOVE || move == QPASSMOVE)
  {
    (*handleFeature)(fset.get(PASS),handleFeatureData);
    return;
  }

  //Moves that win the game have no other possible features, we don't want to spend valuable
  //effort tuning coefficents to match the particular choice of winning move.
  if(bAfter.getWinner() == pla)
  {
    (*handleFeature)(fset.get(WINS_GAME),handleFeatureData);
    return;
  }

  const MoveFeaturePosData& data = *((const MoveFeaturePosData*)(dataBuf));
  const Board& b = data.board;

  int pStronger[2][NUMTYPES];
  Bitmap pStrongerMaps[2][NUMTYPES];
  bAfter.initializeStronger(pStronger);
  bAfter.initializeStrongerMaps(pStrongerMaps);

  //Source and destination piece movement features
  pla_t opp = gOpp(pla);
  Bitmap oppMap = b.pieceMaps[opp][0];
  loc_t src[8];
  loc_t dest[8];
  int numChanges = b.getChanges(move,src,dest);
  for(int i = 0; i<numChanges; i++)
  {
    //Retrieve piece stats
    pla_t owner = b.owners[src[i]];
    piece_t piece = b.pieces[src[i]];
    int pieceIndex = data.pieceIndex[owner][piece];

    //SRCDEST features
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

    if(ENABLE_CAPTURED && (dest[i] == ERRLOC))
    {
      (*handleFeature)(fset.get(owner == pla ? CAPTURED : CAPTURED_O,pieceIndex),handleFeatureData);
    }

    //FRAMESAC features
    if(ENABLE_STRATS)
    {
      if(dest[i] == ERRLOC && owner == pla && Board::ISTRAP[src[i]] && data.oppHoldsFrameAtTrap[Board::TRAPINDEX[src[i]]])
      {
        bool isEleEle = data.oppFrameIsEleEle[Board::TRAPINDEX[src[i]]];
        (*handleFeature)(fset.get(FRAMESAC,pieceIndex,isEleEle),handleFeatureData);
      }
    }

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

    if(ENABLE_GOAL_DIST && owner == pla && dest[i] != ERRLOC)
    {
      if(piece == RAB && (data.goalDist[pla][src[i]] < 9 || data.goalDist[pla][dest[i]] < 9))
        (*handleFeature)(fset.get(MOVES_RAB_GOAL_DIST,data.goalDist[pla][src[i]],data.goalDist[pla][dest[i]]),handleFeatureData);
    }
    if(ENABLE_GOAL_DIST && dest[i] != ERRLOC)
    {
      loc_t plaRlocNearSrc = data.nearAdvancedRabLoc[pla][src[i]];
      loc_t plaRlocNearDest = data.nearAdvancedRabLoc[pla][dest[i]];
      loc_t oppRlocNearSrc = data.nearAdvancedRabLoc[opp][src[i]];
      loc_t oppRlocNearDest = data.nearAdvancedRabLoc[opp][dest[i]];
      if(plaRlocNearSrc != ERRLOC && Board::GOALYDIST[pla][plaRlocNearSrc] > 0)
      {
        DEBUGASSERT(data.goalDist[pla][plaRlocNearSrc] <= 7);
        int mDist = Board::manhattanDist(plaRlocNearSrc+Board::GOALLOCINC[pla],src[i]);
        DEBUGASSERT(mDist <= 3);
        (*handleFeature)(fset.get(owner == pla ? PLA_SRCDEST_NEAR_ADVANCED_PLA_RAB: OPP_SRCDEST_NEAR_ADVANCED_PLA_RAB,
            0,data.goalDist[pla][plaRlocNearSrc],mDist),handleFeatureData);
      }
      if(plaRlocNearDest != ERRLOC && Board::GOALYDIST[pla][plaRlocNearDest] > 0)
      {
        DEBUGASSERT(data.goalDist[pla][plaRlocNearDest] <= 7);
        int mDist = Board::manhattanDist(plaRlocNearDest+Board::GOALLOCINC[pla],dest[i]);
        DEBUGASSERT(mDist <= 3);
        (*handleFeature)(fset.get(owner == pla ? PLA_SRCDEST_NEAR_ADVANCED_PLA_RAB: OPP_SRCDEST_NEAR_ADVANCED_PLA_RAB,
            1,data.goalDist[pla][plaRlocNearDest],mDist),handleFeatureData);
      }
      if(oppRlocNearSrc != ERRLOC && Board::GOALYDIST[opp][oppRlocNearSrc] > 0)
      {
        DEBUGASSERT(data.goalDist[opp][oppRlocNearSrc] <= 7);
        int mDist = Board::manhattanDist(oppRlocNearSrc+Board::GOALLOCINC[opp],src[i]);
        DEBUGASSERT(mDist <= 3);
        (*handleFeature)(fset.get(owner == pla ? PLA_SRCDEST_NEAR_ADVANCED_OPP_RAB: OPP_SRCDEST_NEAR_ADVANCED_OPP_RAB,
            0,data.goalDist[opp][oppRlocNearSrc],mDist),handleFeatureData);
      }
      if(oppRlocNearDest != ERRLOC && Board::GOALYDIST[opp][oppRlocNearDest] > 0)
      {
        DEBUGASSERT(data.goalDist[opp][oppRlocNearDest] <= 7);
        int mDist = Board::manhattanDist(oppRlocNearDest+Board::GOALLOCINC[opp],dest[i]);
        DEBUGASSERT(mDist <= 3);
        (*handleFeature)(fset.get(owner == pla ? PLA_SRCDEST_NEAR_ADVANCED_OPP_RAB: OPP_SRCDEST_NEAR_ADVANCED_OPP_RAB,
            1,data.goalDist[opp][oppRlocNearDest],mDist),handleFeatureData);
      }
    }
  } //END Changes loop

  if(ENABLE_STEPS)
  {
    Board bb = b;
    for(int i = 0; i<4; i++)
    {
      step_t s = getStep(move,i);
      if(s == PASSSTEP || s == ERRSTEP || s == QPASSSTEP)
        break;

      loc_t k0 = gSrc(s);
      loc_t k1 = gDest(s);
      int symLoc = Board::SYMLOC[pla][k0];
      int symDir = getSymDir(pla,k0,k1);
      int pieceIndex = data.pieceIndex[bb.owners[k0]][bb.pieces[k0]];
      if(bb.owners[k0] == pla)
        (*handleFeature)(fset.get(ACTUAL_STEP,pieceIndex,symDir*32 + symLoc),handleFeatureData);
      else
        (*handleFeature)(fset.get(ACTUAL_STEP_O,pieceIndex,symDir*32 + symLoc),handleFeatureData);

      bb.makeStep(s);

    }
  }

  if(ENABLE_STRATS)
  {
    for(int trapid = 0; trapid < 4; trapid++)
    {
      FrameThreat ft;
      loc_t trap = Board::TRAPLOCS[trapid];
      if(Strats::findFrame(bAfter,pla,trap,&ft))
      {
        bool isEleEle = (Board::RADIUS[1][trap] & b.pieceMaps[0][ELE]).hasBits() &&
                        (Board::RADIUS[1][trap] & b.pieceMaps[1][ELE]).hasBits();
        (*handleFeature)(fset.get(FRAME_P,data.pieceIndex[pla][bAfter.pieces[trap]],isEleEle),handleFeatureData);
      }
      else if(Strats::findFrame(bAfter,opp,trap,&ft))
      {
        bool isEleEle = (Board::RADIUS[1][trap] & b.pieceMaps[0][ELE]).hasBits() &&
                        (Board::RADIUS[1][trap] & b.pieceMaps[1][ELE]).hasBits();
        (*handleFeature)(fset.get(FRAME_O,data.pieceIndex[opp][bAfter.pieces[trap]],isEleEle),handleFeatureData);
      }
    }

    BlockadeThreat bt;
    if(Strats::findBlockades(bAfter,pla,&bt))
    {
      int centrality = Board::CENTERDIST[bt.pinnedLoc];
      int gydist = Board::GOALYDIST[pla][bt.pinnedLoc];
      bool usesEle = (bt.holderMap & bAfter.pieceMaps[pla][ELE]).hasBits();
      (*handleFeature)(fset.get(EBLOCKADE_CENT_P,centrality,usesEle),handleFeatureData);
      (*handleFeature)(fset.get(EBLOCKADE_GYDIST_P,gydist,usesEle),handleFeatureData);
    }
    bt = BlockadeThreat();
    if(Strats::findBlockades(bAfter,opp,&bt))
    {
      int centrality = Board::CENTERDIST[bt.pinnedLoc];
      int gydist = Board::GOALYDIST[opp][bt.pinnedLoc];
      bool usesEle = (bt.holderMap & bAfter.pieceMaps[opp][ELE]).hasBits();
      (*handleFeature)(fset.get(EBLOCKADE_CENT_O,centrality,usesEle),handleFeatureData);
      (*handleFeature)(fset.get(EBLOCKADE_GYDIST_O,gydist,usesEle),handleFeatureData);
    }
  }

  //Trap State Features
  if(ENABLE_TRAPSTATE)
  {
    for(int trapIndex = 0; trapIndex < 4; trapIndex++)
    {
      loc_t kt = Board::TRAPLOCS[trapIndex];
      int isOppTrap = !Board::ISPLATRAP[trapIndex][pla];
      int oldPlaTrapState = data.oldTrapStates[pla][trapIndex];
      int newPlaTrapState = ArimaaFeature::getTrapState(bAfter,pla,kt);
      int oldOppTrapState = data.oldTrapStates[opp][trapIndex];
      int newOppTrapState = ArimaaFeature::getTrapState(bAfter,opp,kt);

      if(oldPlaTrapState != newPlaTrapState)
        (*handleFeature)(fset.get(TRAP_STATE_P,isOppTrap,oldPlaTrapState,newPlaTrapState),handleFeatureData);

      if(oldOppTrapState != newOppTrapState)
        (*handleFeature)(fset.get(TRAP_STATE_O,isOppTrap,oldOppTrapState,newOppTrapState),handleFeatureData);
    }
  }

  if(ENABLE_TRAPCONTROL)
  {
    int ufDist[BSIZE];
    UFDist::get(bAfter,ufDist,pStrongerMaps);

    int tc[2][4];
    Eval::getBasicTrapControls(bAfter,pStronger,pStrongerMaps,ufDist,tc);

    for(int trapIndex = 0; trapIndex<4; trapIndex++)
    {
      int isOppTrap = !Board::ISPLATRAP[trapIndex][pla];
      int val = tc[pla][trapIndex]/10 + 8;
      if(val < 0) val = 0;
      else if(val > 16) val = 16;

      (*handleFeature)(fset.get(TRAP_CONTROL,isOppTrap,val),handleFeatureData);
    }
  }

  //Capture Defense Features
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

  if(ENABLE_GOAL_THREAT)
  {
    int newPlaGoalDist = BoardTrees::goalDist(bAfter,pla,4);
    int newOppGoalDist = BoardTrees::goalDist(bAfter,opp,4);

    if(newOppGoalDist <= 4)
      (*handleFeature)(fset.get(ALLOWS_GOAL),handleFeatureData);

    if(newPlaGoalDist < data.plaGoalDist && newPlaGoalDist <= 4 && newPlaGoalDist > 0)
    {
      int minWinDefSteps = SearchMoveGen::minWinDefSteps(bAfter,3);
      DEBUGASSERT(minWinDefSteps >= 1 && minWinDefSteps <= 4)
      (*handleFeature)(fset.get(THREATENS_GOAL,newPlaGoalDist,minWinDefSteps-1),handleFeatureData);
    }

  }

  //Where was the piece at this location prior to this move?
  loc_t priorLocation[BSIZE];
  for(int i = 0; i<BSIZE; i++)
    priorLocation[i] = i;
  for(int i = 0; i<numChanges; i++)
  {
    if(dest[i] != ERRLOC)
      priorLocation[dest[i]] = src[i];
  }

  Bitmap capThreatMap;
  if(ENABLE_CAP_THREAT)
  {
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

  if(ENABLE_BALANCE)
  {
    loc_t plaEleLoc = bAfter.findElephant(pla);
    loc_t oppEleLoc = bAfter.findElephant(opp);
    for(int isPla1 = 0; isPla1 <= 1; isPla1++)
    {
      loc_t eleLoc = (bool)isPla1 ? plaEleLoc : oppEleLoc;
      if(eleLoc == ERRLOC)
        continue;
      for(int isPla2 = 0; isPla2 <= 1; isPla2++)
      {
        pla_t pla2 = (bool)isPla2 ? pla : opp;
        Bitmap camels = bAfter.pieceCounts[pla2][CAM] > 0 ? bAfter.pieceMaps[pla2][CAM] :
                        bAfter.pieceCounts[pla2][HOR] > 0 && pStronger[pla2][HOR] == 1 ? bAfter.pieceMaps[pla2][HOR] :
                        Bitmap();
        Bitmap camelTemp = camels;
        while(camelTemp.hasBits())
        {
          loc_t camLoc = camelTemp.nextBit();
          int y = pla2 == GOLD ? gY(camLoc) : 7-gY(camLoc);
          int dist = Board::MANHATTANDIST[eleLoc - camLoc + DIFFIDX_ADD];
          if(dist > 7) dist = 7;
          (*handleFeature)(fset.get(EM_DIST,isPla1,isPla2,dist-1,y),handleFeatureData);
        }

        if(isPla1 != isPla2)
        {
          Bitmap horses = bAfter.pieceCounts[pla2][CAM] > 0 ? (bAfter.pieceCounts[pla2][HOR] > 0 ? bAfter.pieceMaps[pla2][HOR] : bAfter.pieceMaps[pla2][DOG]) :
                          bAfter.pieceCounts[pla2][HOR] > 0 && pStronger[pla2][HOR] > 1 ? bAfter.pieceMaps[pla2][HOR] :
                          bAfter.pieceMaps[pla2][DOG];
          while(horses.hasBits())
          {
            loc_t horLoc = horses.nextBit();
            int y = pla2 == GOLD ? gY(horLoc) : 7-gY(horLoc);
            int dist = Board::MANHATTANDIST[eleLoc - horLoc  + DIFFIDX_ADD];
            if(dist > 7) dist = 7;
            (*handleFeature)(fset.get(EH_DIST,isPla1,isPla2,dist-1,y),handleFeatureData);
          }

          if(camels.isSingleBit())
          {
            loc_t camLoc = camels.nextBit();
            //Flip pla1 and pla2 implicitly
            pla_t pla1 = gOpp(pla2);
            Bitmap horses = bAfter.pieceCounts[pla1][CAM] > 0 ? (bAfter.pieceCounts[pla1][HOR] > 0 ? bAfter.pieceMaps[pla1][HOR] : bAfter.pieceMaps[pla1][DOG]) :
                            bAfter.pieceCounts[pla1][HOR] > 0 && pStronger[pla1][HOR] > 1 ? bAfter.pieceMaps[pla1][HOR] :
                            bAfter.pieceMaps[pla1][DOG];
            while(horses.hasBits())
            {
              loc_t horLoc = horses.nextBit();
              int y = pla1 == GOLD ? gY(horLoc) : 7-gY(horLoc);
              int dist = Board::MANHATTANDIST[camLoc - horLoc + DIFFIDX_ADD];
              if(dist > 7) dist = 7;
              (*handleFeature)(fset.get(MH_DIST,isPla2,isPla1,dist-1,y),handleFeatureData);
            }
          }
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
    Bitmap oppMap = b.pieceMaps[opp][0] & bAfter.pieceMaps[opp][0] & rad3AffectMap;
    while(oppMap.hasBits())
    {
      loc_t oloc = oppMap.nextBit();
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

    oppMap = bAfter.pieceMaps[opp][0] & rad3AffectMap;
    while(oppMap.hasBits())
    {
      loc_t oloc = oppMap.nextBit();
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

  if(ENABLE_INFLUENCE)
  {
    for(int i = 0; i<numChanges; i++)
    {
      if(b.owners[src[i]] == pla && dest[i] != ERRLOC)
      {
        if(b.pieces[src[i]] == RAB)
        {
          int dist = Board::GOALYDIST[pla][dest[i]];
          int dy = (pla == GOLD ? N : S);
          loc_t goalTarget = dest[i] + dist * dy;
          int influence = data.influence[pla][goalTarget] * 2;
          if(CW1(goalTarget)) {influence += data.influence[pla][goalTarget W];}
          if(CE1(goalTarget)) {influence += data.influence[pla][goalTarget E];}
          influence /= 4; //Averaging
          (*handleFeature)(fset.get(RABBIT_ADVANCE,dist,influence),handleFeatureData);
        }

        int advdest = Board::ADVANCEMENT[pla][dest[i]];
        int advsrc = Board::ADVANCEMENT[pla][src[i]];

        int influence;
        int file = gX(dest[i]);
        if(file <= 2) influence = data.influence[pla][Board::PLATRAPLOCS[gOpp(pla)][0]];
        else if(file >= 5) influence = data.influence[pla][Board::PLATRAPLOCS[gOpp(pla)][1]];
        else influence = (data.influence[pla][Board::PLATRAPLOCS[gOpp(pla)][0]] + data.influence[pla][Board::PLATRAPLOCS[gOpp(pla)][1]])/2;

        if(advdest > advsrc)
          (*handleFeature)(fset.get(
              PIECE_ADVANCE,data.pieceIndex[pla][b.pieces[src[i]]],influence),handleFeatureData);
        else if(advdest < advsrc)
          (*handleFeature)(fset.get(
              PIECE_RETREAT,data.pieceIndex[pla][b.pieces[src[i]]],influence),handleFeatureData);

        loc_t nearestDomSrc = b.nearestDominator(pla,b.pieces[src[i]],src[i],4);
        loc_t nearestDomDest = b.nearestDominator(pla,b.pieces[src[i]],dest[i],4);
        if(nearestDomSrc != ERRLOC)
        {
          int rad = Board::manhattanDist(src[i],nearestDomSrc);
          if(nearestDomDest == ERRLOC || rad < Board::manhattanDist(dest[i],nearestDomDest))
          {
            influence = data.influence[pla][src[i]];
            (*handleFeature)(fset.get(
                ESCAPE_DOMINATOR,data.pieceIndex[pla][b.pieces[src[i]]],influence,rad),handleFeatureData);
          }
        }
        else if(nearestDomDest != ERRLOC)
        {
          int rad = Board::manhattanDist(dest[i],nearestDomDest);
          if(nearestDomSrc == ERRLOC || rad < Board::manhattanDist(src[i],nearestDomSrc))
          {
            influence = data.influence[pla][dest[i]];
            (*handleFeature)(fset.get(
                APPROACH_DOMINATOR,data.pieceIndex[pla][b.pieces[src[i]]],influence,rad),handleFeatureData);
          }
        }

        if(Board::ISTRAP[dest[i]])
        {
          int trapIndex = Board::TRAPINDEX[dest[i]];
          int isOppTrap = !Board::ISPLATRAP[trapIndex][pla];
          if(bAfter.trapGuardCounts[pla][trapIndex] >= 2)
            (*handleFeature)(fset.get(SAFE_STEP_ON_TRAP,isOppTrap,data.pieceIndex[pla][b.pieces[src[i]]]),handleFeatureData);
          else
            (*handleFeature)(fset.get(UNSAFE_STEP_ON_TRAP,isOppTrap,data.pieceIndex[pla][b.pieces[src[i]]]),handleFeatureData);
        }
      }
    }

    Bitmap dominatedOppMap = b.pieceMaps[opp][0] & bAfter.pieceMaps[opp][0] & bAfter.dominatedMap;
    while(dominatedOppMap.hasBits())
    {
      loc_t loc = dominatedOppMap.nextBit();
      loc_t priorLoc = priorLocation[loc];
      if(!b.isDominated(priorLoc))
      {
        int influence = data.influence[pla][loc];
        (*handleFeature)(fset.get(DOMINATES_ADJ,data.pieceIndex[opp][bAfter.pieces[loc]],influence),handleFeatureData);
      }
    }
  }

  if(ENABLE_BLOCKING)
  {
    //Find all opponent pieces pushpulled - should be exactly one
    int numOppsPushpulled = 0;
    loc_t oppPPSrc = ERRLOC;
    loc_t oppPPDest = ERRLOC;
    Bitmap plaDests;
    for(int i = 0; i<numChanges; i++)
    {
      if(dest[i] == ERRLOC)
        continue;
      pla_t owner = b.owners[src[i]];
      if(owner == opp)
      {
        numOppsPushpulled++;
        oppPPSrc = src[i];
        oppPPDest = dest[i];
      }
      else
        plaDests.setOn(dest[i]);
    }

    //Exactly one opp piece pushed one space
    if(numOppsPushpulled == 1 && Board::isAdjacent(oppPPSrc,oppPPDest))
    {
      bool firstStrongerFound = false;
      for(int i = 0; i<numChanges; i++)
      {
        if(dest[i] == ERRLOC)
          continue;
        if(b.owners[src[i]] != pla)
          continue;

        //When are we eligible to be considered a helping blocker?

        //Not eligible unless we are adjacent to its final location
        if(!Board::isAdjacent(oppPPDest,dest[i]))
          continue;

        //Not eligible if we started in its final location or were already adjacent to it
        if(Board::manhattanDist(src[i],oppPPDest) <= 1)
          continue;

        //Not eligible if we were stronger than it and we ended in its start location
        //or began adjacent to its start location or are the only piece that moved next to it
        if(b.pieces[src[i]] > b.pieces[oppPPSrc] &&
            (dest[i] == oppPPSrc || Board::isAdjacent(oppPPSrc,src[i]) || (plaDests & ~Bitmap::makeLoc(dest[i])).isEmpty()))
          continue;

        //Not eligible if we are the first piece stronger passing the previous tests that it that moved next to it
        if(!firstStrongerFound && b.pieces[src[i]] > b.pieces[oppPPSrc])
        {
          firstStrongerFound = true;
          continue;
        }

        //Make sure we didn't step on a trap - that's weird.
        if(Board::ISTRAP[dest[i]])
          continue;

        //So we stepped adjacent to it, yay.
        int pieceIndex = data.pieceIndex[opp][b.pieces[oppPPSrc]];
        bool isWeaker = b.pieces[src[i]] < b.pieces[oppPPSrc];
        int symDir = getSymDir(pla,oppPPDest,dest[i]);
        (*handleFeature)(fset.get(BLOCKS_PUSHPULLED,pieceIndex,isWeaker,symDir),handleFeatureData);

        //While we're at it, if it's frozen...
        if(bAfter.isFrozen(oppPPDest))
          (*handleFeature)(fset.get(BLOCKS_FROZEN,pieceIndex,isWeaker,symDir),handleFeatureData);

        //And if it's under capture threat...
        if(capThreatMap.isOne(oppPPDest))
          (*handleFeature)(fset.get(BLOCKS_CAPTHREATED,pieceIndex,isWeaker,symDir),handleFeatureData);
      }
    }

    //Check all other frozen opponent pieces
    Bitmap frozenOpp = bAfter.frozenMap & bAfter.pieceMaps[opp][0];
    if(oppPPDest != ERRLOC)
      frozenOpp.setOff(oppPPDest);
    while(frozenOpp.hasBits())
    {
      loc_t oloc = frozenOpp.nextBit();
      Bitmap plaDestsAdj = plaDests & Board::RADIUS[1][oloc];
      while(plaDestsAdj.hasBits())
      {
        loc_t ploc = plaDestsAdj.nextBit();
        if(bAfter.pieces[ploc] > bAfter.pieces[oloc])
          continue;

        int pieceIndex = data.pieceIndex[opp][bAfter.pieces[oloc]];
        bool isWeaker = bAfter.pieces[ploc] < bAfter.pieces[oloc];
        int symDir = getSymDir(pla,oloc,ploc);
        (*handleFeature)(fset.get(BLOCKS_FROZEN,pieceIndex,isWeaker,symDir),handleFeatureData);
      }
    }

    //Check all other capturethreated opponent pieces
    Bitmap capThreatedOpp = capThreatMap & bAfter.pieceMaps[opp][0];
    if(oppPPDest != ERRLOC)
      capThreatedOpp.setOff(oppPPDest);
    while(capThreatedOpp.hasBits())
    {
      loc_t oloc = capThreatedOpp.nextBit();
      if(!bAfter.isDominatedC(oloc))
        continue;

      Bitmap plaDestsAdj = plaDests & Board::RADIUS[1][oloc];
      while(plaDestsAdj.hasBits())
      {
        loc_t ploc = plaDestsAdj.nextBit();
        if(bAfter.pieces[ploc] > bAfter.pieces[oloc])
          continue;

        int pieceIndex = data.pieceIndex[opp][bAfter.pieces[oloc]];
        bool isWeaker = bAfter.pieces[ploc] < bAfter.pieces[oloc];
        int symDir = getSymDir(pla,oloc,ploc);
        (*handleFeature)(fset.get(BLOCKS_CAPTHREATED,pieceIndex,isWeaker,symDir),handleFeatureData);
      }
    }
  }

  if(ENABLE_REVFREECAPS && b.step == 0)
  {
    move_t reverseMove;
    DEBUGASSERT(b.turnNumber >= hist.minTurnNumber && b.turnNumber <= hist.maxTurnNumber)
    int rev = 0;
    if(bAfter.pieceCounts[GOLD][0] == b.pieceCounts[GOLD][0] &&
       bAfter.pieceCounts[SILV][0] == b.pieceCounts[SILV][0])
      rev = SearchUtils::isReversibleAssumeNoCaps(move,bAfter,reverseMove);
    if(rev == 2)
      (*handleFeature)(fset.get(IS_FULL_REVERSIBLE),handleFeatureData);
    else if(rev == 1)
      (*handleFeature)(fset.get(IS_MOSTLY_REVERSIBLE),handleFeatureData);
    else
    {
      int numOppMoved = 0;
      loc_t oppSrc = ERRLOC;
      loc_t oppDest = ERRLOC;
      for(int i = 0; i<numChanges; i++)
      {
        if(b.owners[src[i]] == opp && dest[i] != ERRLOC)
        {
          numOppMoved++;
          oppSrc = src[i];
          oppDest = dest[i];
        }
      }

      //Exactly one opp moved - see if it is able to step back
      if(numOppMoved == 1)
      {
        if(Board::isAdjacent(oppSrc,oppDest) && bAfter.owners[oppSrc] == NPLA &&
            bAfter.isThawed(oppDest) && bAfter.isRabOkay(opp,oppDest,oppSrc))
        {
          (*handleFeature)(fset.get(PUSHPULL_OPP_REVERSIBLE),handleFeatureData);
        }
      }
    }

    if(bAfter.pieceCounts[GOLD][0] == b.pieceCounts[GOLD][0] &&
       bAfter.pieceCounts[SILV][0] == b.pieceCounts[SILV][0])
    {
      int nsNotSpentOn;
      int nsCapture;
      loc_t freeCapLoc;
      if(SearchUtils::isFreeCapturable(bAfter,move,nsNotSpentOn,nsCapture,freeCapLoc))
      {
        if(nsNotSpentOn <= 1) //TODO to preserve old behavior, change this
        {
          int fc = (nsNotSpentOn == 0 ? nsCapture + 1 : nsCapture - 2);
          DEBUGASSERT(fc >= 0 && fc < 6)
          (*handleFeature)(fset.get(IS_FREE_CAPTURABLE,fc),handleFeatureData);
        }
      }
    }

    if(BoardHistory::occurredEarlier(b,hist,bAfter))
      (*handleFeature)(fset.get(REMAKES_EARLIER_MOVE),handleFeatureData);
  }

  if(ENABLE_LAST)
  {
    int lastLastTurnNum = b.turnNumber-2;
    if(lastLastTurnNum >= hist.minTurnNumber)
    {
      for(int i = 0; i<data.numLastPushed; i++)
      {
        loc_t lastPushed = data.lastPushed[i];
        if(b.owners[lastPushed] == pla && (bAfter.owners[lastPushed] != pla || bAfter.pieces[lastPushed] != b.pieces[lastPushed]))
        {
          (*handleFeature)(fset.get(MOVES_LAST_PUSHED),handleFeatureData);
          break;
        }
      }

      int ns = numStepsInMove(move);
      int lastNearness = 0;
      int lastLastNearness = 0;
      for(int i = 0; i<ns; i++)
      {
        step_t s = getStep(move,i);
        if(s == PASSSTEP)
          break;
        loc_t loc = gDest(s);
        lastNearness += data.lastMoveSum[loc];
        lastLastNearness += data.lastLastMoveSum[loc];
      }
      if(lastNearness >= 64)
        lastNearness = 63;
      if(lastLastNearness >= 64)
        lastLastNearness = 63;

      (*handleFeature)(fset.get(MOVES_NEAR_LAST,lastNearness),handleFeatureData);
      (*handleFeature)(fset.get(MOVES_NEAR_LAST_LAST,lastLastNearness),handleFeatureData);
    }
  }

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

    DEBUGASSERT((numTotal > 0 && numTotal <= 4) || (b.step != 0 && numTotal == 0));
    if(numTotal > 0)
      (*handleFeature)(fset.get(STEP_INDEPENDENCE,numTotal-1,numIndepT1-1,numIndepT2-1,numIndepT3-1),handleFeatureData);
  }

  if(ENABLE_RELTACTICS)
  {
    if(data.relTactic0SHashes.find(bAfter.sitCurrentHash) != data.relTactic0SHashes.end())
      (*handleFeature)(fset.get(RELTACTICSDIST,0),handleFeatureData);
    else if(data.relTactic1SHashes.find(bAfter.sitCurrentHash) != data.relTactic1SHashes.end())
      (*handleFeature)(fset.get(RELTACTICSDIST,1),handleFeatureData);
  }

  /*
  if(IS_PATTERN_INITIALIZED)
  {
    int ns = numStepsInMove(move);
    TempRecord recs[ns];

    int i;
    for(i = 0; i<ns; i++)
    {
      step_t step = getStep(move,i);
      if(step == PASSSTEP || step == ERRSTEP || step == QPASSSTEP)
        break;

      loc_t k0 = gSrc(step);
      int feature0 = ptree->lookup(b,pla,k0,data.pStronger);
      if(feature0 >= 0)
        (*handleFeature)(fset.get(PATTERN,feature0),handleFeatureData);

      loc_t k1 = gDest(step);
      int feature1 = ptree->lookup(b,pla,k1,data.pStronger);
      if(feature1 >= 0)
        (*handleFeature)(fset.get(PATTERN,feature1),handleFeatureData);

      recs[i] = b.tempStepC(k0,k1);
    }
    i--;
    for(; i>=0; i--)
    {
      b.undoTemp(recs[i]);
    }
  }*/
}

//HELPERS-----------------------------------------------------------------

//0 = back, 1 = outside, 2 = inside, 3 = front
static int getSymDir(pla_t pla, loc_t loc, loc_t adj)
{
  int diff = adj-loc;
  if(pla == GOLD)
  {
    if(diff == S) return 0;
    else if(diff == N) return 3;
  }
  else
  {
    if(diff == N) return 0;
    else if(diff == S) return 3;
  }

  int x = gX(loc);
  if((x < 4 && diff == W) || (x >= 4 && diff == E))
    return 1;

  return 2;
}


//POSDATA----------------------------------------------------------------------

static void fillPosDataHashes(Board& b, pla_t pla, MoveFeaturePosData& data, bool nonRelTacticsLeft)
{
  //Terminate if you've generated for as many steps as you're allowed, or the turn has switched.
  if(b.player != pla)
  {
    if(nonRelTacticsLeft)
      data.relTactic0SHashes.insert(b.sitCurrentHash);
    data.relTactic1SHashes.insert(b.sitCurrentHash);
    return;
  }
  move_t mv[1024];

  int num = SearchMoveGen::genRelevantTactics(b,true,mv,NULL);
  if(b.step != 0)
    mv[num++] = PASSMOVE;

  int numRelTactics = num;

  if(nonRelTacticsLeft)
  {
    if(b.step < 3)
      num += BoardMoveGen::genPushPulls(b,pla,mv+num);
    num += BoardMoveGen::genSteps(b,pla,mv+num);
  }

  DEBUGASSERT(num < 1024);

  UndoMoveData uData;
  for(int i = 0; i<num; i++)
  {
    move_t move = mv[i];
    b.makeMove(move,uData);
    bool newNonRelTacticsLeft = i >= numRelTactics ? false : nonRelTacticsLeft;
    fillPosDataHashes(b,pla,data,newNonRelTacticsLeft);
    b.undoMove(uData);
  }
}

static void* getPosData(Board& b, const BoardHistory& hist, pla_t pla, bool dependOnAdvancement)
{
  MoveFeaturePosData* dataBuf = new MoveFeaturePosData();
  MoveFeaturePosData& data = *dataBuf;
  pla_t opp = gOpp(pla);

  data.board = b;

  if(ENABLE_CAP_DEF)
  {
    Bitmap pStrongerMaps[2][NUMTYPES];
    b.initializeStrongerMaps(pStrongerMaps);
    UFDist::get(b,data.ufDist,pStrongerMaps);

    data.numOppCapThreats = 0;
    int oppRDSteps[4] = {16,16,16,16};
    for(int j = 0; j<4; j++)
    {
      loc_t kt = Board::TRAPLOCS[j];
      data.numOppCapThreats += Threats::findCapThreats(b,opp,kt,data.oppCapThreats+data.numOppCapThreats,data.ufDist,pStrongerMaps,
          4,Threats::maxCapsPerPla-data.numOppCapThreats,oppRDSteps[j]);
    }
  }

  if(ENABLE_GOAL_THREAT)
  {
    data.plaGoalDist = BoardTrees::goalDist(b,pla,4);
    data.oppGoalDist = BoardTrees::goalDist(b,opp,4);
  }

  if(ENABLE_GOAL_DIST)
  {
    Threats::getGoalDistBitmapEst(b,pla,data.goalDistIs[pla]);
    Threats::getGoalDistBitmapEst(b,opp,data.goalDistIs[opp]);

    for(int i = 0; i<BSIZE; i++)
      data.goalDist[pla][i] = 9;
    for(int i = 0; i<BSIZE; i++)
      data.goalDist[opp][i] = 9;

    for(int i = 8; i>=0; i--)
    {
      Bitmap map = data.goalDistIs[pla][i];
      while(map.hasBits())
        data.goalDist[pla][map.nextBit()] = i;
    }
    for(int i = 8; i>=0; i--)
    {
      Bitmap map = data.goalDistIs[opp][i];
      while(map.hasBits())
        data.goalDist[opp][map.nextBit()] = i;
    }

    for(int i = 0; i<BSIZE; i++)
    {
      data.nearAdvancedRabLoc[GOLD][i] = ERRLOC;
      data.nearAdvancedRabLoc[SILV][i] = ERRLOC;
    }
    for(int y = 0; y<8; y++)
    {
      for(int x = 0; x<8; x++)
      {
        loc_t loc = gLoc(x,y);
        for(int p = 0; p <= 1; p++)
        {
          Bitmap gRabMap = b.pieceMaps[p][RAB];
          loc_t bestRLoc = ERRLOC;
          int bestScore = 0;
          while(gRabMap.hasBits())
          {
            loc_t rloc = gRabMap.nextBit();
            if(Board::manhattanDist(loc,rloc+Board::GOALLOCINC[p]) > 3 || data.goalDist[p][rloc] > 7)
              continue;
            int score = 100 - 2*data.goalDist[p][rloc] - Board::manhattanDist(loc,rloc) - Board::GOALYDIST[p][rloc];
            if(score > bestScore)
            {bestScore = score; bestRLoc = rloc;}
          }
          if(Board::GOALYDIST[p][bestRLoc] <= 0)
            continue;
          data.nearAdvancedRabLoc[p][loc] = bestRLoc;
        }
      }
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

  b.initializeStronger(data.pStronger);
  b.initializeStrongerMaps(data.pStrongerMaps);

  for(piece_t piece = RAB; piece <= ELE; piece++)
  {
    data.pieceIndex[GOLD][piece] = ArimaaFeature::getPieceIndexApprox(GOLD,piece,data.pStronger);
    data.pieceIndex[SILV][piece] = ArimaaFeature::getPieceIndexApprox(SILV,piece,data.pStronger);
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

  if(ENABLE_TRAPSTATE)
  {
    for(int i = 0; i<4; i++)
    {
      data.oldTrapStates[SILV][i] = ArimaaFeature::getTrapState(b,SILV,Board::TRAPLOCS[i]);
      data.oldTrapStates[GOLD][i] = ArimaaFeature::getTrapState(b,GOLD,Board::TRAPLOCS[i]);
    }
  }


  if(ENABLE_LAST)
  {
    data.numLastPushed = 0;
    int lastTurnNum = b.turnNumber-1;
    if(lastTurnNum >= hist.minTurnNumber && lastTurnNum <= hist.maxTurnBoardNumber)
    {
      loc_t src2[8];
      loc_t dest2[8];
      int num2 = hist.turnBoard[lastTurnNum].getChanges(hist.turnMove[lastTurnNum],src2,dest2);
      for(int i = 0; i<num2; i++)
      {
        if(dest2[i] != ERRLOC && hist.turnBoard[lastTurnNum].owners[src2[i]] == pla)
          data.lastPushed[data.numLastPushed++] = dest2[i];
      }
      for(int y = 0; y<8; y++)
      {
        for(int x = 0; x<8; x++)
        {
          int i = gLoc(x,y);
          data.lastMoveSum[i] = 0;
          data.lastLastMoveSum[i] = 0;
        }
      }

      {
        move_t move = hist.turnMove[lastTurnNum];
        int ns = numStepsInMove(move);
        for(int i = 0; i<ns; i++)
        {
          step_t s = getStep(move,i);
          if(s == PASSSTEP)
            break;
          loc_t loc = gDest(s);
          for(int j = 0;true;j++)
          {
            loc_t next = Board::SPIRAL[loc][j];
            int dist = Board::manhattanDist(loc,next);
            if(dist >= 4)
              break;
            data.lastMoveSum[next] += 4-dist;
          }
        }
      }

      int lastLastTurnNum = lastTurnNum-1;
      if(lastLastTurnNum >= hist.minTurnNumber)
      {
        move_t move = hist.turnMove[lastLastTurnNum];
        int ns = numStepsInMove(move);
        for(int i = 0; i<ns; i++)
        {
          step_t s = getStep(move,i);
          if(s == PASSSTEP)
            break;
          loc_t loc = gDest(s);
          for(int j = 0;true;j++)
          {
            loc_t next = Board::SPIRAL[loc][j];
            int dist = Board::manhattanDist(loc,next);
            if(dist >= 4)
              break;
            data.lastLastMoveSum[next] += 4-dist;
          }
        }
      }
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

  if(ENABLE_RELTACTICS)
  {
    fillPosDataHashes(b,pla,data,true);
  }

  if(dependOnAdvancement)
  {
    data.parallelWeights.resize(NUM_PARALLEL_WEIGHTS);
    double advancement = ArimaaFeature::advancementProperty(b);
    double advancementNoCaps = ArimaaFeature::advancementNoCapsProperty(b);

    data.parallelWeights[0] = 1.0;

    double advancementEarlyProp =
        (advancement <= 36) ? (1.0) :
        (advancement >= 90) ? (0.0) :
                              (1.0 - (advancement - 36.0) / (90.0 - 36.0));
    double advancementLateProp =
        (advancement <= 74) ? (0.0) :
        (advancement >= 150) ? (1.0) :
                               ((advancement - 74.0) / (150.0 - 74.0));
    data.parallelWeights[1] = advancementEarlyProp * 1.5 - 0.5;
    data.parallelWeights[2] = advancementLateProp * 1.5 - 0.5;

    double advancementNoCapsProp =
        (advancementNoCaps <= 36) ? (0.0) :
        (advancementNoCaps >= 84) ? (1.0) :
                                    ((advancementNoCaps - 36.0) / (84.0 - 36.0));
    data.parallelWeights[3] = advancementNoCapsProp * 2 - 1;
  }
  else
  {
    data.parallelWeights.resize(1);
    data.parallelWeights[0] = 1.0;
  }

  return dataBuf;
}

static void* getPosDataNotDependingOnAdvancement(Board& b, const BoardHistory& hist, pla_t pla)
{
  return getPosData(b,hist,pla,false);
}
static void* getPosDataDependingOnAdvancement(Board& b, const BoardHistory& hist, pla_t pla)
{
  return getPosData(b,hist,pla,true);
}

static void freePosData(void* dataBuf)
{
  MoveFeaturePosData* dataPtr = (MoveFeaturePosData*)(dataBuf);
  delete dataPtr;
}

static void getParallelWeights(const void* dataBuf, vector<double>& buf)
{
  const MoveFeaturePosData* dataPtr = (const MoveFeaturePosData*)(dataBuf);
  size_t size = dataPtr->parallelWeights.size();
  if(buf.size() != size)
    buf.resize(size);

  for(size_t i = 0; i<size; i++)
    buf[i] = dataPtr->parallelWeights[i];
}

const FeatureSet& MoveFeature::getFeatureSet()
{
  return fset;
}

ArimaaFeatureSet MoveFeature::getArimaaFeatureSet()
{
  (void)getPosDataNotDependingOnAdvancement;
  //return ArimaaFeatureSet(&fset,getFeatures,getPosDataNotDependingOnAdvancement,freePosData,
  //    1,1,getParallelWeights,getPriorLogProb,doesCoeffMatterForPrior);
  return ArimaaFeatureSet(&fset,getFeatures,getPosDataDependingOnAdvancement,freePosData,
      NUM_PARALLEL_WEIGHTS,getParallelWeights,getPriorLogProb,doesCoeffMatterForPrior);
}








