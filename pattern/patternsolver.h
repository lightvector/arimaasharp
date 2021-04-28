/*
 * patternsolver.h
 * Author: davidwu
 */

#include "../core/global.h"
#include "../pattern/pattern.h"

struct Possibility
{
  pla_t owner;  //0 = silv, 1 = gold, 2 = npla
  int isRabbit; //0 = false, 1 = true, 2 = unknown

  //This field has the status of being just a special flag.
  //If set to 1, then we just outright assume a piece is frozen,
  //and we set it back to 0 whenever the piece is touched by a friendly
  //or a hostile nonrabbit piece is removed from being adjacent. We also delete the possiblity
  //entirely if a friendly piece is moved from adjacent, or if the piece itself is pushpulled
  //We never set it back to 1, and instead use actual testing of the surrounding
  //squares at move legality checking time to see if normal freezing might happen.
  //We only use this for the maybe player.
  int isSpecialFrozen; //0 = possible false, 1 = true

  Possibility();
  Possibility(pla_t owner, int isRabbit, int isSpecialFrozen);

  //Constructors
  static Possibility npla();
  static Possibility silv();
  static Possibility gold();

  //All of the functions assume adjacency and onboardness
  //All of the definite functions assume that dest is npla.
  //All of the maybe functions assume that dest is maybe npla.
  //Obviously, none of these test actual frozenness, since that depends on adjacent squares on the board.
  //But they do test special frozenness

  bool canDefiniteStep(pla_t pla, loc_t src, loc_t dest) const;
  //Same as canDefiniteStep, but npla is allowed
  bool canDefinitePhantomStep(pla_t pla, loc_t src, loc_t dest) const;
  //Pullers and pushers must be nonrabbit pla.
  bool canDefinitePhantomPush(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const;
  bool canDefinitePhantomPull(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const;
  //Any combination of plas and empties is fine, due to freezyness, might be legal
  //even if neither is legal individually.
  bool canDefinitePhantomStepStep(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const;

  bool maybeCanStep(pla_t pla, loc_t src, loc_t dest) const;
  bool maybeCanPush(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const;
  bool maybeCanPull(const Possibility& destP, pla_t pla, loc_t src, loc_t dest, loc_t dest2) const;

  static void write(ostream& out, const Possibility& p);
  static string write(const Possibility& p);
  friend ostream& operator<<(ostream& out, const Possibility& p);
};

class KB
{
  public:
  static void init();

  private:
  static const int GT = 0;
  static const int GEQ = 1;
  static const int LEQ = 2;
  static const int LT = 3;
  STRUCT_NAMED_PAIR(int,node,int,cmp,StrRelation);
  static inline int invertCmp(int cmp) {return 3-cmp;}

  struct PNode
  {
    bool isHead; //True if this is the head node for a list.
    Possibility possibility; //The possibility at this node
    int nextNode; //Index of next node in pNodes
    int prevNode; //Index of prev node in pNodes

    //Strength relationships to other nodes, the other nodes will have matching reversed relations to this node
    //These are unique per node pair.
    vector<StrRelation>* strRelations;

    PNode();
    ~PNode();
    PNode(const PNode& other);
    PNode& operator=(const PNode& other);
  };

  vector<PNode> pNodes; //Linked list nodes for locations
  int pHead[BSIZE]; //Pointer to head node of circular doubly linked list of possibilities for this location.
  int unusedList;   //Pointer to head node of list of unused nodes

  //The pattern has freezing for this player, for correctness, this player must be the maybe player
  pla_t hasFrozenPla;

  public:
  KB(const Pattern& pattern);
  ~KB();

  //Properties
  bool definitelyNPla(loc_t loc) const;
  bool maybeNPla(loc_t loc) const;
  bool maybeOccupied(loc_t loc) const;
  bool definitelyRabbit(pla_t pla, loc_t loc) const;
  bool definitelyPla(pla_t pla, loc_t loc) const;
  bool maybePla(pla_t pla, loc_t loc) const;
  bool maybePlaNonRab(pla_t pla, loc_t loc) const;
  bool maybePlaElephant(pla_t pla, loc_t loc) const;
  bool definitelyPlaOrNPla(pla_t pla, loc_t loc) const;
  bool definitelyHasDefender(pla_t pla, loc_t loc) const;
  bool maybeHasDefender(pla_t pla, loc_t loc) const;
  bool plaPiecesAtAreAsGeneralAs(pla_t pla, loc_t loc, loc_t loc2) const;
  bool anySpecialFrozen(pla_t pla, loc_t loc) const;
  bool anySpecialFrozenAround(pla_t pla, loc_t loc) const;

  //Returns the node added
  int addPossibilityToLoc(const Possibility& p, loc_t loc);

  //Board actions and movement
  bool canDefiniteStep(pla_t pla, loc_t src, loc_t dest) const;
  bool tryDefiniteStep(pla_t pla, loc_t src, loc_t dest);
  bool tryDefinitePhantomStep(pla_t pla, loc_t src, loc_t dest);
  bool tryDefinitePhantomPush(pla_t pla, loc_t src, loc_t dest, loc_t dest2);
  bool tryDefinitePhantomPull(pla_t pla, loc_t src, loc_t dest, loc_t dest2);
  bool tryDefinitePhantomStepStep(pla_t pla, loc_t src, loc_t dest, loc_t dest2);

  bool tryMaybeStep(pla_t pla, loc_t src, loc_t dest);
  bool tryMaybePush(pla_t pla, loc_t src, loc_t dest, loc_t dest2);
  bool tryMaybePull(pla_t pla, loc_t src, loc_t dest, loc_t dest2);

  static bool canDefinitelyGoal(const KB& kb, pla_t pla, int numSteps, int verbosity=0);
  static bool canMaybeStopGoal(const KB& kb, pla_t pla, int numSteps, int verbosity=0);

  void write(ostream& out) const;
  static void write(ostream& out, const KB& p);
  static string write(const KB& p);
  friend ostream& operator<<(ostream& out, const KB& p);

  private:
  //Basic list operations
  int next(int node) const;
  bool atLeastOne(int head) const;
  void addNode(int head, int node);
  int removeNode(int node);

  //More fancy list operations
  int getUnusedNode();
  void swapLists(int head1, int head2);
  void clear(int head);

  //Strength relation management
  int numStrengthRelations(int node) const;
  bool hasStrengthRelation(int node, int otherNode) const;
  bool isDefinitelyGt(int node, int otherNode) const;
  bool isDefinitelyGeq(int node, int otherNode) const;
  bool isMaybeGt(int node, int otherNode) const;
  bool isMaybeGeq(int node, int otherNode) const;
  void addStrengthRelationVsLoc(int node, int cmp, loc_t loc);
  void removeAllStrengthRelations(int node);

  //Combined with other properties
  bool definitelyDominates(loc_t loc, int otherNode) const;
  bool maybeDominates(loc_t loc, int otherNode) const;
  bool definitelyHasDominator(loc_t loc, int node) const;
  bool maybeHasDominator(loc_t loc, int node) const;
  bool definitelyUnfrozen(loc_t loc, int node) const;
  bool maybeUnfrozen(loc_t loc, int node) const;

  //Internal utility actions
  bool removeAllPlasAt(pla_t pla, loc_t loc);
  bool removeAllSpecialFrozenPlasAt(pla_t pla, loc_t loc);
  void removeAllSpecialFrozenPlasAround(pla_t pla, loc_t loc);
  void unSpecialFreeze(pla_t pla, loc_t loc);
  void unSpecialFreezeAround(pla_t pla, loc_t loc);
  void resolveTrapCapturesAround(pla_t pla, loc_t loc);

  //Testing
  void checkListConsistency(int head, const char* message) const;
  void checkListConsistency(int head, const string& message) const;
  void checkConsistency(const char* message) const;
  void checkConsistency(const string& message) const;
};
