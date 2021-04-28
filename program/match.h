
/*
 * match.h
 * Author: davidwu
 */

#ifndef MATCH_H
#define MATCH_H

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../search/search.h"

struct GameResult
{
  Board startBoard;
  vector<move_t> moves;
  pla_t winner;
  string names[2];
  vector<SearchStats> stats;

  GameResult();

  friend ostream& operator<<(ostream& out, const GameResult& result);
};

struct MatchResult
{
  string names[2];
  int wins[3];
  vector<GameResult> gameResults;

  MatchResult();

  friend ostream& operator<<(ostream& out, const MatchResult& result);
};

namespace Match
{
  //Run a match between the given searchers
  MatchResult runMatch(int len, Searcher& searcher1, Searcher& searcher0, int setupType,
      bool printMatch, bool printGame, uint64_t seed);

  //Run a game between the given searchers
  GameResult runGame(Searcher& searcher1, Searcher& searcher0, int setupType, bool printGame, uint64_t seed);

  //Run a game between the given searchers beginning from the specifid board position
  GameResult runGame(Searcher& searcher1, Searcher& searcher0, const Board& b, bool printGame, uint64_t seed);

}


#endif
