/*
 * mainprunepattern.cpp
 * Author: davidwu
 */

#include <set>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../search/searchprune.h"
#include "../learning/gameiterator.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

static void testBestOnly(GameIterator& iter, bool printPrunedBest, set<hash_t>& discountedHashes, bool haizhi);
static void testFull(GameIterator& iter, bool printPrunedBest, set<hash_t>& discountedHashes, bool printRandomUnpruned, bool haizhi);

static const double printRandomUnprunedProp = 0.1;

int MainFuncs::testPrunePatterns(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile <-bestOnly> <-printPrunedBest> <-discountHashes FILE> <-printRandomUnpruned> <-haizhi>";
  const char* required = "";
  const char* allowed = "bestOnly printPrunedBest discountHashes printRandomUnpruned haizhi";
  const char* empty = "bestOnly printPrunedBest printRandomUnpruned haizhi";
  const char* nonempty = "discountHashes";

  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  /*
  Board b = ArimaaIO::readBoardFile("data/pos.txt",0);
  vector<SearchPrune::ID> ids;
  SearchPrune::findStartMatches(b,ids);
  for(int i = 0; i<ids.size(); i++)
    cout << ids[i] << endl;
  bool shouldPrune = SearchPrune::matchesMove(b,b.player,Board::readMove("Rc2n Mg3w Hd5e Db3s Rc3x"),ids);
  cout << shouldPrune << endl;
  return 0;
  */

  bool bestOnly = Command::isSet(flags,"bestOnly");
  bool printPrunedBest = Command::isSet(flags,"printPrunedBest");
  bool printRandomUnpruned = Command::isSet(flags,"printRandomUnpruned");
  bool haizhi = Command::isSet(flags,"haizhi");

  cout << Command::gitRevisionId() << endl;
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;


  GameIterator iter(mainCommand[1]);
  iter.setDoFilter(true);
  iter.setDoFilterWins(true);
  iter.setDoFilterLemmings(true);
  iter.setNumLoserToFilter(2);

  cout << "Loaded " << iter.getTotalNumGames() << " games" << endl;

  set<hash_t> discountedHashes;
  if(Command::isSet(flags,"discountHashes"))
  {
    vector<hash_t> hashes = ArimaaIO::readHashListFile(flags["discountHashes"]);
    size_t numHashes = hashes.size();
    for(size_t i = 0; i<numHashes; i++)
      discountedHashes.insert(hashes[i]);
  }

  if(bestOnly)
  {
    if(printRandomUnpruned)
      Global::fatalError("Cannot printRandomUnpruned while bestOnly");
    testBestOnly(iter,printPrunedBest,discountedHashes,haizhi);
  }
  else
  {
    iter.setDoPrint(true);
    testFull(iter,printPrunedBest,discountedHashes,printRandomUnpruned,haizhi);
  }
  return EXIT_SUCCESS;
}


static void testBestOnly(GameIterator& iter, bool printPrunedBest,
    set<hash_t>& discountedHashes, bool haizhi)
{
  vector<SearchPrune::ID> ids;
  int64_t total = 0;
  int64_t matched = 0;
  int64_t matchedPos = 0;
  int64_t discounted = 0;

  while(iter.next())
  {
    Board b = iter.getBoard();
    Board orig = b;
    move_t move = iter.getNextMove();
    pla_t pla = b.player;

    if(discountedHashes.find(b.sitCurrentHash) != discountedHashes.end())
    {discounted++; continue;}

    total++;

    SearchPrune::findStartMatches(b,ids);
    if(ids.size() > 0)
    {
      matchedPos++;
      UndoMoveData udata;
      b.makeMoveLegalNoUndo(move,udata);
      bool shouldPrune =
          SearchPrune::matchesMove(b,pla,move,ids) ||
          (haizhi && SearchPrune::canHaizhiPrune(orig,pla,move));
      b.undoMove(udata);

      if(shouldPrune)
      {
        matched++;
        if(printPrunedBest)
        {
          cout << Board::writeMove(b,move) << endl;
          cout << b << endl;
        }
      }
    }
  }
  printf("Prunes %lld/%lld (%.4f %%) of bestmoves\n",
      matched,total,(double)matched/total*100);
  printf("Prunes in %lld/%lld (%.4f %%) of poses\n",
      matchedPos,total,(double)matchedPos/total*100);
  printf("PosGainFactor: %.1f\n",
      (double)matchedPos / matched);
  cout << "Discounted " << discounted << " positions" << endl;
}


static void testFull(GameIterator& iter, bool printPrunedBest,
    set<hash_t>& discountedHashes, bool printRandomUnpruned, bool haizhi)
{
  vector<SearchPrune::ID> ids;
  int64_t totalPoses = 0;
  int64_t matchedPoses = 0;
  int64_t totalMoves = 0;
  int64_t matchedMoves = 0;
  int64_t totalBest = 0;
  int64_t matchedBest = 0;
  int64_t discounted = 0;

  Rand rand;
  vector<move_t> moves;
  while(iter.next())
  {
    Board b = iter.getBoard();
    Board orig = b;
    pla_t pla = b.player;

    if(discountedHashes.find(b.sitCurrentHash) != discountedHashes.end())
    {discounted++; continue;}

    int winningMoveIdx;
    iter.getNextMoves(winningMoveIdx,moves);

    int numMoves = (int)moves.size();
    totalMoves += numMoves;
    totalBest++;
    totalPoses++;

    SearchPrune::findStartMatches(b,ids);
    if(ids.size() > 0)
    {
      matchedPoses++;
      UndoMoveData udata;
      for(int m = 0; m<numMoves; m++)
      {
        move_t move = moves[m];
        b.makeMoveLegalNoUndo(move,udata);
        bool shouldPrune =
            SearchPrune::matchesMove(b,pla,move,ids) ||
            (haizhi && SearchPrune::canHaizhiPrune(orig,pla,move));
        b.undoMove(udata);
        if(shouldPrune)
        {
          matchedMoves++;
          if(m == winningMoveIdx)
          {
            matchedBest++;
            if(printPrunedBest)
            {
              cout << "Pruned bestmove: " << Board::writeMove(b,move) << endl;
              cout << b << endl;
            }
          }
        }
      }
    }

    if(numMoves > 0 && printRandomUnpruned && rand.nextDouble() < printRandomUnprunedProp)
    {
      vector<move_t> unpruned;
      UndoMoveData udata;
      for(int m = 0; m<numMoves; m++)
      {
        move_t move = moves[m];
        b.makeMoveLegalNoUndo(move,udata);
        bool shouldPrune =
            SearchPrune::matchesMove(b,pla,move,ids) ||
            (haizhi && SearchPrune::canHaizhiPrune(orig,pla,move));
        b.undoMove(udata);
        if(!shouldPrune)
          unpruned.push_back(move);
      }
      if(unpruned.size() > 0)
      {
        cout << "Random unpruned move: " << Board::writeMove(b,unpruned[rand.nextUInt(unpruned.size())]) << endl;
        cout << b << endl;
      }
    }
  }

  printf("Prunes %lld/%lld (%.4f %%) of bestmoves\n",
      matchedBest,totalBest,(double)matchedBest/totalBest*100);
  printf("Prunes in %lld/%lld (%.4f %%) of poses\n",
      matchedPoses,totalPoses,(double)matchedPoses/totalPoses*100);
  printf("PosGainFactor: %.1f\n",
      (double)matchedPoses / matchedBest);
  printf("Prunes %lld/%lld (%.4f %%) of moves\n",
      matchedMoves,totalMoves,(double)matchedMoves/totalMoves*100);
  printf("MoveGainFactor: %.2f\n",
      ((double)matchedMoves/totalMoves) / ((double)matchedBest/totalBest));
  cout << "Discounted " << discounted << " positions" << endl;
}







