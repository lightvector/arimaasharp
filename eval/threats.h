
/*
 * threats.h
 * Author: davidwu
 */

#ifndef THREATS_H
#define THREATS_H

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"

class CapThreat;

namespace Threats
{
  //Fill array with goal distance estimate
  //Well, not exactly, since a pla piece counts as zero if unfrozen, because pla pieces tend to help
  //more than empty spots.
  void getGoalDistEst(const Board& b, pla_t pla, int goalDist[BSIZE], int max);
  void getGoalDistBitmapEst(const Board& b, pla_t pla, Bitmap goalDistIs[9]);

  //Distance to walk adjacent to dest remaining unfrozen, assumes ploc is not rabbit
  int traverseAdjUfDist(const Board& b, pla_t pla, loc_t ploc, loc_t dest, const int ufDist[BSIZE], int maxDist);

  extern const int ZEROS[BSIZE];
  int traverseDist(const Board& b, loc_t ploc, loc_t dest, bool endUF,
      const int ufDist[BSIZE], int maxDist, const int* bonusVals = ZEROS);

  //A faster and less accurate version of traverseAdjUfDist
  //ploc is assumed not to be a rabbit.
  int fastTraverseAdjUFAssumeNonRabDist(const Board& b, loc_t ploc, loc_t dest, const int ufDist[BSIZE], int maxDist);

  //DO NOT put in arbitrarily large numbers for maxDist - it uses maxDist for things like board RADIUS and DISK
  //indexing and also might take a long time.
  //Dist to move a piece stronger than strongerThan adjacent and unfrozen
  //The maximum return value of this will be maxDist
  //Specify strongerThan and leqThan for constraints on piece strength
  //Specify blockApproachNotFrom (adjacent to loc) to add +2 extra block distance if the attacker doesn't come through blockApproachNotFrom
  int moveAdjDistUF(const Board& b, pla_t pla, loc_t loc, int maxDist,
      const int ufDist[BSIZE], const Bitmap pStrongerMaps[2][NUMTYPES] = NULL,
      piece_t strongerThan = EMP, piece_t leqThan = ELE, loc_t blockApproachNotFrom = ERRLOC);

  //DO NOT put in arbitrarily large numbers for maxDist - it uses maxDist for things like board RADIUS and DISK
  //indexing and also might take a long time.
  //Dist for opp to threaten ploc (adjacent UF stronger piece)
  //The maximum return value of this will be maxDist
  int attackDist(const Board& b, loc_t ploc, int maxDist,
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE]);
  int attackDistNonEle(const Board& b, loc_t ploc, int maxDist,
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE]);

  //Adds a special case where if the attacked piece is on a trap, the attackdist might be increased if the attacker
  //can't easily pushpull it around
  int attackDistPushpullingForward(const Board& b, loc_t ploc, int maxDist,
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE]);
  int attackDistPushpullingForwardNonEle(const Board& b, loc_t ploc, int maxDist,
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE]);

  //DO NOT put in arbitrarily large numbers for maxDist - it uses maxDist for things like board RADIUS and DISK
  //indexing and also might take a long time.
  //Dist for pla to occupy loc
  //Not quite accurate - disregards freezing at the end
  int occupyDist(const Board& b, pla_t pla, loc_t loc, int maxDist,
      const Bitmap pStrongerMaps[2][NUMTYPES], const int ufDist[BSIZE]);

  //Threats BY pla
  const int maxCapsPerPla = 128;
  int findCapThreats(Board& b, pla_t pla, loc_t kt, CapThreat* threats, const int ufDist[BSIZE],
      const Bitmap pStronger[2][NUMTYPES], int maxCapDist, int maxThreatLen, int& rdSteps);
}

class CapThreat
{
  public:
  int8_t dist;            //Number of steps required for capture
  loc_t oloc;             //The capturable piece
  loc_t kt;               //The trap used for capture
  loc_t ploc;             //The capturing piece or ERRLOC if this is a sacrifical cap.
  int8_t baseDist;        //The original number of steps required for capture, pre-defense modification

  inline CapThreat()
  :dist(0),oloc(0),kt(0),ploc(0),baseDist(0)
  {}

  inline CapThreat(int8_t dist, loc_t oloc, loc_t kt, loc_t ploc)
  :dist(dist),oloc(oloc),kt(kt),ploc(ploc),baseDist(dist)
  {}

  inline friend ostream& operator<<(ostream& out, const CapThreat& t)
  {
    out << "Cap " << (int)t.oloc << " by " << (int)t.ploc << " at " << (int)t.kt << " in " << (int)t.dist << " base " << (int)t.baseDist;
    return out;
  }

};

#endif
