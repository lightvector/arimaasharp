
/*
 * pattern.cpp
 * Author: davidwu
 */

#include <sstream>
#include <algorithm>
#include "../core/global.h"
#include "../core/hash.h"
#include "../core/md5.h"
#include "../core/rand.h"
#include "../board/btypes.h"
#include "../board/board.h"
#include "../pattern/pattern.h"
#include "../program/arimaaio.h"

using namespace std;

Condition::Condition()
:type(COND_TRUE),loc(ERRLOC),v1(0),istrue(true),c1(NULL),c2(NULL)
{}

Condition::Condition(int type, loc_t loc, int v1, bool isTrue, const Condition* otherc1, const Condition* otherc2)
:type(type),loc(loc),v1(v1),istrue(isTrue),c1(NULL),c2(NULL)
{
  if(type == AND)
  {
    c1 = new Condition(*(otherc1));
    c2 = new Condition(*(otherc2));
  }
}
Condition::Condition(int type, loc_t loc, int v1, bool isTrue, const Condition& otherc1, const Condition& otherc2)
:type(type),loc(loc),v1(v1),istrue(isTrue),c1(NULL),c2(NULL)
{
  if(type == AND)
  {
    c1 = new Condition(otherc1);
    c2 = new Condition(otherc2);
  }
}

Condition::Condition(const Condition& other)
:type(other.type),loc(other.loc),v1(other.v1),istrue(other.istrue),c1(NULL),c2(NULL)
{
  if(other.type == AND)
  {
    c1 = new Condition(*(other.c1));
    c2 = new Condition(*(other.c2));
  }
}

Condition::~Condition()
{
  if(c1 != NULL) delete c1;
  if(c2 != NULL) delete c2;
}

Condition Condition::cTrue() {return Condition(COND_TRUE,ERRLOC,0,true,NULL,NULL);}
Condition Condition::cFalse() {return Condition(COND_TRUE,ERRLOC,0,false,NULL,NULL);}

Condition Condition::cAnd(const Condition& c1, const Condition& c2) {return Condition(AND,ERRLOC,0,true,c1,c2);}
Condition Condition::cOr(const Condition& c1, const Condition& c2) {return Condition(AND,ERRLOC,0,false,!c1,!c2);}
Condition Condition::cAnd(const Condition* c1, const Condition* c2) {return Condition(AND,ERRLOC,0,true,c1,c2);}
Condition Condition::cOr(const Condition* c1, const Condition* c2) {return Condition(AND,ERRLOC,0,false,!(*c1),!(*c2));}

Condition Condition::unknown(int val) {return Condition(UNKNOWN,ERRLOC,val,true,NULL,NULL);}

Condition Condition::ownerIs(loc_t loc, pla_t pla) {return Condition(OWNER_IS,loc,pla,true,NULL,NULL);}
Condition Condition::pieceIs(loc_t loc, piece_t piece) {return Condition(PIECE_IS,loc,piece,true,NULL,NULL);}
Condition Condition::leqThanLoc(loc_t loc, loc_t than) {return Condition(LEQ_THAN_LOC,loc,than,true,NULL,NULL);}
Condition Condition::lessThanLoc(loc_t loc, loc_t than) {return Condition(LESS_THAN_LOC,loc,than,true,NULL,NULL);}
Condition Condition::adjPla(loc_t loc, pla_t pla) {return Condition(ADJ_PLA,loc,pla,true,NULL,NULL);}
Condition Condition::adj2Pla(loc_t loc, pla_t pla) {return Condition(ADJ2_PLA,loc,pla,true,NULL,NULL);}
Condition Condition::frozen(loc_t loc, pla_t pla) {return Condition(FROZEN,loc,pla,true,NULL,NULL);}
Condition Condition::dominated(loc_t loc, pla_t pla) {return Condition(DOMINATED,loc,pla,true,NULL,NULL);}
Condition Condition::isOpen(loc_t loc) {return Condition(IS_OPEN,loc,0,true,NULL,NULL);}

Condition Condition::notOwnerIs(loc_t loc, pla_t pla) {return Condition(OWNER_IS,loc,pla,false,NULL,NULL);}
Condition Condition::notPieceIs(loc_t loc, piece_t piece) {return Condition(PIECE_IS,loc,piece,false,NULL,NULL);}
Condition Condition::geqThanLoc(loc_t loc, loc_t than) {return Condition(LESS_THAN_LOC,loc,than,false,NULL,NULL);}
Condition Condition::greaterThanLoc(loc_t loc, loc_t than) {return Condition(LEQ_THAN_LOC,loc,than,false,NULL,NULL);}
Condition Condition::notAdjPla(loc_t loc, pla_t pla) {return Condition(ADJ_PLA,loc,pla,false,NULL,NULL);}
Condition Condition::notAdj2Pla(loc_t loc, pla_t pla) {return Condition(ADJ2_PLA,loc,pla,false,NULL,NULL);}
Condition Condition::notFrozen(loc_t loc, pla_t pla) {return Condition(FROZEN,loc,pla,false,NULL,NULL);}
Condition Condition::notDominated(loc_t loc, pla_t pla) {return Condition(DOMINATED,loc,pla,false,NULL,NULL);}
Condition Condition::notIsOpen(loc_t loc) {return Condition(IS_OPEN,loc,0,false,NULL,NULL);}

int Condition::size() const
{
  if(type == AND) return 1 + c1->size() + c2->size();
  else return 1;
}

bool Condition::isTrue() const { return type == COND_TRUE && istrue; }
bool Condition::isFalse() const { return type == COND_TRUE && !istrue; }

bool Condition::operator==(const Condition& other) const
{
  if(type != other.type)
    return false;
  if(type == AND)
    return istrue == other.istrue
        && *c1 == *(other.c1)
        && *c2 == *(other.c2);
  else
    return loc == other.loc
        && v1 == other.v1
        && istrue == other.istrue;
}

bool Condition::operator!=(const Condition& other) const
{
  return !(*this == other);
}

void Condition::operator=(const Condition& other)
{
  if(*this == other)
    return;
  if(c1 != NULL) delete c1;
  if(c2 != NULL) delete c2;

  type = other.type;
  loc = other.loc;
  v1 = other.v1;
  istrue = other.istrue;
  if(other.type == AND)
  {
    c1 = new Condition(*(other.c1));
    c2 = new Condition(*(other.c2));
  }
}

Condition Condition::operator!() const
{return Condition(type,loc,v1,!istrue,c1,c2);}

Condition Condition::operator&&(const Condition& other) const
{
  if(impliedBy(other)) return other;
  else if(other.impliedBy(*this)) return *this;
  return cAnd(*this,other);
}
Condition Condition::operator||(const Condition& other) const
{
  if(impliedBy(other)) return *this;
  else if(other.impliedBy(*this)) return other;
  return cOr(*this,other);
}

bool Condition::impliedBy(const Condition& other) const
{
  if(other.type == COND_TRUE && !other.istrue) return true;
  if(type == COND_TRUE && istrue)              return true;

  if(type == AND && istrue) return c1->impliedBy(other) && c2->impliedBy(other);
  if(type == AND)           return (!(*c1)).impliedBy(other) || (!(*c2)).impliedBy(other);

  if(other.type == AND && other.istrue) return impliedBy(*(other.c1)) || impliedBy(*(other.c2));
  if(other.type == AND)                 return impliedBy(!(*(other.c1))) && impliedBy(!(*(other.c2)));

  //SAME LOCATION
  if(loc == other.loc)
  {
    if(type == other.type)
    {
      if(type == OWNER_IS || type == PIECE_IS)
      {
        if(istrue && other.istrue) return v1 == other.v1;
        else if(istrue && !other.istrue) return false;
        else if(!istrue && other.istrue) return v1 != other.v1;
        else if(!istrue && !other.istrue)  return v1 == other.v1;
      }

      if(type == IS_OPEN)
        return istrue == other.istrue;

      if(type == LEQ_THAN_LOC || type == LESS_THAN_LOC || type == FROZEN || type == DOMINATED
          || type == ADJ_PLA || type == ADJ2_PLA || type == UNKNOWN)
        return istrue == other.istrue && v1 == other.v1;
    }

    if(type == OWNER_IS && istrue && other.type == FROZEN && other.istrue && v1 == other.v1)   return true;
    if(type == FROZEN && !istrue && other.type == OWNER_IS && !other.istrue && v1 == other.v1)   return true;
    if(type == OWNER_IS && !istrue && other.type == FROZEN && other.istrue && v1 != other.v1)   return true;
    if(type == FROZEN && !istrue && other.type == OWNER_IS && other.istrue && v1 != other.v1)   return true;

    if(type == OWNER_IS && istrue && other.type == DOMINATED && other.istrue && v1 == other.v1)   return true;
    if(type == DOMINATED && !istrue && other.type == OWNER_IS && !other.istrue && v1 == other.v1)   return true;
    if(type == OWNER_IS && !istrue && other.type == DOMINATED && other.istrue && v1 != other.v1)   return true;
    if(type == DOMINATED && !istrue && other.type == OWNER_IS && other.istrue && v1 != other.v1)   return true;

    if(type == DOMINATED && istrue && other.type == FROZEN && other.istrue && v1 == other.v1)   return true;
    if(type == FROZEN && !istrue && other.type == DOMINATED && !other.istrue && v1 == other.v1)   return true;
    if(type == DOMINATED && !istrue && other.type == FROZEN && other.istrue && v1 != other.v1)   return true;
    if(type == FROZEN && !istrue && other.type == DOMINATED && other.istrue && v1 != other.v1)   return true;

    if(type == PIECE_IS && !istrue && other.type == FROZEN && other.istrue && v1 == ELE)   return true;
    if(type == FROZEN && !istrue && other.type == PIECE_IS && other.istrue && other.v1 == ELE)   return true;
    if(type == PIECE_IS && !istrue && other.type == DOMINATED && other.istrue && v1 == ELE)   return true;
    if(type == DOMINATED && !istrue && other.type == PIECE_IS && other.istrue && other.v1 == ELE)   return true;

    if(type == OWNER_IS && !istrue && other.type == FROZEN && other.istrue && v1 == NPLA)   return true;
    if(type == FROZEN && !istrue && other.type == OWNER_IS && other.istrue && other.v1 == NPLA)   return true;
    if(type == PIECE_IS && !istrue && other.type == FROZEN && other.istrue && v1 == EMP)   return true;
    if(type == FROZEN && !istrue && other.type == PIECE_IS && other.istrue && other.v1 == EMP)   return true;

    if(type == OWNER_IS && !istrue && other.type == DOMINATED && other.istrue && v1 == NPLA)   return true;
    if(type == DOMINATED && !istrue && other.type == OWNER_IS && other.istrue && other.v1 == NPLA)   return true;
    if(type == PIECE_IS && !istrue && other.type == DOMINATED && other.istrue && v1 == EMP)   return true;
    if(type == DOMINATED && !istrue && other.type == PIECE_IS && other.istrue && other.v1 == EMP)   return true;

    if(type == PIECE_IS && !istrue && other.type == LESS_THAN_LOC && other.istrue && v1 == ELE)   return true;
    if(type == LESS_THAN_LOC && !istrue && other.type == PIECE_IS && other.istrue && other.v1 == ELE)   return true;

    if(type == ADJ_PLA && istrue && other.type == FROZEN && other.istrue && v1 == gOpp(other.v1))   return true;
    if(type == FROZEN && !istrue && other.type == ADJ_PLA && !other.istrue && v1 == gOpp(other.v1))   return true;
    if(type == ADJ_PLA && istrue && other.type == DOMINATED && other.istrue && v1 == gOpp(other.v1))   return true;
    if(type == DOMINATED && !istrue && other.type == ADJ_PLA && !other.istrue && v1 == gOpp(other.v1))   return true;

    if(type == FROZEN && !istrue && other.type == ADJ_PLA && other.istrue && v1 == other.v1)   return true;
    if(type == ADJ_PLA && !istrue && other.type == FROZEN && other.istrue && v1 == other.v1)   return true;
    if(type == FROZEN && !istrue && other.type == ADJ2_PLA && other.istrue && v1 == other.v1)   return true;
    if(type == ADJ2_PLA && !istrue && other.type == FROZEN && other.istrue && v1 == other.v1)   return true;

    if(type == ADJ_PLA && istrue && other.type == ADJ2_PLA && other.istrue && v1 == other.v1)   return true;
    if(type == ADJ2_PLA && !istrue && other.type == ADJ_PLA && !other.istrue && v1 == other.v1)   return true;

    if(type == LEQ_THAN_LOC && istrue && other.type == LESS_THAN_LOC && other.istrue && v1 == other.v1) return true;
    if(type == LESS_THAN_LOC && !istrue && other.type == LEQ_THAN_LOC && !other.istrue && v1 == other.v1) return true;

  }
  //ADJACENT LOCATION
  else if(loc != ERRLOC && other.loc != ERRLOC && Board::isAdjacent(loc,other.loc))
  {
    if(type == FROZEN && !istrue && other.type == OWNER_IS && other.istrue && v1 == other.v1)   return true;
    if(type == OWNER_IS && !istrue && other.type == FROZEN && other.istrue && v1 == other.v1)   return true;
    if(type == ADJ_PLA && istrue && other.type == OWNER_IS && other.istrue && v1 == other.v1)   return true;
    if(type == OWNER_IS && !istrue && other.type == ADJ_PLA && !other.istrue && v1 == other.v1)   return true;

    if(type == FROZEN && !istrue && other.type == OWNER_IS && !other.istrue && v1 != other.v1)   return true;
    if(type == OWNER_IS && istrue && other.type == FROZEN && other.istrue && v1 != other.v1)   return true;

    if(type == DOMINATED && !istrue && other.type == OWNER_IS && !other.istrue && v1 != other.v1)   return true;
    if(type == OWNER_IS && istrue && other.type == DOMINATED && other.istrue && v1 != other.v1)   return true;
  }
  return false;
}

bool Condition::isEquivalent(const Condition& other) const
{
  return impliedBy(other) && other.impliedBy(*this);
}

bool Condition::isLocationIndependent() const
{
  if(type == COND_TRUE || type == UNKNOWN)
    return true;
  if(type == AND)
    return c1->isLocationIndependent() && c2->isLocationIndependent();
  return loc == ERRLOC;
}

bool Condition::comparesStrengthAgainstAny(Bitmap locs) const
{
  if(type == LESS_THAN_LOC || type == LEQ_THAN_LOC)
    return locs.isOne(v1);
  if(type == AND)
    return c1->comparesStrengthAgainstAny(locs) || c2->comparesStrengthAgainstAny(locs);
  return false;
}

Condition Condition::substituteDefault(loc_t defaultLoc) const
{
  switch(type)
  {
  case COND_TRUE: return *this;
  case AND: return Condition(AND,ERRLOC,0,istrue,c1->substituteDefault(defaultLoc),c2->substituteDefault(defaultLoc));
  default:
    if(loc == ERRLOC)
      return Condition(type,defaultLoc,v1,istrue,NULL,NULL);
    else
      return *this;
  }
}

bool Condition::matches(const Board& b) const
{
  return matches(b,ERRLOC);
}

bool Condition::matches(const Board& b, loc_t defaultloc) const
{
  bool result;
  switch(type)
  {
  case COND_TRUE: result = true; break;
  case AND: result = c1->matches(b,defaultloc) && c2->matches(b,defaultloc); break;
  default:
    loc_t loc = this->loc == ERRLOC ? defaultloc : this->loc;
    DEBUGASSERT(loc != ERRLOC);
    switch(type)
    {
    case OWNER_IS: result = b.owners[loc] == v1; break;
    case PIECE_IS: result = b.pieces[loc] == v1; break;
    case LEQ_THAN_LOC: result = b.pieces[loc] <= b.pieces[v1]; break;
    case LESS_THAN_LOC: result = b.pieces[loc] < b.pieces[v1]; break;
    case ADJ_PLA: result = b.isGuarded(v1,loc); break;
    case ADJ2_PLA: result = b.isGuarded2(v1,loc); break;
    case FROZEN: result = b.owners[loc] == v1 && b.isFrozen(loc); break;
    case DOMINATED: result = b.owners[loc] == v1 && b.isDominated(loc); break;
    case IS_OPEN: result = b.isOpen(loc); break;
    case UNKNOWN: Global::fatalError("Condition::matches - attempted to match with condition UNKNOWN"); result = false; break;
    default: DEBUGASSERT(false); result = false; break;
    }
    break;
  }
  return result ^ (!istrue);
}

bool Condition::matchesAssumeFrozen(const Board& b, loc_t defaultloc, bool whatToAssume) const
{
  bool result;
  switch(type)
  {
  case COND_TRUE: result = true; break;
  case AND: result = c1->matchesAssumeFrozen(b,defaultloc,whatToAssume) && c2->matchesAssumeFrozen(b,defaultloc,whatToAssume); break;
  default:
    loc_t loc = this->loc == ERRLOC ? defaultloc : this->loc;
    DEBUGASSERT(loc != ERRLOC);
    switch(type)
    {
    case OWNER_IS: result = b.owners[loc] == v1; break;
    case PIECE_IS: result = b.pieces[loc] == v1; break;
    case LEQ_THAN_LOC: result = b.pieces[loc] <= b.pieces[v1]; break;
    case LESS_THAN_LOC: result = b.pieces[loc] < b.pieces[v1]; break;
    case ADJ_PLA: result = b.isGuarded(v1,loc); break;
    case ADJ2_PLA: result = b.isGuarded2(v1,loc); break;
    case FROZEN: result = b.owners[loc] == v1 && whatToAssume == true; break;
    case DOMINATED: result = b.owners[loc] == v1 && b.isDominated(loc); break;
    case IS_OPEN: result = b.isOpen(loc); break;
    case UNKNOWN: Global::fatalError("Condition::matchesAssumeFrozen - attempted to match with condition UNKNOWN"); result = false; break;
    default: DEBUGASSERT(false); result = false; break;
    }
    break;
  }
  return result ^ (!istrue);
}

bool Condition::conflictsWith(const Condition& other) const
{
  return (!other).impliedBy(*this) || (!(*this)).impliedBy(other);
}

Condition Condition::simplifiedGiven(const Condition& other) const
{
  if(other.type == COND_TRUE || type == COND_TRUE)
    return *this;

  if(type == AND && istrue)  return c1->simplifiedGiven(other) && c2->simplifiedGiven(other);
  if(type == AND && !istrue) return (!(*c1)).simplifiedGiven(other) || (!(*c2)).simplifiedGiven(other);

  if(other.type == AND && other.istrue)
    return (*this).simplifiedGiven(*other.c1).simplifiedGiven(*other.c2);

  if(impliedBy(other))
    return cTrue();

  return (*this);
}

Condition Condition::simplifiedGiven(const Bitmap ownerIs[2][3], const Bitmap pieceIs[2][7]) const
{
  if(type == AND && istrue)  return c1->simplifiedGiven(ownerIs,pieceIs) && c2->simplifiedGiven(ownerIs,pieceIs);
  if(type == AND && !istrue) return (!(*c1)).simplifiedGiven(ownerIs,pieceIs) || (!(*c2)).simplifiedGiven(ownerIs,pieceIs);
  if(type == OWNER_IS)
  {
    if(ownerIs[istrue][v1].isOne(loc))
      return cTrue();
    else if(ownerIs[!istrue][v1].isOne(loc))
      return cFalse();
  }
  else if(type == PIECE_IS)
  {
    if(pieceIs[istrue][v1].isOne(loc))
      return cTrue();
    else if(pieceIs[!istrue][v1].isOne(loc))
      return cFalse();
  }
  return *this;
}

static int lrFlipLoc(int x) {return x == ERRLOC ? ERRLOC : gLoc(7-gX(x),gY(x));}
static int udFlipLoc(int x) {return x == ERRLOC ? ERRLOC : gLoc(gX(x),7-gY(x));}

Condition Condition::lrFlip() const
{
  switch(type)
  {
  case COND_TRUE:     return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case OWNER_IS:      return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case PIECE_IS:      return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case LEQ_THAN_LOC:  return Condition(type,lrFlipLoc(loc),lrFlipLoc(v1),istrue,NULL,NULL);
  case LESS_THAN_LOC: return Condition(type,lrFlipLoc(loc),lrFlipLoc(v1),istrue,NULL,NULL);
  case ADJ_PLA:       return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case ADJ2_PLA:      return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case FROZEN:        return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case DOMINATED:     return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case IS_OPEN:       return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  case AND:           return Condition(AND,ERRLOC,0,istrue,c1->lrFlip(),c2->lrFlip());
  case UNKNOWN:       return Condition(type,lrFlipLoc(loc),v1,istrue,NULL,NULL);
  default: DEBUGASSERT(false); break;
  }
  return Condition();
}

Condition Condition::udColorFlip() const
{
  static const int OPPPLA[3] = {1,0,2};
  switch(type)
  {
  case COND_TRUE:     return Condition(type,udFlipLoc(loc),v1,istrue,NULL,NULL);
  case OWNER_IS:      return Condition(type,udFlipLoc(loc),OPPPLA[v1],istrue,NULL,NULL);
  case PIECE_IS:      return Condition(type,udFlipLoc(loc),v1,istrue,NULL,NULL);
  case LEQ_THAN_LOC:  return Condition(type,udFlipLoc(loc),udFlipLoc(v1),istrue,NULL,NULL);
  case LESS_THAN_LOC: return Condition(type,udFlipLoc(loc),udFlipLoc(v1),istrue,NULL,NULL);
  case ADJ_PLA:       return Condition(type,udFlipLoc(loc),OPPPLA[v1],istrue,NULL,NULL);
  case ADJ2_PLA:      return Condition(type,udFlipLoc(loc),OPPPLA[v1],istrue,NULL,NULL);
  case FROZEN:        return Condition(type,udFlipLoc(loc),OPPPLA[v1],istrue,NULL,NULL);
  case DOMINATED:     return Condition(type,udFlipLoc(loc),OPPPLA[v1],istrue,NULL,NULL);
  case IS_OPEN:       return Condition(type,udFlipLoc(loc),v1,istrue,NULL,NULL);
  case AND:           return Condition(AND,ERRLOC,0,istrue,c1->udColorFlip(),c2->udColorFlip());
  case UNKNOWN:       return Condition(type,udFlipLoc(loc),v1,istrue,NULL,NULL);
  default: DEBUGASSERT(false); break;
  }
  return Condition();
}

bool Condition::isConjunction() const
{
  if(type == AND)
  {
    if(!istrue)
      return false;
    return c1->isConjunction() && c2->isConjunction();
  }
  return true;
}

int Condition::getLeafConditions(Condition* buf, int bufSize, int numSoFar, bool leavesMake) const
{
  if(numSoFar >= bufSize)
    return numSoFar;

  if(type == AND)
  {
    bool newLeavesMake = (leavesMake == istrue);
    numSoFar = c1->getLeafConditions(buf,bufSize,numSoFar,newLeavesMake);
    if(numSoFar >= bufSize)
      return numSoFar;
    numSoFar = c2->getLeafConditions(buf,bufSize,numSoFar,newLeavesMake);
    return numSoFar;
  }

  if(leavesMake)
  {
    for(int i = 0; i<numSoFar; i++)
      if(this->isEquivalent(buf[i]))
        return numSoFar;
    buf[numSoFar] = *this;
  }
  else
  {
    buf[numSoFar] = !(*this);
    for(int i = 0; i<numSoFar; i++)
      if(buf[numSoFar].isEquivalent(buf[i]))
        return numSoFar;
  }

  return numSoFar+1;
}

string Condition::toCPP() const
{
  return toCPP(ERRLOC);
}

string Condition::toCPP(loc_t defaultLoc) const
{
  ostringstream out;
  loc_t loc = this->loc == ERRLOC ? defaultLoc : this->loc;
  if(istrue)
  {
    switch(type)
    {
    case COND_TRUE: out << "true"; break;
    case OWNER_IS: out << "b.owners[" << (int)loc << "] == " << (int)v1; break;
    case PIECE_IS: out << "b.pieces[" << (int)loc << "] == " << (int)v1; break;
    case LEQ_THAN_LOC: out << "b.pieces[" << (int)loc << "] <= b.pieces[" << (int)v1 << "]"; break;
    case LESS_THAN_LOC: out << "b.pieces[" << (int)loc << "] < b.pieces[" << (int)v1 << "]"; break;
    case ADJ_PLA: out << "b.isGuarded(" << (int)v1 << "," << (int)loc << ")"; break;
    case ADJ2_PLA: out << "b.isGuarded2(" << (int)v1 << "," << (int)loc << ")"; break;
    case FROZEN: out << "b.owners[" << (int)loc << "] == " << (int)v1 << " && b.isFrozen(" << (int)loc << ")"; break;
    case DOMINATED: out << "b.owners[" << (int)loc << "] == " << (int)v1 << " && b.isDominated(" << (int)loc << ")"; break;
    case IS_OPEN: out << "b.isOpen(" << (int)loc << ")"; break;
    case AND: out << "(" << c1->toCPP(defaultLoc) << ") && (" << c2->toCPP(defaultLoc) << ")"; break;
    case UNKNOWN: Global::fatalError("Condition::toCPP - attempted to convert condition UNKNOWN"); break;
    default: DEBUGASSERT(false); break;
    }
  }
  else
  {
    switch(type)
    {
    case COND_TRUE: out << "false"; break;
    case OWNER_IS: out << "b.owners[" << (int)loc << "] != " << (int)v1; break;
    case PIECE_IS: out << "b.pieces[" << (int)loc << "] != " << (int)v1; break;
    case LEQ_THAN_LOC: out << "b.pieces[" << (int)loc << "] > b.pieces[" << (int)v1 << "]"; break;
    case LESS_THAN_LOC: out << "b.pieces[" << (int)loc << "] >= b.pieces[" << (int)v1 << "]"; break;
    case ADJ_PLA: out << "!b.isGuarded(" << (int)v1 << "," << (int)loc << ")"; break;
    case ADJ2_PLA: out << "!b.isGuarded2(" << (int)v1 << "," << (int)loc << ")"; break;
    case FROZEN: out << "b.owners[" << (int)loc << "] != " << (int)v1 << " || b.isThawed(" << (int)loc << ")"; break;
    case DOMINATED: out << "b.owners[" << (int)loc << "] != " << (int)v1 << " || !b.isDominated(" << (int)loc << ")"; break;
    case IS_OPEN: out << "!b.isOpen(" << (int)loc << ")"; break;
    case AND: out << "(" << (!(*c1)).toCPP(defaultLoc) << ") || (" << (!(*c2)).toCPP(defaultLoc) << ")"; break;
    case UNKNOWN: Global::fatalError("Condition::toCPP - attempted to convert condition UNKNOWN"); break;
    default: DEBUGASSERT(false); break;
    }
  }
  return out.str();
}

static Hash128 COND_TRUE_HASH_ARR[2];
static Hash128 OWNER_IS_HASH_ARR[2][NUMPLAS+1];
static Hash128 PIECE_IS_HASH_ARR[2][NUMTYPES];
static Hash128 LEQ_THAN_LOC_HASH_ARR[2][BSIZE];
static Hash128 LESS_THAN_LOC_HASH_ARR[2][BSIZE];
static Hash128 ADJ_PLA_HASH_ARR[2][NUMPLAS];
static Hash128 ADJ2_PLA_HASH_ARR[2][NUMPLAS];
static Hash128 FROZEN_HASH_ARR[2][NUMPLAS];
static Hash128 DOMINATED_HASH_ARR[2][NUMPLAS];
static Hash128 IS_OPEN_HASH_ARR[2];

static Hash128 makeHash128(Rand& rand)
{
  uint64_t h0 = rand.nextUInt64();
  uint64_t h1 = rand.nextUInt64();
  return Hash128(h0,h1);
}

void Condition::init()
{
  Rand rand(Hash::simpleHash("Condition::init"));
  for(int isTrue = 0; isTrue < 2; isTrue++)
  {
    COND_TRUE_HASH_ARR[isTrue] = makeHash128(rand);
    for(int i = 0; i<NUMPLAS+1; i++)
      OWNER_IS_HASH_ARR[isTrue][i] = makeHash128(rand);
    for(int i = 0; i<NUMTYPES; i++)
      PIECE_IS_HASH_ARR[isTrue][i] = makeHash128(rand);
    for(int i = 0; i<BSIZE; i++)
    {
      LEQ_THAN_LOC_HASH_ARR[isTrue][i] = makeHash128(rand);
      LESS_THAN_LOC_HASH_ARR[isTrue][i] = makeHash128(rand);
    }
    for(int i = 0; i<NUMPLAS; i++)
    {
      ADJ_PLA_HASH_ARR[isTrue][i] = makeHash128(rand);
      ADJ2_PLA_HASH_ARR[isTrue][i] = makeHash128(rand);
      FROZEN_HASH_ARR[isTrue][i] = makeHash128(rand);
      DOMINATED_HASH_ARR[isTrue][i] = makeHash128(rand);
    }
    IS_OPEN_HASH_ARR[isTrue] = makeHash128(rand);
  }
}

Hash128 Condition::getLocationIndependentHash() const
{
  switch(type)
  {
  case COND_TRUE: return COND_TRUE_HASH_ARR[istrue];
  case OWNER_IS: return OWNER_IS_HASH_ARR[istrue][v1];
  case PIECE_IS: return PIECE_IS_HASH_ARR[istrue][v1];
  case LEQ_THAN_LOC: return LEQ_THAN_LOC_HASH_ARR[istrue][v1];
  case LESS_THAN_LOC: return LESS_THAN_LOC_HASH_ARR[istrue][v1];
  case ADJ_PLA: return ADJ_PLA_HASH_ARR[istrue][v1];
  case ADJ2_PLA: return ADJ2_PLA_HASH_ARR[istrue][v1];
  case FROZEN: return FROZEN_HASH_ARR[istrue][v1];
  case DOMINATED: return DOMINATED_HASH_ARR[istrue][v1];
  case IS_OPEN: return IS_OPEN_HASH_ARR[istrue];
  case AND:
  {
    Hash128 a = c1->getLocationIndependentHash();
    Hash128 b = c2->getLocationIndependentHash();
    Hash128 combined = Hash128(a.hash0 + b.hash0, a.hash1 ^ b.hash1);
    if(!istrue)
    {
      uint32_t x = Hash::lowBits(combined.hash0);
      uint32_t y = Hash::highBits(combined.hash0);
      uint32_t z = Hash::lowBits(combined.hash1);
      uint32_t w = Hash::highBits(combined.hash1);
      uint32_t u = 0xCAFEBABE;
      Hash::jenkinsMix(x,w,u);
      Hash::jenkinsMix(y,u,z);
      return Hash128(Hash::combine(x,u),Hash::combine(y,w));
    }
    return combined;
  }
  break;
  case UNKNOWN: Global::fatalError("Condition::getLocationIndependentHash - cannot get hash of UNKNOWN"); break;
  default: DEBUGASSERT(false); break;
  }
  return Hash128();
}


//PATTERN-----------------------------------------------------------------------------------

const int Pattern::IDX_FALSE = -1;
const int Pattern::IDX_TRUE = -2;
static const int FALSE_LIST[1] = {-1};
static const int TRUE_LIST[1] = {-2};
static string FALSE_STR;
static string TRUE_STR;
static Condition FALSE_COND;
static Condition TRUE_COND;

static map<string,Condition> condOfDefaultName;

void Pattern::init()
{
  FALSE_STR = string("-");
  TRUE_STR = string("");
  FALSE_COND = Condition::cFalse();
  TRUE_COND = Condition::cTrue();

  loc_t esq = ERRLOC;
  condOfDefaultName[FALSE_STR] = Condition::cFalse();
  condOfDefaultName[TRUE_STR] = Condition::cTrue();
  condOfDefaultName["."] = Condition::ownerIs(esq,NPLA);
  condOfDefaultName["A"] = Condition::ownerIs(esq,GOLD);
  condOfDefaultName["a"] = Condition::ownerIs(esq,SILV);
  condOfDefaultName["P"] = Condition::ownerIs(esq,GOLD) && !Condition::pieceIs(esq,RAB);
  condOfDefaultName["p"] = Condition::ownerIs(esq,SILV) && !Condition::pieceIs(esq,RAB);
  condOfDefaultName["R"] = Condition::ownerIs(esq,GOLD) && Condition::pieceIs(esq,RAB);
  condOfDefaultName["r"] = Condition::ownerIs(esq,SILV) && Condition::pieceIs(esq,RAB);
  condOfDefaultName["C"] = Condition::ownerIs(esq,GOLD) && Condition::pieceIs(esq,CAT);
  condOfDefaultName["c"] = Condition::ownerIs(esq,SILV) && Condition::pieceIs(esq,CAT);
  condOfDefaultName["D"] = Condition::ownerIs(esq,GOLD) && Condition::pieceIs(esq,DOG);
  condOfDefaultName["d"] = Condition::ownerIs(esq,SILV) && Condition::pieceIs(esq,DOG);
  condOfDefaultName["H"] = Condition::ownerIs(esq,GOLD) && Condition::pieceIs(esq,HOR);
  condOfDefaultName["h"] = Condition::ownerIs(esq,SILV) && Condition::pieceIs(esq,HOR);
  condOfDefaultName["M"] = Condition::ownerIs(esq,GOLD) && Condition::pieceIs(esq,CAM);
  condOfDefaultName["m"] = Condition::ownerIs(esq,SILV) && Condition::pieceIs(esq,CAM);
  condOfDefaultName["E"] = Condition::ownerIs(esq,GOLD) && Condition::pieceIs(esq,ELE);
  condOfDefaultName["e"] = Condition::ownerIs(esq,SILV) && Condition::pieceIs(esq,ELE);
  condOfDefaultName["F"] = Condition::frozen(esq,GOLD);
  condOfDefaultName["f"] = Condition::frozen(esq,SILV);
  condOfDefaultName["Q"] = Condition::frozen(esq,GOLD) && Condition::pieceIs(esq,RAB);
  condOfDefaultName["q"] = Condition::frozen(esq,SILV) && Condition::pieceIs(esq,RAB);
  condOfDefaultName["?"] = Condition::unknown(0);
}

static bool maybeGetDefaultName(const Condition& cond, string& ret)
{
  for(map<string,Condition>::const_iterator iter = condOfDefaultName.begin(); iter != condOfDefaultName.end(); ++iter)
  {
    if(iter->second.isEquivalent(cond))
    {ret = iter->first; return true;}
  }
  return false;
}

bool Pattern::isDefault(const string& s)
{
  return condOfDefaultName.find(s) != condOfDefaultName.end();
}

bool Pattern::maybeGetDefaultCondition(const string& s, Condition& ret)
{
  map<string,Condition>::const_iterator iter = condOfDefaultName.find(s);
  if(iter == condOfDefaultName.end())
    return false;
  ret = iter->second;
  return true;
}

Pattern::Pattern()
{
  anonymousIdx = 0;
  for(int i = 0; i<BSIZE; i++)
  {
    start[i] = IDX_FALSE;
    len[i] = 0;
  }
}
Pattern::~Pattern()
{

}

int Pattern::getNumConditions() const
{
  return conditions.size();
}

const int* Pattern::getList(loc_t loc) const
{
  if(start[loc] == IDX_FALSE)
    return FALSE_LIST;
  if(start[loc] == IDX_TRUE)
    return TRUE_LIST;
  return &lists[start[loc]];
}

int Pattern::getListLen(loc_t loc) const
{
  if(start[loc] == IDX_FALSE)
    return 1;
  if(start[loc] == IDX_TRUE)
    return 1;
  return len[loc];
}

bool Pattern::nameAlreadyExists(const string& name)
{
  int numNames = names.size();
  for(int i = 0; i<numNames; i++)
    if(names[i] == name)
      return true;
  return false;
}

int Pattern::getOrAddCondition(const Condition& cond)
{
  if(cond.isFalse())
    return IDX_FALSE;
  if(cond.isTrue())
    return IDX_TRUE;

  int size = conditions.size();
  for(int i = 0; i<size; i++)
    if(conditions[i] == cond)
      return i;
  DEBUGASSERT(cond.isLocationIndependent());
  conditions.push_back(cond);

  string defaultName;
  if(maybeGetDefaultName(cond,defaultName))
  {
    if(nameAlreadyExists(defaultName))
      Global::fatalError(string("Default name somehow already exists: ") + defaultName);
    names.push_back(defaultName);
  }
  else
  {
    string nextName = Global::intToString(anonymousIdx++);
    while(nameAlreadyExists(nextName))
      nextName = Global::intToString(anonymousIdx++);
    names.push_back(nextName);
  }
  return (int)conditions.size()-1;
}

int Pattern::addCondition(const Condition& cond, const string& name)
{
  if(isDefault(name))
    Global::fatalError(string("Cannot add condition with a default name: ") + name);

  if(cond.isFalse())
    return IDX_FALSE;
  if(cond.isTrue())
    return IDX_TRUE;

  DEBUGASSERT(cond.isLocationIndependent());
  conditions.push_back(cond);

  int numNames = names.size();
  for(int i = 0; i<numNames; i++)
    if(names[i] == name)
      Global::fatalError(string("Attempted to add condition with a duplicate name: ") + name);
  names.push_back(name);

  int x = 0;
  if(Global::tryStringToInt(name,x) && x == anonymousIdx)
    anonymousIdx++;

  return (int)conditions.size()-1;
}

int Pattern::getConditionIndex(const Condition& cond) const
{
  if(cond.isFalse())
    return IDX_FALSE;
  if(cond.isTrue())
    return IDX_TRUE;

  int size = conditions.size();
  for(int i = 0; i<size; i++)
    if(conditions[i] == cond)
      return i;
  return IDX_FALSE;
}

const Condition& Pattern::getCondition(int conditionIdx) const
{
  if(conditionIdx == IDX_FALSE)
    return FALSE_COND;
  if(conditionIdx == IDX_TRUE)
    return TRUE_COND;
  return conditions[conditionIdx];
}

const string& Pattern::getConditionName(int conditionIdx) const
{
  if(conditionIdx == IDX_FALSE)
    return FALSE_STR;
  if(conditionIdx == IDX_TRUE)
    return TRUE_STR;
  return names[conditionIdx];
}

int Pattern::findConditionIdxWithName(const string& name) const
{
  int numNames = names.size();
  for(int i = 0; i<numNames; i++)
    if(names[i] == name)
      return i;
  return IDX_FALSE;
}

bool Pattern::conditionIdxIsAtLoc(int conditionIdx, loc_t loc)
{
  int st = start[loc];
  int ln = len[loc];
  for(int i = st; i<st+ln; i++)
  {
    if(lists[i] == conditionIdx)
      return true;
  }
  return false;
}

void Pattern::addConditionToLoc(int conditionIdx, loc_t loc)
{
  if(start[loc] == IDX_TRUE)
    return;
  if(conditionIdx == IDX_FALSE)
    return;
  if(conditionIdx == IDX_TRUE)
  {
    clearLoc(loc,IDX_TRUE);
    return;
  }

  if(start[loc] == IDX_FALSE)
  {
    //Create a sublist at the end
    start[loc] = lists.size();
    len[loc] = 1;
    lists.push_back(conditionIdx);
    return;
  }

  //Check if this condition already exists
  //If so, don't add it again
  int st = start[loc];
  int ln = len[loc];
  for(int i = st; i<st+ln; i++)
  {
    if(lists[i] == conditionIdx)
      return;
  }

  //New condition, so add it. Shift everything up and then insert it.
  for(int i = 0; i<BSIZE; i++)
  {
    if(start[i] > st)
      start[i] += 1;
  }
  lists.insert(lists.begin()+(st+ln), conditionIdx);
  len[loc]++;
  return;
}

void Pattern::clearLoc(loc_t loc)
{
  clearLoc(loc,IDX_FALSE);
}

void Pattern::clearLoc(loc_t loc, int value)
{
  if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE)
  {
    start[loc] = value;
    return;
  }

  //Delete the sublist and shift everything down
  int st = start[loc];
  int ln = len[loc];
  lists.erase(lists.begin()+st, lists.begin()+(st+ln));
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t i = gLoc(x,y);
      if(start[i] > st)
        start[i] -= ln;
    }
  }
  start[loc] = value;
  return;
}

void Pattern::deleteFromList(loc_t loc, int idx)
{
  if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE)
    DEBUGASSERT(false);
  //Delete the elt and shift everything down
  int st = start[loc];
  int ln = len[loc];
  DEBUGASSERT(ln > 0);
  DEBUGASSERT(idx >= 0 && idx < ln);
  if(ln == 1)
  {clearLoc(loc,IDX_FALSE); return;}

  lists.erase(lists.begin()+st+idx);
  len[loc]--;
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t i = gLoc(x,y);
      if(start[i] > st)
        start[i]--;
    }
  }
}

bool Pattern::isTrue() const
{
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(start[loc] != IDX_TRUE)
        return false;
    }
  }
  return true;
}

bool Pattern::isFalse() const
{
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(start[loc] == IDX_FALSE)
        return true;
    }
  }
  return false;
}

int Pattern::countTrue() const
{
  int count = 0;
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(start[loc] == IDX_TRUE)
        count++;
    }
  }
  return count;
}


bool Pattern::isTrue(loc_t loc) const
{
  return start[loc] == IDX_TRUE;
}
bool Pattern::isFalse(loc_t loc) const
{
  return start[loc] == IDX_FALSE;
}

bool Pattern::matches(const Board& b) const
{
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(!matchesAt(b,loc))
        return false;
    }
  }
  return true;
}

bool Pattern::matchesAt(const Board& b, loc_t loc) const
{
  int st = start[loc];
  int ln = len[loc];

  if(st == IDX_FALSE)
    return false;
  if(st == IDX_TRUE)
    return true;

  for(int j = st; j<st+ln; j++)
  {
    int cidx = lists[j];
    if(conditions[cidx].matches(b,loc))
      return true;
  }
  return false;
}

bool Pattern::matchesAtAssumeFrozen(const Board& b, loc_t loc, bool whatToAssume) const
{
  int st = start[loc];
  int ln = len[loc];

  if(st == IDX_FALSE)
    return false;
  if(st == IDX_TRUE)
    return true;

  for(int j = st; j<st+ln; j++)
  {
    int cidx = lists[j];
    if(conditions[cidx].matchesAssumeFrozen(b,loc,whatToAssume))
      return true;
  }
  return false;
}



bool Pattern::conflictsWith(const Condition& other, loc_t loc) const
{
  DEBUGASSERT(other.isLocationIndependent());
  int st = start[loc];
  int ln = len[loc];

  if(st == IDX_FALSE)
    return true;
  if(st == IDX_TRUE)
    return false;

  for(int j = st; j<st+ln; j++)
  {
    int cidx = lists[j];
    if(!(conditions[cidx].conflictsWith(other)))
      return false;
  }
  return true;
}

bool Pattern::isAnyImpliedBy(const Condition& other, loc_t loc) const
{
  DEBUGASSERT(other.isLocationIndependent());
  int st = start[loc];
  int ln = len[loc];

  if(other.isFalse())
    return true;
  if(st == IDX_FALSE)
    return false;
  if(st == IDX_TRUE)
    return true;

  for(int j = st; j<st+ln; j++)
  {
    int cidx = lists[j];
    if(conditions[cidx].impliedBy(other))
      return true;
  }
  return false;
}

//MINOR make this understand adjacency stuff like freezing?
Pattern Pattern::simplifiedGiven(const Condition& other, loc_t loc) const
{
  DEBUGASSERT(other.isLocationIndependent());
  Pattern p = *this;

  int st = start[loc];
  int ln = len[loc];

  if(st == IDX_TRUE || st == IDX_FALSE)
    return p;

  //Copy condition indices before we clear them
  int cidxsBuffer[64];
  int* cidxs;
  bool allocated = false;
  if(ln > 64)
  {
    allocated = true;
    cidxs = new int[ln];
  }
  else
    cidxs = cidxsBuffer;

  for(int j = st; j<st+ln; j++)
    cidxs[j-st] = lists[j];

  //Clear them
  p.clearLoc(loc,IDX_FALSE);

  //Now add back all the simplified conditions
  for(int j = st; j<st+ln; j++)
  {
    int cidx = cidxs[j-st];
    if(conditions[cidx].conflictsWith(other))
      continue;
    Condition c = conditions[cidx].simplifiedGiven(other);
    int otherCidx = p.getOrAddCondition(c);
    p.addConditionToLoc(otherCidx,loc);
  }

  if(allocated)
    delete[] cidxs;
  return p;
}

int Pattern::getLeafConditions(loc_t loc, Condition* buf, int bufSize, int numSoFar, bool leavesMake) const
{
  int st = start[loc];
  int ln = len[loc];

  if(st == IDX_TRUE || st == IDX_FALSE)
    return numSoFar;
  for(int j = st; j<st+ln; j++)
  {
    int cidx = lists[j];
    numSoFar = conditions[cidx].getLeafConditions(buf,bufSize,numSoFar,leavesMake);
  }
  return numSoFar;
}


Pattern Pattern::lrFlip() const
{
  Pattern p = *this;
  for(int i = 0; i<(int)p.conditions.size(); i++)
    p.conditions[i] = p.conditions[i].lrFlip();

  for(int y = 7; y >= 0; y--)
  {
    for(int x = 0; x<4; x++)
    {
      int loc = gLoc(x,y);
      int revloc = gLoc(7-x,y);
      int temp;
      temp = p.start[loc];
      p.start[loc] = p.start[revloc];
      p.start[revloc] = temp;
      temp = p.len[loc];
      p.len[loc] = p.len[revloc];
      p.len[revloc] = temp;
    }
  }
  return p;
}

Pattern Pattern::udColorFlip() const
{
  Pattern p = *this;
  for(int i = 0; i<(int)p.conditions.size(); i++)
    p.conditions[i] = p.conditions[i].udColorFlip();

  for(int y = 7; y >= 4; y--)
  {
    for(int x = 0; x<8; x++)
    {
      int loc = gLoc(x,y);
      int revloc = gLoc(x,7-y);
      int temp;
      temp = p.start[loc];
      p.start[loc] = p.start[revloc];
      p.start[revloc] = temp;
      temp = p.len[loc];
      p.len[loc] = p.len[revloc];
      p.len[revloc] = temp;
    }
  }
  return p;
}

//Does every adjacent square to loc have at least one of  condition imply cond?
bool Pattern::aroundAllDefinitely(loc_t loc, const Condition& cond)
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    int adj = loc + Board::ADJOFFSETS[i];
    const int* list = getList(adj);
    int length = getListLen(adj);
    for(int j = 0; j<length; j++)
    {
      if(!(cond.impliedBy(getCondition(list[j]))))
        return false;
    }
  }
  return true;
}

//Does any adjacent square to loc have at all of its conditions imply...
bool Pattern::aroundExistsDefinitely(loc_t loc, const Condition& cond)
{
  for(int i = 0; i<4; i++)
  {
    if(!Board::ADJOKAY[i][loc])
      continue;
    int adj = loc + Board::ADJOFFSETS[i];
    const int* list = getList(adj);
    int length = getListLen(adj);
    bool allGood = true;
    for(int j = 0; j<length; j++)
    {
      if(!(cond.impliedBy(getCondition(list[j]))))
      {allGood = false; break;}
    }
    if(allGood)
      return true;
  }
  return false;
}

void Pattern::simplifyGoalPattern(bool filterRabbitsOnGoal)
{
  int esq = ERRLOC;
  string stringA = string("A");
  string stringa = string("a");
  Condition isGold = Condition::ownerIs(esq,GOLD);
  Condition isSilv = Condition::ownerIs(esq,SILV);
  Condition isNPla = Condition::ownerIs(esq,NPLA);
  Condition notGold = !Condition::ownerIs(esq,GOLD);
  Condition notSilv = !Condition::ownerIs(esq,SILV);
  Condition isRab = Condition::pieceIs(esq,RAB);
  Condition goldRab = Condition::ownerIs(esq,GOLD) && Condition::pieceIs(esq,RAB);
  Condition silvRab = Condition::ownerIs(esq,SILV) && Condition::pieceIs(esq,RAB);
  Condition goldFrozen = Condition::frozen(esq,GOLD);
  Condition silvFrozen = Condition::frozen(esq,SILV);
  Condition anyFrozen = Condition::frozen(esq,GOLD) || Condition::frozen(esq,SILV);
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE)
        continue;

      int* list = &lists[start[loc]];
      int length = len[loc];
      for(int i = 0; i<length; i++)
      {
        string defaultName = "";
        const Condition& cond = getCondition(list[i]);
        bool isDefault = maybeGetDefaultName(cond, defaultName);

        if(filterRabbitsOnGoal)
        {
          if(y == 0 && silvRab.impliedBy(cond))
          {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}
          if(y == 0 && isDefault && defaultName == stringa)
          {int cidx = getOrAddCondition(condOfDefaultName["p"]);
           if(conditionIdxIsAtLoc(cidx,loc))
           {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}
           else list[i] = cidx;}
          if(y == 7 && goldRab.impliedBy(cond))
          {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}
          if(y == 7 && isDefault && defaultName == stringA)
          {int cidx = getOrAddCondition(condOfDefaultName["P"]);
           if(conditionIdxIsAtLoc(cidx,loc))
           {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}
           else list[i] = cidx;}
        }

        if(Board::ISTRAP[loc] && anyFrozen.impliedBy(cond))
        {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}

        if(goldFrozen.impliedBy(cond) && (aroundExistsDefinitely(loc,isGold) || aroundAllDefinitely(loc,notSilv || isRab)))
        {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}
        if(silvFrozen.impliedBy(cond) && (aroundExistsDefinitely(loc,isSilv) || aroundAllDefinitely(loc,notGold || isRab)))
        {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}

        if(Board::ISTRAP[loc] && isGold.impliedBy(cond) && aroundAllDefinitely(loc,notGold))
        {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}
        if(Board::ISTRAP[loc] && isSilv.impliedBy(cond) && aroundAllDefinitely(loc,notSilv))
        {deleteFromList(loc,i); if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE) break; list = &lists[start[loc]]; length = len[loc]; i--; continue;}
      }
    }
  }
  for(int y = 0; y < 8; y++)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(start[loc] == IDX_FALSE || start[loc] == IDX_TRUE)
        continue;
      if(isAnyImpliedBy(isNPla,loc) && isAnyImpliedBy(isGold,loc) && isAnyImpliedBy(isSilv,loc))
        clearLoc(loc,IDX_TRUE);
    }
  }
}

void Pattern::computeHashBytes(ByteBuffer& buffer) const
{
  buffer.clear();
  buffer.setAutoExpand(true);
  buffer.add32(0x12345678U);

  int numConditions = conditions.size();
  DEBUGASSERT(numConditions <= 512);

  Hash128 conditionHashes[numConditions];
  for(int i = 0; i<numConditions; i++)
    conditionHashes[i] = conditions[i].getLocationIndependentHash();

  for(loc_t loc = 0; loc < BSIZE; loc++)
  {
    if(loc & 0x88)
      continue;
    int st = start[loc];
    int ln = len[loc];

    buffer.add8(loc);
    if(st == IDX_TRUE)
      buffer.add8('T');
    else if(st == IDX_FALSE)
      buffer.add8('F');
    else
    {
      buffer.add8('N');
      buffer.add32(ln);

      DEBUGASSERT(ln < 64);
      int cidxs[ln];
      for(int j = st; j<st+ln; j++)
        cidxs[j-st] = lists[j];
      std::sort(cidxs,cidxs+ln);

      for(int j = 0; j<ln; j++)
      {
        buffer.add64(conditionHashes[cidxs[j]].hash0);
        buffer.add64(conditionHashes[cidxs[j]].hash1);
      }
    }
  }
}

Hash128 Pattern::computeHash(ByteBuffer& buffer) const
{
  computeHashBytes(buffer);
  return MD5::get(buffer.bytes,buffer.numBytes);
}


//PATTERNRECORD----------------------------------------------------------------

PatternRecord::PatternRecord()
:pattern(),keyValues()
{

}

PatternRecord::PatternRecord(const Pattern& p, const map<string,string>& k)
:pattern(p),keyValues(k)
{

}

