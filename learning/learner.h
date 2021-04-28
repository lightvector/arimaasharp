
/*
 * learner.h
 * Author: davidwu
 */

#ifndef LEARNER_H
#define LEARNER_H

#include "../core/global.h"
#include "../learning/feature.h"
#include "../learning/featurearimaa.h"

class GameIterator;

class NaiveBayes
{
  public:
  ArimaaFeatureSet afset;
  int numFeatures;
  vector<unsigned long long> featureFreq[2];  //[outcome][feature] - how often did this feature occur with this outcome?
  unsigned long long outcomeFreq[2];          //[outcome] - how often did this outcome occur?
  unsigned long long totalFreq;               //Total number of instances trained on
  double priorStrength;

  double logWinOdds;
  double logWinOddsIfNoFeatures;
  vector<double> logLikelihoodIfPresent;
  vector<double> logLikelihoodIfAbsent;

  //priorStrength - treat each feature as if we've seen it priorStrength times
  //and its win/loss ratio was exactly proportional to outcomeFreq
  NaiveBayes(ArimaaFeatureSet afset, int numFeatures, double priorStrength);

  ~NaiveBayes();

  void train(GameIterator& iter);
  double evaluate(const vector<findex_t>& team);
  void outputToFile(const char* file);
  static NaiveBayes inputFromFile(ArimaaFeatureSet afset, const char* file);

  private:
  void computeLogOdds();

};

class BradleyTerry
{
  public:
  ArimaaFeatureSet afset;
  vector<vector<double>> logGamma;
  vector<vector<double>> gamma;
  int numFolds;
  int numFeatures;

  BradleyTerry(ArimaaFeatureSet afset);

  ~BradleyTerry();

  void train(GameIterator& iter, int numIterations);
  void train(GameIterator& iter, int numIterations, const string& partialFilePrefix);
  void train(const vector<vector<vector<findex_t> > >& matches, const vector<int>& winners, int numIterations, const string& partialFilePrefix);

  //For restarting a training interrupted in the middle. Assumes this BT was loaded from a file.
  void resumeTraining(GameIterator& iter, int numPrevIterations, int numIterations,
      const string& initialLastTypeFile, const string& initialDeltaFile, const string& partialFilePrefix);

  double evaluate(const vector<findex_t>& team, vector<double>& matchParallelFactors);
  void outputToFile(const char* file, const char* notes);
  void outputToFile(const GameIterator& trainingIter, const char* file, const char* notes);
  static BradleyTerry inputFromDefault(ArimaaFeatureSet afset);
  static BradleyTerry inputFromFile(ArimaaFeatureSet afset, const char* file);
  static BradleyTerry inputFromIStream(ArimaaFeatureSet afset, istream& in);

  void outputLogsToFile(const char* file);

};

/*
class LeastSquares : public Learner
{
  public:
  const FeatureSet& fset;
  vector<double> logGamma;
  vector<double> gamma;
  int numFeatures;
  int numIterations;
  findex_t priorIndex;

  LeastSquares(const FeatureSet& fset, int numIterations, findex_t priorIndex);

  ~LeastSquares();

  void train(MatchIterator& iter);
  double evaluate(const vector<findex_t>& team);
  double evaluateLog(const vector<findex_t>& team);
  void outputToFile(const char* file);
  static LeastSquares inputFromFile(const FeatureSet& fset, const char* file);

  private:
  void computeLogGammas();
};
*/

/*
class SVMLearner : public Learner
{
  public:
  struct svm_problem svmprob;
  struct svm_parameter svmparams;
  struct svm_model* svmmodel;

  SVMLearner(double c);

  ~SVMLearner();

  void train(MatchIterator& iter);
  double evaluate(const vector<findex_t>& team);
  void outputToFile(const char* file);
  static SVMLearner inputFromFile(const char* file);
};
*/

#endif
