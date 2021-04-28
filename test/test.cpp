
/*
 * test.cpp
 * Author: davidwu
 */

#include <cmath>
#include <cstdlib>
#include "../core/global.h"
#include "../core/hash.h"
#include "../core/md5.h"
#include "../core/rand.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardmovegen.h"
#include "../eval/strats.h"
#include "../setups/setup.h"
#include "../test/tests.h"

using namespace std;

static void testBmpBits(uint64_t seed);
static void testBmpGetSet(uint64_t seed);
static void testBmpBoolOps(uint64_t seed);
static void testBmpShifty(uint64_t seed);
static void testBoardStepConsistency(uint64_t seed);
static void testBoardMoveGenConsistency(uint64_t seed);
static void testBoardLegalityChecking(uint64_t seed);
static void testBoardLegalityCornerCases();
static void testRNG();

void Tests::runBasicTests(uint64_t seed)
{
  Rand rand(seed + Hash::simpleHash("runTests"));

  //Test bitmaps----------------------------------
  cout << "----Testing Bitmaps----" << endl;

  cout << "Constructing and retrieving bits" << endl;
  for(int i = 0; i<5000; i++)
  {testBmpBits(rand.nextUInt64());}

  cout << "Seting and retrieving bits" << endl;
  for(int i = 0; i<5000; i++)
  {testBmpGetSet(rand.nextUInt64());}

  cout << "Bitmap boolean operations" << endl;
  for(int i = 0; i<5000; i++)
  {testBmpBoolOps(rand.nextUInt64());}

  cout << "Bitmap shifty" << endl;
  for(int i = 0; i<5000; i++)
  {testBmpShifty(rand.nextUInt64());}

  cout << "----Testing Board----" << endl;

  cout << "Step consistency" << endl;
  for(int i = 0; i<50; i++)
  {testBoardStepConsistency(rand.nextUInt64());}

  cout << "Movegen consistency" << endl;
  for(int i = 0; i<400; i++)
  {testBoardMoveGenConsistency(rand.nextUInt64());}

  cout << "Legality checking" << endl;
  for(int i = 0; i<10; i++)
  {testBoardLegalityChecking(rand.nextUInt64());}

  cout << "Specific legality corner cases" << endl;
  testBoardLegalityCornerCases();

  cout << "----Other----" << endl;

  cout << "Verifying that md5 hash works" << endl;
  char hashResult[33];
  MD5::get("The quick brown fox jumps over the lazy dog",hashResult);
  if(string(hashResult) != string("9e107d9d372bb6826bd81d3542a419d6"))
  {cout << "MD5 hash unexpected: " << hashResult << endl; exit(0);}

  cout << "Verifying that the random number generator gives expected values" << endl;
  testRNG();

  cout << "Testing blockades" << endl;
  testBlockades();
  cout << "Testing ufdist" << endl;
  testUfDist(false);
  cout << "Testing basic search" << endl;
  testBasicSearch();

  cout << "Testing complete!" << endl;
}

static void testBoardLegalityCornerCases()
{
  Board b;
  b = Board::read(
  "P = 1, S = 0, T = 0,\n"
  " +--------+\n"
  "8|.......r|\n"
  "7|.h......|\n"
  "6|.C*..*..|\n"
  "5|..E.....|\n"
  "4|...d....|\n"
  "3|..*..*..|\n"
  "2|..e...C.|\n"
  "1|.......R|\n"
  " +--------+\n"
  "  abcdefgh"
  );

  {
    Board copy = b;
    bool suc1 = copy.makeMoveLegalNoUndo(Board::readMove("g2e"));
    bool suc2 = copy.makeMoveLegalNoUndo(Board::readMove("c5n c6n c7e b7e"));
    assert(suc1 == true);
    assert(suc2 == false);
  }
  {
    Board copy = b;
    UndoMoveData udata;
    UndoMoveData udata2;
    bool suc1 = copy.makeMoveLegalNoUndo(Board::readMove("g2e"),udata);
    bool suc2 = copy.makeMoveLegalNoUndo(Board::readMove("c5n c6n c7e b7e"),udata2);
    assert(suc1 == true);
    assert(suc2 == false);
    copy.undoMove(udata2);
    copy.undoMove(udata);
    assert(Board::areIdentical(copy,b));
  }
  {
    Board copy = b;
    bool suc1 = copy.makeMoveLegalNoUndo(Board::readMove("c5e d4w d5s"));
    assert(suc1 == true);
  }
  {
    Board copy = b;
    bool suc1 = copy.makeMoveLegalNoUndo(Board::readMove("c5e d4w d5s c4n"));
    assert(suc1 == false);
  }
  {
    Board copy = b;
    UndoMoveData udata;
    bool suc1 = copy.makeMoveLegalNoUndo(Board::readMove("c5e d4w d5s"),udata);
    assert(suc1 == true);
    copy.undoMove(udata);
    assert(Board::areIdentical(copy,b));
  }
  {
    Board copy = b;
    UndoMoveData udata;
    bool suc1 = copy.makeMoveLegalNoUndo(Board::readMove("c5e d4w d5s c4n"),udata);
    assert(suc1 == false);
    copy.undoMove(udata);
    assert(Board::areIdentical(copy,b));
  }
}

static void testBoardLegalityChecking(uint64_t seed)
{
  Rand rand(seed);

  move_t* mv = new move_t[512];

  Board b = Board();
  Setup::setupRandom(b,seed);
  Setup::setupRandom(b,seed);

  for(int i = 0; i<200; i++)
  {
    int num = 0;
    num += BoardMoveGen::genSteps(b,b.player,mv+num);
    if(b.step < 3)
      num += BoardMoveGen::genPushPulls(b,b.player,mv+num);

    if(b.step != 0 && b.posCurrentHash != b.posStartHash)
      mv[num++] = PASSMOVE;
    mv[num++] = QPASSMOVE;

    assert(num <= 512);

    UndoMoveData udata;
    vector<UndoMoveData> uDatas;
    for(int numSteps = 1; numSteps <= 4; numSteps++)
    {
      for(int j = 0; j<800; j++)
      {
        move_t move = getMove(
            numSteps >= 1 ? (step_t)rand.nextUInt(256) : ERRSTEP,
            numSteps >= 2 ? (step_t)rand.nextUInt(256) : ERRSTEP,
            numSteps >= 3 ? (step_t)rand.nextUInt(256) : ERRSTEP,
            numSteps >= 4 ? (step_t)rand.nextUInt(256) : ERRSTEP);
        Board copy1 = b;
        Board copy2 = b;
        Board copy3 = b;
        bool suc1 = copy1.makeMoveLegalNoUndo(move);
        bool suc2 = copy2.makeMoveLegalNoUndo(move,udata);
        bool suc3 = copy3.makeMoveLegal(move,uDatas);
        bool maybeSuc = false;
        for(int k = 0; k<num; k++)
          if(getStep(move,0) != ERRSTEP && isPrefix(mv[k],move))
          {maybeSuc = true; break;}
        assert(suc1 == suc2);
        assert(suc1 == suc3);
        if(!maybeSuc)
        {
          assert(suc1 == false);
          assert(suc2 == false);
          assert(suc3 == false);
        }
        assert(Board::areIdentical(copy1,copy2));
        if(suc1)
        {assert(Board::areIdentical(copy2,copy3));}
        copy2.undoMove(udata);
        if(!suc1)
        {
          assert(Board::areIdentical(copy2,copy3));
          assert(Board::areIdentical(copy3,b));
        }
      }
    }
    int r = rand.nextUInt(num);
    bool suc = b.makeMoveLegalNoUndo(mv[r]);
    assert(suc);
  }

  delete[] mv;
}

static void testBoardMoveGenConsistency(uint64_t seed)
{
  Rand rand(seed);

  move_t* mv = new move_t[512];
  int* hm = new int[512];

  Board b = Board();
  Setup::setupRandom(b,seed);
  Setup::setupRandom(b,seed);

  vector<Board> boards;
  vector<UndoMoveData> udatas;
  boards.push_back(b);
  for(int i = 0; i<400; i++)
  {
    int stepNum = BoardMoveGen::genSteps(b,b.player,mv);
    int pushNum = 0;
    if(b.step < 3)
    {pushNum += BoardMoveGen::genPushPulls(b,b.player,mv+stepNum);}

    if((stepNum != 0) != BoardMoveGen::canSteps(b,b.player))
    {cout << "Steps != canSteps" << endl; cout << b << endl; exit(0);}
    if(b.step < 3 && (pushNum != 0) != BoardMoveGen::canPushPulls(b,b.player))
    {cout << "Pushpulls != canPushPull" << endl; cout << b << endl; exit(0);}

    int num = stepNum+pushNum;

    if(b.step != 0 && b.posCurrentHash != b.posStartHash)
    {*(mv+num) = getMove(PASSSTEP); *(hm+num) = 0; num++;}

    if(num == 0)
      break;
    assert(num <= 512);

    for(int j = 0; j<num; j++)
    {
      Board copy = b;
      bool success = copy.makeMoveLegalNoUndo(mv[j]);
      if(!success)
      {cout << "Illegal move generated: " << Board::writeMove(b,mv[j]) << " " << b; exit(0);}
    }
    int r = rand.nextUInt(num);
    b.makeMove(mv[r],udatas);
    boards.push_back(b);
  }

  int size = boards.size();
  for(int i = size - 2; i >= 0; i--)
  {
    if(udatas.size() <= 0)
    {cout << "Ran out of udatas" << endl; exit(0);}
    b.undoMove(udatas);

    if(!b.testConsistency(cout))
    {exit(0);}

    if(!Board::areIdentical(b,boards[i]))
    {cout << "Boards don't match" << endl << b << endl << boards[i] << endl; exit(0);}
  }

  delete[] mv;
  delete[] hm;
}

static void testBoardStepConsistency(uint64_t seed)
{
  Rand rand(seed);

  Board b = Board();
  Setup::setupRandom(b,seed);

  if(!b.testConsistency(cout)) {exit(0);}
  for(int i = 0; i<1500; i++)
  {
    step_t s = rand.nextUInt(256);
    loc_t k0 = gSrc(s);
    loc_t k1 = gDest(s);

    if(k0 == ERRLOC)
    {continue;}

    if(b.owners[k0] != NPLA && b.isThawed(k0) && (b.pieces[k0] != RAB || rabStepOkay(b.owners[k0],s)) && b.owners[k1] == NPLA)
    {
      bool success = b.makeStepLegal(gStepSrcDest(k0,k1));
      if(!success)
      {cout << "Illegal step: " << s << " = (" << k0 << "," << k1 << ")" << endl; cout << b; exit(0);}
    }

    if(!b.testConsistency(cout)) {exit(0);}
  }

  b = Board();
  Setup::setupRandom(b,seed);
  Setup::setupRandom(b,seed);

  if(!b.testConsistency(cout)) {exit(0);}
  for(int i = 0; i<1500; i++)
  {
    step_t s = rand.nextUInt(256);
    loc_t k0 = gSrc(s);
    loc_t k1 = gDest(s);

    if(k0 == ERRLOC)
    {continue;}

    if(b.owners[k0] != NPLA && b.isThawed(k0) && (b.pieces[k0] != RAB || rabStepOkay(b.owners[k0],s)) && b.owners[k1] == NPLA)
    {
      bool success = b.makeStepLegal(gStepSrcDest(k0,k1));
      if(!success)
      {cout << "Illegal step: " << s << " = (" << k0 << "," << k1 << ")" << endl; cout << b; exit(0);}
    }

    if(!b.testConsistency(cout)) {exit(0);}
  }
}

static void testBmpBits(uint64_t seed)
{
  Rand rand(seed);
  uint64_t x = rand.nextUInt64();

  Bitmap b = Bitmap(x);
  Bitmap copy = b;
  bool set[64];

  int manualCount = 0;
  for(int i = 0; i<64; i++)
  {
    set[i] = (((x >> i) & 1ULL) == 1ULL);
    if(set[i])
      manualCount++;
  }

  if(manualCount != b.countBits())
  {
    cout << "manualCount = " << manualCount << endl;
    cout << "b.countBits() = " << b.countBits() << endl;
    copy.print(cout);
    exit(0);
  }

  if(manualCount != b.countBitsIterative())
  {
    cout << "manualCount = " << manualCount << endl;
    cout << "b.countBitsIterative() = " << b.countBitsIterative() << endl;
    copy.print(cout);
    exit(0);
  }

  int last = -1;
  while(b.hasBits())
  {
    int k = gIdx(b.nextBit());
    if(k <= last || k > 63 || !set[k])
    {
      cout << "x = " << Global::uint64ToHexString(x) << endl;
      cout << "k = " << k << endl;
      cout << "last = " << last << endl;
      cout << "set[k] = " << (int)set[k] << endl;
      cout << copy << endl;
      exit(0);
    }
    last = k;
    set[k] = false;
  }

  for(int i = 0; i<64; i++)
  {
    if(set[i])
    {
      cout << "x = " << x << endl;
      cout << "k = " << i << endl;
      copy.print(cout);
      exit(0);
    }
  }
}

static void testBmpGetSet(uint64_t seed)
{
  Rand rand(seed);

  Bitmap b;
  bool set[64];

  for(int i = 0; i<64; i++)
  {set[i] = false;}

  for(int i = 0; i<400; i++)
  {
    if((rand.nextUInt() & 0x8000) == 0)
    {
      int k = rand.nextUInt(64);
      b.setOff(gLoc(k));
      set[k] = false;
    }
    else
    {
      int k = rand.nextUInt(64);
      b.setOn(gLoc(k));
      set[k] = true;
    }
  }

  for(int i = 0; i<400; i++)
  {
    int k = rand.nextUInt(64);
    b.toggle(gLoc(k));
    set[k] = !set[k];
  }

  for(int i = 0; i<64; i++)
  {
    if(set[i] != b.isOne(gLoc(i)))
    {
      cout << "i = " << i << endl;
      cout << b << endl;
      exit(0);
    }
  }

  Bitmap copy = b;

  int last = -1;
  while(b.hasBits())
  {
    int k = gIdx(b.nextBit());
    if(k <= last || k > 63 || !set[k])
    {
      cout << copy << endl;
      exit(0);
    }
    set[k] = false;
  }

  for(int i = 0; i<64; i++)
  {
    if(set[i])
    {
      cout << copy << endl;
      exit(0);
    }
  }
}

static void testBmpBoolOps(uint64_t seed)
{
  Rand rand(seed);

  uint64_t x = rand.nextUInt64();
  uint64_t y = rand.nextUInt64();

  Bitmap bx = Bitmap(x);
  Bitmap by = Bitmap(y);
  Bitmap band = Bitmap(x & y);
  Bitmap bor = Bitmap(x | y);
  Bitmap bnot = Bitmap(~x);
  Bitmap bxor = Bitmap(x ^ y);

  if(!(bx == bx))  {cout << "==" << endl; cout << bx << endl; exit(0);}
  if((bx == by) != (x == y))  {cout << "==" << endl; cout << bx << endl; cout << by << endl; exit(0);}
  if((bx != by) != (x != y))  {cout << "!=" << endl; cout << bx << endl; cout << by << endl; exit(0);}

  Bitmap temp;

  temp = bx;
  temp &= by;
  if(band != temp)  {cout << "and=" << endl; cout << bx << endl; cout << by << endl; cout << temp << endl; exit(0);}

  temp = bx;
  temp |= by;
  if(!(bor == temp))  {cout << "or=" << endl; cout << bx << endl; cout << by << endl; cout << temp << endl; exit(0);}

  temp = bx;
  temp ^= by;
  if(bxor != temp)  {cout << "xor=" << endl; cout << bx << endl; cout << by << endl; cout << temp << endl; exit(0);}

  temp = ~bx;
  if(!(bnot == temp))  {cout << "not" << endl; cout << bx << endl; cout << by << endl; cout << temp << endl; exit(0);}

  if(band != (bx & by))  {cout << "and" << endl; cout << bx << endl; cout << by << endl; exit(0);}
  if(bor != (bx | by))  {cout << "or" << endl; cout << bx << endl; cout << by << endl; exit(0);}
  if(bxor != (bx ^ by))  {cout << "xor" << endl; cout << bx << endl; cout << by << endl; exit(0);}

}

static void testBmpShifty(uint64_t seed)
{
  Rand rand(seed);

  uint64_t x = rand.nextUInt64();

  Bitmap orig = Bitmap(x);
  Bitmap bx;
  Bitmap cc;

  //SOUTH-----------------
  bx = Bitmap::shiftS(orig);
  for(int i = 56; i<64; i++)
  {if(bx.isOne(gLoc(i))) {cout << "s\nx = " << x << endl; exit(0);}}

  cc = orig & ~Bitmap::BMPY[0];
  while(cc.hasBits())
  {if(!bx.hasBits() || cc.nextBit() S != bx.nextBit()) {cout << "s\nx = " << x << endl; exit(0);}}
  if(bx.hasBits()) {cout << "s\nx = " << x << endl; exit(0);}

  //NORTH-----------------
  bx = Bitmap::shiftN(orig);
  for(int i = 0; i<8; i++)
  {if(bx.isOne(gLoc(i))) {cout << "n\nx = " << x << endl; exit(0);}}

  cc = orig & ~Bitmap::BMPY[7];
  while(cc.hasBits())
  {if(!bx.hasBits() || cc.nextBit() N != bx.nextBit()) {cout << "n\nx = " << x << endl; exit(0);}}
  if(bx.hasBits()) {cout << "n\nx = " << x << endl; exit(0);}

  //WEST-----------------
  bx = Bitmap::shiftW(orig);
  for(int i = 7; i<64; i+=8)
  {if(bx.isOne(gLoc(i))) {cout << "w\nx = " << x << endl; exit(0);}}

  cc = orig & ~Bitmap::BMPX[0];
  while(cc.hasBits())
  {if(!bx.hasBits() || cc.nextBit() W != bx.nextBit()) {cout << "w\nx = " << x << endl; exit(0);}}
  if(bx.hasBits()) {cout << "w\nx = " << x << endl; exit(0);}

  //EAST-----------------
  bx = Bitmap::shiftE(orig);
  for(int i = 0; i<64; i+=8)
  {if(bx.isOne(gLoc(i))) {cout << "e\nx = " << x << endl; exit(0);}}

  cc = orig & ~Bitmap::BMPX[7];
  while(cc.hasBits())
  {if(!bx.hasBits() || cc.nextBit() E != bx.nextBit()) {cout << "e\nx = " << x << endl; exit(0);}}
  if(bx.hasBits()) {cout << "e\nx = " << x << endl; exit(0);}

  //ADJ------------------
  bx = Bitmap::adj(orig);
  Bitmap c;
  c |= Bitmap::shiftS(orig);
  c |= Bitmap::shiftW(orig);
  c |= Bitmap::shiftE(orig);
  c |= Bitmap::shiftN(orig);

  if(bx != c) {cout << "a\nx = " << x << endl; exit(0);}

}

uint64_t Tests::hashGame(const GameRecord& game, uint64_t seed)
{
  const Board& b = game.board;
  const vector<move_t>& moves = game.moves;
  uint64_t movesHash = 0;
  for(int i = (int)moves.size()-1; i >= 0; i--)
    movesHash = movesHash * 37 + (uint64_t)moves[i];
  return ((movesHash ^ (uint64_t)game.moves.size()) + seed) ^ (uint64_t)b.sitCurrentHash;
}

void Tests::perturbBoard(Board& b, Rand& rand)
{
  int numRandomPieces = rand.nextInt(2,8);
  for(int j = 0; j<numRandomPieces; j++)
    b.setPiece(gLoc(rand.nextUInt(64)),rand.nextUInt(2),rand.nextInt(RAB,ELE));

  //Resolve captures
  for(int trapIndex = 0; trapIndex<4; trapIndex++)
  {
    loc_t kt = Board::TRAPLOCS[trapIndex];
    if(b.owners[kt] != NPLA && b.trapGuardCounts[b.owners[kt]][trapIndex] == 0)
      b.setPiece(kt,NPLA,EMP);
  }

  //Remove rabbits that are winning the game
  for(int x = 0; x<8; x++)
  {
    if(b.owners[gLoc(x,0)] == SILV && b.pieces[gLoc(x,0)] == RAB) b.setPiece(gLoc(x,0),NPLA,EMP);
    if(b.owners[gLoc(x,7)] == GOLD && b.pieces[gLoc(x,7)] == RAB) b.setPiece(gLoc(x,7),NPLA,EMP);
  }

  b.refreshStartHash();
}


static void testRNG()
{
  Rand rand0(0);
  Rand rand1(1);
  Rand rand2(2);
  Rand rand3((uint64_t)0x0000000100000000ULL);
  Rand rand4((uint64_t)0xFFFFFFFFFFFFFFFFULL);
  Rand rand5((uint64_t)(0xFFFFFFFFFFFFFFFFULL + 1ULL));

  uint32_t values[48] = {
      2284190000U,1991260160U,2698633015U,1979211513U,3807442256U,2284190000U,
      319328369U,4294414628U,1309657874U,1785015958U,2292656245U,319328369U,
      2162954259U,836130050U,3871510843U,1345729986U,3755777710U,2162954259U,
      1550243610U,1358275232U,375852642U,3963908980U,2275389431U,1550243610U,
      2374183595U,2494216400U,3602579427U,4105467993U,1369395527U,2374183595U,
      4018218069U,1874127362U,3367930953U,1388971419U,2898470274U,4018218069U,
      3864210252U,2171181368U,2098550638U,2930063681U,64383862U,3864210252U,
      616870580U,1613963168U,1787719648U,1222970598U,1640756330U,616870580U,
  };
  for(int i = 0; i<8; i++)
  {
    assert(rand0.nextUInt() == values[i*6+0]);
    assert(rand1.nextUInt() == values[i*6+1]);
    assert(rand2.nextUInt() == values[i*6+2]);
    assert(rand3.nextUInt() == values[i*6+3]);
    assert(rand4.nextUInt() == values[i*6+4]);
    assert(rand5.nextUInt() == values[i*6+5]);
    /*
    cout << rand0.nextUInt() << "U,";
    cout << rand1.nextUInt() << "U,";
    cout << rand2.nextUInt() << "U,";
    cout << rand3.nextUInt() << "U,";
    cout << rand4.nextUInt() << "U,";
    cout << rand5.nextUInt() << "U,";
    cout << endl;
    */
  }

  uint64_t sum0 = 0;
  for(int i = 0; i<10000000; i++)
    sum0 += rand0.nextUInt();

  uint64_t sum1 = 0;
  for(int i = 0; i<1000000; i++)
  {
    Rand subRand(rand0.nextUInt64());
    sum1 += subRand.nextUInt();
  }
  assert(sum0 == 21466155345590174ULL);
  assert(sum1 == 2149791032498227ULL);

  //cout << sum0 << endl;
  //cout << sum1 << endl;
}
