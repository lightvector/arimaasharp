
/*
 * boardhistory.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardhistory.h"

//TODO rewrite it might be possible to remove or seriously reduce the use of BoardHistory
using namespace std;

BoardHistory::BoardHistory()
{
  minTurnNumber = 0;
  maxTurnNumber = -1;
  maxTurnBoardNumber = -1;
}

BoardHistory::BoardHistory(const Board& b)
{
  reset(b);
}

BoardHistory::~BoardHistory()
{

}

BoardHistory::BoardHistory(const Board& b, const vector<move_t>& moves)
{
  initMoves(b,moves);
}

BoardHistory::BoardHistory(const GameRecord& record)
{
  initMoves(record.board, record.moves);
}

void BoardHistory::initMoves(const Board& b, const vector<move_t>& moves)
{
  reset(b);
  int numMoves = moves.size();
  Board copy = b;
  for(int i = 0; i<numMoves; i++)
  {
    int oldStep = copy.step;
    Board prev = copy;
    bool success = copy.makeMoveLegalNoUndo(moves[i]);
    if(!success)
    {
      Global::fatalError(string("BoardHistory: Illegal move ") +
          Board::writeMove(prev,moves[i]) + "\n" + Board::write(prev));
    }
    reportMove(copy,moves[i],oldStep);
  }
}


void BoardHistory::resizeIfTooSmall()
{
  if((int)turnPosHash.size() < maxTurnNumber+1)
  {
    turnPosHash.resize(maxTurnNumber+1,0);
    turnSitHash.resize(maxTurnNumber+1,0);
    turnMove.resize(maxTurnNumber+1,ERRMOVE);
    turnPieceCount[0].resize(maxTurnNumber+1,0);
    turnPieceCount[1].resize(maxTurnNumber+1,0);
  }
}

void BoardHistory::resizeTurnBoardIfTooSmall()
{
  if((int)turnBoard.size() < maxTurnBoardNumber+1)
    turnBoard.resize(maxTurnBoardNumber+1,Board());
}

void BoardHistory::reset(const Board& b)
{
  minTurnNumber = b.turnNumber;
  maxTurnNumber = minTurnNumber;
  maxTurnBoardNumber = minTurnNumber;

  resizeIfTooSmall();
  resizeTurnBoardIfTooSmall();

  for(int i = 0; i<minTurnNumber; i++)
  {
    turnPosHash[i] = 0;
    turnSitHash[i] = 0;
    turnMove[i] = ERRMOVE;
    turnPieceCount[0][i] = 0;
    turnPieceCount[1][i] = 0;
  }

  turnBoard[minTurnNumber] = b;
  turnPosHash[minTurnNumber] = b.posCurrentHash;
  turnSitHash[minTurnNumber] = b.sitCurrentHash;
  turnMove[minTurnNumber] = ERRMOVE;
  turnPieceCount[0][minTurnNumber] = b.pieceCounts[0][0];
  turnPieceCount[1][minTurnNumber] = b.pieceCounts[1][0];
}

//Indicate that move m was made, resulting in board b, and the step number was lastStep prior to m
//Invalidates all history that occurs in any turns occuring after the turnNumber of b, and appends the results of m to the current
//turnNumber.
//At all times, this will result in the history matching the board up to the new position after the move.
void BoardHistory::reportMoveNoTurnBoard(const Board& b, move_t m, int lastStep)
{
  DEBUGASSERT(m != ERRMOVE);
  DEBUGASSERT(minTurnNumber <= b.turnNumber);
  DEBUGASSERT(maxTurnNumber + 1 >= b.turnNumber);
  int turnNumber = b.turnNumber;
  int oldTurnNumber = b.step == 0 ? turnNumber-1 : turnNumber;
  maxTurnNumber = turnNumber;
  if(maxTurnBoardNumber < turnNumber)
    maxTurnBoardNumber = turnNumber;

  resizeIfTooSmall();

  if(b.step == 0)
  {
    turnPosHash[turnNumber] = b.posCurrentHash;
    turnSitHash[turnNumber] = b.sitCurrentHash;
    turnMove[turnNumber] = ERRMOVE;
    turnPieceCount[0][turnNumber] = b.pieceCounts[0][0];
    turnPieceCount[1][turnNumber] = b.pieceCounts[1][0];
  }

  turnMove[oldTurnNumber] = concatMoves(turnMove[oldTurnNumber],m,lastStep);
}

void BoardHistory::reportMove(const Board& b, move_t m, int lastStep)
{
  reportMoveNoTurnBoard(b,m,lastStep);
  DEBUGASSERT(maxTurnBoardNumber + 1 >= b.turnNumber);
  maxTurnBoardNumber = b.turnNumber;

  resizeTurnBoardIfTooSmall();

  if(b.step == 0)
    turnBoard[b.turnNumber] = b;
}

bool BoardHistory::isThirdRepetition(const Board& b, const BoardHistory& hist)
{
  if(b.step != 0)
    return false;

  hash_t currentHash = b.sitCurrentHash;
  int count = 0;
  int currentTurn = b.turnNumber;

  DEBUGASSERT(currentTurn >= hist.minTurnNumber && currentTurn <= hist.maxTurnNumber+1);
  DEBUGASSERT(currentTurn == hist.minTurnNumber || hist.turnPosHash[currentTurn-1] == b.posStartHash);

  //Compute the start of the boards we care about. In the corner case where the initial board
  //did not begin on step 0, we add one to skip it.
  int start = hist.minTurnNumber + (hist.turnBoard[hist.minTurnNumber].step > 0 ? 1 : 0);

  //TODO compute piece count as an optimization?

  //Walk backwards, checking for a previous occurrences
  for(int i = currentTurn-2; i >= start; i -= 2)
  {
    //Don't check through a move made by null move pruning
    if(hist.turnMove[i+1] == QPASSMOVE || hist.turnMove[i] == QPASSMOVE)
      return false;

    if(hist.turnSitHash[i] == currentHash)
    {
      count++;
      if(count >= 2)
        return true;
    }
  }
  return false;
}

bool BoardHistory::occurredEarlierAndMoveIsLegalNow(const Board& b, const BoardHistory& hist, move_t& bestMove)
{
  if(b.step != 0)
    return false;

  hash_t currentHash = b.sitCurrentHash;
  int currentTurn = b.turnNumber;

  DEBUGASSERT(currentTurn >= hist.minTurnNumber && currentTurn <= hist.maxTurnNumber);
  DEBUGASSERT(currentTurn == hist.minTurnNumber || hist.turnPosHash[currentTurn-1] == b.posStartHash);
  DEBUGASSERT(hist.turnSitHash[currentTurn] == b.sitCurrentHash);
  DEBUGASSERT(currentTurn <= hist.maxTurnBoardNumber);

  //Compute the start of the boards we care about. In the corner case where the initial board
  //did not begin on step 0, we add one to skip it.
  int start = hist.minTurnNumber + (hist.turnBoard[hist.minTurnNumber].step > 0 ? 1 : 0);

  //Walk backwards, checking for a previous occurrences
  for(int i = currentTurn-2; i >= start; i -= 2)
  {
    if(hist.turnSitHash[i] == currentHash)
    {
      //Just in case, check that the owners and pieces exactly match
      //so that we're correct even on an unlikely hash collision
      const Board& histBoard = hist.turnBoard[i];
      if(Board::pieceMapsAreIdentical(b,histBoard))
      {
        //We're in the same position as before. Verify that the move is legal.
        move_t move = hist.turnMove[i];
        Board other(b);
        bool suc = other.makeMoveLegalNoUndo(move);
        if(!suc || isThirdRepetition(other,hist))
          return false;

        //Legal!
        bestMove = move;
        return true;
      }
    }
  }
  return false;
}

bool BoardHistory::occurredEarlier(const Board& b, const BoardHistory& hist, const Board& bAfter)
{
  if(b.step != 0)
    return false;

  hash_t currentHash = b.sitCurrentHash;
  int currentTurn = b.turnNumber;

  DEBUGASSERT(bAfter.step == 0 && bAfter.turnNumber == b.turnNumber + 1);
  DEBUGASSERT(currentTurn >= hist.minTurnNumber && currentTurn <= hist.maxTurnNumber+1);
  DEBUGASSERT(currentTurn == hist.minTurnNumber || hist.turnPosHash[currentTurn-1] == b.posStartHash);

  //Compute the start of the boards we care about. In the corner case where the initial board
  //did not begin on step 0, we add one to skip it.
  int start = hist.minTurnNumber + (hist.turnBoard[hist.minTurnNumber].step > 0 ? 1 : 0);

  //Walk backwards, checking for a previous occurrences
  for(int i = currentTurn-2; i >= start; i -= 2)
  {
    if(hist.turnSitHash[i] == currentHash)
      return hist.turnSitHash[i+1] == bAfter.sitCurrentHash;
  }
  return false;
}


ostream& operator<<(ostream& out, const BoardHistory& hist)
{
  cout << "BoardHistory:" << endl;
  if(hist.minTurnNumber <= hist.maxTurnNumber)
  {
    vector<move_t> moves;
    for(int i = hist.minTurnNumber; i<= hist.maxTurnNumber; i++)
      moves.push_back(hist.turnMove[i]);
    out << Board::writeGame(hist.turnBoard[hist.minTurnNumber],moves);
  }
  return out;
}






