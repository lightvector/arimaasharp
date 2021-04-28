
/*
 * pattern.h
 * Author: davidwu
 */

#ifndef PATTERN_H
#define PATTERN_H

#include "../core/global.h"
#include "../core/hash.h"
#include "../core/bytebuffer.h"
#include "../board/board.h"

using namespace std;

class Condition
{
  public:
  static void init();

  static const int COND_TRUE = 0;
  static const int OWNER_IS = 1;     //[pla], applies to a loc
  static const int PIECE_IS = 2;     //[piece], applies to a loc
  static const int LEQ_THAN_LOC = 3; //[loc], applies to a loc  //comparisons work properly for empty
  static const int LESS_THAN_LOC = 4;//[loc], applies to a loc  //comparisons work properly for empty
  static const int ADJ_PLA = 5;      //[pla], applies to a loc
  static const int ADJ2_PLA = 6;     //[pla], applies to a loc
  static const int FROZEN = 7;       //[pla], applies to a loc  //false if the square is empty
  static const int DOMINATED = 8;    //[pla], applies to a loc  //false if the square is empty
  static const int IS_OPEN = 9;      //, applies to a loc
  static const int AND = 10;
  static const int UNKNOWN = 11;     //[anything], unknown condition, can be used for variables, etc.

  int type;
  loc_t loc;
  int v1;
  bool istrue;
  Condition* c1;
  Condition* c2;

  Condition();
  Condition(int type, loc_t loc, int v1, bool istrue, const Condition* c1, const Condition* c2);
  Condition(int type, loc_t loc, int v1, bool istrue, const Condition& c1, const Condition& c2);
  Condition(const Condition& other);
  ~Condition();

  static Condition cTrue();
  static Condition cFalse();

  static Condition cAnd(const Condition& c1, const Condition& c2);
  static Condition cOr(const Condition& c1, const Condition& c2);
  static Condition cAnd(const Condition* c1, const Condition* c2);
  static Condition cOr(const Condition* c1, const Condition* c2);

  static Condition unknown(int val);

  static Condition ownerIs(loc_t loc, pla_t pla);
  static Condition pieceIs(loc_t loc, piece_t piece);
  static Condition leqThanLoc(loc_t loc, loc_t than);
  static Condition lessThanLoc(loc_t loc, loc_t than);
  static Condition adjPla(loc_t loc, pla_t pla);
  static Condition adj2Pla(loc_t loc, pla_t pla);
  static Condition frozen(loc_t loc, pla_t pla);
  static Condition dominated(loc_t loc, pla_t pla);
  static Condition isOpen(loc_t loc);

  static Condition notOwnerIs(loc_t loc, pla_t pla);
  static Condition notPieceIs(loc_t loc, piece_t piece);
  static Condition geqThanLoc(loc_t loc, loc_t than);
  static Condition greaterThanLoc(loc_t loc, loc_t than);
  static Condition notAdjPla(loc_t loc, pla_t pla);
  static Condition notAdj2Pla(loc_t loc, pla_t pla);
  static Condition notFrozen(loc_t loc, pla_t pla);
  static Condition notDominated(loc_t loc, pla_t pla);
  static Condition notIsOpen(loc_t loc);

  int size() const;

  //Check if condition is always true or always false
  bool isTrue() const;
  bool isFalse() const;

  //Standard assignment and equality
  bool operator==(const Condition& other) const;
  bool operator!=(const Condition& other) const;
  void operator=(const Condition& other);

  //Composition. Unlike cOr and cAnd above, these also attempt to do some equivalence
  //checking and simplifying of the expression
  Condition operator!() const;
  Condition operator&&(const Condition& other) const;
  Condition operator||(const Condition& other) const;

  //True if B definitely implies A or A definitely implies B.
  //False if no or we're not sure.
  //Can have false negatives but no false positives
  bool impliedBy(const Condition& other) const;
  bool implies(const Condition& other) const;

  //True if A implies B and B implies A, false if no or we're not sure
  //Can have false negatives but no false positives
  bool isEquivalent(const Condition& other) const;

  //Returns true if all the locs in this condition are ERRLOC;
  bool isLocationIndependent() const;
  //Returns true if this condition involves any leaves that compare strength against
  //any of the specified locations
  bool comparesStrengthAgainstAny(Bitmap locs) const;

  //Returns the condition with defaultLoc subsituted for every instance of ERRLOC
  Condition substituteDefault(loc_t defaultLoc) const;

  //Does a condition match the board?
  bool matches(const Board& b) const;
  //All ERRLOC in locs are filled in with loc
  bool matches(const Board& b, loc_t loc) const;
  //If true, treat all FROZEN X like OWNER_IS X. If false, treat all FROZEN X as false.
  bool matchesAssumeFrozen(const Board& b, loc_t loc, bool whatToAssume) const;

  //Returns true if it's impossible that both A and B are true.
  //Can have false negatives byt no false positives.
  bool conflictsWith(const Condition& other) const;

  //(A simplifiedGiven B) returns C such that (C and B) => A
  Condition simplifiedGiven(const Condition& other) const;
  Condition simplifiedGiven(const Bitmap ownerIs[2][3], const Bitmap pieceIs[2][7]) const;

  //Symmetry
  Condition lrFlip() const;
  Condition udColorFlip() const;

  Hash128 getLocationIndependentHash() const;

  //Check if this condition is equivalent to a conjunction of leaf conditions
  //That is, check that all ands in this condition have istrue.
  bool isConjunction() const;

  //Extracting all of the leaf conditions, not the ands/ors.
  //The conditions returned are the ones that if true, make it more likely that the condition is [leavesMake].
  //Returns new numSoFar
  int getLeafConditions(Condition* buf, int bufSize, int numSoFar=0, bool leavesMake = true) const;

  //Code conversion
  string toCPP() const;
  string toCPP(loc_t loc) const;

  static Condition read(const string& str);
  static string write(const Condition& cond, bool ignoreErrLoc = true);
  static void write(ostream& out, const Condition& cond, bool ignoreErrLoc = true);

  friend ostream& operator<<(ostream& out, const Condition& cond);

  private:
  static Condition readRecursive(const string& original, istringstream& in);
};

class Pattern
{
  int anonymousIdx;
  vector<string> names;
  vector<Condition> conditions;
  vector<int> lists;
  int16_t start[BSIZE];
  int16_t len[BSIZE];

  public:

  static void init();

  Pattern();
  ~Pattern();

  int getNumConditions() const;
  const int* getList(loc_t loc) const; //Doing anything to the pattern could realloc, invalidating the ptr
  int getListLen(loc_t loc) const;

  int getOrAddCondition(const Condition& cond);
  int addCondition(const Condition& cond, const string& name);

  int getConditionIndex(const Condition& cond) const;
  const Condition& getCondition(int conditionIdx) const;
  const string& getConditionName(int conditionIdx) const;

  int findConditionIdxWithName(const string& name) const;

  static bool isDefault(const string& name);
  static bool maybeGetDefaultCondition(const string& name, Condition& ret);

  void addConditionToLoc(int conditionIdx, loc_t loc);

  static const int IDX_FALSE; //Same idx as not found
  static const int IDX_TRUE;
  void clearLoc(loc_t loc);
  void clearLoc(loc_t loc, int value);

  bool isTrue() const;
  bool isFalse() const;
  int countTrue() const;
  bool isTrue(loc_t loc) const;
  bool isFalse(loc_t loc) const;
  bool matches(const Board& b) const;
  bool matchesAt(const Board& b, loc_t loc) const;
  bool matchesAtAssumeFrozen(const Board& b, loc_t loc, bool whatToAssume) const;

  //Is any condition at this loc implied by other?
  bool isAnyImpliedBy(const Condition& other, loc_t loc) const;
  //Impossible that both this and condition other at loc are true.
  //Can have false negatives but no false positives
  bool conflictsWith(const Condition& other, loc_t loc) const;
  //(A simplifiedGiven B) returns C such that (C and B) => A
  Pattern simplifiedGiven(const Condition& other, loc_t loc) const;

  //Returns new numSoFar.
  //Does NOT add the true or the false condition to buf if this loc of the pattern is known to be
  //always true or false (although it might add it to buf if we find this condition in a subtree
  //of one of the conditions at this loc)
  int getLeafConditions(loc_t loc, Condition* buf, int bufSize, int numSoFar=0, bool leavesMake = true) const;

  //Dump a bunch of bytes into buffer representing a hashable key for this pattern.
  //Buffer will be cleared and may be reallocated in the process and may have properties (autoexpand) modified.
  void computeHashBytes(ByteBuffer& buffer) const;
  //Same, but go ahead and compute a hash from the bytes
  Hash128 computeHash(ByteBuffer& buffer) const;

  Pattern lrFlip() const;
  Pattern udColorFlip() const;

  //Tries to remove some useless cases -
  //rabbits cannot already be on the goal row, pieces cannot be frozen if next
  //to guaranteed helper pieces, pieces cannot be frozen on a trap, etc.
  void simplifyGoalPattern(bool filterRabbitsOnGoal);

  static string write(const Pattern& pattern);
  static void write(ostream& out, const Pattern& pattern);
  friend ostream& operator<<(ostream& out, const Pattern& pattern);

  private:
  void deleteFromList(loc_t loc, int idx);
  bool nameAlreadyExists(const string& name);

  bool aroundExistsDefinitely(loc_t loc, const Condition& cond);
  bool aroundAllDefinitely(loc_t loc, const Condition& cond);
  bool conditionIdxIsAtLoc(int conditionIdx, loc_t loc);

};

class PatternRecord
{
  public:
  Pattern pattern;
  map<string,string> keyValues;

  PatternRecord();
  PatternRecord(const Pattern& p, const map<string,string>& keyValues);

  static PatternRecord read(const string& str);
  static string write(const PatternRecord& record);
  static void write(ostream& out, const PatternRecord& record);
  friend ostream& operator<<(ostream& out, const PatternRecord& record);
};

#endif
