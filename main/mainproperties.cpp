/*
 * mainproperties.cpp
 * Author: davidwu
 */

#include <fstream>
#include <cstdlib>
#include <algorithm>
#include "../core/global.h"
#include "../core/rand.h"
#include "../core/timer.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/gameiterator.h"
#include "../learning/learner.h"
#include "../search/search.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

typedef int (*GetBucketFunc)(const Board& b);
typedef double (*GetValueFunc)(const Board& b);

static void setBasicFiltering(GameIterator& iter)
{
  iter.setDoFilter(true);
  iter.setDoFilterLemmings(true);
  iter.setDoFilterWinsInTwo(true);
  iter.setNumLoserToFilter(2);
  //Do NOT filter wins - those we still train on.
}

int MainFuncs::boardProperties(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile"
      "-property0 property"
      "<-property1 property>"
      "<-minrating rating>"
      "<-botkeepprop prop>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>"
      "<-view>"
      "<-viewslow>"
      "<-filter0 value>"
      "<-filter1 value>";
  const char* required = "property0";
  const char* allowed = "property1 view viewslow filter0 filter1 minrating botkeepprop botgameweight botposweight fancyweight";
  const char* empty = "fancyweight view viewslow";
  const char* nonempty = "property0 property1 filter0 filter1 minrating botkeepprop botgameweight botposweight";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];

  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double botKeepProp = Command::getDouble(flags,"botkeepprop",1.0);
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);

  GameIterator iter(infile);
  setBasicFiltering(iter);
  iter.setPosKeepProp(1.0);
  iter.setMoveKeepBase(20);
  iter.setMinRating(minRating);
  iter.setBotPosKeepProp(botKeepProp);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  iter.setDoPrint(true);

  string property0 = Command::getString(flags,"property0");
  int p0 = -1;
  int p1 = -1;
  if(property0 == "turnNumber") p0 = 0;
  else if(property0 == "pieceCount") p0 = 1;
  else if(property0 == "advancement") p0 = 2;
  else if(property0 == "advancementNoCaps") p0 = 3;
  else if(property0 == "separateness") p0 = 4;
  else Global::fatalError("Unknown property0");

  if(Command::isSet(flags,"property1"))
  {
    string property1 = Command::getString(flags,"property1");
    if(property1 == "turnNumber") p1 = 0;
    else if(property1 == "pieceCount") p1 = 1;
    else if(property1 == "advancement") p1 = 2;
    else if(property1 == "advancementNoCaps") p1 = 3;
    else if(property1 == "separateness") p1 = 3;
    else Global::fatalError("Unknown property1");
  }

  bool viewing = Command::isSet(flags,"view") || Command::isSet(flags,"viewslow");
  bool viewingSlow = Command::isSet(flags,"viewslow");
  bool filtering0 = Command::isSet(flags,"filter0");
  bool filtering1 = Command::isSet(flags,"filter1");
  double filter0 = Command::getDouble(flags,"filter0",0);
  double filter1 = Command::getDouble(flags,"filter1",0);

  const vector<double> bucketss[5] = {
      ArimaaFeature::turnNumberBuckets(),
      ArimaaFeature::pieceCountBuckets(),
      ArimaaFeature::advancementBuckets(),
      ArimaaFeature::advancementNoCapsBuckets(),
      ArimaaFeature::separatenessBuckets(),
  };
  GetBucketFunc bucketFuncs[5] = {
      &ArimaaFeature::turnNumberBucket,
      &ArimaaFeature::pieceCountBucket,
      &ArimaaFeature::advancementBucket,
      &ArimaaFeature::advancementNoCapsBucket,
      &ArimaaFeature::separatenessBucket,
  };

  if(p1 == -1)
  {
    GetBucketFunc bucketFunc = bucketFuncs[p0];
    const vector<double>& buckets = bucketss[p0];
    vector<double> counts = vector<double>(buckets.size(),0);

    while(iter.next())
    {
      const Board& b = iter.getBoard();
      int bucket = (*bucketFunc)(b);
      DEBUGASSERT(bucket >= 0 && bucket < (int)buckets.size());
      counts[bucket] += iter.getPosWeight();

      if(viewing && !(filtering0 && bucket != ArimaaFeature::lookupBucket(filter0,buckets)))
      {
        cout << "pc " << ArimaaFeature::pieceCountProperty(b) << " "
             << "adv " << ArimaaFeature::advancementProperty(b) << " "
             << "advnc " << ArimaaFeature::advancementNoCapsProperty(b) << " "
             << "sep " << ArimaaFeature::separatenessProperty(b) << " " << endl;
        cout << b << endl;

        if(viewingSlow)
          Global::pauseForKey();
      }
    }

    for(int i = 0; i<(int)buckets.size(); i++)
      cout << buckets[i] << "\t" << counts[i] << endl;
  }
  else
  {
    GetBucketFunc bucketFunc0 = bucketFuncs[p0];
    GetBucketFunc bucketFunc1 = bucketFuncs[p1];
    const vector<double>& buckets0 = bucketss[p0];
    const vector<double>& buckets1 = bucketss[p1];
    int rowLen = buckets0.size();
    vector<double> counts = vector<double>(buckets0.size() * buckets1.size(),0);

    while(iter.next())
    {
      const Board& b = iter.getBoard();
      int bucket0 = (*bucketFunc0)(b);
      int bucket1 = (*bucketFunc1)(b);
      DEBUGASSERT(bucket0 >= 0 && bucket0 < (int)buckets0.size());
      DEBUGASSERT(bucket1 >= 0 && bucket1 < (int)buckets1.size());
      counts[bucket0 + bucket1*rowLen] += iter.getPosWeight();

      if(viewing
         && !(filtering0 && bucket0 != ArimaaFeature::lookupBucket(filter0,buckets0))
         && !(filtering1 && bucket1 != ArimaaFeature::lookupBucket(filter1,buckets1)))
      {
        cout << "pc " << ArimaaFeature::pieceCountProperty(b) << " "
             << "adv " << ArimaaFeature::advancementProperty(b) << " "
             << "advnc " << ArimaaFeature::advancementNoCapsProperty(b) << " "
             << "sep " << ArimaaFeature::separatenessProperty(b) << " " << endl;
        cout << b << endl;

        if(viewingSlow)
          Global::pauseForKey();
      }
    }

    for(int j = 0; j<(int)buckets1.size(); j++)
      cout << "\t" << buckets1[j];
    cout << endl;
    for(int i = 0; i<(int)buckets0.size(); i++)
    {
      cout << buckets0[i];
      for(int j = 0; j<(int)buckets1.size(); j++)
        cout << "\t" << counts[i + j*rowLen];
      cout << endl;
    }
  }

  return EXIT_SUCCESS;
}


