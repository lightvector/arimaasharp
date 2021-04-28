/*
 * ufdist.h
 * Author: davidwu
 */

#ifndef STRATS_H
#define STRATS_H

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"

class FrameThreat;
class BlockadeThreat;

namespace Strats
{
  //Find pla holding a frame, blockade, or hostage
  const int maxFramesPerKT = 1; //NOTE: If you change this you must change its usage in featuremove.cpp
  int findFrame(const Board& b, pla_t pla, loc_t kt, FrameThreat* threats);
  const int maxHostagesPerKT = 9;
  const int maxBlockadesPerPla = 1; //NOTE: If you change this you must change its usage in featuremove.cpp
  int findBlockades(Board& b, pla_t pla, BlockadeThreat* threats);

  const int frameThreatMax = maxFramesPerKT*4;
  const int hostageThreatMax = maxHostagesPerKT*2;
  const int blockadeThreatMax = maxBlockadesPerPla;
  const int numStratsMax = frameThreatMax + hostageThreatMax + blockadeThreatMax;

  //New code
  static const int TOTAL_IMMO = 0;
  static const int ALMOST_IMMO = 1;
  static const int CENTRAL_BLOCK = 2;
  static const int CENTRAL_BLOCK_WEAK = 3;
  loc_t findEBlockade(Board& b, pla_t pla, int& immoType, Bitmap& recursedMap, Bitmap& holderHeldMap, Bitmap& freezeHeldMap);

}


class FrameThreat
{
  public:
  loc_t pinnedLoc;  //Location of pinned defending piece
  loc_t kt;         //The trap where a piece is framed
  bool isPartial;   //Is this a loose/partial frame?

  //Bitmap marking all pieces and locations involved in holding the frame
  //Includes all holding pieces, including opps, and the possibly empty square above a framed rabbit for framed rabbits.
  Bitmap holderMap;

  inline friend ostream& operator<<(ostream& out, const FrameThreat& t)
  {
    out << "Frame: Trap " << (int)t.kt << " Pinned " << (int)t.pinnedLoc << " " << (t.isPartial ? "partial " : "full ");
    return out;
  }
};

class BlockadeThreat
{
  public:
  loc_t pinnedLoc;     //Piece blockaded
  int tightness;       //How tight the blockade is - one of the constants below
  Bitmap holderMap;    //Bitmap marking all pieces and locations involved in holding the blockade

  static const int FULLBLOCKADE = 0;  //Airtight in this direction
  static const int LOOSEBLOCKADE = 1; //Loose in this direction
  static const int LEAKYBLOCKADE = 2; //Leaky in this direction
  static const int OPENBLOCKADE = 3;  //Completely unblocked in this direction

  inline friend ostream& operator<<(ostream& out, const BlockadeThreat& t)
  {
    out << "Blockade " << (int)t.pinnedLoc << " tightness " << t.tightness;
    out << t.holderMap;
    return out;
  }
};

class GoalThreat
{
  public:
  int8_t dist;   //Number of steps to goal
  loc_t rloc;           //Rabbit threatening to goal
  loc_t goalloc;        //Goal square

  inline GoalThreat()
  :dist(0),rloc(0),goalloc(0)
  {}

  inline GoalThreat(int8_t dist, loc_t rloc, loc_t goalloc)
  :dist(dist),rloc(rloc),goalloc(goalloc)
  {}

  inline friend ostream& operator<<(ostream& out, const GoalThreat& t)
  {
    out << "Goal in " << (int)t.dist << " by " << (int)t.rloc << " at " << (int)t.goalloc;
    return out;
  }
};

class BlockadeRotation
{
  public:
  loc_t blockerLoc;     //Location of piece that can be rotated in, or ERRLOC
  bool isSwarm;         //Is this forming a swarm holding where there was none before?
  int8_t dist;   //Number of steps to perform rotation

  inline BlockadeRotation()
  :blockerLoc(ERRLOC),isSwarm(false),dist(0)
  {}

  inline BlockadeRotation(loc_t blockerLoc, bool isSwarm, int8_t dist)
  :blockerLoc(blockerLoc),isSwarm(isSwarm),dist(dist)
  {}
};

class Strat
{
  public:
  int value;         //Value of strat, and cost for pla to giveup
  loc_t pieceLoc;    //Piece that this strat is focused on. If captured, will lose the strat value, and will not be multicounted.
  bool wasHostage;    //Frame or hostage?

  Bitmap plaHolders;    //Pla gives up unless all of these pieces are unused.

  inline Strat()
  :value(0),pieceLoc(ERRLOC),wasHostage(false),plaHolders(Bitmap())
  {}

  inline Strat(int value, loc_t pieceLoc, bool wasHostage, Bitmap plaHolders)
  :value(value),pieceLoc(pieceLoc),wasHostage(wasHostage),plaHolders(plaHolders)
  {}

  inline friend ostream& operator<<(ostream& out, const Strat& s)
  {
    out << "Strat val " << s.value << " pieceLoc " << (int)s.pieceLoc;
    return out;
  }
};

#endif /* STRATS_H */
