
/*
 * arimaaio.cpp
 * Author: davidwu
 */

#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>
#include <cctype>
#include "../core/global.h"
#include "../board/board.h"
#include "../board/gamerecord.h"
#include "../eval/eval.h"
#include "../search/searchutils.h"
#include "../search/timecontrol.h"
#include "../program/arimaaio.h"

using namespace std;

//HELPERS-----------------------------------------------------------------------
static string unescapeGameStateString(const string& str);

static bool isDigits(const string& str) {return Global::isDigits(str);}
//static bool isDigits(const string& str, int start, int end) {return Global::isDigits(str,start,end);}
static int parseDigits(const string& str) {return Global::parseDigits(str);}
//static int parseDigits(const string& str, int start, int end) {return Global::parseDigits(str,start,end);}


//FILE OPEN-----------------------------------------------------------------
static bool openFile(ifstream& in, string s)
{
  in.open(s.c_str());
  if(!in.fail())
    return true;

  in.clear();
  return false;
}

static bool openFile(ifstream& in, const char* s)
{
  return openFile(in, string(s));
}


//OUTPUT--------------------------------------------------------------------


string ArimaaIO::writeBArray(int* arr, const char* fmt)
{
  string s;
  for(int y = 7; y>=0; y--)
  {
    for(int x = 0; x<8; x++)
      s += Global::strprintf(fmt,arr[gLoc(x,y)]);
    s += Global::strprintf("\n");
  }
  return s;
}

string ArimaaIO::writeBArray(double* arr, const char* fmt)
{
  string s;
  for(int y = 7; y>=0; y--)
  {
    for(int x = 0; x<8; x++)
      s += Global::strprintf(fmt,arr[gLoc(x,y)]);
    s += Global::strprintf("\n");
  }
  return s;
}

string ArimaaIO::writeBArray(char* arr, const char* fmt)
{
  string s;
  for(int y = 7; y>=0; y--)
  {
    for(int x = 0; x<8; x++)
      s += Global::strprintf(fmt,arr[gLoc(x,y)]);
    s += Global::strprintf("\n");
  }
  return s;
}

string ArimaaIO::writeBArray(float* arr, const char* fmt)
{
  string s;
  for(int y = 7; y>=0; y--)
  {
    for(int x = 0; x<8; x++)
      s += Global::strprintf(fmt,arr[gLoc(x,y)]);
    s += Global::strprintf("\n");
  }
  return s;
}

//KEY VALUE PAIRS-------------------------------------------------------------

map<string,string> ArimaaIO::readKeyValues(const string& contents)
{
  istringstream lineIn(contents);
  string line;
  map<string,string> keyValues;
  while(getline(lineIn,line))
  {
    if(line.length() <= 0) continue;
    istringstream commaIn(line);
    string commaChunk;
    while(getline(commaIn,commaChunk,','))
    {
      if(commaChunk.length() <= 0) continue;
      size_t equalsPos = commaChunk.find_first_of('=');
      if(equalsPos == string::npos) continue;
      string leftChunk = Global::trim(commaChunk.substr(0,equalsPos));
      string rightChunk = Global::trim(commaChunk.substr(equalsPos+1));
      if(leftChunk.length() == 0)
        Global::fatalError("ArimaaIO: key value pair without key: " + line);
      if(rightChunk.length() == 0)
        Global::fatalError("ArimaaIO: key value pair without value: " + line);
      if(keyValues.find(leftChunk) != keyValues.end())
        Global::fatalError("ArimaaIO: duplicate key: " + leftChunk);
      keyValues[leftChunk] = rightChunk;
    }
  }
  return keyValues;
}

string ArimaaIO::stripComments(const string& str)
{
  if(str.find_first_of('#') == string::npos)
    return str;

  //Turn str into a stream so we can go line by line
  istringstream in(str);
  string line;
  string result;

  while(getline(in,line))
  {
    size_t pos = line.find_first_of('#');
    if(pos != string::npos)
      result += line.substr(0,pos);
    else
      result += line;
    result += "\n";
  }
  return result;
}


//BOARD----------------------------------------------------------------------

vector<BoardRecord> ArimaaIO::readBoardRecordFile(const string& boardFile, bool ignoreConsistency)
{
  return readBoardRecordFile(boardFile.c_str(),ignoreConsistency);
}

vector<BoardRecord> ArimaaIO::readBoardRecordFile(const char* boardFile, bool ignoreConsistency)
{
  ifstream in;
  if(!openFile(in,boardFile))
    Global::fatalError(string("ArimaaIO: could not open file: ") + boardFile);

  string str;
  vector<BoardRecord> boardRecords;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    boardRecords.push_back(BoardRecord::read(str, ignoreConsistency));
  }
  in.close();
  return boardRecords;
}

BoardRecord ArimaaIO::readBoardRecordFile(const string& boardFile, int idx, bool ignoreConsistency)
{
  return readBoardRecordFile(boardFile.c_str(),idx,ignoreConsistency);
}

BoardRecord ArimaaIO::readBoardRecordFile(const char* boardFile, int idx, bool ignoreConsistency)
{
  if(idx < 0)
    Global::fatalError(string("ArimaaIO: idx is negative: ") + Global::intToString(idx));

  ifstream in;
  if(!openFile(in,boardFile))
    Global::fatalError(string("ArimaaIO: could not open file: ") + boardFile);

  string str;
  BoardRecord boardRecord;
  int i = 0;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    if(i < idx)
    {i++; continue;}
    boardRecord = BoardRecord::read(str, ignoreConsistency);
    i++;
    break;
  }
  if(i <= idx)
    Global::fatalError(string("ArimaaIO: could not find idx ") + Global::intToString(idx) + " in file: " + boardFile);

  in.close();
  return boardRecord;
}

vector<Board> ArimaaIO::readBoardFile(const string& boardFile, bool ignoreConsistency)
{
  return readBoardFile(boardFile.c_str(),ignoreConsistency);
}

vector<Board> ArimaaIO::readBoardFile(const char* boardFile, bool ignoreConsistency)
{
  vector<BoardRecord> boardRecords = readBoardRecordFile(boardFile,ignoreConsistency);
  vector<Board> boards;
  boards.reserve(boardRecords.size());
  for(int i = 0; i<(int)boardRecords.size(); i++)
    boards.push_back(boardRecords[i].board);
  return boards;
}

Board ArimaaIO::readBoardFile(const string& boardFile, int idx, bool ignoreConsistency)
{
  return readBoardRecordFile(boardFile,idx,ignoreConsistency).board;
}

Board ArimaaIO::readBoardFile(const char* boardFile, int idx, bool ignoreConsistency)
{
  return readBoardRecordFile(boardFile,idx,ignoreConsistency).board;
}

static bool isToken(const string& str, int pos, int len)
{
  return (pos <= 0 || (!isalnum(str[pos-1]) && str[pos-1] != '_'))
      && (pos+len >= (int)str.length() || (!isalnum(str[pos+len]) && str[pos+len] != '_'));
}

static size_t findToken(const string& str, const string& token)
{
  size_t pos = 0;
  while(true)
  {
    pos = str.find(token,pos);
    if(pos == string::npos)
      return string::npos;
    if(isToken(str,pos,token.length()))
      return pos;
    pos = pos+token.length();
    if(pos >= str.length())
      return string::npos;
  }
  return 0;
}

static pair<BoardRecord,BoardRecord> readBadGoodPair(const string& str)
{
  size_t badpos = findToken(str,string("BAD"));
  size_t goodpos = findToken(str,string("GOOD"));

  if(badpos == string::npos || goodpos == string::npos)
    Global::fatalError(string("ArimaaIO: did not find BAD or GOOD in parsing badgoodpair"));
  if(badpos >= goodpos)
    Global::fatalError(string("ArimaaIO: found GOOD before BAD in badgoodpair"));

  string badstr = str.substr(badpos+3,goodpos);
  string goodstr = str.substr(goodpos+4);

  BoardRecord badrecord = BoardRecord::read(badstr,false);
  BoardRecord goodrecord = BoardRecord::read(goodstr,false);

  return std::make_pair(badrecord,goodrecord);
}

vector<pair<BoardRecord,BoardRecord> > ArimaaIO::readBadGoodPairFile(const char* pairFile)
{
  ifstream in;
  if(!openFile(in,pairFile))
    Global::fatalError(string("ArimaaIO: could not open file: ") + pairFile);

  string str;
  vector<pair<BoardRecord,BoardRecord> > pairs;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    pairs.push_back(readBadGoodPair(str));
  }
  in.close();
  return pairs;
}

vector<pair<BoardRecord,BoardRecord> > ArimaaIO::readBadGoodPairFile(const string& pairFile)
{
  return readBadGoodPairFile(pairFile.c_str());
}

pair<BoardRecord,BoardRecord> ArimaaIO::readBadGoodPairFile(const char* pairFile, int idx)
{
  if(idx < 0)
    Global::fatalError(string("ArimaaIO: idx is negative: ") + Global::intToString(idx));

  ifstream in;
  if(!openFile(in,pairFile))
    Global::fatalError(string("ArimaaIO: could not open file: ") + pairFile);

  string str;
  pair<BoardRecord,BoardRecord> pair;
  int i = 0;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    if(i < idx)
    {i++; continue;}
    pair = readBadGoodPair(str);
    i++;
    break;
  }
  if(i <= idx)
    Global::fatalError(string("ArimaaIO: could not find idx ") + Global::intToString(idx) + " in file: " + pairFile);

  in.close();
  return pair;
}

pair<BoardRecord,BoardRecord> ArimaaIO::readBadGoodPairFile(const string& pairFile, int idx)
{
  return readBadGoodPairFile(pairFile.c_str(),idx);
}

//MOVES---------------------------------------------------------------------

vector<GameRecord> ArimaaIO::readMovesFile(const string& moveFile)
{
  return readMovesFile(moveFile.c_str());
}

vector<GameRecord> ArimaaIO::readMovesFile(const char* moveFile)
{
  ifstream in;
  if(!openFile(in,moveFile))
    Global::fatalError(string("ArimaaIO: could not open file: ") + moveFile);

  string str;
  vector<GameRecord> records;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    records.push_back(GameRecord::read(unescapeGameStateString(str)));
  }
  in.close();
  return records;
}

GameRecord ArimaaIO::readMovesFile(const string& moveFile, int idx)
{
  return readMovesFile(moveFile.c_str(), idx);
}

GameRecord ArimaaIO::readMovesFile(const char* moveFile, int idx)
{
  if(idx < 0)
    Global::fatalError(string("ArimaaIO: idx is negative: ") + Global::intToString(idx));

  ifstream in;
  if(!openFile(in,moveFile))
    Global::fatalError(string("ArimaaIO: could not open file: ") + moveFile);

  string str;
  GameRecord record;
  int i = 0;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    if(i < idx)
    {i++; continue;}
    record = GameRecord::read(unescapeGameStateString(str));
    i++;
    break;
  }
  if(i <= idx)
    Global::fatalError(string("ArimaaIO: could not find idx ") + Global::intToString(idx) + " in file: " + moveFile);

  in.close();
  return record;
}


//GAME STATE-----------------------------------------------------------------

map<string,string> ArimaaIO::readGameState(istream& in)
{
  map<string,string> properties;
  string str;

  //Iterate line by line until we run out:
  while(getline(in,str))
  {
    //Locate the '=' that divides the key and value
    size_t eqI = str.find('=');
    if(eqI == string::npos)
    {continue;}

    //Split!
    string prop = str.substr(0,eqI);
    string value = str.substr(eqI+1);

    //Unescape, and done!
    value = unescapeGameStateString(value);
    properties[prop] = value;
  }
  return properties;
}

map<string,string> ArimaaIO::readGameStateFile(const char* fileName)
{
  ifstream in;
  if(!openFile(in,fileName))
    Global::fatalError("ArimaaIO: could not open gamestate file!");
  map<string,string> m = readGameState(in);
  in.close();
  return m;
}

TimeControl ArimaaIO::getTCFromGameState(map<string,string> gameState)
{
  TimeControl tc = TimeControl();

  //Parse all the constant properties
  if(gameState.find("tcmove") != gameState.end()     && isDigits(gameState["tcmove"]))     tc.perMove =     parseDigits(gameState["tcmove"]);
  if(gameState.find("tcpercent") != gameState.end()  && isDigits(gameState["tcpercent"]))  tc.percent =     parseDigits(gameState["tcpercent"]);
  if(gameState.find("tcmax") != gameState.end()      && isDigits(gameState["tcmax"]))      tc.reserveMax =  parseDigits(gameState["tcmax"]);
  if(gameState.find("tcgame") != gameState.end()     && isDigits(gameState["tcgame"]))     tc.gameCurrent = parseDigits(gameState["tcgame"]);
  if(gameState.find("tctotal") != gameState.end()    && isDigits(gameState["tctotal"]))    tc.gameMax =     parseDigits(gameState["tctotal"]);
  if(gameState.find("tcturntime") != gameState.end() && isDigits(gameState["tcturntime"])) tc.perMoveMax =  parseDigits(gameState["tcturntime"]);
  if(tc.gameMax == 0) tc.gameMax = 86400;
  if(tc.perMoveMax == 0) tc.perMoveMax = 86400;

  //Find who the current player to move is, and parse out the reserve for that side
  string side = gameState["s"];
  if(side == "w" || side == "g")
  {
    if(gameState.find("tcwreserve") != gameState.end() && isDigits(gameState["tcwreserve"]))
      tc.reserveCurrent = parseDigits(gameState["tcwreserve"]);
    if(gameState.find("tcgreserve") != gameState.end() && isDigits(gameState["tcgreserve"]))
      tc.reserveCurrent = parseDigits(gameState["tcgreserve"]);

    if(gameState.find("wused") != gameState.end() && isDigits(gameState["wused"]))
      tc.alreadyUsed = parseDigits(gameState["wused"]);
    if(gameState.find("gused") != gameState.end() && isDigits(gameState["gused"]))
      tc.alreadyUsed = parseDigits(gameState["gused"]);
  }
  else if(side == "b" || side == "s")
  {
    if(gameState.find("tcbreserve") != gameState.end() && isDigits(gameState["tcbreserve"]))
      tc.reserveCurrent = parseDigits(gameState["tcbreserve"]);
    if(gameState.find("tcsreserve") != gameState.end() && isDigits(gameState["tcsreserve"]))
      tc.reserveCurrent = parseDigits(gameState["tcsreserve"]);

    if(gameState.find("bused") != gameState.end() && isDigits(gameState["bused"]))
      tc.alreadyUsed = parseDigits(gameState["bused"]);
    if(gameState.find("sused") != gameState.end() && isDigits(gameState["sused"]))
      tc.alreadyUsed = parseDigits(gameState["sused"]);
  }
  else
  {
    Global::fatalError(string("ArimaaIO: unknown side:") + side);
  }

  tc.validate();
  return tc;
}

static bool tryParseTCTime(const string& str, double colonFactor, double& seconds)
{
  if(str.size() <= 0)
  {seconds = 0; return true;}

  size_t colonPos = str.find(':');
  if(colonPos != string::npos)
  {
    vector<string> pieces = Global::split(str,':');
    if(pieces.size() != 2)
      return false;
    int t0;
    double t1;
    bool suc0 = Global::tryStringToInt(pieces[0],t0);
    bool suc1 = Global::tryStringToDouble(pieces[1],t1);
    if(!suc0 || !suc1)
      return false;
    seconds = colonFactor * (t0 * 60.0 + t1);
    return true;
  }

  size_t dPos = str.find('d');
  size_t hPos = str.find('h');
  size_t mPos = str.find('m');
  size_t sPos = str.find('s');
  if(dPos != string::npos ||
     hPos != string::npos ||
     mPos != string::npos ||
     sPos != string::npos)
  {
    int n = 0;
    double x = 0;
    double time = 0;
    size_t start = 0;
    if(dPos != string::npos)
    {
      if(dPos <= start) return false;
      bool suc = Global::tryStringToInt(str.substr(start,dPos-start),n);
      if(!suc) return false;
      time += 86400.0 * n;
      start = dPos+1;
      if(start >= str.size())
      {seconds = time; return true;}
    }
    if(hPos != string::npos)
    {
      if(hPos <= start) return false;
      bool suc = Global::tryStringToInt(str.substr(start,hPos-start),n);
      if(!suc) return false;
      time += 3600.0 * n;
      start = hPos+1;
      if(start >= str.size())
      {seconds = time; return true;}
    }
    if(mPos != string::npos)
    {
      if(mPos <= start) return false;
      bool suc = Global::tryStringToInt(str.substr(start,mPos-start),n);
      if(!suc) return false;
      time += 60.0 * n;
      start = mPos+1;
      if(start >= str.size())
      {seconds = time; return true;}
    }
    if(sPos != string::npos)
    {
      if(sPos <= start) return false;
      bool suc = Global::tryStringToDouble(str.substr(start,sPos-start),x);
      if(!suc) return false;
      time += x;
      start = sPos+1;
      if(start >= str.size())
      {seconds = time; return true;}
    }

    //Hack to handle things like "2m30"
    if(mPos != string::npos && sPos == string::npos && Global::tryStringToInt(str.substr(start),n))
    {
      time += (double)n;
      {seconds = time; return true;}
    }

    return false;
  }

  int t = 0;
  bool suc = Global::tryStringToInt(str,t);
  if(!suc) return false;
  seconds = colonFactor * t * 60.0;
  return true;
}

static bool tryParseTC(const string& tcString, double alreadyUsed, double reserveCurrent, double gameCurrent, TimeControl& tc)
{
  tc = TimeControl();
  vector<string> pieces = Global::split(tcString,'/');
  double d = 0;
  if(pieces.size() > 0)
  {
    bool suc = tryParseTCTime(pieces[0],1,d);
    if(!suc) return false;
    tc.perMove = d;
  }
  if(pieces.size() > 1)
  {
    bool suc = tryParseTCTime(pieces[1],1,d);
    if(!suc) return false;
    tc.reserveStart = d;
  }
  if(pieces.size() > 2)
  {
    bool suc = Global::tryStringToDouble(pieces[2],d);
    if(!suc) return false;
    tc.percent = d;
  }
  if(pieces.size() > 3)
  {
    bool suc = tryParseTCTime(pieces[3],1,d);
    if(!suc) return false;
    if(d > 0) tc.reserveMax = d;
  }
  if(pieces.size() > 4)
  {
    //Number of turns limit, ignore it and do nothing.
    if(pieces[4].size() > 0 && pieces[4][pieces[4].size()-1] == 't')
    {}
    else
    {
      bool suc = tryParseTCTime(pieces[4],60,d);
      if(!suc) return false;
      if(d > 0) tc.gameMax = d;
    }
  }
  if(pieces.size() > 5)
  {
    bool suc = tryParseTCTime(pieces[5],1,d);
    if(!suc) return false;
    if(d > 0) tc.perMoveMax = d;
  }
  if(pieces.size() > 6)
  {
    bool suc = Global::tryStringToDouble(pieces[6],d);
    if(!suc) return false;
    tc.alreadyUsed = d;
  }
  else
    tc.alreadyUsed = alreadyUsed;

  if(pieces.size() > 7)
  {
    bool suc = tryParseTCTime(pieces[7],1,d);
    if(!suc) return false;
    tc.reserveCurrent = d;
  }
  else
    tc.reserveCurrent = reserveCurrent;

  if(pieces.size() > 8)
  {
    bool suc = tryParseTCTime(pieces[8],1,d);
    if(!suc) return false;
    tc.gameCurrent = d;
  }
  else
    tc.gameCurrent = gameCurrent;

  return true;
}

TimeControl ArimaaIO::readTC(const string& tcString, double alreadyUsed, double reserveCurrent, double gameCurrent)
{
  TimeControl tc;
  bool suc = tryParseTC(tcString,alreadyUsed,reserveCurrent,gameCurrent,tc);
  if(!suc)
    Global::fatalError("Could not parse timecontrol: " + tcString);
  tc.validate();
  return tc;
}

bool ArimaaIO::tryReadTC(const string& tcString, double alreadyUsed, double reserveCurrent, double gameCurrent, TimeControl& tc)
{
  TimeControl tc2;
  bool suc = tryParseTC(tcString,alreadyUsed,reserveCurrent,gameCurrent,tc2);
  if(!suc)
    return false;
  if(!tc2.isValid())
    return false;
  tc = tc2;
  return true;
}

//EXPERIMENTAL--------------------------------------------------------------------

vector<hash_t> ArimaaIO::readHashListFile(const char* file)
{
  ifstream in;
  if(!openFile(in,file))
    Global::fatalError("ArimaaIO: could not open hashlist file!");

  vector<hash_t> hashes;
  string str;
  while(getline(in,str))
  {
    size_t commentPos = str.find_first_of('#');
    if(commentPos != string::npos)
      str = str.substr(0,commentPos);
    if(str.length() == 0)
      continue;
    hashes.push_back(Global::stringToUInt64(str));
  }
  return hashes;
}

vector<hash_t> ArimaaIO::readHashListFile(const string& file)
{
  return readHashListFile(file.c_str());
}

//OTHER--------------------------------------------------------------

uint64_t ArimaaIO::readMem(const string& str)
{
  if(str.size() < 2)
    Global::fatalError("ArimaaIO: Could not parse amount of memory: " + str);

  size_t end = str.size()-1;
  size_t snd = str.size()-2;

  string numericPart;
  int shiftFactor;
  if     (str.find_first_of("K") == end)  {shiftFactor = 10; numericPart = str.substr(0,end); }
  else if(str.find_first_of("KB") == snd) {shiftFactor = 10; numericPart = str.substr(0,snd); }
  else if(str.find_first_of("M") == end)  {shiftFactor = 20; numericPart = str.substr(0,end); }
  else if(str.find_first_of("MB") == snd) {shiftFactor = 20; numericPart = str.substr(0,snd); }
  else if(str.find_first_of("G") == end)  {shiftFactor = 30; numericPart = str.substr(0,end); }
  else if(str.find_first_of("GB") == snd) {shiftFactor = 30; numericPart = str.substr(0,snd); }
  else if(str.find_first_of("T") == end)  {shiftFactor = 40; numericPart = str.substr(0,end); }
  else if(str.find_first_of("TB") == snd) {shiftFactor = 40; numericPart = str.substr(0,snd); }
  else if(str.find_first_of("P") == end)  {shiftFactor = 50; numericPart = str.substr(0,end); }
  else if(str.find_first_of("PB") == snd) {shiftFactor = 50; numericPart = str.substr(0,snd); }
  else if(str.find_first_of("B") == end)  {shiftFactor = 0;  numericPart = str.substr(0,end); }
  else                                    {shiftFactor = 0;  numericPart = str; }

  if(!isDigits(numericPart))
    Global::fatalError("ArimaaIO: Could not parse amount of memory: " + str);
  uint64_t mem = 0;
  istringstream in(numericPart);
  in >> mem;
  if(in.bad())
    Global::fatalError("ArimaaIO: Could not parse amount of memory: " + str);

  for(int i = 0; i<shiftFactor; i++)
  {
    uint64_t newMem = mem << 1;
    if(newMem < mem)
      Global::fatalError("ArimaaIO: Could not parse amount of memory (too large): " + str);
    mem = newMem;
  }
  return mem;
}

uint64_t ArimaaIO::readMem(const char* str)
{
  return readMem(string(str));
}

string ArimaaIO::writeEval(int eval)
{
  if(!SearchUtils::isTerminalEval(eval) || SearchUtils::isIllegalityTerminalEval(eval))
    return Global::intToString(eval);
  if(eval < 0)
    return "Loss" + Global::intToString((eval - Eval::LOSE + 7) / 8);
  return "Win" + Global::intToString((Eval::WIN - eval + 11) / 8);
}


//HELPERS-------------------------------------------------------------------------

static string unescapeGameStateString(const string& str)
{
  //Walk along the string, unescaping characters
  string result = str;
  for(int i = 0; i<(int)result.size()-1; i++)
  {
    if     (result[i] == '\\' && result[i+1] == 'n') result.replace(i,2,"\n");
    else if(result[i] == '\\' && result[i+1] == 't') result.replace(i,2,"\t");
    else if(result[i] == '\\' && result[i+1] == '\\') result.replace(i,2,"\\");
    else if(result[i] == '%' && result[i+1] == '1' && i < (int)result.size()-2 && result[i+2] == '3' ) result.replace(i,3,"\n");
  }
  return result;
}


//EXPERIMENTAL---------------------------------------------------------------
/*
GoalPatternInput ArimaaIO::readGoalPattern(const string& str)
{
  istringstream in(str);
  string wrd;
  GoalPatternInput pattern;
  for(int i = 0; i<64; i++)
  {
    in >> wrd;
    if(wrd.length() != 1)
      Global::fatalError("ArimaaIO: invalid pattern!\n" + str);
    int y = 7 - i/8;
    int x = i % 8;
    pattern.pattern[y*8+x] = wrd[0];
  }
  return pattern;
}

vector<GoalPatternInput> ArimaaIO::readMultiGoalPatternFile(const string& patFile)
{
  return readMultiGoalPatternFile(patFile.c_str());
}

vector<GoalPatternInput> ArimaaIO::readMultiGoalPatternFile(const char* patFile)
{
  ifstream in;
  if(!openFile(in,patFile))
    Global::fatalError("ArimaaIO: could not open pattern file!");

  string str;
  vector<GoalPatternInput> patterns;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    if(str.length() == 0)
      continue;

    patterns.push_back(readGoalPattern(str));
  }
  in.close();
  return patterns;
}

string ArimaaIO::writeGoalPattern(const GoalPatternInput& pattern)
{
  string str;
  for(int y = 7; y>=0; y--)
  {
    for(int x = 0; x<8; x++)
      str += pattern.pattern[y*8+x];
    str += "\n";
  }
  return str;
}


vector<PatternRecord> ArimaaIO::readPatternFile(const char* patFile)
{
  ifstream in;
  if(!openFile(in,patFile))
    Global::fatalError("ArimaaIO: could not open pattern file!");

  string str;
  vector<PatternRecord> patterns;
  while(getline(in,str,';'))
  {
    str = stripComments(str);
    if(Global::isWhitespace(str))
      continue;
    if(str.length() == 0)
      continue;

    patterns.push_back(PatternRecord::read(str));
  }
  in.close();
  return patterns;
}

vector<PatternRecord> ArimaaIO::readPatternFile(const string& patFile)
{
  return readPatternFile(patFile.c_str());
}

DecisionTree ArimaaIO::readDecisionTreeFile(const char* file)
{
  ifstream in;
  if(!openFile(in,file))
    Global::fatalError("ArimaaIO: could not open decision tree file!");

  string str;
  if(!getline(in,str,'\0'))
    Global::fatalError("ArimaaIO: error reading decision tree file!");

  str = stripComments(str);
  if(Global::isWhitespace(str))
    Global::fatalError("ArimaaIO: empty decision tree file!");

  return DecisionTree::read(str);
}

DecisionTree ArimaaIO::readDecisionTreeFile(const string& file)
{
  return readDecisionTreeFile(file.c_str());
}
*/
