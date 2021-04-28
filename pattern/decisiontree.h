/*
 * decisiontree.h
 * Author: davidwu
 */

#ifndef DECISIONTREE_H_
#define DECISIONTREE_H_

#include "../core/global.h"
#include "../pattern/pattern.h"
using namespace std;

class Rand;

struct Decision
{
  static const int OWNER = 0;             //loc     -> Results 0,1,2 for SILV,GOLD,NPLA
  static const int STRCOMPARE = 1;        //loc,loc -> Results 0,1,2 for <, =, >
  static const int IS_RABBIT = 2;         //loc -> Results in 0,1 for nonrab, rab
  static const int IS_FROZEN = 3;         //loc -> Results in 0,1 for notfrozen, frozen
  int type;
  int loc;
  int v1;

  //For each possiblity, what index node do we step to next?
  //If the dest is negative, then we return immediately with value (-dest-1)
  int dest[3];

  Decision();
  Decision(int type, int loc, int dest0, int dest1);
  Decision(int type, int loc, int v1, int dest0, int dest1, int dest2);

  Decision lrFlip() const;
  Decision udColorFlip() const;

  static Decision read(const string& str);
  static string write(const Decision& d);
  static void write(ostream& out, const Decision& d);
  friend ostream& operator<<(ostream& out, const Decision& d);
};

class DecisionTree
{
  STRUCT_NAMED_TRIPLE(int,type,int,loc,int,v1,ScoreRet);
  STRUCT_NAMED_QUAD(Pattern,pattern,int,valueIfTrue,int,numTrue,Hash128,hash,PatternData);
  struct ScoreBuffer;

  int startDest;
  vector<Decision> tree;

  public:
  DecisionTree();
  ~DecisionTree();

  //Get the result of the decisionTree on the given board
  int get(const Board& board);

  //Various tree properties
  int getSize() const;
  int randomPathLength(Rand& rand) const;

  //Flip colors and north/south
  DecisionTree lrFlip() const;
  DecisionTree udColorFlip() const;

  //Currently only tuned for goal patterns, where silver is to goal and gold is defending
  //Only handles OWNER_IS, PIECE_IS for rabbits, strength comparisons, and frozen conditions
  static void compile(DecisionTree& tree, const vector<pair<Pattern,int> >& patterns, int valueIfFalse);

  static DecisionTree read(const string& str);
  static string write(const DecisionTree& t);
  static void write(ostream& out, const DecisionTree& t);
  friend ostream& operator<<(ostream& out, const DecisionTree& t);

  private:
  static int compileRec(
      DecisionTree& tree,
      int valueIfFalse,
      pla_t ownerKnownArray[BSIZE],
      map<Hash128,int>& hashIdxMap,
      ByteBuffer& hashBuffer,
      ScoreBuffer* scores,
      const vector<PatternData>& patterns
      );
  static int filterDownByRec(
      DecisionTree& tree,
      int valueIfFalse,
      pla_t ownerKnownArray[BSIZE],
      map<Hash128,int>& hashIdxMap,
      ByteBuffer& hashBuffer,
      ScoreBuffer* scores,
      const vector<PatternData>& patterns,
      loc_t loc,
      Condition cond
      );
};

#endif
