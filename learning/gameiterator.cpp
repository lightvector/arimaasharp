
/*
 * gameiterator.cpp
 * Author: davidwu
 */

#include <fstream>
#include <sstream>
#include <algorithm>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../board/gamerecord.h"
#include "../board/hashtable.h"
#include "../learning/gameiterator.h"
#include "../learning/feature.h"
#include "../learning/featuremove.h"
#include "../learning/learner.h"
#include "../eval/eval.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"
#include "../program/command.h"

#include "../core/generator.h"

using namespace std;

static int genNextMoves(Board& b, const BoardHistory& hist, Rand& rand, move_t recordedMove, int move_type,
    int& nextMoveIdx, move_t* nextMoves, FastExistsHashTable* table,
    double moveKeepProp, int moveKeepBase, int moveKeepMin);
static move_t rearrangeMoveToJoinCombos(const Board& b, move_t move);
static int genFullMoves(Board& b, const BoardHistory& hist, move_t* moves, FastExistsHashTable* table);
static int genFullMoveHelper(Board& b, const BoardHistory& hist, pla_t originalPla, move_t* moves, move_t moveSoFar, int stepIndex,
    hash_t prevHashes[5], int numPrevHashes, FastExistsHashTable* table);

static void ensureMoveBuf(int moveType, move_t*& moveBuf, int& moveBufSize);

static const int MV_FULL_HASH_EXP = 23;

GameIterator::GameIterator(const char* filename)
:filename(filename)
{
  init(filename,false,0);
}
GameIterator::GameIterator(string filename)
:filename(filename)
{
  init(filename.c_str(),false,0);
}
GameIterator::GameIterator(const char* filename, int idx)
:filename(filename)
{
  init(filename,true,idx);
}
GameIterator::GameIterator(string filename, int idx)
:filename(filename)
{
  init(filename.c_str(),true,idx);
}
void GameIterator::init(const char* filename, bool specificIdx, int idx)
{
  rand = new Rand((uint64_t)0xCAFECAFE12345678ULL);

  doFilter = false;
  numInitialToFilter = 0;
  numLoserToFilter = 0;
  doFilterWins = false;
  doFilterWinsInTwo = false;
  doFilterLemmings = false;
  doFilterManyShortMoves = false;
  minPlaUnfilteredMoves = 0;
  moveType = FULL_MOVES;
  posKeepProp = 1.0;
  botPosKeepProp = 1.0;
  moveKeepProp = 1.0;
  moveKeepBase = 1;
  moveKeepMin = 0;
  minRating = 0;
  ratedOnly = false;
  botGameWeight = 1.0;
  botPosWeight = 1.0;
  doFancyWeight = false;
  doPrint = false;
  printEveryNumGames = 1;

  if(specificIdx)
  {
    GameRecord record = ArimaaIO::readMovesFile(filename,idx);
    games.clear();
    games.push_back(record);
    initialGameIdx = idx;
  }
  else
  {
    games = ArimaaIO::readMovesFile(filename);
    initialGameIdx = 0;
  }

  moveBufSize = -1;
  moveBuf = NULL;
  existsTable = new FastExistsHashTable(MV_FULL_HASH_EXP);

  gameIdx = 0;
  moveIdx = 0;
  gameRecord = NULL;
  turnMove = ERRMOVE;
  move = ERRMOVE;

  metadataKnown = false;
  plaRating[GOLD] = 0;
  plaRating[SILV] = 0;
  plaIsBot[GOLD] = false;
  plaIsBot[SILV] = false;
  metadataKnown2 = false;
  gameIsRated = false;
  metaGameId = -1;
  metaGameWinner = NPLA;

  //Convert all the moves to join the comboed steps together.
  for(int i = 0; i<(int)games.size(); i++)
  {
    Board b = games[i].board;
    for(int j = 0; j<(int)games[i].moves.size(); j++)
    {
      move_t m = rearrangeMoveToJoinCombos(b, games[i].moves[j]);
      games[i].moves[j] = m;
      bool suc = b.makeMoveLegalNoUndo(m);
      DEBUGASSERT(suc);
    }
  }
  GEN_INIT;
}

GameIterator::~GameIterator()
{
  if(moveBuf != NULL)
    delete[] moveBuf;
  delete existsTable;
  delete rand;
}

vector<string> GameIterator::getTrainingPropertiesComments() const
{
  vector<string> comments;
  comments.push_back(Command::gitRevisionId());
  comments.push_back(filename);
  comments.push_back("doFilter = " + Global::intToString((int)doFilter));
  comments.push_back("doFilterWins = " + Global::intToString((int)doFilterWins));
  comments.push_back("doFilterWinsInTwo = " + Global::intToString((int)doFilterWinsInTwo));
  comments.push_back("doFilterLemmings = " + Global::intToString((int)doFilterLemmings));
  comments.push_back("doFilterManyShortMoves = " + Global::intToString((int)doFilterManyShortMoves));
  comments.push_back("numInitialToFilter = " + Global::intToString(numInitialToFilter));
  comments.push_back("numLoserToFilter = " + Global::intToString(numLoserToFilter));
  comments.push_back("minPlaUnfilteredMoves = " + Global::intToString(minPlaUnfilteredMoves));
  comments.push_back("moveType = " + Global::intToString(moveType));
  comments.push_back("posKeepProp = " + Global::doubleToString(posKeepProp));
  comments.push_back("botPosKeepProp = " + Global::doubleToString(botPosKeepProp));
  comments.push_back("moveKeepProp = " + Global::doubleToString(moveKeepProp));
  comments.push_back("moveKeepBase = " + Global::intToString(moveKeepBase));
  comments.push_back("moveKeepMin = " + Global::intToString(moveKeepMin));
  comments.push_back("minRating = " + Global::intToString(minRating));
  comments.push_back("ratedOnly = " + Global::intToString((int)ratedOnly));
  comments.push_back("botGameWeight = " + Global::doubleToString(botGameWeight));
  comments.push_back("botPosWeight = " + Global::doubleToString(botPosWeight));
  comments.push_back("doFancyWeight = " + Global::intToString((int)doFancyWeight));
  return comments;
}


bool GameIterator::getDoFilter() const {return doFilter;}
int GameIterator::getNumInitialToFilter() const {return numInitialToFilter;}
int GameIterator::getNumLoserToFilter() const {return numLoserToFilter;}
bool GameIterator::getDoFilterWins() const {return doFilterWins;}
bool GameIterator::getDoFilterWinsInTwo() const {return doFilterWinsInTwo;}
bool GameIterator::getDoFilterLemmings() const {return doFilterLemmings;}
bool GameIterator::getDoFilterManyShortMoves() const {return doFilterManyShortMoves;}
int GameIterator::getMinPlaUnfilteredMoves() const {return minPlaUnfilteredMoves;}
int GameIterator::getMoveType() const {return moveType;}
double GameIterator::getPosKeepProp() const {return posKeepProp;}
double GameIterator::getBotPosKeepProp() const {return botPosKeepProp;}
double GameIterator::getMoveKeepProp() const {return moveKeepProp;}
int GameIterator::getMoveKeepBase() const {return moveKeepBase;}
int GameIterator::getMoveKeepMin() const {return moveKeepMin;}
int GameIterator::getMinRating() const {return minRating;}
bool GameIterator::getRatedOnly() const {return ratedOnly;}
double GameIterator::getBotGameWeight() const {return botGameWeight;}
double GameIterator::getBotPosWeight() const {return botPosWeight;}
bool GameIterator::getDoFancyWeight() const {return doFancyWeight;}
bool GameIterator::getDoPrint() const {return doPrint;}
int GameIterator::getPrintEveryNumGames() const {return printEveryNumGames;}
uint64_t GameIterator::getRandSeed() const {return rand->getSeed();}

vector<bool> GameIterator::getCurrentFilter() const {return filtering;}
bool GameIterator::wouldFilterCurrent() const
{if((int)filtering.size() > moveIdx && moveIdx >= 0) return filtering[moveIdx]; else return false;}

void GameIterator::setDoFilter(bool b) {doFilter = b;}
void GameIterator::setNumInitialToFilter(int n) {numInitialToFilter = n;}
void GameIterator::setNumLoserToFilter(int n) {numLoserToFilter = n;}
void GameIterator::setDoFilterWins(bool b) {doFilterWins = b;}
void GameIterator::setDoFilterWinsInTwo(bool b) {doFilterWinsInTwo = b;}
void GameIterator::setDoFilterLemmings(bool b) {doFilterLemmings = b;}
void GameIterator::setDoFilterManyShortMoves(bool b) {doFilterManyShortMoves = b;}
void GameIterator::setMinPlaUnfilteredMoves(int n) {minPlaUnfilteredMoves = n;}
void GameIterator::setMoveType(int type) {moveType = type;}
void GameIterator::setPosKeepProp(double p) {posKeepProp = p;}
void GameIterator::setBotPosKeepProp(double p) {botPosKeepProp = p;}
void GameIterator::setMoveKeepProp(double p) {moveKeepProp = p;}
void GameIterator::setMoveKeepBase(int p) {moveKeepBase = p;}
void GameIterator::setMoveKeepMin(int p) {moveKeepMin = p;}
void GameIterator::setMinRating(int p) {minRating = p;}
void GameIterator::setRatedOnly(bool b) {ratedOnly = b;}
void GameIterator::setBotGameWeight(double p) {botGameWeight = p;}
void GameIterator::setBotPosWeight(double p) {botPosWeight = p;}
void GameIterator::setDoFancyWeight(bool b) {doFancyWeight = b;}
void GameIterator::setDoPrint(bool b) {doPrint = b;}
void GameIterator::setPrintEveryNumGames(int n) {printEveryNumGames = n;}
void GameIterator::setRandSeed(uint64_t seed) {rand->init(seed);}

void GameIterator::fillMetadataFromRecord()
{
  DEBUGASSERT(gameRecord != NULL);
  metadataKnown =
      map_contains(gameRecord->keyValues,"G_RATING") &&
      map_contains(gameRecord->keyValues,"S_RATING") &&
      map_contains(gameRecord->keyValues,"G_IS_BOT") &&
      map_contains(gameRecord->keyValues,"S_IS_BOT") &&
      map_contains(gameRecord->keyValues,"G_USERNAME") &&
      map_contains(gameRecord->keyValues,"S_USERNAME") &&
      map_contains(gameRecord->keyValues,"TC");
  metadataKnown2 =
      map_contains(gameRecord->keyValues,"RATED") &&
      map_contains(gameRecord->keyValues,"WINNER") &&
      map_contains(gameRecord->keyValues,"GID");

  if(metadataKnown)
  {
    plaRating[GOLD] = Global::stringToInt(map_get(gameRecord->keyValues,"G_RATING"));
    plaRating[SILV] = Global::stringToInt(map_get(gameRecord->keyValues,"S_RATING"));
    plaIsBot[GOLD] = Global::stringToBool(map_get(gameRecord->keyValues,"G_IS_BOT"));
    plaIsBot[SILV] = Global::stringToBool(map_get(gameRecord->keyValues,"S_IS_BOT"));
    plaName[GOLD] = map_get(gameRecord->keyValues,"G_USERNAME");
    plaName[SILV] = map_get(gameRecord->keyValues,"S_USERNAME");

    string tcString = map_get(gameRecord->keyValues,"TC");
    bool suc = ArimaaIO::tryReadTC(tcString,0,0,0,timeControl);
    if(!suc)
    {
      cout << "Warning: could not parse tc " << tcString << "replacing with default 15s/2m30s/100/0/1h/2m30s" << endl;
      timeControl = ArimaaIO::readTC("15s/2m30s/100/0/1h/2m30s",0,0,0);
    }
  }

  if(metadataKnown2)
  {
    gameIsRated = Global::stringToBool(map_get(gameRecord->keyValues,"RATED"));
    metaGameId = Global::stringToInt(map_get(gameRecord->keyValues,"GID"));
    metaGameWinner = Board::readPla(map_get(gameRecord->keyValues,"WINNER"));
  }
}

void GameIterator::reset()
{
  board = Board();
  hist = BoardHistory();
  move = ERRMOVE;
  turnMove = ERRMOVE;
  gameRecord = NULL;
  metadataKnown = false;
  metadataKnown2 = false;
  filtering.clear();
  moveIdx = 0;
  delete rand;
  rand = new Rand((uint64_t)0xCAFECAFE12345678ULL);
  GEN_INIT;
}

bool GameIterator::next()
{
  GEN_BEGIN;
  for(gameIdx = 0; gameIdx<(int)games.size(); gameIdx++)
  {
    if(doPrint && gameIdx % printEveryNumGames == 0)
      cout << "Iterating game " << gameIdx << endl;
    gameRecord = &games[gameIdx];
    board = gameRecord->board;
    hist = BoardHistory(*gameRecord);
    filtering = getFiltering(*gameRecord,hist,
        numInitialToFilter,numLoserToFilter,doFilterWins,doFilterWinsInTwo,
        doFilterLemmings,doFilterManyShortMoves,minPlaUnfilteredMoves);
    fillMetadataFromRecord();

    for(moveIdx = 0; moveIdx<(int)gameRecord->moves.size(); moveIdx++)
    {
      turnMove = gameRecord->moves[moveIdx];
      if(doFilter && filtering[moveIdx])
      {
        bool suc = board.makeMoveLegalNoUndo(turnMove);
        DEBUGASSERT(suc);
        continue;
      }

      if(moveType == GameIterator::FULL_MOVES)
      {
        move = turnMove;
        if(rand->nextDouble() < posKeepProp &&
           (botPosKeepProp >= 1.0 || !isBot(board.player) || rand->nextDouble() < botPosKeepProp) &&
           (minRating <= 0 || getRating(board.player) >= minRating) &&
           (!ratedOnly || isGameRated()))
        {GEN_YIELD(true);}
        bool suc = board.makeMoveLegalNoUndo(turnMove);
        DEBUGASSERT(suc);
        continue;
      }

      while(true)
      {
        {
          int nextMoveIdx = -1;
          ensureMoveBuf(moveType,moveBuf,moveBufSize);
          int turn = board.turnNumber;
          hash_t hash = board.sitCurrentHash;
          genNextMoves(board,hist,*rand,turnMove,moveType,nextMoveIdx,moveBuf,
              existsTable,moveKeepProp,moveKeepBase,moveKeepMin);
          DEBUGASSERT(nextMoveIdx != -1);
          move = moveBuf[nextMoveIdx];
          DEBUGASSERT(board.sitCurrentHash == hash);
          DEBUGASSERT(board.turnNumber == turn);
          DEBUGASSERT(move != ERRMOVE);
          //Legality test
          Board copy = board;
          bool suc = copy.makeMoveLegalNoUndo(move);
          if(!suc)
            Global::fatalError(string("Illegal move: ") + Board::writeMove(move) + Board::write(board));
        }

        if(rand->nextDouble() < posKeepProp &&
            (botPosKeepProp >= 1.0 || !isBot(board.player) || rand->nextDouble() < botPosKeepProp) &&
            (minRating <= 0 || getRating(board.player) >= minRating))
        {GEN_YIELD(true);}
        bool suc = board.makeMoveLegalNoUndo(move);
        DEBUGASSERT(suc);

        if(board.step == 0)
          break;
      }
    }
  }

  //Iteration done, clear state
  board = Board();
  hist = BoardHistory();
  move = ERRMOVE;
  turnMove = ERRMOVE;
  gameRecord = NULL;
  metadataKnown = false;
  metadataKnown2 = false;
  filtering.clear();
  moveIdx = 0;

  GEN_END(false);
}

int GameIterator::getTotalNumGames() const {return (int)games.size();}

const Board& GameIterator::getBoard() const {return board;}
const BoardHistory& GameIterator::getHist() const {return hist;}
move_t GameIterator::getNextMove() const {return move;}

int GameIterator::getGameIdx() const {return gameIdx + initialGameIdx;}
pla_t GameIterator::getGameWinner() const
{
  DEBUGASSERT(gameRecord != NULL);
  return gameRecord->winner;
}
int GameIterator::getRating(pla_t pla) const
{
  DEBUGASSERT(gameRecord != NULL);
  DEBUGASSERT(pla == GOLD || pla == SILV);
  if(!metadataKnown)
    Global::fatalError("GameIterator: Could not get rating, metadata unknown");
  return plaRating[pla];
}
bool GameIterator::isBot(pla_t pla) const
{
  DEBUGASSERT(gameRecord != NULL);
  DEBUGASSERT(pla == GOLD || pla == SILV);
  if(!metadataKnown)
    Global::fatalError("GameIterator: Could not check if pla is bot, metadata unknown");
  return plaIsBot[pla];
}
string GameIterator::getUsername(pla_t pla) const
{
  DEBUGASSERT(gameRecord != NULL);
  DEBUGASSERT(pla == GOLD || pla == SILV);
  if(!metadataKnown)
    Global::fatalError("GameIterator: Could not get username, metadata unknown");
  return plaName[pla];
}
bool GameIterator::isGameRated() const
{
  DEBUGASSERT(gameRecord != NULL);
  if(!metadataKnown2)
    Global::fatalError("GameIterator: Could not get rated, metadata2 unknown");
  return gameIsRated;
}
int GameIterator::getMetaGameId() const
{
  DEBUGASSERT(gameRecord != NULL);
  if(!metadataKnown2)
    Global::fatalError("GameIterator: Could not get meta game id, metadata2 unknown");
  return metaGameId;
}
pla_t GameIterator::getMetaGameWinner() const
{
  DEBUGASSERT(gameRecord != NULL);
  if(!metadataKnown2)
    Global::fatalError("GameIterator: Could not get meta winner, metadata2 unknown");
  return metaGameWinner;
}

double GameIterator::getPosWeight() const
{
  DEBUGASSERT(gameRecord != NULL);

  double weight = 1.0;
  if(doFancyWeight)
  {
    if(!metadataKnown)
      Global::fatalError("GameIterator: Cannot compute fancy pos weight, metadata unknown");
    //Weight the winner's moves slightly more
    if(gameRecord->winner == board.player)
      weight *= 1.1;

    //If human, weight higher ratings slightly more
    if(!isBot(board.player))
    {
      int rating = getRating(board.player);
      if     (rating >= 2500) weight *= 1.30;
      else if(rating >= 2450) weight *= 1.24;
      else if(rating >= 2400) weight *= 1.19;
      else if(rating >= 2350) weight *= 1.14;
      else if(rating >= 2300) weight *= 1.10;
      else if(rating >= 2250) weight *= 1.06;
      else if(rating >= 2200) weight *= 1.03;
    }

    //De-weight blitz games slightly, en-weight postal games slightly
    if(timeControl.perMove <= 16)
      weight *= 0.9;
    else if(timeControl.perMove >= 3600)
      weight *= 1.25;

    //Decrease weight if the material balance is super-skewed
    eval_t materialEval = Eval::getMaterialScore(board,board.player);
    double x = materialEval >= 0 ? materialEval : - materialEval;
    //0.987046272469304 is normalization so that when eval is 0, weight is 1.
    //4000  0.9650754513
    //8000  0.8521696688
    //12000 0.5902156998
    //16000 0.2724709356
    //20000 0.0895598106
    weight *= 1.0 / (1.0 + exp((x-13000.0)/3000.0)) / 0.987046272469304;

    //If the player is one of few specific players who are overrepresented, deweight them a little
    if(plaName[board.player] == "Max")
      weight *= 0.5;
    else if(plaName[board.player] == "bot_Sharp2011Blitz")
      weight *= 0.6;
    else if(plaName[board.player] == "bot_marwin")
      weight *= 0.75;
    else if(plaName[board.player] == "bot_Marwin2010Blitz")
      weight *= 0.7;
    else if(plaName[board.player] == "onigawara")
      weight *= 0.75;
    else if(plaName[board.player] == "browni3141")
      weight *= 0.9;
  }

  if(botGameWeight == 1.0 && botPosWeight == 1.0)
    return weight;
  else
  {
    if(isBot(board.player))
      weight *= botPosWeight;
    if(isBot(GOLD) || isBot(SILV))
      weight *= botGameWeight;
    return weight;
  }
}

vector<bool> GameIterator::getFiltering(const GameRecord& game, const BoardHistory& hist,
    int numInitialToFilter, int numLoserToFilter, bool doFilterWins, bool doFilterWinsInTwo,
    bool doFilterLemmings, bool doFilterManyShortMoves, int minPlaUnfilteredMoves)
{
  vector<bool> filter;
  int numMoves = game.moves.size();
  filter.resize(game.moves.size());

  DEBUGASSERT(hist.minTurnNumber == 0 && hist.maxTurnNumber == numMoves);

  //Filter opening sacrifices
  for(pla_t pla = 0; pla <= 1; pla++)
  {
    for(int i = (pla == GOLD ? 0 : 1); i<numMoves; i+=2)
    {
      DEBUGASSERT(game.moves[i] == hist.turnMove[i]);
      DEBUGASSERT(hist.turnBoard[i].player == pla);

      const Board& b = hist.turnBoard[i];
      loc_t src[8];
      loc_t dest[8];
      move_t move = game.moves[i];
      int num = b.getChanges(move,src,dest);

      bool hasSac = false;
      for(int j = 0; j<num; j++)
      {
        if(dest[j] == ERRLOC && b.owners[src[j]] == pla)
        {hasSac = true; break;}
      }

      bool hasAnyPastHomeRows = false;
      for(int j = 0; j<num; j++)
      {
        if(dest[j] == ERRLOC)
          continue;
        int y = gY(dest[j]);
        if(y >= 2 && y <= 5)
        {hasAnyPastHomeRows = true; break;}
      }

      if(hasSac || !hasAnyPastHomeRows)
        filter[i] = true;
      else
      {
        for(int j = 0; j<numInitialToFilter && j+i < numMoves; j++)
          filter[j+i] = true;
        break;
      }
    }
  }

  //Filter concession sacrifices
  if(game.winner != NPLA)
  {
    for(int i = numMoves - 5; i < numMoves; i++)
    {
      const Board& b = hist.turnBoard[i];
      if(b.player == gOpp(game.winner))
      {
        loc_t src[8];
        loc_t dest[8];
        move_t move = game.moves[i];
        int num = b.getChanges(move,src,dest);

        bool hasSac = false;
        for(int j = 0; j<num; j++)
        {
          if(dest[j] == ERRLOC && b.owners[src[j]] == b.player)
          {hasSac = true; break;}
        }

        if(hasSac)
          filter[i] = true;
      }
    }
  }

  //Filter the last move by the winning player, if desired
  if(doFilterWins && game.winner != NPLA)
  {
    for(int i = numMoves - 1; i >= 0; i--)
    {
      const Board& b = hist.turnBoard[i];
      if(b.player == game.winner)
      {
        filter[i] = true;
        break;
      }
    }
  }

  //Filter the last few moves by the losing player, since they are likely to be nonsense
  if(game.winner != NPLA)
  {
    int numLoserFiltered = 0;
    for(int i = numMoves - 1; i >= 0; i--)
    {
      if(numLoserFiltered >= numLoserToFilter)
        break;
      const Board& b = hist.turnBoard[i];
      if(b.player == gOpp(game.winner))
      {
        filter[i] = true;
        numLoserFiltered++;
      }
    }
  }

  //Filter missed win in 1 and deliberate game prolonging
  int numPlaMissedWins[2] = {0,0};
  int firstMissedWinIdx[2] = {0,0};
  for(int i = 0; i < numMoves; i++)
  {
    const Board& b = hist.turnBoard[i];
    pla_t pla = b.player;
    Board copy = b;

    //Possible to win
    if(BoardTrees::goalDist(copy,pla,4) < 5 || BoardTrees::canElim(copy,pla,4))
    {
      bool suc = copy.makeMoveLegalNoUndo(game.moves[i]);
      DEBUGASSERT(suc);
      pla_t winner = copy.getWinner();

      //We failed to win, so filter it
      if(winner != pla)
      {
        numPlaMissedWins[pla]++;
        filter[i] = true;
        if(numPlaMissedWins[pla] == 1)
          firstMissedWinIdx[pla] = i;
      }

      //Regardless of whether we on or not, clearly the opponent failed to prevent a goal in 1
      //so we filter the opponent's previous moves as well since they are probably nonsense
      int numLoserFiltered = 0;
      for(int j = i - 1; j >= 0; j--)
      {
        if(numLoserFiltered >= numLoserToFilter)
          break;
        const Board& b = hist.turnBoard[j];
        if(b.player == gOpp(pla))
        {
          filter[j] = true;
          numLoserFiltered++;
        }
      }

    }
  }

  if(doFilterWinsInTwo)
  {
    for(int i = 1; i < numMoves; i++)
    {
      Board turnBoard = hist.turnBoard[i];
      pla_t pla = turnBoard.player;
      pla_t opp = gOpp(pla);

      //Current board opp is threatening to win and we aren't.
      bool oppThreateningWin =
        BoardTrees::goalDist(turnBoard,opp,4) <= 4 || BoardTrees::canElim(turnBoard,opp,4);
      if(!oppThreateningWin)
        continue;
      bool plaThreateningWin =
        BoardTrees::goalDist(turnBoard,pla,4) <= 4 || BoardTrees::canElim(turnBoard,pla,4);
      if(plaThreateningWin)
        continue;

      //Pull the previous board and make sure it's not just the opponent passing up a goal
      Board prevBoard = hist.turnBoard[i-1];
      DEBUGASSERT(prevBoard.step == 0);
      if(BoardTrees::goalDist(prevBoard,opp,4) <= 4 || BoardTrees::canElim(turnBoard,opp,4))
        continue;

      //Check on the current board that the loss is forced
      bool forcedLoss = SearchMoveGen::definitelyForcedLossInTwo(turnBoard);

      //Filter the previous board
      if(forcedLoss)
        filter[i-1] = true;
    }
  }

  //If any player missed a goal in 1 more than once, we assume that player was deliberately
  //prologing the game so we filter every non-winning move past the first miss
  for(pla_t pla = 0; pla <= 1; pla++)
  {
    if(numPlaMissedWins[pla] <= 1)
      continue;

    for(int i = firstMissedWinIdx[pla]+1; i<numMoves; i++)
    {
      //Check if someone won this move
      const Board& b = hist.turnBoard[i];
      Board copy = b;

      bool suc = copy.makeMoveLegalNoUndo(game.moves[i]);
      DEBUGASSERT(suc);
      pla_t winner = copy.getWinner();

      //Not a winning move, so filter it
      if(winner != b.player)
        filter[i] = true;
    }
  }

  //Filter the first few moves if desired
  for(int i = 0; i<numInitialToFilter && i < numMoves; i++)
    filter[i] = true;


  //Filter moves that look like lemmings, by the player who eventually lost
  if(doFilterLemmings)
  {
    for(pla_t lemmingPla = 0; lemmingPla <= 1; lemmingPla++)
    {
      vector<pair<int,int> > lemmingStartEnds;
      pla_t opp = gOpp(lemmingPla);
      //Look for consecutive captures with no countercaptures
      //and no goal threats by the lemming player
      //And where the captured pieces also were moved by the lemming player
      int consecutivePotentialLemmings = 0;
      int consecutiveStartTurn = 0;
      int numMovedCaptures = 0;
      loc_t lemmingTrap = ERRLOC;

      DEBUGASSERT(hist.maxTurnNumber >= numMoves);
      int i;
      for(i = 0; i<numMoves-1; i++)
      {
        //Check if a move was made where the opponent captured on the following turn
        if(hist.turnBoard[i].player != lemmingPla)
          continue;
        bool potentialLemming = false;
        bool potentialLemmingWeMoved = false;
        loc_t captureTrap = ERRLOC;
        if(hist.turnPieceCount[lemmingPla][i+2] < hist.turnPieceCount[lemmingPla][i+1] //Opp captured our piece next turn
           && hist.turnPieceCount[opp][i+2] == hist.turnPieceCount[opp][i]) //We captured nothing
        {
          //Our move made no goal threat
          Board bb = hist.turnBoard[i+1];
          if(BoardTrees::goalDist(bb,lemmingPla,4) >= 5)
          {
            potentialLemming = true;

            //Check if we moved any pieces that the opp captured.
            //First, get the changes from our move
            const Board& b = hist.turnBoard[i];
            loc_t src[8];
            loc_t dest[8];
            int num = b.getChanges(hist.turnMove[i],src,dest);
            //Mark all squares that we moved pieces to that were ours
            Bitmap weMoved;
            for(int k = 0; k<num; k++)
              if(dest[k] != ERRLOC && b.owners[src[k]] == lemmingPla)
                weMoved.setOn(dest[k]);

            //Now check if any piece we moved was captured
            const Board& nextB = hist.turnBoard[i+1];
            num = nextB.getChanges(hist.turnMove[i+1],src,dest);
            for(int k = 0; k<num; k++)
              if(dest[k] == ERRLOC && weMoved.isOne(src[k]))
              {potentialLemmingWeMoved = true; break;}

            //Additionally, find out which trap had a capture
            for(int k = 0; k<num; k++)
            {
              if(dest[k] != ERRLOC)
                continue;
              loc_t loc = src[k];
              if(Board::ISTRAP[loc])
              {captureTrap = loc; break;}

              //Iterate through the move and examine each step
              for(int n = 0; n<4; n++)
              {
                step_t s = getStep(hist.turnMove[i+1],n);
                if(s == ERRSTEP || s == PASSSTEP || s == QPASSSTEP)
                  break;

                loc_t srcLoc = gSrc(s);
                loc_t destLoc = gDest(s);
                if(srcLoc == loc)
                  loc = destLoc;
                if(Board::ISTRAP[loc])
                {captureTrap = loc; break;}
              }
              break;
            }
            DEBUGASSERT(captureTrap != ERRLOC);
          }
        }

        //No potential lemming detected
        if(!(potentialLemming && (lemmingTrap == ERRLOC || captureTrap == lemmingTrap)))
        {
          //If the count prior to this was high enough, record a lemming streak
          if(consecutivePotentialLemmings >= 3 && numMovedCaptures >= 2)
            lemmingStartEnds.push_back(make_pair(consecutiveStartTurn,i));

          //Reset count
          consecutivePotentialLemmings = 0;
          numMovedCaptures = 0;
          lemmingTrap = ERRLOC;
        }
        //Potential lemming detected
        else
        {
          //Count
          if(consecutivePotentialLemmings <= 0)
            consecutiveStartTurn = i;
          consecutivePotentialLemmings++;
          if(potentialLemmingWeMoved)
            numMovedCaptures++;
          lemmingTrap = captureTrap;
        }
      }

      //If the count at the end was high enough, record a lemming streak
      if(consecutivePotentialLemmings >= 3 && numMovedCaptures >= 2)
        lemmingStartEnds.push_back(make_pair(consecutiveStartTurn,i));

      //Now process all lemming streaks
      int numStreaks = lemmingStartEnds.size();
      for(int j = 0; j<numStreaks; j++)
      {
        int start = lemmingStartEnds[j].first;
        int end = lemmingStartEnds[j].second;
        for(int k = start; k < end; k += 2)
        {
          /*
          cout << hist.turnBoard[k] << endl;
          cout << Board::writeMove(hist.turnBoard[k],hist.turnMove[k]) << endl;
          Global::pauseForKey();
          cout << hist.turnBoard[k+1] << endl;
          cout << Board::writeMove(hist.turnBoard[k+1],hist.turnMove[k+1]) << endl;
          Global::pauseForKey();*/
          filter[k] = true;
        }
        //cout << "end streak" << endl;
        //Global::pauseForKey();
      }
    }

  }

  if(doFilterManyShortMoves)
  {
    DEBUGASSERT(hist.maxTurnNumber >= numMoves);
    for(pla_t pla = 0; pla <= 1; pla++)
    {
      //Filter runs of 3 or more consecutive moves that are not 4 steps.
      int consecutiveShorts = 0;
      for(int i = 0; i<numMoves; i++)
      {
        if(hist.turnBoard[i].player != pla)
          continue;
        if(numStepsInMove(game.moves[i]) >= 4)
          consecutiveShorts = 0;
        else
        {
          consecutiveShorts++;
          if(consecutiveShorts >= 3)
          {
            filter[i-4] = true;
            filter[i-2] = true;
            filter[i] = true;

            //cout << hist.turnBoard[i] << endl;
            //cout << Board::writeMove(hist.turnBoard[i],hist.turnMove[i]) << endl;
            //Global::pauseForKey();
          }
        }
      }
    }

    //Filter any 2-step or fewer moves in the first 20 moves of the game.
    for(int i = 0; i<20 && i<numMoves; i++)
    {
      if(numStepsInMove(game.moves[i]) <= 2)
        filter[i] = true;
    }
  }

  if(minPlaUnfilteredMoves > 0)
  {
    for(pla_t pla = 0; pla <= 1; pla++)
    {
      int numGoodMoves = 0;
      for(int i = 0; i<numMoves; i++)
      {
        if(hist.turnBoard[i].player != pla || filter[i])
          continue;
        numGoodMoves++;
      }
      if(numGoodMoves < minPlaUnfilteredMoves)
      {
        for(int i = 0; i<numMoves; i++)
          filter[i] = true;
        break;
      }
    }
  }

  return filter;
}

void GameIterator::getNextMoves(int& winningMoveIdx, vector<move_t>& moves)
{
  Board copy = board;

  int nextMoveIdx = -1;
  ensureMoveBuf(moveType,moveBuf,moveBufSize);
  int nextMovesLen = genNextMoves(copy,hist,*rand,move,moveType,nextMoveIdx,moveBuf,
      existsTable,moveKeepProp,moveKeepBase,moveKeepMin);

  moves.clear();
  moves.reserve(nextMovesLen);
  for(int i = 0; i<nextMovesLen; i++)
    moves.push_back(moveBuf[i]);

  DEBUGASSERT(nextMoveIdx != -1);
  winningMoveIdx = nextMoveIdx;
}

void GameIterator::getNextMoveFeatures(ArimaaFeatureSet afset, int& winningTeam,
    vector<vector<findex_t> >& teams)
{
  vector<double> matchParallelFactors;
  return getNextMoveFeatures(afset,winningTeam,teams);
}

void GameIterator::getNextMoveFeatures(ArimaaFeatureSet afset, int& winningTeam,
    vector<vector<findex_t> >& teams, vector<double>& matchParallelFactors)
{
  //Compute the data for this spot
  pla_t pla = board.player;
  Board copy = board;

  int nextMoveIdx = -1;
  ensureMoveBuf(moveType,moveBuf,moveBufSize);
  int nextMovesLen = genNextMoves(copy,hist,*rand,move,moveType,nextMoveIdx,moveBuf,
      existsTable,moveKeepProp,moveKeepBase,moveKeepMin);

  //Build all the feature teams!
  void* data = afset.getPosData(copy,hist,pla);
  teams.clear();
  UndoMoveData uData;
  for(int i = 0; i<nextMovesLen; i++)
  {
    copy.makeMove(moveBuf[i],uData);
    teams.push_back(afset.getFeatures(copy,data,pla,moveBuf[i],hist));
    copy.undoMove(uData);
  }
  afset.getParallelWeights(data,matchParallelFactors);
  afset.freePosData(data);

  DEBUGASSERT(nextMoveIdx != -1);
  winningTeam = nextMoveIdx;
}


//HELPERS----------------------------------------------------------------------------------------------------

static int genNextMoves(Board& b, const BoardHistory& hist, Rand& rand, move_t recordedMove, int move_type,
    int& nextMoveIdx, move_t* nextMoves, FastExistsHashTable* existsTable,
    double moveKeepProp, int moveKeepBase, int moveKeepMin)
{
  //Generate possible moves!
  int num = 0;
  if(move_type == GameIterator::STEP_MOVES)
  {
    if(b.step < 3)
      num += BoardMoveGen::genPushPulls(b,b.player,nextMoves+num);
    num += BoardMoveGen::genSteps(b,b.player,nextMoves+num);
    if(b.step > 0 && b.posCurrentHash != b.posStartHash)
      nextMoves[num++] = PASSMOVE;
  }
  else if(move_type == GameIterator::SIMPLE_CHAIN_MOVES)
  {
    num += BoardMoveGen::genSimpleChainMoves(b,b.player,4-b.step,nextMoves+num);
    if(b.step > 0 && b.posCurrentHash != b.posStartHash)
      nextMoves[num++] = PASSMOVE;
  }
  else if(move_type == GameIterator::LOCAL_COMBO_MOVES)
  {
    num += BoardMoveGen::genLocalComboMoves(b,b.player,4-b.step,nextMoves+num);
    if(b.step > 0 && b.posCurrentHash != b.posStartHash)
      nextMoves[num++] = PASSMOVE;
  }
  else if(move_type == GameIterator::FULL_MOVES)
  {
    DEBUGASSERT(b.step == 0);
    num += genFullMoves(b,hist,nextMoves+num,existsTable);
    if(num > 2000000)
      cout << "ArimaaFeature::genMoves .. fullMoves .. Num = " << num << endl;

    //Find a move equivalent to this one and treat it as the recorded move,
    //so that later, we choose this move as the actual next move
    move_t postMove = getPostMove(recordedMove,b.step);
    Board temp = b;
    bool suc = temp.makeMoveLegalNoUndo(postMove);
    if(!suc)
    {
      cout << "ArimaaFeature::genMoves .. fullMoves - illegal Move!" << endl;
      cout << b << endl;
      cout << Board::writeMove(b,postMove);
      DEBUGASSERT(false);
    }
    hash_t postMoveHash = temp.sitCurrentHash;
    bool found = false;
    for(int i = 0; i<num; i++)
    {
      temp = b;
      suc = temp.makeMoveLegalNoUndo(nextMoves[i]);
      if(!suc)
      {
        cout << "ArimaaFeature::genMoves .. fullMoves - illegal Move!" << endl;
        cout << b << endl;
        cout << Board::writeMove(b,nextMoves[i]);
        DEBUGASSERT(false);
      }
      hash_t thisMoveHash = temp.sitCurrentHash;
      if(thisMoveHash == postMoveHash)
      {
        recordedMove = nextMoves[i];
        found = true;
        break;
      }
    }
    if(!found)
    {
      cout << "ArimaaFeature::genMoves .. fullMoves - move not found!" << endl;
      cout << b << endl;
      cout << Board::writeMove(b,postMove) << endl;
      nextMoves[num++] = postMove;
      recordedMove = postMove;
      DEBUGASSERT(false);
    }
  }
  else
  {
    Global::fatalError("Unknown move type!");
  }

  //Locate the maximal prefix of the recorded move and use it as the next move
  int bestLen = 0;
  move_t postMove = getPostMove(recordedMove,b.step);
  move_t nextMove = ERRMOVE;
  for(int i = 0; i<num; i++)
  {
    if(isPrefix(nextMoves[i], postMove))
    {
      int len = numStepsInMove(nextMoves[i]);
      if(len > bestLen)
      {
        bestLen = len;
        nextMove = nextMoves[i];
      }
    }
  }
  DEBUGASSERT(bestLen > 0);

  //Discard moves randomly
  //The -1 is because we are always keeping the actual next move, numToKeep does not include it.
  int numOtherMoves = num-1;
  int numToKeep = (int)round(max(numOtherMoves-moveKeepBase,0) * moveKeepProp) + moveKeepBase;
  if(numToKeep < moveKeepMin)
    numToKeep = moveKeepMin;
  if(numToKeep < numOtherMoves)
  {
    int newNum = 0;
    for(int i = 0; i<num; i++)
    {
      if(nextMoves[i] == nextMove)
        nextMoves[newNum++] = nextMoves[i];
      else
      {
        numOtherMoves--;
        if(rand.nextDouble() < (double)numToKeep/numOtherMoves)
        {
          nextMoves[newNum++] = nextMoves[i];
          numToKeep--;
        }
      }
    }
    num = newNum;
  }

  //Uniquify moves, taking care to retain the one selected
  if(move_type == GameIterator::SIMPLE_CHAIN_MOVES || move_type == GameIterator::LOCAL_COMBO_MOVES)
  {
    //Uniquify moves
    int newNum = 0;
    hash_t posHashes[num];
    int numSteps[num];
    for(int i = 0; i<num; i++)
    {
      //Always keep the pass move
      if(nextMoves[i] == PASSMOVE)
      {
        posHashes[newNum] = posHashes[i];
        numSteps[newNum] = numSteps[i];
        nextMoves[newNum] = nextMoves[i];
        newNum++;
        continue;
      }

      //Try the move and see the resulting hash
      Board tempBoard = b;
      bool suc = tempBoard.makeMoveLegalNoUndo(nextMoves[i]);
      if(!suc)
      {
        cout << "ArimaaFeature::genMoves - illegal Move!" << endl;
        cout << b << endl;
        cout << Board::writeMove(b,nextMoves[i]);
      }

      //Record data about the move
      posHashes[i] = tempBoard.posCurrentHash;
      numSteps[i] = numStepsInMove(nextMoves[i]);

      //Move changes nothing! - skip it unless it is the move we are going to make
      if(tempBoard.posCurrentHash == b.posCurrentHash && nextMoves[i] != nextMove)
        continue;

      //Check to see if this move is equivalent a previous move
      bool equivalent = false;
      for(int j = 0; j<newNum; j++)
      {
        //Equivalent move!
        if(posHashes[i] == posHashes[j] && nextMoves[i] != PASSMOVE)
        {
          //Keep the one with the smaller number of steps made, except that we always keep the move that is the nextMove
          if(nextMoves[i] == nextMove || (numSteps[i] < numSteps[j] && nextMoves[j] != nextMove))
          {
            posHashes[j] = posHashes[i];
            numSteps[j] = numSteps[i];
            nextMoves[j] = nextMoves[i];
          }
          equivalent = true;
          break;
        }
      }
      //Found no equivalents? Then we add it!
      if(!equivalent)
      {
        posHashes[newNum] = posHashes[i];
        numSteps[newNum] = numSteps[i];
        nextMoves[newNum] = nextMoves[i];
        newNum++;
      }
    }
    num = newNum;
  }

  for(int i = 0; i<num; i++)
  {
    if(nextMoves[i] == nextMove)
    {
      nextMoveIdx = i;
      break;
    }
  }

  return num;
}

static move_t rearrangeMoveToJoinCombos(const Board& b, move_t move)
{
  int dependencyStrength[4][4];
  for(int i = 0; i<4; i++)
    for(int j = 0; j<4; j++)
      dependencyStrength[i][j] = 0;

  loc_t k0[4];
  loc_t k1[4];
  int num = numStepsInMove(move);
  int actualStepNum = 0;
  for(int i = 0; i<num; i++)
  {
    step_t step = getStep(move,i);
    if(step == PASSSTEP)
      break;
    k0[i] = gSrc(step);
    k1[i] = gDest(step);
    actualStepNum++;
  }

  for(int i = 0; i<actualStepNum; i++)
  {
    for(int j = 0; j<i; j++)
    {
      if(k0[i] == k1[j])
        dependencyStrength[i][j] = 1000;
      else if(k1[i] == k0[j])
        dependencyStrength[i][j] = 150;
      else
        dependencyStrength[i][j] = 30-Board::manhattanDist(k1[i],k1[j]);
      dependencyStrength[j][i] = dependencyStrength[i][j];
    }
  }

  Board actualBoard = b;
  actualBoard.makeMove(move);

  move_t bestMove = ERRMOVE;
  int bestTension = 0x7FFFFFFF;
  int permutations[4] = {0,1,2,3};
  do
  {
    move_t permutedMove = getMove(
        getStep(move,permutations[0]),
        getStep(move,permutations[1]),
        getStep(move,permutations[2]),
        getStep(move,permutations[3]));

    int tension = 0;
    for(int i = 0; i<actualStepNum; i++)
    {
      for(int j = 0; j<actualStepNum; j++)
      {
        if(i == j) continue;
        tension += dependencyStrength[permutations[i]][permutations[j]] * abs(i-j);
      }
    }

    if(tension < bestTension)
    {
      Board copy = b;
      bool suc = copy.makeMoveLegalNoUndo(permutedMove);
      if(suc)
      {
        if(copy.sitCurrentHash == actualBoard.sitCurrentHash)
        {
          bestTension = tension;
          bestMove = permutedMove;
        }
      }
    }

  } while(next_permutation(permutations,permutations+actualStepNum));

  if(bestMove == ERRMOVE)
  {
    cout << "ArimaaFeature::rearrangeMoveToJoinCombos - Successful permutation not found!" << endl;
    bestMove = move;
  }
  return bestMove;
}

//HELPERS---------------------------------------------------------------

//MINOR Maybe use SearchUtils full move generation so as to learn under the same move generation conditions
//(regarding self-reversals, transpositions, move ordering, returning to the same position as before, etc)
//as the searcher uses in real life
static int genFullMoveHelper(Board& b, const BoardHistory& hist, pla_t originalPla, move_t* moves, move_t moveSoFar, int stepIndex,
    hash_t prevHashes[5], int numPrevHashes, FastExistsHashTable* table)
{
  hash_t hash = b.sitCurrentHash;
  if(table->lookup(hash))
    return 0;
  table->record(hash);

  if(b.player != originalPla)
  {
    if(BoardHistory::isThirdRepetition(b,hist))
      return 0;
    moves[0] = moveSoFar;
    return 1;
  }

  move_t mv[256];
  pla_t pla = b.player;

  int num = 0;
  if(b.step < 3)
    num += BoardMoveGen::genPushPulls(b,pla,mv);
  int numPPs = num;
  num += BoardMoveGen::genSteps(b,pla,mv+num);
  if(b.step != 0)
    mv[num++] = PASSMOVE;

  int numTotalMoves = 0;
  UndoMoveData data;
  for(int i = 0; i<num; i++)
  {
    b.makeMove(mv[i],data);

    //Check if we returned to a previous position hash
    bool returnsToPrevPos = false;
    //numPrevHashes-1 because it's impossible to return to the immediate current position
    //in a single step or pushpull unless you pass, and passing is okay
    for(int j = 0; j<numPrevHashes-1; j++)
      if(b.posCurrentHash == prevHashes[j])
      {returnsToPrevPos = true; break;}

    //Or if we transposed to a situation hash that we already movegenned for
    if(returnsToPrevPos)
    {b.undoMove(data); continue;}

    //Otherwise record the current hash as one taken care of for movegenning
    prevHashes[numPrevHashes] = b.posCurrentHash;

    int ns = (i < numPPs) ? 2 : 1;
    numTotalMoves += genFullMoveHelper(b,hist,originalPla,moves+numTotalMoves,concatMoves(moveSoFar,mv[i],stepIndex),
        stepIndex+ns,prevHashes,numPrevHashes+1,table);
    b.undoMove(data);
  }
  return numTotalMoves;
}

static int genFullMoves(Board& b, const BoardHistory& hist, move_t* moves, FastExistsHashTable* table)
{
  DEBUGASSERT(b.step == 0);
  table->clear();
  hash_t prevHashes[5];
  prevHashes[0] = b.posCurrentHash;
  int numPrevHashes = 1;
  return genFullMoveHelper(b, hist, b.player, moves, ERRMOVE, 0, prevHashes, numPrevHashes, table);
}

static void ensureMoveBuf(int moveType, move_t*& moveBuf, int& moveBufSize)
{
  int size = 0;
  if(moveType == GameIterator::FULL_MOVES)
    size = 4000000;
  else if(moveType == GameIterator::LOCAL_COMBO_MOVES)
    size = 400000;
  else if(moveType == GameIterator::SIMPLE_CHAIN_MOVES)
    size = 4096;
  else if(moveType == GameIterator::STEP_MOVES)
    size = 256;

  if(moveBufSize >= 0 && moveBufSize < size)
  {
    moveBufSize = -1;
    delete[] moveBuf;
    moveBuf = NULL;
  }

  if(moveBuf == NULL)
  {
    moveBufSize = size;
    moveBuf = new move_t[size];
  }
}

