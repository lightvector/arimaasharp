
/*
 * testtree.cpp
 * Author: davidwu
 */

#include <cstdlib>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../board/boardhistory.h"
#include "../board/gamerecord.h"
#include "../search/searchmovegen.h"
#include "../test/tests.h"

using namespace std;

static void printMoves(const Board& b, const move_t* mv, int num)
{
  for(int i = 0; i<num; i++)
    cout << Board::writeMove(b,mv[i]) << endl;
}

typedef bool (*TestTreeFunc)(const Board& b, int trustDepth, int testDepth, move_t* mv);

//Run cap and goal tests
static bool testGoals(const Board& b, int trustDepth, int testDepth, move_t* mv);
static bool testCaps(const Board& b, int trustDepth, int testDepth, move_t* mv);
static bool testRelTactics(const Board& b, int trustDepth, int testDepth, move_t* mv);

static bool runTests(const Board& b, int trustDepth, int testDepth, int numRandomPerturbations, move_t* mv, Rand& gameRand, TestTreeFunc func);
static void runTests(const vector<GameRecord>& games, int trustDepth, int testDepth, int numRandomPerturbations, uint64_t seed, TestTreeFunc func);

//Actually test by search if a goal can happen
static int goalDist(Board& b, pla_t pla, int rdepth, int cdepth, bool trust, int trustDepth, move_t* mv);

//Actually test by search if a piece of the specified type be captured
static bool canCapType(Board& b, pla_t pla, piece_t piece, int rdepth, bool trust, int trustDepth, move_t* mv);
static bool canCapType(Board& b, pla_t pla, piece_t piece, int origCount, int rdepth, int cdepth, bool trust, int trustDepth, move_t* mv);

//TOP LEVEL-------------------------------------------------------------------------------------------------------

void Tests::testGoalTree(const vector<GameRecord>& games, int trustDepth, int testDepth, int numRandomPerturbations, uint64_t seed)
{
  cout << "Testing goal tree, " << numRandomPerturbations << " perturbations per position" << endl;
  cout << "Using seed " << seed << " for each game (adding some hash of the starting board and the move list)" << endl;
  runTests(games,trustDepth,testDepth,numRandomPerturbations,seed,&testGoals);
}
void Tests::testCapTree(const vector<GameRecord>& games, int trustDepth, int testDepth, int numRandomPerturbations, uint64_t seed)
{
  cout << "Testing cap tree, " << numRandomPerturbations << " perturbations per position" << endl;
  cout << "Using seed " << seed << " for each game (adding some hash of the starting board and the move list)" << endl;
  runTests(games,trustDepth,testDepth,numRandomPerturbations,seed,&testCaps);
}
void Tests::testRelTacticsGen(const vector<GameRecord>& games, int numRandomPerturbations, uint64_t seed)
{
  cout << "Testing rel tactics movegen, " << numRandomPerturbations << " perturbations per position" << endl;
  cout << "Using seed " << seed << " for each game (adding some hash of the starting board and the move list)" << endl;
  runTests(games,0,0,numRandomPerturbations,seed,&testRelTactics);
}

void Tests::testElimTree(const vector<GameRecord>& games)
{
  cout << "Unimplemented" << endl;
  (void)games;
  //MINOR automate this
  /*
  cout << "Testing elim tree" << endl;
  for(int i = 0; i<(int)boards.size(); i++)
  {
    cout << boards[i] << endl;
    cout << "P0 Can Elim in 2: " << BoardTrees::canElim(boards[i],SILV,2) << endl;
    cout << "P0 Can Elim in 3: " << BoardTrees::canElim(boards[i],SILV,3) << endl;
    cout << "P0 Can Elim in 4: " << BoardTrees::canElim(boards[i],SILV,4) << endl;
    cout << "P1 Can Elim in 2: " << BoardTrees::canElim(boards[i],GOLD,2) << endl;
    cout << "P1 Can Elim in 3: " << BoardTrees::canElim(boards[i],GOLD,3) << endl;
    cout << "P1 Can Elim in 4: " << BoardTrees::canElim(boards[i],GOLD,4) << endl;
    boards[i].testConsistency(cout);
  }*/
}

static void runTests(const vector<GameRecord>& games, int trustDepth, int testDepth, int numRandomPerturbations, uint64_t seed, TestTreeFunc func)
{
  int numGames = games.size();
  move_t* mv = new move_t[1048576];
  for(int i = 0; i<numGames; i++)
  {
    if(i % 100 == 0)
      cout << "Game " << i << endl;

    Board b = games[i].board;
    vector<move_t> moves = games[i].moves;

    //Make a random generator just for this game
    Rand gameRand(Tests::hashGame(games[i],seed));

    int numMoves = moves.size();
    for(int m = 0; m<numMoves; m++)
    {
      move_t move = moves[m];
      int numSteps = numStepsInMove(move);
      for(int s = 0; s<numSteps; s++)
      {
        step_t step = getStep(move,s);
        bool succ = b.makeStepLegal(step);
        assert(succ);
        bool testGood = runTests(b,trustDepth,testDepth,numRandomPerturbations,mv,gameRand,func);
        if(!testGood)
        {
          cout << "Test failed on game " << i << " move " << m << " step " << s << endl;
          delete[] mv;
          return;
        }
      }
    }
  }
  delete[] mv;
}

static bool runTests(const Board& b, int trustDepth, int testDepth, int numRandomPerturbations, move_t* mv, Rand& gameRand, TestTreeFunc func)
{
  {
    Board copy = b;
    copy.setPlaStep(0,0);
    copy.refreshStartHash();
    Board copy2(copy);
    bool succ = func(copy2,trustDepth,testDepth,mv);
    if(!succ)
    {
      cout << "Failed board:" << endl;
      cout << copy << endl;
      return false;
    }
  }
  {
    Board copy = b;
    copy.setPlaStep(1,0);
    copy.refreshStartHash();
    Board copy2(copy);
    bool succ = func(copy2,trustDepth,testDepth,mv);
    if(!succ)
    {
      cout << "Failed board:" << endl;
      cout << copy << endl;
      return false;
    }
  }

  //Try some random pertrubations
  for(int rpt = 0; rpt < numRandomPerturbations; rpt++)
  {
    Board bb = b;
    Tests::perturbBoard(bb,gameRand);
    {
      Board copy = bb;
      copy.setPlaStep(0,0);
      copy.refreshStartHash();
      Board copy2(copy);
      bool succ = func(copy2,trustDepth,testDepth,mv);
      if(!succ)
      {
        cout << "Test failed on perturbation " << rpt << " / " << numRandomPerturbations << endl;
        cout << "Failed board:" << endl;
        cout << copy << endl;
        return false;
      }
    }
    {
      Board copy = b;
      copy.setPlaStep(1,0);
      copy.refreshStartHash();
      Board copy2(copy);
      bool succ = func(copy2,trustDepth,testDepth,mv);
      if(!succ)
      {
        cout << "Test failed on perturbation " << rpt << " / " << numRandomPerturbations << endl;
        cout << "Failed board:" << endl;
        cout << copy << endl;
        return false;
      }
    }
  }
  return true;
}

static bool testGoals(const Board& b, int trustDepth, int testDepth, move_t* mv)
{
  pla_t pla = b.player;
  Board copy = b;
  int searchDist = goalDist(copy,pla,testDepth,0,true,trustDepth,mv); if(searchDist > testDepth) {searchDist = 5;}
  int treeDist = BoardTrees::goalDist(copy,pla,testDepth);
  move_t gmove = copy.goalTreeMove;
  int gmovelen = numStepsInMove(gmove);
  if(!Board::areIdentical(copy,b))
  {cout << "Tree modified position" << endl; cout << "New position:" << endl << copy << endl; return false;}
  if(searchDist != treeDist)
  {cout << "Search goaldist: " << searchDist << " Tree goaldist: " << treeDist << endl; return false;}
  if(searchDist <= testDepth) //If there was indeed a goal
  {
    if(searchDist == 0)
    {
      if(gmove != ERRMOVE) {cout << "Search goaldist: " << searchDist << " Tree goaldist: " << treeDist << " goalTreeMove: " << Board::writeMove(b,gmove) << endl; return false;}
    }
    else
    {
      Board copycopy = b;
      bool suc = copycopy.makeMoveLegalNoUndo(gmove);
      if(!suc)
      {cout << "goalTreeMove is illegal " << endl;
       cout << "Search goaldist: " << searchDist << " Tree goaldist: " << treeDist << " goalTreeMove: " << Board::writeMove(b,gmove) << endl; return false; }
      if(getStep(gmove,0) == ERRMOVE || copycopy.step != gmovelen % 4)
      {cout << "goalTreeMove is wrong length: " << gmovelen << endl;
       cout << "Search goaldist: " << searchDist << " Tree goaldist: " << treeDist << " goalTreeMove: " << Board::writeMove(b,gmove) << endl; return false; }
      if((gmovelen <= 0 && searchDist > 0) || goalDist(copycopy,pla,searchDist-gmovelen,0,false,trustDepth,mv) != searchDist-gmovelen)
      {cout << "goalTreeMove doesn't goal" << endl;
       cout << "Search goaldist: " << searchDist << " Tree goaldist: " << treeDist << " goalTreeMove: " << Board::writeMove(b,gmove) << endl; return false; }
    }
  }

  return true;
}

static bool testRelTactics(const Board& b, int trustDepth, int testDepth, move_t* mv)
{
  (void)trustDepth;
  (void)testDepth;
  for(int step = 0; step < 4; step++)
  {
    Board copy(b);
    copy.setPlaStep(copy.player,step);
    copy.refreshStartHash();
    Board reference(copy);
    int num = SearchMoveGen::genRelevantTactics(copy,true,mv,NULL);
    if(!Board::areIdentical(copy,reference))
    {
      cout << "SearchMoveGen::genRelevantTactics modified board copy with step set to " << step << endl;
      cout << copy << endl;
      return false;
    }
    UndoMoveData udata;
    for(int i = 0; i<num; i++)
    {
      bool suc = copy.makeMoveLegalNoUndo(mv[i],udata);
      if(!suc)
      {
        cout << "Illegal move with board step set to " << step << endl;
        cout << Board::writeMove(reference,mv[i]) << endl;
        return false;
      }
      copy.undoMove(udata);
    }
  }
  return true;
}

static bool testCaps(const Board& b, int trustDepth, int testDepth, move_t* mv)
{
  move_t capmv[4096];
  int caphm[4096];
  pla_t pla = b.player;
  Board copy = b;
  int num = BoardTrees::genCaps(copy,pla,testDepth,capmv,caphm);
  if(num >= 4096)
  {
    cout << "genCaps produced more than 4096 caps" << endl;
    cout << b << endl;
    assert(false);
  }
  if(!Board::areIdentical(copy,b))
  {cout << "genCaps modified position" << endl; cout << "New position" << endl << copy << endl; return false;}

  if(num < 0)
  {cout << "Negative number of captures generated: " << num << endl; return false;}

  bool canCapTree = BoardTrees::canCaps(copy,pla,testDepth);
  if(!Board::areIdentical(copy,b))
  {cout << "canCaps modified position" << endl; cout << "New position" << endl << copy << endl; return false;}

  bool canCap = canCapType(copy,pla,0,testDepth,true,trustDepth,mv);

  if(canCapTree != (num > 0) || canCap != (num > 0))
  {
    cout << "Num generated caps = " << num << " " << " canCapTree = " << canCapTree << " canCapSearch = " << canCap << endl;
    printMoves(b,capmv,num);
    return false;
  }

  for(int piece = RAB; piece <= ELE; piece++)
  {
    bool canCapTypeSearch = canCapType(copy,pla,piece,testDepth,true,trustDepth,mv);
    bool canCapTypeTree = false;
    for(int j = 0; j<num; j++)
    {
      if(caphm[j] == piece)
      {canCapTypeTree = true; break;}
    }

    if(canCapTypeSearch != canCapTypeTree)
    {
      cout << " canCapSearch " << piece << " = " << canCapTypeSearch
           << " canCapTree " << piece << " = " << canCapTypeTree << endl;
      printMoves(b,capmv,num);
      return false;
    }
  }

  for(int c = 0; c<num; c++)
  {
    move_t move = capmv[c];
    Board copy2(b);
    bool suc = copy2.makeMoveLegalNoUndo(move);
    if(!suc)
    {
      cout << "Captree output illegal move: " << Board::writeMove(move) << endl;
      printMoves(b,capmv,num);
      return false;
    }
    //Capture happened
    if(copy2.pieceCounts[gOpp(pla)][0] < copy.pieceCounts[gOpp(pla)][0])
      continue;

    //Otherwise, make sure capture still possible
    int remainingDepth = testDepth - numStepsInMove(move);
    bool canCap = remainingDepth > 1 && canCapType(copy2,pla,0,remainingDepth,true,trustDepth,mv);
    if(!canCap)
    {
      cout << "Captree output move that doesn't capture: " << Board::writeMove(move) << endl;
      printMoves(b,capmv,num);
      return false;
    }
  }

  num = 0;
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    loc_t kt = Board::TRAPLOCS[trapIndex];
    num += BoardTrees::genCapsFull(copy,pla,testDepth,2,true,kt,capmv+num,caphm+num);
  }
  if(num >= 4096)
  {
    cout << "genCapsFull produced more than 4096 caps" << endl;
    cout << b << endl;
    assert(false);
  }
  if(!Board::areIdentical(copy,b))
  {cout << "genCapsFull modified position" << endl; cout << "New position" << endl << copy << endl; return false;}

  for(int c = 0; c<num; c++)
  {
    move_t move = capmv[c];
    Board copy2(b);
    bool suc = copy2.makeMoveLegalNoUndo(move);
    if(!suc)
    {
      cout << "Captreefull output illegal move: " << Board::writeMove(move) << endl;
      printMoves(b,capmv,num);
      return false;
    }
    //Capture happened
    if(copy2.pieceCounts[gOpp(pla)][0] < copy.pieceCounts[gOpp(pla)][0])
      continue;

    //Otherwise, make sure capture still possible
    cout << "Captreefull output move that doesn't capture: " << Board::writeMove(move) << endl;
    printMoves(b,capmv,num);
    return false;
  }

  return true;
}



static int goalDist(Board& b, pla_t pla, int rdepth, int cdepth, bool trust, int trustDepth, move_t* mv)
{
  if(b.isGoal(pla))
  {return cdepth;}

  if(rdepth <= 0)
  {return cdepth+1;}

  //Trust the goal tree
  if(trust && trustDepth >= 1 && rdepth <= trustDepth)
  {
    int dist = cdepth + BoardTrees::goalDist(b,pla,rdepth);
    return dist > 5 ? 5 : dist;
  }

  if(rdepth <= 4)
  {
    Bitmap rabbits = b.pieceMaps[pla][RAB];
    Bitmap all = b.pieceMaps[pla][0] | b.pieceMaps[gOpp(pla)][0];
    Bitmap allBlock;
    if(pla == 0)
    {
      allBlock = Bitmap::shiftN(all);
      allBlock = allBlock | Bitmap::shiftN(allBlock);
      allBlock = allBlock | Bitmap::shiftN(allBlock);
    }
    else
    {
      allBlock = Bitmap::shiftS(all);
      allBlock = allBlock | Bitmap::shiftS(allBlock);
      allBlock = allBlock | Bitmap::shiftS(allBlock);
    }

    Bitmap uBRabbits = rabbits & ~allBlock;
    Bitmap uFRabbits = rabbits & ~b.frozenMap;

    Bitmap uBuFRabbits = uBRabbits & uFRabbits;

    if(!(Board::PGOALMASKS[rdepth][pla] & uBuFRabbits).hasBits()
    && !(Board::PGOALMASKS[rdepth-1][pla] & (uBRabbits | uFRabbits)).hasBits()
    && (rdepth < 2 || !(Board::PGOALMASKS[rdepth-2][pla] & rabbits).hasBits()))
    {return cdepth+rdepth+1;}
  }

  int num = 0;

  num += BoardMoveGen::genSteps(b,pla,mv+num);
  if(rdepth > 1)
    num += BoardMoveGen::genPushPulls(b,pla,mv+num);

  if(num == 0)
  {return cdepth+rdepth+1;}

  int best = cdepth+rdepth+1;

  for(int j = 0; j<num; j++)
  {
    Board copy = b;
    bool success = copy.makeMoveLegalNoUndo(mv[j]);
    assert(success);

    int ns = (((copy.step - b.step) + 3) & 0x3) + 1;
    int val = goalDist(copy, pla, best-cdepth-ns-1, cdepth+ns, trust, trustDepth, mv+num);

    if(val < best)
    {
      best = val;
      if(best <= cdepth+1)
      {return best;}
    }
  }

  return best;
}

static bool canCapType(Board& b, pla_t pla, piece_t piece, int rdepth, bool trust, int trustDepth, move_t* mv)
{
  pla_t opp = gOpp(pla);
  int origCount = b.pieceMaps[opp][piece].countBits();
  int cdepth = 0;

  return canCapType(b,pla,piece,origCount,rdepth,cdepth, trust, trustDepth, mv);
}

//Piece = 0 for any type
static bool canCapType(Board& b, pla_t pla, piece_t piece, int origCount, int rdepth, int cdepth, bool trust, int trustDepth, move_t* mv)
{
  pla_t opp = gOpp(pla);

  if(b.pieceMaps[opp][piece].countBits() < origCount)
  {return true;}

  if(rdepth <= 1)
  {return false;}

  if(trust && trustDepth >= 1 && rdepth <= trustDepth)
  {
    if(piece == 0)
    {return BoardTrees::canCaps(b,pla,rdepth);}
    else
    {
      int hm[2048];
      int num = BoardTrees::genCaps(b,pla,rdepth,mv,hm);
      assert(num < 2048);
      for(int i = 0; i<num; i++)
      {
        if(hm[i] == piece)
        {return true;}
      }
      return false;
    }
  }

  if(rdepth <= 3)
  {
    //All opponent pieces
    Bitmap oppMap = b.pieceMaps[opp][0];

    //All squares S,W,E,N of an opponent piece
    Bitmap oppMapS = Bitmap::shiftS(oppMap);
    Bitmap oppMapW = Bitmap::shiftW(oppMap);
    Bitmap oppMapE = Bitmap::shiftE(oppMap);
    Bitmap oppMapN = Bitmap::shiftN(oppMap);

    //All squares guarded by an odd number of opponent pieces
    Bitmap oppOneDef = oppMapS ^ oppMapW ^ oppMapE ^ oppMapN;

    //All squares with exactly one opponent guard (if 3 either S and W, or E and N are both opps)
    oppOneDef &= ~(oppMapS & oppMapW);
    oppOneDef &= ~(oppMapE & oppMapN);

    //All traps with exactly one opponent guard
    oppOneDef &= Bitmap::BMPTRAPS;

    //All opponents of the relvant type adjacent to an unsafe trap
    oppOneDef |= Bitmap::adj(oppOneDef);
    oppOneDef &= b.pieceMaps[opp][piece];

    if(!oppOneDef.hasBits())
    {return false;}
  }

  int num = 0;

  if(rdepth >= 3)
    num += BoardMoveGen::genSteps(b,pla,mv+num);
  num += BoardMoveGen::genPushPulls(b,pla,mv+num);

  if(num == 0)
  {return false;}

  for(int j = 0; j<num; j++)
  {
    Board copy = b;
    bool success = copy.makeMoveLegalNoUndo(mv[j]);
    assert(success);

    int ns = (((copy.step - b.step) + 3) & 0x3) + 1;
    bool suc = canCapType(copy, pla, piece, origCount, rdepth-ns, cdepth+ns, trust, trustDepth, mv+num);

    if(suc)
    {return true;}
  }

  return false;
}



