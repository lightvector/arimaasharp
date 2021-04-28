
/*
 * learner.cpp
 * Author: davidwu
 */

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "../core/global.h"
#include "../core/timer.h"
#include "../learning/gameiterator.h"
#include "../learning/feature.h"
#include "../learning/featurearimaa.h"
#include "../learning/featuremove.h"
#include "../learning/learner.h"

using namespace std;

//-------------------------------------------------------------------------

NaiveBayes::NaiveBayes(ArimaaFeatureSet afset, int nFeatures, double pstr)
:afset(afset)
{
  numFeatures = nFeatures;
  totalFreq = 0;
  featureFreq[0].resize(numFeatures);
  featureFreq[1].resize(numFeatures);
  outcomeFreq[0] = 0;
  outcomeFreq[1] = 0;
  priorStrength = pstr;
  logWinOdds = 0;
  logWinOddsIfNoFeatures = 0;
}

NaiveBayes::~NaiveBayes()
{

}

void NaiveBayes::train(GameIterator& iter)
{
  cout << "Training NaiveBayes" << endl;

  int numTrained = 0;
  while(iter.next())
  {
    if(numTrained%1000 == 0)
      cout << "Training NB: " << numTrained << endl;
    numTrained++;

    int winningTeam;
    vector<vector<findex_t> > teams;
    iter.getNextMoveFeatures(afset,winningTeam,teams);

    int size = teams.size();
    for(int i = 0; i<size; i++)
    {
      totalFreq++;

      int outcome = (i == winningTeam) ? 1 : 0;
      outcomeFreq[outcome]++;

      int teamsize = teams[i].size();
      for(int j = 0; j<teamsize; j++)
        featureFreq[outcome][teams[i][j]]++;
    }
  }

  computeLogOdds();

  cout << "Trained on " << numTrained << " instances" << endl;
}

void NaiveBayes::computeLogOdds()
{
  logWinOdds = log((double)outcomeFreq[1] / (double)outcomeFreq[0]);

  double virtualLoss = priorStrength * outcomeFreq[0] / (double)totalFreq;
  double virtualWin = priorStrength * outcomeFreq[1] / (double)totalFreq;

  logWinOddsIfNoFeatures = logWinOdds;
  logLikelihoodIfPresent.resize(numFeatures);
  logLikelihoodIfAbsent.resize(numFeatures);
  for(int i = 0; i<numFeatures; i++)
  {
    double pFeatureGLoss = (featureFreq[0][i]+virtualLoss)/(outcomeFreq[0]+virtualLoss*2);
    double pFeatureGWin = (featureFreq[1][i]+virtualWin)/(outcomeFreq[1]+virtualWin*2);
    logLikelihoodIfPresent[i] = log(pFeatureGWin / pFeatureGLoss);

    double pNoFeatureGLoss = (outcomeFreq[0]-featureFreq[0][i]+virtualLoss)/(outcomeFreq[0]+virtualLoss*2);
    double pNoFeatureGWin = (outcomeFreq[1]-featureFreq[1][i]+virtualWin)/(outcomeFreq[1]+virtualWin*2);
    logLikelihoodIfAbsent[i] = log(pNoFeatureGWin / pNoFeatureGLoss);

    logWinOddsIfNoFeatures += logLikelihoodIfAbsent[i];
  }
}

double NaiveBayes::evaluate(const vector<findex_t>& team)
{
  int teamSize = team.size();
  double logOdds = logWinOddsIfNoFeatures;
  for(int j = 0; j<teamSize; j++)
  {
    logOdds += logLikelihoodIfPresent[team[j]] - logLikelihoodIfAbsent[team[j]];
  }
  return logOdds;
}

void NaiveBayes::outputToFile(const char* file)
{
  ofstream out;
  out.open(file,ios::out);
  out.precision(14);
  out << numFeatures << endl;
  out << priorStrength << endl;
  out << totalFreq << endl;
  out << outcomeFreq[0] << " " << outcomeFreq[1] << endl;

  for(int i = 0; i<(int)featureFreq[0].size(); i++)
    out << featureFreq[0][i] << " " << featureFreq[1][i] << endl;
  out.close();
}

NaiveBayes NaiveBayes::inputFromFile(ArimaaFeatureSet afset, const char* file)
{
  ifstream in;
  in.open(file,ios::in);
  if(in.fail())
  {
    cout << "Failed to input from " << file << endl;
    return NaiveBayes(afset,0,0);
  }
  int numFeatures;
  double priorStrength;
  in >> numFeatures;
  in >> priorStrength;
  NaiveBayes nb(afset,numFeatures,priorStrength);
  in >> nb.totalFreq;
  in >> nb.outcomeFreq[0];
  in >> nb.outcomeFreq[1];
  for(int i = 0; i<numFeatures; i++)
  {
    in >> nb.featureFreq[0][i];
    in >> nb.featureFreq[1][i];
  }
  in.close();

  nb.computeLogOdds();

  return nb;
}

//-------------------------------------------------------------------------------------------

BradleyTerry::BradleyTerry(ArimaaFeatureSet afset)
:afset(afset)
{
  numFeatures = afset.fset->numFeatures;
  numFolds = afset.numParallelFolds;
  gamma.resize(numFolds);
  logGamma.resize(numFolds);

  for(int pf = 0; pf<numFolds; pf++)
  {
    gamma[pf].resize(numFeatures);
    logGamma[pf].resize(numFeatures);
    for(int i = 0; i<numFeatures; i++)
      gamma[pf][i] = 1;
    for(int i = 0; i<numFeatures; i++)
      logGamma[pf][i] = 0;
  }
}

BradleyTerry::~BradleyTerry()
{

}

static void outputBTToFile(const char* file, const char* notes,
    ArimaaFeatureSet afset, const vector<vector<double>>& logGamma);
static void outputBTToFile(const GameIterator& trainingIter, const char* file, const char* notes,
    ArimaaFeatureSet afset, const vector<vector<double>>& logGamma);
static vector<vector<double>> readDoublesFile(const string& file);
static void writeDoublesFile(const string& file, const vector<vector<double>>& doubles);
static vector<vector<int>> readIntsFile(const string& file);
static void writeIntsFile(const string& file, const vector<vector<int>>& ints);

//Encodes a sequence of MTDs for matches/teams in increasing order.
//Each feature has a sequence of MTDs, indicating that the feature is present in the specified team of the
//specified match, and that the feature is present on that team *degree* times.
//Format for each MTD in the stream is a sequence of bytes encoding the difference in the match and team from
//the previous. This is done together by the delta in the number of teams that occurs if you concatenate all matches
//together consecutively. Then, if the degree is not 1, this is followed by bytes that encode the degree.
//
//Actually, we always encode x = 2*delta + (degree != 1), so that we can tell when there are bytes following that encode the
//degree or not.
//
//For encoding any of the x's or the degree's, we write its bits in the form a0a1a2... where each ai is 7 bits,
//then we write the bytes:
//  <bit 7: hasNext, bit 6-0: a0>
//  <bit 7: hasNext, bit 6-0: a1>
//  <bit 7: hasNext, bit 6-0: a2>
//  ...

namespace {

STRUCT_NAMED_TRIPLE(int,match,int,team,int,degree,MTD);

struct MTDByteStream
{
  vector<uint8_t> bytes;
  MTD writeHead;

  MTD readHead;
  int readIndex;

  MTDByteStream()
  :bytes(),
   writeHead(0,0,0),
   readHead(0,0,0),
   readIndex(0)
  {
  }

  void resetWrite()
  {
    writeHead = MTD(0,0,0);
    bytes.clear();
    resetRead();
  }

  void resetRead()
  {
    readHead = MTD(0,0,0);
    readIndex = 0;
  }

  void writeUInt(uint32_t x)
  {
    int numNewBytes = 0;
    uint8_t newBytes[5];

    do
    {
      newBytes[numNewBytes++] = x & 0x7F;
      x = x >> 7;
    } while(x > 0);

    for(int i = numNewBytes-1; i > 0; i--)
      bytes.push_back(newBytes[i] | 0x80);
    bytes.push_back(newBytes[0]);
  }

  uint32_t readUInt()
  {
    uint32_t value = 0;
    bool hasNext = true;
    while(hasNext)
    {
      uint8_t byte = bytes[readIndex++];
      hasNext = ((byte & 0x80) != 0);
      value = value * 128 + (byte & 0x7F);
    }
    DEBUGASSERT(readIndex <= (int)bytes.size());
    return value;
  }

  void write(const MTD& mtd, const vector<int>& matchNumTeams)
  {
    if(mtd.degree == 0)
      return;

    //Compute the number of teams between this and the previous
    int delta = -writeHead.team;
    for(int m = writeHead.match; m < mtd.match; m++)
      delta += matchNumTeams[m];
    delta += mtd.team;

    //Assert that the top three bits are zero, which also means delta is nonnegative
    DEBUGASSERT((delta & 0xE0000000) == 0);

    delta = 2*delta + (mtd.degree != 1);

    writeUInt(delta);

    if(mtd.degree != 1)
    {
      //Fold the degree to force it to be positive. Zero is unused because of the return at the top,
      //so we map negatives to the positive evens and positives to the positive odds
      int folded = (mtd.degree < 0) ? -mtd.degree * 2 - 2 : mtd.degree * 2 - 1;
      writeUInt(folded);
    }

    writeHead = mtd;
  }

  bool canRead()
  {
    return readIndex < (int)bytes.size();
  }

  MTD read(const vector<int>& matchNumTeams)
  {
    int delta = readUInt();
    int degree = (delta & 0x1) ? readUInt() : 1;
    delta = delta >> 1;

    //Compute the team and match based on the delta from the previous
    delta += readHead.team;
    int match = readHead.match;
    while(delta - matchNumTeams[match] >= 0)
    {
      delta = delta - matchNumTeams[match];
      match++;
    }
    int team = delta;

    //Unfold the degree
    if(degree & 0x1)
      degree = (degree + 1) >> 1;
    else
      degree = -((degree >> 1) + 1);

    readHead = MTD(match,team,degree);
    return readHead;
  }
};
}

static double computeLogProb(const ArimaaFeatureSet& afset,
    const vector<vector<double>>& matchTeamStrength, const vector<int>& matchWinners,
    const vector<double>& matchWeight, const vector<vector<double>>& logGammas)
{
  double logProbSum = 0.0;

  int numMatches = (int)matchTeamStrength.size();
  for(int m = 0; m<numMatches; m++)
  {
    const vector<double>& teamStrength = matchTeamStrength[m];

    double winLogProb = log(teamStrength[matchWinners[m]]);

    int numTeams = (int)teamStrength.size();
    double totalStrength = 0.0;
    for(int t = 0; t<numTeams; t++)
      totalStrength += teamStrength[t];

    double totalLogProb = -log(totalStrength);
    logProbSum += matchWeight[m] * (winLogProb + totalLogProb);
  }
  logProbSum += afset.getPriorLogProb(logGammas);

  return logProbSum;
}

static void updateMatchTeamStrengths(const ArimaaFeatureSet& afset, int parallelFold, findex_t feature,
    double deltaLogGamma, const vector<int>& matchNumTeams, const vector<double>& matchParallelFactor,
    vector<MTDByteStream>& featureMatchTeams, vector<vector<double>>& matchTeamLogStrength,
    vector<vector<double>>& matchTeamStrength)
{
  (void)afset;
  (void)parallelFold;
  MTDByteStream& matchTeamList = featureMatchTeams[feature];
  matchTeamList.resetRead();
  while(matchTeamList.canRead())
  {
    const MTD& mtd = matchTeamList.read(matchNumTeams);
    int match = mtd.match;
    int team = mtd.team;
    int degree = mtd.degree;
    DEBUGASSERT(match >= 0);
    DEBUGASSERT(team >= 0);
    DEBUGASSERT(match < (int)matchTeamLogStrength.size());
    DEBUGASSERT(team < (int)matchTeamLogStrength[match].size());
    matchTeamLogStrength[match][team] += deltaLogGamma * degree * matchParallelFactor[match];
    matchTeamStrength[match][team] = exp(matchTeamLogStrength[match][team]);
  }
}

static const int LAST_FAIL = 0;
static const int LAST_POS = 1;
static const int LAST_NEG = 2;

static void trainGradientHelperSingleTwiddle(const ArimaaFeatureSet& afset,
    int parallelFold, findex_t f,
    vector<vector<double>>& deltaSize, vector<vector<double>>& logGamma, double& logProb,
    vector<vector<int>>& lastType,
    vector<MTDByteStream>& featureMatchTeams,
    const vector<int>& matchNumTeams, const vector<int>& matchWinners, const vector<double>& matchWeight,
    const vector<vector<double>>& foldMatchParallelFactors,
    vector<vector<double>>& matchTeamLogStrength,
    vector<vector<double>>& matchTeamStrength,
    const vector<int64_t>& winFrequency,
    const vector<int64_t>& frequency
    )
{
  int pf = parallelFold;

  //Store all old log gammas for folds for this feature
  double oldLogGamma = logGamma[pf][f];

  //Twiddle the feature positive to see if the log prob improves
  updateMatchTeamStrengths(afset,pf,f,deltaSize[pf][f],matchNumTeams,foldMatchParallelFactors[pf],
      featureMatchTeams,matchTeamLogStrength,matchTeamStrength);
  logGamma[pf][f] += deltaSize[pf][f];
  double logProbPos = computeLogProb(afset,matchTeamStrength,matchWinners,matchWeight,logGamma);

  if(logProbPos > logProb)
  {
    logProb = logProbPos;
    cout << f << " " << pf << " " << afset.fset->getName(f) << " " << logGamma[pf][f] <<
        " [+" << deltaSize[pf][f] << "] logProb " << Global::strprintf("%.2f,",logProb) <<
        "(" << winFrequency[f] << "/" << frequency[f] << ")" << endl;

    if(lastType[pf][f] == LAST_POS)
      deltaSize[pf][f] *= 1.2;
    else
      deltaSize[pf][f] *= 0.9;
    lastType[pf][f] = LAST_POS;
    return;
  }
  //Restore
  logGamma[pf][f] = oldLogGamma;

  //Twiddle the feature negative to see if the log prob improves
  updateMatchTeamStrengths(afset,pf,f,-2.0*deltaSize[pf][f],matchNumTeams,foldMatchParallelFactors[pf],
      featureMatchTeams,matchTeamLogStrength,matchTeamStrength);
  logGamma[pf][f] -= deltaSize[pf][f];
  double logProbNeg = computeLogProb(afset,matchTeamStrength,matchWinners,matchWeight,logGamma);

  if(logProbNeg > logProb)
  {
    logProb = logProbNeg;
    cout << f << " " << pf << " " << afset.fset->getName(f) << " " << logGamma[pf][f] <<
        " [-" << deltaSize[pf][f] << "] logProb " << Global::strprintf("%.2f,",logProb) <<
        "(" << winFrequency[f] << "/" << frequency[f] << ")" << endl;

    if(lastType[pf][f] == LAST_NEG)
      deltaSize[pf][f] *= 1.2;
    else
      deltaSize[pf][f] *= 0.9;
    lastType[pf][f] = LAST_NEG;
    return;
  }
  //Restore
  logGamma[pf][f] = oldLogGamma;

  //Restore old feature value
  updateMatchTeamStrengths(afset,pf,f,deltaSize[pf][f],matchNumTeams,foldMatchParallelFactors[pf],
      featureMatchTeams,matchTeamLogStrength,matchTeamStrength);

  cout << f << " " << pf << " " << afset.fset->getName(f) << " " << logGamma[pf][f] <<
      " [!" << deltaSize[pf][f] << "] logProb " << Global::strprintf("%.2f,",logProb) <<
      "(" << winFrequency[f] << "/" << frequency[f] << ")" << endl;

  deltaSize[pf][f] *= 0.6;
  lastType[pf][f] = LAST_FAIL;
}


static vector<vector<double>> trainGradientHelper(const ArimaaFeatureSet& afset,
    int numIters, vector<MTDByteStream>& featureMatchTeams,
    const vector<int>& matchNumTeams, const vector<int>& matchWinners, const vector<double>& matchWeight,
    const vector<vector<double>>& foldMatchParallelFactors,
    const vector<int64_t>& winFrequency, const vector<int64_t>& frequency,
    const vector<vector<double>>* initialLogGamma,
    const vector<vector<int>>* initialLastType,
    const vector<vector<double>>* initialDelta,
    int numPrevIterations,
    const string& partialFilePrefix)
{
  //We use the last index (== numFolds) to store lasttype and delta for adjustments to the
  //joint values of all the folds together
  int numFolds = afset.numParallelFolds;
  DEBUGASSERT((int)foldMatchParallelFactors.size() == numFolds);
  int numFeatures = featureMatchTeams.size();
  int numMatches = matchNumTeams.size();

  vector<vector<double>> logGamma;
  logGamma.resize(numFolds);
  for(int pf = 0; pf < numFolds; pf++)
    logGamma[pf].resize(numFeatures,0.0);

  vector<vector<double>> matchTeamLogStrength;
  vector<vector<double>> matchTeamStrength;
  matchTeamLogStrength.reserve(numMatches);
  int64_t numTotalTeams = 0;
  double numWeightedMatches = 0;
  for(int m = 0; m<numMatches; m++)
  {
    int numTeams = matchNumTeams[m];
    vector<double> teamLogStrength;
    vector<double> teamStrength;
    teamLogStrength.resize(numTeams,0.0);
    teamStrength.resize(numTeams,1.0);

    matchTeamLogStrength.push_back(teamLogStrength);
    matchTeamStrength.push_back(teamStrength);
    numTotalTeams += numTeams;
    numWeightedMatches += matchWeight[m];
  }

  int64_t numTotalFeatureInstances = 0;
  for(int f = 0; f<numFeatures; f++)
    numTotalFeatureInstances += frequency[f];

  vector<vector<double>> deltaSize;
  vector<vector<int>> lastType;
  lastType.resize(numFolds);
  deltaSize.resize(numFolds);
  for(int pf = 0; pf < numFolds; pf++)
  {
    lastType[pf].resize(numFeatures,0);
    deltaSize[pf].resize(numFeatures,1.0);
  }

  DEBUGASSERT((initialLogGamma != NULL) == (initialLastType != NULL));
  DEBUGASSERT((initialLogGamma != NULL) == (initialDelta != NULL));
  DEBUGASSERT((initialLogGamma != NULL) || (numPrevIterations == 0));
  if(initialLogGamma != NULL)
  {
    const vector<vector<double>>& initLogGamma = *initialLogGamma;
    const vector<vector<int>>& initLastType = *initialLastType;
    const vector<vector<double>>& initDelta = *initialDelta;
    DEBUGASSERT((int)initLogGamma.size() == numFolds);
    DEBUGASSERT((int)initLastType.size() == numFolds);
    DEBUGASSERT((int)initDelta.size() == numFolds);

    for(int pf = 0; pf<numFolds; pf++)
    {
      const vector<double>& itLogGamma = initLogGamma[pf];
      const vector<int>& itLastType = initLastType[pf];
      const vector<double>& itDelta = initDelta[pf];
      DEBUGASSERT((int)itLogGamma.size() == numFeatures);
      DEBUGASSERT((int)itLastType.size() == numFeatures);
      DEBUGASSERT((int)itDelta.size() == numFeatures);

      for(int i = 0; i<numFeatures; i++)
      {
        updateMatchTeamStrengths(afset,pf,i,itLogGamma[i],matchNumTeams,foldMatchParallelFactors[pf],
            featureMatchTeams,matchTeamLogStrength,matchTeamStrength);
        logGamma[pf][i] = itLogGamma[i];
        lastType[pf][i] = itLastType[i];
        deltaSize[pf][i] = itDelta[i];
      }
    }
  }

  ClockTimer timer;
  double logProb = computeLogProb(afset,matchTeamStrength,matchWinners,matchWeight,logGamma);
  for(int iter = numPrevIterations; iter <= numIters; iter++)
  {
    cout << "Training BT iteration " << iter << "/" << numIters << endl;
    cout << "logProb " << Global::strprintf("%.3f,",logProb)
         << " (" << (logProb / numWeightedMatches) << "/weightedmatch)"
         << " (" << (logProb / numMatches) << "/match)" << endl;
    cout << "Num Matches: " << numMatches << endl;
    cout << "Num Weighted Matches: " << numWeightedMatches << endl;
    cout << "Num Total Teams:" << numTotalTeams
         << " (" << ((double)numTotalTeams / numMatches) << "/match)" << endl;
    cout << "Num Total Feature Instances:" << numTotalFeatureInstances << " ("
        << ((double)numTotalFeatureInstances / numTotalTeams) << "/team)" << endl;
    cout << "Seconds since start of iterating " << timer.getSeconds() << endl;
    double maxDelta = 0.0;
    for(int pf = 0; pf<numFolds; pf++)
    {
      for(int i = 0; i<numFeatures; i++)
      {
        if(frequency[i] <= 0 && !afset.doesCoeffMatterForPrior(i))
          continue;
        if(deltaSize[pf][i] > maxDelta)
          maxDelta = deltaSize[pf][i];
      }
    }
    cout << "Max feature coeff delta: " << maxDelta << endl;

    //Output intermediate weights
    if(partialFilePrefix.length() > 0 && (iter <= 5 || (iter % 2 == 0 && iter <= 20) || (iter % 5 == 0)))
    {
      string file = partialFilePrefix + ".iter" + Global::intToString(iter) + ".partial";
      outputBTToFile(file.c_str(),"",afset,logGamma);
      string filelasttype = partialFilePrefix + ".iter" + Global::intToString(iter) + ".partial.lasttype";
      string filedeltas = partialFilePrefix + ".iter" + Global::intToString(iter) + ".partial.deltas";
      writeIntsFile(filelasttype,lastType);
      writeDoublesFile(filedeltas,deltaSize);
    }

    if(iter >= numIters)
      break;

    //Update for all features
    for(int f = 0; f<numFeatures; f++)
    {
      if(frequency[f] <= 0 && !afset.doesCoeffMatterForPrior(f))
        continue;

      //TODO hack - do a few iterations first to get the main average feature value to convergence before
      //fiddling with the other parallel folds, and focus on the average value
      int numFoldsToIter = (iter <= 6 || iter % 2 == 0) ? 1 : numFolds;
      for(int pf = 0; pf < numFoldsToIter; pf++)
      {
        trainGradientHelperSingleTwiddle(afset,pf,f,
            deltaSize, logGamma, logProb,
            lastType,
            featureMatchTeams,
            matchNumTeams, matchWinners, matchWeight,
            foldMatchParallelFactors,
            matchTeamLogStrength,
            matchTeamStrength,
            winFrequency,
            frequency
        );
      }
    }
  }

  return logGamma;

}

static void initializeFromGames(const ArimaaFeatureSet& afset, GameIterator& iter, vector<int>& matchWinners,
    vector<int>& matchNumTeams, vector<double>& matchWeight, vector<vector<double>>& foldMatchParallelFactors,
    vector<MTDByteStream>& featureMatchTeams,
    vector<int64_t>& winFrequency, vector<int64_t>& frequency)
{
  //We use the last index (== numFolds) to store the sum of the parallel factors
  int numFolds = afset.numParallelFolds;
  int numFeatures = afset.fset->numFeatures;
  winFrequency.resize(numFeatures);
  frequency.resize(numFeatures);
  featureMatchTeams.resize(numFeatures);
  foldMatchParallelFactors.resize(numFolds);

  int match = 0;
  match = matchWinners.size();

  cout << "Loading " << iter.getTotalNumGames() << " games" << endl;
  int winningTeam;
  vector<vector<findex_t> > teams;
  vector<double> matchParallelFactorsBuf;
  while(iter.next())
  {
    iter.getNextMoveFeatures(afset,winningTeam,teams,matchParallelFactorsBuf);
    double posWeight = iter.getPosWeight();
    matchWinners.push_back(winningTeam);
    matchNumTeams.push_back((int)teams.size());
    matchWeight.push_back(posWeight);

    DEBUGASSERT((int)matchParallelFactorsBuf.size() == numFolds);
    for(int pf = 0; pf < numFolds; pf++)
      foldMatchParallelFactors[pf].push_back(matchParallelFactorsBuf[pf]);

    int numTeams = teams.size();
    for(int t = 0; t<numTeams; t++)
    {
      const vector<findex_t>& members = teams[t];
      int numMembers = members.size();
      for(int i = 0; i<numMembers; i++)
      {
        MTD mtd;
        mtd.match = match;
        mtd.team = t;
        mtd.degree = 1;
        featureMatchTeams[members[i]].write(mtd,matchNumTeams);
        frequency[members[i]]++;
        if(t == winningTeam)
          winFrequency[members[i]]++;
      }
    }
    match++;
  }
}

void BradleyTerry::resumeTraining(GameIterator& iter, int numPrevIterations, int numIterations,
    const string& initialLastTypeFile, const string& initialDeltaFile, const string& partialFilePrefix)
{
  vector<int64_t> winFrequency;
  vector<int64_t> frequency;
  vector<int> matchWinners;
  vector<int> matchNumTeams;
  vector<double> matchWeight;
  vector<vector<double>> foldMatchParallelFactors;
  vector<MTDByteStream> featureMatchTeams;

  DEBUGASSERT(numFolds == afset.numParallelFolds);
  vector<vector<int>> initialLastType = readIntsFile(initialLastTypeFile);
  vector<vector<double>> initialDelta = readDoublesFile(initialDeltaFile);
  DEBUGASSERT((int)initialLastType.size() == numFolds);
  DEBUGASSERT((int)initialDelta.size() == numFolds);
  for(int pf = 0; pf<numFolds; pf++)
  {
    DEBUGASSERT((int)initialLastType[pf].size() == afset.fset->numFeatures);
    DEBUGASSERT((int)initialDelta[pf].size() == afset.fset->numFeatures);
  }

  initializeFromGames(afset,iter,matchWinners,matchNumTeams,matchWeight,foldMatchParallelFactors,featureMatchTeams,winFrequency,frequency);

  logGamma = trainGradientHelper(afset,numIterations,featureMatchTeams,matchNumTeams,matchWinners,matchWeight,
      foldMatchParallelFactors,
      winFrequency,frequency,
      &logGamma, &initialLastType, &initialDelta, numPrevIterations, partialFilePrefix);
  for(int pf = 0; pf<numFolds; pf++)
    for(int i = 0; i<numFeatures; i++)
      gamma[pf][i] = exp(logGamma[pf][i]);

}


void BradleyTerry::train(GameIterator& iter, int numIterations)
{
  train(iter,numIterations,string());
}

void BradleyTerry::train(GameIterator& iter, int numIterations, const string& partialFilePrefix)
{
  vector<int64_t> winFrequency;
  vector<int64_t> frequency;
  vector<int> matchWinners;
  vector<int> matchNumTeams;
  vector<double> matchWeight;
  vector<vector<double>> foldMatchParallelFactors;
  vector<MTDByteStream> featureMatchTeams;

  const vector<vector<double>>* initialLogGamma = NULL;
  const vector<vector<int>>* initialLastType = NULL;
  const vector<vector<double>>* initialDelta = NULL;
  int numPrevIterations = 0;

  initializeFromGames(afset,iter,matchWinners,matchNumTeams,matchWeight,foldMatchParallelFactors,featureMatchTeams,winFrequency,frequency);

  logGamma = trainGradientHelper(afset,numIterations,featureMatchTeams,matchNumTeams,matchWinners,matchWeight,
      foldMatchParallelFactors,
      winFrequency,frequency,
      initialLogGamma, initialLastType, initialDelta, numPrevIterations, partialFilePrefix);
  for(int pf = 0; pf<numFolds; pf++)
    for(int i = 0; i<numFeatures; i++)
      gamma[pf][i] = exp(logGamma[pf][i]);

}

void BradleyTerry::train(const vector<vector<vector<findex_t> > >& matches, const vector<int>& winners, int numIterations, const string& partialFilePrefix)
{
  vector<int64_t> winFrequency;
  vector<int64_t> frequency;
  vector<int> matchWinners;
  vector<int> matchNumTeams;
  vector<double> matchWeight;
  vector<vector<double>> foldMatchParallelFactors;
  vector<MTDByteStream> featureMatchTeams;
  frequency.resize(numFeatures);
  featureMatchTeams.resize(numFeatures);

  DEBUGASSERT(numFolds == 1);
  foldMatchParallelFactors.resize(numFolds);

  int match = 0;

  int numMatches = matches.size();
  for(int m = 0; m<numMatches; m++)
  {
    const vector<vector<findex_t> >& teams = matches[m];
    matchWinners.push_back(winners[m]);
    matchNumTeams.push_back((int)teams.size());
    matchWeight.push_back(1.0);
    foldMatchParallelFactors[0].push_back(1.0);

    int numTeams = teams.size();
    for(int t = 0; t<numTeams; t++)
    {
      const vector<findex_t>& members = teams[t];
      int numMembers = members.size();
      for(int i = 0; i<numMembers; i++)
      {
        MTD mtd;
        mtd.match = match;
        mtd.team = t;
        mtd.degree = 1;
        featureMatchTeams[members[i]].write(mtd,matchNumTeams);
        frequency[members[i]]++;
        if(t == winners[m])
          winFrequency[members[i]]++;
      }
    }
    match++;
  }

  const vector<vector<double>>* initialLogGamma = NULL;
  const vector<vector<int>>* initialLastType = NULL;
  const vector<vector<double>>* initialDelta = NULL;
  int numPrevIterations = 0;

  logGamma = trainGradientHelper(afset,numIterations,featureMatchTeams,matchNumTeams,matchWinners,matchWeight,
      foldMatchParallelFactors,
      winFrequency,frequency,
      initialLogGamma, initialLastType, initialDelta, numPrevIterations, partialFilePrefix);
  for(int pf = 0; pf<numFolds; pf++)
    for(int i = 0; i<numFeatures; i++)
      gamma[pf][i] = exp(logGamma[pf][i]);
}

double BradleyTerry::evaluate(const vector<findex_t>& team, vector<double>& matchParallelFactors)
{
  double str = 0.0;
  for(int pf = 0; pf<numFolds; pf++)
    for(int i = 0; i<(int)team.size(); i++)
      str += logGamma[pf][team[i]] * matchParallelFactors[pf];
  return str;
}

static void outputBTToFile(const char* file, const char* notes,
    ArimaaFeatureSet afset, const vector<vector<double>>& logGamma)
{
  int numFolds = afset.numParallelFolds;
  int numFeatures = afset.fset->numFeatures;
  DEBUGASSERT((int)logGamma.size() == numFolds);
  for(int pf = 0; pf<numFolds; pf++)
    DEBUGASSERT((int)logGamma[pf].size() == numFeatures);

  ofstream out;
  out.open(file,ios::out);
  out.precision(20);
  out << "BradleyTerry3" << endl;
  out << numFeatures << endl;
  out << numFolds << endl;
  out << "#" << notes << endl;
  for(int i = 0; i<numFeatures; i++)
    for(int pf = 0; pf<numFolds; pf++)
      out << afset.fset->getName(i) << "\t" << pf << "\t" << logGamma[pf][i] << endl;
  out.close();
}

static void outputBTToFile(const GameIterator& trainingIter, const char* file, const char* notes,
    ArimaaFeatureSet afset, const vector<vector<double>>& logGamma)
{
  int numFolds = afset.numParallelFolds;
  int numFeatures = afset.fset->numFeatures;
  DEBUGASSERT((int)logGamma.size() == numFolds);
  for(int pf = 0; pf<numFolds; pf++)
    DEBUGASSERT((int)logGamma[pf].size() == numFeatures);

  ofstream out;
  out.open(file,ios::out);
  out.precision(20);
  out << "BradleyTerry3" << endl;
  out << numFeatures << endl;
  out << numFolds << endl;

  out << "#" << notes << endl;
  vector<string> iterNotes = trainingIter.getTrainingPropertiesComments();
  for(size_t i = 0; i<iterNotes.size(); i++)
    out << "#"<< iterNotes[i] << endl;

  for(int i = 0; i<numFeatures; i++)
    for(int pf = 0; pf<numFolds; pf++)
      out << afset.fset->getName(i) << "\t" << pf << "\t" << logGamma[pf][i] << endl;
  out.close();
}

void BradleyTerry::outputToFile(const char* file, const char* notes)
{
  outputBTToFile(file,notes,afset,logGamma);
}

void BradleyTerry::outputToFile(const GameIterator& trainingIter, const char* file, const char* notes)
{
  outputBTToFile(trainingIter,file,notes,afset,logGamma);
}

#include "../learning/defaultmoveweights.h"

BradleyTerry BradleyTerry::inputFromDefault(ArimaaFeatureSet afset)
{
  istringstream in(DEFAULT_MOVE_WEIGHTS);
  if(in.fail())
  {
    Global::fatalError(string("BradleyTerry::inputFromDefault: Failed to input"));
    return BradleyTerry(afset);
  }
  return inputFromIStream(afset,in);
}

BradleyTerry BradleyTerry::inputFromFile(ArimaaFeatureSet afset, const char* file)
{
  ifstream in;
  in.open(file,ios::in);
  if(in.fail())
  {
    Global::fatalError(string("BradleyTerry::inputFromFile: Failed to input from ") + string(file));
    return BradleyTerry(afset);
  }
  BradleyTerry t = inputFromIStream(afset,in);
  in.close();
  return t;
}

BradleyTerry BradleyTerry::inputFromIStream(ArimaaFeatureSet afset, istream& in)
{
  string header;
  in >> header;
  if(header != string("BradleyTerry3"))
  {
    Global::fatalError(string("BradleyTerry::inputFromIStream: Invalid header"));
    return BradleyTerry(afset);
  }

  int numFeatures;
  int numFolds;
  in >> numFeatures;
  in >> numFolds;
  BradleyTerry bt(afset);
  DEBUGASSERT(numFeatures == afset.fset->numFeatures);
  DEBUGASSERT(numFolds == afset.numParallelFolds);

  string line;
  while(!in.fail())
  {
    getline(in,line);
    if(in.fail())
      break;

    if(line.find('#') != string::npos)
      line = line.substr(0,line.find_first_of('#'));

    if(Global::trim(line).length() == 0)
      continue;

    istringstream iss(line);
    string name;
    string buf;

    iss >> name;
    int i = afset.fset->getFeature(name);
    if(i == -1)
    {
      cout << "BradleyTerry::inputFromIStream: Feature " << name << " not found" << endl;
      continue;
    }

    iss >> buf;
    int pf = Global::stringToInt(buf);
    DEBUGASSERT(pf >= 0 && pf < numFolds);
    iss >> buf;
    bt.logGamma[pf][i] = Global::stringToDouble(buf);
  }

  for(int pf = 0; pf<numFolds; pf++)
    for(int i = 0; i<bt.numFeatures; i++)
      bt.gamma[pf][i] = exp(bt.logGamma[pf][i]);

  return bt;
}

static void writeDoublesFile(const string& file, const vector<vector<double>>& values)
{
  ofstream out;
  out.open(file.c_str(),ios::out);
  out.precision(20);

  int size = values.size();
  DEBUGASSERT(size > 0);
  int subSize = values[0].size();
  out << size << endl;
  out << subSize << endl;

  for(int i = 0; i<size; i++)
  {
    DEBUGASSERT((int)values[i].size() == subSize);
    for(int j = 0; j<subSize; j++)
      out << values[i][j] << endl;
  }
  out.close();
}

static void writeIntsFile(const string& file, const vector<vector<int>>& values)
{
  ofstream out;
  out.open(file.c_str(),ios::out);
  out.precision(20);

  int size = values.size();
  DEBUGASSERT(size > 0);
  int subSize = values[0].size();
  out << size << endl;
  out << subSize << endl;

  for(int i = 0; i<size; i++)
  {
    DEBUGASSERT((int)values[i].size() == subSize);
    for(int j = 0; j<subSize; j++)
      out << values[i][j] << endl;
  }
  out.close();
}

static vector<vector<double>> readDoublesFile(const string& file)
{
  ifstream in;
  in.open(file.c_str(),ios::in);
  if(in.fail())
    Global::fatalError(string("BradleyTerry::readDoublesFile: Failed to input from ") + string(file));

  int length;
  in >> length;
  int subLength;
  in >> subLength;

  vector<double> values;
  string line;
  while(!in.fail())
  {
    getline(in,line);
    if(in.fail())
      break;

    if(line.find('#') != string::npos)
      line = line.substr(0,line.find_first_of('#'));

    if(Global::trim(line).length() == 0)
      continue;

    values.push_back(Global::stringToDouble(line));
  }

  if((int)values.size() != length * subLength)
    Global::fatalError("BradleyTerry::readDoublesFile: Incorrect number of doubles in file");

  vector<vector<double>> returnValues;
  returnValues.resize(length);
  for(int i = 0; i<length; i++)
  {
    returnValues[i].resize(subLength);
    for(int j = 0; j<subLength; j++)
      returnValues[i][j] = values[i*subLength+j];
  }

  return returnValues;
}

static vector<vector<int>> readIntsFile(const string& file)
{
  ifstream in;
  in.open(file.c_str(),ios::in);
  if(in.fail())
    Global::fatalError(string("BradleyTerry::readIntsFile: Failed to input from ") + string(file));

  int length;
  in >> length;
  int subLength;
  in >> subLength;

  vector<int> values;
  string line;
  while(!in.fail())
  {
    getline(in,line);
    if(in.fail())
      break;

    if(line.find('#') != string::npos)
      line = line.substr(0,line.find_first_of('#'));

    if(Global::trim(line).length() == 0)
      continue;

    values.push_back(Global::stringToInt(line));
  }

  if((int)values.size() != length * subLength)
    Global::fatalError("BradleyTerry::readIntsFile: Incorrect number of ints in file");

  vector<vector<int>> returnValues;
  returnValues.resize(length);
  for(int i = 0; i<length; i++)
  {
    returnValues[i].resize(subLength);
    for(int j = 0; j<subLength; j++)
      returnValues[i][j] = values[i*subLength+j];
  }

  return returnValues;
}
//---------------------------------------------------------------------------------------

/*
LeastSquares::LeastSquares(const FeatureSet& fset, int numIterations, findex_t priorIndex)
:fset(fset), numIterations(numIterations), priorIndex(priorIndex)
{
  numFeatures = fset.numFeatures;
  gamma.resize(numFeatures);
  logGamma.resize(numFeatures);

  for(int i = 0; i<numFeatures; i++)
    gamma[i] = 1;

  computeLogGammas();
}

LeastSquares::~LeastSquares()
{

}

void LeastSquares::train(MatchIterator& iter)
{
  cout << "Training Least-Squares" << endl;

  //Extract all the data into memory
  vector<int> winners; //For each match, which team won?
  vector<unsigned long long> numWins; //How many wins does each feature have in total?
  vector<vector<vector<pair<findex_t,double> > > > matches; //All matches
  vector<bool> unused; //For each feature, which ones are unused?
  vector<unsigned long long> numOccurrences; //How many times does each feature occur?
  extractAllEvalFromIter(iter,matches,winners);

  //Training initialization
  int numFeatures = gamma.size();
  numOccurrences.resize(numFeatures);
  numWins.resize(numFeatures);
  unused.resize(numFeatures);
  for(int i = 0; i<numFeatures; i++)
  {
    numWins[i] = 0;
    unused[i] = false;
  }

  //Look for unused features
  int numMatches = matches.size();
  for(int j = 0; j<numMatches; j++)
  {
    const vector<vector<findex_t> >& match = matches[j];
    int numTeams = match.size();
    for(int t = 0; t<numTeams; t++)
    {
      const vector<findex_t>& team = match[t];
      int numMems = team.size();
      for(int m = 0; m<numMems; m++)
        numOccurrences[team[m]]++;
    }
  }
  for(int i = 0; i<numFeatures; i++)
  {
    if(numOccurrences[i] <= 0)
      unused[i] = true;
  }

  //Add a prior if one was requested
  if(priorIndex >= 0 && priorIndex < numFeatures)
  {
    for(int i = 0; i<numFeatures; i++)
    {
      if(priorIndex == i)
        continue;

      vector<vector<findex_t> > teams;
      vector<findex_t> featurePrior(1,priorIndex);
      teams.push_back(featurePrior);
      vector<findex_t> featurei(1,i);
      teams.push_back(featurei);

      matches.push_back(teams);
      winners.push_back(0);

      matches.push_back(teams);
      winners.push_back(1);
    }
  }
  numMatches = matches.size();

  //Count wins and look for unused features
  cout << "Counting wins" << endl;
  int numTeamsTotal = 0;
  for(int j = 0; j<numMatches; j++)
  {
    const vector<vector<findex_t> >& match = matches[j];
    int numTeams = match.size();
    numTeamsTotal += numTeams;
    for(int t = 0; t<numTeams; t++)
    {
      const vector<findex_t>& team = match[t];
      int numMems = team.size();
      for(int m = 0; m<numMems; m++)
        if(t == winners[j])
          numWins[team[m]]++;
    }
  }

  //Compute strengths
  cout << "Precomputing match and team strengths" << endl;
  cout << "Building feature -> match map" << endl;
  int* matchSizes = new int[numMatches];             //Maps [match] -> number of teams in match
  double** matchStrengths = new double*[numMatches]; //Maps [match][team] -> strength of team
  vector<vector<pair<int,int> > > featureTeams;      //Maps [feature] -> list of pairs of (match,team) containing the feature
  featureTeams.resize(numFeatures);

  //For each match
  for(int i = 0; i<numMatches; i++)
  {
    vector<vector<findex_t> >& match = matches[i];
    int numTeams = match.size();
    matchSizes[i] = numTeams;
    double* teamStrengths = new double[numTeams];

    //For each team in the match
    for(int t = 0; t<numTeams; t++)
    {
      const vector<findex_t>& team = match[t];

      //Find the team strength - product of members
      //And also record for each feature the (match,team) for that feature
      double teamStr = 1;
      int numMems = team.size();
      for(int m = 0; m<numMems; m++)
      {
        if(team[m] < 0 || team[m] >= numFeatures)
          cout << "ERROR, bad feature " << team[m] << endl;
        teamStr *= gamma[team[m]];
        featureTeams[team[m]].push_back(make_pair(i,t));
      }
      teamStrengths[t] = teamStr;
    }

    matchStrengths[i] = teamStrengths;

    //Delete as we go to save memory
    match.clear();
  }

  //Destruct the inner vectors, since we don't need them any more
  matches.clear();

  //Perform main iteration
  for(int iters = 0; iters < numIterations; iters++)
  {
    cout << "LeastSquares training iteration: " << iters << endl;
    //For each feature, update its weight
    for(int i = 0; i<numFeatures; i++)
    {
      if(unused[i])
        continue;
      if(i == priorIndex)
        continue;

      if(gamma[i] <= 0)
        cout << "BT ERROR! Gamma <= 0... " << i << endl;

      //For each match
      int featureTeamsIdx = 0;
      double baseExpectedWins = 0;
      for(int j = 0; j<numMatches; j++)
      {
        double matchStr = 0;  //Overall total strength in this match
        double matchAllyStr = 0;   //Overall strength of allies

        //For each team in the match
        int numTeams = matchSizes[j];
        for(int t = 0; t<numTeams; t++)
        {
          double teamStr = matchStrengths[j][t];
          matchStr += teamStr;

          pair<int,int> nextTeamWithFeature = featureTeams[i][featureTeamsIdx];
          if(nextTeamWithFeature.first == j && nextTeamWithFeature.second == t)
          {
            featureTeamsIdx++;

            //Find the combined strength of the other members
            double teamAllyStr = teamStr/gamma[i];
            matchAllyStr += teamAllyStr;
          }
        }
        baseExpectedWins += matchAllyStr/matchStr;
      }

      //Update!
      double oldGamma = gamma[i];
      double newGamma = numWins[i]/baseExpectedWins;
      gamma[i] = newGamma;

      //Update team strengths
      double ratio = newGamma/oldGamma;
      int featureTeamsSize = featureTeams[i].size();
      for(int a = 0; a<featureTeamsSize; a++)
      {
        pair<int,int> nextTeamWithFeature = featureTeams[i][a];
        matchStrengths[nextTeamWithFeature.first][nextTeamWithFeature.second] *= ratio;
      }

      cout << i << " " << fset.featureName[i] << " " << gamma[i] << " from " << oldGamma << endl;
    }
  }

  for(int i = 0; i<numMatches; i++)
    delete[] matchStrengths[i];
  delete[] matchStrengths;

  computeLogGammas();

  cout << "Total Teams: " << numTeamsTotal << endl;
  cout << "Total Matches: " << numMatches << endl;
  cout << "Average teams per match: " << (double)numTeamsTotal/numMatches << endl;
  cout << "Done!" << endl;

}


void LeastSquares::computeLogGammas()
{
  for(int i = 0; i<numFeatures; i++)
  {
    double d = gamma[i];
    if(d <= 0)
      cout << "BradleyTerry ERROR - Gamma <= 0!" << fset.featureName[i] << " " << d << endl;
    logGamma[i] = log(d);
  }
}

double LeastSquares::evaluate(const vector<findex_t>& team)
{
  double str = 1.0;
  for(int i = 0; i<(int)team.size(); i++)
    str *= gamma[team[i]];
  return str;
}

double LeastSquares::evaluateLog(const vector<findex_t>& team)
{
  double str = 0.0;
  for(int i = 0; i<(int)team.size(); i++)
    str += logGamma[team[i]];
  return str;
}

void LeastSquares::outputToFile(const char* file)
{
  ofstream out;
  out.open(file,ios::out);
  out.precision(14);
  out << numFeatures << endl;
  out << numIterations << endl;
  out << priorIndex << endl;

  for(int i = 0; i<numFeatures; i++)
    out << fset.featureName[i] << " " << gamma[i] << endl;
  out.close();
}

LeastSquares LeastSquares::inputFromFile(const FeatureSet& fset, const char* file)
{
  ifstream in;
  in.open(file,ios::in);
  if(in.fail())
  {
    cout << "Failed to input from " << file << endl;
    return LeastSquares(fset,0,-1);
  }
  int numFeatures;
  int numIterations;
  findex_t priorIndex;
  in >> numFeatures;
  in >> numIterations;
  in >> priorIndex;
  LeastSquares bt(fset,numIterations,priorIndex);

  double junk;
  string name;
  while(!in.fail())
  {
    in >> name;
    int i = fset.getFeature(name);
    if(i != -1)
    {
        in >> bt.gamma[i];
    }
    else
    {
      cout << "Feature " << name << " not found" << endl;
      in >> junk;
    }
  }
  in.close();

  bt.computeLogGammas();

  return bt;
}


*/


//---------------------------------------------------------------------------------------

/*
SVMLearner::SVMLearner(double c)
{
  svmparams.svm_type = C_SVC;
  svmparams.kernel_type = LINEAR;
  svmparams.cache_size = 200;
  svmparams.eps = 0.001;
  svmparams.C = c;
  svmparams.nr_weight = 0;
  svmparams.weight_label = NULL;
  svmparams.weight = NULL;
  svmparams.shrinking = 1;
  svmparams.probability = 0;

  svmparams.degree = 3;
  svmparams.gamma = 0;
  svmparams.coef0 = 0;
  svmparams.nu = 0.5;
  svmparams.p = 0.1;

  svmmodel = NULL;
}

SVMLearner::~SVMLearner()
{
  if(svmmodel != NULL)
    svm_destroy_model(svmmodel);
  for(int i = 0; i<svmprob.l; i++)
    delete[] svmprob.x[i];
  delete[] svmprob.x;
  delete[] svmprob.y;
}

static struct svm_node* convertTeamToSVMNodes(const vector<findex_t>& t)
{
  vector<findex_t> team = t;
  std::sort(team.begin(),team.end());
  int numMembers = team.size();
  struct svm_node* nodes = new struct svm_node[numMembers+1];
  for(int f = 0; f<numMembers; f++)
  {
    nodes[f].index = team[f];
    nodes[f].value = 1.0;
  }
  nodes[numMembers].index = -1;
  nodes[numMembers].value = 0.0;
  return nodes;
}

void SVMLearner::train(MatchIterator& iter)
{
  cout << "Training SVM" << endl;

  //Extract all the data into memory
  vector<int> winners;
  vector<vector<vector<findex_t> > > matches;
  extractAllFromIter(iter,matches,winners);

  //Count instances of the problem
  int numInstances = 0;
  int numMatches = matches.size();
  for(int m = 0; m<numMatches; m++)
    numInstances += matches[m].size();

  //Initialize svm problem
  svmprob.l = numInstances;
  svmprob.x = new struct svm_node*[numInstances];
  svmprob.y = new double[numInstances];

  //Convert data to libsvm format
  int i = 0;
  int totalNumTeams = 0;
  for(int m = 0; m<numMatches; m++)
  {
    vector<vector<findex_t> >& match = matches[m];
    int numTeams = match.size();
    for(int t = 0; t<numTeams; t++)
    {
      totalNumTeams += numTeams;
      vector<findex_t>& team = match[t];
      svmprob.x[i] = convertTeamToSVMNodes(team);
      svmprob.y[i] = (t == winners[m]) ? 1.0 : 0.0;
      i++;
    }
    match.clear();
  }
  winners.clear();
  matches.clear();

  if(i != svmprob.l)
    cout << "ERROR: i != svmprob.l" << endl;

  //Scale weight penalties
  svmparams.nr_weight = 2;
  svmparams.weight_label = new int[2];
  svmparams.weight = new double[2];
  svmparams.weight_label[0] = 0;
  svmparams.weight_label[1] = 1;
  svmparams.weight[0] = (double)numMatches/totalNumTeams;
  svmparams.weight[1] = 1.0;

  const char* err = svm_check_parameter(&svmprob,&svmparams);
  if(err != NULL)
  {
    cout << err << endl;
    return;
  }

  cout << "Data converted. Begin training..." << endl;
  svmmodel = svm_train(&svmprob,&svmparams);
}

double SVMLearner::evaluate(const vector<findex_t>& team)
{
  struct svm_node* nodes = convertTeamToSVMNodes(team);
  double decision_values[1];
  svm_predict_values(svmmodel,nodes,decision_values);
  delete[] nodes;
  return svm_get_label(svmmodel,0) == 1 ? decision_values[0] : -decision_values[0];
}

void SVMLearner::outputToFile(const char* file)
{
  if(svmmodel != NULL)
  {
    int success = svm_save_model(file, svmmodel);
    if(success == -1)
      cout << "Error - could not save model" << endl;
  }
}

SVMLearner SVMLearner::inputFromFile(const char* file)
{
  SVMLearner learner(1.0);
  learner.svmmodel = svm_load_model(file);
  if(svm_check_probability_model(learner.svmmodel) == 0)
    cout << "Error - no probability model" << endl;
  return learner;
}

*/


