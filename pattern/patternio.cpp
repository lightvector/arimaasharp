/*
 * patternio.cpp
 * Author: davidwu
 */

#include <sstream>
#include "../core/global.h"
#include "../board/btypes.h"
#include "../board/board.h"
#include "../pattern/pattern.h"
#include "../program/arimaaio.h"

//CONDITIONS------------------------------------------------------------------------------

ostream& operator<<(ostream& out, const Condition& cond)
{
  Condition::write(out,cond,true);
  return out;
}

string Condition::write(const Condition& cond, bool ignoreErrLoc)
{
  ostringstream out(ostringstream::out);
  write(out,cond,ignoreErrLoc);
  return out.str();
}

void Condition::write(ostream& out, const Condition& cond, bool ignoreErrLoc)
{
  int v1 = cond.v1;
  int loc = cond.loc;
  int istrue = cond.istrue;

  switch(cond.type)
  {
  case Condition::AND:
    if(!istrue)
    {
      out << "OR ";
      write(out,!(*cond.c1),ignoreErrLoc);
      out << " ";
      write(out,!(*cond.c2),ignoreErrLoc);
    }
    else
    {
      out << "AND ";
      write(out,*cond.c1,ignoreErrLoc);
      out << " ";
      write(out,*cond.c2,ignoreErrLoc);
    }
    break;
  default:
    if(!istrue)
      out << "!";
    string locstr;
    if(!ignoreErrLoc || loc != ERRLOC)
      locstr = Board::writeLoc(loc) + " ";
    switch(cond.type)
    {
    case Condition::COND_TRUE: out << "COND_TRUE"; break;
    case Condition::OWNER_IS: out << "OWNER_IS " << locstr << Board::writePla(v1); break;
    case Condition::PIECE_IS: out << "PIECE_IS " << locstr << Board::writePieceWord(v1); break;
    case Condition::LEQ_THAN_LOC: out << "LEQ_THAN " << locstr << Board::writeLoc(v1); break;
    case Condition::LESS_THAN_LOC: out << "LESS_THAN " << locstr << Board::writeLoc(v1); break;
    case Condition::ADJ_PLA: out << "ADJ_PLA " << locstr << Board::writePla(v1); break;
    case Condition::ADJ2_PLA: out << "ADJ2_PLA " << locstr << Board::writePla(v1); break;
    case Condition::FROZEN: out << "FROZEN " << locstr << Board::writePla(v1); break;
    case Condition::DOMINATED: out << "DOMINATED " << locstr << Board::writePla(v1); break;
    case Condition::IS_OPEN: out << "IS_OPEN " << locstr; break;
    case Condition::UNKNOWN: out << "UNKNOWN " << locstr << v1; break;
    default: DEBUGASSERT(false); break;
    }
    break;
  }
}

Condition Condition::readRecursive(const string& original, istringstream& in)
{
  string wrd;
  in >> wrd;
  if(in.bad() || wrd.size() <= 0)
  {Global::fatalError("Condition: Could not parse condition: " + original);}
  int size = wrd.size();

  for(int i = 0; i<size; i++)
    wrd[i] = toupper(wrd[i]);

  bool negated = (wrd[0] == '!');
  if(negated)
    wrd = wrd.substr(1);

  Condition c;

  loc_t esq = ERRLOC;
  if(wrd == "OWNER_IS")       {string s; in >> s; c = ownerIs(esq,Board::readPla(s));}
  else if(wrd == "PIECE_IS")  {string s; in >> s; c = pieceIs(esq,Board::readPiece(s));}
  else if(wrd == "LEQ_THAN")  {string s; in >> s; c = leqThanLoc(esq,Board::readLoc(s));}
  else if(wrd == "LESS_THAN") {string s; in >> s; c = lessThanLoc(esq,Board::readLoc(s));}
  else if(wrd == "ADJ_PLA")   {string s; in >> s; c = adjPla(esq,Board::readPla(s));}
  else if(wrd == "ADJ2_PLA")  {string s; in >> s; c = adj2Pla(esq,Board::readPla(s));}
  else if(wrd == "FROZEN")    {string s; in >> s; c = frozen(esq,Board::readPla(s));}
  else if(wrd == "DOMINATED") {string s; in >> s; c = dominated(esq,Board::readPla(s));}
  else if(wrd == "IS_OPEN")   {c = Condition::isOpen(esq);}
  else if(wrd == "UNKNOWN")   {string s; in >> s; c = Condition::unknown(Global::stringToInt(s));}
  else if(wrd == "AND")       {Condition c1 = readRecursive(original,in); Condition c2 = readRecursive(original,in); c = c1 && c2;}
  else {Global::fatalError("Condition: Could not parse condition(failed on " + wrd + "): " + original);}

  if(in.bad())
  {Global::fatalError("Condition: Could not parse condition (bad): " + original);}

  if(negated)
    return !c;
  else
    return c;

}

Condition Condition::read(const string& arg)
{
  string str = arg;
  int size = str.size();
  for(int i = 0; i<size; i++)
    if(!isalpha(str[i]) && !isdigit(str[i]) && str[i] != '!')
      str[i] = ' ';

  str = Global::trim(arg);
  istringstream in(str);
  return readRecursive(arg,in);
}

//PATTERNS-----------------------------------------------------------------------------------

ostream& operator<<(ostream& out, const Pattern& pattern)
{
  Pattern::write(out,pattern);
  return out;
}

ostream& operator<<(ostream& out, const PatternRecord& p)
{
  PatternRecord::write(out,p);
  return out;
}

string PatternRecord::write(const PatternRecord& p)
{
  ostringstream out(ostringstream::out);
  write(out,p);
  return out.str();
}

void PatternRecord::write(ostream& out, const PatternRecord& p)
{
  for(map<string,string>::const_iterator iter = p.keyValues.begin(); iter != p.keyValues.end(); ++iter)
    out << iter->first << " = " << iter->second << "\n";
  Pattern::write(out,p.pattern);
}

string Pattern::write(const Pattern& pattern)
{
  ostringstream out(ostringstream::out);
  write(out,pattern);
  return out.str();
}
void Pattern::write(ostream& out, const Pattern& pattern)
{
  for(int i = 0; i<(int)pattern.getNumConditions(); i++)
  {
    if(pattern.isDefault(pattern.getConditionName(i)))
      continue;
    out << pattern.getConditionName(i) << " = " << Condition::write(pattern.getCondition(i),true) << endl;
  }

  string patternPieces[128];
  for(int loc = 0; loc < BSIZE; loc++)
  {
    if(loc & 0x88)
      continue;
    const int* list = pattern.getList(loc);
    int ln = pattern.getListLen(loc);

    string longerNames;
    for(int i = 0; i<ln; i++)
    {
      const string& name = pattern.getConditionName(list[i]);
      if((int)name.length() == 1)
        patternPieces[loc] += name;
      else
      {
        longerNames += (longerNames.length() > 0 ? " " : "");
        longerNames += name;
      }
    }

    if(longerNames.length() > 0)
      patternPieces[loc] += "/" + longerNames;
  }

  int longest[8] = {1,1,1,1,1,1,1,1};
  for(int y = 7; y >= 0; y--)
  {
    for(int x = 0; x<8; x++)
    {
      if((int)patternPieces[gLoc(x,y)].length() > longest[x])
        longest[x] = patternPieces[gLoc(x,y)].length();
    }
  }

  out << "---" << endl;
  for(int y = 7; y >= 0; y--)
  {
    for(int x = 0; x<8; x++)
    {
      int loc = gLoc(x,y);
      out << patternPieces[loc];
      int numSpacesNeeded = longest[x] - patternPieces[loc].length();
      for(int i = 0; i<numSpacesNeeded; i++)
        out << " ";
      out << ",";
    }
    if(y >= 0) out << endl;
  }
}

PatternRecord PatternRecord::read(const string& str)
{
  map<string,string> keyValues = ArimaaIO::readKeyValues(str);

  Pattern p;
  vector<string> conditionKeys;
  for(map<string,string>::const_iterator iter = keyValues.begin(); iter != keyValues.end(); ++iter)
  {
    const string& key = iter->first;
    if(key.length() != 1 && !Global::isDigits(key,0,key.size()) && !(key.length() > 1 && key[0] == '@'))
      continue;
    const string& data = iter->second;
    Condition cond = Condition::read(data);
    conditionKeys.push_back(key);
    p.addCondition(cond,key);
  }

  for(int i = 0; i<(int)conditionKeys.size(); i++)
    keyValues.erase(conditionKeys[i]);

  size_t start = str.find("---");
  if(start == string::npos)
    Global::fatalError("Pattern: could not parse, could not find \"---\":\n" + str);
  start += 3;
  istringstream commaIn(str.substr(start));
  string commaPart;
  int idx = 0;
  while(getline(commaIn,commaPart,','))
  {
    if(commaIn.bad())
      break;

    if(idx >= 64)
      break;

    commaPart = Global::trim(commaPart);

    int loc = gLoc(idx%8,7-idx/8);
    int size = commaPart.size();
    if(size == 0 && idx < 64)
      p.addConditionToLoc(p.getOrAddCondition(Condition::cTrue()),loc);
    else
    {
      int slashIdx = -1;
      for(int i = 0; i<size; i++)
      {
        if(commaPart[i] == '/')
        {slashIdx = i; break;}
        if(Global::isWhitespace(commaPart[i]))
          continue;
        string token; token.push_back(commaPart[i]);
        if(Pattern::isDefault(token))
        {
          Condition defaultCond;
          bool success = Pattern::maybeGetDefaultCondition(token,defaultCond);
          DEBUGASSERT(success);
          p.addConditionToLoc(p.getOrAddCondition(defaultCond),loc);
          continue;
        }
        int condIdx = p.findConditionIdxWithName(token);
        if(condIdx == -1)
          Global::fatalError("Pattern: could not parse, could not find condition with name:\n" + str);
        p.addConditionToLoc(condIdx,loc);
      }
      if(slashIdx != -1)
      {
        istringstream condIn(commaPart.substr(slashIdx+1));
        string token;
        while(condIn >> token)
        {
          if(!condIn.good())
            break;
          token = Global::trim(token);
          if(Pattern::isDefault(token))
          {
            Condition defaultCond;
            bool success = Pattern::maybeGetDefaultCondition(token,defaultCond);
            DEBUGASSERT(success);
            p.addConditionToLoc(p.getOrAddCondition(defaultCond),loc);
            continue;
          }
          int condIdx = p.findConditionIdxWithName(token);
          if(condIdx == -1)
            Global::fatalError("Pattern: could not parse, could not find condition with name:\n" + str);
          p.addConditionToLoc(condIdx,loc);
        }
      }
    }
    idx++;
  }

  while(getline(commaIn,commaPart))
  {
    if(!Global::isWhitespace(commaPart))
      Global::fatalError("Pattern: could not parse, extra data beyond end:\n" + str);
  }

  if(idx != 64)
    Global::fatalError("Pattern: could not parse, not 64 squares:\n" + str);
  return PatternRecord(p,keyValues);
}


