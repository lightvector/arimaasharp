/*
 * gameiterator.h
 * Author: davidwu
 */

#ifndef GAMEITERATOR_H_
#define GAMEITERATOR_H_

#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/gamerecord.h"
#include "../learning/feature.h"
#include "../learning/featurearimaa.h"
#include "../search/timecontrol.h"

using namespace std;

class Rand;
class FastExistsHashTable;

class GameIterator
{
  public:
  static const int STEP_MOVES = 0;
  static const int SIMPLE_CHAIN_MOVES = 1;
  static const int LOCAL_COMBO_MOVES = 2;
  static const int FULL_MOVES = 3;

  GameIterator(const char* filename);
  GameIterator(string filename);
  GameIterator(const char* filename, int idx);
  GameIterator(string filename, int idx);
  ~GameIterator();

  //Returns false as soon as there are no more positions
  bool next();

  //Reset the game iterator to the starting board
  void reset();

  //Filter likely bad moves? If false, filter is still computed but not applied.
  //Filtering that is always computed, and applied if this is true, is:
  // * Deliberate game prolonging and missing of goal in 1
  // * Opening sacs
  // * Concession sacs
  //Other filtering can be turned on by the various settable options below
  bool getDoFilter() const;

  int getNumInitialToFilter() const; //If filtering, how many moves off the front to filter?
  int getNumLoserToFilter() const; //If filtering, how many moves off the end to filter for the loser?
  bool getDoFilterWins() const; //If filtering,filter winning moves?
  bool getDoFilterWinsInTwo() const; //If filtering, try to filter moves made in the game that were wins in 2
  bool getDoFilterLemmings() const; //If filtering, try to filter lemming-like moves
  bool getDoFilterManyShortMoves() const; //If filtering, filter sequences of too many consecutive moves < 4 steps
  int getMinPlaUnfilteredMoves() const; //If filtering, filter game if #unfiltered moves by either pla is less than this.
  int getMoveType() const; //How to iterate between moves?
  double getPosKeepProp() const; //Probability to keep a position
  double getBotPosKeepProp() const; //Probability to keep a position where a bot made a move
  double getMoveKeepProp() const; //Keep this proportion of legal moves per pos other than the actual move
  int getMoveKeepBase() const; //This many legal moves other than the actual move don't count towards the threshold
  int getMoveKeepMin() const; //Always keep at least this many legal moves other than the actual move
  int getMinRating() const; //Filter moves made by players with rating less than this, unless <= 0, in which case don't filter
  bool getRatedOnly() const; //Only iterate over rated games
  double getBotGameWeight() const; //How much to multiply weight for bot games by?
  double getBotPosWeight() const; //How much to multiply weight for bot moves by?
  bool getDoFancyWeight() const; //Fancy training weighting scheme enabled?
  bool getDoPrint() const; //Should print output as we iterate?
  int getPrintEveryNumGames() const; //If printing output, the number of games between each print
  uint64_t getRandSeed() const;

  vector<bool> getCurrentFilter() const; //Get the filter for the current game. Index i is the move played on turn i
  bool wouldFilterCurrent() const; //If not filtering, would we filter the current move if we were filtering?

  void setDoFilter(bool b); //default false
  void setNumInitialToFilter(int n); //default 0
  void setNumLoserToFilter(int n); //default 0
  void setDoFilterWins(bool b); //default false
  void setDoFilterWinsInTwo(bool b); //default false
  void setDoFilterLemmings(bool b); //default false
  void setDoFilterManyShortMoves(bool b); //default false
  void setMinPlaUnfilteredMoves(int n); //default 0
  void setMoveType(int type); //default FULL_MOVES
  void setPosKeepProp(double p); //default 1.0
  void setBotPosKeepProp(double p); //default 1.0
  void setMoveKeepProp(double p); //default 1.0
  void setMoveKeepBase(int p); //default 1
  void setMoveKeepMin(int p); //default 0
  void setMinRating(int p); //default 0
  void setRatedOnly(bool b); //default false
  void setBotGameWeight(double p); //default 1.0
  void setBotPosWeight(double p); //default 1.0
  void setDoFancyWeight(bool b); //default false
  void setDoPrint(bool b); //default false
  void setPrintEveryNumGames(int n); //default 1
  void setRandSeed(uint64_t seed);

  int getTotalNumGames() const;

  const Board& getBoard() const;
  const BoardHistory& getHist() const;
  move_t getNextMove() const;
  void getNextMoves(int& winningMoveIdx, vector<move_t>& moves);
  void getNextMoveFeatures(ArimaaFeatureSet afset, int& winningTeam,
      vector<vector<findex_t> >& teams);
  void getNextMoveFeatures(ArimaaFeatureSet afset, int& winningTeam,
      vector<vector<findex_t> >& teams, vector<double>& matchParallelFactors);
  double getPosWeight() const;

  int getGameIdx() const;
  pla_t getGameWinner() const;
  int getRating(pla_t pla) const;
  bool isBot(pla_t pla) const;
  string getUsername(pla_t pla) const;
  bool isGameRated() const;
  int getMetaGameId() const;
  pla_t getMetaGameWinner() const;

  //Get some data that can be output into a moveweights file for reference
  vector<string> getTrainingPropertiesComments() const;

  //Return a vector indicating any moves that seem to be obviously bad play and should be filtered
  //hist should be the board history constructed directly from the game record
  static vector<bool> getFiltering(const GameRecord& game, const BoardHistory& hist,
      int numInitialToFilter, int numLoserToFilter, bool doFilterWins, bool doFilterWinsInTwo,
      bool doFilterLemmings, bool doFilterManyShortMoves, int minPlaUnfilteredMoves);

  private:
  string filename;
  Rand* rand;
  int initialGameIdx;

  //Parameters
  bool doFilter;
  int numInitialToFilter;
  int numLoserToFilter;
  bool doFilterWins;
  bool doFilterWinsInTwo;
  bool doFilterLemmings;
  bool doFilterManyShortMoves;
  int minPlaUnfilteredMoves;
  int moveType;
  double posKeepProp;
  double botPosKeepProp;
  double moveKeepProp;
  int moveKeepBase;
  int moveKeepMin;
  int minRating;
  bool ratedOnly;
  double botGameWeight;
  double botPosWeight;
  bool doFancyWeight;
  bool doPrint;
  int printEveryNumGames;

  //Internal movegen and buffers
  int moveBufSize;
  move_t* moveBuf;
  FastExistsHashTable* existsTable;

  //Generator state
  int genInternalState;
  vector<GameRecord> games;
  int gameIdx;
  int moveIdx;
  GameRecord* gameRecord;
  vector<bool> filtering;
  Board board;
  BoardHistory hist;
  move_t turnMove;
  move_t move;

  //Game record metadata, refreshed each successive game record
  bool metadataKnown;
  int plaRating[2];
  bool plaIsBot[2];
  string plaName[2];
  TimeControl timeControl;

  bool metadataKnown2;
  bool gameIsRated;
  int metaGameId;
  pla_t metaGameWinner;

  void init(const char* filename, bool specificIdx, int idx);
  void fillMetadataFromRecord();

};

#endif /* GAMEITERATOR_H_ */
