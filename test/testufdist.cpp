/*
 * testufdist.cpp
 * Author: davidwu
 */

#include "../board/btypes.h"
#include "../board/board.h"
#include "../eval/internal.h"
#include "../test/tests.h"

static void printUfDist(const Board& b, int ufDist[BSIZE])
{
  const char* chars = "0123456789ABCDEF";

  for(int y = 7; y >= 0; y--)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      cout << (b.owners[loc] == NPLA ? '#' : chars[ufDist[loc]]) << " ";
    }
    cout << endl;
  }
  cout << endl;
}

void Tests::testUfDist(bool print)
{
  cout << "Testing..." << endl;

  Board b;
  int ufDist[BSIZE];
  Bitmap pStrongerMaps[2][NUMTYPES];

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "Rc*..*.."
    ".C...d.."
    "..D...c."
    "..*..*Cr"
    "........"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 1);
  assert(ufDist[Board::readLoc("b5")] == 0);
  assert(ufDist[Board::readLoc("b6")] == 0);
  assert(ufDist[Board::readLoc("h3")] == 1);
  assert(ufDist[Board::readLoc("g4")] == 0);
  assert(ufDist[Board::readLoc("g3")] == 0);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "Rd*..*.."
    ".C...d.."
    "..D...c."
    "..*..*Dr"
    "........"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("b5")] == 1);
  assert(ufDist[Board::readLoc("b6")] == 0);
  assert(ufDist[Board::readLoc("h3")] == 2);
  assert(ufDist[Board::readLoc("g4")] == 1);
  assert(ufDist[Board::readLoc("g3")] == 0);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "Rd*..*.."
    ".....d.."
    "..D....."
    "..*..*Dr"
    "........"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  //if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("b6")] == 0);
  assert(ufDist[Board::readLoc("h3")] == 3);
  assert(ufDist[Board::readLoc("g3")] == 0);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    ".......d"
    "Rd*..*.."
    ".R......"
    "......r."
    "..*..*Dr"
    "D......."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("b5")] == 3);
  assert(ufDist[Board::readLoc("b6")] == 0);
  assert(ufDist[Board::readLoc("h3")] == 3);
  assert(ufDist[Board::readLoc("g4")] == 3);
  assert(ufDist[Board::readLoc("g3")] == 0);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "Rc*.e*.."
    ".Cd.Hd.."
    "..Dh.Dc."
    "..*E.*Cr"
    "........"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("b5")] == 2);
  assert(ufDist[Board::readLoc("c4")] == 1);
  assert(ufDist[Board::readLoc("h3")] == 3);
  assert(ufDist[Board::readLoc("g4")] == 2);
  assert(ufDist[Board::readLoc("e5")] == 1);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "Rc*..*.."
    "r......h"
    "H......R"
    "..*..*Cr"
    "........"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("a5")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);
  assert(ufDist[Board::readLoc("h4")] == 2);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "Rc*..*.."
    "rr.....h"
    "H.....RR"
    "..*..*Cr"
    "........"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("a5")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 4);
  assert(ufDist[Board::readLoc("h4")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    ".H......"
    "Rh*..*.."
    "........"
    "........"
    "..*..*Hr"
    "......h."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("h3")] == 3);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..H....."
    "Rh*..*.."
    "........"
    "........"
    "..*..*Hr"
    ".....h.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    ".H......"
    "Ch*..*.."
    "........"
    "........"
    "..*..*Hc"
    "......h."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 1);
  assert(ufDist[Board::readLoc("h3")] == 1);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "rH......"
    "Rh*..*.."
    "........"
    "........"
    "..*..*Hr"
    "......hR"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);
  assert(ufDist[Board::readLoc("a7")] == 4);
  assert(ufDist[Board::readLoc("h2")] == 4);

  //This one is only 1 because it considers the rabbit not immo.
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "rH......"
    "R.*..*.."
    "h......."
    ".......H"
    "..*..*.r"
    "......hR"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 1);
  assert(ufDist[Board::readLoc("h3")] == 1);


  //This one has ufdist only 3 because it can't distinguish between the horse being
  //where it is, and on a5, where the ufdist would really be 3.
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "r.H....."
    "Rh*..*.."
    "........"
    "........"
    "..*..*Hr"
    ".....h.R"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("h3")] == 3);

  //This one has ufdist only 4 because it can't distinguish between the horse being
  //where it is, and on a5, where the ufdist would really be 4.
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "r..H...."
    "Rh*..*.."
    "........"
    "........"
    "..*..*Hr"
    ".......R"
    ".....h.."
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "rH......"
    "Ch*..*.."
    "........"
    "........"
    "..*..*Hc"
    "......hR"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);


  b = Board::read(
    "P = 1, S = 0\n"
    ".H......"
    "r......."
    "Ch*..*.."
    "........"
    "........"
    "..*..*Hc"
    "....h..R"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("h3")] == 4);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dD......"
    "Rd*..*.."
    "........"
    "........"
    "..*..*Hr"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 6);
  assert(ufDist[Board::readLoc("h3")] == 6);
  assert(ufDist[Board::readLoc("a7")] == 0);
  assert(ufDist[Board::readLoc("h2")] == 0);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dD......"
    "Cd*..*.."
    "........"
    "........"
    "..*..*Hc"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 6);
  assert(ufDist[Board::readLoc("h3")] == 6);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "cH......"
    "Rh*..*.."
    "r......."
    ".......R"
    "..*..*Hr"
    "......hC"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "cH......"
    "Ch*..*.."
    "r......."
    ".......R"
    "..*..*Hc"
    "......hC"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "cH......"
    "Ch*..*.."
    "d......."
    ".......D"
    "..*..*Hc"
    "......hC"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);


  //Same problem as before - it can't distinguish between this case and the one
  //where the b6 square is empty, in which case the ufdist would really be 3
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "c.H....."
    "Ch*..*.."
    "d......."
    ".......D"
    "..*..*Hc"
    ".....h.C"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("h3")] == 3);


  b = Board::read(
    "P = 1, S = 0\n"
    "r......."
    "rH......"
    "Rh*..*.."
    "........"
    "........"
    "..*..*Hr"
    "......hR"
    ".......R"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 6);
  assert(ufDist[Board::readLoc("h3")] == 6);
  assert(ufDist[Board::readLoc("a7")] == 2);
  assert(ufDist[Board::readLoc("h2")] == 2);


  b = Board::read(
    "P = 1, S = 0\n"
    "r......."
    "rH......"
    "Ch*..*.."
    "........"
    "........"
    "..*..*Hc"
    "......hR"
    ".......R"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);
  assert(ufDist[Board::readLoc("a7")] == 2);
  assert(ufDist[Board::readLoc("h2")] == 2);

  //Note that between this and the next test, the definition of "ufDist" is a
  //little bit inconsistent. But it's not too bad.
  b = Board::read(
    "P = 1, S = 0\n"
    "r......."
    "rH......"
    "Rh*..*.."
    ".......c"
    "C......."
    "..*..*Hr"
    "......hR"
    ".......R"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 1);
  assert(ufDist[Board::readLoc("h3")] == 1);

  //Note that between this and the previous test, the definition of "ufDist" is a
  //little bit inconsistent. But it's not too bad
  b = Board::read(
    "P = 1, S = 0\n"
    "r......."
    "rH......"
    "Rh*..*.."
    "C......."
    ".......c"
    "..*..*Hr"
    "......hR"
    ".......R"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "r......."
    "rH......"
    "Ch*..*.."
    "C......."
    ".......c"
    "..*..*Hc"
    "......hR"
    ".......R"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "r......."
    "dH......"
    "Ch*..*.."
    "C......."
    ".......c"
    "..*..*Hc"
    "......hD"
    ".......R"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "hH......"
    "Cd*..*.."
    "........"
    "........"
    "..*..*Dc"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "hH......"
    "Rd*..*.."
    "........"
    "........"
    "..*..*Dr"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "h..H...."
    "Rd*..*.."
    "........"
    "........"
    "..*..*Dr"
    ".....h.H"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 3);

  //Slightly inaccurate, but acceptable for now.
  b = Board::read(
    "P = 1, S = 0\n"
    "rr......"
    "rHr....."
    "Rh*..*.."
    "C......."
    ".......c"
    "..*..*Hr"
    ".....RhR"
    "......RR"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 2);
  assert(ufDist[Board::readLoc("h3")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dD......"
    "Rd*..*.."
    "R......."
    ".......r"
    "..*..*Hr"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 6);
  assert(ufDist[Board::readLoc("h3")] == 6);
  assert(ufDist[Board::readLoc("a7")] == 0);
  assert(ufDist[Board::readLoc("h2")] == 0);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dD......"
    "Rd*..*.."
    ".......r"
    "R......."
    "..*..*Hr"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 6);
  assert(ufDist[Board::readLoc("h3")] == 6);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "r......."
    "Rr*..*.."
    ".......r"
    "R......."
    "..*..*Rr"
    ".......R"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 4);
  assert(ufDist[Board::readLoc("h3")] == 4);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "r......."
    "Rr*..*.."
    "R......."
    ".......r"
    "..*..*Rr"
    ".......R"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 6);
  assert(ufDist[Board::readLoc("h3")] == 6);


  //The rabbit ufdist is only 1 because ufdist doesn't consider it immo
  //The dog and horse ufdists are 4 because their only escape square is into an unsafe trap.
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dD......"
    "Rd*..*.."
    ".E......"
    "......m."
    "..*..*Hr"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 1);
  assert(ufDist[Board::readLoc("h3")] == 1);
  assert(ufDist[Board::readLoc("b6")] == 4);
  assert(ufDist[Board::readLoc("g3")] == 4);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dDr....."
    "Rd*..*.."
    ".E......"
    "......m."
    "..*..*Hr"
    ".....RhH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 1);
  assert(ufDist[Board::readLoc("h3")] == 1);
  assert(ufDist[Board::readLoc("b6")] == 2);
  assert(ufDist[Board::readLoc("g3")] == 2);


  //Because the rabbit isn't considered immo
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dDr....."
    "Rdr..*.."
    ".E......"
    "......m."
    "..*..RHr"
    ".....RhH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 1);
  assert(ufDist[Board::readLoc("h3")] == 1);
  assert(ufDist[Board::readLoc("b6")] == 0);
  assert(ufDist[Board::readLoc("g3")] == 0);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dD......"
    "Rd*..*.."
    "......m."
    ".E......"
    "..*..*Hr"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("h3")] == 3);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dD......"
    "Rd*..*.."
    "E......."
    ".......m"
    "..*..*Hr"
    "......hH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("h3")] == 3);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "dDr....."
    "Rdr..*.."
    "E......."
    ".......m"
    "..*..RHr"
    ".....RhH"
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a6")] == 3);
  assert(ufDist[Board::readLoc("h3")] == 3);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    ".Dr....."
    "..*..*.."
    "..e....."
    ".....E.."
    "..*..*.."
    ".....Rd."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("c7")] == 1);
  assert(ufDist[Board::readLoc("f2")] == 1);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    "..*..*.."
    "..e....."
    ".....E.."
    "..*..*.."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 2);
  assert(ufDist[Board::readLoc("e2")] == 2);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    "..*E.*.."
    "..e....."
    ".....M.."
    "..*.m*.."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 6);
  assert(ufDist[Board::readLoc("e2")] == 6);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    ".r*E.*.."
    "..e....."
    ".....M.."
    "..*.m*R."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 3);
  assert(ufDist[Board::readLoc("e2")] == 3);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    "..*H.*.."
    "..e....."
    ".....M.."
    "..*.h*.."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 3);
  assert(ufDist[Board::readLoc("e2")] == 3);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    "..*H.*.."
    ".....M.."
    "..e....."
    "..*.h*.."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 4);
  assert(ufDist[Board::readLoc("e2")] == 4);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    ".r*E.*.."
    ".....M.."
    "..e....."
    "..*.m*R."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 4);
  assert(ufDist[Board::readLoc("e2")] == 4);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    "..*E.*.."
    ".....M.."
    "..e....."
    "..*.m*.."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 6);
  assert(ufDist[Board::readLoc("e2")] == 6);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    ".rrE.*.."
    "..e....."
    ".....M.."
    "..*.mRR."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 6);
  assert(ufDist[Board::readLoc("e2")] == 6);

  //Currently don't know how to traverse own plas as obstacles
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    "..rE.*.."
    "..e....."
    ".....M.."
    "..*.mR.."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 6);
  assert(ufDist[Board::readLoc("e2")] == 6);


  //Currently don't know how to traverse own plas as obstacles
  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "...Dr..."
    "..*rE*.."
    "...e...."
    "....M..."
    "..*mR*.."
    "...Rd..."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("e7")] == 6);
  assert(ufDist[Board::readLoc("d2")] == 6);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..Dr...."
    ".r*E.*.."
    "..r..M.."
    "..e..R.."
    "..*.m*R."
    "....Rd.."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d7")] == 6);
  assert(ufDist[Board::readLoc("e2")] == 6);



  b = Board::read(
    "P = 1, S = 0\n"
    "rr......"
    "RRr....."
    "..*..*.."
    "........"
    "........"
    "..*..*.."
    ".....Rrr"
    "......RR"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a7")] == 6);
  assert(ufDist[Board::readLoc("b7")] == 6);
  assert(ufDist[Board::readLoc("g2")] == 6);
  assert(ufDist[Board::readLoc("h2")] == 6);


  b = Board::read(
    "P = 1, S = 0\n"
    "rr.M...."
    "RRr....."
    "..*..*.."
    "........"
    "........"
    "..*e.*.."
    ".....Rrr"
    "......RR"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("a7")] == 4);
  assert(ufDist[Board::readLoc("b7")] == 3);
  assert(ufDist[Board::readLoc("g2")] == 4);
  assert(ufDist[Board::readLoc("h2")] == 5);


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*..*.."
    "........"
    "...e...."
    ".C*M.*.."
    ".r.d...."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d2")] == 2);
  assert(ufDist[Board::readLoc("b2")] == 3);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*..*.."
    "........"
    "...e...."
    ".C*M.*.."
    ".rRd...."
    "........"
    "\n"
  );
  b.initializeStrongerMaps(pStrongerMaps);
  UFDist::get(b,ufDist,pStrongerMaps);
  if(print) printUfDist(b,ufDist);
  assert(ufDist[Board::readLoc("d2")] == 2);
  assert(ufDist[Board::readLoc("b2")] == 4);

  cout << "Success, all ufdists gave expected results" << endl;
}

