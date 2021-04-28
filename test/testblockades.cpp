/*
 * testblockades.cpp
 * Author: davidwu
 */

#include "../board/board.h"
#include "../eval/strats.h"
#include "../test/tests.h"

void Tests::testBlockades()
{
  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|..*R.*..|\n"
  "2|.EeCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 531982ULL && freezeHeldMap.bits == 0ULL);
  }
  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|..*..*..|\n"
  "2|.EeCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 2);
  }
  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|.E*R.*..|\n"
  "2|..eCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 2 && holderHeldMap.bits == 531464ULL);
  }
  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|.RrR.*..|\n"
  "2|RReCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925454ULL && freezeHeldMap.bits == 0ULL);
  }
  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|..r.....|\n"
  "3|.RrR.*..|\n"
  "2|RReCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925454ULL && freezeHeldMap.bits == 0ULL);
  }
  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|...R.*..|\n"
  "2|RreCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 532238ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|...R.*..|\n"
  "2|RceCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 2 && holderHeldMap.bits == 531464ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|..rR.*..|\n"
  "2|RreCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == ERRLOC);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|.rrR.*..|\n"
  "2|RreCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == ERRLOC);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|rrrR.*..|\n"
  "2|RreCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 990990ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|rrrR.*..|\n"
  "2|RceCC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == ERRLOC);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|rrrR.*..|\n"
  "2|CceMC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 990990ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|RrER.*..|\n"
  "2|CceMC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 990990ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|.R......|\n"
  "3|RrER.*..|\n"
  "2|CceMC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 990990ULL && freezeHeldMap.bits == 0ULL);
  }
  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|..R.....|\n"
  "3|RrRR.*..|\n"
  "2|CceMC...|\n"
  "1|.DDR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 68099854ULL && freezeHeldMap.bits == 0ULL);
  }


  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|.DrR.*..|\n"
  "2|rMeHC...|\n"
  "1|RRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925455ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|.DrR.*..|\n"
  "2|hMeHC...|\n"
  "1|RRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925455ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|dDrR.*..|\n"
  "2|hMeHC...|\n"
  "1|RRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925455ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|rDrR.*..|\n"
  "2|hMeHC...|\n"
  "1|dRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 990991ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|cDrR.*..|\n"
  "2|hMeHC...|\n"
  "1|dRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 2 && holderHeldMap.bits == 924680ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|rDrR.*..|\n"
  "2|rMeHC...|\n"
  "1|RRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925455ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|CDrR.*..|\n"
  "2|rMeHC...|\n"
  "1|RRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925455ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|CDrR.*..|\n"
  "2|rdeHC...|\n"
  "1|RRRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925455ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|..R.....|\n"
  "3|.RrR.*..|\n"
  "2|rReHC...|\n"
  "1|RDRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 0 && holderHeldMap.bits == 925455ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|.H......|\n"
  "3|.MD..*..|\n"
  "2|reEHC...|\n"
  "1|RDRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == ERRLOC);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|CcC.....|\n"
  "3|rMD..*..|\n"
  "2|reEHR...|\n"
  "1|RDRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("b2") && immoType == 0 && holderHeldMap.bits == 117901063ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|.r......|\n"
  "4|CcC.....|\n"
  "3|rMD..*..|\n"
  "2|reEHR...|\n"
  "1|RDRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("b2") && immoType == 0 && holderHeldMap.bits == 117901063ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|.EM.C*..|\n"
  "2|remrcrrR|\n"
  "1|RDRRRRR.|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("b2") && immoType == 0 && holderHeldMap.bits == 1245055ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|HrC.....|\n"
  "3|hMR.D*H.|\n"
  "2|R.e..C.R|\n"
  "1|RDR..RRR|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 2 && holderHeldMap.bits == 117900551ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|.RrR.*..|\n"
  "2|RReCC...|\n"
  "1|.D.R...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 1 && holderHeldMap.bits == 925450ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|DRrR.*..|\n"
  "2|RReCC...|\n"
  "1|R.RR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 2 && holderHeldMap.bits == 924680ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|........|\n"
  "3|RrRR.*..|\n"
  "2|RCeCR...|\n"
  "1|.DRR...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == ERRLOC);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|..D.....|\n"
  "3|RrRR.*..|\n"
  "2|RCeCR...|\n"
  "1|.D.R...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 1 && holderHeldMap.bits == 68099850ULL && freezeHeldMap.bits == 0ULL);
  }

  {
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|........|\n"
  "6|..*..*..|\n"
  "5|........|\n"
  "4|..c.....|\n"
  "3|RRRR.*..|\n"
  "2|RCeCR...|\n"
  "1|.D.R...R|\n"
  " +--------+\n"
  "  abcdefgh"
  );
  int immoType;
  Bitmap recursedMap;
  Bitmap holderHeldMap;
  Bitmap freezeHeldMap;
  loc_t loc = Strats::findEBlockade(b,b.player,immoType,recursedMap,holderHeldMap,freezeHeldMap);
  //cout << Board::writeLoc(loc) << " " << immoType << " " << endl << recursedMap << endl << holderHeldMap << endl << freezeHeldMap << endl;
  //cout << holderHeldMap.bits << endl << freezeHeldMap.bits << endl;
  assert(loc == Board::readLoc("c2") && immoType == 1 && holderHeldMap.bits == 925450ULL && freezeHeldMap.bits == 0ULL);
  }
}


