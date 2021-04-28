
/*
 * board.h
 * Author: davidwu
 *
 * Welcome to the board!
 * General notes: copiable, stores current hash but not past state.
 * One player (gold, captial letters) starts at top (small indices) and his rabbits can move down (north, increase indices) and plays first.
 * Zero player (silver, lowercase lettors) starts at bottom (large indices) and his rabbits can move up (south, decrease indices) and plays second.
 *
 * MUST CALL initData() on Board to initalize arrays, before using this class!
 */

#ifndef BOARD_H
#define BOARD_H

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/btypes.h"
#include "../board/offsets.h"

using namespace std;


//TEMP MOVE AND UNDO CLASS-----------------------------------------------------------------------------
//Note! - after making a temp move, none of the bitmaps are updated!
//Nor are the hashes, step counter, etc updated. Do not trust isFrozen or isThawed.

class TempRecord
{
  public:
  loc_t capLoc;
  pla_t capOwner;
  piece_t capPiece;
  loc_t k0;
  loc_t k1;

  TempRecord()
  {}

  TempRecord(loc_t x, loc_t y)
  {
    k0 = x;
    k1 = y;
    capLoc = ERRLOC;
    capOwner = NPLA;
    capPiece = EMP;
  }
};

class UndoData
{
  public:
  step_t s;
  hash_t posStartHash;
  hash_t posCurrentHash;
  hash_t sitCurrentHash;
  int step;
  int turn;
  Bitmap dominatedMap;
  Bitmap frozenMap;
  loc_t caploc;
  pla_t capowner;
  piece_t cappiece;

  UndoData()
  {}
};

class UndoMoveData
{
  public:
  UndoData uData[4];
  int size;
  UndoMoveData()
  {}
};


//PRIMARY CLASS----------------------------------------------------------------------------------------

class Board
{
  public:

  //MAIN DATA--------------------------------------
  int8_t _ownersBufferSpace[BSTARTIDX];
  int8_t owners[BBUFSIZE-BSTARTIDX];
  int8_t _piecesBufferSpace[BSTARTIDX];
  int8_t pieces[BBUFSIZE-BSTARTIDX];

  Bitmap pieceMaps[NUMPLAS][NUMTYPES];  //Bitmaps for each piece, 0 = all
  Bitmap dominatedMap;                  //Bitmap for dominated pieces
  Bitmap frozenMap;                     //Bitmap for frozen pieces

  int8_t player;   //Next player (0-1)
  int8_t step;      //Step phase (0-3)
  int turnNumber; //Number of turns finished in total up to this point (0 after setup)

  int8_t pieceCounts[2][NUMTYPES];  //Number of each type of piece, 0 = total
  int8_t trapGuardCounts[2][4]; //Number of pieces guarding each trap for each player

  //ZOBRIST HASHING--------------------------------
  hash_t posStartHash;    //Hash value for the situation (position + player to move + step) at the start of the move (last move if step = 0)
  hash_t posCurrentHash;  //Current hash value for the position (position only)
  hash_t sitCurrentHash;  //Current hash value for the situation (position + player to move + step)

  //GOAL TREE--------------------------------------
  move_t goalTreeMove;  //Move reported by the goal tree stored here

  //STATIC ARRAYS----------------------------------
  //TODO experiment with some of these being int or uint32_t rather than int8 for faster access?
  static hash_t HASHPIECE[2][NUMTYPES][BSIZE]; //Hash XOR'ed for piece on that position
  static hash_t HASHPLA[2];                 //Hash XOR'ed by player turn
  static hash_t HASHSTEP[4];                //Hash XOR'ed by step

  static const bool ISEDGE[BSIZE];           //[loc]: Is this square at the edge?
  static const bool ISEDGE2[BSIZE];         //[loc]: Is this square next to squares at the edge?
  static const int EDGEDIST[BSIZE];         //[loc]: How far is this from the edge?

  static Bitmap XINTERVAL[8][8];      //[x1][x2]: All squares with x coordinate in [x1,x2] or [x2,x1]
  static Bitmap YINTERVAL[8][8];      //[y1][y2]: All squares with y coordinate in [y1,y2] or [y2,y1]

  static const bool ISTRAP[BSIZE];           //[loc]: Is this square a trap?
  static const int8_t TRAPLOCS[4];        //[index]: What locations are traps?
  static const int TRAPINDEX[BSIZE];        //[loc]: What index is this trap? ERRLOC for nontrap locations.
  static const int8_t TRAPDEFLOCS[4][4];  //[index][index2]: What locations are defenses to traps?
  static const int PLATRAPINDICES[2][2]; //[pla][index]: What trapindices are traps on pla's side?
  static const int8_t PLATRAPLOCS[2][2];  //[pla][index]: What locations are traps on pla's side?
  static const bool ISPLATRAP[4][2];     //[trapID][pla]: Is this a trap on pla's side?
  static const int8_t CLOSEST_TRAP[BSIZE];   //[loc]: What is the closest trap to this location?
  static const int CLOSEST_TRAPINDEX[BSIZE];//[loc]: What is the closest trap's index to this location?
  static const int CLOSEST_TDIST[BSIZE];    //[loc]: What is the distance to the closest trap to this location?

  static const int8_t TRAPBACKLOCS[4];    //[index]: What is the square behind this trap?
  static const int8_t TRAPOUTSIDELOCS[4]; //[index]: What is the square on the outer side this trap?
  static const int8_t TRAPINSIDELOCS[4];  //[index]: What is the square on the inner side this trap?
  static const int8_t TRAPTOPLOCS[4];     //[index]: What is the square on the top of this trap?

  static const int8_t ADJACENTTRAP[BSIZE];  //[loc]: The trap location adjacent, else ERRLOC if none
  static const int ADJACENTTRAPINDEX[BSIZE]; //[loc]: The trap index adjacent, else -1 if none
  static const int8_t RAD2TRAP[BSIZE];      //[loc]: The trap location at exactly radius 2, else ERRLOC if none

  static const int ADJOFFSETS[4];        //[index]: What offsets should be added to get the adjacent location in each direction?
  static const bool ADJOKAY[4][BSIZE];   //[index][loc]: Is it safe to take ADJOFFSETS[index] from the given square (not go off board)?
  static const int STEPOFFSETS[4];       //[index]: What offsets should be added to indicate a step in each direction?

  static const int STARTY[2];            //[pla]: Y coordinate of back starting row for player pla
  static const int GOALY[2];             //[pla]: Y coordinate of goal for player pla
  static const int GOALYINC[2];          //[pla]: Direction of advancement for player pla (+1 or -1)
  static const int GOALLOCINC[2];        //[pla]: Offset to add for advancement for player pla ( N or  S)
  static const bool ISGOAL[2][BSIZE];       //[pla][loc]: Is this square a goal for this player?
  static const int GOALYDIST[2][BSIZE];     //[pla][loc]: What is the vertical distance from this loc to goal?
  static const Bitmap PGOALMASKS[8][2];     //[index][pla]: From what locations can rabbits attempt to goal in n steps, for each player?
  static Bitmap GOALMASKS[2];               //[pla]: Where are the goals?

  //These are the squares that, if these squares stayed the same, the rabbit at the loc would still be able to goal.
  //They are NOT the all squares that one might interact with to stop goal, due to traps.
  static Bitmap GPRUNEMASKS[2][BSIZE][5];   //[pla][loc][gdist]: What are the possible locs that matter to stop pla goal?

  static const int ADVANCEMENT[2][BSIZE];   //[pla][loc]: Array indicating roughly how 'advanced' a region is for a player
  static const int ADVANCE_OFFSET[2];       //[pla]: What is the offset corresponding to moving forward for this player?
  static const int CENTERDIST[BSIZE];       //[loc]: Distance from the center

  static bool ISADJACENT[DIFFIDX_SIZE];     //[loc1-loc2 + DIFFIDX_ADD]: Are the two given locations adjacent?
  static int MANHATTANDIST[DIFFIDX_SIZE];   //[loc1-loc2 + DIFFIDX_ADD]: What is the manhattan distance between two locations?
  static int DIRTORFAR[DIFFIDX_SIZE];       //[src-dest + DIFFIDX_ADD]: The dir from src to dest, shrinking the farther distance first, horizontal bias if equal
  static int DIRTORNEAR[DIFFIDX_SIZE];      //[src-dest + DIFFIDX_ADD]: The dir from src to dest, shrinking the nearer distance first, vertical bias if equal
  static int8_t SPIRAL[BSIZE][64];         //[loc1][index]: For every loc, a list of every other loc by increasing manhattan distance
  static Bitmap RADIUS[16][BSIZE];          //[rad][loc]: For each location, the set of locations at a given manhattan radius from it
  static Bitmap DISK[16][BSIZE];            //[rad][loc]: For each location, the set of locations leq to a given manhattan radius from it

  static const int NUMSYMLOCS = 32;
  static const int NUMSYMDIR = 3;
  static const int SYMDIR_FRONT = 0;
  static const int SYMDIR_BACK = 1;
  static const int SYMDIR_SIDE = 2;
  static const int8_t PSYMLOC[2][BSIZE];     //[pla][loc]: What is a symmetrical player-indepdendent index for this location?
  static const int8_t SYMLOC[2][BSIZE];      //[pla][loc]: What is a left-right symmetrical player-indepdendent index for this location?
  static const int SYMTRAPINDEX[2][4];   //[pla][trapID]: What is a player-indepdendent index for this trap?
  static const int PSYMDIR[2][4];        //[pla][dir]: What is a player-independent direction?
  static const int SYMDIR[2][4];         //[pla][dir]: What is a player-independent left-right independent direction?

  //INITIALIZATION------------------------------------------------------------------

  //Initialize all the necessary data. Call this first before using this class!
  static void initData();

  //CONSTRUCTOR---------------------------------------------------------------------

  //Constructs an empty board with no pieces
  Board();

  //Copy and assignment
  Board(const Board& other);
  void operator=(const Board& other);

  //SETUP---------------------------------------------------------------------------

  //Note: To set up an initial position, call these methods as needed to create the board configuration and
  //then call refreshStartHash.

  //Set the number of turns that have finished since setup.
  void setTurnNumber(int num);
  //Set the player and the step, updating the situation hash only. Does NOT update the position start hash.
  void setPlaStep(pla_t p, char s);
  //Set a piece, updating all the bitmaps and hashes, except the position start hash.
  //Does NOT resolve trap captures - assumes that it is valid or that another piece will be set afterwards to defend.
  void setPiece(loc_t k, pla_t owner, piece_t piece);
  //Set the position start hash to the current position hash.
  void refreshStartHash();


  //MOVEMENT------------------------------------------------------------------------

  //Perform a basic legality check. Checks that the step is actually a valid index, that the source has a piece,
  //that the destination is empty. If owned by the current player, checks that the source piece is not a retreating
  //rabbit or a frozen piece. Does NOT check pushpulling constraints.
  //PASSSTEP and QPASSSTEP are always legal.
  bool isStepLegal(step_t s) const;

  //Make a step, assuming that s is legal. Updates the hashes, step, and player appropriately, and resolves captures.
  //If PASSSTEP, then skips all remaining steps in the turn, swaps the player, and updates everything appropriately.
  //If QPASSSTEP, does nothing.
  void makeStep(step_t s);
  void makeStep(step_t s, UndoData& udata);

  //Make a step, but performs the check isLegal first. If the step is not legal, then does nothing and returns false.
  bool makeStepLegal(step_t s);
  bool makeStepLegal(step_t s, UndoData& udata);

  //Make a move, assuming that it is legal. Makes each step in the move in sequence, terminating if it hits ERRSTEP or PASSSTEP.
  void makeMove(move_t m);
  void makeMove(move_t m, UndoMoveData& udata);
  void makeMove(move_t m, vector<UndoMoveData>& udatas);

  //Make a move, performing normal and pushpull legality checks, without automatic undo on illegality
  //Does NOT check for board repetition or for the fact that a 4 step move must alter the board.
  //Makes each step in the move in sequence, terminating if it hits ERRSTEP or PASSSTEP or QPASSSTEP or an illegal move.
  //All moves up to the point of illegality will still be made!
  //Regardless of legality, uData will be filled with the correct information so that calling undoMove on it will
  //undo the move or partial move.
  //On an illegal move, the uDatas vector will STILL be extended by one.
  bool makeMoveLegalNoUndo(move_t m);
  bool makeMoveLegalNoUndo(move_t m, UndoMoveData& udata);
  bool makeMoveLegalNoUndo(move_t m, vector<UndoMoveData>& udatas);

  //If illegality is inecountered, undoes and restores the board's state, and the vector is NOT extended,
  //although it may still be reallocated/capacity-changed
  bool makeMoveLegal(move_t m, vector<UndoMoveData>& udatas);

  //Same as makeMoveLegal, but makes a sequence of moves [start,end).
  //All moves up to the point of illegality will still be made!
  bool makeMovesLegalNoUndo(const vector<move_t>& moves);
  bool makeMovesLegalNoUndo(const vector<move_t>& moves, vector<UndoMoveData>& udatas);

  //Undo any step made with any of the makeStep variants
  void undoStep(const UndoData& udata);

  //Undo any move made with any of the makeMove variants
  void undoMove(const UndoMoveData& udatas);
  void undoMove(vector<UndoMoveData>& udatas);

  private:
  //Makes a step, assuming that s is legal. Does NOT update the domination map nor the freeze map.
  pla_t makeStepRaw(step_t s, UndoData& udata);
  //Undoes a step
  void undoStepRaw(const UndoData& udata);
  //Updates the domination and freezing bitmaps.
  void recalcDomMapFor(pla_t pla);
  void recalcAllDomAndFreezeMaps();
  //Helper to check step legality in the middle of a larger move
  bool stepLegalWithinMove(int i, step_t s, pla_t pla, loc_t& pushK, loc_t& pullK, piece_t& pushPower, piece_t& pullPower);

  public:

  //MISC--------------------------------------------------------------------------

  //Parses the move into steps in the format of up to 8 src and dest locations, indicating the accumulated movement.
  //The arrays src and dest must be at least 8 steps long, and this functions returns the number of steps
  //in the move.
  //When pieces become captured, their destinations are ERRLOC
  //8 src and dest locations are necessary because we could have 4 moves and 4 captures in one turn in the worst case
  int getChanges(move_t move, loc_t* src, loc_t* dest) const;
  int getChanges(move_t move, loc_t* src, loc_t* dest, int8_t newTrapGuardCounts[2][4]) const;

  //Same, except expects to be called on the board that resulted from making the given move.
  //Will not return any changes from pieces that were captured
  int getChangesInHindsightNoCaps(move_t move, loc_t* src, loc_t* dest) const;

  //Get the direction for the best approach to the dest
  static int getApproachDir(loc_t src, loc_t dest);

  //Get the direction for best retreat from dest. If equal, sets nothing. If only one retreat dir, sets nothing for dir2
  static void getRetreatDirs(loc_t src, loc_t dest, int& dir1, int& dir2);

  //Get the situation hash after the specified move. Fails if the move is illegal
  hash_t sitHashAfterLegal(move_t move) const;

  //END CONDITIONS------------------------------------------------------------------

  pla_t getWinner() const;

  inline bool isGoal(pla_t pla) const
  {return (pieceMaps[pla][RAB] & GOALMASKS[pla]).hasBits();}

  inline bool isRabbitless(pla_t pla) const
  {return pieceMaps[pla][RAB].isEmpty();}

  bool noMoves(pla_t pla) const;

  //MISC----------------------------------------------------------------------------

  //How many opposing pieces are stronger than this piece?
  int numStronger(pla_t pla, piece_t piece) const;

  //How many opposing pieces are the same strength as this piece?
  int numEqual(pla_t pla, piece_t piece) const;

  //How many opposing pieces are weaker than this piece?
  int numWeaker(pla_t pla, piece_t piece) const;

  //Returns the location of the elephant owned by pla, or ERRLOC if none
  loc_t findElephant(pla_t pla) const;

  //Fill arrays with, for each piece type, the number of opp pieces stronger than it
  void initializeStronger(int pStronger[2][NUMTYPES]) const;

  //Fill arrays with, for each piece types, a bitmap of the opp pieces stronger than it
  void initializeStrongerMaps(Bitmap pStrongerMaps[2][NUMTYPES]) const;

  //Find the nearest opponent piece stronger than piece this from loc within maxRad, or ERRLOC if none
  loc_t nearestDominator(pla_t pla, piece_t piece, loc_t loc, int maxRad) const;

  //ERROR CHECKING-----------------------------------------------------------------------

  //Perform a few tests to try to detect if this board's data is inconsistent or corrupt
  bool testConsistency(ostream& out) const;
  static bool areIdentical(const Board& b0, const Board& b1);
  static bool pieceMapsAreIdentical(const Board& b0, const Board& b1);

  //USEFUL CONDITIONS----------------------------

  //Are the two locations adjacent?
  static inline bool isAdjacent(loc_t k, loc_t j)
  {
#ifdef EXTRA_BOUND_CHECKS
    DEBUGASSERT((k & 0x77) == k && (j & 0x77) == j);
#endif
    return ISADJACENT[k-j+DIFFIDX_ADD];
  }

  //Manhattan distance between two locs
  static inline int manhattanDist(loc_t k, loc_t j)
  {
#ifdef EXTRA_BOUND_CHECKS
    DEBUGASSERT((k & 0x77) == k && (j & 0x77) == j);
#endif
    return MANHATTANDIST[k-j+DIFFIDX_ADD];
  }

  //For accessing diffidx squares
  static inline int diffIdx(loc_t src, loc_t dest)
  {
    return src - dest + DIFFIDX_ADD;
  }

  //Is the square a trap?
  inline bool isTrap(loc_t k) const
  {return ISTRAP[k];}

  //Is the square against the edge?
  inline bool isEdge(loc_t k) const
  {return ISEDGE[k];}

  //Is the square against the edge?
  inline bool isEdge2(loc_t k) const
  {return ISEDGE2[k];}

  //Is the piece at square k a rabbit?
  inline bool isRabbit(loc_t k) const
  {return pieces[k] == RAB;}

  //Is the piece at square k frozen?
  inline bool isFrozen(loc_t k) const
  {return frozenMap.isOne(k);}

  //Is the piece at square k unfrozen?
  inline bool isThawed(loc_t k) const
  {return !frozenMap.isOne(k);}

  inline bool isDominated(loc_t k) const
  {return dominatedMap.isOne(k);}

  //Is the piece at square k frozen?
  inline bool isFrozenC(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);

    return
    !(
      (owners[k S] == pla) ||
      (owners[k W] == pla) ||
      (owners[k E] == pla) ||
      (owners[k N] == pla)
    )
    &&
    (
      (owners[k S] == opp && pieces[k S] > pieces[k]) ||
      (owners[k W] == opp && pieces[k W] > pieces[k]) ||
      (owners[k E] == opp && pieces[k E] > pieces[k]) ||
      (owners[k N] == opp && pieces[k N] > pieces[k])
    );
  }

  //Is the piece at square k unfrozen?
  inline bool isThawedC(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);
    return
    (
      (owners[k S] == pla) ||
      (owners[k W] == pla) ||
      (owners[k E] == pla) ||
      (owners[k N] == pla)
    )
    ||
    !(
      (owners[k S] == opp && pieces[k S] > pieces[k]) ||
      (owners[k W] == opp && pieces[k W] > pieces[k]) ||
      (owners[k E] == opp && pieces[k E] > pieces[k]) ||
      (owners[k N] == opp && pieces[k N] > pieces[k])
    );
  }

  //Is the piece at square k adjacent to any stronger opponent?
  inline bool isDominatedC(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);
    return
    (owners[k S] == opp && pieces[k S] > pieces[k]) ||
    (owners[k W] == opp && pieces[k W] > pieces[k]) ||
    (owners[k E] == opp && pieces[k E] > pieces[k]) ||
    (owners[k N] == opp && pieces[k N] > pieces[k]);
  }

  //Is the piece at square k adjacent to any stronger unfrozen opponent?
  inline bool isDominatedByUF(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);
    return
    (owners[k S] == opp && pieces[k S] > pieces[k] && isThawedC(k S)) ||
    (owners[k W] == opp && pieces[k W] > pieces[k] && isThawedC(k W)) ||
    (owners[k E] == opp && pieces[k E] > pieces[k] && isThawedC(k E)) ||
    (owners[k N] == opp && pieces[k N] > pieces[k] && isThawedC(k N));
  }

  //Is the piece at square k adjacent to any weaker opponent piece?
  inline bool isDominating(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);
    return
    (owners[k S] == opp && pieces[k S] < pieces[k]) ||
    (owners[k W] == opp && pieces[k W] < pieces[k]) ||
    (owners[k E] == opp && pieces[k E] < pieces[k]) ||
    (owners[k N] == opp && pieces[k N] < pieces[k]);
  }

  //Return the dominating piece against k if and only if there is exactly one
  inline loc_t findSingleDominator(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);
    int count = 0;
    loc_t found = ERRLOC;
    if(owners[k S] == opp && pieces[k S] > pieces[k]) {count++; found = k S;}
    if(owners[k W] == opp && pieces[k W] > pieces[k]) {count++; found = k W;}
    if(owners[k E] == opp && pieces[k E] > pieces[k]) {count++; found = k E;}
    if(owners[k N] == opp && pieces[k N] > pieces[k]) {count++; found = k N;}
    if(count != 1)
      return ERRLOC;
    return found;
  }

  //Is the square k adjacent to any friendly piece?
  inline bool isGuarded(pla_t pla, loc_t k) const
  {
    return
    (owners[k S] == pla) ||
    (owners[k W] == pla) ||
    (owners[k E] == pla) ||
    (owners[k N] == pla);
  }

  //Is the square k adjacent to 2 or more friendly pieces?
  inline bool isGuarded2(pla_t pla, loc_t k) const
  {
    return
    (owners[k S] == pla) +
    (owners[k W] == pla) +
    (owners[k E] == pla) +
    (owners[k N] == pla)
    >= 2;
  }

  //Is the square k adjacent to 3 or more friendly pieces?
  inline bool isGuarded3(pla_t pla, loc_t k) const
  {
    return
    (owners[k S] == pla) +
    (owners[k W] == pla) +
    (owners[k E] == pla) +
    (owners[k N] == pla)
    >= 3;
  }

  //Atempts to find a piece of pla guarding k. ERRLOC if none.
  inline loc_t findGuard(pla_t pla, loc_t k) const
  {
    if(owners[k S] == pla) return k S;
    if(owners[k W] == pla) return k W;
    if(owners[k E] == pla) return k E;
    if(owners[k N] == pla) return k N;
    return ERRLOC;
  }

  //Atempts to find a piece of pla guarding k ignoring ign. ERRLOC if none.
  inline loc_t findGuard(pla_t pla, loc_t k, loc_t ign) const
  {
    if(k S != ign && owners[k S] == pla) return k S;
    if(k W != ign && owners[k W] == pla) return k W;
    if(k E != ign && owners[k E] == pla) return k E;
    if(k N != ign && owners[k N] == pla) return k N;
    return ERRLOC;
  }

  //Is the square regular or guarded by at least 1 friendly piece?
  inline bool isTrapSafe1(pla_t pla, loc_t k) const
  {
    return !ISTRAP[k] || trapGuardCounts[pla][TRAPINDEX[k]] >= 1;
  }

  //Is the square regular or guarded by at least 2 friendly pieces?
  inline bool isTrapSafe2(pla_t pla, loc_t k) const
  {
    return !ISTRAP[k] || trapGuardCounts[pla][TRAPINDEX[k]] >= 2;
  }

  //Is the square regular or guarded by at least 3 friendly pieces?
  inline bool isTrapSafe3(pla_t pla, loc_t k) const
  {
    return !ISTRAP[k] || trapGuardCounts[pla][TRAPINDEX[k]] >= 3;
  }

  //Can the given piece step anywhere, assuming it is unfrozen?
  inline bool isOpenToStep(loc_t k) const
  {
    return
    (owners[k S] == NPLA && isRabOkayS(owners[k],k)) ||
    (owners[k W] == NPLA) ||
    (owners[k E] == NPLA) ||
    (owners[k N] == NPLA && isRabOkayN(owners[k],k));
  }

  //Can the given piece step anywhere, assuming it is unfrozen?
  inline bool isOpenToStep(loc_t k, loc_t ign) const
  {
    return
    (ign != k S && owners[k S] == NPLA && isRabOkayS(owners[k],k)) ||
    (ign != k W && owners[k W] == NPLA) ||
    (ign != k E && owners[k E] == NPLA) ||
    (ign != k N && owners[k N] == NPLA && isRabOkayN(owners[k],k));
  }

  //Can the given piece push out in 2 steps, assuming it is unfrozen?
  inline bool isOpenToPush(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);
    if(pieces[k] == RAB) {return false;}
    if(owners[k S] == opp && pieces[k S] < pieces[k] && isOpen(k S)) {return true;}
    if(owners[k W] == opp && pieces[k W] < pieces[k] && isOpen(k W)) {return true;}
    if(owners[k E] == opp && pieces[k E] < pieces[k] && isOpen(k E)) {return true;}
    if(owners[k N] == opp && pieces[k N] < pieces[k] && isOpen(k N)) {return true;}
    return false;
  }

  //Can the given piece push or step out anywhere in 2 steps, assuming it is unfrozen?
  //Does NOT consider trapsafety!
  inline bool isOpenToMove(loc_t k) const
  {
    pla_t pla = owners[k];
    pla_t opp = gOpp(pla);
    if(isOpenToStep(k)) {return true;}
    if(pieces[k] == RAB) {return false;}
    if(owners[k S] == opp && pieces[k S] < pieces[k] && isOpen(k S)) {return true;}
    if(owners[k W] == opp && pieces[k W] < pieces[k] && isOpen(k W)) {return true;}
    if(owners[k E] == opp && pieces[k E] < pieces[k] && isOpen(k E)) {return true;}
    if(owners[k N] == opp && pieces[k N] < pieces[k] && isOpen(k N)) {return true;}
    if(owners[k S] == pla && isOpenToStep(k S)) {return true;}
    if(owners[k W] == pla && isOpenToStep(k W)) {return true;}
    if(owners[k E] == pla && isOpenToStep(k E)) {return true;}
    if(owners[k N] == pla && isOpenToStep(k N)) {return true;}
    return false;
  }

  //Is the given square adjacent to any open space?
  inline bool isOpen(loc_t k) const
  {
    return
    (owners[k S] == NPLA) ||
    (owners[k W] == NPLA) ||
    (owners[k E] == NPLA) ||
    (owners[k N] == NPLA);
  }

  inline bool isOpen(loc_t k, loc_t ign) const
  {
    return
    (k S != ign && owners[k S] == NPLA) ||
    (k W != ign && owners[k W] == NPLA) ||
    (k E != ign && owners[k E] == NPLA) ||
    (k N != ign && owners[k N] == NPLA);
  }

  //Count the number of adjacent empty squares
  inline int countOpen(loc_t k) const
  {
    return
    (owners[k S] == NPLA) +
    (owners[k W] == NPLA) +
    (owners[k E] == NPLA) +
    (owners[k N] == NPLA);
  }

  //Find empty square adjacent
  inline loc_t findOpen(loc_t k) const
  {
    if(owners[k S] == NPLA) {return k S;}
    if(owners[k W] == NPLA) {return k W;}
    if(owners[k E] == NPLA) {return k E;}
    if(owners[k N] == NPLA) {return k N;}
    return ERRLOC;
  }

  //Find empty square adjacent other than ign
  inline loc_t findOpenIgnoring(loc_t k, loc_t ign) const
  {
    if(k S != ign && owners[k S] == NPLA) {return k S;}
    if(k W != ign && owners[k W] == NPLA) {return k W;}
    if(k E != ign && owners[k E] == NPLA) {return k E;}
    if(k N != ign && owners[k N] == NPLA) {return k N;}
    return ERRLOC;
  }

  //Find empty square adjacent to step
  inline loc_t findOpenToStep(loc_t k) const
  {
    if(owners[k S] == NPLA && isRabOkayS(owners[k],k)) return k S;
    if(owners[k W] == NPLA)                            return k W;
    if(owners[k E] == NPLA)                            return k E;
    if(owners[k N] == NPLA && isRabOkayN(owners[k],k)) return k N;
    return ERRLOC;
  }

  //Is the given square adjacent to at least two spaces?
  inline bool isOpen2(loc_t k) const
  {
    return
    (owners[k S] == NPLA) +
    (owners[k W] == NPLA) +
    (owners[k E] == NPLA) +
    (owners[k N] == NPLA)
    >= 2;
  }

  //Can we make a trap safe for ploc in one step? By stepping a piece adjacent to it.
  //ploc might be anywhere on the board
  inline bool canMakeTrapSafeFor(pla_t pla, loc_t kt, loc_t ploc) const
  {
    if((Board::RADIUS[2][kt] & pieceMaps[pla][0] & ~frozenMap & ~Bitmap::makeLoc(ploc)).isEmpty())
      return false;
    if(owners[kt S] == NPLA)
    {
      loc_t loc = kt S;
      //TODO simple multidirectional tests could probably be bottled up in bitmap opterations,
      //especially if we have a version of Bitmap::adj that looks adjacent but excludes the bad rabbit direction

      //TODO is it better to repeatedly access the owners and the frozenmap, or can we instead
      //do an & of the piecemap with ~frozenMap and then just test bits in the map?
      if(loc S != ploc && owners[loc S] == pla && isThawed(loc S) && isRabOkayN(pla,loc S)) return true;
      if(loc W != ploc && owners[loc W] == pla && isThawed(loc W)) return true;
      if(loc E != ploc && owners[loc E] == pla && isThawed(loc E)) return true;
    }
    if(owners[kt W] == NPLA)
    {
      loc_t loc = kt W;
      if(loc S != ploc && owners[loc S] == pla && isThawed(loc S) && isRabOkayN(pla,loc S)) return true;
      if(loc W != ploc && owners[loc W] == pla && isThawed(loc W)) return true;
      if(loc N != ploc && owners[loc N] == pla && isThawed(loc N) && isRabOkayS(pla,loc N)) return true;
    }
    if(owners[kt E] == NPLA)
    {
      loc_t loc = kt E;
      if(loc S != ploc && owners[loc S] == pla && isThawed(loc S) && isRabOkayN(pla,loc S)) return true;
      if(loc E != ploc && owners[loc E] == pla && isThawed(loc E)) return true;
      if(loc N != ploc && owners[loc N] == pla && isThawed(loc N) && isRabOkayS(pla,loc N)) return true;
    }
    if(owners[kt N] == NPLA)
    {
      loc_t loc = kt N;
      if(loc W != ploc && owners[loc W] == pla && isThawed(loc W)) return true;
      if(loc E != ploc && owners[loc E] == pla && isThawed(loc E)) return true;
      if(loc N != ploc && owners[loc N] == pla && isThawed(loc N) && isRabOkayS(pla,loc N)) return true;
    }
    return false;
  }

  //Is the piece at square k surrounded on all 4 sides?
  inline bool isBlocked(loc_t k) const
  {return !isOpen(k);}

  inline bool wouldRabbitBeDomAt(pla_t pla, loc_t k) const
  {
    pla_t opp = gOpp(pla);
    return
      (owners[k S] == opp && pieces[k S] > RAB) ||
      (owners[k W] == opp && pieces[k W] > RAB) ||
      (owners[k E] == opp && pieces[k E] > RAB) ||
      (owners[k N] == opp && pieces[k N] > RAB);
  }

  inline bool wouldRabbitBeUFAt(pla_t pla, loc_t k) const
  {
    pla_t opp = gOpp(pla);
    return
    (
      (owners[k S] == pla) ||
      (owners[k W] == pla) ||
      (owners[k E] == pla) ||
      (owners[k N] == pla)
    )
    ||
    !(
      (owners[k S] == opp && pieces[k S] > RAB) ||
      (owners[k W] == opp && pieces[k W] > RAB) ||
      (owners[k E] == opp && pieces[k E] > RAB) ||
      (owners[k N] == opp && pieces[k N] > RAB)
    );
  }

  //Would a piece of the given power be unfrozen at square k?
  inline bool wouldBeUF(pla_t pla, loc_t loc, loc_t k) const
  {
    piece_t piece = pieces[loc];
    pla_t opp = gOpp(pla);
    return
    (
      (owners[k S] == pla) ||
      (owners[k W] == pla) ||
      (owners[k E] == pla) ||
      (owners[k N] == pla)
    )
    ||
    !(
      (owners[k S] == opp && pieces[k S] > piece) ||
      (owners[k W] == opp && pieces[k W] > piece) ||
      (owners[k E] == opp && pieces[k E] > piece) ||
      (owners[k N] == opp && pieces[k N] > piece)
    );
  }

  //Would a piece of the given power be unfrozen at square k, ignoring anything on squares ign1?
  inline bool wouldBeUF(pla_t pla, loc_t loc, loc_t k, loc_t ign1) const
  {
    piece_t piece = pieces[loc];
    pla_t opp = gOpp(pla);
    return
    (
      (k S != ign1 && owners[k S] == pla) ||
      (k W != ign1 && owners[k W] == pla) ||
      (k E != ign1 && owners[k E] == pla) ||
      (k N != ign1 && owners[k N] == pla)
    )
    ||
    !(
      (k S != ign1 && owners[k S] == opp && pieces[k S] > piece) ||
      (k W != ign1 && owners[k W] == opp && pieces[k W] > piece) ||
      (k E != ign1 && owners[k E] == opp && pieces[k E] > piece) ||
      (k N != ign1 && owners[k N] == opp && pieces[k N] > piece)
    );
  }

  //Would a piece of the given power be unfrozen at square k, ignoring anything on squares ign1, ign2?
  inline bool wouldBeUF(pla_t pla, loc_t loc, loc_t k, loc_t ign1, loc_t ign2) const
  {
    piece_t piece = pieces[loc];
    pla_t opp = gOpp(pla);
    return
    (
      (k S != ign1 && k S != ign2 && owners[k S] == pla) ||
      (k W != ign1 && k W != ign2 && owners[k W] == pla) ||
      (k E != ign1 && k E != ign2 && owners[k E] == pla) ||
      (k N != ign1 && k N != ign2 && owners[k N] == pla)
    )
    ||
    !(
      (k S != ign1 && k S != ign2 && owners[k S] == opp && pieces[k S] > piece) ||
      (k W != ign1 && k W != ign2 && owners[k W] == opp && pieces[k W] > piece) ||
      (k E != ign1 && k E != ign2 && owners[k E] == opp && pieces[k E] > piece) ||
      (k N != ign1 && k N != ign2 && owners[k N] == opp && pieces[k N] > piece)
    );
  }

  //Would a piece be guarded at square k?
  inline bool wouldBeG(pla_t pla, loc_t k) const
  {
    return
    (
      (owners[k S] == pla) ||
      (owners[k W] == pla) ||
      (owners[k E] == pla) ||
      (owners[k N] == pla)
    );
  }

  //Would a piece be guarded at square k, ignoring anything on squares ign1?
  inline bool wouldBeG(pla_t pla, loc_t k, loc_t ign1) const
  {
    return
    (
      (k S != ign1 && owners[k S] == pla) ||
      (k W != ign1 && owners[k W] == pla) ||
      (k E != ign1 && owners[k E] == pla) ||
      (k N != ign1 && owners[k N] == pla)
    );
  }

  //Would a piece be guarded at square k, ignoring anything on squares ign1, ign2?
  inline bool wouldBeG(pla_t pla, loc_t k, loc_t ign1, loc_t ign2) const
  {
    return
    (
      (k S != ign1 && k S != ign2 && owners[k S] == pla) ||
      (k W != ign1 && k W != ign2 && owners[k W] == pla) ||
      (k E != ign1 && k E != ign2 && owners[k E] == pla) ||
      (k N != ign1 && k N != ign2 && owners[k N] == pla)
    );
  }

  //Would a piece currently at loc be dominated at square k?
  inline bool wouldBeDom(pla_t pla, loc_t loc, loc_t k) const
  {
    pla_t opp = gOpp(pla);
    return
    (
      (owners[k S] == opp && pieces[k S] > pieces[loc]) ||
      (owners[k W] == opp && pieces[k W] > pieces[loc]) ||
      (owners[k E] == opp && pieces[k E] > pieces[loc]) ||
      (owners[k N] == opp && pieces[k N] > pieces[loc])
    );
  }

  //Would a piece currently at loc be dominated at square k, ignoring square ign?
  inline bool wouldBeDom(pla_t pla, loc_t loc, loc_t k, loc_t ign) const
  {
    pla_t opp = gOpp(pla);
    return
    (
      (k S != ign && owners[k S] == opp && pieces[k S] > pieces[loc]) ||
      (k W != ign && owners[k W] == opp && pieces[k W] > pieces[loc]) ||
      (k E != ign && owners[k E] == opp && pieces[k E] > pieces[loc]) ||
      (k N != ign && owners[k N] == opp && pieces[k N] > pieces[loc])
    );
  }

  //Is it okay by the rabbit rule for this piece to step south?
  inline bool isRabOkayS(pla_t pla, loc_t k) const
  {return pieces[k] != RAB || pla == SILV;}

  //Is it okay by the rabbit rule for this piece to step north?
  inline bool isRabOkayN(pla_t pla, loc_t k) const
  {return pieces[k] != RAB || pla == GOLD;}

  //Is it okay by the rabbit rule for this piece at k0 to walk its way to k1 in any number of steps?
  inline bool isRabOkay(pla_t pla, loc_t k0, loc_t k1) const
  {return (pieces[k0] != RAB) || (pla == SILV && gY(k1) <= gY(k0)) || (pla == GOLD && gY(k1) >= gY(k0));}

  //Would it be okay by the rabbit rule for this piece at ploc to go from k0 to k1 in any number of steps?
  inline bool isRabOkay(pla_t pla, loc_t ploc, loc_t k0, loc_t k1) const
  {return (pieces[ploc] != RAB) || (pla == SILV && gY(k1) <= gY(k0)) || (pla == GOLD && gY(k1) >= gY(k0));}

  //Would it bee okay by the rabbit rule for a rabbit to go from k0 to k1 in any number of steps?
  inline static bool isRabOkayStatic(pla_t pla, loc_t k0, loc_t k1)
  {return (pla == SILV && gY(k1) <= gY(k0)) || (pla == GOLD && gY(k1) >= gY(k0));}

  //Can a piece of pla step to loc?
  //Does not check trapsafety
  inline bool canStepAndOccupy(pla_t pla, loc_t loc) const
  {
    if(owners[loc S] == pla && isThawedC(loc S) && isRabOkayN(pla,loc S)) {return true;}
    if(owners[loc W] == pla && isThawedC(loc W)                         ) {return true;}
    if(owners[loc E] == pla && isThawedC(loc E)                         ) {return true;}
    if(owners[loc N] == pla && isThawedC(loc N) && isRabOkayS(pla,loc N)) {return true;}
    return false;
  }

  //Get the location, if any, of an adjacent piece that will be sacrificed if this piece moves.
  inline loc_t getSacLoc(pla_t pla, loc_t loc) const
  {
    loc_t capLoc = ADJACENTTRAP[loc];
    if(capLoc == ERRLOC)
      return ERRLOC;
    if(owners[capLoc] == pla && trapGuardCounts[pla][Board::TRAPINDEX[capLoc]] <= 1)
      return capLoc;
    return ERRLOC;
  }

  inline piece_t strongestAdjacentPla(pla_t pla, loc_t loc) const
  {
    piece_t piece = EMP;
    if(owners[loc S] == pla) {piece = piece >= pieces[loc S] ? piece : pieces[loc S];}
    if(owners[loc W] == pla) {piece = piece >= pieces[loc W] ? piece : pieces[loc W];}
    if(owners[loc E] == pla) {piece = piece >= pieces[loc E] ? piece : pieces[loc E];}
    if(owners[loc N] == pla) {piece = piece >= pieces[loc N] ? piece : pieces[loc N];}
    return piece;
  }

  inline piece_t strongestAdjacentUFPla(pla_t pla, loc_t loc) const
  {
    piece_t piece = EMP;
    if(owners[loc S] == pla && isThawed(loc S)) {piece = piece >= pieces[loc S] ? piece : pieces[loc S];}
    if(owners[loc W] == pla && isThawed(loc S)) {piece = piece >= pieces[loc W] ? piece : pieces[loc W];}
    if(owners[loc E] == pla && isThawed(loc S)) {piece = piece >= pieces[loc E] ? piece : pieces[loc E];}
    if(owners[loc N] == pla && isThawed(loc S)) {piece = piece >= pieces[loc N] ? piece : pieces[loc N];}
    return piece;
  }

  //TEMPORARY CHANGES----------------------------------------------------------------------

  //Note! - after making a temp step/move, none of the bitmaps are updated!
  //Nor are the hashes, step counter, etc updated. Do not trust isFrozen or isThawed, use isFrozenC or isThawedC instead.

  inline void tempStep(loc_t k0, loc_t k1)
  {
    owners[k1] = owners[k0];
    pieces[k1] = pieces[k0];
    owners[k0] = NPLA;
    pieces[k0] = EMP;
    if(ADJACENTTRAP[k0] != ERRLOC)
      trapGuardCounts[owners[k1]][TRAPINDEX[ADJACENTTRAP[k0]]]--;
    if(ADJACENTTRAP[k1] != ERRLOC)
      trapGuardCounts[owners[k1]][TRAPINDEX[ADJACENTTRAP[k1]]]++;
  }

  inline void tempPP(loc_t k0, loc_t k1, loc_t k2)
  {
    owners[k2] = owners[k1];
    pieces[k2] = pieces[k1];
    owners[k1] = owners[k0];
    pieces[k1] = pieces[k0];
    owners[k0] = NPLA;
    pieces[k0] = EMP;
    if(ADJACENTTRAP[k0] != ERRLOC)
      trapGuardCounts[owners[k1]][TRAPINDEX[ADJACENTTRAP[k0]]]--;
    if(ADJACENTTRAP[k1] != ERRLOC)
      trapGuardCounts[owners[k1]][TRAPINDEX[ADJACENTTRAP[k1]]]++;
    if(ADJACENTTRAP[k1] != ERRLOC)
      trapGuardCounts[owners[k2]][TRAPINDEX[ADJACENTTRAP[k1]]]--;
    if(ADJACENTTRAP[k2] != ERRLOC)
      trapGuardCounts[owners[k2]][TRAPINDEX[ADJACENTTRAP[k2]]]++;
  }

  inline TempRecord tempStepC(loc_t k0, loc_t k1)
  {
    owners[k1] = owners[k0];
    pieces[k1] = pieces[k0];
    owners[k0] = NPLA;
    pieces[k0] = EMP;

    if(ADJACENTTRAP[k0] != ERRLOC)
      trapGuardCounts[owners[k1]][TRAPINDEX[ADJACENTTRAP[k0]]]--;
    if(ADJACENTTRAP[k1] != ERRLOC)
      trapGuardCounts[owners[k1]][TRAPINDEX[ADJACENTTRAP[k1]]]++;

    TempRecord rec = TempRecord(k0,k1);

    int caploc = ADJACENTTRAP[k0];
    if(caploc != ERRLOC)
    {checkCapsR(caploc, rec);}

    return rec;
  }

  inline void undoTemp(TempRecord rec)
  {
    if(rec.capLoc != ERRLOC)
    {
      owners[rec.capLoc] = rec.capOwner;
      pieces[rec.capLoc] = rec.capPiece;
    }

    owners[rec.k0] = owners[rec.k1];
    pieces[rec.k0] = pieces[rec.k1];

    owners[rec.k1] = NPLA;
    pieces[rec.k1] = EMP;

    if(ADJACENTTRAP[rec.k0] != ERRLOC)
      trapGuardCounts[owners[rec.k0]][TRAPINDEX[ADJACENTTRAP[rec.k0]]]++;
    if(ADJACENTTRAP[rec.k1] != ERRLOC)
      trapGuardCounts[owners[rec.k0]][TRAPINDEX[ADJACENTTRAP[rec.k1]]]--;
  }

  inline bool checkCapsR(loc_t k, TempRecord &rec)
  {
    pla_t owner = owners[k];
    if(ISTRAP[k] && owner != NPLA && trapGuardCounts[owner][TRAPINDEX[k]] == 0)
    {
      rec.capLoc = k;
      rec.capOwner = owner;
      rec.capPiece = pieces[k];
      owners[k] = NPLA;
      pieces[k] = EMP;
      return true;
    }
    return false;
  }

  //OUTPUT-------------------------------------------------------------------------------
  friend ostream& operator<<(ostream& out, const Board& b);

  //In general, writing functions will never fail and will make a best effort to output data.

  string write() const;
  static string write(const Board& b);

  static char writePla(pla_t pla);                     //s,g,n
  static char writePiece(pla_t pla, piece_t piece);    //rcdhmeRCDHME.
  static string writePieceWord(piece_t piece);         //rab, cat, dog, hor, cam, ele
  static string writeLoc(loc_t k);                     //a1, b3, ... including "errloc"
  static string writePlacement(loc_t k, pla_t pla, piece_t piece); //Ra1, Cd2, ...
  static string writePlaTurn(int turn);                //1g, 1s, 2g,...
  static string writePlaTurn(pla_t pla, int turn);     //Same, but handles the game starting with silver's turn

  static string writeMaterial(const int8_t pieceCounts[2][NUMTYPES]);
  static string writePlacements(const Board& b, pla_t pla);      //All placements for the given player
  static string writeStartingPlacementsAndTurns(const Board& b); //All placements for both players AND the "1g" and "1s"

  static string writeStep(step_t step, bool showPass = true);    //Handles pass, qpass, errstep
  static string writeStep(const Board& b, step_t step, bool showPass=true);
  static string writeMove(move_t move, bool showPass = true);
  static string writeMove(const Board& b, move_t move, bool showPass=true);

  static string writeMoves(const vector<move_t>& moves);
  static string writeMoves(const Board& b, const vector<move_t>& moves);
  static string writeMoves(const move_t* moves, int numMoves);
  static string writeMoves(const Board& b, const move_t* moves, int numMoves);

  static string writeGame(const Board& b, const vector<move_t>& moves);

  //OSTREAM OUTPUT-----------------------------------------------------------------

  void write(ostream& out) const;
  static void write(ostream& out, const Board& b);

  static void writePla(ostream& out, pla_t pla);
  static void writePiece(ostream& out, pla_t pla, piece_t piece);
  static void writePieceWord(ostream& out, piece_t piece);
  static void writeLoc(ostream& out, loc_t k);
  static void writePlacement(ostream& out, loc_t k, pla_t pla, piece_t piece);
  static void writePlaTurn(ostream& out, int turn);
  static void writePlaTurn(ostream& out, pla_t pla, int turn);
  static void writeHash(ostream& out, hash_t hash);

  static void writeMaterial(ostream& out, const int8_t pieceCounts[2][NUMTYPES]);
  static void writePlacements(ostream& out, const Board& b, pla_t pla);
  static void writeStartingPlacementsAndTurns(ostream& out, const Board& b);

  static void writeStep(ostream& out, step_t step, bool showPass = true);
  static void writeStep(ostream& out, const Board& b, step_t step, bool showPass=true);
  static void writeMove(ostream& out, move_t move, bool showPass = true);
  static void writeMove(ostream& out, const Board& b, move_t move, bool showPass=true);

  static void writeMoves(ostream& out, const vector<move_t>& moves);
  static void writeMoves(ostream& out, const Board& b, const vector<move_t>& moves);
  static void writeMoves(ostream& out, const move_t* moves, int numMoves);
  static void writeMoves(ostream& out, const Board& b, const move_t* moves, int numMoves);

  static void writeGame(ostream& out, const Board& b, const vector<move_t>& moves);

  //INPUT------------------------------------------------------------------------------
  STRUCT_NAMED_TRIPLE(loc_t,loc,pla_t,owner,piece_t,piece,Placement);

  friend istream& operator>>(istream& in, Board& b);

  static pla_t readPla(char c);          //g, s, w, b, n, 0, 1, 2, any captialization handled
  static pla_t readPla(const char* s);   //g, s, w, b, n, gold, silv, npla, silver, any captialization handled
  static pla_t readPla(const string& s); //g, s, w, b, n, gold, silv, npla, silver, any captialization handled
  static bool tryReadPla(char c, pla_t& pla);
  static bool tryReadPla(const char* s, pla_t& pla);
  static bool tryReadPla(const string& s, pla_t& pla);

  static pla_t readOwner(char c);            //rcdhmeRCDHME. as well as misc markings for traps
  static piece_t readPiece(char c);          //rcdhmeRCDHME. as well as misc markings for traps
  static piece_t readPiece(const char* s);   //Also handles words ("rab")
  static piece_t readPiece(const string& s); //Also handles words ("rab")

  static loc_t readLoc(const char* s);     //a1, f3, ... not including errloc
  static loc_t readLoc(const string& s);
  static bool tryReadLoc(const char* s, loc_t& loc);
  static bool tryReadLoc(const string& s, loc_t& loc);

  static loc_t readLocWithErrloc(const char* s);     //a1, f3, ... including errloc
  static loc_t readLocWithErrloc(const string& s);
  static bool tryReadLocWithErrloc(const char* s, loc_t& loc);
  static bool tryReadLocWithErrloc(const string& s, loc_t& loc);

  static int readPlaTurn(const char* s);                  //1s, 2g, ...
  static int readPlaTurn(const string& s);
  static bool tryReadPlaTurn(const char* s, int& turn);
  static bool tryReadPlaTurn(const string& s, int& turn);
  static int readPlaTurnOrInt(const char* s);
  static int readPlaTurnOrInt(const string& s);
  static bool tryReadPlaTurnOrInt(const char* s, int& turn);
  static bool tryReadPlaTurnOrInt(const string& s, int& turn);

  static Board read(const char* str, bool ignoreConsistency = false);
  static Board read(const string& str, bool ignoreConsistency = false);

  static step_t readStep(const char* str);   //succeeds on capture move tokens but returns ERRSTEP
  static step_t readStep(const string& str);
  static bool tryReadStep(const char* str, step_t& step);
  static bool tryReadStep(const string& str, step_t& step);
  static move_t readMove(const char* str);
  static move_t readMove(const string& str);
  static bool tryReadMove(const char* str, move_t& move);
  static bool tryReadMove(const string& str, move_t& move);

  static Placement readPlacement(const char* str);
  static Placement readPlacement(const string& str);
  static bool tryReadPlacement(const char* str, Placement& ret);
  static bool tryReadPlacement(const string& str, Placement& ret);
  static bool tryReadPlacements(const char* str, vector<Placement>& ret);
  static bool tryReadPlacements(const string& str, vector<Placement>& ret);

  //Read a sequence of moves (no placements, turn indicators are ignored)
  //Assumed to start on step initialStep, defaulting to 0
  static vector<move_t> readMoveSequence(const char* str);
  static vector<move_t> readMoveSequence(const string& str);
  static vector<move_t> readMoveSequence(const string& str, int initialStep);
};


#endif
