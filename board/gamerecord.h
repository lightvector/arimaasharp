
/*
 * gamerecord.h
 * Author: davidwu
 */

#ifndef GAMERECORD_H
#define GAMERECORD_H

#include "../core/global.h"
#include "../board/board.h"

using namespace std;

class BoardRecord
{
  public:
  Board board;
  map<string,string> keyValues;

  BoardRecord();
  BoardRecord(const Board& b, const map<string,string>& keyValues);

  static BoardRecord read(const string& arg, bool ignoreConsistency);

  string write();
  void write(ostream& out);
  static string write(const BoardRecord& record);
  static void write(ostream& out, const BoardRecord& record);
  friend ostream& operator<<(ostream& out, const BoardRecord& record);
};

class GameRecord
{
  public:

  Board board;
  vector<move_t> moves;
  pla_t winner;
  map<string,string> keyValues;

  GameRecord();
  GameRecord(const Board& b, const vector<move_t>& moves, pla_t winner, const map<string,string>& keyValues);

  //On error, will not terminate the program, but will rather truncate the move list so that what is there is good.
  static GameRecord read(const string& arg);

  string write();
  void write(ostream& out);
  static string write(const GameRecord& record);
  static void write(ostream& out, const GameRecord& record);

  string writeKeyValues();
  void writeKeyValues(ostream& out);
  static string writeKeyValues(const GameRecord& record);
  static void writeKeyValues(ostream& out, const GameRecord& record);

  friend ostream& operator<<(ostream& out, const GameRecord& record);
};


#endif
