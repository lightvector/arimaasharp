/*
 * readwrite.cpp
 * Author: davidwu
 */

#include <sstream>
#include <cstring>
#include "../core/global.h"
#include "../board/btypes.h"
#include "../board/board.h"
#include "../board/gamerecord.h"
#include "../program/arimaaio.h"

static const char PIECECHARS[3][7] =
{
    {'<', 'r', 'c', 'd', 'h', 'm', 'e'},
    {'>', 'R', 'C', 'D', 'H', 'M', 'E'},
    {'.', '@', '#', '$', '%', '^', '&'}
};

static const char PLACHARS[3] = {'s','g','n'};
static const char DIRCHARS[4] = {'s','w','e','n'};

static const char* LOCSTRINGS[BSIZE] =
{
"a1","b1","c1","d1","e1","f1","g1","h1","i1","j1","k1","l1","m1","n1","o1","p1",
"a2","b2","c2","d2","e2","f2","g2","h2","i2","j2","k2","l2","m2","n2","o2","p2",
"a3","b3","c3","d3","e3","f3","g3","h3","i3","j3","k3","l3","m3","n3","o3","p3",
"a4","b4","c4","d4","e4","f4","g4","h4","i4","j4","k4","l4","m4","n4","o4","p4",
"a5","b5","c5","d5","e5","f5","g5","h5","i5","j5","k5","l5","m5","n5","o5","p5",
"a6","b6","c6","d6","e6","f6","g6","h6","i6","j6","k6","l6","m6","n6","o6","p6",
"a7","b7","c7","d7","e7","f7","g7","h7","i7","j7","k7","l7","m7","n7","o7","p7",
"a8","b8","c8","d8","e8","f8","g8","h8","i8","j8","k8","l8","m8","n8","o8","p8",
};

static const char* VALIDBOARDCHARS = "rcdhmeRCDHME .xX*";
static const char* VALIDPIECECHARS = "rcdhmeRCDHME";

static const char* PIECE_WORDS[NUMTYPES] = {
"emp","rab","cat","dog","hor","cam","ele"
};

//OUTPUT--------------------------------------------------------------------------------

char Board::writePla(pla_t pla)
{
  if(pla < 0 || pla > 2)
    return '?';
  return PLACHARS[pla];
}
void Board::writePla(ostream& out, pla_t pla)
{out << writePla(pla);}

char Board::writePiece(pla_t pla, piece_t piece)
{
  if(pla < 0 || pla > 2 || piece < 0 || piece >= NUMTYPES)
    return '?';
  return PIECECHARS[pla][piece];
}
void Board::writePiece(ostream& out, pla_t pla, piece_t piece)
{out << writePiece(pla,piece);}

void Board::writePieceWord(ostream& out, piece_t piece)
{
  if(piece < 0 || piece >= NUMTYPES)
    out << '?';
  else
    out << PIECE_WORDS[piece];
}
string Board::writePieceWord(piece_t piece)
{
  if(piece < 0 || piece >= NUMTYPES)
    return string("?");
  return string(PIECE_WORDS[piece]);
}

void Board::writeLoc(ostream& out, loc_t k)
{
  if(k >= BSIZE || k < 0)
    out << "errloc";
  else
    out << LOCSTRINGS[k];
}
string Board::writeLoc(loc_t k)
{
  if(k >= BSIZE || k < 0)
    return string("errloc");
  return string(LOCSTRINGS[k]);
}

string Board::writePlacement(loc_t k, pla_t pla, piece_t piece)
{
  if(k >= BSIZE || k < 0)
    return writePiece(pla,piece) + string("errloc");
  return writePiece(pla,piece) + string(LOCSTRINGS[k]);
}
void Board::writePlacement(ostream& out, loc_t k, pla_t pla, piece_t piece)
{
  writePiece(out,pla,piece);
  if(k >= BSIZE || k < 0)
    out << "errloc";
  else
    out << LOCSTRINGS[k];
}

string Board::writePlaTurn(int turn)
{
  pla_t pla = (turn % 2 == 0) ? GOLD : SILV;
  return writePlaTurn(pla,turn);
}
void Board::writePlaTurn(ostream& out, int turn)
{
  pla_t pla = (turn % 2 == 0) ? GOLD : SILV;
  return writePlaTurn(out,pla,turn);
}

string Board::writePlaTurn(pla_t pla, int turn)
{
  if((pla == GOLD) != (turn % 2 == 0))
    turn++;

  turn /= 2;
  turn += 2;

  string plaString;
  if(pla < 0 || pla > 2) plaString = "errorpla";
  else if(pla == GOLD)   plaString = "g";
  else if (pla == SILV)  plaString = "s";
  else                   plaString = "n";

  return Global::intToString(turn) + plaString;
}
void Board::writePlaTurn(ostream& out, pla_t pla, int turn)
{
  if((pla == GOLD) != (turn % 2 == 0))
    turn++;

  turn /= 2;
  turn += 2;

  out << turn;
  if(pla < 0 || pla > 2) out << "errorpla";
  else if(pla == GOLD)   out << "g";
  else if (pla == SILV)  out << "s";
  else                   out << "n";
}

string Board::writeMaterial(const int8_t pieceCounts[2][NUMTYPES])
{
  ostringstream out(ostringstream::out);
  writeMaterial(out,pieceCounts);
  return out.str();
}
void Board::writeMaterial(ostream& out, const int8_t pieceCounts[2][NUMTYPES])
{
  for(piece_t piece = ELE; piece >= RAB; piece--)
  {
    for(int i = 0; i<pieceCounts[1][piece]; i++)
      out << PIECECHARS[1][piece];
  }
  out << " ";
  for(piece_t piece = ELE; piece >= RAB; piece--)
  {
    for(int i = 0; i<pieceCounts[0][piece]; i++)
      out << PIECECHARS[0][piece];
  }
}

string Board::writePlacements(const Board& b, pla_t pla)
{
  ostringstream out(ostringstream::out);
  writePlacements(out,b,pla);
  return out.str();
}
void Board::writePlacements(ostream& out, const Board& b, pla_t pla)
{
  bool first = true;
  for(loc_t loc = 0; loc < BSIZE; loc++)
  {
    if(loc & 0x88)
      continue;
    if(b.owners[loc] == pla)
    {
      if(!first)
        out << " ";
      first = false;
      writePlacement(out,loc,b.owners[loc],b.pieces[loc]);
    }
  }
}

string Board::writeStartingPlacementsAndTurns(const Board& b)
{
  ostringstream out(ostringstream::out);
  writeStartingPlacementsAndTurns(out,b);
  return out.str();
}
void Board::writeStartingPlacementsAndTurns(ostream& out, const Board& b)
{
  if(b.pieceCounts[GOLD][0] <= 0)
    return;
  out << "1g ";
  writePlacements(out,b,GOLD);
  out << "\n";
  if(b.pieceCounts[SILV][0] <= 0)
    return;
  out << "1s ";
  writePlacements(out,b,SILV);
  out << "\n";
}

ostream& operator<<(ostream& out, const Board& b)
{
  Board::write(out,b);
  return out;
}

string Board::write() const
{
  ostringstream out(ostringstream::out);
  write(out,*this);
  return out.str();
}
void Board::write(ostream& out) const
{
  write(out,*this);
}

string Board::write(const Board& b)
{
  ostringstream out(ostringstream::out);
  write(out,b);
  return out.str();
}
void Board::write(ostream& out, const Board& b)
{
  out << "P = " << (int)b.player
      << ", S = " << (int)b.step
      << ", T = " << writePlaTurn(b.turnNumber)
      << ", H = " << b.sitCurrentHash << "\n";
  out << " +--------+" << "\n";
  for(int y = 7; y>=0; y--)
  {
    out << (y+1) << "|";
    for(int x = 0; x<8; x++)
    {
      loc_t k = gLoc(x,y);
      if(b.owners[k] == NPLA && Board::ISTRAP[k])
        out << "*";
      else
        out << PIECECHARS[b.owners[k]][b.pieces[k]];
    }
    out << "|" << "\n";
  }
  out << " +--------+" << "\n";

  out << "  ";
  for(char c = 'a'; c < 'i'; c++)
    out << c;
  out << " ";
  out << endl;
}

string Board::writeStep(step_t s0, bool showPass)
{
  ostringstream out(ostringstream::out);
  writeStep(out,s0,showPass);
  return out.str();
}

void Board::writeStep(ostream& out, step_t s0, bool showPass)
{
  if(s0 == PASSSTEP)
  {if(showPass) out << "pass";}
  else if(s0 == ERRSTEP)
  {if(showPass) out << "errorstep";}
  else if(s0 == QPASSSTEP)
  {if(showPass) out << "qpss";}
  else
  {
    //The main step
    loc_t k0 = gSrc(s0);

    if(k0 == ERRLOC)
      out << "errstep";
    else
    {
      int dir = gDir(s0);
      out << LOCSTRINGS[k0] << DIRCHARS[dir];
    }
  }
}

string Board::writeStep(const Board& b, step_t s0, bool showPass)
{
  ostringstream out(ostringstream::out);
  writeStep(out,b,s0,showPass);
  return out.str();
}

void Board::writeStep(ostream& out, const Board& b, step_t s0, bool showPass)
{
  if(s0 == PASSSTEP)
  {if(showPass) out << "pass";}
  else if(s0 == ERRSTEP)
  {if(showPass) out << "errstep";}
  else if(s0 == QPASSSTEP)
  {if(showPass) out << "qpss";}
  else
  {
    //The main step
    loc_t k0 = gSrc(s0);
    loc_t k1 = gDest(s0);

    if(k0 == ERRLOC)
      out << "errstep";
    else
    {
      int dir = gDir(s0);
      out << PIECECHARS[b.owners[k0]][b.pieces[k0]] << LOCSTRINGS[k0] << DIRCHARS[dir];

      //Captures
      loc_t kt = Board::ADJACENTTRAP[k0];
      if(kt != ERRLOC)
      {
        if(kt == k1 && !b.isGuarded2(b.owners[k0],kt))
          out << " " << PIECECHARS[b.owners[k0]][b.pieces[k0]] << LOCSTRINGS[kt] << "x";
        else if(b.owners[kt] == b.owners[k0] && !b.isGuarded2(b.owners[k0],kt))
          out << " " << PIECECHARS[b.owners[kt]][b.pieces[kt]] << LOCSTRINGS[kt] << "x";
      }
    }
  }
}

string Board::writeMove(move_t move, bool showPass)
{
  ostringstream out(ostringstream::out);
  writeMove(out,move,showPass);
  return out.str();
}
void Board::writeMove(ostream& out, move_t move, bool showPass)
{
  for(int i = 0; i<4; i++)
  {
    step_t s = getStep(move,i);
    if(s == ERRSTEP) break;
    if(i > 0) out << " ";
    writeStep(out,s,showPass);
    if(s == PASSSTEP || s == QPASSSTEP) break;
  }
}

string Board::writeMove(const Board& b, move_t move, bool showPass)
{
  ostringstream out(ostringstream::out);
  writeMove(out,b,move,showPass);
  return out.str();
}
void Board::writeMove(ostream& out, const Board& b, move_t move, bool showPass)
{
  Board copy = b;
  for(int i = 0; i<4; i++)
  {
    step_t s = getStep(move,i);
    if(s == ERRSTEP) break;
    if(i > 0) out << " ";
    writeStep(out,copy,s,showPass);
    copy.makeStepLegal(s);
    if(s == PASSSTEP || s == QPASSSTEP) break;
  }
}

string Board::writeMoves(const vector<move_t>& moves)
{
  ostringstream out(ostringstream::out);
  writeMoves(out,moves);
  return out.str();
}
void Board::writeMoves(ostream& out, const vector<move_t>& moves)
{
  bool first = true;
  for(int i = 0; i<(int)moves.size(); i++)
  {
    if(!first)
      out << " ";
    first = false;
    writeMove(out,moves[i]);
  }
}

string Board::writeMoves(const Board& b, const vector<move_t>& moves)
{
  ostringstream out(ostringstream::out);
  writeMoves(out,b,moves);
  return out.str();
}
void Board::writeMoves(ostream& out, const Board& b, const vector<move_t>& moves)
{
  bool first = true;
  Board copy = b;
  for(int i = 0; i<(int)moves.size(); i++)
  {
    if(!first && copy.step == 0)
      out << "  ";
    else if(!first)
      out << " ";
    first = false;
    writeMove(out, copy, moves[i]);

    //Use makeMoveLegal to guard vs corrupt moves but ignore failures
    copy.makeMoveLegalNoUndo(moves[i]);
  }
}

string Board::writeMoves(const move_t* moves, int numMoves)
{
  ostringstream out(ostringstream::out);
  writeMoves(out,moves,numMoves);
  return out.str();
}
void Board::writeMoves(ostream& out, const move_t* moves, int numMoves)
{
  bool first = true;
  for(int i = 0; i<numMoves; i++)
  {
    if(!first)
      out << " ";
    first = false;
    writeMove(out, moves[i]);
  }
}

string Board::writeMoves(const Board& b, const move_t* moves, int numMoves)
{
  ostringstream out(ostringstream::out);
  writeMoves(out,b,moves,numMoves);
  return out.str();
}
void Board::writeMoves(ostream& out, const Board& b, const move_t* moves, int numMoves)
{
  bool first = true;
  Board copy = b;
  for(int i = 0; i<numMoves; i++)
  {
    if(!first && copy.step == 0)
      out << "  ";
    else if(!first)
      out << " ";
    first = false;
    writeMove(out, copy, moves[i]);

    //Use makeMoveLegal to guard vs corrupt moves but ignore failures
    copy.makeMoveLegalNoUndo(moves[i]);
  }
}


string Board::writeGame(const Board& b, const vector<move_t>& moves)
{
  ostringstream out(ostringstream::out);
  writeGame(out,b,moves);
  return out.str();
}
void Board::writeGame(ostream& out, const Board& b, const vector<move_t>& moves)
{
  writeStartingPlacementsAndTurns(out,b);

  Board copy = b;
  for(int i = 0; i<(int)moves.size(); i++)
  {
    writePlaTurn(out,copy.player,copy.turnNumber);
    out << " ";

    bool showPass = (moves[i] == PASSMOVE);
    writeMove(out,copy,moves[i],showPass);

    out << "\n";

    //Use makeMoveLegal to guard vs corrupt moves but ignore failures
    copy.makeMoveLegalNoUndo(moves[i]);
  }
}

static const int NUM_BOARDKEYS = 4;
static const char* BOARDKEYS[NUM_BOARDKEYS] = {
    "P",
    "S",
    "T",
    "H",
};

ostream& operator<<(ostream& out, const BoardRecord& record)
{
  BoardRecord::write(out,record);
  return out;
}
string BoardRecord::write()
{
  return write(*this);
}
void BoardRecord::write(ostream& out)
{
  return write(out,*this);
}
string BoardRecord::write(const BoardRecord& record)
{
  ostringstream out(ostringstream::out);
  write(out,record);
  return out.str();
}
void BoardRecord::write(ostream& out, const BoardRecord& record)
{
  for(map<string,string>::const_iterator iter = record.keyValues.begin(); iter != record.keyValues.end(); ++iter)
  {
    bool isBoardKey = false;
    for(int i = 0; i<NUM_BOARDKEYS; i++)
      if(iter->first == BOARDKEYS[i])
        {isBoardKey = true; break;}
    if(isBoardKey)
      continue;

    out << iter->first << " = " << iter->second << "\n";
  }
  out << record.board << "\n";
}

ostream& operator<<(ostream& out, const GameRecord& record)
{
  GameRecord::write(out,record);
  return out;
}
string GameRecord::writeKeyValues()
{
  return writeKeyValues(*this);
}
string GameRecord::write()
{
  return write(*this);
}
void GameRecord::writeKeyValues(ostream& out)
{
  return writeKeyValues(out,*this);
}
void GameRecord::write(ostream& out)
{
  return write(out,*this);
}
string GameRecord::writeKeyValues(const GameRecord& record)
{
  ostringstream out(ostringstream::out);
  writeKeyValues(out,record);
  return out.str();
}
string GameRecord::write(const GameRecord& record)
{
  ostringstream out(ostringstream::out);
  write(out,record);
  return out.str();
}
void GameRecord::writeKeyValues(ostream& out, const GameRecord& record)
{
  for(map<string,string>::const_iterator iter = record.keyValues.begin(); iter != record.keyValues.end(); ++iter)
    out << iter->first << " = " << iter->second << "\n";
}
void GameRecord::write(ostream& out, const GameRecord& record)
{
  for(map<string,string>::const_iterator iter = record.keyValues.begin(); iter != record.keyValues.end(); ++iter)
    out << iter->first << " = " << iter->second << "\n";
  Board::writeMoves(out,record.board,record.moves);
  out << "\n";
}


//INPUT----------------------------------------------------------------------------

pla_t Board::readPla(char c)
{
  switch(c)
  {
    case 's': case 'b': case '0': case 'S': case 'B': return SILV;
    case 'g': case 'w': case '1': case 'G': case 'W': return GOLD;
    case 'n': case 'N': case '2': return NPLA;
    default: Global::fatalError(string("Board::readPla: Could not parse: ") + c); return NPLA;
  }
}

bool Board::tryReadPla(char c, pla_t& pla)
{
  switch(c)
  {
    case 's': case 'b': case '0': case 'S': case 'B': pla = SILV; return true;
    case 'g': case 'w': case '1': case 'G': case 'W': pla = GOLD; return true;
    case 'n': case 'N': case '2': pla = NPLA; return true;
    default: return false;
  }
  return false;
}

bool Board::tryReadPla(const string& s, pla_t& pla)
{
  int len = s.length();
  if(len == 1)
    return tryReadPla(s[0],pla);

  if(len != 4 && len != 6)
    return false;

  string t = Global::toLower(s);
  if(t == "gold")
  {pla = GOLD; return true;}
  if(t == "silv" || t == "silver")
  {pla = SILV; return true;}
  if(t == "npla")
  {pla = NPLA; return true;}

  return false;
}
bool Board::tryReadPla(const char* s, pla_t& pla)
{
  return tryReadPla(string(s),pla);
}

pla_t Board::readPla(const string& s)
{
  pla_t pla;
  if(tryReadPla(s,pla))
    return pla;
  Global::fatalError(string("Board::readPla: Could not parse: ") + s);
  return NPLA;
}

pla_t Board::readPla(const char* s)
{
  pla_t pla;
  if(tryReadPla(s,pla))
    return pla;
  Global::fatalError(string("Board::readPla: Could not parse: ") + s);
  return NPLA;
}

pla_t Board::readOwner(char c)
{
  switch(c)
  {
    case 'r': case 'c': case 'd': case 'h': case 'm': case 'e': return SILV;
    case 'R': case 'C': case 'D': case 'H': case 'M': case 'E': return GOLD;
    case ' ': case '.': case 'x': case 'X': case '*': return NPLA;
    default: Global::fatalError(string("Board::readOwner: Could not parse: ") + c); return NPLA;
  }
}

piece_t Board::readPiece(char c)
{
  switch(c)
  {
    case 'r': case 'R': return RAB;
    case 'c': case 'C': return CAT;
    case 'd': case 'D': return DOG;
    case 'h': case 'H': return HOR;
    case 'm': case 'M': return CAM;
    case 'e': case 'E': return ELE;
    case ' ': case '.': case 'x': case 'X': case '*': return EMP;
    default: Global::fatalError(string("Board::readPiece: Could not parse: ") + c);return EMP;
  }
}
piece_t Board::readPiece(const string& s)
{
  if(s.length() == 1)
    return readPiece(s[0]);

  if(s.length() != 3)
    Global::fatalError(string("Board::readPiece: Could not parse: ") + s);

  string t = Global::toLower(s);
  if(t == "ele") return ELE;
  if(t == "cam") return CAM;
  if(t == "hor") return HOR;
  if(t == "dog") return DOG;
  if(t == "cat") return CAT;
  if(t == "rab") return RAB;
  if(t == "emp") return EMP;

  Global::fatalError(string("Board::readPiece: Could not parse: ") + s);
  return EMP;
}
piece_t Board::readPiece(const char* s)
{
  return readPiece(string(s));
}

loc_t Board::readLoc(const char* s)
{
  if(!(s[0] != 0 && s[1] != 0))
    Global::fatalError(string("Board::readLoc: Could not parse: ") + s);

  char x = s[0]-'a';
  char y = s[1]-'1';

  if(!(x >= 0 && x < 8) || !(y >= 0 && y < 8))
    Global::fatalError(string("Board::readLoc: Could not parse: ") + s);

  return gLoc(x,y);
}
loc_t Board::readLoc(const string& s)
{
  return readLoc(s.c_str());
}

bool Board::tryReadLoc(const char* s, loc_t& loc)
{
  if(s[0] == 0 || s[1] == 0)
    return false;
  char x = s[0]-'a';
  char y = s[1]-'1';
  if(!(x >= 0 && x < 8) || !(y >= 0 && y < 8))
    return false;
  loc = gLoc(x,y);
  return true;
}
bool Board::tryReadLoc(const string& s, loc_t& loc)
{
  return tryReadLoc(s.c_str(),loc);
}

loc_t Board::readLocWithErrloc(const char* s)
{
  if(!(s[0] != 0 && s[1] != 0))
    Global::fatalError(string("Board::readLocWithErrloc: Could not parse: ") + s);
  if(strcmp(s,"errloc") == 0)
    return ERRLOC;

  char x = s[0]-'a';
  char y = s[1]-'1';

  if(!(x >= 0 && x < 8) || !(y >= 0 && y < 8))
    Global::fatalError(string("Board::readLocWithErrloc: Could not parse: ") + s);

  return gLoc(x,y);
}
loc_t Board::readLocWithErrloc(const string& s)
{
  return readLocWithErrloc(s.c_str());
}

bool Board::tryReadLocWithErrloc(const char* s, loc_t& loc)
{
  if(s[0] == 0 || s[1] == 0)
    return false;
  if(strcmp(s,"errloc") == 0)
  {loc = ERRLOC; return true;}
  char x = s[0]-'a';
  char y = s[1]-'1';
  if(!(x >= 0 && x < 8) || !(y >= 0 && y < 8))
    return false;
  loc = gLoc(x,y);
  return true;
}
bool Board::tryReadLocWithErrloc(const string& s, loc_t& loc)
{
  return tryReadLocWithErrloc(s.c_str(),loc);
}

int Board::readPlaTurn(const char* s)
{
  return readPlaTurn(string(s));
}
int Board::readPlaTurn(const string& s)
{
  int turn = 0;
  if(!tryReadPlaTurn(s,turn))
    Global::fatalError(string("Board: invalid platurn: ") + s);
  return turn;
}
bool Board::tryReadPlaTurn(const char* s, int& turn)
{
  return tryReadPlaTurn(string(s),turn);
}
bool Board::tryReadPlaTurn(const string& s, int& turn)
{
  int len = s.length();
  if(len < 2)
    return false;
  pla_t pla;
  if(!tryReadPla(s[len-1],pla) || Global::isDigit(s[len-1]))
    return false;

  if(pla == NPLA)
    return false;

  if(!Global::isDigits(s,0,len-1))
    return false;

  int t = Global::parseDigits(s,0,len-1);

  if((t & 0x3FFFFFFF) != t)
    return false;

  turn = (t-2) * 2 + (pla == SILV ? 1 : 0);
  return true;
}

int Board::readPlaTurnOrInt(const char* s)
{
  return readPlaTurnOrInt(string(s));
}
int Board::readPlaTurnOrInt(const string& s)
{
  int turn = 0;
  if(!tryReadPlaTurnOrInt(s,turn))
    Global::fatalError(string("Board: invalid platurn or int: ") + s);
  return turn;
}
bool Board::tryReadPlaTurnOrInt(const char* s, int& turn)
{
  return tryReadPlaTurnOrInt(string(s),turn);
}
bool Board::tryReadPlaTurnOrInt(const string& s, int& turn)
{
  if(Global::tryStringToInt(s,turn))
    return true;
  return tryReadPlaTurn(s,turn);
}

//BOARD INPUT----------------------------------------------------------------------------

static bool isAlpha(char c) {return Global::isAlpha(c);}
static bool isDigit(char c) {return Global::isDigit(c);}
static bool isDigits(const string& str) {return Global::isDigits(str);}
static bool isDigits(const string& str, int start, int end) {return Global::isDigits(str,start,end);}
static int parseDigits(const string& str) {return Global::parseDigits(str);}
static int parseDigits(const string& str, int start, int end) {return Global::parseDigits(str,start,end);}

static bool tryFillBoardRow(char bchars[64], int y, const string& boardLine, bool allowSpaces)
{
  //Ensure we have a valid index
  if(y < 0 || y >= 8)
    return false;
  //Ensure it is the right size to be a board row - 8 exactly, or around 16
  int size = boardLine.size();
  if(size != 8 && size != 15 && size != 16 && size != 17)
    return false;
  //Ensure it is composed only of board charcters
  for(int i = 0; i<size; i++)
  {
    if(!Global::strContains(VALIDBOARDCHARS,boardLine[i]))
      return false;
  }

  int skip = (boardLine.size()+1)/8;
  int start = (size >= 16) ? 1 : 0;

  //Scan across and check for spaces in the board input
  if(!allowSpaces)
    for(int x = 0; x<8; x++)
      if(boardLine[start+x*skip] == ' ')
        return false;

  //Ensure that everything else is empty
  for(int i = 0; i<size; i++)
    if((i-start+skip)%skip != 0 && boardLine[i] != ' ')
      return false;

  //Scan across and set pieces!
  for(int x = 0; x<8; x++)
    bchars[y*8+x] = boardLine[start+x*skip];
  return true;
}

static bool isIsolatedToken(const string& line, size_t pos, size_t endPos)
{
  if(pos > 0 && pos <= line.length() && (isAlpha(line[pos-1]) || isDigit(line[pos-1])))
    return false;
  if(endPos < line.length() && (isAlpha(line[endPos]) || isDigit(line[endPos])))
    return false;
  return true;
}

static bool lookForKeyValueNumber(const map<string,string>& keyValues, const string& key, int lowerBound, int upperBound, int& number)
{
  map<string,string>::const_iterator iter = keyValues.find(key);
  if(iter == keyValues.end())
    return false;
  const string& value = iter->second;
  if(!isDigits(value))
    Global::fatalError(string("Board: could not parse value for override: ") + key + " " + value);
  int num = parseDigits(value);
  if(num < lowerBound || num >= upperBound)
    Global::fatalError(string("Board: could not parse value for override: ") + key + " " + value);
  number = num;
  return true;
}

static bool lookForKeyValueTurn(const map<string,string>& keyValues, const string& key, int lowerBound, int upperBound, int& number)
{
  map<string,string>::const_iterator iter = keyValues.find(key);
  if(iter == keyValues.end())
    return false;
  const string& value = iter->second;
  int turn = 0;
  if(!Board::tryReadPlaTurnOrInt(value,turn))
  {
    if(!isDigits(value))
      Global::fatalError(string("Board: could not parse value for override: ") + key + " " + value);
    turn = parseDigits(value);
  }
  if(turn < lowerBound || turn >= upperBound)
    Global::fatalError(string("Board: could not parse value for override: ") + key + " " + value);
  number = turn;
  return true;
}

BoardRecord BoardRecord::read(const string& str, bool ignoreConsistency)
{
  Board b = Board();
  char bchars[64];
  for(int i = 0; i<64; i++)
    bchars[i] = '.';

  int linesSucceeded = 0; //Track # of rows of board successfully read in
  bool readingBoard = false; //Have we found where the board encoding begins?
  istringstream in(str);  //Turn str into a stream so we can go line by line

  //Special orientation overrides
  bool overrideNToS = false;
  bool overrideSToN = false;
  bool overrideLRMirror = false;

  //Board rows must be contiguous
  //Read each row...
  //Pound characters act as comments
  string line;
  while(getline(in,line))
  {
    line = Global::trim(line);
    //If it has exactly 64 valid non-space characters
    int lineLen = line.length();
    if(lineLen >= 64)
    {
      int numValid = 0;
      for(int i = 0; i<lineLen; i++)
        if(line[i] != ' ' && Global::strContains(VALIDBOARDCHARS,line[i]))
          numValid++;
      if(numValid == 64)
      {
        loc_t loc = 0;
        for(int i = 0; i<lineLen; i++)
          if(line[i] != ' ' && Global::strContains(VALIDBOARDCHARS,line[i]))
            bchars[loc++] = line[i];
        readingBoard = true;
        linesSucceeded += 8;
        continue;
      }
    }

    //If it has exactly 7 slashes and all other characters are digits from 1-8 or piecechars
    {
      int numSlashes = 0;
      bool bad = false;
      int numSpaces = 0;
      for(int i = 0; i<lineLen; i++)
      {
        if(numSpaces > 8) {bad = true; break;}
        if(line[i] == '/') {numSpaces = 0; numSlashes++; continue;}
        if(line[i] >= '1' && line[i] <= '8') {numSpaces += line[i]-'0'; continue;}
        if(Global::strContains(VALIDPIECECHARS,line[i])) {numSpaces++; continue;}
        bad = true;
        break;
      }
      if(numSpaces > 8) {bad = true;}
      if(numSlashes == 7 && !bad)
      {
        loc_t loc = 0;
        numSlashes = 0;
        for(int i = 0; i<lineLen; i++)
        {
          if(line[i] == '/') {numSlashes++; loc = numSlashes*8; continue;}
          if(line[i] >= '1' && line[i] <= '8') {loc += line[i]-'0'; continue;}
          bchars[loc++] = line[i];
        }
        readingBoard = true;
        linesSucceeded += 8;
        continue;
      }
    }

    //If it's has vertical bars, maybe it's a board row delimited by them?
    if(line.find('|') != string::npos)
    {
      //Find the boundaries
      size_t start = line.find_first_of('|')+1;
      size_t end = line.find_last_of('|');

      //Figure out which row of the board to fill.
      //If there's a number before the vertical bars, use that to determine it, 1-indexed
      //Else use the #of lines succeeded
      int y;
      if(start > 0 && isDigit(line[start-1]))
        y = line[start-1] - '1';
      else
        y = linesSucceeded;

      //Try to fill it as a board row
      if(y >= 0 && y < 8 && tryFillBoardRow(bchars,y,line.substr(start,end-start),true))
      {linesSucceeded++; readingBoard = true; continue;}
    }

    //If it's exactly 8 characters, maybe it's a board row then?
    if(line.size() == 8)
    {
      if(linesSucceeded < 8 && tryFillBoardRow(bchars,linesSucceeded,line,false))
      {linesSucceeded++; readingBoard = true; continue;}
    }

    //If we got here, we didn't find a board line, but if we're reading the board and not done...
    if(readingBoard && linesSucceeded < 8)
      Global::fatalError("Board: error reading board:\n" + str);

    //If it begins with something line 4b or 22g, then it specifies a side to move...
    int i = 0;
    while(i < lineLen && isDigit(line[i])) {i++;}
    if(i < lineLen && i > 0)
    {
      int turnNumber = parseDigits(line,0,i);
      if(line[i] == 'b' || line[i] == 's') {b.setPlaStep(0,b.step); b.turnNumber = max(turnNumber*2-4,0);}
      else if(line[i] == 'w' || line[i] == 'g') {b.setPlaStep(1,b.step); b.turnNumber = max(turnNumber*2-3,0);}
    }

    //Scan through the string looking for special codes
    static const char* nToSKeyword = "N_TO_S";
    static const int nToSKeywordLen = strlen(nToSKeyword);
    static const char* sToNKeyword = "S_TO_N";
    static const int sToNKeywordLen = strlen(sToNKeyword);
    static const char* lrMirrorKeyword = "LRMIRROR";
    static const int lrMirrorKeywordLen = strlen(lrMirrorKeyword);
    size_t pos;

    pos = line.find(nToSKeyword);
    if(pos != string::npos && isIsolatedToken(line, pos, pos + nToSKeywordLen))
      overrideNToS = true;

    pos = line.find(sToNKeyword);
    if(pos != string::npos && isIsolatedToken(line, pos, pos + sToNKeywordLen))
      overrideSToN = true;

    pos = line.find(lrMirrorKeyword);
    if(pos != string::npos && isIsolatedToken(line, pos, pos + lrMirrorKeywordLen))
      overrideLRMirror = true;
  }

  if(linesSucceeded != 8)
    Global::fatalError("Board: error reading board:\n" + str);

  if(overrideNToS && overrideSToN)
    Global::fatalError("Board: both overrides specified:\n" + str);

  //Flip board
  if(!overrideSToN)
  {
    if(!overrideLRMirror)
    {
      for(int y = 0; y<8; y++)
        for(int x = 0; x<8; x++)
          b.setPiece(gLoc(x,7-y), Board::readOwner(bchars[y*8+x]), Board::readPiece(bchars[y*8+x]));
    }
    else
    {
      for(int y = 0; y<8; y++)
        for(int x = 0; x<8; x++)
          b.setPiece(gLoc(7-x,7-y), Board::readOwner(bchars[y*8+x]), Board::readPiece(bchars[y*8+x]));
    }
  }
  else
  {
    if(!overrideLRMirror)
    {
      for(int y = 0; y<8; y++)
        for(int x = 0; x<8; x++)
          b.setPiece(gLoc(x,y), Board::readOwner(bchars[y*8+x]), Board::readPiece(bchars[y*8+x]));
    }
    else
    {
      for(int y = 0; y<8; y++)
        for(int x = 0; x<8; x++)
          b.setPiece(gLoc(7-x,y), Board::readOwner(bchars[y*8+x]), Board::readPiece(bchars[y*8+x]));
    }
  }

  map<string,string> keyValues = ArimaaIO::readKeyValues(str);

  //Special keywords
  int plaOverride = 0;
  static const char* plaKeyword = "P";
  if(lookForKeyValueNumber(keyValues,plaKeyword,0,2,plaOverride))
    b.setPlaStep(plaOverride,b.step);

  int stepOverride = 0;
  static const char* stepKeyword = "S";
  if(lookForKeyValueNumber(keyValues,stepKeyword,0,4,stepOverride))
    b.setPlaStep(b.player,stepOverride);

  int turnOverride = 0;
  static const char* turnKeyword = "T";
  if(lookForKeyValueTurn(keyValues,turnKeyword,0,1000000000,turnOverride))
    b.turnNumber = turnOverride;

  b.refreshStartHash();

  if(!ignoreConsistency && !b.testConsistency(cout))
    Global::fatalError("Board: inconsistent board:\n" + str);

  BoardRecord bret;
  bret.board = b;
  bret.keyValues = keyValues;
  return bret;
}

Board Board::read(const char* str, bool ignoreConsistency)
{
  return BoardRecord::read(string(str),ignoreConsistency).board;
}

Board Board::read(const string& str, bool ignoreConsistency)
{
  return BoardRecord::read(str,ignoreConsistency).board;
}

//READING MOVES----------------------------------------------------------------------------

bool Board::tryReadStep(const char* wrd, step_t& step)
{
  return tryReadStep(string(wrd),step);
}
bool Board::tryReadStep(const string& wrd, step_t& step)
{
  if(wrd == "pass")
  {step = PASSSTEP; return true;}

  if(wrd == "qpss")
  {step = QPASSSTEP; return true;}

  char xchar;
  char ychar;
  char dirchar;
  if(wrd.size() == 4)
  {
    //Ensure the first character really is valid
    if(!Global::strContains(VALIDPIECECHARS,wrd[0]))
      return false;

    xchar = wrd[1];
    ychar = wrd[2];
    dirchar = wrd[3];
  }
  else if(wrd.size() == 3)
  {
    xchar = wrd[0];
    ychar = wrd[1];
    dirchar = wrd[2];
  }
  else
    return false;

  //Ensure the characters specify a location
  if(xchar < 'a' || xchar > 'h')
    return false;
  if(ychar < '1' || ychar > '8')
    return false;

  //Parse out the location
  int x = xchar-'a';
  int y = ychar-'1';
  loc_t k = gLoc(x,y);

  //Create the step
  if     (dirchar == 's' && y > 0) {step = gStepSrcDest(k,k S);}
  else if(dirchar == 'w' && x > 0) {step = gStepSrcDest(k,k W);}
  else if(dirchar == 'e' && x < 7) {step = gStepSrcDest(k,k E);}
  else if(dirchar == 'n' && y < 7) {step = gStepSrcDest(k,k N);}
  else if(dirchar == 'x'         ) {step = ERRSTEP;} // Capture token
  else {return false;}

  return true;
}

step_t Board::readStep(const char* str)
{
  return readStep(string(str));
}
step_t Board::readStep(const string& str)
{
  step_t step = ERRSTEP;
  bool suc = tryReadStep(str,step);
  if(!suc)
    Global::fatalError(string("Board: invalid step: ") + str);
  return step;
}

bool Board::tryReadMove(const char* str, move_t& ret)
{
  return tryReadMove(string(str),ret);
}
bool Board::tryReadMove(const string& str, move_t& ret)
{
  istringstream in(str);
  string wrd;
  move_t move = ERRMOVE;
  int ns = 0;
  while(in >> wrd)
  {
    if(wrd.length() == 0)
      break;
    step_t step;
    bool suc = tryReadStep(wrd,step);
    if(!suc)
      return false;

    if(step == ERRSTEP)
      continue;

    if(ns >= 4)
      return false;

    move = setStep(move,step,ns);
    ns++;
  }

  ret = move;
  return true;
}

move_t Board::readMove(const char* str)
{
  return readMove(string(str));
}
move_t Board::readMove(const string& str)
{
  move_t move = ERRMOVE;
  bool suc = tryReadMove(str,move);
  if(!suc)
    Global::fatalError(string("Board: invalid move: ") + str);
  return move;
}

bool Board::tryReadPlacement(const char* wrd, Placement& ret)
{
  return tryReadPlacement(string(wrd),ret);
}
bool Board::tryReadPlacement(const string& wrd, Placement& ret)
{
  if(wrd.size() != 3)
    return false;

  char piecechar = wrd[0];
  char xchar = wrd[1];
  char ychar = wrd[2];

  if(!Global::strContains(VALIDPIECECHARS,piecechar))
    return false;
  if(xchar < 'a' || xchar > 'h')
    return false;
  if(ychar < '1' || ychar > '8')
    return false;

  //Parse out the location
  int x = xchar-'a';
  int y = ychar-'1';
  loc_t k = gLoc(x,y);

  pla_t owner = readOwner(piecechar);
  piece_t piece = readPiece(piecechar);

  ret.loc = k;
  ret.owner = owner;
  ret.piece = piece;
  return true;
}

Board::Placement Board::readPlacement(const string& str)
{
  Placement p;
  bool suc = tryReadPlacement(str,p);
  if(!suc)
    Global::fatalError(string("Board: invalid placement: ") + str);
  return p;
}

bool Board::tryReadPlacements(const char* str, vector<Placement>& ret)
{
  return tryReadPlacements(string(str),ret);
}

bool Board::tryReadPlacements(const string& str, vector<Placement>& ret)
{
  vector<string> pieces = Global::split(str);
  ret.clear();
  for(int i = 0; i<(int)pieces.size(); i++)
  {
    Placement p;
    if(!tryReadPlacement(pieces[i],p))
      return false;
    ret.push_back(p);
  }
  return true;
}

vector<move_t> Board::readMoveSequence(const string& str)
{return readMoveSequence(str,0);}

vector<move_t> Board::readMoveSequence(const string& str, int initialStep)
{
  vector<move_t> moveList;
  move_t move = ERRMOVE;

  //Read token by token
  istringstream in(str);
  string wrd;
  bool isFirst = true;
  while(in >> wrd)
  {
    int size = wrd.size();

    //Tokens like 2w, 34b - indicates turn change, ignore them
    if(size >= 2 && isDigits(wrd,0,size-1) && (wrd[size-1] == 'g' || wrd[size-1] == 'w' || wrd[size-1] == 's' || wrd[size-1] == 'b'))
      continue;

    step_t step;
    if(tryReadStep(wrd,step))
    {
      if(step == ERRSTEP)
        continue;

      int ns = numStepsInMove(move);
      if(ns >= 4 || (isFirst && ns >= 4-initialStep))
      {
        moveList.push_back(move);
        isFirst = false;
        move = ERRMOVE;
        ns = 0;
      }
      move = setStep(move,step,ns);
      if(step == PASSSTEP || step == QPASSSTEP)
      {
        moveList.push_back(move);
        isFirst = false;
        move = ERRMOVE;
      }
      continue;
    }

    cout << "Board: unknown move token: " << wrd << endl;
    cout << str << endl;
    break;
  }

  //Append a finishing move if one exists
  if(move != ERRMOVE)
    moveList.push_back(move);
  return moveList;
}

GameRecord GameRecord::read(const string& str)
{
  Board b;
  vector<move_t> moveList;

  //Tracking vars
  //-3: before 1w. -2: during 1w. -1: during 1b. 0: during 2g
  int moveIndex = -3; //Begins -3 because we have to pass 1w and 1b and 2w to begin actual moves
  move_t move = ERRMOVE;
  pla_t activePla = SILV;

  //Read line by line...
  string line;
  istringstream in_line(str);
  vector<string> errors;
  while(getline(in_line,line))
  {
    //Skip key value pairs
    if(line.find_first_of('=') != string::npos)
      continue;

    //Read token by token
    istringstream in(line);
    string wrd;
    while(in >> wrd)
    {
      int size = wrd.size();

      if(wrd == "takeback")
      {
        //Takeback! So we need to unwind the last move made.
        //Decrement twice because the next turn change token will increment again
        //Like 2w <move> 2b takeback 2w <new move>
        moveIndex -= 2;
        activePla = ((moveIndex+4) % 2 == 0) ? GOLD : SILV;

        //Need to clear the board of placements
        if(moveIndex <= -3)
          b = Board();

        //Need to clear just silver
        if(moveIndex == -2)
          for(int x = 0; x<BSIZE; x++)
            if(!(x & 0x88) && b.owners[x] == SILV)
              b.setPiece(x,NPLA,EMP);

        //Clear move
        move = ERRMOVE;
        continue;
      }

      if(wrd == "resigns" || wrd == "resign")
      {
        //Clear move
        move = ERRMOVE;
        break;
      }

      //Tokens like 2w, 34b - indicates turn change
      if(size >= 2 && isDigits(wrd,0,size-1) && (wrd[size-1] == 'g' || wrd[size-1] == 'w' || wrd[size-1] == 's' || wrd[size-1] == 'b'))
      {
        //Numbers can't be too large
        if(size > 10)
        {errors.push_back("Board: value too large: " + wrd); break;}

        //Count up
        moveIndex++;
        activePla = ((moveIndex+4) % 2 == 0) ? GOLD : SILV;

        //Ensure things match
        int wrdnum = parseDigits(wrd,0,size-1);
        if(wrdnum != (moveIndex+4)/2 || ((wrd[size-1] == 'g' || wrd[size-1] == 'w') != (activePla == GOLD)))
        {errors.push_back("Board: turn number not valid: " + wrd); break;}

        //Append move, except if there were no steps at all, this probably is an empty move or follows a takeback or something
        if(moveIndex >= 1 && move != ERRMOVE)
        {
          //Append a pass if not all steps taken and no pass
          move = completeTurn(move);

          //Expand list if needed
          if((int)moveList.size() < moveIndex)
            moveList.resize(moveIndex);
          moveList[moveIndex-1] = move;
        }

        //Clear move
        move = ERRMOVE;

        continue;
      }

      Board::Placement placement;
      if(Board::tryReadPlacement(wrd,placement))
      {
        //Fail on placements which happen after 1w/1b
        if(moveIndex < 0)
          b.setPiece(placement.loc,placement.owner,placement.piece);
        else
        {cout << "Board: illegal placement after first turn" << endl; cout << str << endl; break;}

        continue;
      }

      step_t step;
      if(Board::tryReadStep(wrd,step))
      {
        if(step == ERRSTEP)
          continue;

        int ns = numStepsInMove(move);
        if(ns >= 4)
        {errors.push_back("Board: Too many steps in move " + Board::writePlaTurn(moveIndex)); break;}
        move = setStep(move,step,ns);

        continue;
      }

      errors.push_back("Board: Unknown move token: " + wrd);
      break;
    }
  }

  if(errors.size() > 0)
  {
    cout << "Error parsing game: " << str << endl;
    for(int i = 0; i<(int)errors.size(); i++)
      cout << errors[i] << endl;
  }

  //Append a finishing move if one exists
  if(move != ERRMOVE && moveIndex >= 0)
  {
    //Append a pass if not all steps taken and no pass
    move = completeTurn(move);

    //Expand list if needed
    if((int)moveList.size() < moveIndex+1)
      moveList.resize(moveIndex+1);
    moveList[moveIndex] = move;
    moveIndex++;
    activePla = ((moveIndex+4) % 2 == 0) ? GOLD : SILV;
  }

  if(moveIndex >= 0)
    moveList.resize(moveIndex);

  //Set appropriate player
  if(b.pieceCounts[GOLD][0] == 0 && b.pieceCounts[SILV][0] == 0)
    b.setPlaStep(moveIndex == -1 ? SILV : GOLD,0);
  else if(b.pieceCounts[SILV][0] == 0)
    b.setPlaStep(moveIndex == 0 ? GOLD : SILV,0);
  else
    b.setPlaStep(GOLD,0);

  //Done updating board
  b.refreshStartHash();

  //Hack to make the movelist work for things that start with 2g pass to switch the side
  if(moveList.size() > 0 && moveList[0] == PASSMOVE)
    moveList[0] = QPASSMOVE;

  //Verify legality
  pla_t winner = NPLA;
  Board copy = b;
  for(int i = 0; i<(int)moveList.size(); i++)
  {
    //Game should not be over yet
    if(winner != NPLA)
    {
      cout << "Board: Game continues after a player has won: " << Board::writePlaTurn(i) << endl;

      //Limit the damage
      moveList.resize(i);
      break;
    }

    //Move must be legal
    bool suc = copy.makeMoveLegalNoUndo(moveList[i]);
    if(!suc)
    {
      cout << "Board: Illegal move " << Board::writePlaTurn(i) << " : " << Board::writeMove(copy,moveList[i]) << endl;

      //Limit the damage
      moveList.resize(i);
      break;
    }

    //Check winning conditions
    pla_t pla = copy.player;
    pla_t opp = gOpp(pla);
    if(copy.isGoal(opp)) winner = opp;
    else if(copy.isGoal(pla)) winner = pla;
    else if(copy.isRabbitless(pla)) winner = opp;
    else if(copy.isRabbitless(opp)) winner = pla;
    else if(copy.noMoves(pla)) winner = opp;
  }

  map<string,string> keyValues = ArimaaIO::readKeyValues(str);

  return GameRecord(b,moveList,winner,keyValues);
}





