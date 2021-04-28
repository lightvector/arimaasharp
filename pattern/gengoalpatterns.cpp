/*
 * gengoalpatterns.cpp
 * Author: davidwu
 */

#include <sstream>
#include "../core/global.h"
#include "../core/rand.h"
#include "../core/timer.h"
#include "../board/board.h"
#include "../board/boardtrees.h"
#include "../pattern/gengoalpatterns.h"
#include "../search/searchutils.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"

static int genWinConditionMoves(Board& b, move_t* mv, int* hm)
{
  SearchMoveGen::RelevantArea rel;
  loc_t rloc;
  if(SearchMoveGen::getGoalDefRelArea(b,4-b.step,rloc,rel))
    return SearchMoveGen::genGoalDefMoves(b,mv,hm,4-b.step,rloc,rel);
  return -1;
}

//static int winDefSearchHashSize = 0; //Must be power of 2
//static pair<uint64_t,int>* winDefSearchHash = NULL;

static int winDefSearchHelper(Board& b, Bitmap fakeFrozenMap, pla_t pla, int origStepsLeft, int stepsLeft,
    step_t lastStep)
{
  //Can't undo the position
  if(stepsLeft < origStepsLeft && b.posCurrentHash == b.posStartHash)
    return origStepsLeft;

  //int hashIdx = b.sitCurrentHash & (winDefSearchHashSize - 1);

  //Can't go any further - test if opponent can win
  if(stepsLeft <= 0)
  {
    pla_t opp = gOpp(pla);

    int oppGoalDist;
    //if(winDefSearchHash[hashIdx].first == b.sitCurrentHash)
    //  oppGoalDist = winDefSearchHash[hashIdx].second;
    //else
    //{
      oppGoalDist = BoardTrees::goalDist(b,opp,4);
      //winDefSearchHash[hashIdx].first = b.sitCurrentHash;
      //winDefSearchHash[hashIdx].second = oppGoalDist;
    //}

    //Opponent can still goal? - we lose in number of steps we had, plus number of steps for opp to goal
    if(oppGoalDist < 5)
      return origStepsLeft + oppGoalDist;

    return 9;
  }

  //Generate win defense moves
  move_t mv[256];
  int hm[256];
  int num = genWinConditionMoves(b,mv,hm);

  //No defenses - we lose right away...
  if(num == 0)
    return origStepsLeft;

  //No win threat! We're done!
  if(num == -1)
    return 9;

  //Try all the possible moves
  int bestStepsToLose = 0;
  int lastk0 = gSrc(lastStep);
  int lastk1 = gDest(lastStep);
  for(int m = 0; m<num; m++)
  {
    move_t move = mv[m];
    int nsInMove = numStepsInMove(move);

    //Move may not move a piece that is fake frozen
    //Check that while computing the new map
    Bitmap newFakeFrozenMap = fakeFrozenMap;
    if(fakeFrozenMap.hasBits())
    {
      bool movesFakeFrozen = false;
      for(int i = 0; i<nsInMove; i++)
      {
        step_t step = getStep(move,i);
        if(step == PASSSTEP || step == QPASSSTEP)
          continue;

        if(fakeFrozenMap.isOne(gSrc(step)))
        {
          movesFakeFrozen = true;
          break;
        }

        //Update fake freezing
        //We unset the bits whenever we step to a location, or we pull something.
        if(b.owners[gSrc(step)] == pla)
          newFakeFrozenMap &= ~Board::RADIUS[1][gDest(step)];
        else
          newFakeFrozenMap &= ~Board::RADIUS[1][gSrc(step)];
      }
      if(movesFakeFrozen)
        continue;
    }

    step_t newLastStep = ERRSTEP;
    if(nsInMove == 1)
    {
      step_t step = getStep(move,0);
      int k0 = gSrc(step);
      int k1 = gDest(step);
      if(k0 == lastk1 && k1 == lastk0)
        continue;
      newLastStep = step;
    }

    Board copy = b;
    copy.makeMove(move);

    int stepsToLose = winDefSearchHelper(copy,newFakeFrozenMap,pla,origStepsLeft,stepsLeft-nsInMove,newLastStep);
    if(stepsToLose > bestStepsToLose)
    {
      bestStepsToLose = stepsToLose;
      if(stepsToLose >= 9)  //Beta pruning
        return stepsToLose;
    }
  }
  return bestStepsToLose;
}

static int winDefSearch(const Board& b, Bitmap fakeFrozenMap)
{
  //if(winDefSearchHash == NULL) TODO test this sort of hash in the main search too
  //{
  //  winDefSearchHash = new pair<uint64_t,int>[1024];
  //  winDefSearchHashSize = 1024;
  //}

  int numStepsLeft = 4-b.step;
  int bestStepsToLose = 0;
  for(int maxSteps = 1; maxSteps <= numStepsLeft; maxSteps++)
  {
    Board copy = b;
    int stepsToLose = winDefSearchHelper(copy,fakeFrozenMap,copy.player,numStepsLeft,maxSteps,ERRSTEP);
    if(stepsToLose >= 9)
      return stepsToLose;

    if(stepsToLose > bestStepsToLose)
      bestStepsToLose = stepsToLose;
  }
  return bestStepsToLose;
}

static double sq(double x) {return x * x;}
static double cube(double x) {return x * x * x;}

static int choose(Rand& rand, vector<double> weights)
{
  double sum = 0;
  for(int i = 0; i<(int)weights.size(); i++)
    sum += weights[i];
  double r = rand.nextDouble(sum);
  for(int i = 0; i<(int)weights.size(); i++)
  {
    r -= weights[i];
    if(r < 0)
      return i;
  }
  return weights.size()-1;
}

static const int TEST_RESULT_STOP = 0;
static const int TEST_RESULT_GOAL = 1;
static const int TEST_RESULT_IMPOSSIBLE = 2;

static int testGoalPattern(const Pattern& pat, Rand& rand, int stepsLeft, bool print, Board& stopBoard)
{
  Board b;
  b.setPlaStep(GOLD,4-stepsLeft);

  Bitmap alreadySet;

  Condition silvRab = Condition::ownerIs(ERRLOC,SILV) &&
      Condition::pieceIs(ERRLOC,RAB);
  Condition silvNonRab = Condition::ownerIs(ERRLOC,SILV) &&
      !Condition::pieceIs(ERRLOC,RAB);

  //Assign guaranteed silver pieces
  for(int i = 0; i<BSIZE; i++)
  {
    if(i & 0x88)
      continue;
    if(pat.getListLen(i) != 1)
      continue;
    const Condition& cond = pat.getCondition(pat.getList(i)[0]);
    if(silvNonRab.impliedBy(cond))
    {
      b.setPiece(i,SILV,rand.nextInt(CAT,ELE));
      alreadySet.setOn(i);
    }
    else if(silvRab.impliedBy(cond))
    {
      b.setPiece(i,SILV,RAB);
      alreadySet.setOn(i);
    }
  }

  //Compute densities of each piece type
  double frozenDensity = sq(rand.nextDouble(0,0.8));
  double goldBonus = rand.nextDouble(3,112);
  double silvBonus = sq(rand.nextDouble(1,6.5));
  double emptyBonus = sq(rand.nextDouble(1,8.5));
  double silvRabbitVCat_A = cube(rand.nextDouble()) + 0.00000000001;
  double silvRabbitVCat_B = cube(rand.nextDouble()) + 0.00000000001;
  double silvRabbitVCat = silvRabbitVCat_A / (silvRabbitVCat_A + silvRabbitVCat_B);
  silvRabbitVCat = silvRabbitVCat * (rand.nextDouble() < 0.33 ?  0.1 : rand.nextDouble());

  //Gather together what will be placed at each square
  vector<loc_t> locsVec;
  vector<vector<pla_t> > possiblePlasVec;
  vector<vector<double> > plaProbsVec;
  vector<piece_t> pieceIfOppVec;
  vector<piece_t> pieceIfPlaVec;
  vector<bool> freezeIfOppVec;
  for(int i = 0; i<BSIZE; i++)
  {
    if(i & 0x88)
      continue;
    if(alreadySet.isOne(i))
      continue;
    bool maybeFreeze = rand.nextDouble() < frozenDensity;
    b.setPiece(i,NPLA,EMP);
    bool emptyPossible = pat.matchesAt(b,i);

    //The strongest opp possible if opp
    piece_t strongestOppPiecePossible = EMP;
    for(piece_t piece = ELE; piece >= RAB; piece--)
    {
      b.setPiece(i,GOLD,piece);
      if(pat.matchesAtAssumeFrozen(b,i,false))
      {strongestOppPiecePossible = piece; break;}
    }

    //And the strongest frozen opp possible if frozen
    piece_t strongestFrozenOppPiecePossible = EMP;
    for(piece_t piece = ELE; piece >= RAB; piece--)
    {
      b.setPiece(i,GOLD,piece);
      if(pat.matchesAtAssumeFrozen(b,i,true))
      {strongestFrozenOppPiecePossible = piece; break;}
    }

    //The weakest nonrab possible if nonrab is possible
    piece_t weakestPlaPiecePossible = EMP;
    for(piece_t piece = CAT; piece <= ELE; piece++)
    {
      b.setPiece(i,SILV,piece);
      if(pat.matchesAt(b,i))
      {weakestPlaPiecePossible = piece; break;}
    }

    //And a rabbit if rab is possible
    b.setPiece(i,SILV,RAB);
    bool rabPossible = pat.matchesAt(b,i);

    vector<pla_t> possiblePlas;
    vector<double> plaProbs;
    piece_t pieceIfOpp = EMP;
    piece_t pieceIfPla = EMP;
    bool freezeIfOpp = false;
    if(emptyPossible)
    {possiblePlas.push_back(NPLA); plaProbs.push_back(emptyBonus);}
    if(strongestOppPiecePossible != EMP || strongestFrozenOppPiecePossible != EMP)
    {
      if(maybeFreeze && strongestFrozenOppPiecePossible > strongestOppPiecePossible)
      {possiblePlas.push_back(GOLD); plaProbs.push_back(goldBonus); freezeIfOpp = true; pieceIfOpp = strongestFrozenOppPiecePossible;}
      else if(strongestOppPiecePossible != EMP)
      {possiblePlas.push_back(GOLD); plaProbs.push_back(goldBonus); pieceIfOpp = strongestOppPiecePossible;}
    }
    if(weakestPlaPiecePossible != EMP || rabPossible)
    {
      if(rabPossible && (weakestPlaPiecePossible == EMP || rand.nextDouble() < silvRabbitVCat))
      {possiblePlas.push_back(SILV); plaProbs.push_back(silvBonus); pieceIfPla = RAB;}
      else
      {possiblePlas.push_back(SILV); plaProbs.push_back(silvBonus); pieceIfPla = weakestPlaPiecePossible;}
    }

    //If only frozen opp is possible, then allow that, even if maybeFreeze isn't set
    if(possiblePlas.size() == 0 && strongestFrozenOppPiecePossible != EMP)
    {possiblePlas.push_back(GOLD); plaProbs.push_back(goldBonus); freezeIfOpp = true; pieceIfOpp = strongestFrozenOppPiecePossible;}

    if(possiblePlas.size() == 0)
    {
      cout << "Impossible: No possible piece " << Board::writeLoc(i) << endl << pat << endl;
      return TEST_RESULT_IMPOSSIBLE;
    }

    locsVec.push_back(i);
    possiblePlasVec.push_back(possiblePlas);
    plaProbsVec.push_back(plaProbs);
    pieceIfOppVec.push_back(pieceIfOpp);
    pieceIfPlaVec.push_back(pieceIfPla);
    freezeIfOppVec.push_back(freezeIfOpp);
  }

  //Actually randomize and place all the pieces
  Bitmap doFreezeMap;
  for(int i = 0; i<(int)locsVec.size(); i++)
  {
    loc_t loc = locsVec[i];
    pla_t pla = possiblePlasVec[i][choose(rand,plaProbsVec[i])];
    if(pla == NPLA)
    {b.setPiece(loc,NPLA,EMP);}
    else if(pla == GOLD)
    {b.setPiece(loc,GOLD,pieceIfOppVec[i]); if(freezeIfOppVec[i]) doFreezeMap.setOn(loc);}
    else
    {b.setPiece(loc,SILV,pieceIfPlaVec[i]);}
  }

  //In a random order, delete all opp pieces next to things that were frozen and rechoose the piece
  //And also if the square is next to an opp piece that has no adjacent pla piece geq rabbit, and it is possible, add one.
  int perm[locsVec.size()];
  for(int i = 0; i<(int)locsVec.size(); i++)
    perm[i] = i;
  for(int i = (int)locsVec.size() - 1; i > 0; i--)
  {
    int j = rand.nextUInt(i+1);
    int temp = perm[i];
    perm[i] = perm[j];
    perm[j] = temp;
  }
  for(int p = 0; p<(int)locsVec.size(); p++)
  {
    int i = perm[p];
    loc_t loc = locsVec[i];
    if(!(Bitmap::adj(doFreezeMap)).isOne(loc))
      continue;
    if(b.owners[loc] != GOLD)
      continue;
    if(possiblePlasVec[i].size() == 1) //We can't delete this piece, it's a forced choice
      continue;

    doFreezeMap.setOff(loc);
    for(int j = 0; j<(int)possiblePlasVec[i].size(); j++)
    {
      if(possiblePlasVec[i][j] == GOLD)
      {
        possiblePlasVec[i].erase(possiblePlasVec[i].begin()+j);
        plaProbsVec[i].erase(plaProbsVec[i].begin()+j);
        j--;
      }
    }
    if(possiblePlasVec[i].size() == 0)
    {
      cout << "Impossible: No possible piece when removing adj to frozen, " <<
      Board::writeLoc(loc) << endl << pat << endl << b << endl << doFreezeMap << endl;
      return TEST_RESULT_IMPOSSIBLE;
    }
    pla_t pla = possiblePlasVec[i][choose(rand,plaProbsVec[i])];
    if(pla == NPLA)
    {b.setPiece(loc,NPLA,EMP);}
    else if(pla == GOLD)
    {DEBUGASSERT(false);}
    else
    {b.setPiece(loc,SILV,pieceIfPlaVec[i]);}
  }

  //And also if the square is next to an opp piece that has no adjacent pla piece geq rabbit, and it is possible, add one.
  vector<loc_t> locsToMaybePromotePla;
  locsToMaybePromotePla.insert(locsToMaybePromotePla.begin(),locsVec.begin(),locsVec.end());
  for(int i = 0; i<BSIZE; i++) //Also add in pieces that were specified in the initial pattern as promotion candidates
  {
    if(i & 0x88)
      continue;
    if(alreadySet.isOne(i) && b.owners[i] == SILV)
      locsToMaybePromotePla.push_back(i);
  }

  int perm2[locsToMaybePromotePla.size()];
  for(int i = 0; i<(int)locsToMaybePromotePla.size(); i++)
    perm2[i] = i;
  for(int i = (int)locsToMaybePromotePla.size() - 1; i > 0; i--)
  {
    int j = rand.nextUInt(i+1);
    int temp = perm2[i];
    perm2[i] = perm2[j];
    perm2[j] = temp;
  }

  //ASSSUMES THAT IF a piece is a valid opp for a square in the pattern, then any weaker piece is too
  for(int p = 0; p<(int)locsToMaybePromotePla.size(); p++)
  {
    int i = perm2[p];
    loc_t loc = locsToMaybePromotePla[i];
    if(!(Bitmap::adj(doFreezeMap)).isOne(loc))
      continue;
    int oppsNeedingPlaDominator[4];
    int numOppsNeedingPlaDominator = 0;
    for(int j = 0; j<4; j++)
    {
      if(!Board::ADJOKAY[j][loc])
        continue;
      int adj = loc + Board::ADJOFFSETS[j];
      if(b.owners[adj] != GOLD || !doFreezeMap.isOne(adj))
        continue;
      bool adjPlaDominating = false;
      for(int k = 0; k<4; k++)
      {
        if(!Board::ADJOKAY[k][adj])
          continue;
        int adjadj = adj + Board::ADJOFFSETS[k];
        if(b.owners[adjadj] == SILV && b.pieces[adjadj] > b.pieces[adj])
        {adjPlaDominating = true; break;}
      }
      if(adjPlaDominating)
        continue;
      oppsNeedingPlaDominator[numOppsNeedingPlaDominator++] = adj;
    }
    if(numOppsNeedingPlaDominator == 0)
      continue;

    piece_t plaPiecesPossible[7];
    int numPlaPiecesPossible = 0;
    Board copy = b;
    piece_t startingPiece = CAT;
    if(b.owners[loc] == SILV && b.pieces[loc] > startingPiece) //Don't weaken what's on the square already
      startingPiece = b.pieces[loc];
    for(piece_t piece = startingPiece; piece <= ELE; piece++)
    {
      copy.setPiece(loc,SILV,piece);
      if(pat.matchesAt(copy,loc))
      {
        plaPiecesPossible[numPlaPiecesPossible++] = piece;
        //If this pla piece is already sufficient to dominate everything as-is, we don't need anything stronger
        bool strongerThanAll = true;
        for(int j = 0; j<numOppsNeedingPlaDominator; j++)
          if(b.pieces[oppsNeedingPlaDominator[j]] >= piece)
          {strongerThanAll = false; break;}
        if(strongerThanAll)
          break;
      }
    }
    if(numPlaPiecesPossible > 0)
    {
      //Choose a random possible pla piece and set it to that, then set all the opps one below that if not already weaker.
      piece_t plaPiece = plaPiecesPossible[rand.nextUInt(numPlaPiecesPossible)];
      b.setPiece(loc,SILV,plaPiece);
      doFreezeMap.setOff(loc); //Just in case we somehow replaced a frozen opp piece here
      for(int j = 0; j<numOppsNeedingPlaDominator; j++)
        if(b.pieces[oppsNeedingPlaDominator[j]] >= plaPiece)
          b.setPiece(oppsNeedingPlaDominator[j],GOLD,plaPiece-1);
    }
  }

  b.refreshStartHash();
  int dist = winDefSearch(b,doFreezeMap);
  if(dist < 9)
    return TEST_RESULT_GOAL;

  if(print)
  {
    cout << b << endl;
    cout << doFreezeMap << endl;
  }
  stopBoard = b;
  return TEST_RESULT_STOP;
}

static double global_randomTestTime = 0;

//Returns the proper result, ASSUMING that no rabbits are already on the goal line
//testPatExclusive - used for the randomized solver - if adding a new cond, can clear all but the new cond so
//that the randomization will always test the desired case
static int testGoalPattern(const Pattern& testPatExclusive, uint64_t seed, int stepsLeft, int reps)
{
  Pattern pat = testPatExclusive;
  pat.simplifyGoalPattern(true);

  ClockTimer clock;

  pat = testPatExclusive;
  pat.simplifyGoalPattern(true);

  Rand rand(seed);
  bool stopped = false;
  clock.reset();
  Board stopBoard;
  for(int i = 0; i<reps; i++)
  {
    int result = testGoalPattern(pat,rand,stepsLeft,false,stopBoard);
    if(result == TEST_RESULT_IMPOSSIBLE)
    {DEBUGASSERT(false);}
    else if(result == TEST_RESULT_STOP)
    {
      stopped = true; break;
    }
    else if(result == TEST_RESULT_GOAL)
    {continue;}
    else
    {DEBUGASSERT(false);}
  }
  global_randomTestTime += clock.getSeconds();

  if(stopped)
    return TEST_RESULT_STOP;
  return TEST_RESULT_GOAL;
}

static void tryAddAll(Pattern& pat, Rand& rand, const Condition& newCond, const Bitmap& whichSquares,
    int stepsLeft, int center, int repsPerCond, int printVerbosity, Bitmap& whichSquaresAdded)
{
  for(int i = 0; i<BSIZE; i++)
  {
    loc_t loc = Board::SPIRAL[center][63-i];
    if(!whichSquares.isOne(loc))
      continue;

    //Don't add redundant conditions
    if(pat.isAnyImpliedBy(newCond,loc))
      continue;

    Pattern newPat = pat;
    int lenBefore = newPat.getListLen(loc);
    newPat.addConditionToLoc(newPat.getOrAddCondition(newCond),loc);
    //If simplification kills the new cond, then there's nothing to be done
    //When simplifying for generation, to help the solver (and to make random trials more useful)
    //we forbid rabbits on goal lines (true).
    newPat.simplifyGoalPattern(true);
    int lenAfter = newPat.getListLen(loc);
    if(lenBefore == lenAfter)
      continue;

    //Clear all but the new cond so that randomized testing tests this case always
    Pattern newPatExclusive = newPat;
    newPatExclusive.clearLoc(loc);
    newPatExclusive.addConditionToLoc(newPatExclusive.getOrAddCondition(newCond),loc);

    int result = testGoalPattern(newPatExclusive,rand.nextUInt64(),stepsLeft,repsPerCond);
    if(result == TEST_RESULT_STOP)
    {
      if(printVerbosity > 1) cout << "stopped " << Board::writeLoc(loc) << " " << newCond << endl;
      continue;
    }
    if(result == TEST_RESULT_IMPOSSIBLE)
    {
      if(printVerbosity > 1) cout << "impossible " << Board::writeLoc(loc) << " " << newCond << endl;
      continue;
    }
    pat.addConditionToLoc(pat.getOrAddCondition(newCond),loc);
    whichSquaresAdded.setOn(loc);
    //When simplifying for use, we allow rabbits on the goal line in conditions, and then we merely have
    //to remember not to use the pattern matching when a rabbit is on the goal line.
    pat.simplifyGoalPattern(false);
    if(printVerbosity > 1) cout << "success " << Board::writeLoc(loc) << " " << newCond << endl;
    if(printVerbosity > 1) cout << pat << endl;
  }
}
static void tryAddAll(Pattern& pat, Rand& rand, const Condition& newCond, const Bitmap& whichSquares,
    int stepsLeft, int center, int repsPerCond, int printVerbosity)
{
  Bitmap whichSquaresAdded;
  tryAddAll(pat,rand,newCond,whichSquares,stepsLeft,center,repsPerCond,printVerbosity,whichSquaresAdded);
}

bool GenGoalPatterns::genGoalPattern(const Pattern& basePat, uint64_t seed, int stepsLeft, loc_t center, int repsPerCond,
    int printVerbosity, Pattern& output)
{
  global_randomTestTime = 0;

  Rand rand(seed);
  Pattern pat = basePat;
  pat.simplifyGoalPattern(false);
  Bitmap unknownMap;
  Condition empty = Condition::ownerIs(ERRLOC,NPLA);
  int emptyIdx = pat.getOrAddCondition(empty);
  for(int i = 0; i<64; i++)
  {
    if(i & 0x88)
      continue;
    if(basePat.getListLen(i) != 1)
      continue;
    const Condition& cond = basePat.getCondition(basePat.getList(i)[0]);
    if(cond.type == Condition::UNKNOWN)
    {
      unknownMap.setOn(i);
      pat.clearLoc(i);
      pat.addConditionToLoc(emptyIdx,i);
    }
  }

  vector<loc_t> silvNonRabLocs;
  Condition silvNonRab = Condition::ownerIs(ERRLOC,SILV) &&
      !Condition::pieceIs(ERRLOC,RAB);
  for(int i = 0; i<64; i++)
  {
    if(i & 0x88)
      continue;
    if(pat.getListLen(i) != 1)
      continue;
    const Condition& cond = pat.getCondition(pat.getList(i)[0]);
    if(silvNonRab.impliedBy(cond))
      silvNonRabLocs.push_back(i);
  }

  Bitmap knownMapExpanded = ~unknownMap;
  for(int i = 0; i<stepsLeft+1; i++)
    knownMapExpanded |= Bitmap::adj(knownMapExpanded);
  for(int i = 0; i<64; i++)
  {
    if(i & 0x88)
      continue;
    if(!knownMapExpanded.isOne(i))
      pat.clearLoc(i,Pattern::IDX_TRUE);
  }

  int result = testGoalPattern(pat,rand.nextUInt64(),stepsLeft, 1000 + repsPerCond);
  if(result == TEST_RESULT_STOP)
  {cout << "Pattern already stoppable\n" << basePat << endl; return false;}
  else if(result == TEST_RESULT_IMPOSSIBLE)
  {cout << "Pattern impossible?\n" << basePat << endl; return false;}

  if(printVerbosity > 0) cout << "Seed Pattern:" << endl << basePat << endl;

  int C_FRZ = Condition::FROZEN;
  int C_OIS = Condition::OWNER_IS;
  int C_LEQ = Condition::LEQ_THAN_LOC;
  int C_LT = Condition::LESS_THAN_LOC;

  Bitmap dontAddOppRabsHere;
  Bitmap dontAddFrozenOppRabsHere;
  for(int onlyTraps = 0; onlyTraps <= 1; onlyTraps++)
  {
    Bitmap whichSquares = onlyTraps ? (unknownMap & Bitmap::BMPTRAPS & knownMapExpanded) : (unknownMap & knownMapExpanded);
    for(int addFrozen = 0; addFrozen <= 1; addFrozen++)
    {
      Condition isOpp = Condition(addFrozen == 1 ? C_FRZ : C_OIS,ERRLOC,GOLD,true,NULL,NULL);
      Bitmap whichAdded;
      tryAddAll(pat,rand,isOpp,whichSquares,stepsLeft,center,repsPerCond,printVerbosity,whichAdded);

      for(int lt = 0; lt <= 1; lt++)
      {
        for(int i = 0; i<(int)silvNonRabLocs.size(); i++)
        {
          loc_t silvLoc = silvNonRabLocs[i];
          Condition isOppLeq = Condition(addFrozen == 1 ? C_FRZ : C_OIS,ERRLOC,GOLD,true,NULL,NULL)
              && Condition(lt == 0 ? C_LEQ : C_LT,ERRLOC,silvLoc,true,NULL,NULL);
          tryAddAll(pat,rand,isOppLeq,whichSquares,stepsLeft,center,repsPerCond,printVerbosity,whichAdded);
        }
      }

      for(int lt = 0; lt <= 1; lt++)
      {
        for(int i = 0; i<(int)silvNonRabLocs.size(); i++)
        {
          for(int lt2 = 0; lt2 <= 1; lt2++)
          {
            for(int i2 = 0; i2<(int)silvNonRabLocs.size(); i2++)
            {
              loc_t silvLoc = silvNonRabLocs[i];
              loc_t silvLoc2 = silvNonRabLocs[i2];
              if(silvLoc == silvLoc2)
                continue;
              if(silvLoc > silvLoc2)
                continue;
              Condition isOppLeq = Condition(addFrozen == 1 ? C_FRZ : C_OIS,ERRLOC,GOLD,true,NULL,NULL)
                  && Condition(lt == 0 ? C_LEQ : C_LT,ERRLOC,silvLoc,true,NULL,NULL)
                  && Condition(lt2 == 0 ? C_LEQ : C_LT,ERRLOC,silvLoc2,true,NULL,NULL);
              tryAddAll(pat,rand,isOppLeq,whichSquares,stepsLeft,center,repsPerCond,printVerbosity,whichAdded);
            }
          }
        }
      }

      if(addFrozen)
        dontAddFrozenOppRabsHere |= whichAdded;
      else
        dontAddOppRabsHere |= whichAdded;

      Bitmap whichToAddRab = whichSquares & ~(addFrozen ? dontAddFrozenOppRabsHere : Bitmap()) & ~dontAddOppRabsHere;

      Condition isOppRab = Condition(addFrozen == 1 ? C_FRZ : C_OIS,ERRLOC,GOLD,true,NULL,NULL) && Condition::pieceIs(ERRLOC,RAB);
      tryAddAll(pat,rand,isOppRab,whichToAddRab,stepsLeft,center,repsPerCond,printVerbosity);

      if(addFrozen == 0)
      {
        Condition isPla = Condition::ownerIs(ERRLOC,SILV);
        tryAddAll(pat,rand,isPla,whichSquares,stepsLeft,center,repsPerCond,printVerbosity);
        Condition isPlaP = Condition::ownerIs(ERRLOC,SILV) && !Condition::pieceIs(ERRLOC,RAB);
        tryAddAll(pat,rand,isPlaP,whichSquares,stepsLeft,center,repsPerCond,printVerbosity);
      }
    }
  }

  if(printVerbosity > 1) cout << "FINAL VERIFY" << endl;

  //Final verification
  int finalResult = testGoalPattern(pat,rand.nextUInt64(),stepsLeft,repsPerCond*8);

  cout << "RandomTestTime: " << global_randomTestTime << endl;

  if(finalResult == TEST_RESULT_STOP)
  {
    if(printVerbosity > 1) cout << pat << endl;
    if(printVerbosity > 1) cout << "stopped " << endl;
    return false;
  }
  if(finalResult == TEST_RESULT_IMPOSSIBLE)
  {
    if(printVerbosity > 1) cout << pat << endl;
    if(printVerbosity > 1) cout << "impossible " << endl;
    return false;
  }

  if(printVerbosity > 0) cout << "success " << endl;
  if(printVerbosity > 0) cout << pat << endl;

  output = pat;
  return true;
}






/*

static const int NUM_RANDOM_REPS = 10000;
static const int NUM_RANDOM_REPS_FAR = 1500;
static const int VERIFICATION_NUM_REPS = 400000;
static const int OUTPUT_EXAMPLES_NUMREPS = 10000;
static const int SUPPRESS_GOALSTOP_REPORT_NUMREPS = 100;

static Bitmap getFreeLocsMap(const char basePattern[64])
{
  Bitmap freeLocsMap;
  for(int i = 0; i<64; i++)
    if(basePattern[i] == '?')
      freeLocsMap.setOn(i);
  return freeLocsMap;
}

static loc_t findGoldRabbitLoc(const GoalPatternInput& pattern)
{
  loc_t goldRabbitLoc = ERRLOC;
  for(int y = 7; y>=0 && goldRabbitLoc == ERRLOC; y--)
    for(int x = 0; x<8 && goldRabbitLoc == ERRLOC; x++)
      if(pattern.pattern[y*8+x] == 'R')
        goldRabbitLoc = y*8+x;
  if(goldRabbitLoc == ERRLOC)
  {
    string s;
    for(int y = 7; y>=0 && goldRabbitLoc == ERRLOC; y--)
    {
      for(int x = 0; x<8 && goldRabbitLoc == ERRLOC; x++)
        s += pattern.pattern[y*8+x];
      s += "\n";
    }
    Global::fatalError("Could not find rabbit in pattern!\n" + s);
  }
  return goldRabbitLoc;
}

static int findLabeledPieces(const char basePattern[64], loc_t labeledPieceLocs[2])
{
  int numLabeledPieces = 0;
  for(int i = 0; i<64; i++)
  {
    if(basePattern[i] == 'P')
    {
      if(numLabeledPieces != 0)
        Global::fatalError("More than one P in goal pattern input!");
      labeledPieceLocs[0] = i;
      numLabeledPieces++;
    }
  }
  for(int i = 0; i<64; i++)
  {
    if(basePattern[i] == 'Q')
    {
      if(numLabeledPieces != 1)
        Global::fatalError("Q without P or more than one Q in goal pattern input!");
      labeledPieceLocs[1] = i;
      numLabeledPieces++;
    }
  }
  return numLabeledPieces;
}

static int min(int a, int b) {return a < b ? a : b;}

struct OppPiecesAllowed
{
  public:
  static const int CONSTRAINT_NONE = 0;
  static const int CONSTRAINT_LESS = 1;
  static const int CONSTRAINT_LEQ = 2;
  static const int CONSTRAINT_ANY = 3;

  bool allowRabbit;
  int p0Constraint;
  int p1Constraint;
  int strength;

  OppPiecesAllowed()
  {allowRabbit = false; p0Constraint = CONSTRAINT_NONE; p1Constraint = CONSTRAINT_NONE; strength = 0;}

  bool operator==(const OppPiecesAllowed other) const
  {
    return allowRabbit == other.allowRabbit && p0Constraint == other.p0Constraint &&
        p1Constraint == other.p1Constraint && strength == other.strength;
  }

  bool operator!=(const OppPiecesAllowed other) const
  {
    return !(*this == other);
  }

  piece_t getStrongestAllowed(const piece_t labeledStrengths[2]) const
  {
    piece_t piece = ELE;
    if(p0Constraint == OppPiecesAllowed::CONSTRAINT_NONE)
      piece = min(piece,RAB);
    else if(p0Constraint == OppPiecesAllowed::CONSTRAINT_LESS)
      piece = min(piece,labeledStrengths[0]-1);
    else if(p0Constraint == OppPiecesAllowed::CONSTRAINT_LEQ)
      piece = min(piece,labeledStrengths[0]);
    if(p1Constraint == OppPiecesAllowed::CONSTRAINT_NONE)
      piece = min(piece,RAB);
    else if(p1Constraint == OppPiecesAllowed::CONSTRAINT_LESS)
      piece = min(piece,labeledStrengths[1]-1);
    else if(p1Constraint == OppPiecesAllowed::CONSTRAINT_LEQ)
      piece = min(piece,labeledStrengths[1]);
    if(piece == RAB && !allowRabbit)
      piece = EMP;
    return piece;
  }
};

//Randomly generate a test board containing the base pattern satisfing all the constraints for the pieces desired
//and that specifically contains the piece for the tested location. Marks the results in the board b and also the bitmap
//of pieces that we will fakely pretend are frozen.
static void getTestBoard(const char basePattern[64], int numOppSteps, bool assumeTrapDef, int goldRabbitRow,
    int numLabeledPieces, const loc_t labeledPieceLocs[2],
    const OppPiecesAllowed oppAllowed[64], const OppPiecesAllowed frozenOppAllowed[64], const Bitmap& freeLocsMap,
    loc_t testLoc, bool testingFreeze,
    Board& b, Bitmap& fakeFreezeMap)
{
  if(numOppSteps < 1 || numOppSteps > 4)
    Global::fatalError("Invalid number of opp steps: " + Global::intToString(numOppSteps));

  piece_t labeledStrengths[2];

  if(numLabeledPieces == 2)
  {
    const char* const strPossibilities[9] = {"35","53","44","54","45","34","43","46","64"};
    const int strFrequencies[9] = {5, 5, 3, 2, 2, 2, 2, 1, 1};
    const char* selectedStrs = strPossibilities[Rand::rand.nextUInt(strFrequencies,9)];
    labeledStrengths[0] = selectedStrs[0] - '0';
    labeledStrengths[1] = selectedStrs[1] - '0';
  }
  else //(numLabeledPieces == 1) or (numLabeledPieces == 0)
  {
    labeledStrengths[0] = HOR;
    labeledStrengths[1] = HOR;
  }


  Board baseBoard;
  for(int i = 0; i<64; i++)
  {
    switch(basePattern[i])
    {
    case '.': baseBoard.setPiece(i,NPLA,EMP); break;
    case '?': baseBoard.setPiece(i,NPLA,EMP); break;
    case 'R': baseBoard.setPiece(i,GOLD,RAB); break;
    case 'P': baseBoard.setPiece(i,GOLD,labeledStrengths[0]); break;
    case 'Q': baseBoard.setPiece(i,GOLD,labeledStrengths[1]); break;
    case 'C': baseBoard.setPiece(i,GOLD,CAT); break;
    case 'r': baseBoard.setPiece(i,SILV,RAB); break;
    case 'w': baseBoard.setPiece(i,SILV,labeledStrengths[0]-1); break;
    case 'x': baseBoard.setPiece(i,SILV,labeledStrengths[0]); break;
    case 'y': baseBoard.setPiece(i,SILV,labeledStrengths[1]-1); break;
    case 'z': baseBoard.setPiece(i,SILV,labeledStrengths[1]); break;
    case 'q': baseBoard.setPiece(i,SILV,min(labeledStrengths[0],labeledStrengths[1])); break;
    case 's': baseBoard.setPiece(i,SILV,min(labeledStrengths[0],labeledStrengths[1])-1); break;
    default:
      Global::fatalError(string("Unknown pattern character: ") + basePattern[i]);
    }
  }
  baseBoard.setPlaStep(SILV,4-numOppSteps);
  baseBoard.refreshStartHash();

  //Try a bunch of times to see if we can fill successfully
  //We might fail because the capture rule causes us to kill a piece on the trap, and we wanted to test that piece.
  for(int tries = 0; tries < 5; tries++)
  {
    b = baseBoard;
    fakeFreezeMap = Bitmap();

    double detrimentalRabbitProb = Rand::rand.nextDouble();
    double rabbitProb = Rand::rand.nextDouble();
    double pieceProb = 1-(1-Rand::rand.nextDouble())*(1-Rand::rand.nextDouble()*0.7);

    double addPlaPiecesProb = 0.6;
    double addPlaPieceProb = Rand::rand.nextDouble() * 0.94;
    double addPlaRabsProb = 0.3;
    double addPlaRabProb = Rand::rand.nextDouble() * 0.3;
    bool doAddPlaPieces = Rand::rand.nextDouble() < addPlaPiecesProb;
    bool doAddPlaRabs = Rand::rand.nextDouble() < addPlaRabsProb;

    bool plaPieceRabBlockBonus = 0.3;

    double plugFailProb = Rand::rand.nextDouble() * 0.14;

    double plugRabProb = Rand::rand.nextDouble() * 0.15;

    double addPlaPlugProb = 0.44;
    bool doAddPlaPlug = Rand::rand.nextDouble() < addPlaPlugProb;

    double addSecondPlaPlugProb = 0.12;
    bool doAddSecondPlaPlug = Rand::rand.nextDouble() < addSecondPlaPlugProb;

    double addDoublePlaPlugProb = 0.24;
    bool doAddDoublePlaPlug = Rand::rand.nextDouble() < addDoublePlaPlugProb;

    double clearBehindPlaRabProb = Rand::rand.nextDouble() * 0.50;
    double clearTopRowProb = Rand::rand.nextDouble() * 0.60;

    double carveFromPlaRabProb = 0.35;
    bool doCarveFromPlaRab = Rand::rand.nextDouble() < carveFromPlaRabProb;


    double addFakeFreezeProb = 0.5;
    int numFakeFreezes = Rand::rand.nextDouble() < 0.5 ? Rand::rand.nextInt(1,2) : Rand::rand.nextInt(1,8);
    bool doAddFakeFreezes = Rand::rand.nextDouble() < addFakeFreezeProb;

    //Fill regular opp pieces
    for(int i = 0; i<64; i++)
    {
      if(!freeLocsMap.isOne(i))
        continue;

      //Determine piece to place on this square
      piece_t piece = oppAllowed[i].getStrongestAllowed(labeledStrengths);

      //We are testing this loc - place no matter what
      if(i == (int)testLoc && !testingFreeze)
      {
        DEBUGASSERT(piece > EMP);
        b.setPiece(i,SILV,piece);
        continue;
      }

      //Nothing to place - done
      if(piece == EMP)
        continue;

      //Placing rabbit - might fail
      if(piece == RAB)
      {
        //Standard removal chance
        if(Rand::rand.nextDouble() > rabbitProb)
          continue;
        //Detrimental removal chance
        int row = i/8;
        if(row <= goldRabbitRow && Rand::rand.nextDouble() > detrimentalRabbitProb)
          continue;

        //Didn't fail
        b.setPiece(i,SILV,piece);
      }

      //Placing other piece - might fail
      else
      {
        if(Rand::rand.nextDouble() < pieceProb)
          b.setPiece(i,SILV,piece);
      }
    }

    //Add fake frozen opp pieces
    if(doAddFakeFreezes || testingFreeze)
    {
      //Find all potential frozen pieces we could add
      Bitmap potentialFrozens;
      for(int i = 0; i<64; i++)
        if(frozenOppAllowed[i] != OppPiecesAllowed())
          potentialFrozens.setOn(i);

      //Make sure nothing on traps
      DEBUGASSERT(potentialFrozens == (potentialFrozens & (~Bitmap::BMPTRAPS)));

      //Testing a frozen piece - add no matter what
      if(testingFreeze && testLoc != ERRLOC)
      {
        //Determine piece to place on this square
        DEBUGASSERT(potentialFrozens.isOne(testLoc));
        piece_t piece = frozenOppAllowed[testLoc].getStrongestAllowed(labeledStrengths);

        //Add the tested location
        b.setPiece(testLoc,SILV,piece);
        //Destroy all surrounding opp pieces
        for(int i = 0; i<4; i++)
        {
          if(!Board::ADJOKAY[i][testLoc])
            continue;
          loc_t adj = testLoc + Board::ADJOFFSETS[i];
          if(b.owners[adj] == SILV)
            b.setPiece(adj,NPLA,EMP);
        }
        //Remove surroundings as a possible frozen piece loc
        potentialFrozens &= ~Board::RADIUS[1][testLoc];
        fakeFreezeMap.setOn(testLoc);
      }

      //Otherwise, add only probabilistically
      if(doAddFakeFreezes)
      {
        while(numFakeFreezes > 0 && potentialFrozens.hasBits())
        {
          //Choose a random potential frozen location
          int num = potentialFrozens.countBits();
          int r = Rand::rand.nextUInt(num);
          Bitmap map = potentialFrozens;
          for(int i = 0; i<r; i++)
            map.nextBit();
          loc_t loc = map.nextBit();

          //Determine piece to place on this square
          piece_t piece = frozenOppAllowed[loc].getStrongestAllowed(labeledStrengths);
          DEBUGASSERT(piece > EMP);

          //Add the location
          b.setPiece(loc,SILV,piece);
          //Destroy all surrounding opp pieces
          for(int i = 0; i<4; i++)
          {
            if(!Board::ADJOKAY[i][loc])
              continue;
            loc_t adj = loc + Board::ADJOFFSETS[i];
            if(b.owners[adj] == SILV)
              b.setPiece(adj,NPLA,EMP);
          }
          //Remove surroundings as a possible frozen piece loc
          potentialFrozens &= ~Board::RADIUS[1][loc];
          fakeFreezeMap.setOn(loc);
          numFakeFreezes--;
        }
      }
    }

    //Fill pla pieces
    if(doAddPlaPieces)
    {
      for(int i = 0; i<64; i++)
      {
        if(!freeLocsMap.isOne(i))
          continue;
        if(b.owners[i] != NPLA)
          continue;

        //Must succeed an extra time for far away zones
        if((Board::DISK[2][i] & ~freeLocsMap).isEmpty())
        {
          if(!(Rand::rand.nextDouble() < addPlaPieceProb))
            continue;
        }

        if(Rand::rand.nextDouble() < addPlaPieceProb)
          b.setPiece(i,GOLD,CAT); //We add cats because they are the least helpful - maximizes chance of finding goal stoppage
        else if(i < 56 && !freeLocsMap.isOne(i+8) && b.owners[i+8] == GOLD && b.pieces[i+8] > RAB)
          if(Rand::rand.nextDouble() < plaPieceRabBlockBonus)
            b.setPiece(i,GOLD,CAT);
      }
    }

    //Fill pla rabbits
    if(doAddPlaRabs)
    {
      for(int i = 0; i<64; i++)
      {
        if(!freeLocsMap.isOne(i))
          continue;
        if(b.owners[i] != NPLA)
          continue;
        //Don't add rabbits on the goal line
        if(Board::GOALYDIST[GOLD][i] == 0)
          continue;

        //Only add rabbits within radius 2 of a nonfree location
        //So as to maximize chances of detecting self rabbit blocking interference
        //instead of accidentally creating a new goal threat
        if((Board::RADIUS[2][i] & (~freeLocsMap)).isEmpty())
          continue;

        if(Rand::rand.nextDouble() < addPlaRabProb)
          b.setPiece(i,GOLD,RAB);
        else if(i < 56 && !freeLocsMap.isOne(i+8) && b.owners[i+8] == GOLD && b.pieces[i+8] > RAB)
          if(Rand::rand.nextDouble() < plaPieceRabBlockBonus)
            b.setPiece(i,GOLD,RAB);
      }
    }

    //Add a concentrated plug of pla pieces
    if(doAddPlaPlug || doAddSecondPlaPlug)
    {
      int numPlugs = doAddPlaPlug + doAddSecondPlaPlug;
      for(int np = 0; np < numPlugs; np++)
      {
        //Anything within radius 1 of an unfree loc that is free
        Bitmap possibleLocs = Bitmap::adj(~freeLocsMap) & freeLocsMap;
        int numBits = possibleLocs.countBits();
        if(numBits > 0)
        {
          int r = Rand::rand.nextUInt(numBits);
          while(r > 0)
          {
            possibleLocs.nextBit();
            r--;
          }
          loc_t plugLoc = possibleLocs.nextBit();
          for(int i = 0; i<5; i++)
          {
            loc_t loc = Board::SPIRAL[plugLoc][i];
            if(b.owners[loc] == NPLA && freeLocsMap.isOne(loc) && Rand::rand.nextDouble() > plugFailProb)
            {
              if(loc > 8 && Rand::rand.nextDouble() < plugRabProb)
                b.setPiece(loc,GOLD,RAB);
              else
                b.setPiece(loc,GOLD,CAT);
            }
          }
        }
      }
    }

    if(doAddDoublePlaPlug)
    {
      //Anything within radius 1 of an unfree loc that is free
      Bitmap possibleLocs = Bitmap::adj(~freeLocsMap) & freeLocsMap;
      int numBits = possibleLocs.countBits();
      if(numBits > 0)
      {
        int r = Rand::rand.nextUInt(numBits);
        while(r > 0)
        {
          possibleLocs.nextBit();
          r--;
        }
        loc_t plugLoc = possibleLocs.nextBit();
        for(int i = 0; i<13; i++)
        {
          loc_t loc = Board::SPIRAL[plugLoc][i];
          if(b.owners[loc] == NPLA && freeLocsMap.isOne(loc) && Rand::rand.nextDouble() > plugFailProb)
          {
            if(loc > 8 && Rand::rand.nextDouble() < plugRabProb)
              b.setPiece(loc,GOLD,RAB);
            else
              b.setPiece(loc,GOLD,CAT);
          }
        }
      }
    }

    //Keep it clear behind the rabbits
    for(int i = 8; i<64; i++)
    {
      if(!freeLocsMap.isOne(i) && b.owners[i] == GOLD && b.pieces[i] == RAB && freeLocsMap.isOne(i-8) && b.owners[i-8] == GOLD)
      {
        if(Rand::rand.nextDouble() < clearBehindPlaRabProb)
          b.setPiece(i-8,NPLA,EMP);
      }
      if(!freeLocsMap.isOne(i) && b.owners[i] == GOLD && b.pieces[i] == RAB && i >= 16 && freeLocsMap.isOne(i-16) && b.owners[i-16] == GOLD)
      {
        if(Rand::rand.nextDouble() < clearBehindPlaRabProb)
          b.setPiece(i-16,NPLA,EMP);
      }
    }

    //Clear the top row?
    if(Rand::rand.nextDouble() < clearTopRowProb)
    {
      for(int i = 56; i<64; i++)
      {
        if(freeLocsMap.isOne(i) && b.owners[i] == GOLD)
          b.setPiece(i,NPLA,EMP);
      }
    }

    if(doCarveFromPlaRab)
    {
      //Pick random rabbit
      loc_t rabLoc = ERRLOC;
      int numSeen = 0;
      for(int i = 0; i<64; i++)
      {
        if(basePattern[i] == 'R')
        {
          numSeen++;
          if(Rand::rand.nextDouble() <= 1.0/numSeen)
            rabLoc = i;
        }
      }

      //Here we go
      if(rabLoc != ERRLOC)
      {
        //Pick random opponent, preferring closer ones
        //Accept fakefrozen only half the time
        bool acceptFakeFrozen = Rand::rand.nextInt(0,1) == 1;
        loc_t oppLoc = ERRLOC;
        for(int i = 0; i<180; i++)
        {
          loc_t loc = Rand::rand.nextUInt(64);
          if((Board::DISK[5][loc] & ~freeLocsMap).isEmpty())
            continue;
          if(b.owners[loc] != SILV || b.pieces[loc] < CAT)
            continue;
          if(fakeFreezeMap.isOne(loc) && !acceptFakeFrozen)
            continue;

          if(oppLoc == ERRLOC || Board::manhattanDist(loc,rabLoc) < Board::manhattanDist(loc,oppLoc))
            oppLoc = loc;
        }

        if(oppLoc != ERRLOC)
        {
          DEBUGASSERT(oppLoc != rabLoc);

          //Carve the path
          int dxy[2] = {oppLoc%8-rabLoc%8,oppLoc/8-rabLoc/8};
          loc_t curLoc = rabLoc;
          int xNeg = 1;
          int yNeg = 1;
          if(dxy[0] < 0)
          {
            xNeg = -1;
            dxy[0] = -dxy[0];
          }
          if(dxy[1] < 0)
          {
            yNeg = -1;
            dxy[1] = -dxy[1];
          }

          while(dxy[0] > 0 || dxy[1] > 0)
          {
            int c = Rand::rand.nextUInt(dxy,2);
            if(c == 0)
            {
              dxy[0]--;
              curLoc += xNeg * 1;
            }
            else
            {
              dxy[1]--;
              curLoc += yNeg * 8;
            }

            if(freeLocsMap.isOne(curLoc) && b.owners[curLoc] == GOLD)
              b.setPiece(curLoc,NPLA,EMP);
          }
        }
      }
    }

    //Check traps for hanging pieces
    for(int trapIndex = 0; trapIndex < 4; trapIndex++)
    {
      loc_t trap = Board::TRAPLOCS[trapIndex];
      if(b.owners[trap] == GOLD && !b.isTrapSafe1(b.owners[trap],trap))
      {
        b.setPiece(trap,NPLA,EMP);

        //Half the time, attempt to support it
        if(Rand::rand.nextInt(0,1) == 1)
        {
          int offset = Rand::rand.nextUInt(4);
          for(int i = 0; i<4; i++)
          {
            loc_t loc = trap + Board::ADJOFFSETS[(offset + i)%4];
            if(b.owners[loc] == NPLA && freeLocsMap.isOne(loc))
            {
              if(loc > 8 && Rand::rand.nextDouble() < plugRabProb)
                b.setPiece(loc,GOLD,RAB);
              else
                b.setPiece(loc,GOLD,CAT);
              break;
            }
          }
        }
      }
      if(b.owners[trap] == SILV && !b.isTrapSafe1(b.owners[trap],trap))
        b.setPiece(trap,NPLA,EMP);
    }

    //We fail if we don't have the desired piece we wanted to test
    if(testLoc != ERRLOC && b.owners[testLoc] != SILV)
      continue;

    //Else we're done
    else
      break;
  }

  b.refreshStartHash();
}


static void getOutputChar(OppPiecesAllowed opa, bool isFrozen, int& numExtraChars, char extraChars[2], bool& isAny)
{
  if(!isFrozen)
  {
    if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_ANY &&
      opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_ANY)
      isAny = true;
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LEQ &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_ANY)
      extraChars[numExtraChars++] = 'x';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LESS &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_ANY)
      extraChars[numExtraChars++] = 'w';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_ANY &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LEQ)
      extraChars[numExtraChars++] = 'z';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_ANY &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LESS)
      extraChars[numExtraChars++] = 'y';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LEQ &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LEQ)
      extraChars[numExtraChars++] = 'q';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LESS &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LESS)
      extraChars[numExtraChars++] = 's';
    else if(opa.allowRabbit)
      extraChars[numExtraChars++] = 'r';
    else if(opa == OppPiecesAllowed())
      ;
    else
      DEBUGASSERT(false);
  }
  else
  {
    if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_ANY &&
      opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_ANY)
      extraChars[numExtraChars++] = 'f';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LEQ &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_ANY)
      extraChars[numExtraChars++] = 'j';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LESS &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_ANY)
      extraChars[numExtraChars++] = 'i';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_ANY &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LEQ)
      extraChars[numExtraChars++] = 'n';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_ANY &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LESS)
      extraChars[numExtraChars++] = 'k';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LEQ &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LEQ)
      extraChars[numExtraChars++] = 't';
    else if(opa.p0Constraint == OppPiecesAllowed::CONSTRAINT_LESS &&
        opa.p1Constraint == OppPiecesAllowed::CONSTRAINT_LESS)
      extraChars[numExtraChars++] = 'v';
    else if(opa.allowRabbit)
      extraChars[numExtraChars++] = 'g';
    else if(opa == OppPiecesAllowed())
      ;
    else
      DEBUGASSERT(false);
  }
}

static string getGoalPatternOutput(const char basePattern[64], const Bitmap& freeLocsMap,
    const OppPiecesAllowed oppAllowed[64], const OppPiecesAllowed frozenOppAllowed[64])
{
  //Convert board into output pattern
  ostringstream out;
  for(int y = 7; y>=0; y--)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = y*8+x;

      if(!freeLocsMap.isOne(loc))
      {
        if(basePattern[loc] == 'P')
          out << " P0   ";
        else if(basePattern[loc] == 'Q')
          out << " P1   ";
        else if(basePattern[loc] == 'C')
          out << " P" << "    ";
        else
          out << " " << basePattern[loc] << "    ";
        continue;
      }

      bool isAny = false;
      int numExtraChars = 0;
      char extraChars[2];
      getOutputChar(oppAllowed[loc],false,numExtraChars,extraChars,isAny);
      getOutputChar(frozenOppAllowed[loc],true,numExtraChars,extraChars,isAny);

      if(isAny)
        out << " ?    ";
      else
      {
        out << " .A";
        for(int i = 0; i<numExtraChars; i++)
          out << extraChars[i];
        for(int i = numExtraChars; i<3; i++)
          out << " ";
      }
    }
    out << endl;
  }
  out << "DEFGOALCHARS" << endl;
  return out.str();
}


static bool isGoalStoppable(const char basePattern[64], int numOppSteps, bool assumeTrapDef, int goldRabbitRow,
    int numLabeledPieces, const loc_t labeledPieceLocs[2],
    const OppPiecesAllowed oppAllowed[64], const OppPiecesAllowed frozenOppAllowed[64],
    const Bitmap& freeLocsMap, const Bitmap& farMap,
    loc_t testLoc, bool testingFreeze, int numReps)
{
  for(int reps = 0; reps < numReps; reps++)
  {
    Board copy;
    Bitmap fakeFreezeMap;
    getTestBoard(basePattern,numOppSteps,assumeTrapDef,goldRabbitRow,numLabeledPieces,labeledPieceLocs,
        oppAllowed,frozenOppAllowed,freeLocsMap,testLoc,testingFreeze,copy,fakeFreezeMap);
    int loseSteps = winDefSearch(copy,fakeFreezeMap,9,assumeTrapDef);
    if(loseSteps >= 9)
    {
      if(reps >= SUPPRESS_GOALSTOP_REPORT_NUMREPS)
      {
        cout << "goal stop after " << reps << " reps" << endl;
      }

      if(reps > OUTPUT_EXAMPLES_NUMREPS)
      {
        cout << getGoalPatternOutput(basePattern,freeLocsMap,oppAllowed,frozenOppAllowed) << endl;
        cout << copy << endl;
        cout << fakeFreezeMap << endl;

        for(int r = 0; r<40; r++)
        {
          cout << "EXAMPLE" << endl;
          getTestBoard(basePattern,numOppSteps,assumeTrapDef,goldRabbitRow,numLabeledPieces,labeledPieceLocs,
              oppAllowed,frozenOppAllowed,freeLocsMap,testLoc,testingFreeze,copy,fakeFreezeMap);
          int loseSteps = winDefSearch(copy,fakeFreezeMap,9,assumeTrapDef);
          cout << "LOSESTEPS " << loseSteps << endl;
          cout << copy << endl;
          cout << fakeFreezeMap << endl;
          cout << "END EXAMPLE" << endl;
        }
      }

      return true;
    }
  }
  return false;
}


static bool isTrapSafe1(loc_t trapLoc, const GoalPatternInput& pattern, OppPiecesAllowed oppAllowed[64])
{
  if(pattern.pattern[trapLoc-8] >= 'a' &&  pattern.pattern[trapLoc-8] <= 'z') return true;
  if(pattern.pattern[trapLoc-1] >= 'a' &&  pattern.pattern[trapLoc-1] <= 'z') return true;
  if(pattern.pattern[trapLoc+1] >= 'a' &&  pattern.pattern[trapLoc+1] <= 'z') return true;
  if(pattern.pattern[trapLoc+8] >= 'a' &&  pattern.pattern[trapLoc+8] <= 'z') return true;
  if(oppAllowed[trapLoc-8] != OppPiecesAllowed()) return true;
  if(oppAllowed[trapLoc-1] != OppPiecesAllowed()) return true;
  if(oppAllowed[trapLoc+1] != OppPiecesAllowed()) return true;
  if(oppAllowed[trapLoc+8] != OppPiecesAllowed()) return true;

  return false;
}

static bool isNextToFixedOpponent(loc_t loc, const GoalPatternInput& pattern)
{
  if(CS1(loc) && pattern.pattern[loc-8] >= 'a' &&  pattern.pattern[loc-8] <= 'z') return true;
  if(CW1(loc) && pattern.pattern[loc-1] >= 'a' &&  pattern.pattern[loc-1] <= 'z') return true;
  if(CE1(loc) && pattern.pattern[loc+1] >= 'a' &&  pattern.pattern[loc+1] <= 'z') return true;
  if(CN1(loc) && pattern.pattern[loc+8] >= 'a' &&  pattern.pattern[loc+8] <= 'z') return true;
  return false;
}

string genGoalPatternSpiralFrozenLast(const GoalPatternInput& pattern, int numOppSteps, bool assumeTrapDef, string& coutString)
{
  coutString = string();

  //Inititalize and find everything - rabbit, free squares, labeled pieces
  loc_t goldRabbitLoc = findGoldRabbitLoc(pattern);
  int goldRabbitRow = goldRabbitLoc/8;

  Bitmap freeLocsMap = getFreeLocsMap(pattern.pattern);

  loc_t labeledPieceLocs[2];
  int numLabeledPieces = findLabeledPieces(pattern.pattern,labeledPieceLocs);

  //Initialize the empty allowed opponent pieces
  OppPiecesAllowed oppAllowed[64];
  OppPiecesAllowed frozenOppAllowed[64];
  for(int i = 0; i<64; i++)
  {
    oppAllowed[i] = OppPiecesAllowed();
    frozenOppAllowed[i] = OppPiecesAllowed();
  }

  Bitmap farMap = ~freeLocsMap;
  for(int i = 0; i<numOppSteps+2; i++)
    farMap |= Bitmap::adj(farMap);
  farMap = ~farMap;

  //If the goal threat is blockable as is, it's hopeless
  if(isGoalStoppable(pattern.pattern,numOppSteps,assumeTrapDef,goldRabbitRow,numLabeledPieces,labeledPieceLocs,
      oppAllowed,frozenOppAllowed,freeLocsMap,farMap,ERRLOC,false,NUM_RANDOM_REPS_FAR))
  {
    coutString += "WARNING: plain threat stoppable by self or opp or doesn't exist\n";
    coutString += ArimaaIO::writeGoalPattern(pattern) + "\n";
    return string("");
  }

  //Proceed trying to allow opponent pieces
  static const int STAGE_ALL = 0;
  static const int STAGE_LEQP0 = 1;
  static const int STAGE_LEQP1 = 2;
  static const int STAGE_LESSP0 = 3;
  static const int STAGE_LESSP1 = 4;
  static const int STAGE_LEQBOTH = 5;
  static const int STAGE_LESSBOTH = 6;
  static const int STAGE_RABBIT = 7;
  static const int STAGE_STRENGTHS[8] = {5,4,4,3,3,3,2,1};
  for(int stage = 0; stage < 8; stage++)
  {
    //Skip the stage if not appropriate
    if(numLabeledPieces == 0 && stage != 0 && stage != 7)
      continue;
    if(numLabeledPieces == 1 && stage != 0 && stage != 1 && stage != 4 && stage != 7)
      continue;

    //Get the allowance for this stage
    OppPiecesAllowed opa;
    switch(stage)
    {
    case STAGE_ALL: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_ANY; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_ANY; break;
    case STAGE_LEQP0: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_ANY; break;
    case STAGE_LEQP1: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_ANY; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; break;
    case STAGE_LESSP0: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LESS; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_ANY; break;
    case STAGE_LESSP1: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_ANY; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LESS; break;
    case STAGE_LEQBOTH: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; break;
    case STAGE_LESSBOTH: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LESS; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LESS; break;
    case STAGE_RABBIT: opa.allowRabbit = true;
    default: DEBUGASSERT(false);
    }
    opa.strength = STAGE_STRENGTHS[stage];

    //Walk in a spiral inward to the rabbit
    for(int spiralidx = 63; spiralidx >=0; spiralidx--)
    {
      loc_t loc = Board::SPIRAL[goldRabbitLoc][spiralidx];
      //Must be a free loc to allow opp piece
      if(!freeLocsMap.isOne(loc))
        continue;
      //We skip the traps for now, do them last
      if(Board::ISTRAP[loc])
        continue;
      //If we have added at an earlier stage, we can't do anything
      if(oppAllowed[loc] != OppPiecesAllowed())
        continue;

      //Try opp piece
      oppAllowed[loc] = opa;

      int numReps = farMap.isOne(loc) ? NUM_RANDOM_REPS_FAR : NUM_RANDOM_REPS;
      bool stoppedGoal = isGoalStoppable(pattern.pattern,numOppSteps,assumeTrapDef,goldRabbitRow,numLabeledPieces,labeledPieceLocs,
          oppAllowed,frozenOppAllowed,freeLocsMap,farMap,loc,false,numReps);

      //Undo if it didn't work
      if(stoppedGoal)
        oppAllowed[loc] = OppPiecesAllowed();
    }

    for(int trapIndex = 0; trapIndex < 4; trapIndex++)
    {
      loc_t loc = Board::TRAPLOCS[trapIndex];
      //Must be a free loc to allow opp piece
      if(!freeLocsMap.isOne(loc))
        continue;
      //If we have added at an earlier stage, we can't do anything
      if(oppAllowed[loc] != OppPiecesAllowed())
        continue;
      //Can only add if there will potentially be something next to us
      if(!isTrapSafe1(loc,pattern,oppAllowed))
        continue;

      //Try opp piece
      oppAllowed[loc] = opa;

      int numReps = farMap.isOne(loc) ? NUM_RANDOM_REPS_FAR : NUM_RANDOM_REPS;
      bool stoppedGoal = isGoalStoppable(pattern.pattern,numOppSteps,assumeTrapDef,goldRabbitRow,numLabeledPieces,labeledPieceLocs,
          oppAllowed,frozenOppAllowed,freeLocsMap,farMap,loc,false,numReps);

      //Undo if it didn't work
      if(stoppedGoal)
      {
        oppAllowed[loc] = OppPiecesAllowed();

        //Another sanity check - the lower right corner should be filled in with e's and it should still be okay
        if(stage == 0 && (loc == 7 || loc == 15 || loc == 6))
        {
          coutString += "WARNING: LR corner didn't get set on stage 0!\n";
          coutString += ArimaaIO::writeGoalPattern(pattern) + "\n";
          return string("");
        }
      }
    }
  }

  for(int stage = 0; stage < 8; stage++)
  {
    //Skip the stage if not appropriate
    if(numLabeledPieces == 0 && stage != 0 && stage != 7)
      continue;
    if(numLabeledPieces == 1 && stage != 0 && stage != 1 && stage != 4 && stage != 7)
      continue;

    //Get the allowance for this stage
    OppPiecesAllowed opa;
    switch(stage)
    {
    case STAGE_ALL: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_ANY; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_ANY; break;
    case STAGE_LEQP0: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_ANY; break;
    case STAGE_LEQP1: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_ANY; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; break;
    case STAGE_LESSP0: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LESS; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_ANY; break;
    case STAGE_LESSP1: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_ANY; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LESS; break;
    case STAGE_LEQBOTH: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LEQ; break;
    case STAGE_LESSBOTH: opa.p0Constraint = OppPiecesAllowed::CONSTRAINT_LESS; opa.p1Constraint = OppPiecesAllowed::CONSTRAINT_LESS; break;
    case STAGE_RABBIT: opa.allowRabbit = true;
    default: DEBUGASSERT(false);
    }
    opa.strength = STAGE_STRENGTHS[stage];

    //Walk in a spiral inward to the rabbit
    for(int spiralidx = 63; spiralidx >=0; spiralidx--)
    {
      loc_t loc = Board::SPIRAL[goldRabbitLoc][spiralidx];
      //Must be a free loc to allow opp piece
      if(!freeLocsMap.isOne(loc))
        continue;
      //We skip the traps because pieces can't be frozen on traps
      if(Board::ISTRAP[loc])
        continue;
      //If we have added at an earlier stage, we can't do anything
      if(frozenOppAllowed[loc] != OppPiecesAllowed())
        continue;
      //If we have an unfrozen allowance with leq the strenght, we can't do anything
      if(opa.strength <= oppAllowed[loc].strength)
        continue;
      //Cannot put frozen pieces next to pieces that have been fixed as the opponent
      if(isNextToFixedOpponent(loc,pattern))
        continue;

      //Try opp piece
      frozenOppAllowed[loc] = opa;

      int numReps = farMap.isOne(loc) ? NUM_RANDOM_REPS_FAR : NUM_RANDOM_REPS;
      bool stoppedGoal = isGoalStoppable(pattern.pattern,numOppSteps,assumeTrapDef,goldRabbitRow,numLabeledPieces,labeledPieceLocs,
          oppAllowed,frozenOppAllowed,freeLocsMap,farMap,loc,true,numReps);

      //Undo if it didn't work
      if(stoppedGoal)
        frozenOppAllowed[loc] = OppPiecesAllowed();
    }
  }

  //Verification - repeat many times
  cout << "VERIFYING " << endl;
  bool verifyFailed = isGoalStoppable(pattern.pattern,numOppSteps,assumeTrapDef,goldRabbitRow,numLabeledPieces,labeledPieceLocs,
        oppAllowed,frozenOppAllowed,freeLocsMap,farMap,ERRLOC,false,VERIFICATION_NUM_REPS);

  string output = getGoalPatternOutput(pattern.pattern, freeLocsMap, oppAllowed, frozenOppAllowed);

  if(verifyFailed)
  {
    output = "#VERIFY FAILED\n" + ArimaaIO::writeGoalPattern(pattern) + output;
    size_t pos = output.find("\n");
    while(pos != string::npos)
    {
      output.replace(pos,1,"\n#");
      pos = output.find("\n",pos+2);
    }
  }

  coutString = string();
  coutString += output + "\n";

  return output;
}

*/




