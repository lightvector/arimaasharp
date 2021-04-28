/*
 * coeffbasis.cpp
 * Author: davidwu
 */
#include "../core/global.h"
#include "../board/board.h"
#include "../eval/internal.h"

void CoeffBasis::single(const string& name, double* x, double scale,
    vector<string>& names, vector<double*>& coeffs,
    vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale)
{
  int start = coeffs.size();
  names.push_back(name);
  coeffs.push_back(x);
  vector<pair<int,double>> basisVec;
  basisNames.push_back(name);
  basisVec.push_back(make_pair(start,scale*scaleScale));
  bases.push_back(basisVec);
}

void CoeffBasis::arrayLeftHigh(const string& namePrefix, double* arr, int len, double lScale, double rScale,
    vector<string>& names, vector<double*>& coeffs,
    vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale)
{
  int start = coeffs.size();
  for(int i = 0; i<len; i++)
  {
    names.push_back(namePrefix + Global::intToString(i));
    coeffs.push_back(arr+i);
  }

  for(int i = 0; i<len; i++)
  {
    double scale = (len <= 1) ? lScale : (lScale + (double)i/(len-1)*(rScale-lScale));
    vector<pair<int,double>> basisVec;
    for(int j = 0; j <= i; j++)
      basisVec.push_back(make_pair(start+j,scale*scaleScale));
    basisNames.push_back(namePrefix + Global::strprintf("%d-%d",0,i));
    bases.push_back(basisVec);
  }
}

void CoeffBasis::arrayRightHigh(const string& namePrefix, double* arr, int len, double lScale, double rScale,
    vector<string>& names, vector<double*>& coeffs,
    vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale)
{
  int start = coeffs.size();
  for(int i = 0; i<len; i++)
  {
    names.push_back(namePrefix + Global::intToString(i));
    coeffs.push_back(arr+i);
  }

  for(int i = 0; i<len; i++)
  {
    double scale = (len <= 1) ? lScale : (lScale + (double)i/(len-1)*(rScale-lScale));
    vector<pair<int,double>> basisVec;
    for(int j = i; j<len; j++)
      basisVec.push_back(make_pair(start+j,scale*scaleScale));
    basisNames.push_back(namePrefix + Global::strprintf("%d-%d",i,len-1));
    bases.push_back(basisVec);
  }
}

void CoeffBasis::boardArray(const string& namePrefix, double arr[2][BSIZE], int len, double rowScale, double colScale,
    vector<string>& names, vector<double*>& coeffs,
    vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale)
{
  if(len != BSIZE)
    Global::fatalError("boardArray called with something non-board-sized");
  int start = coeffs.size();
  for(pla_t pla = 0; pla <= 1; pla++)
  {
    for(int y = 0; y<8; y++)
    {
      for(int x = 0; x<8; x++)
      {
        loc_t loc = gLoc(x,y);
        names.push_back(namePrefix + Board::writePla(pla) + Board::writeLoc(loc));
        coeffs.push_back(&arr[pla][loc]);
      }
    }
  }

  if(rowScale > 0)
  {
    for(int y = 0; y<8; y++)
    {
      double scale = rowScale;
      vector<pair<int,double>> basisVec;
      for(int x = 0; x<8; x++)
      {
        basisVec.push_back(make_pair(start+y*8+x+64,scale*scaleScale));
        basisVec.push_back(make_pair(start+(7-y)*8+x,scale*scaleScale));
      }
      basisNames.push_back(namePrefix + Global::strprintf("@y=%d",y));
      bases.push_back(basisVec);
    }
  }
  if(colScale > 0)
  {
    for(int x = 0; x<4; x++)
    {
      double scale = colScale;
      vector<pair<int,double>> basisVec;
      for(int y = 0; y<8; y++)
      {
        basisVec.push_back(make_pair(start+y*8+x,scale*scaleScale));
        basisVec.push_back(make_pair(start+y*8+x+64,scale*scaleScale));
        basisVec.push_back(make_pair(start+y*8+(7-x),scale*scaleScale));
        basisVec.push_back(make_pair(start+y*8+(7-x)+64,scale*scaleScale));
      }
      basisNames.push_back(namePrefix + Global::strprintf("@x=%d",x));
      bases.push_back(basisVec);
    }
  }
}

void CoeffBasis::linear(const string& namePrefix, double* arr, int len, double constantScale, double slopeScale, double center,
    vector<string>& names, vector<double*>& coeffs,
    vector<string>& basisNames, vector<vector<pair<int,double>>>& bases, double scaleScale)
{
  int start = coeffs.size();
  for(int i = 0; i<len; i++)
  {
    names.push_back(namePrefix + Global::intToString(i));
    coeffs.push_back(arr+i);
  }

  if(constantScale > 0)
  {
    double scale = constantScale;
    vector<pair<int,double>> basisVec;
    for(int j = 0; j < len; j++)
      basisVec.push_back(make_pair(start+j,scale*scaleScale));
    basisNames.push_back(namePrefix + "-constant");
    bases.push_back(basisVec);
  }
  if(slopeScale > 0)
  {
    double scale = slopeScale;
    vector<pair<int,double>> basisVec;
    for(int j = 0; j < len; j++)
      basisVec.push_back(make_pair(start+j,scale*scaleScale*(j-center)));
    basisNames.push_back(namePrefix + "-slope");
    bases.push_back(basisVec);
  }
}
