/*
 * decisiontreeio.cpp
 * Author: davidwu
 */

#include <sstream>
#include "../core/global.h"
#include "../pattern/decisiontree.h"

ostream& operator<<(ostream& out, const Decision& d)
{
  Decision::write(out,d);
  return out;
}
string Decision::write(const Decision& d)
{
  ostringstream out;
  write(out,d);
  return out.str();
}
void Decision::write(ostream& out, const Decision& d)
{
  string locstr = Board::writeLoc(d.loc);
  switch(d.type)
  {
  case Decision::OWNER:      out << "OWNER "    << locstr << " " << d.dest[0] << " " << d.dest[1] << " " << d.dest[2]; break;
  case Decision::STRCOMPARE: out << "STRCMP "   << locstr << " " << Board::writeLoc(d.v1) << " " << d.dest[0] << " " << d.dest[1] << " " << d.dest[2]; break;
  case Decision::IS_RABBIT:  out << "ISRAB "    << locstr << " " << d.dest[0] << " " << d.dest[1]; break;
  case Decision::IS_FROZEN:  out << "ISFROZEN " << locstr << " " << d.dest[0] << " " << d.dest[1]; break;
  default: DEBUGASSERT(false); break;
  }
}

Decision Decision::read(const string& str)
{
  istringstream in(str);
  string wrd;
  in >> wrd;
  if(in.bad() || wrd.size() <= 0)
  {Global::fatalError("Decision: Could not parse: " + str);}
  int size = wrd.size();

  for(int i = 0; i<size; i++)
    wrd[i] = toupper(wrd[i]);

  int type = 0;
  loc_t loc = ERRLOC;
  int v1 = 0;
  int dest0 = -1;
  int dest1 = -1;
  int dest2 = -1;

  string s;
  if(wrd == "OWNER")         {type = Decision::OWNER; in >> s; loc = Board::readLoc(s); in >> dest0; in >> dest1; in >> dest2;}
  else if(wrd == "STRCMP")   {type = Decision::STRCOMPARE; in >> s; loc = Board::readLoc(s); in >> s; v1 = Board::readLoc(s); in >> dest0; in >> dest1; in >> dest2;}
  else if(wrd == "ISRAB")    {type = Decision::IS_RABBIT; in >> s; loc = Board::readLoc(s); in >> dest0; in >> dest1;}
  else if(wrd == "ISFROZEN") {type = Decision::IS_FROZEN; in >> s; loc = Board::readLoc(s); in >> dest0; in >> dest1;}
  else {Global::fatalError("Decision: Could not parse (failed on " + wrd + "): " + str);}

  if(in >> wrd)
  {Global::fatalError("Decision: Could not parse (bad): " + str);}

  return Decision(type,loc,v1,dest0,dest1,dest2);
}

ostream& operator<<(ostream& out, const DecisionTree& tree)
{
  DecisionTree::write(out,tree);
  return out;
}
string DecisionTree::write(const DecisionTree& tree)
{
  ostringstream out;
  write(out,tree);
  return out.str();
}
void DecisionTree::write(ostream& out, const DecisionTree& tree)
{
  out << "DecisionTree" << endl;
  int treeSize = tree.tree.size();
  out << treeSize << endl;
  out << tree.startDest << endl;
  for(int i = 0; i<treeSize; i++)
    out << i << " " << tree.tree[i] << endl;
}

DecisionTree DecisionTree::read(const string& str)
{
  istringstream in(str);
  string line;

  DecisionTree tree;

  if(!getline(in,line))
    Global::fatalError("DecisionTree: could not parse tree, unexpected end");
  if(line != "DecisionTree")
    Global::fatalError("DecisionTree: could not parse tree, first line is not DecisionTree: " + line);

  if(!getline(in,line))
    Global::fatalError("DecisionTree: could not parse tree, unexpected end");
  int treeSize = Global::stringToInt(line);
  if(!getline(in,line))
    Global::fatalError("DecisionTree: could not parse tree, unexpected end");
  tree.startDest = Global::stringToInt(line);

  for(int i = 0; i<treeSize; i++)
  {
    if(!getline(in,line))
      Global::fatalError("DecisionTree: could not parse tree, unexpected end on node " + Global::intToString(i));
    if(Global::isWhitespace(line))
    {i--; continue;}

    string substr = line.substr(line.find_first_not_of("0123456789"));
    tree.tree.push_back(Decision::read(substr));
  }

  while(getline(in,line))
    if(!Global::isWhitespace(line))
      Global::fatalError("DecisionTree: could not parse tree, unexpected data after end ");

  return tree;
}



