
/*
 * feature.cpp
 * Author: davidwu
 */

#include <sstream>
#include "../core/global.h"
#include "../learning/feature.h"

using namespace std;

FeatureGroup::FeatureGroup()
:featureName()
{
  baseIdx = 0;
  numDims = 0;
  dimSize[0] = 1;
  dimSize[1] = 1;
  dimSize[2] = 1;
  dimSize[3] = 1;
  size = dimSize[0] * dimSize[1] * dimSize[2] * dimSize[3];
}

FeatureGroup::FeatureGroup(const string& name, int base)
:featureName(name)
{
  baseIdx = base;
  numDims = 0;
  dimSize[0] = 1;
  dimSize[1] = 1;
  dimSize[2] = 1;
  dimSize[3] = 1;
  size = dimSize[0] * dimSize[1] * dimSize[2] * dimSize[3];
}

FeatureGroup::FeatureGroup(const string& name, int base, int dim0size)
:featureName(name)
{
  if(dim0size <= 0) Global::fatalError("dimsize <= 0");
  baseIdx = base;
  numDims = 1;
  dimSize[0] = dim0size;
  dimSize[1] = 1;
  dimSize[2] = 1;
  dimSize[3] = 1;
  size = dimSize[0] * dimSize[1] * dimSize[2] * dimSize[3];
}

FeatureGroup::FeatureGroup(const string& name, int base, int dim0size, int dim1size)
:featureName(name)
{
  if(dim0size <= 0) Global::fatalError("dimsize <= 0");
  if(dim1size <= 0) Global::fatalError("dimsize <= 0");
  baseIdx = base;
  numDims = 2;
  dimSize[0] = dim0size;
  dimSize[1] = dim1size;
  dimSize[2] = 1;
  dimSize[3] = 1;
  size = dimSize[0] * dimSize[1] * dimSize[2] * dimSize[3];
}

FeatureGroup::FeatureGroup(const string& name, int base, int dim0size, int dim1size, int dim2size)
:featureName(name)
{
  if(dim0size <= 0) Global::fatalError("dimsize <= 0");
  if(dim1size <= 0) Global::fatalError("dimsize <= 0");
  if(dim2size <= 0) Global::fatalError("dimsize <= 0");
  baseIdx = base;
  numDims = 3;
  dimSize[0] = dim0size;
  dimSize[1] = dim1size;
  dimSize[2] = dim2size;
  dimSize[3] = 1;
  size = dimSize[0] * dimSize[1] * dimSize[2] * dimSize[3];
}

FeatureGroup::FeatureGroup(const string& name, int base, int dim0size, int dim1size, int dim2size, int dim3size)
:featureName(name)
{
  if(dim0size <= 0) Global::fatalError("dimsize <= 0");
  if(dim1size <= 0) Global::fatalError("dimsize <= 0");
  if(dim2size <= 0) Global::fatalError("dimsize <= 0");
  if(dim3size <= 0) Global::fatalError("dimsize <= 0");
  baseIdx = base;
  numDims = 4;
  dimSize[0] = dim0size;
  dimSize[1] = dim1size;
  dimSize[2] = dim2size;
  dimSize[3] = dim3size;
  size = dimSize[0] * dimSize[1] * dimSize[2] * dimSize[3];
}

FeatureGroup::~FeatureGroup()
{

}

int FeatureGroup::get() const
{
  return baseIdx;
}

int FeatureGroup::get(int i0) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(i0 >= 0 && i0 < dimSize[0]);
#endif
  return baseIdx + i0;
}

int FeatureGroup::get(int i0, int i1) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(i0 >= 0 && i0 < dimSize[0]);
  assert(i1 >= 0 && i1 < dimSize[1]);
#endif
  return baseIdx + i0*dimSize[1] + i1;
}

int FeatureGroup::get(int i0, int i1, int i2) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(i0 >= 0 && i0 < dimSize[0]);
  assert(i1 >= 0 && i1 < dimSize[1]);
  assert(i2 >= 0 && i2 < dimSize[2]);
#endif
  return baseIdx + i0*dimSize[1]*dimSize[2] + i1*dimSize[2] + i2;
}

int FeatureGroup::get(int i0, int i1, int i2, int i3) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(i0 >= 0 && i0 < dimSize[0]);
  assert(i1 >= 0 && i1 < dimSize[1]);
  assert(i2 >= 0 && i2 < dimSize[2]);
  assert(i3 >= 0 && i3 < dimSize[3]);
#endif
  return baseIdx + i0*dimSize[1]*dimSize[2]*dimSize[3] + i1*dimSize[2]*dimSize[3] + i2*dimSize[3] + i3;
}

//------------------------------------------------------------------------------------

FeatureSet::FeatureSet()
{
  numFeatures = 0;
}

FeatureSet::~FeatureSet()
{

}

fgrpindex_t FeatureSet::add(const string& name)
{
  FeatureGroup grp(name,numFeatures);
  groups.push_back(grp);
  names.push_back(name);
  featureOfName[name] = names.size()-1;
  numFeatures += grp.size;
  return groups.size()-1;
}

fgrpindex_t FeatureSet::add(const string& name, int num0, const string* names0)
{
  FeatureGroup grp(name,numFeatures,num0);
  groups.push_back(grp);

  for(int i0 = 0; i0<num0; i0++)
  {
    string name0 = name + "_" + (names0 == NULL ? Global::intToString(i0) : names0[i0]);
    names.push_back(name0);
    featureOfName[name0] = names.size()-1;
  }

  numFeatures += grp.size;
  return groups.size()-1;
}

fgrpindex_t FeatureSet::add(const string& name, int num0, int num1, const string* names0, const string* names1)
{
  FeatureGroup grp(name,numFeatures,num0,num1);
  groups.push_back(grp);

  for(int i0 = 0; i0<num0; i0++)
  {
    string name0 = name + "_" + (names0 == NULL ? Global::intToString(i0) : names0[i0]);
    for(int i1 = 0; i1<num1; i1++)
    {
      string name1 = name0 + "_" + (names1 == NULL ? Global::intToString(i1) : names1[i1]);
      names.push_back(name1);
      featureOfName[name1] = names.size()-1;
    }
  }

  numFeatures += grp.size;
  return groups.size()-1;
}

fgrpindex_t FeatureSet::add(const string& name, int num0, int num1, int num2, const string* names0, const string* names1, const string* names2)
{
  FeatureGroup grp(name,numFeatures,num0,num1,num2);
  groups.push_back(grp);

  for(int i0 = 0; i0<num0; i0++)
  {
    string name0 = name + "_" + (names0 == NULL ? Global::intToString(i0) : names0[i0]);
    for(int i1 = 0; i1<num1; i1++)
    {
      string name1 = name0 + "_" + (names1 == NULL ? Global::intToString(i1) : names1[i1]);
      for(int i2 = 0; i2<num2; i2++)
      {
        string name2 = name1 + "_" + (names2 == NULL ? Global::intToString(i2) : names2[i2]);
        names.push_back(name2);
        featureOfName[name2] = names.size()-1;
      }
    }
  }

  numFeatures += grp.size;
  return groups.size()-1;
}

fgrpindex_t FeatureSet::add(const string& name, int num0, int num1, int num2, int num3, const string* names0, const string* names1, const string* names2, const string* names3)
{
  FeatureGroup grp(name,numFeatures,num0,num1,num2,num3);
  groups.push_back(grp);

  for(int i0 = 0; i0<num0; i0++)
  {
    string name0 = name + "_" + (names0 == NULL ? Global::intToString(i0) : names0[i0]);
    for(int i1 = 0; i1<num1; i1++)
    {
      string name1 = name0 + "_" + (names1 == NULL ? Global::intToString(i1) : names1[i1]);
      for(int i2 = 0; i2<num2; i2++)
      {
        string name2 = name1 + "_" + (names2 == NULL ? Global::intToString(i2) : names2[i2]);
        for(int i3 = 0; i3<num3; i3++)
        {
          string name3 = name2 + "_" + (names3 == NULL ? Global::intToString(i3) : names3[i3]);
          names.push_back(name3);
          featureOfName[name3] = names.size()-1;
        }
      }
    }
  }

  numFeatures += grp.size;
  return groups.size()-1;
}

fgrpindex_t FeatureSet::add(const char* name)
{
  return add(string(name));
}

fgrpindex_t FeatureSet::add(const char* name, int num0, const string* names0)
{
  return add(string(name),num0,names0);
}

fgrpindex_t FeatureSet::add(const char* name, int num0, int num1, const string* names0, const string* names1)
{
  return add(string(name),num0,num1,names0,names1);
}

fgrpindex_t FeatureSet::add(const char* name, int num0, int num1, int num2, const string* names0, const string* names1, const string* names2)
{
  return add(string(name),num0,num1,num2,names0,names1,names2);
}

fgrpindex_t FeatureSet::add(const char* name, int num0, int num1, int num2, int num3, const string* names0, const string* names1, const string* names2, const string* names3)
{
  return add(string(name),num0,num1,num2,num3,names0,names1,names2,names3);
}

findex_t FeatureSet::get(fgrpindex_t group) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(group > 0 && group < (int)groups.size());
#endif
  return groups[group].get();
}

findex_t FeatureSet::get(fgrpindex_t group, int i0) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(group > 0 && group < (int)groups.size());
#endif
  return groups[group].get(i0);
}

findex_t FeatureSet::get(fgrpindex_t group, int i0, int i1) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(group > 0 && group < (int)groups.size());
#endif
  return groups[group].get(i0,i1);
}

findex_t FeatureSet::get(fgrpindex_t group, int i0, int i1, int i2) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(group > 0 && group < (int)groups.size());
#endif
  return groups[group].get(i0,i1,i2);
}

findex_t FeatureSet::get(fgrpindex_t group, int i0, int i1, int i2, int i3) const
{
#ifdef FEATURE_GROUP_INDEX_CHECK
  assert(group > 0 && group < (int)groups.size());
#endif
  return groups[group].get(i0,i1,i2,i3);
}


string FeatureSet::getName(findex_t feature) const
{
  return names[feature];
}

findex_t FeatureSet::getFeature(string name) const
{
  map<string,int>::const_iterator iter = featureOfName.find(name);
  if(iter == featureOfName.end())
    return -1;
  return iter->second;
}

