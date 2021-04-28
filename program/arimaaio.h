
/*
 * arimaaio.h
 * Author: davidwu
 *
 * Functions for parsing files containing position, move, or gamestate data.
 *
 * BOARD AND MOVE FORMATS------------------------------------------------------
 *
 * ---Common--------------------
 * Board files contain specific board positions.
 * Move files contain gamerecords.
 * Both formats may have multiple boards/gamerecords contained in the same file, if they are semicolon separated.
 *
 * Whenever '#' character is encountered, subsequent characters on that line are ignored, EXCEPT FOR ';'!!!
 *
 * ---Key-value pairs------------
 * Any particular board file or game record may have key-value pairs associated with it.
 * Key value pairs are of the form "x=y" or "x = y".
 * Multiple key value pairs are allowed on one line if comma separated.
 * Key value pairs are also broken by newlines.

 * ---Board---------------------
 * Valid board line formats are:
 * 'xxxxxxxx' x = valid board char excluding spaces
 * '|xxxxxxxx|' x = valid board char
 * '|x x x x x x x x|' x = valid board char
 * '|x x x x x x x x |' x = valid board char
 * '| x x x x x x x x |' x = valid board char
 *
 * A board consists of 8 contiguous board lines.
 * Header and footer rows, like "+--------+" are allowed.
 *
 * Special key-value pairs P, S, and T are recognized, for player (0-1), step (0-3), and turn number.
 * Special keywords like '42g' or '8s', or '7b', or '10w' at the start of a line will also specify a turn and side to move
 * Other special keywords:
 *   N_TO_S, S_TO_N - will control the board orientation. Default is N_TO_S.
 *   LR_MIRROR - will flip left and right for the board.
 * All other lines and characters not occuring during the board are ignored.
 *
 * ---Moves--------------------
 * A single move follows standard arimaa move notation, such as "4s Eb5w Md6e Me7n Ca3n"
 * Special keywords:
 *   pass appends a PASSSTEP
 *   takeback undoes the move made the previous (not current!) turn
 *   resigns terminates the move list
 *
 * ---Gamestate ---------------
 * Standard Arimaa gamestate notation
 *
 */

#ifndef ARIMAAIO_H
#define ARIMAAIO_H

#include "../core/global.h"
#include "../board/board.h"
#include "../board/gamerecord.h"
#include "../search/timecontrol.h"

using namespace std;

namespace ArimaaIO
{
  string stripComments(const string& str);
  map<string,string> readKeyValues(const string& contents);

  //OUTPUT--------------------------------------------------------------------
  string writeBArray(char* arr, const char* fmt);
  string writeBArray(int* arr, const char* fmt);
  string writeBArray(float* arr, const char* fmt);
  string writeBArray(double* arr, const char* fmt);

  //BASIC INPUT---------------------------------------------------------------------
  vector<Board> readBoardFile(const string& boardFile,bool ignoreConsistency = false);
  vector<Board> readBoardFile(const char* boardFile, bool ignoreConsistency = false);
  Board readBoardFile(const string& boardFile, int idx, bool ignoreConsistency = false);
  Board readBoardFile(const char* boardFile, int idx, bool ignoreConsistency = false);

  vector<BoardRecord> readBoardRecordFile(const string& boardFile, bool ignoreConsistency = false);
  vector<BoardRecord> readBoardRecordFile(const char* boardFile, bool ignoreConsistency = false);
  BoardRecord readBoardRecordFile(const string& boardFile, int idx, bool ignoreConsistency = false);
  BoardRecord readBoardRecordFile(const char* boardFile, int idx, bool ignoreConsistency = false);

  vector<pair<BoardRecord,BoardRecord> > readBadGoodPairFile(const char* pairFile);
  vector<pair<BoardRecord,BoardRecord> > readBadGoodPairFile(const string& pairFile);
  pair<BoardRecord,BoardRecord> readBadGoodPairFile(const char* pairFile, int idx);
  pair<BoardRecord,BoardRecord> readBadGoodPairFile(const string& pairFile, int idx);

  //MOVES---------------------------------------------------------------------
  //On error, will not terminate the program, but will rather truncate the move list so that what is there is good.
  vector<GameRecord> readMovesFile(const string& moveFile);
  vector<GameRecord> readMovesFile(const char* moveFile);
  GameRecord readMovesFile(const string& moveFile, int idx);
  GameRecord readMovesFile(const char* moveFile, int idx);

  //GAME STATE-----------------------------------------------------------------

  //Parses a given gamestate file into key-value pairs and unescapes the characters in the values.
  map<string,string> readGameState(istream& in);
  map<string,string> readGameStateFile(const char* fileName);

  //Retrieves the time control specified by the given gameState
  TimeControl getTCFromGameState(map<string,string> gameState);

  //Read time control from tc string like "0:30/5/100/3"
  //Extended time control string also allows the 6th, 7th, and 8th arguments to override
  //the three dynamic time values passed in, for convenience
  // permove M:S / reservestart M:S / percent / reservemax M:S / gamemax H:M / permovemax M:S / alreadyused S / reservecurrent S / gamecurrent S
  bool tryReadTC(const string& tcString, double alreadyUsed, double reserveCurrent, double gameCurrent, TimeControl& tc);
  TimeControl readTC(const string& tcString, double alreadyUsed, double reserveCurrent, double gameCurrent);

  //EXPERIMENTAL---------------------------------------------------------------

  vector<hash_t> readHashListFile(const char* file);
  vector<hash_t> readHashListFile(const string& file);

  /*
  GoalPatternInput readGoalPattern(const string& pattern);
  string writeGoalPattern(const GoalPatternInput& pattern);
  vector<GoalPatternInput> readMultiGoalPatternFile(const char* patFile);
  vector<GoalPatternInput> readMultiGoalPatternFile(const string& patFile);

  vector<PatternRecord> readPatternFile(const char* patFile);
  vector<PatternRecord> readPatternFile(const string& patFile);

  DecisionTree readDecisionTreeFile(const char* file);
  DecisionTree readDecisionTreeFile(const string& file);
  */

  //MISC----------------------------------------------------------------------
  uint64_t readMem(const char* str);
  uint64_t readMem(const string& str);

  //EVAL------------------------------------------------------------------------
  string writeEval(int eval);
}


#endif
