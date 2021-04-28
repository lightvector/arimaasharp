
/*
 * featurearimaa.h
 * Author: davidwu
 */

#ifndef FEATUREARIMAA_H
#define FEATUREARIMAA_H

#include <vector>
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/feature.h"

typedef void (*GetFeaturesFunc)(Board& bAfter, const void* data, pla_t, move_t, const BoardHistory&, void (*handleFeature)(findex_t,void*), void*);
typedef void* (*GetPosDataFunc)(Board& b, const BoardHistory& hist, pla_t pla);
typedef void (*FreePosDataFunc)(void*);
typedef void (*GetParallelWeightsFunc)(const void*,vector<double>& buf);
typedef bool (*DoesCoeffMatterForPriorFunc)(findex_t);
typedef double (*GetPriorLogProbFunc)(const vector<vector<double>>& coeffs);

struct ArimaaFeatureSet
{
  const FeatureSet* fset;
  GetFeaturesFunc getFeaturesFunc;
  GetPosDataFunc getPosDataFunc;
  FreePosDataFunc freePosDataFunc;

  int numParallelFolds;
  GetParallelWeightsFunc getParallelWeightsFunc;
  GetPriorLogProbFunc getPriorLogProbFunc;
  DoesCoeffMatterForPriorFunc doesCoeffMatterForPriorFunc;

  ArimaaFeatureSet();
  ArimaaFeatureSet(const FeatureSet* fset,
      GetFeaturesFunc getFeaturesFunc, GetPosDataFunc getPosDataFunc, FreePosDataFunc freePosDataFunc,
      int numParallelFolds, GetParallelWeightsFunc getParallelWeightsFunc, GetPriorLogProbFunc getPriorLogProbFunc,
      DoesCoeffMatterForPriorFunc doesCoeffMatterForPriorFunc);

  void getFeatureWeights(const void* data,
      const vector<vector<double>>& featureWeights, vector<double>& buf) const;

  //The board is expected to be the board AFTER the move is made. Hist is not necessarily updated for the most recent move.
  double getFeatureSum(Board& bAfter, const void* data,
      pla_t pla, move_t move, const BoardHistory& hist, const vector<double>& featureWeights) const;

  //The board is expected to be the board AFTER the move is made. Hist is not necessarily updated for the most recent move.
  vector<findex_t> getFeatures(Board& bAfter, const void* data,
      pla_t pla, move_t move, const BoardHistory& hist) const;

  void* getPosData(Board& b, const BoardHistory& hist, pla_t pla) const;
  void freePosData(void* data) const;

  void getParallelWeights(const void* data, vector<double>& buf) const;
  double getPriorLogProb(const vector<vector<double>>& coeffs) const;
  bool doesCoeffMatterForPrior(findex_t feature) const;
};

namespace ArimaaFeature
{
  void init();

  //PIECE INDEXING---------------------------------------------------------
  //Get an index corresponding to the strength/type of the piece, 0 to 7
  int getPieceIndexApprox(pla_t owner, piece_t piece, const int pStronger[2][NUMTYPES]);
  //Get an index corresponding to the strength/type of the piece, 0 to 11
  int getPieceIndex(const Board& b, pla_t owner, piece_t piece, const int pStronger[2][NUMTYPES]);


  //TRAPSTATE---------------------------------------------------------------
  const int TRAPSTATE_0 = 0;
  const int TRAPSTATE_1 = 1;
  const int TRAPSTATE_1E = 2;
  const int TRAPSTATE_2 = 3;
  const int TRAPSTATE_2E = 4;
  const int TRAPSTATE_3 = 5;
  const int TRAPSTATE_3E = 6;
  const int TRAPSTATE_4 = 7;
  const int TRAPSTATE_4E = 8; //Only returned by getTrapStateExtended

  int getTrapState(const Board& b, pla_t pla, loc_t kt); // 0-7
  int getTrapStateExtended(const Board& b, pla_t pla, loc_t kt); //0-8

  //PHALANXES---------------------------------------------------------------
  //Is there a phalanx by pla against oloc at the adjacent location adj, using a single piece?
  bool isSinglePhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj);
  //Is there a phalanx by pla against oloc at the adjacent location adj, using a group of pieces?
  bool isMultiPhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj);

  bool isSimpleSinglePhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj);
  bool isSimpleMultiPhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj);

  //BOARD PROPERTIES--------------------------------------------------------

  int lookupBucket(double d, const vector<double>& buckets);

  double turnNumberProperty(const Board& b);
  int turnNumberBucket(const Board& b);
  const vector<double>& turnNumberBuckets();

  double pieceCountProperty(const Board& b);
  int pieceCountBucket(const Board& b);
  const vector<double>& pieceCountBuckets();

  double advancementProperty(const Board& b);
  int advancementBucket(const Board& b);
  const vector<double>& advancementBuckets();

  double advancementNoCapsProperty(const Board& b);
  int advancementNoCapsBucket(const Board& b);
  const vector<double>& advancementNoCapsBuckets();

  double separatenessProperty(const Board& b);
  int separatenessBucket(const Board& b);
  const vector<double>& separatenessBuckets();
}

#endif
