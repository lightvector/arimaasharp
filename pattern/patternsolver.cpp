/*
 * patternsolver.cpp
 * Author: davidwu
 */

#include <sstream>
#include "../core/global.h"
#include "../board/board.h"
#include "../pattern/patternsolver.h"

//When trying to find goal in N steps for pla a rabbit with this many steps left to goal
//only consider steps into these locations
static Bitmap GOAL_STEP_DEST_MASK[2][BSIZE][5];
//only consider pushpulls whose center is these locatoins
static Bitmap GOAL_PP_DEST_MASK[2][BSIZE][5];

void KB::init()
{
  for(pla_t pla = 0; pla < 2; pla++)
  {
    for(loc_t loc = 0; loc < BSIZE; loc++)
    {
      if(loc & 0x88)
        continue;
      int gYDist = Board::GOALYDIST[pla][loc];
      if(gYDist == 0)
        continue;
      for(int numSteps = 0; numSteps < 5; numSteps++)
      {
        Bitmap map;
        //Barely enough - set only the square in front of the rabbit
        if(gYDist == numSteps)
          map.setOn(loc + Board::GOALLOCINC[pla]);
        else if(gYDist < numSteps)
        {
          int excessSteps = numSteps - gYDist;
          int dy = Board::GOALLOCINC[pla];
          for(int i = 0; i<=gYDist; i++)
            map.setOn(loc + dy * i);

          int expansions = excessSteps * 2 - 1;
          for(int i = 0; i<expansions; i++)
            map = map | Bitmap::adj(map);
        }
        GOAL_STEP_DEST_MASK[pla][loc][numSteps] = map;

        map = Bitmap();
        if(gYDist < numSteps-1)
        {
          int excessSteps = numSteps - gYDist - 1;
          int dy = Board::GOALLOCINC[pla];
          for(int i = 0; i<=gYDist; i++)
            map.setOn(loc + dy * i);

          for(int i = 0; i<excessSteps; i++)
          {
            map = map | (Bitmap::adj(map) & Bitmap::BMPTRAPS);
            map = map | Bitmap::adj(map);
            map = map | Bitmap::adj(map);
          }
        }
        GOAL_PP_DEST_MASK[pla][loc][numSteps] = map;
      }
    }
  }
}


Possibility::Possibility()
:owner(NPLA),isRabbit(0),isSpecialFrozen(0)
{}
Possibility::Possibility(pla_t owner, int isRabbit, int isSpecialFrozen)
:owner(owner),isRabbit(isRabbit),isSpecialFrozen(isSpecialFrozen)
{}

Possibility Possibility::npla() { return Possibility(NPLA,0,0); }
Possibility Possibility::silv() { return Possibility(SILV,2,0); }
Possibility Possibility::gold() { return Possibility(GOLD,2,0); }

//These functions check the special isFrozen flag, but do NOT check actual surrounding strengths for real frozenness

bool Possibility::canDefiniteStep(pla_t pla, loc_t src, loc_t dest) const
{
  return owner == pla && (isRabbit == 0 || Board::isRabOkayStatic(pla,src,dest)) && isSpecialFrozen == 0;
}
bool Possibility::canDefinitePhantomStep(pla_t pla, loc_t src, loc_t dest) const
{
  if(owner == NPLA)
    return true;
  return canDefiniteStep(pla,src,dest);
}

//Does NOT check strength - assumes that nonrabbit pla can push any opp, leaving strength checking elsewhere.
bool Possibility::canDefinitePhantomPush(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const
{
  (void)src;
  (void)dest2;
  if(!(owner == pla && isRabbit == 0 && isSpecialFrozen == 0))
    return false;
  if(destP.owner == NPLA)
    return true;
  if(destP.owner == gOpp(pla))
    return true;
  if(destP.owner == pla && (destP.isRabbit == 0 || Board::isRabOkayStatic(pla,dest,dest2)))
    return true;
  return false;
}

//Does NOT check strength - assumes that nonrabbit pla can pull any opp, leaving strength checking elsewhere.
bool Possibility::canDefinitePhantomPull(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const
{
  (void)dest2;
  if(!(destP.owner == pla && destP.isRabbit == 0 && destP.isSpecialFrozen == 0))
    return false;
  if(owner == NPLA)
    return true;
  if(owner == gOpp(pla))
    return true;
  if(owner == pla && (isRabbit == 0 || Board::isRabOkayStatic(pla,src,dest)))
    return true;
  return false;
}

//TODO the below explanation doesn't match with the actual code - for example we currently
//always perform two steps, even though in one of the cases described below, no steps at all get
//performed due to freezing. We might be making illegal moves on the board.
//TODO might be unnecessary, but possibly needed to express the fact that you
//can still clear a square even if frozen status is unknown.
//Imagine you have to clear A via B and C and the opponent pushed a piece into B, and you
//are unsure if A is npla or pla, so B might be frozen
//If A is npla, you're done, but if A is pla, then B is unfrozen, so you can still clear A
//So even while each individual phantom step might be impossible on it's own, the stepstep might be legal.
bool Possibility::canDefinitePhantomStepStep(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const
{
  return canDefinitePhantomStep(pla,src,dest) && destP.canDefinitePhantomStep(pla,dest,dest2);
}

bool Possibility::maybeCanStep(pla_t pla, loc_t src, loc_t dest) const
{
  return owner == pla && (isRabbit != 1 || Board::isRabOkayStatic(pla,src,dest)) && isSpecialFrozen == 0;
}

//Does NOT check strength - assumes that nonrabbit pla can push any opp, leaving strength checking elsewhere.
bool Possibility::maybeCanPush(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const
{
  (void)src;
  (void)dest;
  (void)dest2;
  return owner == pla && isRabbit != 1 && isSpecialFrozen == 0 && destP.owner == gOpp(pla);
}

//Does NOT check strength - assumes that nonrabbit pla can pull any opp, leaving strength checking elsewhere.
bool Possibility::maybeCanPull(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const
{
  (void)src;
  (void)dest;
  (void)dest2;
  return destP.owner == pla && destP.isRabbit != 1 && destP.isSpecialFrozen == 0 && owner == gOpp(pla);
}

void Possibility::write(ostream& out, const Possibility& p)
{
  static const char* isRabbitChars = "pra";
  out << Board::writePla(p.owner) << isRabbitChars[p.isRabbit];
}

string Possibility::write(const Possibility& p)
{
  ostringstream out;
  write(out,p);
  return out.str();
}

ostream& operator<<(ostream& out, const Possibility& p)
{
  Possibility::write(out,p);
  return out;
}

//PNODE------------------------------------------------------------------------------------

KB::PNode::PNode()
:isHead(false),possibility(),nextNode(0),prevNode(0),strRelations(NULL)
{}
KB::PNode::~PNode()
{
  if(strRelations != NULL)
    delete strRelations;
}
KB::PNode::PNode(const PNode& other)
:isHead(other.isHead),possibility(other.possibility),
 nextNode(other.nextNode),prevNode(other.prevNode)
{
  if(other.strRelations == NULL)
    strRelations = NULL;
  else
    strRelations = new vector<StrRelation>(*other.strRelations);
}

KB::PNode& KB::PNode::operator=(const PNode& other)
{
  if(this == &other)
    return *this;
  isHead = other.isHead;
  possibility = other.possibility;
  nextNode = other.nextNode;
  prevNode = other.prevNode;
  if(strRelations != NULL)
    delete strRelations;

  if(other.strRelations == NULL)
    strRelations = NULL;
  else
    strRelations = new vector<StrRelation>(*other.strRelations);

  return *this;
}

//KB---------------------------------------------------------------------------------------

#define FORALL(node,head) for(int node = next(head); node != head; node = next(node))

KB::~KB()
{}

KB::KB(const Pattern& pattern)
{
  hasFrozenPla = NPLA;

  //Add head node for the unused nodes list
  {
    int newNodeIdx = pNodes.size();
    PNode node;
    node.isHead = true;
    node.nextNode = newNodeIdx;
    node.prevNode = newNodeIdx;
    node.strRelations = NULL;
    pNodes.push_back(node);
    unusedList = newNodeIdx;
  }

  //Add all the heads nodes for locations
  for(loc_t loc = 0; loc<BSIZE; loc++)
  {
    if(loc & 0x88)
      continue;
    int newNodeIdx = pNodes.size();
    PNode node;
    node.isHead = true;
    node.nextNode = newNodeIdx;
    node.prevNode = newNodeIdx;
    node.strRelations = NULL;
    pNodes.push_back(node);
    pHead[loc] = newNodeIdx;
  }

  //Add all the locations to be processed into a list
  vector<loc_t> locsToProcess;
  for(loc_t loc = 0; loc<BSIZE; loc++)
  {
    if(loc & 0x88)
      continue;
    locsToProcess.push_back(loc);
  }

  //Walk through the list of locations. NOTE: the length
  //of this list will change as we walk through it, because sometimes
  //we will need to defer processing of locs
  Condition leafConditions[256]; //buffer
  Bitmap processedLocs;
  for(int k = 0; k<(int)locsToProcess.size(); k++)
  {
    if(k > 64 * 64)
      Global::fatalError("Circular strength dependency in pattern");

    loc_t loc = locsToProcess[k];
    const int* list = pattern.getList(loc);
    int listLen = pattern.getListLen(loc);

    //Check if anything in here depends on an unprocessed loc
    bool dependsOnUnprocessed = false;
    for(int i = 0; i<listLen; i++)
    {
      int conditionIdx = list[i];
      const Condition& cond = pattern.getCondition(conditionIdx);
      if(cond.comparesStrengthAgainstAny(~processedLocs))
      {dependsOnUnprocessed = true;}
    }
    if(dependsOnUnprocessed)
    {
      locsToProcess.push_back(loc);
      continue;
    }

    //Okay, we can process this loc
    processedLocs.setOn(loc);

    //For each condition in the pattern, add it
    for(int listIdx = 0; listIdx<listLen; listIdx++)
    {
      int conditionIdx = list[listIdx];
      const Condition& cond = pattern.getCondition(conditionIdx);
      if(!cond.isConjunction())
        Global::fatalError("KB: Only conjunctions supported for conditions right now");
      int num = cond.getLeafConditions(leafConditions,256);
      DEBUGASSERT(num > 0);
      if(num == 1 && leafConditions[0] == Condition::cFalse())
        continue;
      else if(num == 1 && leafConditions[0] == Condition::cTrue())
      {
        addPossibilityToLoc(Possibility::npla(),loc);
        addPossibilityToLoc(Possibility::silv(),loc);
        addPossibilityToLoc(Possibility::gold(),loc);
      }
      else
      {
        Possibility poss;
        //Extract the owner out of the condition
        pla_t owner = -1;
        for(int i = 0; i<num; i++)
          if(leafConditions[i].type == Condition::OWNER_IS || leafConditions[i].type == Condition::FROZEN)
          {
            if(owner != -1 && owner != leafConditions[i].v1)
              Global::fatalError("KB: Don't know how to parse condition: " + Condition::write(cond));
            if(!leafConditions[i].istrue)
              Global::fatalError("KB: Don't know how to parse condition: " + Condition::write(cond));
            owner = leafConditions[i].v1;
          }
        if(owner == -1)
          Global::fatalError("KB: Don't know how to parse condition: " + Condition::write(cond));
        poss.owner = owner;

        //Extract the rabbitness out of the condition
        int rabbitValue = 2;
        bool foundRabbit = false;
        for(int i = 0; i<num; i++)
          if(leafConditions[i].type == Condition::PIECE_IS)
          {
            if(leafConditions[i].v1 != RAB || foundRabbit || owner == NPLA)
              Global::fatalError("KB: Don't know how to parse condition: " + Condition::write(cond));
            foundRabbit = true;

            if(leafConditions[i].istrue)
              rabbitValue = 1;
            else
              rabbitValue = 0;
          }
        poss.isRabbit = rabbitValue;

        //Extract frozenness
        int frozenValue = 0;
        bool foundFrozen = false;
        for(int i = 0; i<num; i++)
          if(leafConditions[i].type == Condition::FROZEN)
          {
            if(leafConditions[i].v1 != owner || foundFrozen || owner == NPLA)
              Global::fatalError("KB: Don't know how to parse condition: " + Condition::write(cond));
            foundFrozen = true;

            if(leafConditions[i].istrue)
              frozenValue = 1;
            else
              frozenValue = 0;
          }
        poss.isSpecialFrozen = frozenValue;
        if(foundFrozen)
        {
          if(hasFrozenPla == gOpp(owner))
            Global::fatalError("KB: More than one player has frozen condition, can't handle it: " + Pattern::write(pattern));
          hasFrozenPla = owner;
        }

        int node = addPossibilityToLoc(poss,loc);

        //Add strength comparisons
        for(int i = 0; i<num; i++)
        {
          if(leafConditions[i].type == Condition::LESS_THAN_LOC)
          {
            loc_t vsLoc = leafConditions[i].v1;
            if(leafConditions[i].istrue)
              addStrengthRelationVsLoc(node,LT,vsLoc);
            else
              addStrengthRelationVsLoc(node,GEQ,vsLoc);
          }
          else if(leafConditions[i].type == Condition::LEQ_THAN_LOC)
          {
            loc_t vsLoc = leafConditions[i].v1;
            if(leafConditions[i].istrue)
              addStrengthRelationVsLoc(node,LEQ,vsLoc);
            else
              addStrengthRelationVsLoc(node,GT,vsLoc);
          }
        }
      }
    }
  }

  //Remove frozen possibilities everywhere if it's impossible that they're frozen.
  for(loc_t loc = 0; loc<BSIZE; loc++)
  {
    if(loc & 0x88)
      continue;
    if(Board::ISTRAP[loc] || !maybeHasDefender(GOLD,loc) || definitelyHasDefender(SILV,loc))
      removeAllSpecialFrozenPlasAt(SILV,loc);
    if(Board::ISTRAP[loc] || !maybeHasDefender(SILV,loc) || definitelyHasDefender(GOLD,loc))
      removeAllSpecialFrozenPlasAt(GOLD,loc);
  }

  //Remove possibilities on traps if it's impossible that they have a defender.
  for(int i = 0; i<4; i++)
  {
    loc_t loc = Board::TRAPLOCS[i];
    if(!maybeHasDefender(GOLD,loc))
      removeAllPlasAt(GOLD,loc);
    if(!maybeHasDefender(SILV,loc))
      removeAllPlasAt(SILV,loc);
  }
}

void KB::checkListConsistency(int head, const char* message) const
{
  checkListConsistency(head,string(message));
}

void KB::checkListConsistency(int head, const string& message) const
{
  int numPNodes = pNodes.size();
  if(head < 0 || head >= numPNodes)
    Global::fatalError(message + " (head is out of bounds)");

  if(!pNodes[head].isHead)
    Global::fatalError(message + " (list head does not have isHead=true)");
  if(pNodes[pNodes[head].nextNode].prevNode != head)
    Global::fatalError(message + " (head's next node's prev node is not head)");
  if(pNodes[pNodes[head].prevNode].nextNode != head)
    Global::fatalError(message + " (head's prev node's next node is not head)");

  int count = 0;
  FORALL(node,head)
  {
    if(node < 0 || node >= numPNodes)
      Global::fatalError(message + " (node is out of bounds)");
    if(pNodes[node].isHead)
      Global::fatalError(message + " (list node has isHead=true)");
    count++;
    if(count > numPNodes)
      Global::fatalError(message + " (list probably has an infinite loop)");
    if(pNodes[pNodes[node].nextNode].prevNode != node)
      Global::fatalError(message + " (next node's prev node is not node)");
    if(pNodes[pNodes[node].prevNode].nextNode != node)
      Global::fatalError(message + " (prev node's next node is not node)");
  }
}

void KB::checkConsistency(const char* message) const
{
  checkConsistency(string(message));
}

void KB::checkConsistency(const string& message) const
{
  for(int i = 0; i<BSIZE; i++)
  {
    if(i & 0x88)
      continue;
    checkListConsistency(pHead[i],message + " (head for loc " + Board::writeLoc(i) + ")");
  }
  checkListConsistency(unusedList,message + " (unused list)");
  for(int i = 0; i<BSIZE; i++)
  {
    if(i & 0x88)
      continue;
    for(int j = i+1; j<BSIZE; j++)
    {
      if(j & 0x88)
        continue;
      if(pHead[i] == pHead[j])
        Global::fatalError(message + " (lists for " + Board::writeLoc(i) + " and " + Board::writeLoc(j) + " share the same head)");
    }
  }
}

//Get the next node of the list
int KB::next(int node) const
{
  return pNodes[node].nextNode;
}

//Check if this list has at least one elemnt
bool KB::atLeastOne(int head) const
{
  return pNodes[head].nextNode != head;
}

//Add node to the end of the list with the given head
void KB::addNode(int head, int node)
{
  DEBUGASSERT(pNodes[head].isHead && !pNodes[node].isHead)
  int prevNode = pNodes[head].prevNode;
  pNodes[head].prevNode = node;
  pNodes[prevNode].nextNode = node;
  pNodes[node].prevNode = prevNode;
  pNodes[node].nextNode = head;
}

//Remove a node from it's current list (but doesn't add it to the unused list)
//Note that it's okay to use this in a loop and use the removed node's nextNode to step
//to the next element, since we don't modify the node itself.
int KB::removeNode(int node)
{
  DEBUGASSERT(!pNodes[node].isHead)
  int prevNode = pNodes[node].prevNode;
  int nextNode = pNodes[node].nextNode;
  pNodes[nextNode].prevNode = prevNode;
  pNodes[prevNode].nextNode = nextNode;
  return node;
}

//Grab an unused node from the front of the unused list,
//or reallocate to get one if there are none
int KB::getUnusedNode()
{
  int nextUnused = pNodes[unusedList].nextNode;
  if(nextUnused != unusedList)
  {
    DEBUGASSERT(numStrengthRelations(nextUnused) == 0);
    return removeNode(nextUnused);
  }

  int newNodeIdx = pNodes.size();
  PNode node;
  node.isHead = false;
  node.strRelations = NULL;
  pNodes.push_back(node);
  return newNodeIdx;
}

//Swap list contents
void KB::swapLists(int head1, int head2)
{
  int node1 = pNodes[head1].nextNode;
  int node2 = pNodes[head2].nextNode;
  int prevNode1 = pNodes[head1].prevNode;
  int prevNode2 = pNodes[head2].prevNode;

  //Both empty
  if(node1 == head1 && node2 == head2)
    return;
  //Only the first list is empty
  else if(node1 == head1)
  {
    pNodes[head1].nextNode = node2;
    pNodes[head1].prevNode = prevNode2;
    pNodes[head2].nextNode = head2;
    pNodes[head2].prevNode = head2;
    pNodes[node2].prevNode = head1;
    pNodes[prevNode2].nextNode = head1;
  }
  //Only the second list is empty
  else if(node2 == head2)
  {
    pNodes[head2].nextNode = node1;
    pNodes[head2].prevNode = prevNode1;
    pNodes[head1].nextNode = head1;
    pNodes[head1].prevNode = head1;
    pNodes[node1].prevNode = head2;
    pNodes[prevNode1].nextNode = head2;
  }
  //None empty
  else
  {
    pNodes[head1].nextNode = node2;
    pNodes[head2].nextNode = node1;
    pNodes[head1].prevNode = prevNode2;
    pNodes[head2].prevNode = prevNode1;
    pNodes[node1].prevNode = head2;
    pNodes[prevNode1].nextNode = head2;
    pNodes[node2].prevNode = head1;
    pNodes[prevNode2].nextNode = head1;
  }
}

//Move all nodes from this list to the front of the unused list
void KB::clear(int head)
{
  int headNext = pNodes[head].nextNode;
  int headPrev = pNodes[head].prevNode;
  if(headNext == head)
    return;

  //Cycle through all of the nodes to re-relationify them
  FORALL(node,head)
    removeAllStrengthRelations(node);

  int formerNextNode = pNodes[unusedList].nextNode;
  pNodes[unusedList].nextNode = headNext;
  pNodes[formerNextNode].prevNode = headPrev;
  pNodes[headNext].prevNode = unusedList;
  pNodes[headPrev].nextNode = formerNextNode;

  //Tie up the ends of the cleared list
  pNodes[head].nextNode = head;
  pNodes[head].prevNode = head;
}

//Returns the node that was added
int KB::addPossibilityToLoc(const Possibility& p, loc_t loc)
{
  //Currently, for simplicity we don't check if the possibility already exists
  //because it would take time and is probably not a big deal usually
  int node = getUnusedNode();
  pNodes[node].possibility = p;
  addNode(pHead[loc],node);
  return node;
}

int KB::numStrengthRelations(int node) const
{
  return pNodes[node].strRelations == NULL ? 0 : pNodes[node].strRelations->size();
}

//Check if node has a relation to otherNode
bool KB::hasStrengthRelation(int node, int otherNode) const
{
  int numRels = numStrengthRelations(node);
  const vector<StrRelation>& relations = *pNodes[node].strRelations;
  for(int j = 0; j<numRels; j++)
    if(relations[j].node == otherNode)
      return true;
  return false;
}

//Checks strength relations and rabbitness
bool KB::isDefinitelyGt(int node, int otherNode) const
{
  DEBUGASSERT(pNodes[node].possibility.owner != NPLA);
  DEBUGASSERT(pNodes[otherNode].possibility.owner != NPLA);
  if(pNodes[node].possibility.isRabbit != 0)
    return false;
  if(pNodes[otherNode].possibility.isRabbit == 1)
    return true;

  int numRels = numStrengthRelations(node);
  const vector<StrRelation>& relations = *pNodes[node].strRelations;
  for(int j = 0; j<numRels; j++)
    if(relations[j].node == otherNode)
    {
      int cmp = relations[j].cmp;
      if(cmp == GT) return true;
    }
  return false;
}

//Checks strength relations and rabbitness
bool KB::isDefinitelyGeq(int node, int otherNode) const
{
  DEBUGASSERT(pNodes[node].possibility.owner != NPLA);
  DEBUGASSERT(pNodes[otherNode].possibility.owner != NPLA);
  if(pNodes[node].possibility.isRabbit != 0)
    return pNodes[otherNode].possibility.isRabbit == 1;
  if(pNodes[otherNode].possibility.isRabbit == 1)
    return true;

  int numRels = numStrengthRelations(node);
  const vector<StrRelation>& relations = *pNodes[node].strRelations;
  for(int j = 0; j<numRels; j++)
    if(relations[j].node == otherNode)
    {
      int cmp = relations[j].cmp;
      if(cmp == GT || cmp == GEQ) return true;
    }
  return false;
}

bool KB::isMaybeGt(int node, int otherNode) const
{return !isDefinitelyGeq(otherNode,node);}
bool KB::isMaybeGeq(int node, int otherNode) const
{return !isDefinitelyGt(otherNode,node);}


void KB::addStrengthRelationVsLoc(int node, int cmp, loc_t loc)
{
  int otherHead = pHead[loc];
  FORALL(otherNode,otherHead)
  {
    //Make sure we're not already related to the node
    if(hasStrengthRelation(node,otherNode))
      Global::fatalError("Tried to add second strength relation with loc " + Board::writeLoc(loc));
    if(pNodes[node].possibility.owner == NPLA)
      Global::fatalError("Tried to add strength relation from npla vs loc " + Board::writeLoc(loc));
    if(pNodes[otherNode].possibility.owner == NPLA)
      Global::fatalError("Tried to add strength relation from loc vs npla " + Board::writeLoc(loc));

    if(pNodes[node].strRelations == NULL)
      pNodes[node].strRelations = new vector<StrRelation>();
    if(pNodes[otherNode].strRelations == NULL)
      pNodes[otherNode].strRelations = new vector<StrRelation>();

    pNodes[node].strRelations->push_back(StrRelation(otherNode,cmp));
    pNodes[otherNode].strRelations->push_back(StrRelation(node,invertCmp(cmp)));
  }
}
void KB::removeAllStrengthRelations(int node)
{
  int numRels = numStrengthRelations(node);
  const vector<StrRelation>& relations = *pNodes[node].strRelations;
  for(int i = 0; i<numRels; i++)
  {
    const StrRelation& rel = relations[i];
    int otherNode = rel.node;
    int numOtherRels = numStrengthRelations(otherNode);
    DEBUGASSERT(numOtherRels > 0);
    bool foundOtherStrengthRelation = false;
    vector<StrRelation>& otherRelations = *pNodes[otherNode].strRelations;
    for(int j = 0; j<numOtherRels; j++)
    {
      if(otherRelations[j].node == node)
      {
        foundOtherStrengthRelation = true;
        otherRelations.erase(otherRelations.begin()+j);
        break;
      }
    }
    DEBUGASSERT(foundOtherStrengthRelation);
  }
  if(numRels > 0)
    pNodes[node].strRelations->clear();
}

bool KB::definitelyNPla(loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner != NPLA)
      return false;
  return atLeastOne(head);
}

bool KB::maybeNPla(loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == NPLA)
      return true;
  return false;
}

bool KB::maybeOccupied(loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner != NPLA)
      return true;
  return false;
}

bool KB::definitelyRabbit(pla_t pla, loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner != pla || pNodes[node].possibility.isRabbit != 1)
      return false;
  return atLeastOne(head);
}

bool KB::definitelyPla(pla_t pla, loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner != pla)
      return false;
  return atLeastOne(head);
}

bool KB::maybePla(pla_t pla, loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == pla)
      return true;
  return false;
}

bool KB::maybePlaNonRab(pla_t pla, loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == pla && pNodes[node].possibility.isRabbit != 1)
      return true;
  return false;
}

//Check if it's consistent that the piece is an elephant - nonrabbit pla that cannot be frozen
bool KB::maybePlaElephant(pla_t pla, loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == pla && pNodes[node].possibility.isRabbit != 1 && numStrengthRelations(node) == 0)
      return true;
  return false;
}

bool KB::definitelyPlaOrNPla(pla_t pla, loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner != pla && pNodes[node].possibility.owner != NPLA)
      return false;
  return atLeastOne(head);
}

bool KB::definitelyDominates(loc_t loc, int otherNode) const
{
  pla_t otherPla = pNodes[otherNode].possibility.owner;
  DEBUGASSERT(otherPla != NPLA);
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner != gOpp(otherPla) || !isDefinitelyGt(node,otherNode))
      return false;
  return atLeastOne(head);
}

bool KB::maybeDominates(loc_t loc, int otherNode) const
{
  pla_t otherPla = pNodes[otherNode].possibility.owner;
  DEBUGASSERT(otherPla != NPLA);
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == gOpp(otherPla) && isMaybeGt(node,otherNode))
      return true;
  return false;
}

bool KB::anySpecialFrozen(pla_t pla, loc_t loc) const
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == pla && pNodes[node].possibility.isSpecialFrozen == 1)
      return true;
  return false;
}

bool KB::definitelyHasDefender(pla_t pla, loc_t loc) const
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    if(definitelyPla(pla,loc+Board::ADJOFFSETS[i]))
      return true;
  }
  return false;
}

bool KB::maybeHasDefender(pla_t pla, loc_t loc) const
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    if(maybePla(pla,loc+Board::ADJOFFSETS[i]))
      return true;
  }
  return false;
}

bool KB::definitelyHasDominator(loc_t loc, int node) const
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    if(definitelyDominates(loc+Board::ADJOFFSETS[i],node))
      return true;
  }
  return false;
}

bool KB::maybeHasDominator(loc_t loc, int node) const
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    if(maybeDominates(loc+Board::ADJOFFSETS[i],node))
      return true;
  }
  return false;
}

bool KB::definitelyUnfrozen(loc_t loc, int node) const
{
  if(Board::ISTRAP[loc])
    return true;
  pla_t pla = pNodes[node].possibility.owner;
  DEBUGASSERT(pla != NPLA);
  if(definitelyHasDefender(pla,loc))
    return true;
  if(!maybeHasDominator(loc,node))
    return true;
  return false;
}

bool KB::maybeUnfrozen(loc_t loc, int node) const
{
  if(Board::ISTRAP[loc])
    return true;
  pla_t pla = pNodes[node].possibility.owner;
  DEBUGASSERT(pla != NPLA);
  if(maybeHasDefender(pla,loc))
    return true;
  if(!definitelyHasDominator(loc,node))
    return true;
  return false;
}

bool KB::anySpecialFrozenAround(pla_t pla, loc_t loc) const
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    if(anySpecialFrozen(pla,loc+Board::ADJOFFSETS[i]))
      return true;
  }
  return false;
}

//Try to check if the possibilties for pla pieces at loc are as general as than those at loc2
//(If so, then maybeStepping from loc2 to loc won't add any new possibilities to loc)
bool KB::plaPiecesAtAreAsGeneralAs(pla_t pla, loc_t loc, loc_t loc2) const
{
  int head = pHead[loc];
  int head2 = pHead[loc2];
  FORALL(node2,head2)
  {
    const Possibility& p2 = pNodes[node2].possibility;
    if(p2.owner != pla)
      continue;
    bool existsAsGeneral = false;
    FORALL(node,head)
    {
      const Possibility& p = pNodes[node].possibility;
      if(p.owner != pla)
        continue;
      //For simplicity, we say that it can't be more general if it has strength
      //relations binding it.
      if(numStrengthRelations(node) > 0)
        continue;
      bool rabGeneral = p.isRabbit == 2 || p.isRabbit == p2.isRabbit;
      bool specialFrozenGeneral = p.isSpecialFrozen == 0;
      if(rabGeneral && specialFrozenGeneral)
      {existsAsGeneral = true; break;}
    }
    if(!existsAsGeneral)
      return false;
  }
  return true;
}

//Check if tryDefiniteStep would succeed
bool KB::canDefiniteStep(pla_t pla, loc_t src, loc_t dest) const
{
  if(!definitelyNPla(dest))
    return false;

  int srcHead = pHead[src];
  FORALL(node,srcHead)
    if(!pNodes[node].possibility.canDefiniteStep(pla,src,dest) || !definitelyUnfrozen(src,node))
      return false;
  return atLeastOne(srcHead);
}

//Remove all possibilities at loc with owner pla
//Returns true if any were removed
bool KB::removeAllPlasAt(pla_t pla, loc_t loc)
{
  int head = pHead[loc];

  int numRemoved = 0;
  int removed[256];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == pla)
    {
      DEBUGASSERT(numRemoved < 256);
      removed[numRemoved++] = removeNode(node);
      removeAllStrengthRelations(node);
    }

  for(int i = 0; i<numRemoved; i++)
    addNode(unusedList,removed[i]);
  return numRemoved > 0;
}

//Remove all possibilities at loc with owner pla that are specialfrozen
//Returns true if any were removed
bool KB::removeAllSpecialFrozenPlasAt(pla_t pla, loc_t loc)
{
  int head = pHead[loc];

  int numRemoved = 0;
  int removed[256];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == pla && pNodes[node].possibility.isSpecialFrozen == 1)
    {
      DEBUGASSERT(numRemoved < 256);
      removed[numRemoved++] = removeNode(node);
      removeAllStrengthRelations(node);
    }

  for(int i = 0; i<numRemoved; i++)
    addNode(unusedList,removed[i]);
  return numRemoved > 0;
}

void KB::removeAllSpecialFrozenPlasAround(pla_t pla, loc_t loc)
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    removeAllSpecialFrozenPlasAt(pla,loc+Board::ADJOFFSETS[i]);
  }
}


//Check if any traps around the given location have a definite or potential
//capture of a pla piece, and update the possibilities accordingly
void KB::resolveTrapCapturesAround(pla_t pla, loc_t loc)
{
  loc_t trapLoc = Board::ADJACENTTRAP[loc];
  if(trapLoc != ERRLOC)
  {
    if(!maybeHasDefender(pla,trapLoc))
    {
      bool anyNonRab = maybePlaNonRab(pla,trapLoc);
      bool anyRemoved = removeAllPlasAt(pla,trapLoc);
      if(anyRemoved && !maybeNPla(trapLoc)) addPossibilityToLoc(Possibility::npla(),trapLoc);
      if(anyRemoved && anyNonRab) unSpecialFreezeAround(gOpp(pla),trapLoc);
    }
    else if(!definitelyHasDefender(pla,trapLoc))
    {
      bool anyPlas = maybePla(pla,trapLoc);
      bool anyNonRab = maybePlaNonRab(pla,trapLoc);
      if(anyPlas && !maybeNPla(trapLoc)) addPossibilityToLoc(Possibility::npla(),trapLoc);
      if(anyNonRab) unSpecialFreezeAround(gOpp(pla),trapLoc);
    }
  }
}

//Remove the isSpecialFrozen flag for all plas at the loc
void KB::unSpecialFreeze(pla_t pla, loc_t loc)
{
  int head = pHead[loc];
  FORALL(node,head)
    if(pNodes[node].possibility.owner == pla)
      pNodes[node].possibility.isSpecialFrozen = 0;
}

//Remove the isSpecialFrozen flag for all plas around
void KB::unSpecialFreezeAround(pla_t pla, loc_t loc)
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    loc_t adj = loc + Board::ADJOFFSETS[i];
    unSpecialFreeze(pla,adj);
  }
}

//Attempt to make a step of pla from src to dest
bool KB::tryDefiniteStep(pla_t pla, loc_t src, loc_t dest)
{
  if(hasFrozenPla == pla)
    Global::fatalError("Tried to call tryDefiniteStep with player who hasFrozenPla");
  if(!definitelyNPla(dest))
    return false;

  int srcHead = pHead[src];
  FORALL(node,srcHead)
    if(!pNodes[node].possibility.canDefiniteStep(pla,src,dest) || !definitelyUnfrozen(src,node))
      return false;
  if(!atLeastOne(srcHead))
    return false;

  bool anyNonRab = maybePlaNonRab(pla,src);

  swapLists(srcHead,pHead[dest]);
  resolveTrapCapturesAround(pla,src);
  if(anyNonRab) unSpecialFreezeAround(gOpp(pla),src);
  return true;
}

//Attempt to make a step of pla from src to dest,
//allowing src to be possibly npla, in which case dest ends up possibly npla too.
bool KB::tryDefinitePhantomStep(pla_t pla, loc_t src, loc_t dest)
{
  if(hasFrozenPla == pla)
    Global::fatalError("Tried to call tryDefinitePhantomStep with player who hasFrozenPla");
  if(!definitelyNPla(dest))
    return false;

  int srcHead = pHead[src];
  FORALL(node,srcHead)
    if(!pNodes[node].possibility.canDefinitePhantomStep(pla,src,dest)
        || (pNodes[node].possibility.owner != NPLA && !definitelyUnfrozen(src,node)))
      return false;
  if(!atLeastOne(srcHead))
    return false;

  bool anyNonRab = maybePlaNonRab(pla,src);

  swapLists(srcHead,pHead[dest]);
  resolveTrapCapturesAround(pla,src);
  if(anyNonRab) unSpecialFreezeAround(gOpp(pla),src);
  return true;
}

//Attempt to make a push of pla from src to dest to dest2, allowing for the pushed
//location to be either a pushable opp or possibly npla, or even a pla!
//But the pusher must be a nonrabbit pla.
bool KB::tryDefinitePhantomPush(pla_t pla, loc_t src, loc_t dest, loc_t dest2)
{
  if(hasFrozenPla == pla)
    Global::fatalError("Tried to call tryDefinitePhantomPush with player who hasFrozenPla");
  if(!definitelyNPla(dest2))
    return false;

  int srcHead = pHead[src];
  int destHead = pHead[dest];
  FORALL(srcNode,srcHead)
    FORALL(destNode,destHead)
    {
      if(!pNodes[srcNode].possibility.canDefinitePhantomPush(pNodes[destNode].possibility,pla,src,dest,dest2))
        return false;
      if(pNodes[destNode].possibility.owner == gOpp(pla) && !isDefinitelyGt(srcNode,destNode))
        return false;
      if(!definitelyUnfrozen(src,srcNode))
        return false;
    }
  if(!atLeastOne(srcHead) || !atLeastOne(destHead))
    return false;

  swapLists(destHead,pHead[dest2]);
  swapLists(srcHead,destHead);
  resolveTrapCapturesAround(pla,src);
  resolveTrapCapturesAround(gOpp(pla),dest);
  unSpecialFreezeAround(gOpp(pla),src);
  unSpecialFreeze(gOpp(pla),dest2);
  return true;
}

//Attempt to make a push of pla from src to dest to dest2, allowing for the pulled
//location to be either a pullable opp or possibly npla, or even a pla!
//But the puller must be a nonrabbit pla.
bool KB::tryDefinitePhantomPull(pla_t pla, loc_t src, loc_t dest, loc_t dest2)
{
  if(hasFrozenPla == pla)
    Global::fatalError("Tried to call tryDefinitePhantomPull with player who hasFrozenPla");
  if(!definitelyNPla(dest2))
    return false;

  int srcHead = pHead[src];
  int destHead = pHead[dest];
  FORALL(srcNode,srcHead)
    FORALL(destNode,destHead)
    {
      if(!pNodes[srcNode].possibility.canDefinitePhantomPull(pNodes[destNode].possibility,pla,src,dest,dest2))
        return false;
      if(pNodes[srcNode].possibility.owner == gOpp(pla) && !isDefinitelyGt(destNode,srcNode))
        return false;
      if(!definitelyUnfrozen(dest,destNode))
        return false;
    }
  if(!atLeastOne(srcHead) || !atLeastOne(destHead))
    return false;

  swapLists(destHead,pHead[dest2]);
  swapLists(srcHead,destHead);
  resolveTrapCapturesAround(gOpp(pla),src);
  resolveTrapCapturesAround(pla,dest);
  unSpecialFreezeAround(gOpp(pla),dest);
  unSpecialFreeze(gOpp(pla),dest);
  return true;
}

//Attempt to step two plas from src to dest to dest2, allowing for either location
//to be pla or npla.
//TODO disabled for the momement - see other todo in this file, once freezing comes around, either
//this is redundant or it's possibly broken, depending on how it's implementation is updated.
//Also you need to consider trap captures in between the two steps, as well as wouldbeUFing. Ugh.
/*
bool KB::tryDefinitePhantomStepStep(pla_t pla, loc_t src, loc_t dest, loc_t dest2)
{
  if(!definitelyNPla(dest2))
    return false;

  int srcHead = pHead[src];
  int destHead = pHead[dest];
  FORALL(srcNode,srcHead)
    FORALL(destNode,destHead)
      if(!pNodes[srcNode].possibility.canDefinitePhantomStepStep(pNodes[destNode].possibility,pla,src,dest,dest2))
        return false;
  if(!atLeastOne(srcHead) || !atLeastOne(destHead))
    return false;

  swapLists(destHead,pHead[dest2]);
  swapLists(srcHead,destHead);
  resolveTrapCapturesAround(pla,src);
  resolveTrapCapturesAround(pla,dest);
  return true;
}*/

//TODO if maybe-stepping a piece that can only be a rabbit, and
//there are definitely no defenders, then no surrounding piece can be a opp piece,
//so modify the kb to reflect this. This reduces the number of possibilities and
//so can allow detection of legitimate goals.

//Similarly, if maybe-stepping a piece that definitely is dominated, but is maybe
//guarded by only one square, modify the kb to reflect that it is definitely guarded by that
//square. And additionally, this means you can filter specialfrozen adjacent to that square.

bool KB::tryMaybeStep(pla_t pla, loc_t src, loc_t dest)
{
  if(!maybeNPla(dest))
    return false;

  int srcHead = pHead[src];
  int destHead = pHead[dest];

  int numCanMaybeStepNodes = 0;
  int canMaybeStepNodes[256];
  FORALL(node,srcHead)
    if(pNodes[node].possibility.maybeCanStep(pla,src,dest) && maybeUnfrozen(src,node))
      canMaybeStepNodes[numCanMaybeStepNodes++] = removeNode(node);

  if(numCanMaybeStepNodes == 0)
    return false;

  clear(srcHead);
  clear(destHead);
  addPossibilityToLoc(Possibility::npla(),src);
  for(int i = 0; i<numCanMaybeStepNodes; i++)
    addNode(destHead,canMaybeStepNodes[i]);

  resolveTrapCapturesAround(pla,src);
  removeAllSpecialFrozenPlasAround(pla,src);
  unSpecialFreezeAround(pla,dest);
  return true;
}

bool KB::tryMaybePush(pla_t pla, loc_t src, loc_t dest, loc_t dest2)
{
  if(!maybeNPla(dest2))
    return false;

  int srcHead = pHead[src];
  int destHead = pHead[dest];
  int dest2Head = pHead[dest2];

  int numCanMaybeSrcNodes = 0;
  int canMaybeSrcNodes[256];
  int numCanMaybeDestNodes = 0;
  int canMaybeDestNodes[256];

  //MINOR optimize this
  FORALL(srcNode,srcHead)
  {
    bool foundAnyPush = false;
    FORALL(destNode,destHead)
      if(pNodes[srcNode].possibility.maybeCanPush(pNodes[destNode].possibility,pla,src,dest,dest2) &&
          isMaybeGt(srcNode,destNode) && maybeUnfrozen(src,srcNode))
      {foundAnyPush = true; break;}
    if(foundAnyPush)
    {
      DEBUGASSERT(numCanMaybeSrcNodes < 256);
      canMaybeSrcNodes[numCanMaybeSrcNodes++] = srcNode;
    }
  }
  FORALL(destNode,destHead)
  {
    bool foundAnyPush = false;
    FORALL(srcNode,srcHead)
      if(pNodes[srcNode].possibility.maybeCanPush(pNodes[destNode].possibility,pla,src,dest,dest2) &&
          isMaybeGt(srcNode,destNode) && maybeUnfrozen(src,srcNode))
      {foundAnyPush = true; break;}
    if(foundAnyPush)
    {
      DEBUGASSERT(numCanMaybeDestNodes < 256);
      canMaybeDestNodes[numCanMaybeDestNodes++] = destNode;
    }
  }

  if(numCanMaybeSrcNodes == 0 || numCanMaybeDestNodes == 0)
    return false;

  for(int i = 0; i<numCanMaybeSrcNodes; i++)
    removeNode(canMaybeSrcNodes[i]);
  for(int i = 0; i<numCanMaybeDestNodes; i++)
    removeNode(canMaybeDestNodes[i]);

  clear(srcHead);
  clear(destHead);
  clear(dest2Head);

  addPossibilityToLoc(Possibility::npla(),src);
  for(int i = 0; i<numCanMaybeSrcNodes; i++)
    addNode(destHead,canMaybeSrcNodes[i]);
  for(int i = 0; i<numCanMaybeDestNodes; i++)
    addNode(dest2Head,canMaybeDestNodes[i]);

  resolveTrapCapturesAround(pla,src);
  resolveTrapCapturesAround(gOpp(pla),dest);
  removeAllSpecialFrozenPlasAround(pla,src);
  unSpecialFreezeAround(pla,dest);
  return true;
}

bool KB::tryMaybePull(pla_t pla, loc_t src, loc_t dest, loc_t dest2)
{
  if(!maybeNPla(dest2))
    return false;

  int srcHead = pHead[src];
  int destHead = pHead[dest];
  int dest2Head = pHead[dest2];

  int numCanMaybeSrcNodes = 0;
  int canMaybeSrcNodes[256];
  int numCanMaybeDestNodes = 0;
  int canMaybeDestNodes[256];

  //MINOR optimize this
  FORALL(srcNode,srcHead)
  {
    bool foundAnyPull = false;
    FORALL(destNode,destHead)
      if(pNodes[srcNode].possibility.maybeCanPull(pNodes[destNode].possibility,pla,src,dest,dest2) &&
          isMaybeGt(destNode,srcNode) && maybeUnfrozen(dest,destNode))
      {foundAnyPull = true; break;}
    if(foundAnyPull)
    {
      DEBUGASSERT(numCanMaybeSrcNodes < 256);
      canMaybeSrcNodes[numCanMaybeSrcNodes++] = srcNode;
    }
  }
  FORALL(destNode,destHead)
  {
    bool foundAnyPull = false;
    FORALL(srcNode,srcHead)
      if(pNodes[srcNode].possibility.maybeCanPull(pNodes[destNode].possibility,pla,src,dest,dest2) &&
          isMaybeGt(destNode,srcNode) && maybeUnfrozen(dest,destNode))
      {foundAnyPull = true; break;}
    if(foundAnyPull)
    {
      DEBUGASSERT(numCanMaybeDestNodes < 256);
      canMaybeDestNodes[numCanMaybeDestNodes++] = destNode;
    }
  }

  if(numCanMaybeSrcNodes == 0 || numCanMaybeDestNodes == 0)
    return false;

  bool anyNonRab = maybePlaNonRab(gOpp(pla),src);

  for(int i = 0; i<numCanMaybeSrcNodes; i++)
    removeNode(canMaybeSrcNodes[i]);
  for(int i = 0; i<numCanMaybeDestNodes; i++)
    removeNode(canMaybeDestNodes[i]);

  clear(srcHead);
  clear(destHead);
  clear(dest2Head);

  addPossibilityToLoc(Possibility::npla(),src);
  for(int i = 0; i<numCanMaybeSrcNodes; i++)
    addNode(destHead,canMaybeSrcNodes[i]);
  for(int i = 0; i<numCanMaybeDestNodes; i++)
    addNode(dest2Head,canMaybeDestNodes[i]);

  resolveTrapCapturesAround(gOpp(pla),src);
  resolveTrapCapturesAround(pla,dest);
  removeAllSpecialFrozenPlasAround(pla,dest);
  unSpecialFreezeAround(pla,dest2);
  if(anyNonRab) unSpecialFreezeAround(pla,src);
  return true;
}

//OUTPUT_-----------------------------------------------------------------------------------
void KB::write(ostream& out) const
{
  for(int y = 7; y >= 0; y--)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      int head = pHead[loc];
      FORALL(node,head)
        out << pNodes[node].possibility << " ";
      out << ",";
    }
    out << endl;
  }
}
void KB::write(ostream& out, const KB& kb)
{
  kb.write(out);
}

string KB::write(const KB& p)
{
  ostringstream out;
  write(out,p);
  return out.str();
}

ostream& operator<<(ostream& out, const KB& p)
{
  KB::write(out,p);
  return out;
}

//SOLVE-------------------------------------------------------------------------------------

static void printDots(int numDots)
{
  for(int i = 0; i<numDots; i++)
  {
    cout << ".";
    if(i%4 == 3)
      cout << " ";
  }
}

namespace {
struct CanDefinitelyGoalInfo
{
  Bitmap definitelyPlaRab;
  Bitmap definitelyPla;
  Bitmap phantomPla;
  Bitmap definitelyNPla;

  static CanDefinitelyGoalInfo create(const KB& kb, pla_t pla)
  {
    CanDefinitelyGoalInfo info;
    for(int loc = 0; loc<BSIZE; loc++)
    {
      if(loc & 0x88)
        continue;
      if(kb.definitelyRabbit(pla,loc)) info.definitelyPlaRab.setOn(loc);
      if(kb.definitelyPla(pla,loc)) info.definitelyPla.setOn(loc);
      if(kb.definitelyPlaOrNPla(pla,loc) && kb.maybePla(pla,loc)) info.phantomPla.setOn(loc);
      if(kb.definitelyNPla(loc)) info.definitelyNPla.setOn(loc);
    }
    return info;
  }

  //For steps and phantom steps
  static CanDefinitelyGoalInfo update(CanDefinitelyGoalInfo info, const KB& kb, pla_t pla, loc_t src, loc_t dest)
  {
    (void)dest;
    Bitmap recalcLocs = Board::DISK[1][src];
    info.definitelyPlaRab &= ~recalcLocs;
    info.definitelyPla &= ~recalcLocs;
    info.phantomPla &= ~recalcLocs;
    info.definitelyNPla &= ~recalcLocs;
    while(recalcLocs.hasBits())
    {
      loc_t loc = recalcLocs.nextBit();
      if(kb.definitelyRabbit(pla,loc)) info.definitelyPlaRab.setOn(loc);
      if(kb.definitelyPla(pla,loc))    info.definitelyPla.setOn(loc);
      if(kb.definitelyPlaOrNPla(pla,loc) && kb.maybePla(pla,loc)) info.phantomPla.setOn(loc);
      if(kb.definitelyNPla(loc))       info.definitelyNPla.setOn(loc);
    }
    return info;
  }

  //For pushpulls and phantom stepsteps
  static CanDefinitelyGoalInfo update(CanDefinitelyGoalInfo info, const KB& kb, pla_t pla, loc_t src, loc_t dest, loc_t dest2)
  {
    (void)dest2;
    Bitmap recalcLocs = Board::DISK[1][src] | Board::DISK[1][dest];
    info.definitelyPlaRab &= ~recalcLocs;
    info.definitelyPla &= ~recalcLocs;
    info.phantomPla &= ~recalcLocs;
    info.definitelyNPla &= ~recalcLocs;
    while(recalcLocs.hasBits())
    {
      loc_t loc = recalcLocs.nextBit();
      if(kb.definitelyRabbit(pla,loc)) info.definitelyPlaRab.setOn(loc);
      if(kb.definitelyPla(pla,loc))    info.definitelyPla.setOn(loc);
      if(kb.definitelyPlaOrNPla(pla,loc) && kb.maybePla(pla,loc)) info.phantomPla.setOn(loc);
      if(kb.definitelyNPla(loc))       info.definitelyNPla.setOn(loc);
    }
    return info;
  }
};
}

//Verbosity
//<= 0: nothing
//1: goaling move
//2: whole tree
//goalSteps is indexed by numStepsLeft at each level, may contain ERRSTEP for fillers,
//valid indicies are from 0 to numStepsLeft IF goal is found
static bool canDefinitelyGoalRec(const KB& kb, pla_t pla, int numStepsLeft,
    CanDefinitelyGoalInfo info, step_t goalSteps[5], int verbosity)
{
  int goalY = Board::GOALY[pla];
  if((Bitmap::BMPY[goalY] & info.definitelyPlaRab).hasBits())
  {
    for(int i = 0; i<=numStepsLeft; i++)
      goalSteps[i] = ERRSTEP;
    return true;
  }
  if(numStepsLeft <= 0)
    return false;

  //cout << kb << endl;

  //TODO use a tighter cutoff than numstepsleft - freezing and presence of a piece in front
  //Find a way not to make stupid steps when the rabbit is at exact distance and really should
  //only check if charging is possible. (Currently other pieces will move in front of it
  //because all the squares in front are relevant square to move in to)

  int gY = Board::GOALYINC[pla];
  if((Bitmap::BMPY[goalY-gY] & info.definitelyPlaRab).hasBits())
  {
    int y = goalY-gY;
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(!info.definitelyPlaRab.isOne(loc))
        continue;
      loc_t goalLoc = gLoc(x,goalY);
      if(kb.canDefiniteStep(pla,loc,goalLoc))
      {if(verbosity > 0) {printDots(8-numStepsLeft);
      cout << "Goal step " << Board::writeStep(gStepSrcDest(loc,goalLoc)) << endl;}
      for(int i = 0; i<numStepsLeft; i++)
        goalSteps[i] = ERRSTEP;
      goalSteps[numStepsLeft] = gStepSrcDest(loc,goalLoc);
      return true;}
    }
  }
  if(numStepsLeft <= 1)
    return false;

  KB kb2 = kb;

  Bitmap stepRelevantNPla;
  Bitmap map = info.definitelyPlaRab;
  while(map.hasBits())
  {
    loc_t loc = map.nextBit();
    if(!info.definitelyNPla.isOne(loc + Board::GOALLOCINC[pla]) && Board::GOALYDIST[pla][loc] >= numStepsLeft)
      continue;
    stepRelevantNPla |= GOAL_STEP_DEST_MASK[pla][loc][numStepsLeft];
  }
  stepRelevantNPla &= info.definitelyNPla;

  //Regular steps first
  map = info.definitelyPla & Bitmap::adj(stepRelevantNPla);
  while(map.hasBits())
  {
    loc_t src = map.nextBit();
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][src])
        continue;
      loc_t dest = src + Board::ADJOFFSETS[i];
      if(!stepRelevantNPla.isOne(dest))
        continue;
      if(!kb2.tryDefiniteStep(pla,src,dest))
        continue;

      if(verbosity > 1) {printDots(8-numStepsLeft);
        cout << "Try step " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
      bool canGoal =
          canDefinitelyGoalRec(kb2,pla,numStepsLeft-1,
              CanDefinitelyGoalInfo::update(info,kb2,pla,src,dest),
              goalSteps,verbosity);
      if(canGoal)
      {if(verbosity > 0) {printDots(8-numStepsLeft);
      cout << "Goal step " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
      goalSteps[numStepsLeft] = gStepSrcDest(src,dest);
      return true;}
      kb2 = kb;
    }
  }

  //Phantom steps next
  map = (info.phantomPla & ~info.definitelyPla) & Bitmap::adj(stepRelevantNPla);
  while(map.hasBits())
  {
    loc_t src = map.nextBit();
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][src])
        continue;
      loc_t dest = src + Board::ADJOFFSETS[i];
      if(!stepRelevantNPla.isOne(dest))
        continue;
      if(!kb2.tryDefinitePhantomStep(pla,src,dest))
        continue;
      if(verbosity > 1) {printDots(8-numStepsLeft);
        cout << "Try phant " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
      bool canGoal =
          canDefinitelyGoalRec(kb2,pla,numStepsLeft-1,
              CanDefinitelyGoalInfo::update(info,kb2,pla,src,dest),
              goalSteps,verbosity);
      if(canGoal)
      {if(verbosity > 0) {printDots(8-numStepsLeft);
      cout << "Goal phant " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
      goalSteps[numStepsLeft] = gStepSrcDest(src,dest);
      return true;}
      kb2 = kb;
    }
  }

  Bitmap ppDestRelevant;
  map = info.definitelyPlaRab;
  while(map.hasBits())
    ppDestRelevant |= GOAL_PP_DEST_MASK[pla][map.nextBit()][numStepsLeft];
  ppDestRelevant &= Bitmap::adj(info.definitelyNPla);

  //Pulls next - pulling requires using a player nonrab piece
  map = ppDestRelevant & info.definitelyPla & ~info.definitelyPlaRab;
  while(map.hasBits())
  {
    loc_t dest = map.nextBit();
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][dest])
        continue;
      loc_t src = dest + Board::ADJOFFSETS[i];
      //Don't try pulling if we know the square is empty
      if(!kb2.maybeOccupied(src))
        continue;
      //Don't try pulling if we know for sure the square is a player piece
      if(info.definitelyPla.isOne(src))
        continue;
      for(int j = 0; j<4; j++)
      {
        if(i == j || !Board::ADJOKAY[j][dest])
          continue;
        loc_t dest2 = dest + Board::ADJOFFSETS[j];
        //Final destination must be empty
        if(!info.definitelyNPla.isOne(dest2))
          continue;

        if(!kb2.tryDefinitePhantomPull(pla,src,dest,dest2))
          continue;
        if(verbosity > 1) {printDots(8-numStepsLeft);
        cout << "Try pull " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        bool canGoal =
            canDefinitelyGoalRec(kb2,pla,numStepsLeft-2,
                CanDefinitelyGoalInfo::update(info,kb2,pla,src,dest,dest2),
                goalSteps,verbosity);
        if(canGoal)
        {if(verbosity > 0) {printDots(8-numStepsLeft);
        cout << "Goal pull " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        goalSteps[numStepsLeft] = gStepSrcDest(dest,dest2);
        goalSteps[numStepsLeft-1] = gStepSrcDest(src,dest);
        return true;}
        kb2 = kb;
      }
    }
  }

  //Pushes next - requires that a player nonrabbit piece is adjacent to do the pushing
  map = ppDestRelevant & Bitmap::adj(info.definitelyPla & ~info.definitelyPlaRab);
  while(map.hasBits())
  {
    loc_t dest = map.nextBit();
    //Don't try pushing if we know the square is empty
    if(!kb2.maybeOccupied(dest))
      continue;
    //Don't try pushing if we know the square is a player piece
    if(info.definitelyPla.isOne(dest))
      continue;
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][dest])
        continue;
      loc_t src = dest + Board::ADJOFFSETS[i];
      //Need a player piece that's not a rabbit to do the pushing
      if(!info.definitelyPla.isOne(src) || info.definitelyPlaRab.isOne(src))
        continue;
      for(int j = 0; j<4; j++)
      {
        if(i == j || !Board::ADJOKAY[j][dest])
          continue;
        loc_t dest2 = dest + Board::ADJOFFSETS[j];
        //Final destination must be empty
        if(!info.definitelyNPla.isOne(dest2))
          continue;

        if(!kb2.tryDefinitePhantomPush(pla,src,dest,dest2))
          continue;
        if(verbosity > 1) {printDots(8-numStepsLeft);
        cout << "Try push " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        bool canGoal =
            canDefinitelyGoalRec(kb2,pla,numStepsLeft-2,
                CanDefinitelyGoalInfo::update(info,kb2,pla,src,dest,dest2),
                goalSteps,verbosity);
        if(canGoal)
        {if(verbosity > 0) {printDots(8-numStepsLeft);
        cout << "Goal push " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        goalSteps[numStepsLeft] = gStepSrcDest(dest,dest2);
        goalSteps[numStepsLeft-1] = gStepSrcDest(src,dest);
        return true;}
        kb2 = kb;
      }
    }
  }

  //TODO disabled, pending the earlier todo about this stuff
  /*
  //Phantom step step next
  map = ppDestRelevant & Bitmap::adj(info.phantomPla);
  while(map.hasBits())
  {
    loc_t dest = map.nextBit();
    if(!info.phantomPla.isOne(dest))
      continue;
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][dest])
        continue;
      loc_t src = dest + Board::ADJOFFSETS[i];
      if(!info.phantomPla.isOne(src))
        continue;
      for(int j = 0; j<4; j++)
      {
        if(i == j || !Board::ADJOKAY[j][dest])
          continue;
        loc_t dest2 = dest + Board::ADJOFFSETS[j];
        if(!info.definitelyNPla.isOne(dest2))
          continue;

        if(!kb2.tryDefinitePhantomStepStep(pla,src,dest,dest2))
          continue;
        if(verbosity > 1) {printDots(8-numStepsLeft);
        cout << "Try pss " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        bool canGoal =
            canDefinitelyGoalRec(kb2,pla,numStepsLeft-2,
                CanDefinitelyGoalInfo::update(info,kb2,pla,src,dest,dest2),
                goalSteps,verbosity);
        if(canGoal)
        {if(verbosity > 0) {printDots(8-numStepsLeft);
        cout << "Goal pss " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        goalSteps[numStepsLeft] = gStepSrcDest(dest,dest2);
        goalSteps[numStepsLeft-1] = gStepSrcDest(src,dest);
        return true;}
        kb2 = kb;
      }
    }
  }*/

  return false;
}


bool KB::canDefinitelyGoal(const KB& kb, pla_t pla, int numStepsLeft, int verbosity)
{
  if(kb.hasFrozenPla == pla)
    Global::fatalError("Tried to call canDefinitelyGoal with player who hasFrozenPla");

  step_t goalSteps[5];
  return canDefinitelyGoalRec(kb,pla,numStepsLeft,CanDefinitelyGoalInfo::create(kb,pla),goalSteps,verbosity);
}

namespace {
struct CanMaybeStopGoalInfo
{
  Bitmap definitelyOppRab;
  Bitmap maybePla;
  Bitmap maybePlaNonRab;
  Bitmap maybeNPla;
  Bitmap maybeOpp;
  static CanMaybeStopGoalInfo create(const KB& kb, pla_t pla)
  {
    pla_t opp = gOpp(pla);
    CanMaybeStopGoalInfo info;
    for(int loc = 0; loc<BSIZE; loc++)
    {
      if(loc & 0x88)
        continue;
      if(kb.definitelyRabbit(opp,loc)) info.definitelyOppRab.setOn(loc);
      if(kb.maybePla(pla,loc)) info.maybePla.setOn(loc);
      if(kb.maybePlaNonRab(pla,loc)) info.maybePlaNonRab.setOn(loc);
      if(kb.maybeNPla(loc))    info.maybeNPla.setOn(loc);
      if(kb.maybePla(opp,loc)) info.maybeOpp.setOn(loc);
    }
    return info;
  }

  //For steps
  static CanMaybeStopGoalInfo update(CanMaybeStopGoalInfo info, const KB& kb, pla_t pla, loc_t src, loc_t dest)
  {
    (void)dest;
    pla_t opp = gOpp(pla);
    Bitmap recalcLocs = Board::DISK[1][src];
    info.definitelyOppRab &= ~recalcLocs;
    info.maybePla &= ~recalcLocs;
    info.maybePlaNonRab &= ~recalcLocs;
    info.maybeNPla &= ~recalcLocs;
    info.maybeOpp &= ~recalcLocs;
    while(recalcLocs.hasBits())
    {
      loc_t loc = recalcLocs.nextBit();
      if(kb.definitelyRabbit(opp,loc)) info.definitelyOppRab.setOn(loc);
      if(kb.maybePla(pla,loc)) info.maybePla.setOn(loc);
      if(kb.maybePlaNonRab(pla,loc)) info.maybePlaNonRab.setOn(loc);
      if(kb.maybeNPla(loc))    info.maybeNPla.setOn(loc);
      if(kb.maybePla(opp,loc)) info.maybeOpp.setOn(loc);
    }
    return info;
  }

  //For pushpulls
  static CanMaybeStopGoalInfo update(CanMaybeStopGoalInfo info, const KB& kb, pla_t pla, loc_t src, loc_t dest, loc_t dest2)
  {
    (void)dest2;
    pla_t opp = gOpp(pla);
    Bitmap recalcLocs = Board::DISK[1][src] | Board::DISK[1][dest];
    info.definitelyOppRab &= ~recalcLocs;
    info.maybePla &= ~recalcLocs;
    info.maybePlaNonRab &= ~recalcLocs;
    info.maybeNPla &= ~recalcLocs;
    info.maybeOpp &= ~recalcLocs;
    while(recalcLocs.hasBits())
    {
      loc_t loc = recalcLocs.nextBit();
      if(kb.definitelyRabbit(opp,loc)) info.definitelyOppRab.setOn(loc);
      if(kb.maybePla(pla,loc)) info.maybePla.setOn(loc);
      if(kb.maybePlaNonRab(pla,loc)) info.maybePlaNonRab.setOn(loc);
      if(kb.maybeNPla(loc))    info.maybeNPla.setOn(loc);
      if(kb.maybePla(opp,loc)) info.maybeOpp.setOn(loc);
    }
    return info;
  }
};
}

//Verbosity:
//<= 0: nothing
//1: Stopping move
//2: Stopping move and top level of tree
//3: Stopping move and top two levels of tree
//4: Whole tree
//5: Whole tree plus goaling lines, plus whole goal tree on stopping move
//6: Everything
static bool canMaybeStopGoalRec(const KB& kb, pla_t pla, int numStepsLeft,
    CanMaybeStopGoalInfo info, int verbosity)
{
  pla_t opp = gOpp(pla);
  int oppGoalY = Board::GOALY[opp];
  if((Bitmap::BMPY[oppGoalY] & info.definitelyOppRab).hasBits())
    return false;
  step_t goalSteps[5];
  if(!canDefinitelyGoalRec(kb,opp,4,CanDefinitelyGoalInfo::create(kb,opp),goalSteps,verbosity-4))
  {
    if(verbosity == 5)
    canDefinitelyGoalRec(kb,opp,4,CanDefinitelyGoalInfo::create(kb,opp),goalSteps,2);
    return true;
  }

  if(numStepsLeft <= 0)
    return false;

  KB kb2 = kb;

  Bitmap relevantStopMap;
  for(int i = 0; i<5; i++)
  {
    if(goalSteps[i] == ERRSTEP)
      continue;
    relevantStopMap |= Board::DISK[1][gSrc(goalSteps[i])];
  }
  relevantStopMap |= Bitmap::adj(relevantStopMap & Bitmap::BMPTRAPS);
  Bitmap relevantStepInvolvesMap = relevantStopMap;
  for(int i = 0; i<numStepsLeft-1; i++)
  {
    relevantStepInvolvesMap |= Bitmap::adj(relevantStepInvolvesMap);
    relevantStepInvolvesMap |= Bitmap::adj(relevantStepInvolvesMap);
    relevantStepInvolvesMap |= Bitmap::adj(relevantStepInvolvesMap & Bitmap::BMPTRAPS);
  }

  Bitmap relevantPPInvolvesMap;
  if(numStepsLeft >= 2)
  {
    relevantPPInvolvesMap = relevantStopMap;
    for(int i = 0; i<numStepsLeft-2; i++)
    {
      relevantPPInvolvesMap |= Bitmap::adj(relevantPPInvolvesMap);
      relevantPPInvolvesMap |= Bitmap::adj(relevantPPInvolvesMap);
      relevantPPInvolvesMap |= Bitmap::adj(relevantPPInvolvesMap & Bitmap::BMPTRAPS);
    }
  }

  //Regular steps first
  Bitmap map;
  map = info.maybePla & (relevantStepInvolvesMap | Bitmap::adj(relevantStepInvolvesMap)) & Bitmap::adj(info.maybeNPla);
  while(map.hasBits())
  {
    loc_t src = map.nextBit();
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][src])
        continue;
      loc_t dest = src + Board::ADJOFFSETS[i];
      if(numStepsLeft == 1 && !relevantStepInvolvesMap.isOne(dest))
        continue;
      if(!relevantStepInvolvesMap.isOne(src) && !relevantStepInvolvesMap.isOne(dest))
        continue;
      if(!info.maybeNPla.isOne(dest))
        continue;
      //Don't bother stepping if dest already has all the possibilities that src does
      //and src is already potentially empty
      //and any surrounding trap is already potentially empty
      //and the dest has no surrounding special frozen pieces that could be unspecialfrozen by moving there
      if(kb2.plaPiecesAtAreAsGeneralAs(pla,dest,src) && kb2.maybeNPla(src) &&
          (Board::ADJACENTTRAP[src] == ERRLOC || kb2.maybeNPla(Board::ADJACENTTRAP[src])) &&
          !(numStepsLeft > 1 && kb2.anySpecialFrozenAround(pla,dest)))
        continue;
      if(!kb2.tryMaybeStep(pla,src,dest))
        continue;

      if(verbosity > 3 || (verbosity > 2 && numStepsLeft == 3) || (verbosity > 1 && numStepsLeft == 4))
      {printDots(4-numStepsLeft);
        cout << "Try step " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
      bool canStopGoal =
          canMaybeStopGoalRec(kb2,pla,numStepsLeft-1,
              CanMaybeStopGoalInfo::update(info,kb2,pla,src,dest),
              verbosity);
      if(canStopGoal)
      {if(verbosity > 0) {printDots(4-numStepsLeft);
      cout << "Stop step " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
      return true;}
      kb2 = kb;
    }
  }

  //Only one step left, no point in trying pushes or pulls
  if(numStepsLeft <= 1)
    return false;

  //Pulls next
  map = (info.maybePlaNonRab)
      & (Bitmap::adj(info.maybeOpp))
      & (relevantPPInvolvesMap | Bitmap::adj(relevantPPInvolvesMap))
      & Bitmap::adj(info.maybeNPla);
  while(map.hasBits())
  {
    loc_t dest = map.nextBit();
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][dest])
        continue;
      loc_t src = dest + Board::ADJOFFSETS[i];
      //Only pull things that might be opp
      if(!info.maybeOpp.isOne(src))
        continue;
      for(int j = 0; j<4; j++)
      {
        if(i == j || !Board::ADJOKAY[j][dest])
          continue;
        loc_t dest2 = dest + Board::ADJOFFSETS[j];
        //Final destination must be maybe empty
        if(!info.maybeNPla.isOne(dest2))
          continue;
        //At least one square must be involved
        if(!(relevantPPInvolvesMap.isOne(src) || relevantPPInvolvesMap.isOne(dest) || relevantPPInvolvesMap.isOne(dest2)))
          continue;
        //If it's consistent that the pulled piece is actualy your own elephant or empty, and it isn't next to a trap
        //(to avoid weird cases where you prefer a non-elephant because it lets you capture a piece on a trap)
        //and you won't affect the special freezing flags then there's no point pushing or pulling
        if(Board::ADJACENTTRAP[src] == ERRLOC && kb.maybePlaElephant(pla,src) && kb.maybeNPla(src) &&
            !(numStepsLeft > 2 && kb2.anySpecialFrozenAround(pla,src)))
          continue;

        //Don't bother pulling if dest2 already has all the possibilities that dest does
        //and dest already has all the possibilities that src does
        //and dest is already potentially empty
        //and src is already potentially empty
        //and any surrounding trap is around either dest or src is already potentially empty
        //and we won't affect the special freezing flag anywhere
        if(kb2.plaPiecesAtAreAsGeneralAs(pla,dest2,dest) &&
           kb2.plaPiecesAtAreAsGeneralAs(opp,dest,src) &&
           kb2.maybeNPla(src) && kb2.maybeNPla(dest) &&
           !(numStepsLeft > 2 && kb2.anySpecialFrozenAround(pla,dest2)) &&
           !(numStepsLeft > 2 && kb2.anySpecialFrozenAround(pla,src)) &&
           (Board::ADJACENTTRAP[src] == ERRLOC || kb2.maybeNPla(Board::ADJACENTTRAP[src])) &&
           (Board::ADJACENTTRAP[dest] == ERRLOC || kb2.maybeNPla(Board::ADJACENTTRAP[dest]))
           )
          continue;

        if(!kb2.tryMaybePull(pla,src,dest,dest2))
          continue;
        if(verbosity > 3 || (verbosity > 2 && numStepsLeft == 3) || (verbosity > 1 && numStepsLeft == 4))
        {printDots(4-numStepsLeft);
        cout << "Try pull " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        bool canGoal =
            canMaybeStopGoalRec(kb2,pla,numStepsLeft-2,
                CanMaybeStopGoalInfo::update(info,kb2,pla,src,dest,dest2),
                verbosity);
        if(canGoal)
        {if(verbosity > 0) {printDots(4-numStepsLeft);
        cout << "Stop pull " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        return true;}
        kb2 = kb;
      }
    }
  }

  //Pushes next
  map = (Bitmap::adj(info.maybePlaNonRab))
      & (info.maybeOpp)
      & (relevantPPInvolvesMap | Bitmap::adj(relevantPPInvolvesMap))
      & Bitmap::adj(info.maybeNPla);
  while(map.hasBits())
  {
    loc_t dest = map.nextBit();
    for(int i = 0; i<4; i++)
    {
      if(!Board::ADJOKAY[i][dest])
        continue;
      loc_t src = dest + Board::ADJOFFSETS[i];
      //Only push from things that might be pla nonrab
      if(!info.maybePlaNonRab.isOne(src))
        continue;
      for(int j = 0; j<4; j++)
      {
        if(i == j || !Board::ADJOKAY[j][dest])
          continue;
        loc_t dest2 = dest + Board::ADJOFFSETS[j];
        //Final destination must be maybe empty
        if(!info.maybeNPla.isOne(dest2))
          continue;
        //At least one square must be involved
        if(!(relevantPPInvolvesMap.isOne(src) || relevantPPInvolvesMap.isOne(dest) || relevantPPInvolvesMap.isOne(dest2)))
          continue;
        //If it's consistent that the pushed piece is actualy your own elephant, and it isn't next to a trap
        //(to avoid weird cases where you prefer a non-elephant because it lets you capture a piece on a trap)
        //and you won't affect the special freezing flags then there's no point pushing or pulling
        if(Board::ADJACENTTRAP[dest] == ERRLOC && kb.maybePlaElephant(pla,dest) &&
            !(numStepsLeft > 2 && kb2.anySpecialFrozenAround(pla,dest)))
          continue;

        //Don't bother pulling if dest2 already has all the possibilities that dest does
        //and dest already has all the possibilities that src does
        //and dest is already potentially empty
        //and src is already potentially empty
        //and any surrounding trap is around either dest or src is already potentially empty
        //and we won't affect the special freezing flag anywhere
        if(kb2.plaPiecesAtAreAsGeneralAs(opp,dest2,dest) &&
           kb2.plaPiecesAtAreAsGeneralAs(pla,dest,src) &&
           kb2.maybeNPla(src) && kb2.maybeNPla(dest) &&
           !(numStepsLeft > 2 && kb2.anySpecialFrozenAround(pla,dest)) &&
           (Board::ADJACENTTRAP[src] == ERRLOC || kb2.maybeNPla(Board::ADJACENTTRAP[src])) &&
           (Board::ADJACENTTRAP[dest] == ERRLOC || kb2.maybeNPla(Board::ADJACENTTRAP[dest]))
           )
          continue;
        if(!kb2.tryMaybePush(pla,src,dest,dest2))
          continue;
        if(verbosity > 3 || (verbosity > 2 && numStepsLeft == 3) || (verbosity > 1 && numStepsLeft == 4))
        {printDots(4-numStepsLeft);
        cout << "Try push " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        bool canGoal =
            canMaybeStopGoalRec(kb2,pla,numStepsLeft-2,
                CanMaybeStopGoalInfo::update(info,kb2,pla,src,dest,dest2),
                verbosity);
        if(canGoal)
        {if(verbosity > 0) {printDots(4-numStepsLeft);
        cout << "Stop push " << Board::writeStep(gStepSrcDest(dest,dest2)) << " " << Board::writeStep(gStepSrcDest(src,dest)) << endl;}
        return true;}
        kb2 = kb;
      }
    }
  }
  return false;
}

bool KB::canMaybeStopGoal(const KB& kb, pla_t pla, int numStepsLeft, int verbosity)
{
  return canMaybeStopGoalRec(kb,pla,numStepsLeft,CanMaybeStopGoalInfo::create(kb,pla),verbosity);
}
