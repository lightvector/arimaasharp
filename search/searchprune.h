/*
 * searchprune.h
 * Author: davidwu
 */

#ifndef SEARCHPRUNE_H_
#define SEARCHPRUNE_H_

namespace SearchPrune
{
  //These are all from gold's perspective, silver will have things vertically flipped.
  enum ID {
    C3_SAC_E,F3_SAC_E,  //Sac piece in empty home trap
    C6_SAC_E,F6_SAC_E,  //Sac piece in empty opp trap

    C2_RETREAT, F2_RETREAT, //Needlessly retreat a piece from trap defense

    A2_RETREAT, H2_RETREAT, //Needlessly retreat a piece to back row
    B2_RETREAT, G2_RETREAT, //Needlessly retreat a piece to back row
    D2_RETREAT, E2_RETREAT, //Needlessly retreat a piece to back row

    //Sac piece in pla home trap
    //Sac piece in pla opp trap

    //Retreats needlessly from b3 or g3, or moves needlessly from b3 and g3 to the edge of the board
    //Any move that causes any sac at all in a trap if there are no opp pieces within radius 2 of the trap,
    //and (hmmm -maybe it's cause we needed a piece to go 4 steps away and it couldn't reach).
    //Ah, any move that causes any sac at all in a trap if there are no pieces within radius 2 of the trap
    //and the trap has 2 or more defenders. How about that.
    //Elephant needlessly retreats back to home side
    //Elephant pointlessly dives deep into enemy side
    //Hangs a piece while goal distance globally of any rabbit is large and the move creates no capture threat
    //and dominates no new opponent piece and semidominates no new opponent piece and opponent capture would not hang or sac his own piece.

};

  void findStartMatches(Board& b, vector<ID>& buf); //Buf is cleared
  bool matchesMove(Board& endBoard, pla_t pla, move_t m, const vector<ID>& ids);

  bool canHaizhiPrune(const Board& origBoard, pla_t pla, move_t m);

}


#endif
