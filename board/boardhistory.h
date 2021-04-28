
/*
 * boardhistory.h
 * Author: davidwu
 *
 * Tracks the history of a board over the course of a game.
 */

#ifndef BOARDHISTORY_H
#define BOARDHISTORY_H

#include "../core/global.h"
#include "../board/board.h"
#include "../board/gamerecord.h"

using namespace std;

class BoardHistory
{
  public:
  //Note that if the BoardHistory was initialized on step != 0, then we could have:
  //minTurnNumber*4 not a valid step number.
  //turnBoard[minTurnNumber] has step != 0
  //etc.

  int minTurnNumber;             //Starting board's turn num.
  int maxTurnNumber;             //Current board's turn num

  int maxTurnBoardNumber;        //Does turnBoard actually have any info? It does up to here. This is only set to be less than maxTurnNumber in search
  vector<Board> turnBoard;       //The board at the start of the turn [min,max]

  vector<hash_t> turnPosHash;    //The position hash at the start of the turn [min,max]
  vector<hash_t> turnSitHash;    //The situaiton hash at the start of the turn [min,max]
  vector<move_t> turnMove;       //The full move made (possibly so far) this turn [min,max]
  vector<int> turnPieceCount[2]; //The number of pieces of each player at the start of this turn [min,max]

  BoardHistory();
  BoardHistory(const Board& b);
  BoardHistory(const Board& b, const vector<move_t>& moves); //Creates a BoardHistory with all the moves played out and recorded
  BoardHistory(const GameRecord& record); //Creates a BoardHistory with all the moves played out and recorded
  ~BoardHistory();

  //Indicate that move m was made, resulting in board b, and the step number was lastStep prior to m
  //Invalidates all history that occurs in any turns occuring after the turnNumber of b, and appends the results of m to the current
  //turnNumber.
  //At all times, this will result in the history matching the board up to the new position after the move.
  void reportMove(const Board& b, move_t m, int lastStep);
  void reportMoveNoTurnBoard(const Board& b, move_t m, int lastStep);

  //Resets to the given board as the starting position, with the minimum turn and step numbers being given by the
  //board's turnNumber field
  void reset(const Board& b);

  //Returns true if the current situation is the third occurrence in the history
  //Always false when steps have been made this turn.
  //Requires that all but potentially the most recent move have been reported for b, possibly without a turnboard.
  static bool isThirdRepetition(const Board& b, const BoardHistory& hist);

  //Returns true if this current board has occured before earlier in the history
  //and the move made then is legal now, including not causing a third-repetition,
  //And if so, fills bestMove with the move that was made from that board position.
  //Requires that all moves have been reported for b with a turnBoard.
  static bool occurredEarlierAndMoveIsLegalNow(const Board& b, const BoardHistory& hist, move_t& bestMove);

  //Check if the situation for b occured earlier in the history and the move made earlier resulted in bAfter.
  //Requires that all but potentially the most recent move have been reported for b, possibly without a turnboard.
  static bool occurredEarlier(const Board& b, const BoardHistory& hist, const Board& bAfter);

  friend ostream& operator<<(ostream& out, const BoardHistory& hist);

  private:
  void initMoves(const Board& b, const vector<move_t>& moves);
  void resizeIfTooSmall();
  void resizeTurnBoardIfTooSmall();
};



#endif
