#ifndef BTYPES_H_
#define BTYPES_H_

#include "../core/global.h"

#define NUMPLAS 2   //Excludes NPLA
#define NUMTYPES 7  //Excludes OFF
#define NUMDIRS 4
#define NUMTRAPS 4

//16x12 representation
#define BBUFSIZE 192 //0x88 + mailbox
#define BSIZE 128 //0x88, half unused
#define BSTARTIDX 36

//0x88 difference in [-119,119] uniquely identifies relative offset
#define DIFFIDX_ADD 119
#define DIFFIDX_SIZE 256 //Only needs to be 238, but we'll make it a nice power of 2

//Players - 3 valid values
typedef int pla_t;
#define SILV 0
#define GOLD 1
#define NPLA 2

//Piece types - 8 valid values
typedef int piece_t;
#define EMP 0x0
#define RAB 0x1
#define CAT 0x2
#define DOG 0x3
#define HOR 0x4
#define CAM 0x5
#define ELE 0x6
#define OFF 0x7

//Locations - 8 bits, Must either be onboard or ERRLOC, 0x88 format
typedef int loc_t;
#define ERRLOC (-1)
#define TRAP0 0x22
#define TRAP1 0x25
#define TRAP2 0x52
#define TRAP3 0x55

//Bitmap indices - From 0 to 63
typedef int idx_t;
#define IDXTRAP0 18
#define IDXTRAP1 21
#define IDXTRAP2 42
#define IDXTRAP3 45

//Steps - 8 bits
//<dir bit 1> <y coord> <dir bit 0> <x coord>
typedef uint32_t step_t;
#define ERRSTEP 0x00
#define PASSSTEP 0xFF
#define QPASSSTEP 0xFE

//Moves - 32 bits
//<step 3> <step 2> <step 1> <step 0>
typedef uint32_t move_t;
#define ERRMOVE 0x00000000
#define PASSMOVE 0x000000FF
#define QPASSMOVE 0x000000FE

typedef uint64_t hash_t;
typedef int eval_t;

//ONBOARD TESTING------------------------------------------------------------------

#define CS1(x) ((x) >= 16)
#define CW1(x) (((x)&0xF) > 0)
#define CE1(x) (((x)&0xF) < 7)
#define CN1(x) ((x) < 112)
#define CS2(x) ((x) >= 32)
#define CW2(x) (((x)&0xF) > 1)
#define CE2(x) (((x)&0xF) < 6)
#define CN2(x) ((x) < 96)
#define CS3(x) ((x) >= 48)
#define CW3(x) (((x)&0xF) > 2)
#define CE3(x) (((x)&0xF) < 5)
#define CN3(x) ((x) < 80)

//Same as above, except for advance/retreat, given the player
#define CA1(p,x) ((p) == GOLD ? ((x) < 112) : ((x) >= 16))
#define CA2(p,x) ((p) == GOLD ? ((x) < 96) : ((x) >= 32))
#define CA3(p,x) ((p) == GOLD ? ((x) < 80) : ((x) >= 48))
#define CR1(p,x) ((p) != GOLD ? ((x) < 112) : ((x) >= 16))
#define CR2(p,x) ((p) != GOLD ? ((x) < 96) : ((x) >= 32))
#define CR3(p,x) ((p) != GOLD ? ((x) < 80) : ((x) >= 48))


//CONVENIENCE FUNCTIONS FOR TYPES-------------------------------------------------------

static inline pla_t gOpp(pla_t pla) {return 1 - pla;}
static inline pla_t isRealPla(pla_t pla) {return pla < 2;}

static inline loc_t gLoc(int x, int y) {return y * 16 + x;}
static inline int gX(loc_t loc) {return loc & 0x7;}
static inline int gY(loc_t loc) {return loc >> 4;}

static inline idx_t gIdx(int x, int y) {return y * 8 + x;}
static inline int gIdxX(idx_t idx) {return idx & 0x7;}
static inline int gIdxY(idx_t idx) {return idx >> 3;}

static inline loc_t gLoc(idx_t idx) {return idx + (idx & ~7);}
static inline idx_t gIdx(loc_t loc) {return (loc + (loc & 0x7)) >> 1;}

static inline loc_t gRevLoc(loc_t loc) {return 0x77 - loc;}
static inline idx_t gRevIdx(idx_t idx) {return 63 - idx;}

static inline int gDXOffset(int dx) {return dx;}
static inline int gDYOffset(int dy) {return dy * 16;}

//STATIC MOVE/STEP RETRIEVAL------------------------------------------------------
namespace BTypesInternal
{
  extern const int STEP_OFFSET_FROM_OFFSET[33]; //step = src + STEP_OFFSET_FROM_DIR[dest-src+16]
  extern const int8_t DEST_FROM_STEP[256];      //[step]: Look up the end square given the step index
  extern const bool RABBIT_STEP_VALID[2][256];     //[pla][step]: Can a rabbit make such a step?
}

static inline loc_t gSrc(step_t step) {
#ifdef EXTRA_BOUND_CHECKS
  DEBUGASSERT(step != ERRSTEP && step != PASSSTEP && step != QPASSSTEP);
#endif
  return step & 0x77;
}
static inline loc_t gDest(step_t step) {
#ifdef EXTRA_BOUND_CHECKS
  DEBUGASSERT(step != ERRSTEP && step != PASSSTEP && step != QPASSSTEP);
#endif
  return BTypesInternal::DEST_FROM_STEP[step];
}
static inline step_t gStepSrcDest(loc_t src, loc_t dest) {return src + BTypesInternal::STEP_OFFSET_FROM_OFFSET[dest-src+16];}
static inline int gDir(step_t step) {return ((step & 0x8) >> 3) | ((step & 0x80) >> 6);}

//static inline bool rabStepOkay(pla_t pla, step_t step) {return (step & 0x88) != (1-pla)*0x88;}
static inline bool rabStepOkay(pla_t pla, step_t step) {return BTypesInternal::RABBIT_STEP_VALID[pla][step];}
static inline bool rabDirOkay(pla_t pla, int dir) {return dir != (1-pla)*3;}
static inline bool rabOffsetOkay(pla_t pla, int dloc) {
  if(pla == GOLD)
    return dloc >= -8;
  return dloc <= 8;
}

//Returns the move composed of the specified steps
static inline move_t getMove(step_t s0)
{return ((move_t)s0);}
static inline move_t getMove(step_t s0, step_t s1)
{return ((move_t)s0) | (((move_t)s1) << 8);}
static inline move_t getMove(step_t s0, step_t s1, step_t s2)
{return ((move_t)s0) | (((move_t)s1) << 8) | (((move_t)s2) << 16);}
static inline move_t getMove(step_t s0, step_t s1, step_t s2, step_t s3)
{return ((move_t)s0) | (((move_t)s1) << 8) | (((move_t)s2) << 16) | (((move_t)s3) << 24);}

static inline move_t gMove(step_t s0)
{return ((move_t)s0);}
static inline move_t gMove(step_t s0, step_t s1)
{return ((move_t)s0) | (((move_t)s1) << 8);}
static inline move_t gMove(step_t s0, step_t s1, step_t s2)
{return ((move_t)s0) | (((move_t)s1) << 8) | (((move_t)s2) << 16);}
static inline move_t gMove(step_t s0, step_t s1, step_t s2, step_t s3)
{return ((move_t)s0) | (((move_t)s1) << 8) | (((move_t)s2) << 16) | (((move_t)s3) << 24);}

//Returns the move composed of the specified steps, followed by the move m.
static inline move_t preConcatMove(step_t s0, move_t m)
{return ((move_t)s0) | (m << 8);}
static inline move_t preConcatMove(step_t s0, step_t s1, move_t m)
{return ((move_t)s0) | (((move_t)s1) << 8) | (m << 16);}
static inline move_t preConcatMove(step_t s0, step_t s1, step_t s2, move_t m)
{return ((move_t)s0) | (((move_t)s1) << 8) | (((move_t)s2) << 16) | (m << 24);}

//Returns the move m of the given length, except with the last steps overwritten by the given steps
static inline move_t postConcatMove(move_t m, step_t s3)
{return (m & (move_t)0x00FFFFFFU) | (((move_t)s3) << 24);}
static inline move_t postConcatMove(move_t m, step_t s2, step_t s3)
{return (m & (move_t)0x0000FFFFU) | (((move_t)s2) << 16) | (((move_t)s3) << 24);}
static inline move_t postConcatMove(move_t m, step_t s1, step_t s2, step_t s3)
{return (m & (move_t)0x000000FFU) | (((move_t)s1) <<  8) | (((move_t)s2) << 16) | (((move_t)s3) << 24);}

//Returns the move m except with the steps starting with step <index> overwritten by m2. Behaviour is undefined if index is not an integer 0-3
static inline move_t concatMoves(move_t m, move_t m2, int index)
{m &= ((1U << (index*8))-1); return m | (m2 << (index*8));}

//Returns the move m except with the steps at step <index> overwritten by s. Behaviour is undefined if index is not an integer 0-3
static inline move_t setStep(move_t m, step_t s, int index)
{m &= ~(0XFFU << (index*8)); return m | ((move_t)s << (index*8));}

//Returns the move m except with the steps at step <index> overwritten by a passstep.
//Takes advantage of passstep being the all-ones byte
//Behaviour is undefined if index is not an integer 0-3
static inline move_t setStepToPass(move_t m, int index)
{return m | ((move_t)PASSMOVE << (index*8));}

//Returns the move consisting of the first index steps of the move. Behaviour is undefined if index is not an integer 0-3
static inline move_t getPreMove(move_t m, int index)
{return m & ((1U << (index*8))-1);}

//Returns the move, deleting the steps before index. Behaviour is undefined if index is not an integer 0-3
static inline move_t getPostMove(move_t m, int index)
{return m >> (index*8);}

//Returns the indexth step in the move. Behaviour is undefined if index is not an integer 0-3
static inline step_t getStep(move_t move, int index)
{return (step_t)(move >> (index * 8)) & 0xFF;}

//Checks if a move is a single step or pass or errmove
static inline bool isSingleStepOrError(move_t move)
{return (move & 0xFFFFFF00U) == 0;}

//Returns the number of steps in the move
static inline int numStepsInMove(move_t move)
{
  if(move == 0) return 0;
  if((move & 0xFFFFFF00U) == 0) return 1;
  if((move & 0xFFFF0000U) == 0) return 2;
  if((move & 0xFF000000U) == 0) return 3;
  return 4;
}

//Returns the number of steps in the move, taking into account that a pass completes the move to 4 steps
static inline int numRealStepsInMove(move_t move)
{
  int ns = numStepsInMove(move);
  if(ns <= 0)
    return 0;
  step_t lastStep = getStep(move,ns-1);
  if(lastStep == PASSSTEP || lastStep == QPASSSTEP)
    return 4;
  return ns;
}

//Checks if move1 is a prefix of move2
static inline bool isPrefix(move_t move1, move_t move2)
{
  for(int i = 0; i<4; i++)
  {
    step_t step = getStep(move1,i);
    if(step == ERRSTEP) return true;
    if(step != getStep(move2,i)) return false;
  }
  return true;
}

//Complete the move so that it finishes off a whole 4-step turn
static inline move_t completeTurn(move_t move)
{
  int ns = numStepsInMove(move);
  if(ns == 0)
    return PASSMOVE;
  if(ns == 4)
    return move;
  if(getStep(move,ns-1) == PASSSTEP)
    return move;
  if(getStep(move,ns-1) == QPASSSTEP)
    return concatMoves(move,PASSMOVE,ns-1);
  return concatMoves(move,PASSMOVE,ns);
}

//Strips any passstep or qpassstep that might occur in this move (and anything following it)
static inline move_t stripPasses(move_t move)
{
  //Prune pass/qpass
  for(int j = 0; j<4; j++)
  {
    step_t step = getStep(move,j);
    if(step == ERRSTEP)
      return move;
    if(step == PASSSTEP || step == QPASSSTEP)
      return getPreMove(move,j);
  }
  return move;
}

#endif
