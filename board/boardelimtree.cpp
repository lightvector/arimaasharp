
/*
 * boardelimgen.cpp
 * Author: davidwu
 *
 * Elimination detection functions
 */

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/offsets.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"

using namespace std;

bool BoardTrees::canElim(Board& b, pla_t pla, int numSteps)
{
  //0 Rabbits - eliminated already!
  pla_t opp = gOpp(pla);
  if(b.pieceCounts[opp][RAB] <= 0)
    return true;

  //Impossible to elim if > 2 rabbits or if numSteps < 2
  if(b.pieceCounts[opp][RAB] > 2 || numSteps < 2)
    return false;

  //1 Rabbit
  if(b.pieceCounts[opp][RAB] == 1)
  {
    Bitmap rMap = b.pieceMaps[opp][RAB];
    loc_t rloc = rMap.nextBit();

    //If there are 1 and it is not within radius 2 of a trap, impossible.
    //Or, if we have fewer than 4 steps, and it's not within radius 1, impossible
    int requiredRad = numSteps < 4 ? 1 : 2;
    if(Board::CLOSEST_TDIST[rloc] > requiredRad)
      return false;

    //Check each trap around to see if this rabbit is capturable.
    for(int i = 0; i<4; i++)
    {
      loc_t trap = Board::TRAPLOCS[i];
      //Can't capture if we are too far from trap, or if we are at rad 2 but trap has a defender
      if(Board::manhattanDist(rloc,trap) > requiredRad || (Board::manhattanDist(rloc,trap) == 2 && b.trapGuardCounts[opp][i] >= 1))
        continue;
      int capDist = 5;
      Bitmap capMap;
      if(BoardTrees::canCaps(b,pla,numSteps,trap,capMap,capDist))
      {
        //Can capture! Yay!
        if(capMap.isOne(rloc))
          return true;
      }
    }
    return false;
  }
  //2 Rabbits
  else
  {
    //If we have less than 4 steps, impossible
    if(numSteps < 4)
      return false;

    //Locate rabbits
    loc_t rabbits[2];
    Bitmap rMap = b.pieceMaps[opp][RAB];
    rabbits[0] = rMap.nextBit();
    rabbits[1] = rMap.nextBit();

    //Both rabbits must be next to traps with only 1 defender
    loc_t traps[2];
    for(int r = 0; r<=1; r++)
    {
      loc_t rloc = rabbits[r];
      loc_t nearestTrap = Board::CLOSEST_TRAP[rloc];
      int mDist = Board::manhattanDist(nearestTrap,rloc);
      if(mDist > 1 || b.trapGuardCounts[opp][Board::TRAPINDEX[nearestTrap]] > 1)
        return false;

      traps[r] = nearestTrap;
    }

    //For each rabbit, try to generate all 2 step captures of it, then see if we can capture the other rabbit
    move_t mv[64];
    int hm[64];
    for(int r = 0; r<=1; r++)
    {
      if(r == 1 && traps[1] == traps[0])
          break;

      int num = genCaps(b,pla,2,2,traps[r],mv,hm);

      //Try each capture
      for(int i = 0; i<num; i++)
      {
        move_t move = mv[i];
        int ns = numStepsInMove(move);
        if(ns != 2)
          continue;

        //Temp make the move, recording the movement of the rabbits
        bool capturesBoth = false;
        bool capturesRabbit = false;
        TempRecord records[ns];
        loc_t rabbitDests[2];
        rabbitDests[0] = rabbits[0];
        rabbitDests[1] = rabbits[1];
        for(int j = 0; j<ns; j++)
        {
          step_t step = getStep(move,j);
          loc_t k0 = gSrc(step);
          loc_t k1 = gDest(step);
          records[j] = b.tempStepC(k0,k1);

          if     (k0 == rabbitDests[0]) rabbitDests[0] = k1;
          else if(k0 == rabbitDests[1]) rabbitDests[1] = k1;

          if(records[j].capOwner == opp && records[j].capPiece == RAB)
          {
            capturesRabbit = true;
            if     (records[j].capLoc == rabbitDests[0]) rabbitDests[0] = ERRLOC;
            else if(records[j].capLoc == rabbitDests[1]) rabbitDests[1] = ERRLOC;
          }
        }

        //If it does indeed capture the rabbit
        if(capturesRabbit)
        {
          //Locate the other rabbit
          loc_t rloc = ERRLOC;
          if     (rabbitDests[0] != ERRLOC) rloc = rabbitDests[0];
          else if(rabbitDests[1] != ERRLOC) rloc = rabbitDests[1];
          else Global::fatalError("BoardElimTree both rabbits captured?");

          //Check to see that the other rabbit is capturable
          loc_t nextTrap = Board::CLOSEST_TRAP[rloc];
          //On the trap?
          if(nextTrap == rloc)
          {
            loc_t oloc = b.findGuard(opp,nextTrap);
            bool isOpen = b.isOpen(oloc);
            if(
            (b.owners[oloc S] == pla && b.pieces[oloc S] > b.pieces[oloc] && b.isThawedC(oloc S) && (isOpen || b.isOpen(oloc S))) ||
            (b.owners[oloc W] == pla && b.pieces[oloc W] > b.pieces[oloc] && b.isThawedC(oloc W) && (isOpen || b.isOpen(oloc W))) ||
            (b.owners[oloc E] == pla && b.pieces[oloc E] > b.pieces[oloc] && b.isThawedC(oloc E) && (isOpen || b.isOpen(oloc E))) ||
            (b.owners[oloc N] == pla && b.pieces[oloc N] > b.pieces[oloc] && b.isThawedC(oloc N) && (isOpen || b.isOpen(oloc N)))
            )
              capturesBoth = true;
          }
          //Otherwise, adjacent to the trap (as checked earlier above)
          else
          {
            if(b.owners[nextTrap] == NPLA && b.isDominatedByUF(rloc))
              capturesBoth = true;
          }
        }

        //Temp undo the move
        for(int j = ns-1; j>=0; j--)
          b.undoTemp(records[j]);

        if(capturesBoth)
          return true;
      }
    }
  }
  return false;
}








