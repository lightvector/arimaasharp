/*
 * piecesquare.cpp
 * Author: davidwu
 */

#include <cstdio>
#include "../core/global.h"
#include "../board/board.h"
#include "../eval/internal.h"

//Multiply by 2.5
static const int PSSCOREFACTOR = 5;
static const int PSSCOREDIV = 2;

//Top is the player's side, bottom the opponent's side
static const int PIECETABLES[][64] =
{{
//ELEPHANT
-450,-350,-250,-180,-180,-250,-350,-450,
-160,-130, -95, -70, -70, -95,-130,-160,
 -90, -45,   0,   0,   0,   0, -45, -90,
 -60,  -5, +15, +25, +25, +15,  -5, -60,
 -60,  +8, +35, +35, +35, +35,  +8, -60,
 -90, -70, -10, +35, +35, -10, -70, -90,
-180,-140,-115, -70, -70,-115,-140,-180,
-450,-350,-250,-180,-180,-250,-350,-450,
},{
//CAMEL or HORSE w CAMEL captured
-150, -80, -70, -50, -50, -70, -80,-150,
 -35, -15, -13, -10, -10, -13, -15, -35,
  -4,  +2,  +1,  +1,  +1,  +1,  +2,  -4,
   0,  +5,  +5,  +1,  +1,  +5,  +5,   0,
  +6, +15, +10,   0,   0, +10, +15,  +6,
  +6, +22, +10,  -5,  -5, +10, +22,  +6,
   0,  +5, +20,  -5,  -5, +20,  +5,   0,
 -50, -25, -15, -15, -15, -15, -25, -50,
},{
//HORSE or DOG with much stuff captured
-150, -90, -80, -60, -60, -80, -90,-150,
 -35, -16, -12, -11, -11, -12, -16, -35,
 -20, +11,  -5,  +5,  +5,  -5, +11, -20,
   0, +10, +10,  +5,  +5, +10, +10,   0,
  +5, +25, +20, +10, +10, +20, +25,  +5,
 +15, +50, +15, +12, +12, +15, +50, +15,
   0, +25, +40, +15, +15, +40, +25,   0,
 -40, -15, -15, -15, -15, -15, -15, -40,
},{
//DOG with stuff captured
-100, -75, -65, -42, -42, -65, -75,-100,
 -22, -10,  -9,  -9,  -9,  -9, -10, -22,
  -5,  +6,  -2,  +3,  +3,  -2,  +6,  -5,
   0, +10, +10,  +6,  +6, +10, +10,   0,
  +8, +14, +16,  +8,  +8, +16, +14,  +8,
 +10, +20,  +8,  +6,  +6,  +8, +20, +10,
   0, +12, +12,  +6,  +6, +12, +12,   0,
 -25, -12, -12, -12, -12, -12, -12, -25,
},{
//DOG or CAT with much stuff captured
 -80, -60, -45, -25, -25, -45, -60, -80,
 -16,  -4,  +2,  -4,  -4,  +2,  -4, -16,
  -4,  +8,   0,  +4,  +4,   0,  +8,  -4,
   0,  +8,  +8,  +6,  +6,  +8,  +8,   0,
  +4, +12, +12,  +8,  +8, +12, +12,  +4,
  +8, +12,  +4,  +6,  +6,  +4, +12,  +8,
   0,  +4,  +8,  +4,  +4,  +8,  +4,   0,
 -20, -12, -12, -12, -12, -12, -12, -20,
},{
//CAT with some stuff captured
 -60, -45, -35, -25, -25, -35, -45, -60,
 -12,  -6,  +6,  -4,  -4,  +6,  -6, -12,
  -2,  +6,   0,  +6,  +6,   0,  +6,  -2,
   0,  +6,  +6,  +4,  +4,  +6,  +6,   0,
  +4,  +6,  +4,  +2,  +2,  +4,  +6,  +4,
   0,  +4,   0,  -2,  -2,   0,  +2,   0,
  -8,  -8,   0,  -8,  -8,   0,  -8,  -8,
 -20, -12, -12, -12, -12, -12, -12, -20,
},{
//CAT
 -50, -35, -28, -21, -21, -28, -35, -50,
  -8,  -4,  +8,   0,   0,  +8,  -4,  -8,
  -2,  +6,   0,  +4,  +4,   0,  +6,  -2,
   0,  +6,  +6,  +3,  +3,  +6,  +6,   0,
  +2,  +4,  +4,  +2,  +2,  +4,  +4,  +2,
   0,  +2,  -2,  -4,  -4,  -2,  +2,   0,
 -10, -10,  -2, -10, -10,  -2, -10, -10,
 -20, -12, -12, -12, -12, -12, -12, -20,
}};

static const int RABBITTABLE[64] =
{
  +1,  +1,   0,  -1,  -1,   0,  +1,  +1,
  +2,  +1,  +8,  -8,  -8,  +8,  +1,  +2,
  +4,  +3, -12,  -8,  -8, -12,  +3,  +4,
  +6,  +2,  -4, -13, -13,  -4,  +2,  +6,
  +8,  +4,  -6, -16, -16,  -6,  +4,  +8,
 +10,  +6, -10, -24, -24, -10,  +6, +10,
 +12, +12, -10,  +0,  +0, -10, +10, +12,
+999,+999,+999,+999,+999,+999,+999,+999,
};

//Modifier by numOppPieces*2 + numOppRabs, by row of advancement
static const int RABBITADVANCETABLE[25][8] =
{
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0},
{0, 9, 20, 33, 49, 68,118,0}, //4P7R or 5P5R or 6P3R or 7P1R
{0, 9, 20, 33, 49, 68,118,0}, //4P6R or 5P4R or 6P2R or 7P
{0, 9, 20, 33, 49, 68,118,0}, //3P7R or 4P5R or 5P3R or 6P1R
{0, 8, 17, 29, 43, 60,105,0}, //3P6R or 4P4R or 5P2R or 6P
{0, 7, 15, 25, 38, 53, 94,0}, //2P7R or 3P5R or 4P3R or 5P1R
{0, 6, 13, 22, 34, 47, 84,0}, //2P6R or 3P4R or 4P2R or 5P
{0, 5, 11, 19, 29, 41, 74,0}, //1P7R or 2P5R or 3P3R or 4P1R
{0, 4,  9, 16, 24, 33, 64,0}, //1P6R or 2P4R or 3P2R or 4P
{0, 3,  7, 13, 20, 27, 54,0}, //7R or 1P5R or 2P3R or 3P1R
{0, 3,  5, 11, 16, 21, 44,0}, //6R or 1P4R or 2P2R or 3P
{0, 2,  4,  7, 10, 13, 35,0}, //5R or 1P3R or 2P1R
{0, 1,  2,  4,  5,  7, 26,0}, //4R or 1P2R or 2P
{0, 0,  0,  0,  0,  0, 17,0}, //3R or 1P1R
{0, 0, -2, -5, -8,-12,  8,0}, //2R or 1P
{0,-3, -6,-12,-20,-27, -6,0}, //1R
{0,-6,-12,-21,-32,-45,-18,0}, //All
};

eval_t PieceSquare::get(const Board& b, const int pStronger[2][NUMTYPES])
{
  int score[2] = {0,0};
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t i = gLoc(x,y);
      pla_t owner = b.owners[i];
      if(owner == NPLA)
        continue;
      int yRow = owner == GOLD ? y : 7-y;
      idx_t idx = gIdx(x,yRow);
      piece_t piece = b.pieces[i];
      int dScore = 0;

      int numPStronger = pStronger[owner][piece];
      if(piece == RAB)
      {dScore = RABBITTABLE[idx] + RABBITADVANCETABLE[numPStronger*2+b.pieceCounts[gOpp(owner)][RAB]][yRow];}
      else
      {dScore = PIECETABLES[numPStronger][idx];}

      score[owner] += dScore;
    }
  }
  int goldScore = score[GOLD] - score[SILV];
  goldScore = goldScore*PSSCOREFACTOR/PSSCOREDIV;

  if(b.player == GOLD) return goldScore;
  else return -goldScore;
}

void PieceSquare::print(const Board& b, const int pStronger[2][NUMTYPES], ostream& out)
{
  for(int y = 7; y>=0; y--)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t i = gLoc(x,y);
      pla_t owner = b.owners[i];
      if(owner == NPLA)
        out << Global::strprintf("%+4d ",0);
      else
      {
        int yRow = owner == GOLD ? y : 7-y;
        idx_t idx = gIdx(x,yRow);
        piece_t piece = b.pieces[i];
        int dScore = 0;

        int numPStronger = pStronger[owner][piece];
        if(piece == RAB)
        {dScore = RABBITTABLE[idx] + RABBITADVANCETABLE[numPStronger*2+b.pieceCounts[gOpp(owner)][RAB]][yRow];}
        else
        {dScore = PIECETABLES[numPStronger][idx];}

        out << Global::strprintf("%+4d ",dScore*PSSCOREFACTOR/PSSCOREDIV);
      }
      if(x == 7) out << endl;
    }
  }
}



