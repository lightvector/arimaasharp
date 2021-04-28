
/*
 * board.cpp
 * Author: davidwu
 */

#include <sstream>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/offsets.h"
#include "../board/boardmovegen.h"

using namespace std;

//BOARD IMPLEMENTATION-------------------------------------------------------------

Board::Board()
{
  player = GOLD;
  step = 0;

  for(int i = 0; i<BSTARTIDX; i++)
  {
    _ownersBufferSpace[i] = NPLA+1;
    _piecesBufferSpace[i] = ELE+1;
  }

  for(int i = 0; i<BBUFSIZE-BSTARTIDX; i++)
  {
    owners[i] = NPLA+1;
    pieces[i] = ELE+1;
  }

  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      owners[loc] = NPLA;
      pieces[loc] = EMP;
    }
  }

  for(int i = 0; i<NUMTYPES; i++)
  {
    pieceMaps[0][i] = Bitmap();
    pieceMaps[1][i] = Bitmap();
    pieceCounts[0][i] = 0;
    pieceCounts[1][i] = 0;
  }

  for(int i = 0; i<4; i++)
  {
    trapGuardCounts[0][i] = 0;
    trapGuardCounts[1][i] = 0;
  }

  dominatedMap = Bitmap();
  frozenMap = Bitmap();

  posStartHash = 0;
  posCurrentHash = 0;
  sitCurrentHash = posStartHash ^ HASHPLA[player] ^ HASHSTEP[step];
  turnNumber = 0;

  goalTreeMove = ERRMOVE;
}

Board::Board(const Board& other)
{
  std::memcpy(this,&other,sizeof(Board));
}

void Board::operator=(const Board& other)
{
  if(this == &other)
    return;
  std::memcpy(this,&other,sizeof(Board));
}


void Board::setTurnNumber(int num)
{
  if(num < 0)
    Global::fatalError(string("Board::setTurnNumber: Invalid num ") + Global::intToString(num));

  turnNumber = num;
}

void Board::setPlaStep(pla_t p, char s)
{
  if(p != GOLD && p != SILV)
    Global::fatalError(string("Board::setPlaStep: Invalid player ") + Global::intToString(p));
  if(s < 0 || s > 3)
    Global::fatalError(string("Board::setPlaStep: Invalid step ") + Global::intToString(s));

  sitCurrentHash ^= HASHPLA[player];
  sitCurrentHash ^= HASHPLA[p];
  sitCurrentHash ^= HASHSTEP[step];
  sitCurrentHash ^= HASHSTEP[s];

  player = p;
  step = s;
}

void Board::setPiece(loc_t k, pla_t owner, piece_t piece)
{
  if(k >= BSIZE || (k & 0x88))
    Global::fatalError(string("Board::setPiece: Invalid location ") + Global::intToString(k));
  if(owner < 0 || owner > 2 || piece < 0 || piece >= NUMTYPES || (piece == EMP && owner != NPLA) || (piece != EMP && owner == NPLA))
    Global::fatalError(string("Board::setPiece: Invalid owner,piece (") + Global::intToString(owner) + string(",") + Global::intToString(piece) + string(")"));

  if(owners[k] != NPLA)
  {
    pieceMaps[owners[k]][pieces[k]].setOff(k);
    pieceMaps[owners[k]][0].setOff(k);
    pieceCounts[owners[k]][pieces[k]]--;
    pieceCounts[owners[k]][0]--;
    dominatedMap.setOff(k);
    frozenMap.setOff(k);
    posCurrentHash ^= HASHPIECE[owners[k]][pieces[k]][k];
    sitCurrentHash ^= HASHPIECE[owners[k]][pieces[k]][k];
    if(ADJACENTTRAP[k] != ERRLOC)
      trapGuardCounts[owners[k]][TRAPINDEX[ADJACENTTRAP[k]]]--;
  }

  owners[k] = owner;
  pieces[k] = piece;

  if(owner != NPLA)
  {
    pieceMaps[owner][piece].setOn(k);
    pieceMaps[owner][0].setOn(k);
    pieceCounts[owner][piece]++;
    pieceCounts[owner][0]++;
    if(isDominatedC(k))
    {
      dominatedMap.setOn(k);
      if(!isGuarded(owner,k))
        frozenMap.setOn(k);
    }
    posCurrentHash ^= HASHPIECE[owner][piece][k];
    sitCurrentHash ^= HASHPIECE[owner][piece][k];
    if(ADJACENTTRAP[k] != ERRLOC)
      trapGuardCounts[owner][TRAPINDEX[ADJACENTTRAP[k]]]++;
  }

  //Update surrounding freezement
  dominatedMap &= ~Board::RADIUS[1][k];
  frozenMap &= ~Board::RADIUS[1][k];
  if(isRealPla(owners[k S]) && isDominatedC(k S)) {dominatedMap.setOn(k S); if(!isGuarded(owners[k S],k S)) {frozenMap.setOn(k S);}}
  if(isRealPla(owners[k W]) && isDominatedC(k W)) {dominatedMap.setOn(k W); if(!isGuarded(owners[k W],k W)) {frozenMap.setOn(k W);}}
  if(isRealPla(owners[k E]) && isDominatedC(k E)) {dominatedMap.setOn(k E); if(!isGuarded(owners[k E],k E)) {frozenMap.setOn(k E);}}
  if(isRealPla(owners[k N]) && isDominatedC(k N)) {dominatedMap.setOn(k N); if(!isGuarded(owners[k N],k N)) {frozenMap.setOn(k N);}}
}

void Board::refreshStartHash()
{
  posStartHash = posCurrentHash;
}

bool Board::isStepLegal(step_t s) const
{
  if(s == ERRSTEP)
    return false;
  if(s == QPASSSTEP)
    return true;
  if(s == PASSSTEP)
    return step > 0 && posStartHash != posCurrentHash;

  loc_t k0 = gSrc(s);
  loc_t k1 = gDest(s);

  if(k1 == ERRLOC ||
     owners[k0] == NPLA ||
     owners[k1] != NPLA)
    return false;

  if(owners[k0] == player)
  {
    //use isFrozenC so that we don't rely on the bitmaps, which might not be updated in the middle of a move.
    if(isFrozenC(k0) || (pieces[k0] == RAB && !rabStepOkay(player,s)))
      return false;
  }

  return true;
}

//Does a point update of the dominated map for captured pieces
//and for the piece that moves. But NOT for surrounding pieces!
//Returns the owner of the piece that moved, or NPLA for passes
pla_t Board::makeStepRaw(step_t s, UndoData& udata)
{
  udata.s = s;
  udata.posStartHash = posStartHash;
  udata.posCurrentHash = posCurrentHash;
  udata.sitCurrentHash = sitCurrentHash;
  udata.step = step;
  udata.turn = turnNumber;
  udata.dominatedMap = dominatedMap;
  udata.frozenMap = frozenMap;
  udata.caploc = ERRLOC;

  //If this is the first step of a turn, refresh the start hash
  if(step == 0)
    posStartHash = posCurrentHash;

  //If the player to move is changing, update everything accordingly to flip the turn
  if(s == QPASSSTEP || s == PASSSTEP || step == 3)
  {
    sitCurrentHash ^= HASHSTEP[step] ^ HASHSTEP[0] ^ HASHPLA[0] ^ HASHPLA[1];
    player = gOpp(player);
    step = 0;
    turnNumber++;
  }
  //Otherwise, just increment the step.
  else
  {
    sitCurrentHash ^= HASHSTEP[step] ^ HASHSTEP[step+1];
    step++;
  }

  //If this was a pass, we are done
  if(s == QPASSSTEP || s == PASSSTEP)
    return NPLA;

  //Extract locations--------------------
  loc_t k0 = gSrc(s);
  loc_t k1 = gDest(s);

  //Update Arrays------------------------
  pla_t pla = owners[k0];
  piece_t piece = pieces[k0];
  owners[k1] = pla;
  pieces[k1] = piece;
  owners[k0] = NPLA;
  pieces[k0] = EMP;

  hash_t hashDiff = HASHPIECE[pla][piece][k0] ^ HASHPIECE[pla][piece][k1];

  //Piece Bitmaps-------------------------
  pieceMaps[pla][piece].setOff(k0);
  pieceMaps[pla][piece].setOn(k1);
  pieceMaps[pla][0].setOff(k0);
  pieceMaps[pla][0].setOn(k1);

  //Trap Guarding-------------------------
  if(ADJACENTTRAP[k0] != ERRLOC)
    trapGuardCounts[pla][TRAPINDEX[ADJACENTTRAP[k0]]]--;
  if(ADJACENTTRAP[k1] != ERRLOC)
    trapGuardCounts[pla][TRAPINDEX[ADJACENTTRAP[k1]]]++;

  //Captures-----------------------------
  loc_t caploc = ADJACENTTRAP[k0];
  if(caploc != ERRLOC)
  {
    pla_t capowner = owners[caploc];
    if(capowner != NPLA && trapGuardCounts[capowner][TRAPINDEX[caploc]] == 0)
    {
      piece_t cappiece = pieces[caploc];
      hashDiff ^= HASHPIECE[capowner][cappiece][caploc];
      pieceMaps[capowner][cappiece].setOff(caploc);
      pieceMaps[capowner][0].setOff(caploc);
      dominatedMap.setOff(caploc);
      pieceCounts[capowner][cappiece]--;
      pieceCounts[capowner][0]--;
      owners[caploc] = NPLA;
      pieces[caploc] = EMP;

      udata.caploc = caploc;
      udata.capowner = capowner;
      udata.cappiece = cappiece;
    }
  }

  //Update hash------------------------------------
  posCurrentHash ^= hashDiff;
  sitCurrentHash ^= hashDiff;

  //Domination and freezing----------------------
  //Not done here, recalcAllDomAndFreezeMaps needs to be called still.
  //It's not done here because we want to be able to cache and do it all at once for multistep moves.

  return pla;
}

void Board::undoStepRaw(const UndoData& udata)
{
  step_t s = udata.s;

  posStartHash = udata.posStartHash;
  posCurrentHash = udata.posCurrentHash;
  sitCurrentHash = udata.sitCurrentHash;
  dominatedMap = udata.dominatedMap;
  frozenMap = udata.frozenMap;

  if(step == 0)
  {
    player = gOpp(player);
    turnNumber--;
  }

  step = udata.step;

  if(s == QPASSSTEP || s == PASSSTEP)
    return;

  loc_t caploc = udata.caploc;
  if(caploc != ERRLOC)
  {
    pla_t capowner = udata.capowner;
    piece_t cappiece = udata.cappiece;
    owners[caploc] = capowner;
    pieces[caploc] = cappiece;
    pieceMaps[capowner][cappiece].setOn(caploc);
    pieceMaps[capowner][0].setOn(caploc);
    pieceCounts[capowner][cappiece]++;
    pieceCounts[capowner][0]++;
  }

  loc_t k0 = gSrc(s);
  loc_t k1 = gDest(s);

  pla_t pla = owners[k1];
  piece_t piece = pieces[k1];

  owners[k0] = pla;
  pieces[k0] = piece;
  owners[k1] = NPLA;
  pieces[k1] = EMP;
  pieceMaps[pla][piece].setOff(k1);
  pieceMaps[pla][piece].setOn(k0);
  pieceMaps[pla][0].setOff(k1);
  pieceMaps[pla][0].setOn(k0);

  if(ADJACENTTRAP[k1] != ERRLOC)
    trapGuardCounts[pla][TRAPINDEX[ADJACENTTRAP[k1]]]--;
  if(ADJACENTTRAP[k0] != ERRLOC)
    trapGuardCounts[pla][TRAPINDEX[ADJACENTTRAP[k0]]]++;
}

void Board::recalcDomMapFor(pla_t pla)
{
  pla_t opp = gOpp(pla);
  Bitmap plaDomMap = Bitmap();
  Bitmap oppPower  = pieceMaps[opp][ELE]; plaDomMap |= Bitmap::adj(oppPower) & pieceMaps[pla][CAM];
         oppPower |= pieceMaps[opp][CAM]; plaDomMap |= Bitmap::adj(oppPower) & pieceMaps[pla][HOR];
         oppPower |= pieceMaps[opp][HOR]; plaDomMap |= Bitmap::adj(oppPower) & pieceMaps[pla][DOG];
         oppPower |= pieceMaps[opp][DOG]; plaDomMap |= Bitmap::adj(oppPower) & pieceMaps[pla][CAT];
         oppPower |= pieceMaps[opp][CAT]; plaDomMap |= Bitmap::adj(oppPower) & pieceMaps[pla][RAB];

  dominatedMap |= plaDomMap;
}

//Just plain recomputes all the domination and freezing maps
void Board::recalcAllDomAndFreezeMaps()
{
  dominatedMap = Bitmap();
  recalcDomMapFor(GOLD);
  recalcDomMapFor(SILV);
  frozenMap = dominatedMap &
      ((pieceMaps[GOLD][0] & ~Bitmap::adj(pieceMaps[GOLD][0])) |
       (pieceMaps[SILV][0] & ~Bitmap::adj(pieceMaps[SILV][0])) );
}

//A version of makeStep that does the dom and freezing recomputation
void Board::makeStep(step_t s, UndoData& udata)
{
  pla_t pla = makeStepRaw(s,udata);
  if(pla != NPLA)
    recalcAllDomAndFreezeMaps();
}

bool Board::makeStepLegal(step_t s, UndoData& udata)
{
  if(!isStepLegal(s))
    return false;

  makeStep(s,udata);
  return true;
}

void Board::makeMove(move_t m)
{
  UndoData buffer;
  bool recalcFreeze = false;
  for(int i = 0; i<4; i++)
  {
    step_t s = getStep(m,i);
    if(s == ERRSTEP) break;
    pla_t pla = makeStepRaw(s,buffer);
    if(pla == NPLA) break;
    recalcFreeze = true;
  }

  if(recalcFreeze)
    recalcAllDomAndFreezeMaps();
}

void Board::makeMove(move_t m, UndoMoveData& uData)
{
  bool recalcFreeze = false;
  uData.size = 0;
  for(int i = 0; i<4; i++)
  {
    step_t s = getStep(m,i);
    if(s == ERRSTEP) break;

    int size = uData.size;
    pla_t pla = makeStepRaw(s,uData.uData[size]);
    uData.size++;

    if(pla == NPLA) break;
    recalcFreeze = true;
  }

  if(recalcFreeze)
    recalcAllDomAndFreezeMaps();
}

void Board::makeMove(move_t m, vector<UndoMoveData>& uDatas)
{
  int size = uDatas.size();
  uDatas.resize(size+1);
  makeMove(m,uDatas[size]);
}

//Helper for makeMoveLegal and variations.
//If legal, updates all of the pushpull legality tracking metadata
bool Board::stepLegalWithinMove(int i, step_t s, pla_t pla, loc_t& pushK, loc_t& pullK, piece_t& pushPower, piece_t& pullPower)
{
  //Fail if either the step is illegal OR the turn has wrapped around and we still are stepping.
  if(!isStepLegal(s) || (i > 0 && step == 0))
    return false;
  if(s == PASSSTEP || s == QPASSSTEP)
    return pushK == ERRLOC;

  loc_t k0 = gSrc(s);
  loc_t k1 = gDest(s);
  if(owners[k0] == gOpp(pla))
  {
    if(pushK != ERRLOC)
      return false;

    if(!(pullK == k1 && pullPower > pieces[k0]))
    {
      pushK = k0;
      pushPower = pieces[k0];
    }
    pullK = ERRLOC;
  }
  else
  {
    if(pushK != ERRLOC)
    {
      if(k1 != pushK || pieces[k0] <= pushPower)
        return false;
      pushK = ERRLOC;
    }
    else
    {
      pullK = k0;
      pullPower = pieces[k0];
    }
  }
  return true;
}

bool Board::makeMoveLegalNoUndo(move_t m)
{
  if(getStep(m,0) == ERRSTEP)
    return false;

  loc_t pushK = ERRLOC;
  loc_t pullK = ERRLOC;
  piece_t pushPower = 0;
  piece_t pullPower = 0;
  bool recalcFreeze = false;
  pla_t pla = player;

  for(int i = 0; i<4; i++)
  {
    step_t s = getStep(m,i);
    if(s == ERRSTEP)
      break;
    if(!stepLegalWithinMove(i,s,pla,pushK,pullK,pushPower,pullPower))
      return false;

    UndoData buffer;
    pla_t p = makeStepRaw(s,buffer);

    if(p == NPLA)
      break;
    recalcFreeze = true;
  }

  if(pushK != ERRLOC)
    return false;
  if(recalcFreeze)
    recalcAllDomAndFreezeMaps();
  return true;
}

bool Board::makeMoveLegalNoUndo(move_t m, UndoMoveData& uData)
{
  uData.size = 0;
  if(getStep(m,0) == ERRSTEP)
    return false;

  loc_t pushK = ERRLOC;
  loc_t pullK = ERRLOC;
  piece_t pushPower = 0;
  piece_t pullPower = 0;
  bool recalcFreeze = false;
  pla_t pla = player;

  for(int i = 0; i<4; i++)
  {
    step_t s = getStep(m,i);
    if(s == ERRSTEP)
      break;
    if(!stepLegalWithinMove(i,s,pla,pushK,pullK,pushPower,pullPower))
      return false;

    int size = uData.size;
    pla_t p = makeStepRaw(s,uData.uData[size]);
    uData.size++;

    if(p == NPLA)
      break;
    recalcFreeze = true;
  }

  if(pushK != ERRLOC)
    return false;
  if(recalcFreeze)
    recalcAllDomAndFreezeMaps();
  return true;
}

bool Board::makeMoveLegalNoUndo(move_t m, vector<UndoMoveData>& uDatas)
{
  int size = uDatas.size();
  uDatas.resize(size+1);
  return makeMoveLegalNoUndo(m,uDatas[size]);
}

void Board::makeStep(step_t s)
{
  UndoData udata;
  makeStep(s,udata);
}

bool Board::makeStepLegal(step_t s)
{
  UndoData udata;
  return makeStepLegal(s,udata);
}

bool Board::makeMoveLegal(move_t m, vector<UndoMoveData>& uDatas)
{
  bool suc = makeMoveLegalNoUndo(m,uDatas);
  if(suc)
    return true;
  undoMove(uDatas);
  return false;
}

void Board::undoStep(const UndoData& udata)
{
  undoStepRaw(udata);
}

void Board::undoMove(const UndoMoveData& uData)
{
  for(int i = uData.size-1; i >= 0; i--)
    undoStepRaw(uData.uData[i]);
}

void Board::undoMove(vector<UndoMoveData>& uDatas)
{
  int size = uDatas.size();
  DEBUGASSERT(size > 0);
  undoMove(uDatas[size-1]);
  uDatas.pop_back();
}

bool Board::makeMovesLegalNoUndo(const vector<move_t>& moves)
{
  for(int i = 0; i < (int)moves.size(); i++)
    if(!makeMoveLegalNoUndo(moves[i]))
      return false;
  return true;
}

bool Board::makeMovesLegalNoUndo(const vector<move_t>& moves, vector<UndoMoveData>& uDatas)
{
  for(int i = 0; i < (int)moves.size(); i++)
    if(!makeMoveLegalNoUndo(moves[i],uDatas))
      return false;
  return true;
}


pla_t Board::getWinner() const
{
  pla_t pla = player;
  pla_t opp = gOpp(pla);

  if(step == 0)
  {
    if(isGoal(opp)) return opp;
    if(isGoal(pla)) return pla;
    if(isRabbitless(pla)) return opp;
    if(isRabbitless(opp)) return pla;
    if(noMoves(pla)) return opp;
  }
  else
  {
    if(isGoal(pla)) return pla;
    if(isRabbitless(opp)) return pla;
    //Win if opp has no moves, pos different, opp no goal, we have rabbits
    if(noMoves(opp) && posCurrentHash != posStartHash && !isGoal(opp) && !isRabbitless(pla)) return pla;
  }
  return NPLA;
}

bool Board::noMoves(pla_t pla) const
{
  return BoardMoveGen::noMoves(*this,pla);
}

int Board::numStronger(pla_t pla, piece_t piece) const
{
  pla_t opp = gOpp(pla);
  int count = 0;
  for(piece_t i = NUMTYPES-1; i > piece; i--)
  {
    count += pieceCounts[opp][i];
  }
  return count;
}

int Board::numEqual(pla_t pla, piece_t piece) const
{
  return pieceCounts[gOpp(pla)][piece];
}

int Board::numWeaker(pla_t pla, piece_t piece) const
{
  pla_t opp = gOpp(pla);
  int count = 0;
  for(piece_t i = 1; i < piece; i++)
  {
    count += pieceCounts[opp][i];
  }
  return count;
}

loc_t Board::findElephant(pla_t pla) const
{
  Bitmap map = pieceMaps[pla][ELE];
  if(map.isEmpty())
    return ERRLOC;
  return map.nextBit();
}

void Board::initializeStronger(int pStronger[2][NUMTYPES]) const
{
  pStronger[SILV][ELE] = 0;
  pStronger[GOLD][ELE] = 0;
  pStronger[SILV][CAM] = pieceCounts[GOLD][ELE];
  pStronger[GOLD][CAM] = pieceCounts[SILV][ELE];
  pStronger[SILV][HOR] = pieceCounts[GOLD][CAM] + pStronger[SILV][CAM];
  pStronger[GOLD][HOR] = pieceCounts[SILV][CAM] + pStronger[GOLD][CAM];
  pStronger[SILV][DOG] = pieceCounts[GOLD][HOR] + pStronger[SILV][HOR];
  pStronger[GOLD][DOG] = pieceCounts[SILV][HOR] + pStronger[GOLD][HOR];
  pStronger[SILV][CAT] = pieceCounts[GOLD][DOG] + pStronger[SILV][DOG];
  pStronger[GOLD][CAT] = pieceCounts[SILV][DOG] + pStronger[GOLD][DOG];
  pStronger[SILV][RAB] = pieceCounts[GOLD][CAT] + pStronger[SILV][CAT];
  pStronger[GOLD][RAB] = pieceCounts[SILV][CAT] + pStronger[GOLD][CAT];
  pStronger[SILV][EMP] = pieceCounts[GOLD][RAB] + pStronger[SILV][RAB];
  pStronger[GOLD][EMP] = pieceCounts[SILV][RAB] + pStronger[GOLD][RAB];
}

void Board::initializeStrongerMaps(Bitmap pStrongerMaps[2][NUMTYPES]) const
{
    pStrongerMaps[SILV][ELE] = Bitmap();
    pStrongerMaps[GOLD][ELE] = Bitmap();
    pStrongerMaps[SILV][CAM] = pieceMaps[GOLD][ELE];
    pStrongerMaps[GOLD][CAM] = pieceMaps[SILV][ELE];
    pStrongerMaps[SILV][HOR] = pieceMaps[GOLD][CAM] | pStrongerMaps[SILV][CAM];
    pStrongerMaps[GOLD][HOR] = pieceMaps[SILV][CAM] | pStrongerMaps[GOLD][CAM];
    pStrongerMaps[SILV][DOG] = pieceMaps[GOLD][HOR] | pStrongerMaps[SILV][HOR];
    pStrongerMaps[GOLD][DOG] = pieceMaps[SILV][HOR] | pStrongerMaps[GOLD][HOR];
    pStrongerMaps[SILV][CAT] = pieceMaps[GOLD][DOG] | pStrongerMaps[SILV][DOG];
    pStrongerMaps[GOLD][CAT] = pieceMaps[SILV][DOG] | pStrongerMaps[GOLD][DOG];
    pStrongerMaps[SILV][RAB] = pieceMaps[GOLD][CAT] | pStrongerMaps[SILV][CAT];
    pStrongerMaps[GOLD][RAB] = pieceMaps[SILV][CAT] | pStrongerMaps[GOLD][CAT];
    pStrongerMaps[SILV][EMP] = pieceMaps[GOLD][RAB] | pStrongerMaps[SILV][RAB];
    pStrongerMaps[GOLD][EMP] = pieceMaps[SILV][RAB] | pStrongerMaps[GOLD][RAB];
}

loc_t Board::nearestDominator(pla_t pla, piece_t piece, loc_t loc, int maxRad) const
{
  Bitmap map;
  for(piece_t p = ELE; p > piece; p--)
    map |= pieceMaps[gOpp(pla)][p];

  if(maxRad > 15)
    maxRad = 15;
  map &= Board::DISK[maxRad][loc];

  if(map.isEmpty())
    return ERRLOC;

  for(int i = 1; i <= maxRad; i++)
  {
    Bitmap map2 = map & Board::RADIUS[i][loc];
    if(map2.hasBits())
      return map2.nextBit();
  }
  return ERRLOC;
}

//---------------------------------------------------------------------------------

int Board::getChanges(move_t move, loc_t* src, loc_t* dest) const
{
  int8_t newTrapGuardCounts[2][4];
  return getChanges(move,src,dest,newTrapGuardCounts);
}

int Board::getChanges(move_t move, loc_t* src, loc_t* dest, int8_t newTrapGuardCounts[2][4]) const
{
  //Copy over trap guarding counts so we can detect captures
  for(int i = 0; i<4; i++)
  {
    newTrapGuardCounts[0][i] = trapGuardCounts[0][i];
    newTrapGuardCounts[1][i] = trapGuardCounts[1][i];
  }

  //Iterate through the move and examine each step
  int num = 0;
  for(int i = 0; i<4; i++)
  {
    step_t s = getStep(move,i);
    if(s == ERRSTEP || s == PASSSTEP || s == QPASSSTEP)
      break;

    loc_t k0 = gSrc(s);
    loc_t k1 = gDest(s);

    //Check if it was a piece already moved
    int j;
    for(j = 0; j<num; j++)
    {
      //And if so, update it to its new location
      if(dest[j] == k0)
      {dest[j] = k1; break;}
    }
    //Otherwise
    if(j == num)
    {
      //Add a new entry
      src[j] = k0;
      dest[j] = k1;
      num++;
    }

    //Update trap guards and check for captures
    pla_t pla = owners[src[j]];
    if(ADJACENTTRAP[k0] != ERRLOC)
    {
      loc_t kt = ADJACENTTRAP[k0];
      newTrapGuardCounts[pla][TRAPINDEX[kt]]--;

      //0 defenders! Check for captures
      if(newTrapGuardCounts[pla][TRAPINDEX[kt]] == 0)
      {
        //Iterate over pieces to see if they are on the trap
        int m;
        bool pieceOnTrapMovedOff = false; //Did a piece begin this turn on the trap and move off of it?
        for(m = 0; m<num; m++)
        {
          //Is this piece currently on the trap and it is of the player whose piece stepped? If so, it dies!
          if(dest[m] == kt && owners[src[m]] == pla)
          {dest[m] = ERRLOC; break;}
          //If this piece began it's turn on the trap and moved off, mark so:
          if(src[m] == kt)
          {pieceOnTrapMovedOff = true;}
        }
        //No piece captured yet? Check if there is a piece beginning on the trap itself:
        if(m == num && owners[kt] == pla && !pieceOnTrapMovedOff)
        {
          src[num] = kt;
          dest[num] = ERRLOC;
          num++;
        }
      }
    }
    //Update trap guards at destination
    if(ADJACENTTRAP[k1] != ERRLOC)
      newTrapGuardCounts[pla][TRAPINDEX[ADJACENTTRAP[k1]]]++;
  }

  //Remove all null changes - where src and dest are the same
  int newNum = 0;
  for(int i = 0; i<num; i++)
  {
    if(src[i] != dest[i])
    {
      src[newNum] = src[i];
      dest[newNum] = dest[i];
      newNum++;
    }
  }
  return newNum;
}

int Board::getChangesInHindsightNoCaps(move_t move, loc_t* src, loc_t* dest) const
{
  //Iterate through the move and examine each step
  int num = 0;
  for(int i = 3; i>=0; i--)
  {
    step_t s = getStep(move,i);
    if(s == ERRSTEP || s == PASSSTEP || s == QPASSSTEP)
      continue;

    loc_t k0 = gSrc(s);
    loc_t k1 = gDest(s);

    //Check if it was a piece already moved
    int j;
    for(j = 0; j<num; j++)
    {
      //And if so, update it to its new location
      if(src[j] == k1)
      {src[j] = k0; break;}
    }
    //Otherwise
    if(j == num)
    {
      //This piece was captured during the move if the square is empty now,
      //or the piece was the second piece to come from that square.
      bool wasCaptured = owners[k1] == NPLA;
      if(!wasCaptured && Board::ISTRAP[k1])
        for(int a = 0; a<num; a++)
          if(dest[a] == k1)
          {wasCaptured = true; break;}

      src[j] = k0;
      dest[j] = wasCaptured ? ERRLOC : k1;
      num++;
    }
  }

  //Remove all null changes - where src and dest are the same, or where the piece was captured
  int newNum = 0;
  for(int i = 0; i<num; i++)
  {
    if(src[i] != dest[i] && dest[i] != ERRLOC)
    {
      src[newNum] = src[i];
      dest[newNum] = dest[i];
      newNum++;
    }
  }
  return newNum;
}

//Get the direction for the best approach to the dest. If equal, returns an arbitrary direction
int Board::getApproachDir(loc_t src, loc_t dest)
{
  int dx = gX(dest) - gX(src);
  int dy = gY(dest) - gY(src);
  int u = dx+dy;
  int v = dx-dy;
  if(u < 0)
    return v < 0 ? 1 : 0;
  else if(u > 0)
    return v > 0 ? 2 : 3;
  else
    return v > 0 ? 0 : 3;
}

//Get the direction for best retreat from dest. If equal, sets nothing. If only one retreat dir, sets nothing for dir2
void Board::getRetreatDirs(loc_t src, loc_t dest, int& dir1, int& dir2)
{
  int dx = gX(dest) - gX(src);
  int dy = gY(dest) - gY(src);
  if(dx == 0)
  {
    if(dy == 0)
      return;
    dir1 = (dy > 0) ? 0 : 3;
  }
  else if(dy == 0)
  {
    dir1 = (dx > 0) ? 1 : 2;
  }
  else
  {
    dir1 = (dy > 0) ? 0 : 3;
    dir2 = (dx > 0) ? 1 : 2;
  }
}

hash_t Board::sitHashAfterLegal(move_t move) const
{
  Board copy = *this;
  bool suc = copy.makeMoveLegalNoUndo(move);
  if(!suc)
    Global::fatalError("sitHashAfterLegal illegal move");
  return copy.sitCurrentHash;
}

//TESTING------------------------------------------------------------------------

bool Board::testConsistency(ostream& out) const
{
  hash_t hashVal = 0;

  for(int i = 0; i<BSIZE; i++)
  {
    if(i & 0x88)
      continue;
    //Piece array consistency
    if(owners[i] == NPLA && pieces[i] != EMP)
    {out << "Inconsistent board\n" << *this << endl; return false;}
    if(owners[i] != NPLA && pieces[i] == EMP)
    {out << "Inconsistent board\n" << *this << endl; return false;}

    //Bitmap consistency
    for(int j = 0; j<2; j++)
    {
      for(int k = 1; k<NUMTYPES; k++)
      {
        if((owners[i] == j && pieces[i] == k) != pieceMaps[j][k].isOne(i))
        {out << "Inconsistent board\n" << "Piecemap disagrees with array" << endl; return false;}
        if((owners[i] == j) != pieceMaps[j][0].isOne(i))
        {out << "Inconsistent board\n" << "Allmap disagrees with array" << endl; return false;}
        if(pieceMaps[j][k].countBits() != pieceCounts[j][k])
        {out << "Inconsistent board\n" << "Bitmap count disagrees with piececount" << endl;
        out << pieceMaps[j][k].countBits() << " " << (int)pieceCounts[j][k] << endl;
        out << pieceMaps[j][k] << endl; out << j << " " << k << endl; out << *this << endl; return false;}
      }
    }

    //Freezing
    if(owners[i] != NPLA)
    {
      if(isFrozen(i) != isFrozenC(i) || isFrozenC(i) == isThawedC(i) || isThawedC(i) != isThawed(i))
      {out << "Inconsistent board\n" << "Frozen/thawed error" << Board::writeLoc(i) << endl; out << *this << endl; out << frozenMap << endl; return false;}
      if(isFrozen(i) != (!isGuarded(owners[i],i) && isDominatedC(i)))
      {out << "Inconsistent board\n" << "Frozen disagrees with dominated/guarded " << i << endl; out << *this << endl; return false;}
      if(isDominated(i) != isDominatedC(i))
      {out << "Inconsistent board\n" << "Dominated error " << i << endl; out << *this << endl; return false;}
    }
    else
    {
      if(isFrozen(i) || isDominated(i))
      {out << "Inconsistent board\n" << "Frozen/dominated empty square " << i << endl; out << *this << endl; return false;}
    }

    //Traps
    if(i == TRAP0 || i == TRAP1 || i == TRAP2 || i == TRAP3)
    {
      if(!ISTRAP[i] || !isTrap(i))
      {out << "Inconsistent board\n" << "ISTRAP array error" << endl; return false;}
      if(owners[i] != NPLA && !isGuarded(owners[i],i))
      {out << "Inconsistent board\n" << "Trap error" << endl; out << *this << endl; return false;}
      if(isGuarded2(GOLD,i) != isTrapSafe2(GOLD,i))
      {out << "Inconsistent board\n" << "gold isGuarded2 != isTrapSafe2 at " << i << endl; out << *this << endl; return false;}
      if(isGuarded2(SILV,i) != isTrapSafe2(SILV,i))
      {out << "Inconsistent board\n" << "silver isGuarded2 != isTrapSafe2 at " << i << endl; out << *this << endl; return false;}

      int gCount = 0;
      int sCount = 0;
      for(int dir = 0; dir<4; dir++)
      {
        if(owners[i + Board::ADJOFFSETS[dir]] == GOLD)
          gCount++;
        else if(owners[i + Board::ADJOFFSETS[dir]] == SILV)
          sCount++;
      }
      if(gCount != trapGuardCounts[GOLD][Board::TRAPINDEX[i]])
      {out << "Inconsistent board\n" << "gold trapGuardCounts != actual count at " << i << endl; out << *this << endl; return false;}
      if(sCount != trapGuardCounts[SILV][Board::TRAPINDEX[i]])
      {out << "Inconsistent board\n" << "silver trapGuardCounts != actual count at " << i << endl; out << *this << endl; return false;}
    }

    if(owners[i] != NPLA)
    {hashVal ^= HASHPIECE[owners[i]][pieces[i]][i];}
  }

  //Hash consistency
  if(posCurrentHash != hashVal)
  {out << "Inconsistent board\n" << "Hash error posCur != calcualated" << endl; out << *this << endl;; return false;}
  if((posCurrentHash ^ HASHPLA[player] ^ HASHSTEP[step]) != sitCurrentHash)
  {out << "Inconsistent board\n" << "Hash error posCur + sithashs != sitCur" << endl; out << *this << endl;; return false;}

  //Piece counts
  int pieceCountMaxes[NUMTYPES] = {16,8,2,2,2,1,1};
  for(int i = 0; i < NUMTYPES; i++)
  {
    if(pieceCounts[0][i] > pieceCountMaxes[i] || pieceCounts[0][i] < 0)
    {out << "Inconsistent board\n" << "Piece count > max or < 0 for " << writePiece(0,i) << endl; out << *this << endl; return false;}
    if(pieceCounts[1][i] > pieceCountMaxes[i] || pieceCounts[1][i] < 0)
    {out << "Inconsistent board\n" << "Piece count > max or < 0 for " << writePiece(1,i) << endl; out << *this << endl; return false;}
  }

  return true;
}

bool Board::areIdentical(const Board& b0, const Board& b1)
{
  for(int i = 0; i<BSTARTIDX; i++)
  {
    if(b0._ownersBufferSpace[i] != b1._ownersBufferSpace[i])
      return false;
    if(b0._piecesBufferSpace[i] != b1._piecesBufferSpace[i])
      return false;
  }

  for(int i = 0; i<BBUFSIZE-BSTARTIDX; i++)
  {
    if(b0.owners[i] != b1.owners[i])
      return false;
    if(b0.pieces[i] != b1.pieces[i])
      return false;
  }

  for(int i = 0; i<NUMTYPES; i++)
  {
    if(b0.pieceMaps[GOLD][i] != b1.pieceMaps[GOLD][i])
      return false;
    if(b0.pieceMaps[SILV][i] != b1.pieceMaps[SILV][i])
      return false;
    if(b0.pieceCounts[GOLD][i] != b1.pieceCounts[GOLD][i])
      return false;
    if(b0.pieceCounts[SILV][i] != b1.pieceCounts[SILV][i])
      return false;
  }
  if(b0.dominatedMap != b1.dominatedMap)
    return false;
  if(b0.frozenMap != b1.frozenMap)
    return false;
  if(b0.player != b1.player)
    return false;
  if(b0.step != b1.step)
    return false;
  if(b0.turnNumber != b1.turnNumber)
    return false;

  for(int i = 0; i<4; i++)
  {
    if(b0.trapGuardCounts[GOLD][i] != b1.trapGuardCounts[GOLD][i])
      return false;
    if(b0.trapGuardCounts[SILV][i] != b1.trapGuardCounts[SILV][i])
      return false;
  }

  if(b0.posStartHash != b1.posStartHash)
    return false;
  if(b0.posCurrentHash != b1.posCurrentHash)
    return false;
  if(b0.sitCurrentHash != b1.sitCurrentHash)
    return false;

  return true;
}

bool Board::pieceMapsAreIdentical(const Board& b0, const Board& b1)
{
  for(piece_t piece = 0; piece<NUMTYPES; piece++)
  {
    if(b0.pieceMaps[SILV][piece] != b1.pieceMaps[SILV][piece] ||
       b0.pieceMaps[GOLD][piece] != b1.pieceMaps[GOLD][piece])
      return false;
  }
  return true;
}

