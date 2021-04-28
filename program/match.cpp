
/*
 * match.cpp
 * Author: davidwu
 */

#include <sstream>
#include <cmath>
#include "../core/global.h"
#include "../core/hash.h"
#include "../core/rand.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../setups/setup.h"
#include "../search/search.h"
#include "../program/arimaaio.h"
#include "../program/match.h"

using namespace std;

static const int MAXPLAYOUTLEN = 600;

GameResult::GameResult()
{
  winner = NPLA;
  names[GOLD] = string("");
  names[SILV] = string("");
}

ostream& operator<<(ostream& out, const GameResult& result)
{
  out << "# Gold  : " << result.names[GOLD] << endl;
  out << "# Silver: " << result.names[SILV] << endl;
  if(result.winner == GOLD)
    out << "# Winner: Gold" << endl;
  else if(result.winner == SILV)
    out << "# Winner: Silver" << endl;
  else
    out << "# Winner: Draw" << endl;

  out << Board::writeGame(result.startBoard,result.moves) << endl;
  return out;
}

MatchResult::MatchResult()
{
  names[GOLD] = string("");
  names[SILV] = string("");
  wins[GOLD] = 0;
  wins[SILV] = 0;
  wins[NPLA] = 0;
}

ostream& operator<<(ostream& out, const MatchResult& result)
{
  out << "######################################################" << endl;
  out << "# Match " << endl;
  out << "# Gold  : " << result.names[GOLD] << endl;
  out << "# Silver: " << result.names[SILV] << endl;
  out << "# Score (Gold-Draws-Silver): " << result.wins[GOLD] << "-" << result.wins[NPLA] << "-" << result.wins[SILV] << endl;
  out << "######################################################" << endl;

  for(int i = 0; i<(int)result.gameResults.size(); i++)
  {
    out << result.gameResults[i] << ";" << endl;
  }
  return out;
}


MatchResult Match::runMatch(int len, Searcher& searcher1, Searcher& searcher0, int setupType,
    bool printMatch, bool printGame, uint64_t seed)
{
  if(printMatch)
    cout << "Match seed " << seed << endl;

  Rand rand(seed);

  MatchResult mresult;
  mresult.names[GOLD] = searcher1.params.name;
  mresult.names[SILV] = searcher0.params.name;

  for(int i = 0; i<len; i++)
  {
    GameResult gresult;
    pla_t winner;
    uint64_t gameSeed = rand.nextUInt64();
    if(i % 2 == 0)
    {
      gresult = runGame(searcher1,searcher0,setupType,printGame,gameSeed);
      winner = gresult.winner;
    }
    else
    {
      gresult = runGame(searcher0,searcher1,setupType,printGame,gameSeed);
      winner = gresult.winner;
      if(winner != NPLA)
        winner = gOpp(winner);
    }

    mresult.wins[winner]++;
    mresult.gameResults.push_back(gresult);

    if(printMatch)
    {
      if(i%10 == 0)
        cout << "Gold " << mresult.names[GOLD] << " vs Silver " << mresult.names[SILV] << endl;
      cout << "Game " << i << " done (seed " << gameSeed << "), Score (Gold-Draws-Silver): " << mresult.wins[GOLD] << "-" << mresult.wins[NPLA] << "-" << mresult.wins[SILV] << endl;
    }
  }

  return mresult;
}


GameResult Match::runGame(Searcher& searcher1, Searcher& searcher0, int setupType, bool printGame, uint64_t seed)
{
  if(printGame)
    cout << "Game seed " << seed << endl;

  Board b;
  Setup::setup(b,setupType,seed + Hash::simpleHash("First setup"));
  Setup::setup(b,setupType,seed + Hash::simpleHash("Second setup"));

  if(printGame)
    cout << Board::writeStartingPlacementsAndTurns(b);

  return runGame(searcher1, searcher0, b, printGame, seed);
}

GameResult Match::runGame(Searcher& searcher1, Searcher& searcher0, const Board& origBoard, bool printGame, uint64_t seed)
{
  Rand rand(seed);

  Board b = origBoard;
  BoardHistory hist(b);

  GameResult result;
  result.startBoard = b;
  result.names[GOLD] = searcher1.params.name;
  result.names[SILV] = searcher0.params.name;

  while(true)
  {
    pla_t pla = b.player;
    Searcher& searcher = (pla == SILV) ? searcher0 : searcher1;

    DEBUGASSERT(b.step == 0);

    //Seed the searcher
    uint64_t moveSeed = rand.nextUInt64();
    searcher.params.setRandomSeed(moveSeed);
    if(printGame)
      cout << "moveSeed: " << moveSeed << endl;

    //Get the move
    searcher.searchID(b,hist);
    move_t move = searcher.getMove();
    move = completeTurn(move);

    //Validate it
    Board copy = b;
    if(move == ERRMOVE || move == PASSMOVE || !copy.makeMoveLegalNoUndo(move))
    {
      cout << b << endl;
      Global::fatalError(string("Illegal move during match: ")+ Board::writeMove(move));
    }

    //Output
    if(printGame)
    {
      cout << Board::writePlaTurn(b.player,b.turnNumber) << " ";
      cout << Board::writeMove(b,move,false) << endl;
    }

    //Make the move
    hash_t oldHash = b.posCurrentHash;
    b.makeMove(move);
    hist.reportMove(b,move,0);

    DEBUGASSERT(b.step == 0);
    hash_t newHash = b.posCurrentHash;

    //Ensure the position changed
    if(newHash == oldHash)
      Global::fatalError(string("Searcher made move that didn't change board during match: ") + Board::writeMove(move));

    //Record the move and stats
    result.moves.push_back(move);
    result.stats.push_back(searcher.stats);

    //Check for third time repetitions
    if(BoardHistory::isThirdRepetition(b,hist))
    {
      cout << "Third time reptition during match!" << endl;
      result.winner = pla;
      break;
    }

    //Check for game end conditions
    pla_t winner = b.getWinner();
    if(winner != NPLA)
    {
      result.winner = winner;
      break;
    }

    //Gone on way too long
    if(b.turnNumber > MAXPLAYOUTLEN)
    {
      result.winner = NPLA;
      break;
    }
  }
  if(printGame)
  {
    cout << hist;
  }

  return result;
}












