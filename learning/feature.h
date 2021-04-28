
/*
 * feature.h
 * Author: davidwu
 */

#ifndef FEATURE_H
#define FEATURE_H

#include "../core/global.h"

using namespace std;

typedef int fgrpindex_t;
typedef int findex_t;

struct FeatureGroup
{
  string featureName;
  int baseIdx;
  int numDims;
  int size;
  int dimSize[4];

  FeatureGroup();
  FeatureGroup(const string& name, int baseIdx);
  FeatureGroup(const string& name, int baseIdx, int dim0size);
  FeatureGroup(const string& name, int baseIdx, int dim0size, int dim1size);
  FeatureGroup(const string& name, int baseIdx, int dim0size, int dim1size, int dim2size);
  FeatureGroup(const string& name, int baseIdx, int dim0size, int dim1size, int dim2size, int dim3size);
  ~FeatureGroup();

  int get() const;
  int get(int i0) const;
  int get(int i0, int i1) const;
  int get(int i0, int i1, int i2) const;
  int get(int i0, int i1, int i2, int i3) const;
};

class FeatureSet
{
  public:
  int numFeatures;
  vector<FeatureGroup> groups;
  vector<string> names;
  map<string,int> featureOfName;

  FeatureSet();
  ~FeatureSet();

  //Create a feature with the given name, and return its index
  //Optionally, add 1 dimension or 2 dimensions of subfeatures, with the specified names (or default numbered if NULL)
  fgrpindex_t add(const string& name);
  fgrpindex_t add(const string& name, int num0, const string* names0 = NULL);
  fgrpindex_t add(const string& name, int num0, int num1, const string* names0 = NULL, const string* names1 = NULL);
  fgrpindex_t add(const string& name, int num0, int num1, int num2, const string* names0 = NULL, const string* names1 = NULL, const string* names2 = NULL);
  fgrpindex_t add(const string& name, int num0, int num1, int num2, int num3, const string* names0 = NULL, const string* names1 = NULL, const string* names2 = NULL, const string* names3 = NULL);

  fgrpindex_t add(const char* name);
  fgrpindex_t add(const char* name, int num0, const string* names0 = NULL);
  fgrpindex_t add(const char* name, int num0, int num1, const string* names0 = NULL, const string* names1 = NULL);
  fgrpindex_t add(const char* name, int num0, int num1, int num2, const string* names0 = NULL, const string* names1 = NULL, const string* names2 = NULL);
  fgrpindex_t add(const char* name, int num0, int num1, int num2, int num3, const string* names0 = NULL, const string* names1 = NULL, const string* names2 = NULL, const string* names3 = NULL);

  //Get the index of the specified subfeature of this feature
  findex_t get(fgrpindex_t group) const;
  findex_t get(fgrpindex_t group, int i0) const;
  findex_t get(fgrpindex_t group, int i0, int i1) const;
  findex_t get(fgrpindex_t group, int i0, int i1, int i2) const;
  findex_t get(fgrpindex_t group, int i0, int i1, int i2, int i3) const;

  //Get the name of a feature
  string getName(findex_t index) const;

  //Get the feature with a given name or -1 if not found
  findex_t getFeature(string name) const;
};

#endif
