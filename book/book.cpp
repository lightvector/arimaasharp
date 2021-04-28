/*
 * book.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/board.h"
#include "../book/book.h"

//Differences in position matter more the further advanced you are.
static const double X_EXPAND[8] = {1.0,1.0,1.2,1.4,1.6,1.7,1.7,1.7};
static const double Y_MAPPING[8] = {0.0,1.0,2.0,3.3,4.7,6.2,7.8,9.4};
//Max dist for a single piece
static const double MAX_DIST = 0.5*((Y_MAPPING[7]-Y_MAPPING[0]) + X_EXPAND[7]*7.0);

static double abs(double x)
{
  return x >= 0 ? x : -x;
}

//Distance function for how "far" two locations are, treating advanced locations as further
static double locDistance(pla_t pla, loc_t loc0, loc_t loc1)
{
  double x0 = gX(loc0);
  double x1 = gX(loc1);
  int y0 = 7-Board::GOALYDIST[pla][loc0];
  int y1 = 7-Board::GOALYDIST[pla][loc1];

  double dist = abs(x0-x1)*X_EXPAND[min(y0,y1)] + abs(Y_MAPPING[y0]-Y_MAPPING[y1]);
  return min(MAX_DIST,dist);
}

//Lower bound on the distance between two bitmaps
static double lowerBound(Bitmap map0, Bitmap map1)
{
  //For every unmatched value, we need at least a distance of 1.0 to match it. For symmetry, do the xor and then divide by an extra 2.
  //And if they have different numbers of bits, then we need MAX_DIST to match the leftovers (minus 0.5 to avoid double counting unmatched values).
  return (map1 ^ map0).countBits() / 2.0 + abs(map1.countBits() - map0.countBits()) * (MAX_DIST - 0.5);
}

//Compute the minimum distance needed to move all pieces from map0 to match map1, or vice versa
static double minTotalDistance(pla_t pla, Bitmap map0, Bitmap map1, double distSoFar, double bestSoFar)
{
  if(map0.isEmpty() && map1.isEmpty())
    return distSoFar;
  if(map0.hasBits() && map1.isEmpty())
    return distSoFar + map0.countBits() * MAX_DIST;
  if(map0.isEmpty() && map1.hasBits())
    return distSoFar + map1.countBits() * MAX_DIST;

  loc_t loc0 = map0.nextBit();

  //Recursively try matching pieces from map1 to those on map0, charging ourselves the necessary distance required.
  Bitmap map = map1;
  while(map.hasBits())
  {
    loc_t loc1 = map.nextBit();
    double dist = distSoFar + locDistance(pla,loc0,loc1);
    //If we've already over the best so far and we haven't matched everything, then we can't be the best, so cut
    if(dist + lowerBound(map0,map1) >= bestSoFar)
      continue;
    dist = minTotalDistance(pla,map0,map1 & ~Bitmap::makeLoc(loc1),dist,bestSoFar);
    if(dist < bestSoFar)
      bestSoFar = dist;
  }
  return bestSoFar;
}

static const double PIECE_DISTANCE_SCALE[NUMTYPES] =
{0,0.4,0.6,0.8,2.0,2.5,4.5};

double Book::boardDistance(const Board& b0, const Board& b1, Bitmap mask)
{
  double distance = 0;
  Bitmap pieceMaps0[NUMPLAS][NUMTYPES];
  Bitmap pieceMaps1[NUMPLAS][NUMTYPES];
  for(pla_t pla = SILV; pla <= GOLD; pla++)
  {
    for(int i = 0; i <= NUMTYPES; i++)
    {
      pieceMaps0[pla][i] = b0.pieceMaps[pla][i] & mask;
      pieceMaps1[pla][i] = b1.pieceMaps[pla][i] & mask;
    }
  }

  for(pla_t pla = SILV; pla <= GOLD; pla++)
  {
    for(piece_t piece = RAB; piece <= ELE; piece++)
    {
      double distForPieceType = 0;
      distForPieceType += minTotalDistance(pla,pieceMaps0[pla][piece],pieceMaps1[pla][piece],0,1000000);

      //Small extra distance for having any difference in position at all
      distForPieceType += 0.5 * abs(pieceMaps0[pla][piece].countBits() - pieceMaps1[pla][piece].countBits());
      distForPieceType += 0.5 * (pieceMaps0[pla][piece] ^ pieceMaps1[pla][piece]).countBits();

      distance += PIECE_DISTANCE_SCALE[piece] * distForPieceType;
    }
  }

  //Extra distance for having any difference in global piece placement
  {
    distance += 5*abs(pieceMaps0[GOLD][0].countBits() - pieceMaps1[GOLD][0].countBits());
    Bitmap diff = pieceMaps0[GOLD][0] ^ pieceMaps1[GOLD][0];
    distance += 2*diff.countBits();
    distance += (diff & Board::YINTERVAL[2][7]).countBits();
    distance += (diff & Board::YINTERVAL[3][7]).countBits();
    distance += (diff & Board::YINTERVAL[4][7]).countBits();
  }
  {
    distance += 5*abs(pieceMaps0[SILV][0].countBits() - pieceMaps1[SILV][0].countBits());
    Bitmap diff = pieceMaps0[SILV][0] ^ pieceMaps1[SILV][0];
    distance += 2*diff.countBits();
    distance += (diff & Board::YINTERVAL[0][5]).countBits();
    distance += (diff & Board::YINTERVAL[0][4]).countBits();
    distance += (diff & Board::YINTERVAL[0][3]).countBits();
  }

  //Extra distance for a difference in dominated or frozen pieces
  distance += 8.0 * ((b0.dominatedMap ^ b1.dominatedMap) & mask).countBits();
  distance += 6.0 * ((b0.frozenMap ^ b1.frozenMap) & mask).countBits();

  //Extra distance for having different numbers of trap defenders
  for(pla_t pla = SILV; pla <= GOLD; pla++)
  {
    for(int i = 0; i<4; i++)
    {
      if(!mask.isOne(Board::TRAPLOCS[i]))
        continue;
      distance += 4.0 * abs(b0.trapGuardCounts[pla][i] - b1.trapGuardCounts[pla][i]);
    }
  }

  return distance;
}








