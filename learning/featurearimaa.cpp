
/*
 * featurearimaa.cpp
 * Author: davidwu
 */

#include <algorithm>
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../eval/threats.h"
#include "../learning/feature.h"
#include "../learning/featurearimaa.h"

ArimaaFeatureSet::ArimaaFeatureSet()
:fset(NULL),getFeaturesFunc(NULL),getPosDataFunc(NULL),freePosDataFunc(NULL),
 numParallelFolds(0),getParallelWeightsFunc(NULL),getPriorLogProbFunc(NULL),
 doesCoeffMatterForPriorFunc(NULL)
{}

ArimaaFeatureSet::ArimaaFeatureSet(const FeatureSet* fset,
    GetFeaturesFunc getFeaturesFunc, GetPosDataFunc getPosDataFunc, FreePosDataFunc freePosDataFunc,
    int numParallelFolds, GetParallelWeightsFunc getParallelWeightsFunc, GetPriorLogProbFunc getPriorLogProbFunc,
    DoesCoeffMatterForPriorFunc doesCoeffMatterForPriorFunc)
:fset(fset),getFeaturesFunc(getFeaturesFunc),getPosDataFunc(getPosDataFunc),freePosDataFunc(freePosDataFunc),
 numParallelFolds(numParallelFolds),getParallelWeightsFunc(getParallelWeightsFunc),getPriorLogProbFunc(getPriorLogProbFunc),
 doesCoeffMatterForPriorFunc(doesCoeffMatterForPriorFunc)
{}

namespace {
struct FeatureAccum
{
  const vector<double>* featureWeights;
  double accum;

  FeatureAccum(const vector<double>& featureWeights)
  :featureWeights(&featureWeights),accum(0)
  {}
};
}
static void addFeatureValue(findex_t feature, void* a)
{
  FeatureAccum* accum = static_cast<FeatureAccum*>(a);
  accum->accum += (*(accum->featureWeights))[feature];
};

void ArimaaFeatureSet::getFeatureWeights(const void* data, const vector<vector<double>>& featureWeights, vector<double>& buf) const
{
  buf.resize(fset->numFeatures);
  DEBUGASSERT((int)featureWeights.size() == numParallelFolds);
  for(int pf = 0; pf<numParallelFolds; pf++)
    DEBUGASSERT((int)featureWeights[pf].size() == fset->numFeatures);
  if((int)buf.size() != fset->numFeatures)
    buf.resize(fset->numFeatures);

  for(int i = 0; i<fset->numFeatures; i++)
    buf[i] = 0;

  vector<double> parallelWeights;
  getParallelWeights(data,parallelWeights);
  for(int pf = 0; pf<numParallelFolds; pf++)
  {
    double weight = parallelWeights[pf];
    for(int i = 0; i<fset->numFeatures; i++)
      buf[i] += featureWeights[pf][i] * weight;
  }
}

double ArimaaFeatureSet::getFeatureSum(Board& bAfter, const void* data,
    pla_t pla, move_t move, const BoardHistory& hist, const vector<double>& featureWeights) const
{
  DEBUGASSERT((int)featureWeights.size() == fset->numFeatures);
  FeatureAccum accum(featureWeights);
  getFeaturesFunc(bAfter,data,pla,move,hist,&addFeatureValue,&accum);
  return accum.accum;
}

static void addFeatureToVector(findex_t x, void* v)
{
  vector<findex_t>* vec = (vector<findex_t>*)v;
  vec->push_back(x);
}

vector<findex_t> ArimaaFeatureSet::getFeatures(Board& bAfter, const void* data,
    pla_t pla, move_t move, const BoardHistory& hist) const
{
  vector<findex_t> featuresVec;
  getFeaturesFunc(bAfter,data,pla,move,hist,&addFeatureToVector,&featuresVec);
  return featuresVec;
}


void* ArimaaFeatureSet::getPosData(Board& b, const BoardHistory& hist, pla_t pla) const
{
  return getPosDataFunc(b,hist,pla);
}

void ArimaaFeatureSet::freePosData(void* data) const
{
  return freePosDataFunc(data);
}

void ArimaaFeatureSet::getParallelWeights(const void* data, vector<double>& buf) const
{
  getParallelWeightsFunc(data,buf);
}

double ArimaaFeatureSet::getPriorLogProb(const vector<vector<double>>& coeffs) const
{
  return getPriorLogProbFunc(coeffs);
}

bool ArimaaFeatureSet::doesCoeffMatterForPrior(findex_t feature) const
{
  return doesCoeffMatterForPriorFunc(feature);
}

//ARIMAAFEATURE-----------------------------------------------------------------------------------

static void initBuckets();
void ArimaaFeature::init()
{
  initBuckets();
}



static const int PIECEINDEXTABLE[7][9] = {
{0,0,0,0,0,0,0,0,0},
{7,7,7,7,7,6,6,5,5},
{0,1,2,3,3,4,4,4,4},
{0,1,2,3,3,4,4,4,4},
{0,1,2,3,3,4,4,4,4},
{0,1,2,3,3,4,4,4,4},
{0,1,2,3,3,4,4,4,4},
};

int ArimaaFeature::getPieceIndexApprox(pla_t owner, piece_t piece, const int pStronger[2][NUMTYPES])
{
  int ns = pStronger[owner][piece];
  return PIECEINDEXTABLE[piece][ns];
}

static int RAB_INDEX_TABLE[25] =
{4,4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,2,2,2,1,1,1,0,0,0};
int ArimaaFeature::getPieceIndex(const Board& b, pla_t owner, piece_t piece, const int pStronger[2][NUMTYPES])
{
  if(piece != RAB)
    return pStronger[owner][piece];
  else
    return 7+RAB_INDEX_TABLE[pStronger[owner][RAB]+b.pieceCounts[gOpp(owner)][0]];
}

int ArimaaFeature::getTrapState(const Board& b, pla_t pla, loc_t kt)
{
  loc_t plaEleLoc = b.findElephant(pla);
  int defCount = b.trapGuardCounts[pla][Board::TRAPINDEX[kt]];
  switch(defCount)
  {
  case 0: return TRAPSTATE_0;
  case 1: return (plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt)) ? TRAPSTATE_1E : TRAPSTATE_1;
  case 2: return (plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt)) ? TRAPSTATE_2E : TRAPSTATE_2;
  case 3: return (plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt)) ? TRAPSTATE_3E : TRAPSTATE_3;
  case 4: return TRAPSTATE_4;
  default: DEBUGASSERT(false); return 0;
  }
  return 0;
}

int ArimaaFeature::getTrapStateExtended(const Board& b, pla_t pla, loc_t kt)
{
  loc_t plaEleLoc = b.findElephant(pla);
  int defCount = b.trapGuardCounts[pla][Board::TRAPINDEX[kt]];
  switch(defCount)
  {
  case 0: return TRAPSTATE_0;
  case 1: return (plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt)) ? TRAPSTATE_1E : TRAPSTATE_1;
  case 2: return (plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt)) ? TRAPSTATE_2E : TRAPSTATE_2;
  case 3: return (plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt)) ? TRAPSTATE_3E : TRAPSTATE_3;
  case 4: return (plaEleLoc != ERRLOC && Board::isAdjacent(plaEleLoc,kt)) ? TRAPSTATE_4E : TRAPSTATE_4;
  default: DEBUGASSERT(false); return 0;
  }
  return 0;
}

//MINOR these can probably be optimized slightly using bitmaps
bool ArimaaFeature::isSinglePhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj)
{
  pla_t opp = gOpp(pla);
  if(b.owners[oloc] != opp)
    return false;
  if(b.owners[adj] == pla && b.pieces[adj] >= b.pieces[oloc])
    return true;
  if(b.owners[adj] == opp && !b.isOpen(adj) && !b.isOpenToPush(adj))
    return true;
  return false;
}

bool ArimaaFeature::isMultiPhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj)
{
  pla_t opp = gOpp(pla);
  if(b.owners[oloc] != opp)
    return false;
  if(b.owners[adj] == NPLA || (b.owners[adj] == opp && b.isOpenToPush(adj)))
    return false;
  for(int dir = 0; dir < 4; dir++)
  {
    if(!Board::ADJOKAY[dir][adj])
      continue;
    loc_t adjadj = adj + Board::ADJOFFSETS[dir];
    if(b.owners[adjadj] == NPLA || (b.owners[adjadj] == opp && !b.isFrozen(adjadj) && b.isOpenToStep(adjadj)))
      return false;
  }
  return true;
}

static bool phalanxBlockCheck(const Board& b, loc_t k)
{
  pla_t pla = b.owners[k];
  pla_t opp = gOpp(pla);
  if(b.owners[k S] != opp || b.pieces[k S] < b.pieces[k] || !b.isRabOkayS(pla,k)) {return false;}
  if(b.owners[k W] != opp || b.pieces[k W] < b.pieces[k]) {return false;}
  if(b.owners[k E] != opp || b.pieces[k E] < b.pieces[k]) {return false;}
  if(b.owners[k N] != opp || b.pieces[k N] < b.pieces[k] || !b.isRabOkayN(pla,k)) {return false;}
  return true;
}


bool ArimaaFeature::isSimpleSinglePhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj)
{
  if(b.owners[adj] == pla && b.pieces[adj] >= b.pieces[oloc])
    return true;
  return false;
}

bool ArimaaFeature::isSimpleMultiPhalanx(const Board& b, pla_t pla, loc_t oloc, loc_t adj)
{
  pla_t opp = gOpp(pla);
  if(b.owners[adj] == NPLA)
    return false;
  if(b.owners[adj] == opp)
    return phalanxBlockCheck(b,adj);
  return b.pieces[adj] >= b.pieces[oloc] || !b.isOpen(adj);
}


//PROPERTIES----------------------------------------------------------------------------------

static vector<double> turnNumberBucketsVec;
static vector<double> pieceCountBucketsVec;
static vector<double> advancementBucketsVec;
static vector<double> advancementNoCapsBucketsVec;
static vector<double> separatenessBucketsVec;

static void initBuckets()
{
  /*
  turnNumberBucketsVec = vector<double>();
  for(int i = 0; i<=200; i++)
    turnNumberBucketsVec.push_back(i);

  pieceCountBucketsVec = vector<double>();
  for(int i = 0; i<=32; i++)
    pieceCountBucketsVec.push_back(i);

  advancementBucketsVec = vector<double>();
  for(int i = 0; i<=128; i+=2)
    advancementBucketsVec.push_back(i);

  separatenessBucketsVec = vector<double>();
  for(double i = 0; i<=15; i += 1.0/8.0)
    separatenessBucketsVec.push_back(i);
  */

  turnNumberBucketsVec = vector<double>();
  for(int i = 0; i<100; i+=2)
    turnNumberBucketsVec.push_back(i);
  for(int i = 100; i<120; i+=4)
    turnNumberBucketsVec.push_back(i);
  for(int i = 120; i<180; i+=10)
    turnNumberBucketsVec.push_back(i);
  turnNumberBucketsVec.push_back(180);

  pieceCountBucketsVec = vector<double>();
  pieceCountBucketsVec.push_back(0);
  for(int i = 7; i<=32; i++)
    pieceCountBucketsVec.push_back(i);

  advancementBucketsVec = vector<double>();
  for(int i = 16; i<120; i+=4)
    advancementBucketsVec.push_back(i);
  for(int i = 120; i<200; i+=10)
    advancementBucketsVec.push_back(i);
  advancementBucketsVec.push_back(200);

  advancementNoCapsBucketsVec = vector<double>();
  for(int i = 16; i<100; i+=4)
    advancementNoCapsBucketsVec.push_back(i);
  advancementNoCapsBucketsVec.push_back(100);

  separatenessBucketsVec = vector<double>();
  for(double i = 1; i<=7; i += 0.25)
    separatenessBucketsVec.push_back(i);
}

int ArimaaFeature::lookupBucket(double d, const vector<double>& buckets)
{
  DEBUGASSERT(buckets.size() > 0);
  DEBUGASSERT(d >= buckets[0]);

  auto iterator = std::upper_bound(buckets.begin(),buckets.end(),d);
  return (iterator-buckets.begin())-1;
}


double ArimaaFeature::turnNumberProperty(const Board& b)
{
  return (double)b.turnNumber;
}
int ArimaaFeature::turnNumberBucket(const Board& b)
{
  return lookupBucket(turnNumberProperty(b),turnNumberBucketsVec);
}
const vector<double>& ArimaaFeature::turnNumberBuckets()
{
  return turnNumberBucketsVec;
}

double ArimaaFeature::pieceCountProperty(const Board& b)
{
  return (double)(b.pieceCounts[SILV][0] + b.pieceCounts[GOLD][0]);
}
int ArimaaFeature::pieceCountBucket(const Board& b)
{
  return lookupBucket(pieceCountProperty(b),pieceCountBucketsVec);
}
const vector<double>& ArimaaFeature::pieceCountBuckets()
{
  return pieceCountBucketsVec;
}

double ArimaaFeature::advancementProperty(const Board& b)
{
  int advancement = 0;
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(b.owners[loc] == GOLD) advancement += y;
      else if(b.owners[loc] == SILV) advancement += (7-y);
    }
  }
  int pieceCount = b.pieceCounts[SILV][0] + b.pieceCounts[GOLD][0];
  advancement += (32-pieceCount) * 8;
  return (double)advancement;
}
int ArimaaFeature::advancementBucket(const Board& b)
{
  return lookupBucket(advancementProperty(b),advancementBucketsVec);
}
const vector<double>& ArimaaFeature::advancementBuckets()
{
  return advancementBucketsVec;
}

double ArimaaFeature::advancementNoCapsProperty(const Board& b)
{
  int advancement = 0;
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(b.owners[loc] == GOLD) advancement += y;
      else if(b.owners[loc] == SILV) advancement += (7-y);
    }
  }
  int pieceCount = b.pieceCounts[SILV][0] + b.pieceCounts[GOLD][0];
  advancement += (32-pieceCount) * 1;
  return (double)advancement;
}
int ArimaaFeature::advancementNoCapsBucket(const Board& b)
{
  return lookupBucket(advancementNoCapsProperty(b),advancementNoCapsBucketsVec);
}
const vector<double>& ArimaaFeature::advancementNoCapsBuckets()
{
  return advancementNoCapsBucketsVec;
}


double ArimaaFeature::separatenessProperty(const Board& b)
{
  //Average distance between piece and nearest two opp pieces
  int separationSum = 0;
  int separationCount = 0;
  for(int y = 0; y<8; y++)
  {
    for(int x = 0; x<8; x++)
    {
      loc_t loc = gLoc(x,y);
      if(b.owners[loc] == NPLA)
        continue;
      pla_t opp = gOpp(b.owners[loc]);
      int rad;
      for(rad = 1; rad < 15; rad++)
      {
        if((b.pieceMaps[opp][0] & Board::DISK[rad][loc]).hasBits())
          break;
      }
      separationSum += rad;
      separationCount++;

      for(rad = 1; rad < 15; rad++)
      {
        if(!(b.pieceMaps[opp][0] & Board::DISK[rad][loc]).isEmptyOrSingleBit())
          break;
      }
      separationSum += rad;
      separationCount++;
    }
  }

  //Baseline to avoid division by zero and to increase separateness slightly as piececount goes down
  separationSum += 15;
  separationCount++;

  return (double)separationSum/separationCount;
}
int ArimaaFeature::separatenessBucket(const Board& b)
{
  return lookupBucket(separatenessProperty(b),separatenessBucketsVec);
}
const vector<double>& ArimaaFeature::separatenessBuckets()
{
  return separatenessBucketsVec;
}

