
/*
 * mainstats.cpp
 * Author: davidwu
 */

#include <fstream>
#include <cstdlib>
#include <set>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/gamerecord.h"
#include "../learning/learner.h"
#include "../learning/gameiterator.h"
#include "../search/searchmovegen.h"
#include "../search/searchparams.h"
#include "../search/search.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

using namespace std;

int MainFuncs::avgBranching(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile";
  const char* required = "";
  const char* allowed = "filter";
  const char* empty = "filter";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];

  cout << infile << " " << Command::gitRevisionId() << endl;
  GameIterator iter(infile);
  if(Command::isSet(flags,"filter"))
  {
    iter.setDoFilter(true);
    iter.setDoFilterWins(true);
    iter.setDoFilterLemmings(true);
    iter.setDoFilterWinsInTwo(true);
    iter.setNumLoserToFilter(2);
  }

  ExistsHashTable* fullmoveHash = new ExistsHashTable(SearchParams::DEFAULT_FULLMOVE_HASH_EXP);
  int64_t numLegalMovesSum = 0;
  int64_t numPositions = 0;
  int64_t numGameMovesUnpruned = 0;

  vector<move_t> movebuf;
  set<hash_t> hashSet;
  cout << "numgames " << iter.getTotalNumGames() << endl;
  int prevGameIdx = -1;
  while(iter.next())
  {
    if(iter.getGameIdx() % 10 == 0 && prevGameIdx != iter.getGameIdx())
    {
      cout << iter.getGameIdx()
           << " positions " << numPositions
           << " branchy " << (double)numLegalMovesSum / (double)numPositions
           << " gameunpruned " << (double)numGameMovesUnpruned / (double)numPositions << endl;
    }
    prevGameIdx = iter.getGameIdx();
    Board b = iter.getBoard();
    const BoardHistory& hist = iter.getHist();

    movebuf.clear();
    SearchMoveGen::genFullMoves(b,hist,movebuf,4,false,4,fullmoveHash,NULL, NULL);
    int numLegalMoves = movebuf.size();
    numLegalMovesSum += numLegalMoves;

    UndoMoveData udata;
    hashSet.clear();
    for(int k = 0; k<numLegalMoves; k++)
    {
      bool suc = b.makeMoveLegalNoUndo(movebuf[k],udata);
      DEBUGASSERT(suc);
      hashSet.insert(b.sitCurrentHash);
      b.undoMove(udata);
    }
    numPositions++;

    move_t move = iter.getNextMove();
    bool suc = b.makeMoveLegalNoUndo(move);
    if(!suc) Global::fatalError("Illegal move in game record");

    if(hashSet.find(b.sitCurrentHash) != hashSet.end())
      numGameMovesUnpruned++;
  }
  cout << "positions " << numPositions
       << " branchy " << (double)numLegalMovesSum / (double)numPositions
       << " gameunpruned " << (double)numGameMovesUnpruned / (double)numPositions << endl;

  delete fullmoveHash;

  return EXIT_SUCCESS;
}


int MainFuncs::avgGameLength(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string movesfile = mainCommand[1];
  vector<GameRecord> games = ArimaaIO::readMovesFile(movesfile);

  long long gameLenSum = 0;
  long long numGames = 0;

  cout << games.size() << endl;
  for(int i = 0; i<(int)games.size(); i++)
  {
    if(i%20 == 0)
      cout << i << endl;
    vector<move_t>& moves = games[i].moves;

    gameLenSum += moves.size();
    numGames++;
  }
  cout << (double)gameLenSum / (double)numGames << endl;

  return EXIT_SUCCESS;
}



