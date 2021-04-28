/*
 * mainevaltune.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/boardtrees.h"
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../learning/gameiterator.h"
#include "../eval/eval.h"
#include "../eval/internal.h"
#include "../search/search.h"
#include "../search/searchutils.h"
#include "../search/searchparams.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

static int64_t getVariance(const Board& board)
{
  Board b = board;
  pla_t pla = b.player;
  pla_t opp = gOpp(pla);

  int pStronger[2][NUMTYPES];
  Bitmap pStrongerMaps[2][NUMTYPES];
  eval_t pValues[2][NUMTYPES];
  int ufDist[BSIZE];
  b.initializeStronger(pStronger);
  b.initializeStrongerMaps(pStrongerMaps);
  Eval::initializeValues(b,pValues);
  UFDist::get(b,ufDist,pStrongerMaps);

  //Trap control-----------------------------------------------------------------------
  int tc[2][4];
  int tcEle[2][4];
  TrapControl::get(b,pStronger,pStrongerMaps,ufDist,tc,tcEle);

  //Elephant Mobility------------------------------------------------------------------
  const int eleCanReachMax = 9;
  Bitmap eleCanReach[2][EMOB_ARR_LEN];
  Mobility::getEle(b,SILV,eleCanReach[SILV],eleCanReachMax,EMOB_ARR_LEN);
  Mobility::getEle(b,GOLD,eleCanReach[GOLD],eleCanReachMax,EMOB_ARR_LEN);

  //Threats----------------------------------------------------------------------------
  eval_t trapThreats[2][4];
  eval_t reducibleTrapThreats[2][4];
  eval_t pieceThreats[BSIZE];
  for(int i = 0; i<4; i++)
  {trapThreats[SILV][i] = 0; trapThreats[GOLD][i] = 0;}
  for(int i = 0; i<4; i++)
  {reducibleTrapThreats[SILV][i] = 0; reducibleTrapThreats[GOLD][i] = 0;}
  for(int i = 0; i<BSIZE; i++)
  {pieceThreats[i] = 0;}

  PThreats::accumOppAttack(b,pStrongerMaps,pValues,ufDist,tc,eleCanReach,trapThreats,pieceThreats);
  PThreats::accumBasicThreats(b,pStronger,pStrongerMaps,pValues,ufDist,tc,eleCanReach,trapThreats,reducibleTrapThreats,pieceThreats);
  PThreats::accumHeavyThreats(b,pStronger,pStrongerMaps,pValues,ufDist,tc,trapThreats,pieceThreats);

  eval_t threatScore = 0;
  for(int i = 0; i<4; i++)
  {
    threatScore -= trapThreats[pla][i];
    threatScore += trapThreats[opp][i];
  }

  int finalValue = threatScore;
  return (int64_t)finalValue * (int64_t)finalValue;
}

int MainFuncs::evalComponentVariance(int argc, const char* const *argv)
{
  const char* usage =
      "posfile";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  cout << "Loading..." << endl;
  vector<BoardRecord> boards = ArimaaIO::readBoardRecordFile(mainCommand[1]);

  cout << "Evaling..." << endl;
  double variance = 0;
  int numPoses = (int)boards.size();
  for(int i = 0; i<numPoses; i++)
    variance += getVariance(boards[i].board);
  cout << "Variance per pos " << variance/numPoses << endl;
  cout << "Stdev per pos " << sqrt(variance/numPoses) << endl;
  return EXIT_SUCCESS;
}

static void initParams(SearchParams& params)
{
  BradleyTerry rootLearner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(rootLearner);
  params.setMoveImmediatelyIfOnlyOneNonlosing(false);
  params.setRootFancyPrune(true);
  params.setUnsafePruneEnable(true);
  params.setNullMoveEnable(true);
}

static void setGoodMoveFiltering(GameIterator& iter)
{
  iter.setDoFilter(true);
  iter.setDoFilterLemmings(true);
  iter.setDoFilterWins(true);
  iter.setDoFilterWinsInTwo(true);
  iter.setNumInitialToFilter(2);
  iter.setNumLoserToFilter(4);
  iter.setDoFilterManyShortMoves(true);
  iter.setMinPlaUnfilteredMoves(8);
}

static void setGoodEvalFiltering(GameIterator& iter)
{
  iter.setDoFilter(true);
  iter.setDoFilterLemmings(false);
  iter.setDoFilterWins(true);
  iter.setDoFilterWinsInTwo(false);
  iter.setNumInitialToFilter(2);
  iter.setNumLoserToFilter(1);
  iter.setDoFilterManyShortMoves(true);
  iter.setMinPlaUnfilteredMoves(8);
}

//Convert an eval to a pseudo-probability of winning or losing
//winProbScale is the eval at which the probability of winning is e/(1+e)
//Reasonable seems to be 3000-5000
static double pseudoWinProbability(eval_t eval, double winProbScale)
{
  if(SearchUtils::isWinEval(eval))
    return 1.0;
  else if(SearchUtils::isLoseEval(eval))
    return 0.0;
  return 1.0 / (1.0 + exp(-eval/winProbScale));
}

static eval_t getEvaluation(Searcher& searcher, int realDepth, const Board& board, const BoardHistory& hist)
{
  Board b = board;

  pla_t winner = board.getWinner();
  if(winner != NPLA)
    return winner == b.player ? Eval::WIN : Eval::LOSE;
  if(BoardTrees::goalDist(b,b.player,4-b.step) < 5)
    return Eval::WIN;
  else if(SearchMoveGen::definitelyForcedLossInTwo(b))
    return Eval::LOSE;

  eval_t eval;
  if(realDepth <= -2)
    eval = Eval::evaluate(b,NPLA,0,NULL);
  else
  {
    searcher.searchID(b,hist,realDepth,0);
    eval = searcher.stats.finalEval;
  }
  return eval;
}

int MainFuncs::modelEvalLikelihood(int argc, const char* const *argv)
{
  //The model used is that the winning probability in a position is 1/(1+exp(eval/winprobscale))
  //The probability that a player chooses a move leading to win probability p is proportional to exp(p/moveprobscale)
  //except that with constant probability modelblunderprob, they choose a random move
  //Computes the likelihood of the data under this model with the current eval function
  const char* usage =
      "movesfile "
      "-winprobscale scale "
      "-moveprobscale p "
      "-modelblunderprob p "
      "-depth depth "
      "<-ratedonly>"
      "<-minrating rating>"
      "<-poskeepprop prop>"
      "<-botkeepprop prop>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>"
      "<-movekeepprop prop (default 0)>"
      "<-movekeepbase const (default 1)>";
  const char* required = "winprobscale moveprobscale modelblunderprob depth";
  const char* allowed = "ratedonly minrating poskeepprop botkeepprop botgameweight botposweight fancyweight movekeepprop movekeepbase";
  const char* empty = "ratedonly fancyweight";
  const char* nonempty = "winprobscale moveprobscale modelblunderprob depth minrating poskeepprop botkeepprop botgameweight botposweight movekeepprop movekeepbase";

  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];

  double winProbScale = Command::getDouble(flags,"winprobscale");
  double moveProbScale = Command::getDouble(flags,"moveprobscale");
  double modelBlunderProb = Command::getDouble(flags,"modelblunderprob");
  int depth = Command::getInt(flags,"depth");

  bool ratedOnly = Command::isSet(flags,"ratedonly");
  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double posKeepProp = Command::getDouble(flags,"poskeepprop",1.0);
  double botKeepProp = Command::getDouble(flags,"botkeepprop",1.0);
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);
  double moveKeepProp = Command::getDouble(flags,"movekeepprop",0);
  int moveKeepBase = Command::getInt(flags,"movekeepbase",1);

  cout << Command::gitRevisionId() << endl;
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;

  cout << "Reading " << infile << endl;

  ClockTimer timer;

  GameIterator iter(infile);
  setGoodMoveFiltering(iter);
  iter.setPosKeepProp(posKeepProp);
  iter.setMoveKeepProp(moveKeepProp);
  iter.setMoveKeepBase(moveKeepBase);
  iter.setMinRating(minRating);
  iter.setRatedOnly(ratedOnly);
  iter.setBotPosKeepProp(botKeepProp);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  iter.setDoPrint(true);

  cout << "Loaded " << iter.getTotalNumGames() << " games" << endl;

  SearchParams params;
  initParams(params);
  Searcher searcher(params);

  double weightedLogProb = 0;
  double weightSum = 0;
  int numInstances = 0;

  vector<move_t> moves;
  vector<double> moveProbs;
  while(iter.next())
  {
    Board b = iter.getBoard();
    BoardHistory hist = iter.getHist();

    int winningMoveIdx;
    iter.getNextMoves(winningMoveIdx,moves);

    moveProbs.clear();
    int numMoves = moves.size();
    for(int i = 0; i<numMoves; i++)
    {
      Board copy = b;
      bool suc = copy.makeMoveLegalNoUndo(moves[i]);
      hist.reportMove(copy,moves[i],0);
      DEBUGASSERT(suc);
      eval_t eval = -getEvaluation(searcher,depth,copy,hist);
      double winProb = pseudoWinProbability(eval,winProbScale);
      moveProbs.push_back(exp(winProb)/moveProbScale);
    }

    double totalProb = 0;
    for(int i = 0; i<numMoves; i++)
      totalProb += moveProbs[i];
    for(int i = 0; i<numMoves; i++)
      moveProbs[i] = moveProbs[i] / totalProb * (1-modelBlunderProb);
    for(int i = 0; i<numMoves; i++)
      moveProbs[i] += modelBlunderProb / numMoves;

    double logProb = log(moveProbs[winningMoveIdx]);
    double weight = iter.getPosWeight();
    weightedLogProb += logProb * weight;
    weightSum += weight;
    numInstances++;
  }

  cout << "Final logprob per weight " << Global::strprintf("%.12f", weightedLogProb / weightSum)  << endl;
  cout << "Final weight " << weightSum << endl;
  cout << "Total instances " << numInstances << endl;
  cout << "Time taken: " << timer.getSeconds() << endl;

  return EXIT_SUCCESS;
}


//Given a timeseries of evals all from one player's perspective, compute the sum of squared differences
//between the psuedo-win-probability of each point in the game and the td-lambda-averaged pseudo-win-probability
//of future points in the game
//Horizon is 1/(1-lambda)
static double tdLambdaVariance(const vector<eval_t>& evals, const vector<bool>& filter, double horizon, double winProbScale)
{
  //Start at the end
  if(evals.size() <= 0)
    return 0;

  double variance = 0;

  int i = evals.size()-1;
  double winProbSum = pseudoWinProbability(evals[i],winProbScale) * horizon;
  double weightSum = horizon;
  double lambda = 1.0 - (1.0 / horizon);
  i--;
  for(; i>=0; i--)
  {
    double futureWinProb = winProbSum / weightSum;
    double currentWinProb = pseudoWinProbability(evals[i],winProbScale);

    if(!filter[i])
      variance += (currentWinProb - futureWinProb) * (currentWinProb - futureWinProb);

    winProbSum *= lambda;
    weightSum *= lambda;
    winProbSum += currentWinProb;
    weightSum += 1.0;
  }

  return variance;
}

static double averageDefault1(const vector<double>& vec)
{
  int size = vec.size();
  if(size <= 0)
    return 1;
  double sum = 0;
  for(int i = 0; i<size; i++)
    sum += vec[i];
  return sum / (double)size;
}

static double getTDLambdaVariance(GameIterator& iter, Searcher& searcher, int depth, double winProbScale, double horizon)
{
  setGoodEvalFiltering(iter);
  iter.reset();
  double variance = 0;
  vector<eval_t> evals;
  vector<bool> filter;
  vector<double> posWeights;
  int prevGameIdx = -1;
  while(iter.next())
  {
    if(iter.getGameIdx() != prevGameIdx)
    {
      variance += tdLambdaVariance(evals,filter,horizon,winProbScale) * averageDefault1(posWeights);
      evals.clear();
      filter.clear();
      posWeights.clear();
      prevGameIdx = iter.getGameIdx();
    }

    Board b = iter.getBoard();
    const BoardHistory& hist = iter.getHist();

    eval_t eval = getEvaluation(searcher,depth,b,hist);
    evals.push_back(b.player == GOLD ? eval : -eval);
    filter.push_back(iter.wouldFilterCurrent());
    posWeights.push_back(iter.getPosWeight());

    bool suc = b.makeMoveLegalNoUndo(iter.getNextMove());
    DEBUGASSERT(suc);

    pla_t winner = b.getWinner();
    if(winner != NPLA)
    {
      if(winner == GOLD) evals.push_back(Eval::WIN);
      else if(winner == SILV) evals.push_back(Eval::LOSE);
      variance += tdLambdaVariance(evals,filter,horizon,winProbScale) * averageDefault1(posWeights);
      evals.clear();
      filter.clear();
      posWeights.clear();
    }
  }

  variance += tdLambdaVariance(evals,filter,horizon,winProbScale) * averageDefault1(posWeights);
  evals.clear();
  filter.clear();
  posWeights.clear();
  return variance;
}

static double getMovesArePositiveVariance(GameIterator& iter, Searcher& searcher, int depth, double winProbScale)
{
  setGoodMoveFiltering(iter);
  iter.reset();
  double variance = 0;
  vector<eval_t> evals;
  vector<bool> filter;
  while(iter.next())
  {
    if(iter.wouldFilterCurrent())
      continue;
    double posWeight = iter.getPosWeight();
    if(posWeight <= 0)
      continue;

    Board b = iter.getBoard();
    const BoardHistory& hist = iter.getHist();

    eval_t baseEval = getEvaluation(searcher,depth,b,hist);
    bool suc = b.makeMoveLegalNoUndo(iter.getNextMove());
    DEBUGASSERT(suc);
    pla_t winner = b.getWinner();
    if(winner != NPLA)
      continue;

    b.setPlaStep(gOpp(b.player),0);
    b.refreshStartHash();
    BoardHistory newHist = BoardHistory(b);
    eval_t nextEval = getEvaluation(searcher,depth,b,newHist);

    if(nextEval < baseEval)
    {
      double diff = pseudoWinProbability(baseEval,winProbScale) - pseudoWinProbability(nextEval,winProbScale);
      variance += diff * diff * posWeight;
    }
  }
  return variance;
}

static double getPriorVariance(const vector<double>& basisCoeffs)
{
  double variance = 0;
  int numBases = basisCoeffs.size();
  for(int i = 0; i<numBases; i++)
    variance += basisCoeffs[i] * basisCoeffs[i];
  return variance;
}

static double getVariance(GameIterator& iter, Searcher& searcher, const vector<double>& basisCoeffs,
    int depth, double winProbScale, double horizon,
    double tdLambdaScale, double movesArePositiveScale, double priorScale)
{
  double variance = 0;
  variance += tdLambdaScale * getTDLambdaVariance(iter,searcher,depth,winProbScale,horizon);
  variance += movesArePositiveScale * getMovesArePositiveVariance(iter,searcher,depth,winProbScale);
  variance += priorScale * getPriorVariance(basisCoeffs);
  return variance;
}

static void addBasis(int bidx, double amount, vector<double>& basisCoeffs, const vector<double*>& coeffs, const vector<vector<pair<int,double>>>& bases)
{
  basisCoeffs[bidx] += amount;
  const vector<pair<int,double>>& basis = bases[bidx];
  int size = basis.size();
  for(int i = 0; i<size; i++)
    *(coeffs[basis[i].first]) += amount * basis[i].second;
}

static double twiddleBasis(GameIterator& iter, Searcher& searcher,
    double varianceSoFar, int bidx,
    int depth, double winProbScale, double horizon,
    double tdLambdaScale, double movesArePositiveScale, double priorScale,
    vector<double>& basisCoeffs, vector<double>& radius,
    const vector<double*>& coeffs,
    const vector<string>& basisNames, const vector<vector<pair<int,double>>>& bases)
{
  double rad = radius[bidx];
  addBasis(bidx,rad,basisCoeffs,coeffs,bases);
  double upVariance = getVariance(iter,searcher,basisCoeffs,depth,winProbScale,horizon,tdLambdaScale,movesArePositiveScale,priorScale);
  if(upVariance < varianceSoFar)
  {
    varianceSoFar = upVariance;
    cout << basisNames[bidx] << " " << basisCoeffs[bidx] << " [+" << radius[bidx] << "] variance " << Global::strprintf("%.10f,",varianceSoFar) << endl;
    radius[bidx] *= 1.2;
    return varianceSoFar;
  }

  addBasis(bidx,-2*rad,basisCoeffs,coeffs,bases);
  double downVariance = getVariance(iter,searcher,basisCoeffs,depth,winProbScale,horizon,tdLambdaScale,movesArePositiveScale,priorScale);
  if(downVariance < varianceSoFar)
  {
    varianceSoFar = downVariance;
    cout << basisNames[bidx] << " " << basisCoeffs[bidx] << " [-" << radius[bidx] << "] variance " << Global::strprintf("%.10f,",varianceSoFar) << endl;
    radius[bidx] *= 1.2;
    return varianceSoFar;
  }

  addBasis(bidx,rad,basisCoeffs,coeffs,bases);
  cout << basisNames[bidx] << " " << basisCoeffs[bidx] << " [!" << radius[bidx] << "] variance " << Global::strprintf("%.10f,",varianceSoFar) << endl;
  radius[bidx] *= 0.6;
  return varianceSoFar;
}

static void optimize(GameIterator& iter, Searcher& searcher,
    int numIters, int depth, double winProbScale, double horizon,
    double tdLambdaScale, double movesArePositiveScale, double priorScale,
    vector<double>& basisCoeffs, vector<double>& radius,
    const vector<double*>& coeffs,
    const vector<string>& basisNames, const vector<vector<pair<int,double>>>& bases)
{
  double varianceSoFar = getVariance(iter,searcher,basisCoeffs,depth,winProbScale,horizon,tdLambdaScale,movesArePositiveScale,priorScale);
  cout << "TDLambda Variance: " << tdLambdaScale * getTDLambdaVariance(iter,searcher,depth,winProbScale,horizon) << endl;
  cout << "MAP Variance: " << movesArePositiveScale * getMovesArePositiveVariance(iter,searcher,depth,winProbScale) << endl;
  cout << "Prior Variance: " << priorScale * getPriorVariance(basisCoeffs) << endl;
  cout << "Total: " << varianceSoFar << endl;

  for(int i = 0; i<numIters; i++)
  {
    cout << "Starting iteration " << i << endl;
    int numBases = bases.size();
    for(int bidx = 0; bidx<numBases; bidx++)
      varianceSoFar = twiddleBasis(iter,searcher,varianceSoFar,bidx,depth,winProbScale,horizon,tdLambdaScale,movesArePositiveScale,priorScale,basisCoeffs,radius,coeffs,basisNames,bases);

    cout << "TDLambda Variance: " << tdLambdaScale * getTDLambdaVariance(iter,searcher,depth,winProbScale,horizon) << endl;
    cout << "MAP Variance: " << movesArePositiveScale * getMovesArePositiveVariance(iter,searcher,depth,winProbScale) << endl;
    cout << "Prior Variance: " << priorScale * getPriorVariance(basisCoeffs) << endl;
    cout << "Total: " << varianceSoFar << endl;
  }
}

int MainFuncs::optimizeEval(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile "
      "-winprobscale scale "
      "-horizon turns "
      "-depth depth "
      "-numiters numiters "
      "-tdscale scale "
      "-mapscale scale "
      "-priorscale scale "
      "<-ratedonly>"
      "<-minrating rating>"
      "<-poskeepprop prop>"
      "<-botkeepprop prop>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>"
      "<-movekeepprop prop (default 0)>"
      "<-movekeepbase const (default 1)>";
  const char* required = "winprobscale horizon depth numiters tdscale mapscale priorscale";
  const char* allowed = "ratedonly minrating poskeepprop botkeepprop botgameweight botposweight fancyweight movekeepprop movekeepbase";
  const char* empty = "ratedonly fancyweight";
  const char* nonempty = "winprobscale horizon depth numiters tdscale mapscale priorscale minrating poskeepprop botkeepprop botgameweight botposweight movekeepprop movekeepbase";

  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];

  double winProbScale = Command::getDouble(flags,"winprobscale");
  double horizon = Command::getDouble(flags,"horizon");
  int depth = Command::getInt(flags,"depth");
  int numIters = Command::getInt(flags,"numiters");
  double tdLambdaScale = Command::getDouble(flags,"tdscale");
  double movesArePositiveScale = Command::getDouble(flags,"mapscale");
  double priorScale = Command::getDouble(flags,"priorscale");

  bool ratedOnly = Command::isSet(flags,"ratedonly");
  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double posKeepProp = Command::getDouble(flags,"poskeepprop",1.0);
  double botKeepProp = Command::getDouble(flags,"botkeepprop",1.0);
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);
  double moveKeepProp = Command::getDouble(flags,"movekeepprop",0);
  int moveKeepBase = Command::getInt(flags,"movekeepbase",1);

  cout << Command::gitRevisionId() << endl;
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;

  cout << "Reading " << infile << endl;

  ClockTimer timer;

  GameIterator iter(infile);
  iter.setPosKeepProp(posKeepProp);
  iter.setMoveKeepProp(moveKeepProp);
  iter.setMoveKeepBase(moveKeepBase);
  iter.setMinRating(minRating);
  iter.setRatedOnly(ratedOnly);
  iter.setBotPosKeepProp(botKeepProp);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  iter.setDoPrint(false);

  //We don't directly filter moves, we instead check if positions would be filtered
  iter.setDoFilter(false);

  cout << "Loaded " << iter.getTotalNumGames() << " games" << endl;

  SearchParams params;
  initParams(params);
  Searcher searcher(params);

  vector<string> names;
  vector<double*> coeffs;
  vector<string> basisNames;
  vector<vector<pair<int,double>>> bases;
  PThreats::getBasicThreatCoeffsAndBases(names,coeffs,basisNames,bases,1.0);

  int numBases = bases.size();
  vector<double> basisCoeffs;
  vector<double> radius;
  for(int i = 0; i<numBases; i++)
  {
    basisCoeffs.push_back(0);
    radius.push_back(1.0);
  }

  optimize(iter,searcher,numIters,depth,winProbScale,horizon,tdLambdaScale,movesArePositiveScale,priorScale,
      basisCoeffs,radius,coeffs,basisNames,bases);

  cout << "Done, dumping coeffs:" << endl;
  int numCoeffs = coeffs.size();
  for(int i = 0; i<numCoeffs; i++)
    cout << *(coeffs[i]) << endl;
  for(int i = 0; i<numCoeffs; i++)
    cout << names[i] << " " << *(coeffs[i]) << endl;

  return EXIT_SUCCESS;
}


/*

static bool isBadHash(const Board& b, const vector<hash_t>& badHashes)
{
  for(int j = 0; j<(int)badHashes.size(); j++)
    if(b.sitCurrentHash == badHashes[j])
      return true;
  return false;
}

*/

/*
//TODO stuff in here needs to be editied to account for the rewrite from 64 to 0x88 format.

int MainFuncs::compareNumStepsLeftValue(int argc, const char* const *argv)
{
  vector<string> mainCommand = Command::parseCommand(argc, argv);

  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  map<string,string> flags = Command::parseFlags(argc, argv, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2)
    return EXIT_FAILURE;

  vector<Board> boards = ArimaaIO::readBoardFile(mainCommand[1]);

  SearchParams params;
  initParams(params);
  Searcher searcher(params);

  for(int i = 0; i<(int)boards.size(); i++)
  {
    Board b = boards[i];
    BoardHistory hist = BoardHistory(b);

    int depthMax = 4;
    eval_t bestEval;
    int bestNS;
    eval_t depthNEvals[depthMax+1];
    int depthNNS[depthMax+1];
    eval_t depthNFollowedByBestEvals[depthMax+1];
    int depthNFollowedByBestNS[depthMax+1];
    eval_t qs1DirectEval = 0;
    int qs1DirectNS = 0;
    eval_t qs1FollowedByBestEval = 0;
    int qs1FollowedByBestNS = 0;

    {
      searcher.searchID(b,hist,5,0,false);
      bestEval = searcher.stats.finalEval;
      bestNS = 0;
      if(SearchUtils::isTerminalEval(bestEval))
        continue;
    }
    bool foundTerminal = false;
    for(int depth = 1; depth <= depthMax; depth++)
    {
      searcher.searchID(b,hist,depth,0,false);

      vector<move_t> moves = searcher.getIDPVFullMoves();
      if(moves.size() < 2)
        continue;

      Board copy = b;
      bool suc = copy.makeMoveLegal(moves[0]);
      DEBUGASSERT(suc);
      DEBUGASSERT(copy.player != b.player && copy.step == 0);
      BoardHistory copyHist = hist;
      copyHist.reportMove(copy,moves[0],b.step);
      searcher.searchID(copy,copyHist,1,0,false);
      depthNEvals[depth] = copy.player == b.player ? searcher.stats.finalEval : -searcher.stats.finalEval;
      depthNNS[depth] = 4-numStepsInMove(stripPasses(moves[0]));
      if(SearchUtils::isTerminalEval(depthNEvals[depth]))
      {foundTerminal = true; break;}

      copy = b;
      suc = copy.makeMoveLegal(stripPasses(moves[0]));
      DEBUGASSERT(suc);
      copyHist = hist;
      copyHist.reportMove(copy,stripPasses(moves[0]),b.step);
      searcher.searchID(copy,copyHist,copy.step == 0 ? 1 : 4-copy.step+1,0,false);
      depthNFollowedByBestEvals[depth] = copy.player == b.player ? searcher.stats.finalEval : -searcher.stats.finalEval;
      depthNFollowedByBestNS[depth] = 4-numStepsInMove(stripPasses(moves[0]));
      if(SearchUtils::isTerminalEval(depthNFollowedByBestEvals[depth]))
      {foundTerminal = true; break;}

      if(depth == 4)
      {
        copy = b;
        suc = copy.makeMoveLegal(moves[0]);
        DEBUGASSERT(suc);
        DEBUGASSERT(copy.player != b.player && copy.step == 0);
        copyHist = hist;
        copyHist.reportMove(copy,moves[0],b.step);
        move_t m1 = stripPasses(moves[1]);
        if(m1 != ERRMOVE)
        {
          suc = copy.makeMoveLegal(m1);
          DEBUGASSERT(suc);
          copyHist.reportMove(copy,m1,0);
        }
        int oldStep = copy.step;
        if(copy.player != b.player)
        {
          copy.makeMove(PASSMOVE);
          copyHist.reportMove(copy,PASSMOVE,oldStep);
        }
        searcher.searchID(copy,copyHist,1,0,false);
        qs1DirectEval = copy.player == b.player ? searcher.stats.finalEval : -searcher.stats.finalEval;
        qs1DirectNS = 4-numStepsInMove(stripPasses(moves[1]));
        if(SearchUtils::isTerminalEval(qs1DirectEval))
        {foundTerminal = true; break;}

        copy = b;
        suc = copy.makeMoveLegal(moves[0]);
        DEBUGASSERT(suc);
        DEBUGASSERT(copy.player != b.player && copy.step == 0);
        copyHist = hist;
        copyHist.reportMove(copy,moves[0],b.step);
        m1 = stripPasses(moves[1]);
        if(m1 != ERRMOVE)
        {
          suc = copy.makeMoveLegal(m1);
          DEBUGASSERT(suc);
          copyHist.reportMove(copy,m1,0);
        }
        searcher.searchID(copy,copyHist,copy.step == 0 ? 1 : 4-copy.step+1,0,false);
        qs1FollowedByBestEval = copy.player == b.player ? searcher.stats.finalEval : -searcher.stats.finalEval;
        qs1FollowedByBestNS = 4-numStepsInMove(stripPasses(moves[1]));
        if(SearchUtils::isTerminalEval(qs1FollowedByBestEval))
          continue;
      }
    }
    if(foundTerminal)
      continue;

    int pieceCount = b.pieceCounts[0][0]+b.pieceCounts[1][0];
    int advancementScore[8] = {0,0,1,2,3,4,4,4};
    int advancement = 0;
    for(int j = 0; j<64; j++)
    {
      if(b.owners[j] == GOLD) advancement += advancementScore[j/8];
      else if(b.owners[j] == SILV) advancement += advancementScore[7-j/8];
    }

    pla_t pla = b.player;
    pla_t opp = gOpp(pla);
    int pStronger[2][NUMTYPES];
    Bitmap pStrongerMaps[2][NUMTYPES];
    b.initializeStronger(pStronger);
    b.initializeStrongerMaps(pStrongerMaps);
    int ufDist[64];
    UFDist::get(b,ufDist);
    int tc[2][4];
    Eval::getBasicTrapControls(b,pStronger,pStrongerMaps,ufDist,tc);
    int plaTCScore = 0;
    int oppTCScore = 0;
    plaTCScore += Eval::getTrapControlScore(pla,Board::PLATRAPINDICES[opp][0],tc[pla][Board::PLATRAPINDICES[opp][0]]);
    plaTCScore += Eval::getTrapControlScore(pla,Board::PLATRAPINDICES[opp][1],tc[pla][Board::PLATRAPINDICES[opp][1]]);
    oppTCScore += Eval::getTrapControlScore(opp,Board::PLATRAPINDICES[pla][0],tc[opp][Board::PLATRAPINDICES[pla][0]]);
    oppTCScore += Eval::getTrapControlScore(opp,Board::PLATRAPINDICES[pla][1],tc[opp][Board::PLATRAPINDICES[pla][1]]);

    cout
    << i << ","
    << b.turnNumber << ","
    << pieceCount << ","
    << advancement << ","
    << plaTCScore << ","
    << oppTCScore << ",";

    cout << bestNS << ",";
    cout << bestEval << ",";
    for(int depth = 1; depth <= depthMax; depth++)
      cout << depthNNS[depth] << "," << depthNEvals[depth] << ",";
    for(int depth = 1; depth <= depthMax; depth++)
      cout << depthNFollowedByBestNS[depth] << "," << depthNFollowedByBestEvals[depth] << ",";
    cout << qs1DirectNS << "," << qs1DirectEval << ",";
    cout << qs1FollowedByBestNS << "," << qs1FollowedByBestEval << ",";
    cout << endl;
  }
  return EXIT_SUCCESS;
}

int MainFuncs::runEvalBounds(int argc, const char* const *argv)
{
  vector<string> mainCommand = Command::parseCommand(argc, argv);

  const char* required = "";
  const char* allowed = "q";
  const char* empty = "";
  const char* nonempty = "q";
  map<string,string> flags = Command::parseFlags(argc, argv, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2 && mainCommand.size() != 3)
    return EXIT_FAILURE;

  int qdepth = 2;
  if(flags.find("q") != flags.end())
    qdepth = Global::stringToInt(flags["q"]);
  int realDepth = qdepth == 0 ? 1 : qdepth == 1 ? -1 : -2;

  SearchParams params;
  initParams(params);
  Searcher searcher(params);

  if(mainCommand.size() == 2)
  {
    vector<BoardRecord> records = ArimaaIO::readBoardRecordFile(mainCommand[1]);
    vector<pair<double,pair<int,int> > > errorIndexEvalList;
    double totalSqerror = 0;
    double totalAbserror = 0;
    int totalErrorless = 0;
    int totalCount = records.size();
    for(int i = 0; i<totalCount; i++)
    {
      Board board = records[i].board;
      int eval = getEvaluation(searcher,realDepth,board);

      int error = 0;
      int leqBound = 0;
      int geqBound = 0;
      bool foundLEQ = false;
      bool foundGEQ = false;
      if(records[i].keyValues.find("LEQ") != records[i].keyValues.end())
      {
        foundLEQ = true;
        leqBound = Global::stringToInt(records[i].keyValues["LEQ"]);
        if(eval > leqBound)
          error = (eval - leqBound);
      }
      if(records[i].keyValues.find("GEQ") != records[i].keyValues.end())
      {
        foundGEQ = true;
        geqBound = Global::stringToInt(records[i].keyValues["GEQ"]);
        if(eval < geqBound)
          error = (geqBound - eval);
      }

      if(!foundLEQ && !foundGEQ)
        Global::fatalError("Key LEQ or GEQ not found");
      if(foundLEQ && foundGEQ && leqBound < geqBound)
        Global::fatalError("Crossed LEQ and GEQ bounds");

      double weight = 1;
      if(records[i].keyValues.find("WEIGHT") != records[i].keyValues.end())
        weight = Global::stringToDouble(records[i].keyValues["WEIGHT"]);
      if(weight <= 0)
        cout << "warning: zero weight" << endl;

      DEBUGASSERT(error >= 0);
      totalAbserror += weight * error;
      totalSqerror += weight * error * error;
      if(error == 0)
        totalErrorless++;

      records[i].keyValues["ERROR"] = Global::doubleToString(weight * error);

      errorIndexEvalList.push_back(make_pair(weight * error, make_pair(i,eval)));
    }

    std::sort(errorIndexEvalList.begin(),errorIndexEvalList.end());

    cout << "# Avg sqrt sqerror: " << (totalSqerror / totalCount) << endl;
    cout << "# Avg abs error: " << (totalAbserror / totalCount) << endl;
    cout << "# Total errorless: " << totalErrorless << endl;
    cout << "# Total instances: " << totalCount << endl;
    cout << "# Total sqrt sqerror: " << totalSqerror << endl;
    cout << "# Total abs error: " << totalAbserror << endl;
    cout << endl;
    for(int i = totalCount-1; i>=0; i--)
    {
      double error = errorIndexEvalList[i].first;
      if(error <= 0)
        break;

      int index = errorIndexEvalList[i].second.first;
      int eval = errorIndexEvalList[i].second.second;
      cout << "ACTUAL = " << eval << endl;
      cout << writeBoardRecord(records[index]) << endl;
      cout << ";" << endl;
    }
  }
  else
  {
    BoardRecord record = ArimaaIO::readBoardRecordFile(mainCommand[1],Global::stringToInt(mainCommand[2]));

    cout << writeBoardRecord(record) << endl;
    cout << ";" << endl;
    BoardHistory hist = BoardHistory(record.board);
    searcher.searchID(record.board,hist,realDepth,0,true);
  }

  return EXIT_SUCCESS;
}

static bool getBadGoodError(Searcher& searcher, const pair<BoardRecord,BoardRecord>& record,
    const vector<hash_t>& badHashes, bool excludeGoalThreatened, int realDepth,
    double& error, double& weight, int& evalBad, int& evalGood)
{
  if(record.first.keyValues.find("FORPLA") == record.first.keyValues.end())
    Global::fatalError("Key FORPLA not found");

  pla_t pla = ArimaaIO::readPla(record.first.keyValues.find("FORPLA")->second);
  Board firstBoard = record.first.board;
  Board secondBoard = record.second.board;

  if(isBadHash(secondBoard,badHashes))
    return false;

  firstBoard.setPlaStep(gOpp(pla),0);
  firstBoard.refreshStartHash();
  secondBoard.setPlaStep(gOpp(pla),0);
  secondBoard.refreshStartHash();

  if(excludeGoalThreatened && BoardTrees::goalDist(firstBoard,gOpp(pla),4) <= 4)
    return false;

  if(BoardTrees::goalDist(secondBoard,gOpp(pla),4) <= 4)
    return false;

  evalBad = -getEvaluation(searcher,realDepth,firstBoard);
  evalGood = -getEvaluation(searcher,realDepth,secondBoard);

  if(SearchUtils::isTerminalEval(evalGood))
    return false;
  if(SearchUtils::isTerminalEval(evalBad))
    return false;

  int diff = evalGood - evalBad;
  if(record.first.keyValues.find("DIFF") == record.first.keyValues.end())
    Global::fatalError("Key DIFF not found");
  int desiredDiff = Global::stringToInt(record.first.keyValues.find("DIFF")->second);

  error = diff < desiredDiff ? (desiredDiff - diff) : 0;

  weight = 1;
  if(record.first.keyValues.find("WEIGHT") != record.first.keyValues.end())
    weight = Global::stringToDouble(record.first.keyValues.find("WEIGHT")->second);
  if(weight <= 0)
    cout << "warning: zero weight" << endl;

  return true;
}


int MainFuncs::runEvalBadGood(int argc, const char* const *argv)
{
  vector<string> mainCommand = Command::parseCommand(argc, argv);

  const char* required = "";
  const char* allowed = "q badhashes excludegoalthreatened";
  const char* empty = "excludegoalthreatened";
  const char* nonempty = "q badhashes";
  map<string,string> flags = Command::parseFlags(argc, argv, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2 && mainCommand.size() != 3)
    return EXIT_FAILURE;

  int qdepth = 2;
  if(flags.find("q") != flags.end())
    qdepth = Global::stringToInt(flags["q"]);
  int realDepth = qdepth == 0 ? 1 : qdepth == 1 ? -1 : -2;

  //A list of hashes where if the move creates a board with that hash, skip it
  vector<hash_t> badHashes;
  if(flags.find("badhashes") != flags.end())
    badHashes = ArimaaIO::readHashListFile(flags["badhashes"]);

  //Do not test on positions where the BAD position has a goal threat - of course having
  //a goal threat against you is bad when it's the opponent's turn.
  bool excludeGoalThreatened = false;
  if(flags.find("excludegoalthreatened") != flags.end())
    excludeGoalThreatened = true;

  SearchParams params;
  initParams(params);
  Searcher searcher(params);

  if(mainCommand.size() == 2)
  {
    vector<pair<BoardRecord,BoardRecord> > records = ArimaaIO::readBadGoodPairFile(mainCommand[1]);
    vector<pair<double,int> > errorIndexList;
    double totalSqerror = 0;
    double totalAbserror = 0;
    int totalErrorless = 0;
    int totalCount = 0;
    for(int i = 0; i<(int)records.size(); i++)
    {
      if(i%10000 == 0)
        cout << "Finished " << i << "/" << records.size() << endl;

      double error;
      double weight;
      int evalBad;
      int evalGood;

      bool suc = getBadGoodError(searcher,records[i],badHashes,excludeGoalThreatened,realDepth,error,weight,evalBad,evalGood);
      if(!suc)
        continue;

      DEBUGASSERT(error >= 0);
      totalAbserror += weight * error;
      totalSqerror += weight * error * error;
      if(error <= 0)
        totalErrorless++;
      totalCount++;

      errorIndexList.push_back(make_pair(weight * error, i));
      records[i].first.keyValues["EVAL_BAD"] = Global::intToString(evalBad);
      records[i].first.keyValues["EVAL_GOOD"] = Global::intToString(evalGood);
    }

    std::sort(errorIndexList.begin(),errorIndexList.end());

    cout << "# Sqrt avg sqerror: " << sqrt(totalSqerror / totalCount) << endl;
    cout << "# Avg abs error: " << (totalAbserror / totalCount) << endl;
    cout << "# Total errorless: " << totalErrorless << endl;
    cout << "# Total instances: " << totalCount << endl;
    cout << "# Sqrt total sqerror: " << sqrt(totalSqerror) << endl;
    cout << "# Total abs error: " << totalAbserror << endl;
    cout << endl;
    for(int i = errorIndexList.size()-1; i>=0; i--)
    {
      double error = errorIndexList[i].first;
      if(error <= 0)
        break;

      int index = errorIndexList[i].second;
      cout << "ERROR = " << error << endl;
      cout << "BAD" << endl;
      cout << writeBoardRecord(records[index].first) << endl;
      cout << "GOOD" << endl;
      cout << writeBoardRecord(records[index].second) << endl;
      cout << ";" << endl;
    }
  }
  else
  {
    pair<BoardRecord,BoardRecord> record = ArimaaIO::readBadGoodPairFile(mainCommand[1],Global::stringToInt(mainCommand[2]));

    cout << "BAD" << endl;
    cout << writeBoardRecord(record.first) << endl;
    cout << "GOOD" << endl;
    cout << writeBoardRecord(record.first) << endl;
    cout << "================================================================" << endl;
    BoardHistory hist = BoardHistory(record.first.board);
    searcher.searchID(record.first.board,hist,realDepth,0,true);
    cout << "================================================================" << endl;
    hist = BoardHistory(record.second.board);
    searcher.searchID(record.second.board,hist,realDepth,0,true);
  }

  return EXIT_SUCCESS;
}
*/

/*
static const double PRIOR_SQERROR_FACTOR = 16000 * 16000;
static const double SQERRORMAX = 5000;
static const double LAMBDA = 0.7;
static const double TDLAMBDA_WEIGHT = 0.15;

static double getSqerrorMaxed(double error)
{
  if(error > SQERRORMAX)
    return SQERRORMAX * error;
  else
    return error * error;
}

static eval_t getEvaluation(Searcher& searcher, int realDepth, const Board& board)
{
  Board b = board;

  pla_t winner = board.getWinner();
  if(winner != NPLA)
    return winner == b.player ? Eval::WIN : Eval::LOSE;
  if(BoardTrees::goalDist(b,b.player,4-b.step) < 5)
    return Eval::WIN;
  else if(SearchMoveGen::definitelyForcedLossInTwo(b))
    return Eval::LOSE;

  eval_t eval;
  if(realDepth <= -2)
    eval = Eval::evaluate(b,NPLA,0,NULL);
  else
  {
    BoardHistory hist = BoardHistory(b);
    searcher.searchID(b,hist,realDepth,0);
    eval = searcher.stats.finalEval;
  }
  return eval;
}

static double getSqerror(const EvalParams& params, bool useParamsBase, const EvalParams& paramsBase,
    const vector<pair<BoardRecord,BoardRecord> >& bgRecords,
    const vector<GameRecord>& gameRecords, const vector<hash_t>& badHashes, int realDepth,
    double& priorError, double& bgError, double& tdError)
{
  SearchParams sparams;
  Searcher searcher(sparams);

  bool excludeGoalThreatened = true;
  double totalSqerror = 0;

  priorError = PRIOR_SQERROR_FACTOR * params.getPriorError();
  totalSqerror += priorError;

  tdError = 0;
  for(int i = 0; i<(int)gameRecords.size(); i++)
  {
    const GameRecord& game = gameRecords[i];
    if(game.winner == NPLA)
      continue;

    BoardHistory hist(game);

    bool hasBadHash = false;
    for(int j = hist.minTurnNumber; j < hist.maxTurnNumber; j++)
      if(isBadHash(hist.turnBoard[j],badHashes))
      {hasBadHash = true; break;}
    if(hasBadHash)
      continue;

    BoardHistory recordHist(game);
    vector<bool> doFilter = GameIterator::getFiltering(game,recordHist);

    vector<eval_t> evals;
    evals.resize(hist.maxTurnNumber+1);

    int turnMax = hist.maxTurnNumber-1;
    pla_t winner = game.winner;
    for(int j = hist.minTurnNumber; j < hist.maxTurnNumber; j++)
    {
      evals[j] = getEvaluation(searcher,realDepth,hist.turnBoard[j]);
      if(SearchUtils::isTerminalEval(evals[j]))
      {
        winner = evals[j] > 0 ? hist.turnBoard[j].player : gOpp(hist.turnBoard[j].player);
        turnMax = j - 1;
        break;
      }
      else
      {
        DEBUGASSERT(hist.turnBoard[j].getWinner() == NPLA);
      }
    }

    for(int j = turnMax; j >= hist.minTurnNumber; j--)
    {
      if(doFilter[j])
      {
        turnMax = j;
        break;
      }
    }

    for(pla_t pla = 0; pla <= 1; pla++)
    {
      for(int j = hist.minTurnNumber; j < turnMax; j++)
      {
        if(hist.turnBoard[j].player != pla)
          continue;

        int eval = getEvaluation(searcher,realDepth,hist.turnBoard[j]);
        if(SearchUtils::isTerminalEval(eval))
          continue;

        double futureEval = 0;
        double futureWeight = 1;
        double lastEval = 0;

        bool ranIntoFilter = false;
        for(int jj = j+1; jj < turnMax; jj++)
        {
          if(doFilter[jj])
          {
            ranIntoFilter = true;
            break;
          }

          if(hist.turnBoard[jj].player != pla)
            continue;
          double nowWeight = futureWeight - futureWeight * LAMBDA;
          futureWeight = futureWeight * LAMBDA;
          futureEval += evals[jj] * nowWeight;
          lastEval = evals[jj];
        }

        if(ranIntoFilter)
        {
          if(futureWeight >= 1)
            continue;

          double averageEvalSoFar = (double)futureEval/(1-futureWeight);
          double sqerror = getSqerrorMaxed(eval - averageEvalSoFar);
          tdError += TDLAMBDA_WEIGHT * sqerror * (1-futureWeight);
        }
        else
        {
          double averageEvalSoFar;
          if(futureWeight >= 1)
            averageEvalSoFar = eval;
          else
            averageEvalSoFar = (winner == pla) ?
              max((double)futureEval/(1-futureWeight),lastEval) :
              min((double)futureEval/(1-futureWeight),lastEval);

          double unexpectedBonus = (winner == pla) ?
            1 / (1 + exp(averageEvalSoFar/5000.0)) * 4500 :
            1 / (1 + exp(-averageEvalSoFar/5000.0)) * 4500;

          double winScore = (winner == pla) ?
              max(averageEvalSoFar + unexpectedBonus,(double)eval) :
              min(averageEvalSoFar - unexpectedBonus,(double)eval);

          futureEval += winScore * futureWeight;

          double sqerror = getSqerrorMaxed(eval - futureEval);
          tdError += TDLAMBDA_WEIGHT * sqerror;
        }
      }
    }
  }
  totalSqerror += tdError;

  return totalSqerror;
}

static string getErrorStr(double cur, double pr, double bg, double td)
{
  return Global::strprintf("%12.0f (pr %12.0f bg %12.0f td %12.0f)",cur,pr,bg,td);
}

static string getScaleStr(double scale)
{
  return Global::strprintf("%6.4f",scale);
}

int MainFuncs::optimizeEval(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile <flags>";
  const char* required = "q out";
  const char* allowed = "badhashes iters base tdnoparams";
  const char* empty = "tdnoparams";
  const char* nonempty = "q badhashes out iters base";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);

  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 3)
    return EXIT_FAILURE;

  int qdepth = Global::stringToInt(flags["q"]);
  int realDepth = qdepth == 0 ? 1 : qdepth == 1 ? -1 : -2;

  vector<hash_t> badHashes;
  if(flags.find("badhashes") != flags.end())
    badHashes = ArimaaIO::readHashListFile(flags["badhashes"]);

  string outFile = flags["out"];

  int iters = 100;
  if(flags.find("iters") != flags.end())
    iters = Global::stringToInt(flags["iters"]);


  cout << "Loading files..." << endl;
  vector<pair<BoardRecord,BoardRecord> > badGoodRecords = ArimaaIO::readBadGoodPairFile(mainCommand[1]);
  vector<GameRecord> gameRecords = ArimaaIO::readMovesFile(mainCommand[2]);

  EvalParams paramsBase;
  if(flags.find("base") != flags.end())
    paramsBase = EvalParams::inputFromFile(flags["base"]);

  bool useParamsBase = true;
  if(flags.find("tdnoparams") != flags.end())
    useParamsBase = false;

  ofstream out(outFile.c_str());
  ofstream outpartial((outFile + ".part").c_str());
  if(!out.good() || !outpartial.good())
  {
    cout << "Could not open " << outFile << " or " << (outFile + ".part") << endl;
    return EXIT_FAILURE;
  }

  EvalParams params = paramsBase;
  vector<double> scales;
  scales.resize(EvalParams::numBasisVectors,1);

  cout << "Starting eval optimization..." << endl;
  cout << EvalParams::fset.numFeatures << " features" << endl;
  cout << EvalParams::numBasisVectors << " basis vectors" << endl;

  double priorError,bgError,tdError;
  double currentSqerror = getSqerror(params,useParamsBase,paramsBase,badGoodRecords,gameRecords,badHashes,realDepth,priorError,bgError,tdError);
  cout << "Initial sqerr: " << getErrorStr(currentSqerror, priorError, bgError, tdError) << endl;

  for(int iter = 0; iter < iters; iter++)
  {
    cout << "Iteration " << iter << " ---------------------------------- " << endl;
    outpartial << "#" << iter << " ====================================================" << endl;
    outpartial << "#" << getErrorStr(currentSqerror, priorError, bgError, tdError) << endl;
    outpartial << params << endl;
    for(int i = 0; i<EvalParams::numBasisVectors; i++)
    {
      cout << Global::strprintf("%-26s",params.getBasisName(i).c_str());
      params.addBasis(i,scales[i]);
      double ipriorError,ibgError,itdError;
      double incSqerror = getSqerror(params,useParamsBase,paramsBase,badGoodRecords,gameRecords,badHashes,realDepth,ipriorError,ibgError,itdError);
      params.addBasis(i,-scales[i]*2);
      double dpriorError,dbgError,dtdError;
      double decSqerror = getSqerror(params,useParamsBase,paramsBase,badGoodRecords,gameRecords,badHashes,realDepth,dpriorError,dbgError,dtdError);
      params.addBasis(i,scales[i]);

      if(currentSqerror <= incSqerror && currentSqerror <= decSqerror)
      {
        cout << " .(" << getScaleStr(scales[i]) << ") " << getErrorStr(currentSqerror, priorError, bgError, tdError);
        if(incSqerror <= decSqerror)
          cout << " " << getErrorStr(incSqerror, ipriorError, ibgError, itdError) << endl;
        else
          cout << " " << getErrorStr(decSqerror, dpriorError, dbgError, dtdError) << endl;

        scales[i] *= 0.65;
        continue;
      }
      else if(incSqerror <= decSqerror)
      {
        currentSqerror = incSqerror; priorError = ipriorError; bgError = ibgError; tdError = itdError;
        cout << " +(" << getScaleStr(scales[i]) << ") " << getErrorStr(currentSqerror, priorError, bgError, tdError) << endl;
        params.addBasis(i,scales[i]);
        scales[i] *= 1.1;
        continue;
      }
      else
      {
        currentSqerror = decSqerror; priorError = dpriorError; bgError = dbgError; tdError = dtdError;
        cout << " -(" << getScaleStr(scales[i]) << ") " << getErrorStr(currentSqerror, priorError, bgError, tdError) << endl;
        params.addBasis(i,-scales[i]);
        scales[i] *= 1.1;
        continue;
      }
    }
  }
  outpartial.close();

  out << params << endl;
  out.close();

  return EXIT_SUCCESS;
}

*/


