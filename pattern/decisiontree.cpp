/*
 * decisiontree.cpp
 * Author: davidwu
 */

#include <cmath>
#include "../core/global.h"
#include "../core/hash.h"
#include "../core/rand.h"
#include "../core/md5.h"
#include "../core/bytebuffer.h"
#include "../board/btypes.h"
#include "../board/board.h"
#include "../pattern/pattern.h"
#include "../pattern/decisiontree.h"

using namespace std;

Decision::Decision()
:type(0),loc(ERRLOC),v1(0)
{dest[0] = -1; dest[1] = -1; dest[2] = -1;}
Decision::Decision(int type, int loc, int dest0, int dest1)
:type(type),loc(loc),v1(0)
{dest[0] = dest0; dest[1] = dest1; dest[2] = -1;}
Decision::Decision(int type, int loc, int v1, int dest0, int dest1, int dest2)
:type(type),loc(loc),v1(v1)
{dest[0] = dest0; dest[1] = dest1; dest[2] = dest2;}

static int lrFlipLoc(int x) {return x == ERRLOC ? ERRLOC : gLoc(7-gX(x),gY(x));}
static int udFlipLoc(int x) {return x == ERRLOC ? ERRLOC : gLoc(gX(x),7-gY(x));}

Decision Decision::lrFlip() const
{
  switch(type)
  {
  case OWNER:      return Decision(type,lrFlipLoc(loc),v1,dest[0],dest[1],dest[2]);
  case STRCOMPARE: return Decision(type,lrFlipLoc(loc),lrFlipLoc(v1),dest[0],dest[1],dest[2]);
  case IS_RABBIT:  return Decision(type,lrFlipLoc(loc),v1,dest[0],dest[1],dest[2]);
  case IS_FROZEN:  return Decision(type,lrFlipLoc(loc),v1,dest[0],dest[1],dest[2]);
  default: DEBUGASSERT(false); break;
  }
  return Decision();
}
Decision Decision::udColorFlip() const
{
  switch(type)
  {
  case OWNER:      return Decision(type,udFlipLoc(loc),v1,dest[1],dest[0],dest[2]);
  case STRCOMPARE: return Decision(type,udFlipLoc(loc),udFlipLoc(v1),dest[0],dest[1],dest[2]);
  case IS_RABBIT:  return Decision(type,udFlipLoc(loc),v1,dest[0],dest[1],dest[2]);
  case IS_FROZEN:  return Decision(type,udFlipLoc(loc),v1,dest[0],dest[1],dest[2]);
  default: DEBUGASSERT(false); break;
  }
  return Decision();
}


DecisionTree::DecisionTree()
:startDest(-1)
{}
DecisionTree::~DecisionTree()
{}

int DecisionTree::getSize() const
{
  return tree.size();
}

int DecisionTree::randomPathLength(Rand& rand) const
{
  int len = 0;
  int idx = startDest;
  while(idx >= 0)
  {
    len++;
    const Decision& test = tree[idx];
    int legalPaths[3];
    int numLegalPaths = 0;
    for(int i = 0; i<3; i++)
      if(test.dest[i] >= 0)
        legalPaths[numLegalPaths++] = test.dest[i];
    if(numLegalPaths == 0)
      break;
    idx = legalPaths[rand.nextUInt(numLegalPaths)];
    continue;
  }
  return len;
}


DecisionTree DecisionTree::lrFlip() const
{
  DecisionTree newTree = *this;
  int treeSize = newTree.tree.size();
  for(int i = 0; i<treeSize; i++)
    newTree.tree[i] = newTree.tree[i].lrFlip();
  return newTree;
}

DecisionTree DecisionTree::udColorFlip() const
{
  DecisionTree newTree = *this;
  int treeSize = newTree.tree.size();
  for(int i = 0; i<treeSize; i++)
    newTree.tree[i] = newTree.tree[i].udColorFlip();
  return newTree;
}

int DecisionTree::get(const Board& board)
{
  //cout << "STARTING" << endl;
  int idx = startDest;
  while(idx >= 0)
  {
    const Decision& test = tree[idx];
    loc_t loc = test.loc;

    //cout << "idx " << idx << " " << test << endl;

    switch(test.type)
    {
    case Decision::OWNER:
      idx = test.dest[board.owners[loc]];
      break;
    case Decision::STRCOMPARE:
    {
      int loc2 = test.v1;
      if(board.pieces[loc] > board.pieces[loc2])
        idx = test.dest[2];
      else
        idx = test.dest[board.pieces[loc] == board.pieces[loc2]];
    }
      break;
    case Decision::IS_RABBIT:
      idx = test.dest[board.pieces[loc] == RAB];
      break;
    case Decision::IS_FROZEN:
      idx = test.dest[board.isFrozen(loc)];
      break;
    default: DEBUGASSERT(false); break;
    }
  }
  return -idx-1;
}

struct DecisionTree::ScoreBuffer
{
  double buffer[BSIZE][4];
  map<int,double> strCompareMap;

  ScoreBuffer() { clear(); }
  ~ScoreBuffer() {}

  void clear()
  {
    strCompareMap.clear();
    for(int i = 0; i<BSIZE; i++)
    {
      if(i & 0x88)
        continue;
      buffer[i][0] = 0;
      buffer[i][1] = 0;
      buffer[i][2] = 0;
      buffer[i][3] = 0;
    }
  }

  void add(int type, int loc, int v1, double value)
  {
    if(type == Decision::STRCOMPARE)
      strCompareMap[loc+v1*BSIZE] += value;
    else
      buffer[loc][type] += value;
  }

  double get(int type, int loc, int v1)
  {
    if(type == Decision::STRCOMPARE)
      return strCompareMap[loc+v1*BSIZE];
    else
      return buffer[loc][type];
  }

  ScoreRet getMax()
  {
    double maxScore = 0;
    ScoreRet maxRet;
    for(int loc = 0; loc<BSIZE; loc++)
    {
      if(loc & 0x88)
        continue;
      for(int type = 0; type<4; type++)
      {
        if(buffer[loc][type] > maxScore)
        {
          maxScore = buffer[loc][type];
          maxRet = ScoreRet(type,loc,0);
        }
      }
    }

    for(map<int,double>::const_iterator iter = strCompareMap.begin(); iter != strCompareMap.end(); ++iter)
    {
      double score = iter->second;
      if(score > maxScore)
      {
        maxScore = score;
        int idx = iter->first;
        maxRet = ScoreRet(Decision::STRCOMPARE,idx%BSIZE,idx/BSIZE);
      }
    }
    return maxRet;
  }

};

//Expects all patterns passed in to be not false, and have less than 64 true.
int DecisionTree::compileRec(
    DecisionTree& tree,
    int valueIfFalse,
    pla_t ownerKnownArray[BSIZE],
    map<Hash128,int>& hashIdxMap,
    ByteBuffer& hashBuffer,
    ScoreBuffer* scores,
    const vector<PatternData>& patterns
    )
{
  if(patterns.size() <= 0)
    return (-valueIfFalse-1);

  int numPatterns = patterns.size();

  //TODO the hash function does not take into account that valueIfTrue might differ
  int expectedNumBytes = numPatterns * 16 + 4 + 64;
  hashBuffer.clear();
  hashBuffer.setAutoExpand(true);
  hashBuffer.add32(numPatterns);
  for(int i = 0; i<numPatterns; i++)
  {
    hashBuffer.add64(patterns[i].hash.hash0);
    hashBuffer.add64(patterns[i].hash.hash1);
  }
  for(int loc = 0; loc<BSIZE; loc++)
  {
    const int unknownOwnerValue = 63;
    if(loc & 0x88)
      continue;
    if(ownerKnownArray[loc] == -1)
      hashBuffer.add8(unknownOwnerValue);
    else
    {
      bool locUndetermined = false;
      for(int i = 0; i < numPatterns; i++)
      {
        const PatternData& pattern = patterns[i];
        if(!pattern.pattern.isTrue(loc))
        {locUndetermined = true; break;}
      }
      //If the square is undetermined, so that some pattern still depends on checking it,
      //then we need to record the value, but otherwise, if no pattern cares about the value any more,
      //then we're isomorphic to not knowing its value
      if(locUndetermined)
        hashBuffer.add8(ownerKnownArray[loc]);
      else
        hashBuffer.add8(unknownOwnerValue);

    }
  }
  DEBUGASSERT(hashBuffer.numBytes == expectedNumBytes);

  Hash128 hash = MD5::get(hashBuffer.bytes,hashBuffer.numBytes);
  {
    map<Hash128,int>::const_iterator iter = hashIdxMap.find(hash);
    if(iter != hashIdxMap.end())
      return iter->second;
  }

  cout << "Node " << tree.tree.size() << " Patterns " << patterns.size() << " Hash " << hash << endl;

  scores->clear();

  int leafCondsSize = 256;
  Condition leafConds[leafCondsSize];
  for(int i = 0; i < numPatterns; i++)
  {
    const PatternData& pattern = patterns[i];
    double patternWeight = 1.0 / (64.0 - pattern.numTrue) / (64.0 - pattern.numTrue);
    for(loc_t loc = 0; loc < BSIZE; loc++)
    {
      if(loc & 0x88)
        continue;
      if(pattern.pattern.isTrue(loc))
        continue;
      if(ownerKnownArray[loc] == -1)
      {
        bool goldConflict = pattern.pattern.conflictsWith(Condition::ownerIs(ERRLOC,GOLD),loc);
        bool silvConflict = pattern.pattern.conflictsWith(Condition::ownerIs(ERRLOC,SILV),loc);
        bool nplaConflict = pattern.pattern.conflictsWith(Condition::ownerIs(ERRLOC,NPLA),loc);
        //MINOR apparently node count goes down a little if you make the weights much more similar
        //such as 777 555 3.
        DEBUGASSERT(!(goldConflict && silvConflict && nplaConflict));
        if(goldConflict && nplaConflict)
          scores->add(Decision::OWNER,loc,0,patternWeight*25); //Pattern requires silv
        else if(silvConflict && goldConflict)
          scores->add(Decision::OWNER,loc,0,patternWeight*10); //Pattern requires npla
        else if(silvConflict && nplaConflict)
          scores->add(Decision::OWNER,loc,0,patternWeight*8);  //Pattern requires gold
        else if(goldConflict)
          scores->add(Decision::OWNER,loc,0,patternWeight*7);  //Pattern forbids gold
        else if(nplaConflict)
          scores->add(Decision::OWNER,loc,0,patternWeight*5);  //Pattern forbids npla
        else if(silvConflict)
          scores->add(Decision::OWNER,loc,0,patternWeight*4);  //Pattern forbids silv
        else
          scores->add(Decision::OWNER,loc,0,patternWeight*2);  //Pattern forbids nothing
      }
      else
      {
        pla_t pla = ownerKnownArray[loc];
        DEBUGASSERT(pla != NPLA);
        int numLeafConds = pattern.pattern.getLeafConditions(loc,leafConds,leafCondsSize);
        if(numLeafConds == leafCondsSize)
          Global::fatalError("More than leafCondsSize leaf conditions on a single pattern square!");

        int factor = 1;
        if(numLeafConds <= 1)
          factor = 2;
        for(int c = 0; c<numLeafConds; c++)
        {
          if(leafConds[c].type == Condition::PIECE_IS)
          {
            DEBUGASSERT(leafConds[c].v1 == RAB);
            if(pla == SILV) scores->add(Decision::IS_RABBIT,loc,0,patternWeight*50*factor);
            else            scores->add(Decision::IS_RABBIT,loc,0,patternWeight*15*factor);
            break;
          }
        }
        Bitmap locsComparedWith;
        for(int c = 0; c<numLeafConds; c++)
        {
          if(leafConds[c].type == Condition::LESS_THAN_LOC || leafConds[c].type == Condition::LEQ_THAN_LOC)
          {
            loc_t v1 = leafConds[c].v1;
            if(locsComparedWith.isOne(v1))
              continue;
            locsComparedWith.setOn(v1);
            scores->add(Decision::STRCOMPARE,loc,v1,patternWeight*15*factor);
          }
        }
        for(int c = 0; c<numLeafConds; c++)
        {
          if(leafConds[c].type == Condition::FROZEN)
          {
            scores->add(Decision::IS_FROZEN,loc,0,patternWeight*12*factor);
            break;
          }
        }
      }
    }
  }

  ScoreRet best = scores->getMax();

  int nodeIndex = tree.tree.size();
  hashIdxMap[hash] = nodeIndex;
  tree.tree.push_back(Decision());

  Decision node;
  node.loc = best.loc;
  node.type = best.type;
  node.v1 = best.v1;

  if(node.type == Decision::OWNER)
  {
    ownerKnownArray[node.loc] = SILV;
    node.dest[SILV] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::ownerIs(ERRLOC,SILV));
    ownerKnownArray[node.loc] = GOLD;
    node.dest[GOLD] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::ownerIs(ERRLOC,GOLD));
    ownerKnownArray[node.loc] = NPLA;
    node.dest[NPLA] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::ownerIs(ERRLOC,NPLA));
    ownerKnownArray[node.loc] = -1;
  }
  else
  {
    pla_t pla = ownerKnownArray[node.loc];
    DEBUGASSERT(pla != NPLA);
    if(node.type == Decision::STRCOMPARE)
    {
      node.dest[0] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::lessThanLoc(ERRLOC,node.v1));
      node.dest[1] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::leqThanLoc(ERRLOC,node.v1) && Condition::geqThanLoc(ERRLOC,node.v1));
      node.dest[2] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::greaterThanLoc(ERRLOC,node.v1));
    }
    else if(node.type == Decision::IS_RABBIT)
    {
      node.dest[0] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::notPieceIs(ERRLOC,RAB));
      node.dest[1] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::pieceIs(ERRLOC,RAB));
    }
    else if(node.type == Decision::IS_FROZEN)
    {
      node.dest[0] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::notFrozen(ERRLOC,pla));
      node.dest[1] = filterDownByRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,patterns,node.loc,Condition::frozen(ERRLOC,pla));
    }
    else
    {
      DEBUGASSERT(false);
    }
  }

  tree.tree[nodeIndex] = node;
  return nodeIndex;
}

int DecisionTree::filterDownByRec(
    DecisionTree& tree,
    int valueIfFalse,
    pla_t ownerKnownArray[BSIZE],
    map<Hash128,int>& hashIdxMap,
    ByteBuffer& hashBuffer,
    ScoreBuffer* scores,
    const vector<PatternData>& patterns,
    loc_t loc,
    Condition cond
    )
{
  vector<PatternData> newPatterns;
  int numPatterns = patterns.size();
  for(int i = 0; i < numPatterns; i++)
  {
    const PatternData& pattern = patterns[i];
    PatternData newPattern;
    newPattern.pattern = pattern.pattern.simplifiedGiven(cond,loc);
    if(newPattern.pattern.isFalse(loc) || newPattern.pattern.isFalse())
      continue;
    newPattern.numTrue = newPattern.pattern.countTrue();
    if(newPattern.numTrue >= 64)
    {
      cout << "Terminal true" << endl;
      return (-pattern.valueIfTrue-1);
    }
    newPattern.valueIfTrue = pattern.valueIfTrue;
    newPattern.hash = newPattern.pattern.computeHash(hashBuffer);
    newPatterns.push_back(newPattern);
  }

  if(newPatterns.size() <= 0)
    return (-valueIfFalse-1);

  return compileRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,newPatterns);
}

void DecisionTree::compile(DecisionTree& tree, const vector<pair<Pattern,int> >& patterns, int valueIfFalse)
{
  map<Hash128,int> hashIdxMap;
  pla_t ownerKnownArray[BSIZE];
  for(int i = 0; i<BSIZE; i++)
    ownerKnownArray[i] = -1;
  ScoreBuffer* scores = new ScoreBuffer();

  ByteBuffer hashBuffer(0,true);

  vector<PatternData> newPatterns;
  int numPatterns = patterns.size();
  for(int i = 0; i < numPatterns; i++)
  {
    if(patterns[i].first.isFalse())
      continue;

    int idx = newPatterns.size();
    newPatterns.push_back(PatternData());
    PatternData& pData = newPatterns[idx];
    pData.pattern = patterns[i].first;
    pData.valueIfTrue = patterns[i].second;
    pData.numTrue = patterns[i].first.countTrue();
    pData.hash = patterns[i].first.computeHash(hashBuffer);

    if(pData.numTrue >= 64)
    {
      tree.startDest = (-pData.valueIfTrue-1);
      delete scores;
      return;
    }
  }

  tree.startDest = compileRec(tree,valueIfFalse,ownerKnownArray,hashIdxMap,hashBuffer,scores,newPatterns);

  delete scores;
}






