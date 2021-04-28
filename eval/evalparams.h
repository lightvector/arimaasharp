/*
 * evalparams.h
 * Author: davidwu
 */

#ifndef EVALPARAMS_H_
#define EVALPARAMS_H_

#include "../core/global.h"
#include "../learning/feature.h"

class EvalParams
{
  public:
  static FeatureSet fset;
  static int numBasisVectors;

  static fgrpindex_t PIECE_SQUARE_SCALE;
  static fgrpindex_t TRAPDEF_SCORE_SCALE;
  static fgrpindex_t TC_SCORE_SCALE;
  static fgrpindex_t THREAT_SCORE_SCALE;
  static fgrpindex_t STRAT_SCORE_SCALE;
  static fgrpindex_t CAP_SCORE_SCALE;
  static fgrpindex_t RABBIT_SCORE_SCALE;

  static fgrpindex_t RAB_YDIST_TC_VALUES; //[0-7]
  static fgrpindex_t RAB_XPOS_TC;  //[0-3]
  static fgrpindex_t RAB_TC;       //[0-60(+30)]

  static fgrpindex_t RAB_YDIST_VALUES;      //[0-7]
  static fgrpindex_t RAB_XPOS;       //[0-3]
  static fgrpindex_t RAB_FROZEN_UFDIST; //[0-5, isFrozen*3 + min(ufDist,2)]
  static fgrpindex_t RAB_BLOCKER;    //[0-60]
  static fgrpindex_t RAB_SFPGOAL;    //[0-40]
  static fgrpindex_t RAB_INFLFRONT;  //[0-40]

  static void init();

  vector<double> featureWeights;

  EvalParams();
  ~EvalParams();

  static EvalParams inputFromFile(const string& file);
  static EvalParams inputFromFile(const char* file);

  string getBasisName(int basisVector);
  void addBasis(int basisVector, double strength);
  double getPriorError() const;
  double get(fgrpindex_t group) const;
  double get(fgrpindex_t group, int x) const;

  friend ostream& operator<<(ostream& out, const EvalParams& params);
};

#endif /* EVALPARAMS_H_ */
