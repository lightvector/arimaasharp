/*
 * evalparams.cpp
 * Author: davidwu
 */
#include <fstream>
#include <sstream>
#include <cmath>
#include "../core/global.h"
#include "../eval/evalparams.h"
#include "../learning/feature.h"

using namespace std;

namespace {
struct BasisVector
{
  vector<double> vec;
  bool multiplicative;
};

struct PriorRelation
{
  static const int EQ = 0;
  static const int GEQ = 1;
  static const int LEQ = 2;
  int type;
  findex_t f1;
  findex_t f2;
  double stdev;
};
}

FeatureSet EvalParams::fset;
int EvalParams::numBasisVectors;

static vector<double> priorMean;
static vector<double> priorStdev;
static vector<BasisVector> basisVectors;
static vector<string> basisVectorNames;

static vector<PriorRelation> priorRelations;

static void addIndividualBasis(fgrpindex_t group, double value, bool multiplicative)
{
  BasisVector bv;
  bv.multiplicative = multiplicative;
  bv.vec.resize(EvalParams::fset.numFeatures,multiplicative ? 1.0 : 0.0);
  int f = EvalParams::fset.get(group);
  bv.vec[f] = value;
  basisVectors.push_back(bv);
  basisVectorNames.push_back(EvalParams::fset.names[f]);
}
static void addIndividualBasis(fgrpindex_t group, int size, double value, bool multiplicative)
{
  for(int i = 0; i < size; i++)
  {
    BasisVector bv;
    bv.multiplicative = multiplicative;
    bv.vec.resize(EvalParams::fset.numFeatures,multiplicative ? 1.0 : 0.0);
    int f = EvalParams::fset.get(group,i);
    bv.vec[f] = value;
    basisVectors.push_back(bv);
    basisVectorNames.push_back(EvalParams::fset.names[f]);
  }
}
static void addLeftBasis(fgrpindex_t group, int size, double value, bool multiplicative)
{
  for(int max = 1; max <= size; max++)
  {
    BasisVector bv;
    bv.multiplicative = multiplicative;
    bv.vec.resize(EvalParams::fset.numFeatures,multiplicative ? 1.0 : 0.0);
    for(int i = 0; i<max; i++)
    {
      int f = EvalParams::fset.get(group,i);
      bv.vec[f] = value;
    }
    basisVectors.push_back(bv);
    basisVectorNames.push_back(EvalParams::fset.names[EvalParams::fset.get(group,max-1)] + "_LEFT");
  }
}
static void addRightBasis(fgrpindex_t group, int size, double value, bool multiplicative)
{
  for(int min = 0; min < size; min++)
  {
    BasisVector bv;
    bv.multiplicative = multiplicative;
    bv.vec.resize(EvalParams::fset.numFeatures,multiplicative ? 1.0 : 0.0);
    for(int i = size-1; i>=min; i--)
    {
      int f = EvalParams::fset.get(group,i);
      bv.vec[f] = value;
    }
    basisVectors.push_back(bv);
    basisVectorNames.push_back(EvalParams::fset.names[EvalParams::fset.get(group,min)] + "_RIGHT");
  }
}

static void addRelations(fgrpindex_t group, int size, double stdev, int type)
{
  for(int i = 0; i < size - 1; i++)
  {
    PriorRelation rel;
    rel.f1 = EvalParams::fset.get(group,i);
    rel.f2 = EvalParams::fset.get(group,i+1);
    rel.stdev = stdev;
    rel.type = type;
    priorRelations.push_back(rel);
  }
}

fgrpindex_t EvalParams::PIECE_SQUARE_SCALE;
fgrpindex_t EvalParams::TRAPDEF_SCORE_SCALE;
fgrpindex_t EvalParams::TC_SCORE_SCALE;
fgrpindex_t EvalParams::THREAT_SCORE_SCALE;
fgrpindex_t EvalParams::STRAT_SCORE_SCALE;
fgrpindex_t EvalParams::CAP_SCORE_SCALE;
fgrpindex_t EvalParams::RABBIT_SCORE_SCALE;

fgrpindex_t EvalParams::RAB_YDIST_TC_VALUES;
fgrpindex_t EvalParams::RAB_XPOS_TC;
fgrpindex_t EvalParams::RAB_TC;

fgrpindex_t EvalParams::RAB_YDIST_VALUES;
fgrpindex_t EvalParams::RAB_XPOS;
fgrpindex_t EvalParams::RAB_FROZEN_UFDIST;
fgrpindex_t EvalParams::RAB_BLOCKER;
fgrpindex_t EvalParams::RAB_SFPGOAL;
fgrpindex_t EvalParams::RAB_INFLFRONT;

void EvalParams::init()
{
  PIECE_SQUARE_SCALE = fset.add("PIECE_SQUARE_SCALE");
  TRAPDEF_SCORE_SCALE = fset.add("TRAPDEF_SCORE_SCALE");
  TC_SCORE_SCALE = fset.add("TC_SCORE_SCALE");
  THREAT_SCORE_SCALE = fset.add("THREAT_SCORE_SCALE");
  STRAT_SCORE_SCALE = fset.add("STRAT_SCORE_SCALE");
  CAP_SCORE_SCALE = fset.add("CAP_SCORE_SCALE");
  RABBIT_SCORE_SCALE = fset.add("RABBIT_SCORE_SCALE");

  RAB_YDIST_TC_VALUES = fset.add("RAB_YDIST_TC_VALUES",8);
  RAB_XPOS_TC = fset.add("RAB_XPOS_TC",4);
  RAB_TC = fset.add("RAB_TC",61);

  RAB_YDIST_VALUES = fset.add("RAB_YDIST_VALUES",8);
  RAB_XPOS = fset.add("RAB_XPOS",4);
  RAB_FROZEN_UFDIST = fset.add("RAB_FROZEN_UFDIST",6);
  RAB_BLOCKER = fset.add("RAB_BLOCKER",61);
  RAB_SFPGOAL = fset.add("RAB_SFPGOAL",41);
  RAB_INFLFRONT = fset.add("RAB_INFLFRONT",41);

  priorMean.resize(fset.numFeatures);
  priorStdev.resize(fset.numFeatures);

  int g;
  int f;
  g = PIECE_SQUARE_SCALE;  f = fset.get(g); priorMean[f] = 1; priorStdev[f] = 0.5; addIndividualBasis(g, 0.2, false);
  g = TRAPDEF_SCORE_SCALE; f = fset.get(g); priorMean[f] = 0.95; priorStdev[f] = 0.5; addIndividualBasis(g, 0.2, false);
  g = TC_SCORE_SCALE;      f = fset.get(g); priorMean[f] = 0.8; priorStdev[f] = 0.5; addIndividualBasis(g, 0.2, false);
  g = THREAT_SCORE_SCALE;  f = fset.get(g); priorMean[f] = 0.89; priorStdev[f] = 0.5; addIndividualBasis(g, 0.2, false);
  g = STRAT_SCORE_SCALE;   f = fset.get(g); priorMean[f] = 0.91; priorStdev[f] = 0.5; addIndividualBasis(g, 0.2, false);
  g = CAP_SCORE_SCALE;     f = fset.get(g); priorMean[f] = 0.6; priorStdev[f] = 0.5; addIndividualBasis(g, 0.2, false);
  g = RABBIT_SCORE_SCALE;  f = fset.get(g); priorMean[f] = 0; priorStdev[f] = 1.0*2; addIndividualBasis(g, 0.2, false);

  {
    int sz = 8;
    double means[8] =  {2000,790,490,380,270,160,85,20};
    double stdevs[8] = {800,700,600,500,400,350,270,250};
    double basisValue = 100;
    g = RAB_YDIST_TC_VALUES;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdevs[i]*2;}
    addLeftBasis(g,sz,basisValue,false);
    addRelations(g,sz,100,PriorRelation::GEQ);
  }
  {
    int sz = 4;
    double means[4] = {0.97398, 0.985342, 0.887231, 1.02873};
    double stdevs[4] = {0.3,0.3,0.3,0.3};
    double basisValue = 1.1;
    g = RAB_XPOS_TC;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdevs[i]*2;}
    addIndividualBasis(g,sz,basisValue,true);
  }
  {
    int sz = 61;
    double basisValue = 1.1;
    double means[61] =
    {0.005, 0.01, 0.02, 0.03, 0.04, 0.05, 0.07, 0.08, 0.094, 0.112,
     0.131, 0.149, 0.168, 0.186, 0.194, 0.213, 0.231, 0.250, 0.265,
     0.283, 0.301, 0.319, 0.337, 0.355, 0.370, 0.385, 0.398, 0.411,
     0.424, 0.439, 0.451, 0.462, 0.476, 0.490, 0.506, 0.525, 0.545,
     0.568, 0.591, 0.615, 0.639, 0.652, 0.685, 0.708, 0.730, 0.752,
     0.774, 0.795, 0.815, 0.835, 0.853, 0.871, 0.889, 0.904, 0.922,
     0.940, 0.956, 0.974, 0.992, 1.010, 1.030
    };
    g = RAB_TC;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = 0.5*2;}
    addRelations(g,sz,0.09,PriorRelation::LEQ);
    addRightBasis(g,sz,basisValue,true);
  }
  {
    int sz = 8;
    double means[8] = {3000,2200,1770,1400,1040,690,330,105};
    double stdevs[8] = {800, 700, 600, 500, 400,350,300,250};
    double basisValue = 100;
    g = RAB_YDIST_VALUES;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdevs[i]*2;}
    addLeftBasis(g,sz,basisValue,false);
    addRelations(g,sz,200,PriorRelation::GEQ);
  }
  {
    int sz = 4;
    double means[4] = {1.04329, 0.986563, 0.920916, 1.04774};
    double stdevs[4] = {0.3,0.3,0.3,0.3};
    double basisValue = 1.1;
    g = RAB_XPOS;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdevs[i]*2;}
    addIndividualBasis(g,sz,basisValue,true);
  }
  {
    int sz = 6;
    double means[6] = {1.05, 0.935, 0.920, 0.970, 0.970, 0.860};
    double stdevs[6] = {0.5,0.5,0.5,0.5,0.5,0.5};
    double basisValue = 1.1;
    g = RAB_FROZEN_UFDIST;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdevs[i]*2;}
    addIndividualBasis(g,sz,basisValue,true);
  }
  {
    int sz = 61;
    double means[61] = {
        1.355, 1.348, 1.341, 1.334, 1.327, 1.320, 1.313, 1.306, 1.299, 1.292,
        1.285, 1.278, 1.270, 1.262, 1.254, 1.245, 1.236, 1.228, 1.217, 1.185,
        1.125, 1.041, 0.981, 0.826, 0.750, 0.673, 0.540, 0.427, 0.301, 0.225,
        0.195, 0.178, 0.167, 0.162, 0.158, 0.154, 0.150, 0.146, 0.143, 0.139,
        0.135, 0.132, 0.128, 0.125, 0.122, 0.120, 0.119, 0.117, 0.116, 0.115,
        0.114, 0.113, 0.112, 0.111, 0.110, 0.110, 0.109, 0.108, 0.107, 0.107,
        0.106
    };

    double stdev = 0.75;
    double basisValue = 1.1;
    g = RAB_BLOCKER;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdev*2;}
    addLeftBasis(g,sz,basisValue,true);
    addRelations(g,sz,0.09,PriorRelation::GEQ);
  }
  {
    int sz = 41;
    double means[41] = {
        0.370, 0.375, 0.381, 0.388, 0.397, 0.408, 0.421, 0.433, 0.446, 0.460,
        0.472, 0.484, 0.494, 0.504, 0.515, 0.526, 0.538, 0.551, 0.564, 0.579,
        0.595, 0.612, 0.632, 0.655, 0.680, 0.705, 0.733, 0.759, 0.787, 0.815,
        0.842, 0.867, 0.890, 0.910, 0.925, 0.940, 0.955, 0.970, 0.980, 0.990,
        1.000
    };
    double stdev = 0.6;
    double basisValue = 1.1;
    g = RAB_SFPGOAL;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdev*2;}
    addRightBasis(g,sz,basisValue,true);
    addRelations(g,sz,0.10,PriorRelation::LEQ);
  }
  {
    int sz = 41;
    double means[41] = {
        0.60, 0.60, 0.60, 0.60, 0.60, 0.60, 0.60, 0.61, 0.62, 0.64,
        0.65, 0.67, 0.69, 0.71, 0.73, 0.76, 0.80, 0.83, 0.86, 0.89,
        0.91, 0.94, 0.97, 1.00, 1.02, 1.04, 1.06, 1.08, 1.10, 1.11,
        1.13, 1.14, 1.16, 1.17, 1.18, 1.19, 1.20, 1.21, 1.22, 1.23,
        1.24
    };
    double stdev = 0.6;
    double basisValue = 1.1;
    g = RAB_INFLFRONT;
    for(int i = 0; i<sz; i++) {f = fset.get(g,i); priorMean[f] = means[i]; priorStdev[f] = stdev*2;}
    addRightBasis(g,sz,basisValue,true);
    addRelations(g,sz,0.10,PriorRelation::LEQ);
  }

  numBasisVectors = basisVectors.size();

  for(int i = 0; i<(int)priorStdev.size(); i++)
    DEBUGASSERT(priorStdev[i] > 0);
  for(int i = 0; i<(int)priorMean.size(); i++)
    DEBUGASSERT(priorMean[i] == priorMean[i]); //Check for nan
}

EvalParams::EvalParams()
:featureWeights(priorMean)
{
}

EvalParams::~EvalParams()
{
}

EvalParams EvalParams::inputFromFile(const string& file)
{
  return inputFromFile(file.c_str());
}

EvalParams EvalParams::inputFromFile(const char* file)
{
  ifstream in;
  in.open(file,ios::in);
  if(in.fail())
    Global::fatalError(string("EvalParams::inputFromFile: Failed to input from ") + string(file));

  EvalParams params;

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
    int i = fset.getFeature(name);
    if(i == -1)
      Global::fatalError(string("EvalParams::inputFromFile: Feature ") + name + " not found");

    iss >> buf;
    params.featureWeights[i] = Global::stringToDouble(buf);
  }
  in.close();
  return params;
}


string EvalParams::getBasisName(int basisVector)
{
  return basisVectorNames[basisVector];
}

void EvalParams::addBasis(int basisVector, double strength)
{
  const BasisVector& bv = basisVectors[basisVector];
  if(bv.multiplicative)
  {
    for(int i = 0; i<fset.numFeatures; i++)
      featureWeights[i] *= pow(bv.vec[i],strength);
  }
  else
  {
    for(int i = 0; i<fset.numFeatures; i++)
      featureWeights[i] += bv.vec[i] * strength;
  }
}

double EvalParams::getPriorError() const
{
  double error = 0;

  for(int i = 0; i<fset.numFeatures; i++)
  {
    double diff = (featureWeights[i] - priorMean[i]) / priorStdev[i];
    error += diff * diff;
  }

  for(int i = 0; i<(int)priorRelations.size(); i++)
  {
    PriorRelation& rel = priorRelations[i];
    double diff = (featureWeights[rel.f1] - featureWeights[rel.f2]) / rel.stdev;
    if(rel.type == PriorRelation::GEQ)
    {
      if(diff < 0)
        error += diff * diff;
    }
    else if(rel.type == PriorRelation::LEQ)
    {
      if(diff > 0)
        error += diff * diff;
    }
    else
      error += diff * diff;
  }

  return error;
}

double EvalParams::get(fgrpindex_t group) const
{
  return featureWeights[fset.get(group)];
}

double EvalParams::get(fgrpindex_t group, int x) const
{
  return featureWeights[fset.get(group, x)];
}

ostream& operator<<(ostream& out, const EvalParams& params)
{
  for(int i = 0; i<EvalParams::fset.numFeatures; i++)
    out << EvalParams::fset.names[i] << " " << params.featureWeights[i] << "\n";
  return out;
}
