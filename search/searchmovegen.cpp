/*
 * searchmovegen.cpp
 * Author: davidwu
 */

#include <set>
#include "../core/global.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/boardmovegen.h"
#include "../board/boardtrees.h"
#include "../board/boardtreeconst.h"
#include "../search/searchparams.h"
#include "../search/searchutils.h"
#include "../search/searchmovegen.h"
#include "../search/search.h"

//FULL MOVE GENERATION (ROOT) -------------------------------------------------------------------

static void genFullMoveHelper(Board& b, const BoardHistory& hist,
    pla_t pla, vector<move_t>& moves, move_t moveSoFar,
    int nsSoFar, int maxSteps, hash_t prevHashes[5], int numPrevHashes,
    bool winLossPrune, bool restrictToRelTactics, int rootMaxNonRelTactics,
    ExistsHashTable* fullMoveHash, const set<hash_t>& excludedHashSet, const vector<move_t>* excludedStepSets)
{
  //Terminate if you've generated for as many steps as you're allowed, or the turn has switched.
  if(maxSteps <= 0 || b.player != pla)
  {
    //If you're winloss pruning, and the turn has switched and now your opponent can win, prune.
    if(winLossPrune && b.player != pla && !b.isGoal(pla) && !b.isRabbitless(b.player) &&
        (BoardTrees::goalDist(b,b.player,4) < 5 || BoardTrees::canElim(b,b.player,4)))
      return;
    //If you 3x-repeated, then prune.
    if(b.player != pla && BoardHistory::isThirdRepetition(b,hist))
      return;
    //If we're forbidden from making this move, then prune
    if(excludedHashSet.size() > 0 && excludedHashSet.find(b.sitCurrentHash) != excludedHashSet.end())
      return;
    //If we match any step set, then prune
    if(excludedStepSets != NULL)
    {
      const vector<move_t>& excl = *excludedStepSets;
      int size = excl.size();
      for(int i = 0; i<size; i++)
      {
        bool missedStepMatch = false;
        //Iter steps in set
        move_t stepSet = excl[i];
        for(int j = 0; j<4; j++)
        {
          step_t step = getStep(stepSet,j);
          if(step == ERRSTEP)
            continue;
          bool moveMatchesStep = false;
          for(int k = 0; k<4; k++)
          {
            step_t moveStep = getStep(moveSoFar,k);
            if(moveStep == ERRSTEP)
              continue;
            if(moveStep == step)
            {
              moveMatchesStep = true;
              break;
            }
          }
          if(!moveMatchesStep)
          {
            missedStepMatch = true;
            break;
          }
        }
        if(!missedStepMatch)
          return;
      }
    }

    //Keep going!
    moves.push_back(moveSoFar);
    return;
  }
  move_t mv[1024];

  int num = -1;
  int numRelTactics = -1;
  if(restrictToRelTactics)
  {
    num = SearchMoveGen::genRelevantTactics(b,true,mv,NULL);
    numRelTactics = num;

    if(rootMaxNonRelTactics > 0)
    {
      if(b.step < 3)
        num += BoardMoveGen::genPushPulls(b,pla,mv+num);
      num += BoardMoveGen::genSteps(b,pla,mv+num);
    }

    DEBUGASSERT(num < 1024);
    if(b.step != 0)
      mv[num++] = PASSMOVE;
  }
  else
  {
    if(winLossPrune)
    {
      num = SearchMoveGen::genWinDefMovesIfNeeded(b,mv,NULL,4-b.step);
      //No possible defenses for loss
      if(num == 0)
        return;
    }
    //Regular movegen
    if(num == -1)
    {
      num = 0;
      if(b.step < 3)
        num += BoardMoveGen::genPushPulls(b,pla,mv+num);
      num += BoardMoveGen::genSteps(b,pla,mv+num);
      if(b.step != 0 && b.posCurrentHash != b.posStartHash)
        mv[num++] = PASSMOVE;
    }
  }

  UndoMoveData uData;
  for(int i = 0; i<num; i++)
  {
    move_t move = mv[i];
    b.makeMove(move,uData);

    //Check if we returned to a previous position hash
    bool returnsToPrevPos = false;
    //Note that with reltactics, rarely we will actually get a move that returns to the immediately previous pos!
    for(int j = 0; j<numPrevHashes; j++)
    {
      //Allow pure pass moves when legal
      if(j == numPrevHashes-1 && move == PASSMOVE)
        break;
      if(b.posCurrentHash == prevHashes[j])
      {returnsToPrevPos = true; break;}
    }

    //Or if we transposed to a situation hash that we already movegenned for
    if(returnsToPrevPos || (fullMoveHash->lookup(b) && (!restrictToRelTactics || b.player != pla)))
    {b.undoMove(uData); continue;}

    //Otherwise record the current hash as one taken care of for movegenning, and mark this
    //position for later recursions to avoid repeating
    fullMoveHash->record(b);
    prevHashes[numPrevHashes] = b.posCurrentHash;

    int ns = numStepsInMove(mv[i]);
    move_t newMoveSoFar = concatMoves(moveSoFar,mv[i],nsSoFar);
    int newRootMaxNonRelTactics = i >= numRelTactics ? rootMaxNonRelTactics-1 : rootMaxNonRelTactics;
    genFullMoveHelper(b,hist,pla,moves,newMoveSoFar,nsSoFar+ns,maxSteps-ns,
        prevHashes,numPrevHashes+1,winLossPrune,restrictToRelTactics,newRootMaxNonRelTactics,fullMoveHash,excludedHashSet,excludedStepSets);
    b.undoMove(uData);
  }
}

void SearchMoveGen::genFullMoves(const Board& board, const BoardHistory& hist,
    vector<move_t>& moves, int maxSteps,
    bool winLossPrune, int rootMaxNonRelTactics,
    ExistsHashTable* fullMoveHash, const vector<hash_t>* excludedHashes, const vector<move_t>* excludedStepSets)
{
  Board b = board;

  //If we find a move that wins immediately, then just do that one
  if(winLossPrune)
  {
    move_t winningMove = getFullGoalMove(b);
    if(winningMove != ERRMOVE)
    {moves.push_back(winningMove); return;}
  }

  if(winLossPrune && (BoardTrees::goalDist(b,b.player,maxSteps) <= 4 || BoardTrees::canElim(b,b.player,maxSteps)))
    winLossPrune = false;

  set<hash_t> excludedHashSet;
  if(excludedHashes != NULL)
  {
    int size = excludedHashes->size();
    for(int i = 0; i<size; i++)
      excludedHashSet.insert((*excludedHashes)[i]);
  }

  hash_t prevHashes[5];
  prevHashes[0] = (b.step == 0 ? b.posCurrentHash : b.posStartHash);
  int numPrevHashes = 1;
  fullMoveHash->clear();
  genFullMoveHelper(b, hist, b.player, moves, ERRMOVE, 0, maxSteps,
      prevHashes, numPrevHashes, winLossPrune, rootMaxNonRelTactics < 4, rootMaxNonRelTactics, fullMoveHash, excludedHashSet, excludedStepSets);

  //If we failed to generate any moves (presumably because they were all losses), then try again without pruning
  if(winLossPrune && moves.size() == 0)
  {
    fullMoveHash->clear();
    genFullMoveHelper(b, hist, b.player, moves, ERRMOVE, 0, maxSteps,
        prevHashes, numPrevHashes, false, rootMaxNonRelTactics < 4, rootMaxNonRelTactics, fullMoveHash, excludedHashSet, excludedStepSets);
  }
}

move_t SearchMoveGen::getFullGoalMove(Board& b)
{
  pla_t pla = b.player;
  int numSteps = 4-b.step;
  if(BoardTrees::goalDist(b,pla,numSteps) < 5)
  {
    Board copy = b;
    move_t moveSoFar = ERRMOVE;
    int soFarLen = 0;
    while(BoardTrees::goalDist(copy,pla,numSteps-soFarLen) > 0)
    {
      int ns = numStepsInMove(copy.goalTreeMove);
      moveSoFar = concatMoves(moveSoFar,copy.goalTreeMove,soFarLen);
      soFarLen += ns;

      bool suc = copy.makeMoveLegalNoUndo(copy.goalTreeMove);
      if(!suc)
        return ERRMOVE;

      if(soFarLen > numSteps || ns == 0)
        return ERRMOVE;
    }

    if(soFarLen > numSteps)
      return ERRMOVE;

    if(copy.isGoal(pla))
    {
      if(soFarLen < 4-b.step)
        return concatMoves(moveSoFar,PASSMOVE,soFarLen);
      else
        return moveSoFar;
    }
  }
  return ERRMOVE;
}


//REGULAR MOVE GENERATION--------------------------------------------------------------

int SearchMoveGen::genRegularMoves(const Board& b, move_t* mv)
{
  pla_t pla = b.player;
  int num = 0;
  //Generate pushpulls if enough steps left
  if(b.step < 3)
  {num += BoardMoveGen::genPushPulls(b,pla,mv);}

  //Generate steps
  num += BoardMoveGen::genSteps(b,pla,mv+num);
  return num;
}


//CAPTURE-RELATED GENERATION------------------------------------------------------

int SearchMoveGen::genCaptureMoves(Board& b, int numSteps, move_t* mv, int* hm)
{
  int num = 0;
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    //TODO try not suicide pruning in the cap generation (but maybe still suicide extending)
    loc_t kt = Board::TRAPLOCS[trapIndex];
    num += BoardTrees::genCapsFull(b,b.player,numSteps,2,true,kt,mv+num,hm+num);
  }
  return num;
}

int SearchMoveGen::genCaptureDefMoves(Board& b, int numSteps, bool moreBranchy, move_t* mv, int* hm)
{
  bool oppCanCap[4];
  Bitmap wholeCapMap;
  return genCaptureDefMoves(b,numSteps,moreBranchy,mv,hm,oppCanCap,wholeCapMap);
}


//Generate various kinds of capture defenses.
//Also fill in wholeCapMapBuf with all own pieces under capture threat and indicate which traps were threatened.
int SearchMoveGen::genCaptureDefMoves(Board& b, int numSteps, bool moreBranchy, move_t* mv, int* hm,
    bool oppCanCap[4], Bitmap& wholeCapMapBuf)
{
  int num = 0;
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  Bitmap pStrongerMaps[2][NUMTYPES];
  b.initializeStrongerMaps(pStrongerMaps);

  Bitmap capMaps[4];
  Bitmap wholeCapMap;
  piece_t biggestThreats[4];

  //Try all the traps to detect captures
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    loc_t kt = Board::TRAPLOCS[trapIndex];
    int capDist;
    oppCanCap[trapIndex] = BoardTrees::canCaps(b,opp,4,kt,capMaps[trapIndex],capDist);
    if(oppCanCap[trapIndex])
      wholeCapMap |= capMaps[trapIndex];

    piece_t biggestThreat = EMP;
    Bitmap capMap = capMaps[trapIndex];
    while(capMap.hasBits())
    {
      loc_t loc = capMap.nextBit();
      if(b.pieces[loc] > biggestThreat)
        biggestThreat = b.pieces[loc];
    }
    biggestThreats[trapIndex] = biggestThreat;
  }

  int branchyness = moreBranchy ? 2 : 0;

  //Gen all trap defenses
  int shortestGoodDef[4]; //Shortest "good" defense. No need to try runaways unless they're shorter.
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    if(!oppCanCap[trapIndex])
      continue;
    loc_t kt = Board::TRAPLOCS[trapIndex];

    //TODO I think we really want some form of futility pruning here, where after generating all the
    //caps and capdefs, and after trying the killers and getting the eval back from the qpassmove,
    //we prune if it looks like the material defecit will be too large to overcome.

    //TODO if a piece can step off the trap without being dominated at its new location, and also
    //no defender of the trap is dominated, perhaps
    //only generate such steps to limit the branching factor.
    int defNum = BoardTrees::genTrapDefs(b,pla,kt,numSteps,pStrongerMaps,mv+num);
    shortestGoodDef[trapIndex] = BoardTrees::shortestGoodTrapDef(b,pla,kt,mv+num,defNum,wholeCapMap);

    //Prune trap defenses to include nothing longer than the shortest good one
    int newDefNum = 0;
    for(int i = 0; i<defNum; i++)
    {
      if(numStepsInMove(mv[num+i]) <= shortestGoodDef[trapIndex] + branchyness)
      {
        mv[num+newDefNum] = mv[num+i];
        newDefNum++;
      }
    }

    if(hm != NULL)
    {
      for(int i = 0; i<newDefNum; i++)
        hm[num+i] = biggestThreats[trapIndex];
    }

    num += newDefNum;
  }

  //Runaways defense
  for(int trapIndex = 0; trapIndex < 4; trapIndex++)
  {
    if(!oppCanCap[trapIndex])
      continue;
    Bitmap capMap = capMaps[trapIndex];

    int maxFromDef = shortestGoodDef[trapIndex]-1 + branchyness;
    //Already a good defense - no need to go more
    if(maxFromDef <= 0)
      continue;
    int maxRunawaySteps = maxFromDef < numSteps ? maxFromDef : numSteps;

    while(capMap.hasBits())
    {
      loc_t ploc = capMap.nextBit();

      //No use running away on trap - was covered when we tried to add defenders
      if(Board::ISTRAP[ploc])
        continue;

      int runawayNum = BoardTrees::genRunawayDefs(b,pla,ploc,maxRunawaySteps,false,mv+num);
      if(hm != NULL)
      {
        for(int i = 0; i<runawayNum; i++)
          hm[num+i] = b.pieces[ploc];
      }
      num += runawayNum;
    }
  }

  wholeCapMapBuf = wholeCapMap;
  return num;
}

//BLOCKADE-RELATED GENERATION------------------------------------------------------


//Test if adj is a blockaded direction for an owner-owned ele.
//Adj assumed to be adjancent to an owner-owned elephant.
static bool isBlockedVsEleAt(const Board& b, pla_t owner, loc_t adj)
{
  if(b.owners[adj] == NPLA)
  {if(!b.isTrapSafe2(owner,adj)) return true;}
  else if(b.owners[adj] == gOpp(owner))
  {if(b.pieces[adj] >= ELE || !b.isTrapSafe2(owner,adj) || !b.isOpen(adj)) return true;}
  else
  {if(!b.isOpenToStep(adj) && !b.isOpenToPush(adj)) return true;}
  return false;
}

//Assuming not blockaded already
static int definiteMinBlockadeSteps(const Board& b, pla_t owner, loc_t centerLoc, Bitmap plaEleAdjMap)
{
  //If it's empty, it will take at least 2 steps to blockade unless the elephant seals
  //it off in one shot
  if(b.owners[centerLoc] == NPLA)
  {
    if(b.isTrapSafe2(owner,centerLoc))
    {
      if(!plaEleAdjMap.isOne(centerLoc))
        return 2;
    }
  }
  //If it's a weak holder with 2 open spots, the same
  else if(b.owners[centerLoc] == gOpp(owner))
  {
    if(b.isTrapSafe2(owner,centerLoc))
    {
      if(b.pieces[centerLoc] != ELE && b.isOpen2(centerLoc))
        return 2;
    }
  }
  //If it's a nonrabbit opp open enough also 2
  else
  {
    if(b.pieces[centerLoc] != RAB && b.isOpen2(centerLoc))
      return 2;
  }
  return 1;
}

//Pla is the holder, as usual
//0 - total blockade
//1 - partial blockade
//2 - not blockade
static int isEleMostlyImmoStatus(const Board& b, pla_t pla, loc_t loc, int maxRelevantStatus)
{
  if(maxRelevantStatus <= 0)
  {
    Bitmap empty = ~(b.pieceMaps[GOLD][0] | b.pieceMaps[SILV][0]);
    if((empty & Board::RADIUS[1][loc] & ~Bitmap::BMPTRAPS).hasBits())
      return 2;
  }

  pla_t opp = gOpp(pla);
  int x = gX(loc);
  int y = gY(loc);

  { //Center N/S
    loc_t adj = y < 4 ? loc N : loc S;
    if(!isBlockedVsEleAt(b,opp,adj))
      return 2;
  }
  { //Center E/W
    loc_t adj = x < 4 ? loc E : loc W;
    if(!isBlockedVsEleAt(b,opp,adj))
      return 2;
  }
  bool edgeUnblocked = false;
  if(x != 0 && x != 7) //Edge E/W
  {
    loc_t adj = x < 4 ? loc W : loc E;
    if(!isBlockedVsEleAt(b,opp,adj))
    {
      if(maxRelevantStatus <= 0 || !Board::ISEDGE[loc])
        return 2;
      edgeUnblocked = true;
    }
  }
  if(y != 0 && y != 7) //Edge N/S
  {
    loc_t adj = y < 4 ? loc S : loc N;
    if(!isBlockedVsEleAt(b,opp,adj))
    {
      if(maxRelevantStatus <= 0 || edgeUnblocked)
        return 2;
      edgeUnblocked = true;
    }
  }

  return (int)edgeUnblocked;
}

static int genBlockadeMovesRec(Board& b, pla_t pla, loc_t oppEleLoc, move_t moveSoFar, int nsSoFar, int maxSteps,
    int& bestSoFar, uint64_t posStartHash, move_t* mv, int totalNum)
{
  //With < 3 steps left, it's impossible to full-blockade an elephant if it has 2 empty squares next to it
  if(maxSteps < 3 && bestSoFar <= 0 &&
      !(~(b.pieceMaps[GOLD][0] | b.pieceMaps[SILV][0]) & Board::RADIUS[1][oppEleLoc] & ~Bitmap::BMPTRAPS).isEmptyOrSingleBit())
    return totalNum;

  //With < 2 steps left, test if it's possible to prove that blockade cant happen in one step
  Bitmap plaEleAdjMap = Bitmap::adj(b.pieceMaps[pla][ELE]);
  if(maxSteps < 2)
  {
    int y = gY(oppEleLoc);
    loc_t adj1 = y < 4 ? oppEleLoc N : oppEleLoc S;
    int definiteMin1 = definiteMinBlockadeSteps(b,gOpp(pla),adj1,plaEleAdjMap);
    if(definiteMin1 >= 2)
      return totalNum;
    int x = gX(oppEleLoc);
    loc_t adj2 = x < 4 ? oppEleLoc E : oppEleLoc W;
    int definiteMin2 = definiteMinBlockadeSteps(b,gOpp(pla),adj2,plaEleAdjMap);
    if(definiteMin2 >= 2)
      return totalNum;

    if(bestSoFar <= 0)
    {
      loc_t adj3 = x < 4 ? oppEleLoc W : oppEleLoc E;
      int definiteMin3 = definiteMinBlockadeSteps(b,gOpp(pla),adj3,plaEleAdjMap);
      if(definiteMin3 >= 2)
        return totalNum;
      loc_t adj4 = y < 4 ? oppEleLoc S : oppEleLoc N;
      int definiteMin4 = definiteMinBlockadeSteps(b,gOpp(pla),adj4,plaEleAdjMap);
      if(definiteMin4 >= 2)
        return totalNum;
    }
  }

  move_t submv[256];
  int num = 0;
  //TODO can strengthen relevance a lot more...
  Bitmap relevant = Board::DISK[2][oppEleLoc];
  if(b.step < 3 && maxSteps > 1)
    num += BoardMoveGen::genPushPullsInvolving(b,pla,submv+num,relevant);

  //With <= 2 steps left, it's never good to step away from the elephant unless stepping off trap
  if(maxSteps <= 2)
  {
    Bitmap outof;
    if(bestSoFar <= 0)
    {
      outof = ~Board::RADIUS[1][oppEleLoc] | Bitmap::BMPTRAPS;
    }
    else //We have to be more careful if partial blockades are okay
    {
      int y = gY(oppEleLoc);
      int x = gX(oppEleLoc);
      outof =
        ~(Bitmap::makeLoc(y < 4 ? oppEleLoc N : oppEleLoc S) |
          Bitmap::makeLoc(x < 4 ? oppEleLoc E : oppEleLoc W)) | Bitmap::BMPTRAPS;
    }
    if(maxSteps == 2)
      outof |= plaEleAdjMap;
    num += BoardMoveGen::genStepsIntoAndOutof(b,pla,submv+num,relevant,outof);
  }
  else
    num += BoardMoveGen::genStepsInto(b,pla,submv+num,relevant);

  UndoMoveData uData;
  for(int i = 0; i<num; i++)
  {
    b.makeMove(submv[i],uData);
    move_t newMoveSoFar = concatMoves(moveSoFar,submv[i],nsSoFar);

    int immoStatus = isEleMostlyImmoStatus(b,pla,oppEleLoc,bestSoFar);
    if(immoStatus < bestSoFar)
    {bestSoFar = immoStatus; totalNum = 0;}
    if(immoStatus <= bestSoFar)
    {mv[totalNum] = newMoveSoFar; totalNum++; if(totalNum >= 128) {b.undoMove(uData); return totalNum;}}
    else if(b.posCurrentHash != posStartHash && pla == b.player)
    {
      int ns = numStepsInMove(submv[i]);
      if(maxSteps - ns > 0)
      {
        totalNum = genBlockadeMovesRec(b,pla,oppEleLoc,newMoveSoFar,nsSoFar+ns,maxSteps-ns,bestSoFar,posStartHash,mv,totalNum);
        if(totalNum >= 128) {b.undoMove(uData); return totalNum;}
      }
    }
    b.undoMove(uData);
  }

  return totalNum;
}

static const int numPiecesToConsiderBlockade[BSIZE] =
{
   4,5,6,6,6,6,5,4,  0,0,0,0,0,0,0,0,
   5,6,6,0,0,6,6,5,  0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
   5,6,6,0,0,6,6,5,  0,0,0,0,0,0,0,0,
   4,5,6,6,6,6,5,4,  0,0,0,0,0,0,0,0,
};

int SearchMoveGen::genBlockadeMoves(Board& b, move_t* mv)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  loc_t oppEleLoc = b.findElephant(opp);

  if(oppEleLoc == ERRLOC || numPiecesToConsiderBlockade[oppEleLoc] == 0)
    return 0;
  if((Board::DISK[2][oppEleLoc] & b.pieceMaps[pla][0]).countBits() < numPiecesToConsiderBlockade[oppEleLoc])
    return 0;
  int curStatus = isEleMostlyImmoStatus(b,pla,oppEleLoc, 2);
  if(curStatus == 0)
    return 0;

  int bestSoFar = curStatus - 1;
  hash_t posStartHash = (b.step == 0 ? b.posCurrentHash : b.posStartHash);
  int numSteps = 4-b.step;
  for(int i = 1; i<=numSteps; i++)
  {
    int num = genBlockadeMovesRec(b,pla,oppEleLoc,ERRMOVE,0,i,bestSoFar,posStartHash,mv,0);
    if(num > 0)
    {
      if(bestSoFar == 0 || i == numSteps)
        return num;

      //Otherwise, bestSoFar is 1.
      //Search the next iters for a complete blockade
      bestSoFar = 0;
      for(i++; i<=numSteps; i++)
      {
        int numComp = genBlockadeMovesRec(b,pla,oppEleLoc,ERRMOVE,0,i,bestSoFar,posStartHash,mv,0);
        if(numComp > 0)
          return numComp;
      }

      //Return what we have
      return num;
    }
  }
  return 0;
}

//GOAL THREATS ----------------------------------------------------------

static int genSevereGoalThreats(Board& b, int numSteps, move_t* mv, int* hm)
{
  //Currently only expects to be used for qdepth 0, which cannot happen with 4 steps left.
  if(numSteps > 3)
    numSteps = 3;
  int num = BoardTrees::genSevereGoalThreats(b,b.player,numSteps,mv);
  for(int i = 0; i<num; i++)
    hm[i] = 0;
  return num;
}

//REVERSIBLE MOVES ------------------------------------------------------
static const int DYDIR[2] =
{MS,MN};

int SearchMoveGen::genReverseMoves(const Board& b, const BoardHistory& boardHistory, int cDepth, move_t* mv)
{
  //Ensure start of turn and deep enough
  if(b.step != 0 || cDepth < 4)
    return 0;

  //Ensure we have history
  int lastTurn = b.turnNumber-1;
  if(lastTurn < boardHistory.minTurnNumber)
    return 0;

  //Arrays to track pieces and where they went
  loc_t src[4];
  loc_t dest[4];

  move_t lastMove = boardHistory.turnMove[lastTurn];
  int numChanges = b.getChangesInHindsightNoCaps(lastMove,src,dest);

  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  int numMoves = 0;
  for(int ch = 0; ch<numChanges; ch++)
  {
    loc_t k0 = src[ch];
    loc_t k1 = dest[ch];

    //Pla piece moved one step away from where it was
    if(b.owners[k1] == pla &&
       Board::manhattanDist(k1,k0) == 1 &&
       b.isRabOkay(pla,k1,k0) &&
       b.isTrapSafe2(pla,k0))
    {
      //Old location is still empty
      if(b.owners[k0] == NPLA)
      {
        //Directly step
        if(b.isThawed(k1))
          mv[numMoves++] = getMove(gStepSrcDest(k1,k0));
        //Unfreeze then step
        else if(b.step < 3)
        {
          for(int i = 0; i<4; i++)
          {
            if(!Board::ADJOKAY[i][k1])
              continue;
            loc_t loc = k1 + Board::ADJOFFSETS[i];
            int dy = Board::GOALLOCINC[pla];
            do //So that the break doesn't exit out of the for loop entirely
            {
              if(loc != k0 && ISE(loc) && b.isTrapSafe3(pla,loc))
              {
                //And optionally step back, in each case. Take only the first successful way to step back
                if(ISP(loc F)                           && b.isThawed(loc F)) {mv[numMoves++] = getMove(loc F MG,gStepSrcDest(k1,k0)); if(b.step < 2 && b.pieces[loc F] != RAB && b.wouldBeUF(pla,loc F,loc,loc F,k1)) mv[numMoves++] = getMove(loc F MG,gStepSrcDest(k1,k0),loc MF); break;}
                if(ISP(loc W)                           && b.isThawed(loc W)) {mv[numMoves++] = getMove(loc W ME,gStepSrcDest(k1,k0)); if(b.step < 2                           && b.wouldBeUF(pla,loc W,loc,loc W,k1)) mv[numMoves++] = getMove(loc W ME,gStepSrcDest(k1,k0),loc MW); break;}
                if(ISP(loc E)                           && b.isThawed(loc E)) {mv[numMoves++] = getMove(loc E MW,gStepSrcDest(k1,k0)); if(b.step < 2                           && b.wouldBeUF(pla,loc E,loc,loc E,k1)) mv[numMoves++] = getMove(loc E MW,gStepSrcDest(k1,k0),loc ME); break;}
                if(ISP(loc G) && b.pieces[loc G] != RAB && b.isThawed(loc G)) {mv[numMoves++] = getMove(loc G MF,gStepSrcDest(k1,k0)); if(b.step < 2                           && b.wouldBeUF(pla,loc G,loc,loc G,k1)) mv[numMoves++] = getMove(loc G MF,gStepSrcDest(k1,k0),loc MG); break;}
              }
            } while(false);
          }
        }
      }
      //Push back
      else if(b.step < 3 && b.owners[k0] == opp && b.pieces[k0] < b.pieces[k1])
      {
        //Directly push
        if(b.isThawed(k1))
        {
          if(k0 S != k1 && ISE(k0 S)) mv[numMoves++] = getMove(k0 MS,gStepSrcDest(k1,k0));
          if(k0 W != k1 && ISE(k0 W)) mv[numMoves++] = getMove(k0 MW,gStepSrcDest(k1,k0));
          if(k0 E != k1 && ISE(k0 E)) mv[numMoves++] = getMove(k0 ME,gStepSrcDest(k1,k0));
          if(k0 N != k1 && ISE(k0 N)) mv[numMoves++] = getMove(k0 MN,gStepSrcDest(k1,k0));
        }
        //Unfreeze then push
        else if(b.step < 2)
        {
          int dy = Board::GOALLOCINC[pla];

          //In this case, to avoid too much branching, try to compute a preferred location to push back to
          loc_t pushToLoc = ERRLOC;
          for(int i = 0; i<numChanges; i++)
          {
            if(dest[i] == k0)
            {
              loc_t oppSrc = src[i];
              if(oppSrc != k1 && ISE(oppSrc) && Board::manhattanDist(oppSrc,k0) == 1)
                pushToLoc = oppSrc;
              break;
            }
          }
          if(pushToLoc == ERRLOC)
          {
            if(k0 G != k1 && ISE(k0 G)) pushToLoc = k0 G;
            else if(k0 W != k1 && ISE(k0 W)) pushToLoc = k0 W;
            else if(k0 E != k1 && ISE(k0 E)) pushToLoc = k0 E;
            else if(k0 F != k1 && ISE(k0 F)) pushToLoc = k0 F;
          }
          if(pushToLoc != ERRLOC)
          {
            for(int i = 0; i<4; i++)
            {
              if(!Board::ADJOKAY[i][k1])
                continue;
              loc_t loc = k1 + Board::ADJOFFSETS[i];
              do //So that the break doesn't exit out of the for loop entirely
              {
                if(loc != k0 && ISE(loc) && b.isTrapSafe3(pla,loc))
                {
                  //And optionally step back, in each case. Take only the first successful way to step back
                  if(ISP(loc F)                           && b.isThawed(loc F)) {mv[numMoves++] = getMove(loc F MG,gStepSrcDest(k0,pushToLoc),gStepSrcDest(k1,k0)); break;}
                  if(ISP(loc W)                           && b.isThawed(loc W)) {mv[numMoves++] = getMove(loc W ME,gStepSrcDest(k0,pushToLoc),gStepSrcDest(k1,k0)); break;}
                  if(ISP(loc E)                           && b.isThawed(loc E)) {mv[numMoves++] = getMove(loc E MW,gStepSrcDest(k0,pushToLoc),gStepSrcDest(k1,k0)); break;}
                  if(ISP(loc G) && b.pieces[loc G] != RAB && b.isThawed(loc G)) {mv[numMoves++] = getMove(loc G MF,gStepSrcDest(k0,pushToLoc),gStepSrcDest(k1,k0)); break;}
                }
              } while(false);
            }
          }
        }
      }
    }
  }

  return numMoves;
}


//QSEARCH------------------------------------------------------------------------------

int SearchMoveGen::genQuiescenceMoves(Board& b, const BoardHistory& hist, int cDepth, int qDepth, move_t* mv, int* hm)
{
  int num = 0;
  int numSteps = 4-b.step;

  if(qDepth >= 1)
  {
    num += genCaptureMoves(b,numSteps,mv+num,hm+num);
    int bnum = genBlockadeMoves(b,mv+num);
    for(int i = 0; i<bnum; i++)
      hm[num+i] = 0;
    num += bnum;

    if(qDepth == 1)
    {
      num += genSevereGoalThreats(b,numSteps,mv+num,hm+num);
      int tnum = BoardTrees::genImportantTrapDefenses(b,b.player,numSteps,mv+num);
      for(int i = 0; i<tnum; i++)
        hm[num+i] = 0;
      num += tnum;
    }
  }
  else //qDepth == 0
  {
    num += genCaptureMoves(b,numSteps,mv+num,hm+num);
    num += genCaptureDefMoves(b,numSteps,false,mv+num,hm+num);
    int bnum = genBlockadeMoves(b,mv+num);
    for(int i = 0; i<bnum; i++)
      hm[num+i] = 0;
    num += bnum;
    num += genSevereGoalThreats(b,numSteps,mv+num,hm+num);
    int tnum = BoardTrees::genImportantTrapDefenses(b,b.player,numSteps,mv+num);
    for(int i = 0; i<tnum; i++)
      hm[num+i] = 0;
    num += tnum;
  }

  //TODO test if generating reverse moves actually helps. It increases the branching factor and makes the search unstable,
  //but maybe it pays off due to the occasional better eval?
  if(SearchParams::QREVERSEMOVE_ENABLE)
  {
    int rnum = genReverseMoves(b,hist,cDepth,mv+num);
    for(int i = 0; i<rnum; i++)
      hm[num+i] = 0;
    num += rnum;
  }

  //Generate the pass move
  {mv[num] = QPASSMOVE; hm[num] = SearchParams::QPASS_SCORE; num++;}
  //Generate a real pass move if we want to force the opponent to find a goal defense
  if(qDepth == 1 && b.step != 0 && b.posCurrentHash != b.posStartHash && BoardTrees::goalDist(b,b.player,4) <= 4)
  {mv[num] = PASSMOVE; hm[num] = 0; num++;}


  return num;
}


//WIN GENERATION---------------------------------------------------------------------------

/*
Legend:
p = pla
o = opp
e = empty
q = adjacent opp stronger than rab
t = traps
S = the bitmap in question
x = expand
a = expand and or with self
f = frozen
u = adjacent to trap with pla
v = vulnerable

StepTo
* StepTo Sex + 1     (step in)
* PullOppTo Sexp + 1 (pull while stepping)
* Leave Sex
* Enter Se

* NeedSpaceAt Sp
* NeedSpaceAt Sov
* Enter Sp + 1
* Enter So + 1
* Leave Spx + 1
* Leave Sovx + 1
* StepTo Sovx + 2 (step piece closer to push) ?strong enough?
* PushOpp Sov + 1 (push into square)

StepFrom
* Leave Sp
* Enter Spxe
* NeedSpaceAround Sp
* PushOpp Spx + 1 (push way out)
* PullOppTo Sp + 1 (pull while stepping)

PushOpp
* StepTo Sovx + 1 (step piece closer to push) **strong enough?
* NeedSpaceAround Sov
* OppMayEnter Sovxe
* OppMayLeave Sov
* Leave Sovx
* Enter Sov

PullOpp
* StepTo Sovx + 1 (step piece closer to pull) **strong enough?
* NeedSpaceAround Sovxp
* OppMayEnter Sovxp
* OppMayLeave Sov
* Leave Sovx
* Enter Sovxpxe

PushOppTo
* StepTo Sexovx + 1 (step piece closer to push) **strong enough?
* StepFrom S + 1 (clear space for pushing)
* PushOppTo Sexe + 2 (pushpull opp closer)
* PullOppTo Sexe + 2 (pushpull opp closer)
* OppMayEnter Se
* OppMayLeave Sexov
* Leave Sexovx    **strong enough?
* Enter Sexov

PullOppTo
* StepTo S + 1 (step piece closer to pull) **strong enough?
* StepFrom Spxox + 1 (clear space for pull) NOT included - this is actually handled by OppMayEnter!
* PushOppTo Spxe + 2 (pushpull opp closer)
* PullOppTo Spxe + 2 (pushpull opp closer)
* OppMayEnter Sp
* OppMayLeave Spxov
* Leave S        **strong enough?
* Enter Spxe

NeedSpaceAround
* NeedSpaceAt Sx

NeedSpaceAt
* StepFrom S + 1 (clear space by stepping)
* StepTo S + 2 (move a piece through the location before blocking it)
* PullOpp S + 2 (make space by pulling)
* PushOpp Sotx + 2 (make space by capturing)
* PullOpp Sotx + 2 (make space by capturing)

OppMayLeave
* OppMayDie Sxto

OppMayDie
* PushOppTo Sx + 2 (pushpull another opp to be a defender to avoid killing this one)
* PullOppTo Sx + 2 (pushpull another opp to be a defender to avoid killing this one)

OppMayEnter
* StepFrom Sxp + 1 (step before would freeze)
* StepTo Sxe + 2 (move a piece through the location before freezing)
* PushOppTo Sxt + 2 (kill a piece before the entering opp defends)
* PullOppTo Sxt + 2 (kill a piece before the entering opp defends)

Leave
* StepTo Spfx + 1 (unfreeze with defender)
* PullOpp Spfx + 2 (pull freezer away)
* PushOpp Spfxotx + 2 (kill freezer)
* PullOpp Spfxotx + 2 (kill freezer)
* StepTo Spxtpx + 1 (step before moving away would sac, or defend)
* StepFrom Spxq + 1 (step before moving away would freeze)
* StepTo Spxeq + 2 (move a piece through the location before freezing)
* StepTo Spxet + 2 (move a piece to a location and defend it before moving away would undefend)
* PushOppTo Spxpx + 3 (advance-push then step before moving away would freeze)
* PullOppTo Spxpx + 3 (advance-pull then step before moving away would freeze)

Enter
* StepTo Sxt + 1  (sac a piece in trap by stepping in before defending)
* StepFrom Sxtpx + 1  (sac a piece in trap by removingdef before defending)

* StepTo Stx + 1   (defend trap before stepping to it)
* StepFrom Stx + 1 (undefend trap before stepping to it)
*/

static void expandRelArea(const Board& b, pla_t pla, SearchMoveGen::RelevantArea& relBuf,
    Bitmap pushOpps, Bitmap pullOpps, Bitmap pushOppsTo, Bitmap pullOppsTo, int maxSteps)
{
  if(maxSteps <= 1)
    return;

  SearchMoveGen::RelevantArea relBuf2;
  SearchMoveGen::RelevantArea relBuf3;

  SearchMoveGen::RelevantArea* cur = &relBuf;    //current
  SearchMoveGen::RelevantArea* next = &relBuf2;  //+1
  SearchMoveGen::RelevantArea* next2 = &relBuf3; //+2

  pla_t opp = gOpp(pla);
  Bitmap plaMap = b.pieceMaps[pla][0];
  Bitmap oppMap = b.pieceMaps[opp][0];
  Bitmap empMap = ~(plaMap | oppMap);
  Bitmap oppNonRabMap = oppMap & ~b.pieceMaps[opp][RAB];
  Bitmap adjOppNonRab = Bitmap::adj(oppNonRabMap);

  //Opps that can be maybe be pushed in maxSteps. For speed, we assume all rabbits are.
  Bitmap oppVulnerableMap = b.pieceMaps[opp][RAB];
  Bitmap plaDoms;
  for(piece_t piece = CAM; piece >= CAT; piece--)
  {
    plaDoms |= b.pieceMaps[pla][piece+1];
    Bitmap map = b.pieceMaps[opp][piece];
    while(map.hasBits())
    {
      loc_t oloc = map.nextBit();
      if((Board::DISK[maxSteps-1][oloc] & plaDoms).hasBits())
        oppVulnerableMap.setOn(oloc);
    }
  }

  next->pushOpp = pushOpps;
  next->pullOpp = pullOpps;
  next->pushOppTo = pushOppsTo;
  next->pullOppTo = pullOppsTo;

  Bitmap enter;
  Bitmap leave;
  for(int i = 0; i<maxSteps-1; i++)
  {
    Bitmap nextEnter;
    Bitmap nextLeave;
    Bitmap needSpaceAt;
    Bitmap needSpaceAround;
    Bitmap oppMayEnter;
    Bitmap oppMayLeave;
    Bitmap oppMayDie;

    next->stepTo |= cur->stepTo;
    next->stepFrom |= cur->stepFrom;
    next->pushOpp |= cur->pushOpp;
    next->pullOpp |= cur->pullOpp;
    next->pushOppTo |= cur->pushOppTo;
    next->pullOppTo |= cur->pullOppTo;

    //StepTo_e
    Bitmap stepTo_e = cur->stepTo & empMap;
    Bitmap stepTo_ex = Bitmap::adj(stepTo_e);
    next->stepTo |= stepTo_ex;
    next->pullOppTo |= stepTo_ex & plaMap;
    leave |= stepTo_ex;
    enter |= stepTo_e;

    //StepTo_p and StepTo_o
    Bitmap stepTo_ov = cur->stepTo & oppVulnerableMap;
    Bitmap stepTo_p_or_ov = cur->stepTo & (plaMap | oppVulnerableMap);
    needSpaceAt |= stepTo_p_or_ov;
    nextEnter |= stepTo_p_or_ov;
    nextLeave |= Bitmap::adj(stepTo_p_or_ov);
    next2->stepTo |= Bitmap::adj(stepTo_ov);
    next->pushOpp |= stepTo_ov;

    //StepFrom
    Bitmap stepFrom_p = cur->stepFrom & plaMap;
    Bitmap stepFrom_px = Bitmap::adj(stepFrom_p);
    leave |= stepFrom_p;
    enter |= stepFrom_px & empMap;
    needSpaceAround |= stepFrom_p;
    next->pushOpp |= stepFrom_px;
    next->pullOppTo |= stepFrom_p;

    //PushOpp
    Bitmap pushOpp_ov = cur->pushOpp & oppVulnerableMap;
    Bitmap pushOpp_ovx = Bitmap::adj(pushOpp_ov);
    next->stepTo |= pushOpp_ovx;
    needSpaceAround |= pushOpp_ov;
    leave |= pushOpp_ovx;
    enter |= pushOpp_ov;
    oppMayEnter |= pushOpp_ovx & empMap;
    oppMayLeave |= pushOpp_ov;

    //PullOpp
    Bitmap pullOpp_ov = cur->pullOpp & oppVulnerableMap;
    Bitmap pullOpp_ovx = Bitmap::adj(pullOpp_ov);
    Bitmap pullOpp_ovxpx = Bitmap::adj(pullOpp_ovx & plaMap);
    next->stepTo |= pullOpp_ovx;
    needSpaceAround |= pullOpp_ovx & plaMap;
    leave |= pullOpp_ovx;
    enter |= pullOpp_ovxpx & empMap;
    oppMayEnter |= pullOpp_ovx & plaMap;  //TODO & plaNonRabMap?
    oppMayLeave |= pullOpp_ov;

    //PushOppTo
    Bitmap pushOppTo_e = cur->pushOppTo & empMap;
    Bitmap pushOppTo_ex = Bitmap::adj(pushOppTo_e);
    Bitmap pushOppTo_exov = pushOppTo_ex & oppVulnerableMap;
    Bitmap pushOppTo_exovx = Bitmap::adj(pushOppTo_exov);
    next->stepTo |= pushOppTo_exovx;
    next->stepFrom |= cur->pushOppTo;
    next2->pushOppTo |= pushOppTo_ex & empMap;
    next2->pullOppTo |= pushOppTo_ex & empMap;
    oppMayEnter |= pushOppTo_e;
    oppMayLeave |= pushOppTo_exov;
    leave |= pushOppTo_exovx;
    enter |= pushOppTo_exov;

    //PullOppTo
    Bitmap pullOppTo_p = cur->pullOppTo & plaMap;
    Bitmap pullOppTo_px = Bitmap::adj(pullOppTo_p);
    Bitmap pullOppTo_pxov = pullOppTo_px & oppVulnerableMap;
    Bitmap pullOppTo_pxe = pullOppTo_px & empMap;
    next->stepTo |= cur->pullOppTo;
    next2->pushOppTo |= pullOppTo_pxe;
    next2->pullOppTo |= pullOppTo_pxe;
    oppMayEnter |= pullOppTo_p;
    oppMayLeave |= pullOppTo_pxov;
    leave |= cur->pullOppTo;
    enter |= pullOppTo_pxe;

    //NeedSpaceAround
    needSpaceAt |= Bitmap::adj(needSpaceAround);

    //NeedSpaceAt
    next->stepFrom |= needSpaceAt;
    next2->stepTo |= needSpaceAt;
    Bitmap needSpaceAt_otx = Bitmap::adj(needSpaceAt & oppMap & Bitmap::BMPTRAPS);
    next2->pushOpp |= needSpaceAt_otx;
    next2->pullOpp |= needSpaceAt_otx;
    next2->pullOpp |= needSpaceAt;

    //OppMayEnter
    Bitmap oppMayEnter_x = Bitmap::adj(oppMayEnter);
    next->stepFrom |= oppMayEnter_x & plaMap;
    next2->stepTo |= oppMayEnter_x & empMap;
    Bitmap oppMayEnter_xt = oppMayEnter_x & Bitmap::BMPTRAPS;
    next2->pushOppTo |= oppMayEnter_xt;
    next2->pullOppTo |= oppMayEnter_xt;
    oppMayDie |= oppMayEnter & Bitmap::BMPTRAPS;

    //OppMayLeave
    oppMayDie |= Bitmap::adj(oppMayLeave) & Bitmap::BMPTRAPS & oppMap;

    //OppMayDie
    Bitmap oppMayDie_x = Bitmap::adj(oppMayDie);
    next2->pushOppTo |= oppMayDie_x;
    next2->pullOppTo |= oppMayDie_x;

    //Leave
    Bitmap leave_pfx = Bitmap::adj(leave & plaMap & b.frozenMap);
    next->stepTo |= leave_pfx;
    next2->pullOpp |= leave_pfx;
    Bitmap leave_pfxotx = Bitmap::adj(leave_pfx & oppMap & Bitmap::BMPTRAPS);
    next2->pushOpp |= leave_pfxotx;
    next2->pullOpp |= leave_pfxotx;

    Bitmap leave_px = Bitmap::adj(leave & plaMap);
    next->stepTo |= Bitmap::adj(leave_px & Bitmap::BMPTRAPS & plaMap);
    next->stepFrom |= leave_px & adjOppNonRab;
    next2->stepTo |= leave_px & empMap & (Bitmap::BMPTRAPS | adjOppNonRab);
    Bitmap leave_pxpx = Bitmap::adj(leave_px & plaMap);

    //Enter
    Bitmap enter_xt = Bitmap::adj(enter) & Bitmap::BMPTRAPS;
    next->stepTo |= enter_xt;
    next->stepFrom |= Bitmap::adj(enter_xt & plaMap);

    Bitmap enter_tx = Bitmap::adj(enter & Bitmap::BMPTRAPS);
    next->stepTo |= enter_tx;
    next->stepFrom |= enter_tx;

    SearchMoveGen::RelevantArea* temp = cur;
    cur = next;
    next = next2;
    next2 = temp;

    next2->pushOppTo |= leave_pxpx;
    next2->pullOppTo |= leave_pxpx;

    enter = nextEnter;
    leave = nextLeave;
  }

  relBuf = *cur;
}

bool SearchMoveGen::getGoalDefRelArea(Board& b, int maxSteps, loc_t& rloc, RelevantArea& relBuf)
{
  //Pla trying to stop opp goal
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  Bitmap rMap = b.pieceMaps[opp][RAB];
  rMap &= Board::PGOALMASKS[4][opp];

  bool found = false;
  rloc = ERRLOC;
  int gDist = 5;
  while(rMap.hasBits())
  {
    loc_t k = rMap.nextBit();
    gDist = BoardTrees::goalDist(b,opp,4,k);
    if(gDist < 5)
    {
      found = true;
      rloc = k;
      break;
    }
  }

  if(!found)
    return false;

  relBuf.stepTo = Bitmap();
  relBuf.stepFrom = Bitmap();
  relBuf.pushOpp = Bitmap();
  relBuf.pullOpp = Bitmap();
  relBuf.pushOppTo = Bitmap();
  relBuf.pullOppTo = Bitmap();

  if(gDist <= 0)
  {
    relBuf.pushOpp.setOn(rloc);
    relBuf.pullOpp.setOn(rloc);
    return true;
  }

  Bitmap pushPullOpps;
  Bitmap pushOppsTo;
  Bitmap pullOppsTo;

  if(b.goalTreeMove != ERRMOVE)
  {
    Bitmap pStrongerMaps[2][NUMTYPES];
    b.initializeStrongerMaps(pStrongerMaps);
    loc_t rabDest = rloc;
    int nsInMove = numStepsInMove(b.goalTreeMove);
    TempRecord records[nsInMove];
    int i;
    for(i = 0; i<nsInMove; i++)
    {
      step_t step = getStep(b.goalTreeMove,i);
      loc_t src = gSrc(step);
      loc_t dest = gDest(step);

      if(b.owners[src] == opp)
      {
        //The opponent made a step - if there's an adjacent opp, we can potentially interfere by removing it
        //so that the opp's step might be illegal due to freezing.
        if((pStrongerMaps[opp][b.pieces[src]] & Board::DISK[maxSteps+1][src]).hasBits())
        {
          if     (b.owners[src S] == opp) pushPullOpps.setOn(src S);
          else if(b.owners[src W] == opp) pushPullOpps.setOn(src W);
          else if(b.owners[src E] == opp) pushPullOpps.setOn(src E);
          else if(b.owners[src N] == opp) pushPullOpps.setOn(src N);
          //And if there's no adjacent opp, then we can interfere by stepping adjacent to freeze it.
          else relBuf.stepTo |= Board::DISK[1][src];
        }

        //We can of course interfere by pushpulling the piece itself
        pushPullOpps.setOn(src);
      }

      //We can interfere by stepping to the dest ourselves, or filling dest with something else.
      relBuf.stepTo.setOn(dest);
      pushOppsTo.setOn(dest);

      if(src == rabDest)
        rabDest = dest;

      //We only need to expand relarea for traps for the first two steps, or for the third step
      //if it's the continuation of an capture from the previous step.
      if(i == nsInMove-1 && i >= gDist-1)
        break;
      records[i] = b.tempStepC(src,dest);

      //We only need to expand relarea for traps for the first two steps, or for the third step
      //if it's the continuation of a removedef capture from the previous step.
      bool needExpandAroundTraps = (i < gDist-2 || (b.owners[dest] == pla && records[i].capLoc != ERRLOC));
      if(needExpandAroundTraps)
      {
        if(records[i].capLoc != ERRLOC)
        {
          if(records[i].capOwner == pla)
            //Interfere by defending our own trap
            relBuf.stepTo |= Board::RADIUS[1][records[i].capLoc];
          else
          {
            //Interfere by keeping the opp's piece from dying!
            pushOppsTo |= Board::RADIUS[1][records[i].capLoc];
            pullOppsTo |= Board::RADIUS[1][records[i].capLoc];
          }
        }
        else
        {
          loc_t capLoc = Board::ADJACENTTRAP[src];
          if(capLoc != ERRLOC && b.owners[capLoc] == opp)
          {
            //Interfere by pushpulling a defender of a trap that the opp might need
            if     (b.owners[capLoc S] == opp) pushPullOpps.setOn(capLoc S);
            else if(b.owners[capLoc W] == opp) pushPullOpps.setOn(capLoc W);
            else if(b.owners[capLoc E] == opp) pushPullOpps.setOn(capLoc E);
            else if(b.owners[capLoc N] == opp) pushPullOpps.setOn(capLoc N);
          }
        }
      }
    }

    for(int j = i-1; j >= 0; j--)
      b.undoTemp(records[j]);

    Bitmap gPruneMask = Board::GPRUNEMASKS[opp][rabDest][gDist-nsInMove];
    pushPullOpps |= gPruneMask;
    relBuf.stepTo |= gPruneMask;
    pushOppsTo |= gPruneMask;
  }
  else
  {
    Bitmap gPruneMask = Board::GPRUNEMASKS[opp][rloc][gDist];
    pushPullOpps |= gPruneMask;
    relBuf.stepTo |= gPruneMask;
    pushOppsTo |= gPruneMask;
  }

  if(maxSteps > 1)
  {
    //If any opp was relevant, and it was on a trap, then anything keeping it alive is also relevant
    pushPullOpps |= Bitmap::adj(pushPullOpps & Bitmap::BMPTRAPS) & b.pieceMaps[opp][0];

    expandRelArea(b,pla,relBuf,pushPullOpps,pushPullOpps,pushOppsTo,pullOppsTo,maxSteps);

  }
  return true;
}

bool SearchMoveGen::getWinDefRelArea(Board& b, RelevantArea& relBuf)
{
  loc_t rloc;
  int maxSteps = 4-b.step;
  if(getGoalDefRelArea(b,maxSteps,rloc,relBuf))
    return true;

  loc_t trapsBuf[2];
  if(getElimDefRelArea(b,maxSteps,trapsBuf,relBuf))
    return true;

  return false;
}


bool SearchMoveGen::getElimDefRelArea(Board& b, int maxSteps, loc_t trapsBuf[2], RelevantArea& relBuf)
{
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);

  if(!BoardTrees::canElim(b,opp,4))
    return false;

  relBuf.stepTo = Bitmap();
  relBuf.stepFrom = Bitmap();
  relBuf.pushOpp = Bitmap();
  relBuf.pullOpp = Bitmap();
  relBuf.pushOppTo = Bitmap();
  relBuf.pullOppTo = Bitmap();

  if(b.pieceCounts[pla][RAB] == 0)
  {
    trapsBuf[0] = ERRLOC;
    trapsBuf[1] = ERRLOC;
    return true;
  }

  Bitmap relOps;
  if(b.pieceCounts[pla][RAB] == 1)
  {
    Bitmap rMap = b.pieceMaps[pla][RAB];
    loc_t rloc = rMap.nextBit();
    if(rloc == ERRLOC)
      Global::fatalError("genElimDefMoves: PieceCount 1 but no rabbit in bitmap");
    loc_t trap = Board::CLOSEST_TRAP[rloc];
    //TODO could potentially improve this...
    Bitmap relArea = Board::DISK[6][rloc] | Board::DISK[6][trap];
    relBuf.stepTo = relArea;
    relOps = relArea;
    expandRelArea(b,pla,relBuf,relOps,relOps,Bitmap(),Bitmap(),maxSteps);

    trapsBuf[0] = trap;
    trapsBuf[1] = trap;
    return true;
  }
  else if(b.pieceCounts[pla][RAB] == 2)
  {
    Bitmap rMap = b.pieceMaps[pla][RAB];
    Bitmap relArea;
    int i = 0;
    while(rMap.hasBits())
    {
      loc_t rloc = rMap.nextBit();
      loc_t trap = Board::CLOSEST_TRAP[rloc];
      relArea |= Board::DISK[3][rloc] | Board::DISK[3][trap];
      trapsBuf[i++] = trap;
      if(i > 2)
        Global::fatalError("genElimDefMoves: PieceCount 2 but more than 2 in bitmap");
    }
    relBuf.stepTo = relArea;
    relOps = relArea;
    expandRelArea(b,pla,relBuf,relOps,relOps,Bitmap(),Bitmap(),maxSteps);
    return true;
  }
  return false;
}

int SearchMoveGen::genElimDefMoves(Board& b, move_t* mv, int* hm, int maxSteps, loc_t traps[2], const RelevantArea& relArea)
{
  //Generate pushpulls if enough steps left
  int num = 0;
  pla_t pla = b.player;

  if(maxSteps > 1)
    num += BoardMoveGen::genPushPullsOfOpps(b,pla,mv,relArea.pushOpp,relArea.pullOpp,relArea.pushOppTo,relArea.pullOppTo);
  num += BoardMoveGen::genStepsIntoOrOutof(b,pla,mv+num,relArea.stepTo,relArea.stepFrom);

  //Order the moves
  if(hm != NULL)
  {
    for(int i = 0; i<num; i++)
    {
      move_t move = mv[i];
      int h = 0;
      for(int j = 0; j<4; j++)
      {
        step_t step = getStep(move,j);
        if(step == ERRSTEP)
          break;
        loc_t k0 = gSrc(step);

        if(b.owners[k0] == pla)
          h = min(Board::manhattanDist(traps[0],k0),Board::manhattanDist(traps[1],k0));
      }
      hm[i] = h;
    }
  }
  return num;
}

int SearchMoveGen::genGoalDefMoves(Board& b, move_t* mv, int* hm, int maxSteps, loc_t rloc, const RelevantArea& relArea)
{
  pla_t pla = b.player;
  int num = 0;

  if(maxSteps > 1)
    num += BoardMoveGen::genPushPullsOfOpps(b,pla,mv,relArea.pushOpp,relArea.pullOpp,relArea.pushOppTo,relArea.pullOppTo);
  num += BoardMoveGen::genStepsIntoOrOutof(b,pla,mv+num,relArea.stepTo,relArea.stepFrom);

  //Order the moves
  if(hm != NULL)
  {
    for(int i = 0; i<num; i++)
    {
      move_t move = mv[i];
      int h = 0;
      for(int j = 0; j<4; j++)
      {
        step_t step = getStep(move,j);
        if(step == ERRSTEP)
          break;
        loc_t k0 = gSrc(step);
        loc_t k1 = gDest(step);
        if(k0 == rloc && Board::GOALYDIST[gOpp(pla)][k1] > Board::GOALYDIST[gOpp(pla)][k0])
        {h = 10; break;}
        else if(k0 == rloc && Board::GOALYDIST[gOpp(pla)][k1] < Board::GOALYDIST[gOpp(pla)][k0])
        {h = 0; break;}
        else if(k0 == rloc)
        {h = 5; break;}

        if(b.owners[k0] == pla)
        {
          if(Board::manhattanDist(rloc,k1) < Board::manhattanDist(rloc,k0))
            h = 9-Board::manhattanDist(rloc,k1) < 1 ? 1 : 9-Board::manhattanDist(rloc,k1);
          else
            h = 0;
          break;
        }
      }
      hm[i] = h;
    }
  }
  return num;
}


int SearchMoveGen::genWinDefMovesIfNeeded(Board& b, move_t* mv, int* hm, int maxSteps)
{
  //Both a quick early check, and also later just in case to make sure that we don't call
  //genElimDefMoves with trapsBuf containing ERRLOCS, although the latter might be safe
  //anyways because if that's the case, there should be no generated moves either.
  if(b.pieceCounts[b.player][RAB] == 0)
    return 0;

  RelevantArea rel;
  loc_t rloc;
  if(getGoalDefRelArea(b,maxSteps,rloc,rel))
    return genGoalDefMoves(b,mv,hm,maxSteps,rloc,rel);

  loc_t trapsBuf[2];
  if(getElimDefRelArea(b,maxSteps,trapsBuf,rel))
    return genElimDefMoves(b,mv,hm,maxSteps,trapsBuf,rel);

  return -1;
}




//WIN DEFENSE SEARCH GENERATION---------------------------------------------------------------------------

//Recurses down filling shortestmv, incrementing shortestnum, and incrementing numInteriorNodes
static void winDefSearchHelper(Board& b, pla_t pla,
    int rDepth, move_t sofar, int sofarNS, bool stopAfterFirst,
    move_t* shortestmv, int& shortestnum, int& numInteriorNodes)
{
  //Can't go any further - test if opponent can win
  if(rDepth <= 0)
  {
    pla_t opp = gOpp(pla);

    int oppGoalDist = BoardTrees::goalDist(b,opp,4);
    //Opponent can still goal? - we lose
    if(oppGoalDist < 5)
      return;

    //Opponent can elim? - we lose
    if(BoardTrees::canElim(b,opp,4))
      return;

    //Successfuly stopped opp win - add move
    shortestmv[shortestnum] = sofar;
    shortestnum++;
    return;
  }

  //Generate win defense moves
  //TODO can probably be more cache friendly with these 256's, choose a tighter bound.
  move_t mv[256];
  int hm[256];
  int num = SearchMoveGen::genWinDefMovesIfNeeded(b,mv,hm,rDepth);

  //No defenses - we lose right away...
  if(num == 0)
    return;

  //No win threat! We're done!
  if(num == -1)
  {
    shortestmv[shortestnum] = sofar;
    shortestnum++;
    return;
  }

  //Else we need to recurse and try all the possible moves
  numInteriorNodes++;
  UndoMoveData uData;
  for(int m = 0; m<num; m++)
  {
    move_t move = mv[m];
    b.makeMove(move,uData);

    //Can't undo the position.
    //If we have no rabbits (such as we sacked them to stop goal), then we lose if we make this move.
    if(b.posCurrentHash == b.posStartHash || b.pieceCounts[pla][RAB] == 0)
    {b.undoMove(uData); continue;}

    int ns = numStepsInMove(move);
    move_t nextSoFar = concatMoves(sofar,move,sofarNS);
    winDefSearchHelper(b,pla,rDepth-ns,nextSoFar,sofarNS+ns,stopAfterFirst,shortestmv,shortestnum,numInteriorNodes);
    b.undoMove(uData);
    if(stopAfterFirst && shortestnum > 0)
      return;
  }
}

//Returns number of moves generated
//Or -1 if no threat by opponent at all
static int winDefSearchHelperRoot(Board& b, pla_t pla, int rDepth, bool stopAfterFirst, move_t* shortestmv, int& numInteriorNodes)
{
  //Generate win defense moves
  move_t mv[256];
  int hm[256];
  int num = SearchMoveGen::genWinDefMovesIfNeeded(b,mv,hm,rDepth);

  //Either no defenses (0) - we lose right away...
  //Or no win threat (-1), in which case we're done.
  if(num <= 0)
    return num;

  //Else we need to recurse and try all the possible moves
  numInteriorNodes++;
  UndoMoveData uData;
  int shortestnum = 0;
  for(int m = 0; m<num; m++)
  {
    move_t move = mv[m];
    b.makeMove(move,uData);

    //Can't undo the position.
    //Also if we have no rabbits (such as we sacked them to stop goal), then we lose if we make this move.
    //Note that not undoing the position is necessary even here at the root because we need to know
    //if the qsearch will reject our move due to it being an undo of a move made earlier, outside
    //the windefsearch, and if so, propose other windefs instead.
    //Note that we've presumably checked own goals and elims prior to windefsearch (handling the case
    //where we lose all our own rabbits but cap the opponent's too on the same turn and win).
    if(b.posCurrentHash == b.posStartHash || b.pieceCounts[pla][RAB] == 0)
    {b.undoMove(uData); continue;}

    int ns = numStepsInMove(move);
    winDefSearchHelper(b,pla,rDepth-ns,move,ns,stopAfterFirst,shortestmv,shortestnum,numInteriorNodes);
    b.undoMove(uData);
    if(stopAfterFirst && shortestnum > 0)
      return shortestnum;
  }
  return shortestnum;
}


int SearchMoveGen::genShortestFullWinDefMoves(Board& b, move_t* shortestmv, int& numInteriorNodes, int& defenseSteps)
{
  int numStepsLeft = 4-b.step;
  for(int rDepth = 1; rDepth <= numStepsLeft; rDepth++)
  {
    int num = winDefSearchHelperRoot(b,b.player,rDepth,false,shortestmv,numInteriorNodes);
    if(num != 0)
    {
      defenseSteps = num < 0 ? 0 : rDepth;
      return num;
    }
  }
  defenseSteps = numStepsLeft;
  return 0;
}

int SearchMoveGen::minWinDefSteps(Board& b, int maxSteps)
{
  int numInteriorNodes;
  move_t shortestmv[1];
  for(int rDepth = 1; rDepth <= maxSteps; rDepth++)
  {
    int num = winDefSearchHelperRoot(b,b.player,rDepth,true,shortestmv,numInteriorNodes);
    if(num != 0)
      return num < 0 ? 0 : rDepth;
  }
  return maxSteps+1;
}

bool SearchMoveGen::definitelyForcedLossInTwo(Board& b)
{
  int numStepsLeft = 4-b.step;
  return minWinDefSteps(b,numStepsLeft) > numStepsLeft;
}

//ASSUMES that opp only has 1 defender of the trap
static bool oppDefinitelyNotCapturable(Board& b, pla_t pla, loc_t kt, int numSteps, const Bitmap pStrongerMaps[2][NUMTYPES])
{
  pla_t opp = gOpp(pla);
  loc_t defLoc = b.findGuard(opp,kt);
  if((pStrongerMaps[opp][b.pieces[defLoc]] & Board::DISK[numSteps / 2][defLoc]).isEmpty())
    return true;
  return false;
}

//RELEVANT TACTICS----------------------------------------------------------------------------------------

//If genWinDef is true, then generates all windef moves if needed as reltactics, otherwise skips them
//and generates moves as if there's no win threat by the opp.
int SearchMoveGen::genRelevantTactics(Board& b, bool genWinDef, move_t* mv, int* hm)
{
  //TODO this is super branchy, reduce it?
  int numStepsLeft = 4-b.step;
  if(genWinDef)
  {
    int num = SearchMoveGen::genWinDefMovesIfNeeded(b,mv,hm,numStepsLeft);
    if(num >= 0)
      return num;
  }
  int num = 0;

  pla_t pla = b.player;
  pla_t opp = gOpp(pla);
  Bitmap adjTraps = Bitmap::adj(Bitmap::BMPTRAPS);
  Bitmap nearTraps = Bitmap::BMPTRAPS | adjTraps;

  //PUSHPULLS-----------------------------------------------------------------------------
  //All 2-step pushpulls where the opp step involves a trap defense square,
  //excluding suicides
  if(numStepsLeft >= 2)
    num += BoardMoveGen::genPushPullsOfOppsInvolvingNoSuicide(b,b.player,mv+num,nearTraps);

  //All 3-step and 4-step pushpulls of an opp trap defender
  if(numStepsLeft >= 3)
  {
    Bitmap oppTrapDefs = adjTraps & b.pieceMaps[opp][0];
    while(oppTrapDefs.hasBits())
    {
      loc_t oloc = oppTrapDefs.nextBit();
      num += BoardTrees::genPP3Step(b,mv+num,oloc);
      if(numStepsLeft >= 4)
        num += BoardTrees::genPP4Step(b,mv+num,oloc);
    }
  }

  //All pushpulls of opps towards traps that we control
  //Try to make sure the opp step does NOT duplicate what we generated above
  if(numStepsLeft >= 2)
  {
    for(int trapIndex = 0; trapIndex<4; trapIndex++)
    {
      loc_t kt = Board::TRAPLOCS[trapIndex];
      //Make sure we have exclusive control of the trap
      if(b.trapGuardCounts[pla][trapIndex] == 0 || b.trapGuardCounts[opp][trapIndex] > 0)
        continue;
      //2-step pushpulls of opps at radius 3 of our trap to radius 2 of our trap
      Bitmap oppsToPushpull2S = Board::RADIUS[3][kt] & b.pieceMaps[opp][0] & b.dominatedMap;
      while(oppsToPushpull2S.hasBits())
      {
        loc_t oloc = oppsToPushpull2S.nextBit();
        if(Board::manhattanDist(oloc S,kt) == 2 && Board::ADJACENTTRAP[oloc S] == ERRLOC) num += BoardTrees::genPPTo2Step(b,mv+num,oloc,oloc S);
        if(Board::manhattanDist(oloc W,kt) == 2 && Board::ADJACENTTRAP[oloc W] == ERRLOC) num += BoardTrees::genPPTo2Step(b,mv+num,oloc,oloc W);
        if(Board::manhattanDist(oloc E,kt) == 2 && Board::ADJACENTTRAP[oloc E] == ERRLOC) num += BoardTrees::genPPTo2Step(b,mv+num,oloc,oloc E);
        if(Board::manhattanDist(oloc N,kt) == 2 && Board::ADJACENTTRAP[oloc N] == ERRLOC) num += BoardTrees::genPPTo2Step(b,mv+num,oloc,oloc N);
      }

      if(numStepsLeft >= 3)
      {
        //3 and 4-step pushpulls of opps at radius 2 or 3 towards our trap
        for(int radius = 2; radius <= 3; radius++)
        {
          Bitmap oppsToPushpull34S = Board::RADIUS[radius][kt] & b.pieceMaps[opp][0] & ~nearTraps;
          while(oppsToPushpull34S.hasBits())
          {
            loc_t oloc = oppsToPushpull34S.nextBit();
            int oldNum = num;
            if(Board::manhattanDist(oloc S,kt) < radius) num += BoardTrees::genPPTo3Step(b,mv+num,oloc,oloc S);
            if(Board::manhattanDist(oloc W,kt) < radius) num += BoardTrees::genPPTo3Step(b,mv+num,oloc,oloc W);
            if(Board::manhattanDist(oloc E,kt) < radius) num += BoardTrees::genPPTo3Step(b,mv+num,oloc,oloc E);
            if(Board::manhattanDist(oloc N,kt) < radius) num += BoardTrees::genPPTo3Step(b,mv+num,oloc,oloc N);

            //If no 3-step pushpulls, then generate 4-step pushpulls
            if(oldNum == num && numStepsLeft >= 4)
            {
              if(Board::manhattanDist(oloc S,kt) < radius) num += BoardTrees::genPPTo4Step(b,mv+num,oloc,oloc S);
              if(Board::manhattanDist(oloc W,kt) < radius) num += BoardTrees::genPPTo4Step(b,mv+num,oloc,oloc W);
              if(Board::manhattanDist(oloc E,kt) < radius) num += BoardTrees::genPPTo4Step(b,mv+num,oloc,oloc E);
              if(Board::manhattanDist(oloc N,kt) < radius) num += BoardTrees::genPPTo4Step(b,mv+num,oloc,oloc N);
            }
          }
        }
      }
    }
  }

  Bitmap pStrongerMaps[2][NUMTYPES];
  b.initializeStrongerMaps(pStrongerMaps);

  //TODO this is extremely branchy relative to how much it adds
  //MAKING CAPTURE THREATS, BLOCKING RUNAWAYS-------------------------------------------------------------
  //Stepping to and out of the way of threatened pieces
  Bitmap noStepOutOfWayTo;
  Bitmap stepOutOfWayMap;
  Bitmap stepToBlockMap;
  for(int trapIndex = 0; trapIndex<4; trapIndex++)
  {
    loc_t kt = Board::TRAPLOCS[trapIndex];
    //Make sure we have exclusive control of the trap
    if(b.trapGuardCounts[pla][trapIndex] == 0 || b.trapGuardCounts[opp][trapIndex] > 0)
      continue;
    if(b.trapGuardCounts[pla][trapIndex] == 1)
      noStepOutOfWayTo.setOn(kt);

    //Frozen opp piece at radius 2
    Bitmap threatenedOpps = b.pieceMaps[opp][0] & b.frozenMap & Board::RADIUS[2][kt];
    while(threatenedOpps.hasBits())
    {
      loc_t oloc = threatenedOpps.nextBit();
      //Step weak piece out of the way in 1 step if possible
      Bitmap adjOloc = Board::RADIUS[1][oloc];
      Bitmap adjOrOnTrap = Board::DISK[1][kt];
      stepOutOfWayMap |= (~pStrongerMaps[opp][b.pieces[oloc]] & adjOloc & adjOrOnTrap);
      //Step piece to block escape
      stepToBlockMap |= adjOloc & ~adjOrOnTrap;
    }
  }

  //TODO this doesn't add very much, maybe its not necessary?
  //Stepping to block threatened opps right next to the trap
  //Only do this if we have too few steps left to capture it, only do this for home traps
  if(numStepsLeft <= 1)
  {
    for(int i = 0; i<2; i++)
    {
      int trapIndex = Board::PLATRAPINDICES[pla][i];
      loc_t kt = Board::TRAPLOCS[trapIndex];
      //Make sure we have control of the trap and the opp has a single threatened piece next to it
      if(b.trapGuardCounts[pla][trapIndex] == 0 || b.trapGuardCounts[opp][trapIndex] != 1)
        continue;

      //Frozen opp piece at radius 1
      Bitmap threatenedOpps = b.pieceMaps[opp][0] & b.frozenMap & Board::RADIUS[1][kt];
      while(threatenedOpps.hasBits())
      {
        loc_t oloc = threatenedOpps.nextBit();
        //Step piece to block escape
        Bitmap adjOloc = Board::RADIUS[1][oloc];
        stepToBlockMap |= adjOloc & ~Bitmap::makeLoc(kt);
      }
    }
  }
  num += BoardMoveGen::genStepsIntoAndOutof(b,pla,mv+num,~noStepOutOfWayTo,stepOutOfWayMap);
  num += BoardMoveGen::genStepsInto(b,pla,mv+num,stepToBlockMap);

  //TODO there's one more kind of freeze attack that we're missing - rather than an offensive
  //freeze attack, a defensive one, where you freeze one or more pieces in places like b3 or d3
  //that are attacking one of your traps and you don't have the upper hand yet.

  //TODO generate freezes of pieces that dominate our pieces radius 2 from an opp-controlled trap
  //(cap threat, or cap threat if only our elephant weren't guarding)
  //Generate various sorts of attacks where we try to freeze opponent pieces
  num += BoardTrees::genFreezeAttacks(b,pla,numStepsLeft,pStrongerMaps,mv+num);

  if(numStepsLeft >= 2)
  {
    num += BoardTrees::genDistantTrapAttacks(b,pla,numStepsLeft,pStrongerMaps,mv+num);
    num += BoardTrees::genDistantAdvances(b,pla,numStepsLeft,pStrongerMaps,mv+num);
    num += BoardTrees::genDistantTrapFightSwarms(b,pla,numStepsLeft,mv+num);
  }

  //TODO generate moves by the elephant towards a fighty trap
  //TODO generate moves and unblocks by mh towards a deep advanced hd in own territory (back two rows)
  //TODO if there are no opp defenders of a trap and frozen pieces of the opp within radius 2 and we have a piece in the
  //trap, empty the trap. Or if there is 1 opp defender but it would be weaker our strongest piece around the trap.

  //TRAP DEFENSE, DEFENDING CAPTHREATS, RUNAWAYS----------------------------------------------------------

  //Generate defenses of capture threats
  bool oppCanCap[4];
  Bitmap wholeCapMap;
  num += genCaptureDefMoves(b,numStepsLeft,true,mv+num,NULL,oppCanCap,wholeCapMap);

  //TODO this is super branchy, reduce it?
  //TODO where the defender is NOT an elephant?
  //Steps defending a trap defense square of a trap with 1 defender with an opp nearby or an opp trap or 0 defenders if it's our trap.
  //Steps defending a trap defense square stepping forward if the trap has 1 defender without an opp nearby.
  //Filter out if the opp can cap because we handled those defenses in capture defense generation
  //For traps that we don't unconditionally defend...
  //Generate steps to defend it if the piece stepping will not be dominated by an unfrozen piece and
  //there is at least one opponent piece adjacent to it and one opponent defender of the trap
  Bitmap generateDefsOf;
  Bitmap generateDefsOfPeaceful;
  for(int i = 0; i < 2; i++)
  {
    int trapIndex = Board::PLATRAPINDICES[pla][i];
    if(oppCanCap[trapIndex])
      continue;
    loc_t kt = Board::TRAPLOCS[trapIndex];
    if(b.trapGuardCounts[pla][trapIndex] <= 1)
    {
      //If we have a single defender and that defender has nothing nearby, then only generate defenses that step forward
      //or off of the trap.
      if(b.trapGuardCounts[pla][trapIndex] == 0 || (Board::DISK[3][b.findGuard(pla,kt)] & b.pieceMaps[opp][0]).hasBits())
        generateDefsOf.setOn(kt);
      else
        generateDefsOfPeaceful.setOn(kt);
    }
    //If there's an opp on the trap and it's 2-1, and we definitely have no capture threats,
    //then also just defend it unconditionally
    else if(b.trapGuardCounts[pla][trapIndex] == 2 && b.trapGuardCounts[opp][trapIndex] == 1 && ISO(kt) &&
        oppDefinitelyNotCapturable(b,pla,kt,numStepsLeft,pStrongerMaps))
      generateDefsOf.setOn(kt);
    else
      num += BoardTrees::genContestedDefenses(b,pla,kt,numStepsLeft,mv+num);
  }
  for(int i = 0; i < 2; i++)
  {
    int trapIndex = Board::PLATRAPINDICES[opp][i];
    if(oppCanCap[trapIndex])
      continue;
    loc_t kt = Board::TRAPLOCS[trapIndex];
    if(b.trapGuardCounts[pla][trapIndex] == 1)
      generateDefsOf.setOn(kt);
    //If there's an opp on the trap and it's 2-1, and we definitely have no capture threats,
    //then also just defend it unconditionally
    else if(b.trapGuardCounts[pla][trapIndex] == 2 && b.trapGuardCounts[opp][trapIndex] == 1 && ISO(kt) &&
        oppDefinitelyNotCapturable(b,pla,kt,numStepsLeft,pStrongerMaps))
      generateDefsOf.setOn(kt);
    else
      num += BoardTrees::genContestedDefenses(b,pla,kt,numStepsLeft,mv+num);
  }
  num += BoardMoveGen::genStepsInto(b,pla,mv+num,Bitmap::adj(generateDefsOf));
  num += BoardMoveGen::genStepsForwardInto(b,pla,mv+num,Bitmap::adj(generateDefsOfPeaceful));
  num += BoardMoveGen::genStepsIntoAndOutof(b,pla,mv+num,~Board::PGOALMASKS[4][pla],generateDefsOfPeaceful);

  //TODO this is really really branchy!!
  //Generate runaways for pieces that are dominated that might be far enough advanced to be exposed
  //Exclude pieces under direct cap threat since runaways for them should be generated in genCaptureDefMoves.
  Bitmap tryRunawayMap = b.dominatedMap & b.pieceMaps[pla][0] & ~wholeCapMap & Board::PGOALMASKS[5][pla] & ~Bitmap::BMPTRAPS;
  while(tryRunawayMap.hasBits())
  {
    loc_t ploc = tryRunawayMap.nextBit();
    //TODO this has barely any effect on branchy. But maybe we can generalize it? Don't generate runaways in
    //a blockadey situation where you control the nearest trap safely?
    //Exclude rabbits on the edge of the board with a friendly piece on the side.
    if(b.pieces[ploc] == RAB && ((gX(ploc) == 0 && ISP(ploc E)) || (gX(ploc) == 7 && ISP(ploc W))))
        continue;

    num += BoardTrees::genRunawayDefs(b,pla,ploc,numStepsLeft,true,mv+num);
  }

  //PHALANXES AND STUFFING------------------------------------------------------------------
  //For each trap, if it's empty and it's a 2-2 shared control, and either the strongest pieces are equal or
  //ours are weaker, try stuffing a piece on to the trap in 2 steps.
  if(numStepsLeft >= 2)
  {
    for(int i = 0; i<4; i++)
    {
      if(b.trapGuardCounts[pla][i] == 2 && b.trapGuardCounts[opp][i] == 2)
      {
        loc_t kt = Board::TRAPLOCS[i];
        if(ISE(kt) && b.strongestAdjacentPla(pla,kt) <= b.strongestAdjacentPla(opp,kt))
          num += BoardTrees::genStuffings2S(b,mv+num,pla,kt);
      }
    }
  }

  //If there's an opponent piece on a trap, generate all phalanx steps to keep it from pushing off
  num += BoardTrees::genFramePhalanxes(b,pla,numStepsLeft,mv+num);
  //Generate phalanx moves that seal the behinds of empty traps.
  num += BoardTrees::genTrapBackPhalanxes(b,pla,numStepsLeft,mv+num);
  //Generate phalanx moves that do various misc things
  num += BoardTrees::genBackRowMiscPhalanxes(b,pla,numStepsLeft,mv+num);
  //Generate phalanxes against the opp ele
  num += BoardTrees::genOppElePhalanxes(b,pla,numStepsLeft,mv+num);

  //TODO if a single step generates a phalanx against an elephant, play it?
  //TODO possibly call qsearch blockade search

  //TODO generate forward steps by the elephant if it's in the back 3 rows and not dominating anything?
  //TODO generate forward steps by all non-rabbit pieces if on the backmost row?

  //GENERAL PIECE MOVEMENT AND ALIGNMENT----------------------------------------------------
  //Broad branchy elephant move generation
  num += BoardTrees::genUsefulElephantMoves(b,pla,numStepsLeft,mv+num);

  //Generate steps by the camel away from a nearby elephant if the camel is in a hostage-likely situation
  num += BoardTrees::genCamelHostageRunaways(b,pla,numStepsLeft,mv+num);

  //TODO maybe prune advances in 1 step?
  //Generate steps by rabbits near the edge to block opposing rabbits
  num += BoardTrees::genRabSideAdvances(b,pla,numStepsLeft,mv+num);

  //GOAL ATTACKS----------------------------------------------------------------------------
  //Severe goal threats
  num += BoardTrees::genSevereGoalThreats(b,pla,numStepsLeft,mv+num);

  //Goal attacky moves
  num += BoardTrees::genGoalAttacks(b,pla,numStepsLeft,mv+num);

  //BLOCKADES-------------------------------------------------------------------------------
  num += genBlockadeMoves(b,mv+num);

  if(hm != NULL)
  {
    for(int i = 0; i<num; i++)
      hm[i] = 0;
  }
  return num;
}

