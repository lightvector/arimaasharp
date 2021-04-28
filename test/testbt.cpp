/*
 * testbt.cpp
 *
 *  Created on: Jun 21, 2011
 *      Author: davidwu
 */

#include <cmath>
#include "../core/global.h"
#include "../learning/feature.h"
#include "../learning/featurearimaa.h"
#include "../learning/learner.h"
#include "../test/tests.h"

using namespace std;

static void addWL(const vector<findex_t>& team0, const vector<findex_t>& team1,
    int wins0, int wins1, vector<vector<vector<findex_t> > >& matches, vector<int>& winners)
{
  vector<vector<findex_t> > match;
  match.push_back(team0);
  match.push_back(team1);

  for(int i = 0; i<wins0; i++)
  {
    matches.push_back(match);
    winners.push_back(0);
  }

  for(int i = 0; i<wins1; i++)
  {
    matches.push_back(match);
    winners.push_back(1);
  }
}


void Tests::testBTGradient()
{
  FeatureSet fset;
  fgrpindex_t gA = fset.add("A");
  fgrpindex_t gB = fset.add("B");
  fgrpindex_t gC = fset.add("C");
  fgrpindex_t gD = fset.add("D");
  fgrpindex_t gE = fset.add("E");
  fgrpindex_t gF = fset.add("F");

  findex_t a = fset.get(gA);
  findex_t b = fset.get(gB);
  findex_t c = fset.get(gC);
  findex_t d = fset.get(gD);
  findex_t e = fset.get(gE);
  findex_t f = fset.get(gF);
  //findex_t prior = fset.get(PRIOR);

  vector<findex_t> ta;
  ta.push_back(a);
  vector<findex_t> tb;
  tb.push_back(b);
  vector<findex_t> tc;
  tc.push_back(c);
  vector<findex_t> td;
  td.push_back(d);
  vector<findex_t> te;
  te.push_back(e);
  vector<findex_t> tde;
  tde.push_back(d);
  tde.push_back(e);
  vector<findex_t> tff;
  tff.push_back(f);
  tff.push_back(f);

  vector<vector<vector<findex_t> > > matches;
  vector<int> winners;

  addWL(ta,tb,100,200,matches,winners);
  addWL(tb,tc,100,200,matches,winners);
  addWL(tc,tde,100,200,matches,winners);
  addWL(td,te,100,200,matches,winners);
  addWL(te,tff,100,200,matches,winners);

  Global::fatalError("disabled during compilation fix");

  //TODO DISABLED FOR COMPILATION
  //fset.addUniformPrior(1.0,1.0);
  //ArimaaFeatureSet afset(&fset,NULL,NULL,NULL);
  //BradleyTerry bt(afset);

  //bt.train(matches,winners,20,"");

  //for(int i = 0; i<(int)bt.logGamma.size(); i++)
  //{
  //  cout << fset.getName(i) << " " << bt.logGamma[i] / log(2.0) << endl;;
  //}


}
