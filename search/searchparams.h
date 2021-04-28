/*
 * searchparams.h
 *
 *  Created on: Jun 2, 2011
 *      Author: davidwu
 */

#ifndef SEARCHPARAMS_H_
#define SEARCHPARAMS_H_

#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/learner.h"
#include "../learning/featurearimaa.h"
#include "../eval/evalparams.h"
#include "../search/searchflags.h"

struct SearchStats;

using namespace std;

struct SearchParams
{
  //CONSTANT==============================================================================
  //Allow the various features that contribute search instablity?
  //Disabling this will probably cause a tremendous slowdown
  static const bool ALLOW_UNSTABLE = true;

  //MULTITHREADING----------------------------------------------------------
  static const int MAX_THREADS = 32; //Max number of threads allowed

  //ARRAY SIZES AND ALLOC-----------------------------------------------------
  //In addtion, syncWithSplitPointDistant might need work in this case (but maybe not - think about it)
  static const int PV_ARRAY_SIZE = 96; //Size of PV arrays for principal variation storage
  static const int MSEARCH_MOVE_CAPACITY = 768; //How big to ensure the arrays for msearch, per splitpoint?
  static const int QSEARCH_MOVE_CAPACITY = 2048; //How big to ensure the arrays for qsearch, per fdepth?

  //Used in allocating arrays by searchthread.cpp, taking into account extensions
  static const int MAX_MSEARCH_CDEPTH_FACTOR4_OVER_NOMINAL = 8; //Divided by 4
  static const int MAX_MSEARCH_CDEPTH_OVER_NOMINAL; //How much deeper could an msearch go beyond the nominal depth passed in at the top level?
  static const int QMAX_FDEPTH;     //Maximum number of steps that qsearch can go deep (for allocating arrays, etc)
  static const int QMAX_CDEPTH;     //Maximum number of steps that qsearch can go deep (for allocating arrays, etc)

  //LIMITS-------------------------------------------------------------------
  static constexpr double AUTO_TIME = 30*24*60*60; //The max time to search, and the default, 30 days
  static const int AUTO_DEPTH = 96; //The depth to search if unsecified
  static const int DEPTH_DIV = 4; //The factor by which to multiply/divide depths

  //TIME CHECK----------------------------------------------------------------
  static const int TIME_CHECK_PERIOD = 4096; //Check time only every this many mnodes or qnodes

  //HASH TABLE----------------------------------------------------------------
  static const bool HASH_ENABLE = true;
  static const int DEFAULT_MAIN_HASH_EXP = 24; //Size of hashtable is 2**MAIN_HASH_EXP
  static const bool HASH_WL_UNSAFE = false && ALLOW_UNSTABLE; //Return hashtable proven win/losses even if not bounding alpha/beta.
  static const bool HASH_NO_USE_QBM_IN_MAIN = false; //Don't use qsearch best moves in main search
  static const int DEFAULT_FULLMOVE_HASH_EXP = 21; //Size of hashtable for finding full moves at root is 2**FULLMOVE_HASH_EXP

  //For middle-of-turn hash entries, mix in some startPosHash as well to avoid conflating
  //two situations whose situation hash is identical but whose legal moves are different because
  //the steps so far are different.
  static const bool STRICT_HASH_SAFETY = true;

  //QUIESCENCE-----------------------------------------------------------------
  static const bool Q_ENABLE = true;

  //Table indicating how qdepth transitions
  static const int QDEPTH_NEXT[5];  //Next qdepth given the current qdepth (switch to it when the player changes)
  static const int QDEPTH_NEXT_IF_QPASSED[5]; //Next qdepth given the current depth if a pass or qpass was made.
  static const int QDEPTH_EVAL;     //Stop search and eval at this qdepth
  static const int QDEPTH_START[4]; //Maps b.step -> which qdepth to start on
  static const int Q0PASS_BONUS[5]; //Maps numstepspassed -> eval bonus for passing, in qdepth 0
  static const int STEPS_LEFT_BONUS[5]; //Maps numstepsleft -> bonus for player in eval (qdepth2, after qpass)

  static const bool QREVERSEMOVE_ENABLE = true && ALLOW_UNSTABLE;

  //Search regular quiescence moves in addition to windefsearch shortest defenses in qsearch?
  static const bool ALSOQ_ENABLE = false;

  //EXTENSIONS-----------------------------------------------------------------
  //Also controlled by a dynamic parmeter
  static const bool GOAL_EXTENSION_ENABLE = true;

  //PRUNING AND EXTRAEVAL-----------------------------------------------------
  static const bool REVERSE_ENABLE = true && ALLOW_UNSTABLE;
  static const bool ENABLE_REP_FIGHT_CHECK = true && ALLOW_UNSTABLE;
  static const bool FREECAP_PRUNE_ENABLE = true && ALLOW_UNSTABLE;

  static const eval_t FULL_REVERSE_PENALTY = 20000; //For 4-2 reversibles and similar
  static const eval_t PARTIAL_REVERSE_PENALTY_ROOT = 150; //For 3-1 reversibles and similar, root level
  static const eval_t PARTIAL_REVERSE_PENALTY_TREE = 300; //For 3-1 reversibles and similar, within search

  static const eval_t LOSING_REP_FIGHT_PENALTY = 10000; //Penalize moves at the root that lose repetition fights

  static const int DEPTH_ADJ_PRUNE = -65536; //For view purposes, the depth adjustment that signifies a pruned move

  static const int REDUCEFAIL_THRESHOLD = 10000;
  static const int ROOT_REDUCEFAIL_PER_FAIL = 4000;
  static const int ROOT_REDUCEFAIL_PER_SUCCESS = 50;
  static const int ROOT_CAP_REDUCEFAIL_WHEN_ALPHA_IMPROVES = 8000;

  //MISC-----------------------------------------------------------------
  //Turn up to which to avoid early trades and horse attacks
  static const int EARLY_TRADE_TURN_MAX = 4;
  static const eval_t EARLY_TRADE_PENALTY = 1000;
  static const int EARLY_HORSE_ATTACKED_MAX = 4;
  static const eval_t EARLY_HORSE_ATTACKED_PENALTY = 250;
  //Turn up to which to avoid early blockades.
  static const int EARLY_BLOCKADE_TURN_FULL = 16;
  static const int EARLY_BLOCKADE_TURN_HALF = 24;
  static const eval_t EARLY_BLOCKADE_PENALTY = 1000;
  //Penalty if the elephant is on one of the central squares in the mainboard and manages to become blockaded
  static const eval_t EARLY_CENTER_BLOCKADE_PENALTY = 1500;

  //Default factor by which to multiply root ordering value to bias the root eval
  static constexpr double DEFAULT_ROOT_BIAS = 0.0;
  //Factor to use when pondering opponent's move and trying to maximise probability of guess
  static constexpr double PONDER_ROOT_BIAS = 500.0;

  //Default amount to randomize, if randomizing.
  static const eval_t DEFAULT_RANDOM_DELTA = 20;

  //ORDERING---------------------------------------------------------------------
  static const bool KILLER_ENABLE = true && ALLOW_UNSTABLE;
  static const bool QKILLER_ENABLE = true && ALLOW_UNSTABLE;

  static const int NULL_SCORE = 0x5FFFFFFF; //Msearch only, score for null move
  static const int HASH_SCORE = 0x3FFFFFFF; //Shared
  static const int KILLER_SCORE0 = 10000000; //Shared
  static const int KILLER_SCORE1 = 9900000;  //Shared
  static const int KILLER_SCORE_DECREMENT = 100000; //Msearch only - subtract this for killers 2,3,4,5,...

  static const int HASH_MOVE_PREFIX_SCORE = 2000000; //Msearch only
  static const int KILLER0_PREFIX_SCORE = 1800000; //Unused
  static const int KILLER1_PREFIX_SCORE = 1600000; //Unused
  static const int QPASS_SCORE = 160000; //Qsearch only - for some reason, ordering this high makes things faster

  static const int CAPTURE_SCORE = 20000;              //Msearch only
  static const int CAPTURE_SCORE_PER_MATERIAL = 12000; //Msearch only

  //Shared
  static const bool HISTORY_ENABLE = true;
  static const int HISTORY_LEN = 256*5; //[256 pla steps, 256*4 steps+push/pull dir]
  static const int HISTORY_SCORE_MAX = 10000;

  static const int MOVEWEIGHTS_SCALE = 50; //Amount to multiply bt move weights by

  //PVS-------------------------------------------------------------------------
  static const bool PVS_ENABLE = true;

  //NULL MOVE PRUNING-----------------------------------------------------------
  static const int NULLMOVE_REDUCE_DEPTH_BORDERLINE = 1;
  static const int NULLMOVE_REDUCE_DEPTH = 1;
  static const int NULLMOVE_MIN_RDEPTH = 5;

  //MISC-----------------------------------------------------------------------
  //If we're in the same situation as earlier in the game (the opponent reversed us)
  //then imediately make the same move we did before if it's legal.
  static const bool IMMEDIATELY_REMAKE_REVERSED_MOVE = true;


  //PARAMETERS============================================================================

  //NAME-------------------------------------------------------------------------
  string name;

  //MULTITHREADING---------------------------------------------------------------
  int numThreads; //Default = 1, Number of threads

  //OUTPUT-----------------------------------------------------------------------
  ostream* output; //Default = cout

  //SEARCH PARAMS ---------------------------------------------------------------
  int defaultMaxDepth;   //Default = AUTO_DEPTH, Default max depth to search,
  double defaultMaxTime; //Default = AUTO_TIME, Default max time to search,

  bool randomize;    //Default = false, Randomize among equal branches and enable randDelta as well
  eval_t randDelta;  //  Add random vals in [-randDelta,randDelta] three times to evals. Gives a bell-shaped curve with stdev ~ randDelta.
  uint64_t randSeed; //  Random seed for any randomization

  bool disablePartialSearch; //Default = false, If this is set, don't accept a new IDPV until the *entire* the next iterative deepen is done.
  bool moveImmediatelyIfOnlyOneNonlosing; //Default = false, Try to move immediately if there's only one move that doesn't instantly lose.
  bool avoidEarlyTrade; //Default = false, Try to avoid early trades, if board.turn is low.
  bool avoidEarlyBlockade; //Default = false, Try to avoid early blockades, if board.turn is low
  bool overrideMainPla; //Default = false, use normal asymmetric evaluation, such as for blockades and mobility
  pla_t overrideMainPlaTo; //If overrideMainPla is set, override mainpla to this for asymmetric evaluation (NPLA disables it)
  bool allowReduce; //Default = true, Allow reducing depth like LMR

  //These two parameters need to be set BEFORE creating the searcher!!
  int fullMoveHashExp; //Size of hashtable for root move generation is 2**this, defaults to DEFAULT_FULLMOVE_HASH_EXP
  int mainHashExp; //Size of main hashtable is 2**this, defaults to DEFAULT_MAIN_HASH_EXP

  //Enable qsearch?
  bool qEnable; //Default = true
  //Enable extensions?
  bool extensionEnable; //Default = true

  //ROOT MOVE ORDERING AND PRUNING-----------------------------------------------

  ArimaaFeatureSet rootMoveFeatureSet; //Default = no features
  vector<vector<double>> rootMoveFeatureWeights;

  //Don't make any root moves leading to the following hashes
  vector<hash_t> excludeRootMoves;
  //Don't make any root moves that match a given move_t where a match consists of containing all non-pass steps in the move_t in any order.
  vector<move_t> excludeRootStepSets;

  //Always leave this many moves unpruned at least - only prune after the first this many
  int rootMinDontPrune;
  //Enable type of root pruning
  bool rootFancyPrune;
  bool rootFixedPrune;
  bool stupidPrune;
  //Fixed pruning
  double rootPruneAllButProp;
  bool rootPruneSoft;
  //Add this much times the root move predictor value as an extra eval to bias the root
  double rootBiasStrength;

  //For experimental testing, restricts root move generation to only "relevant" tactics
  int rootMaxNonRelTactics;

  //Tree pruning
  bool treePrune;

  //Enable unsafe move pruning techniques
  bool unsafePruneEnable;  //Default = false
  bool nullMoveEnable;     //Default = false

  //VIEWING---------------------------------------------------------------------
  bool viewOn;            //Are we viewing a position?
  Board viewStartBoard;   //The board position we want to view
  bool viewPrintMoves;    //Do we want to view the generated moves for the position?
  bool viewPrintBetterMoves; //Do we want to view the generated moves that improved the score?
  bool viewEvaluate;      //Do we want to view the evaluations?
  bool viewExact;         //Do we require that the move sequence to get here is, on a turn-by-turn resolution, exact?
  vector<hash_t> viewHashes; //Indexed by turndiff

  //METHODS---------------------------------------------------------------------

  SearchParams();
  SearchParams(int depth, double time);
  SearchParams(string name);
  SearchParams(string name, int depth, double time);
  ~SearchParams();

  private:
  void init();


  //PARAMETER SPECIFCATION---------------------------------------------------------------------------
  public:

  //Threading---------------------------------------

  //How many parallel threads to use for the search?
  void setNumThreads(int num);

  //Output------------------------------------------

  //Set the output stream used to print things
  void setOutput(ostream* out);

  //Randomization-----------------------------------

  //Do we randomize the pv among equally strong moves?  And do we add any random deltas to the evals?
  //The eval will receive a roughly bell-shaped curve with stdev rDelta
  void setRandomize(bool r, eval_t rDelta, uint64_t seed);

  void setRandomSeed(uint64_t seed);

  //Root move ordering-------------------------------

  //Add features and weights for root move ordering
  void initRootMoveFeatures(const BradleyTerry& bt);

  //Use root move ordering for pruning too. Prune after the first prop of moves
  void setRootFixedPrune(bool b, double prop, bool soft);

  //Use root move ordering for fancier pruning
  void setRootFancyPrune(bool b);

  //Prune even without an ordering
  void setStupidPrune(bool b, double prop);

  //Add features and weights for tree move ordering
  void initTreeMoveFeatures(const BradleyTerry& bt);

  //Add this much times the root move predictor value as an extra eval to bias the root
  void setRootBiasStrength(double bias);

  //Allow various unsafe pruning techniques
  void setUnsafePruneEnable(bool b);
  void setNullMoveEnable(bool b);
  //Allow search extensions
  void setExtensionEnable(bool b);

  //Search options----------------------------------

  //Set the searcher to avoid early trades
  void setAvoidEarlyTrade(bool avoid);
  //Set the searcher to avoid early blockades
  void setAvoidEarlyBlockade(bool avoid);
  //Override the main pla of the searcher used for asymmetric evaluation
  void setOverrideMainPla(bool override, pla_t to);
  //Set the searcher to allow LMR and similar depth reduction
  void setAllowReduce(bool allow);

  //Re-set the default max depth and time, negative indicates unbounded time.
  void setDefaultMaxDepthTime(int depth, double time);

  //Do not use the results of partially completed iterative deepen?
  void setDisablePartialSearch(bool b);

  //Try to move immediately if there's only one move that doesn't instantly lose.
  void setMoveImmediatelyIfOnlyOneNonlosing(bool b);

  //View----------------------------------------------

  //Indicate a board position to examine in the next search.
  void initView(const Board& b, const string& moveStr, bool printBetterMoves, bool printMoves, bool exactHash);


  //USAGE--------------------------------------------------------------------------------------------

  //View----------------------------------------------
  //Utility functions for search
  bool viewing(const Board& b, const BoardHistory& hist) const;
  void viewSearch(const Board& b, const BoardHistory& hist, int cDepth, int rDepth, int alpha, int beta, const SearchStats& stats) const;
  void viewResearch(int cDepth, int rDepth, int alpha, int beta, const SearchStats& stats) const;
  void viewMove(const Board& b, move_t move, int hm, eval_t eval, eval_t prevBestEval,
      bool reSearched, int origDepthAdj, move_t* pv, int pvLen, const SearchStats& stats) const;
  void viewMovePruned(const Board& b, move_t move) const;
  void viewHash(const Board& b, eval_t hashEval, flag_t hashFlag, int hashDepth, move_t hashMove) const;
  void viewFinished(const Board& b, bool changedPlayer, eval_t bestEval, eval_t extraEval,
      move_t bestMove, flag_t bestFlag, const SearchStats& stats) const;
};


#endif /* SEARCHPARAMS_H_ */
