
/*
 * mainmovelearning.cpp
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
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../learning/gameiterator.h"
#include "../eval/eval.h"
#include "../search/search.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

//MOVE LEARNER-----------------------------------------------------------
static void testMoveLearner(ArimaaFeatureSet afset, GameIterator& iter, BradleyTerry* learner);
static void testMoveLearnerAbsolute(ArimaaFeatureSet afset, GameIterator& iter, BradleyTerry* learner);
static void testEvalOrderingHelper(GameIterator& iter);

static void setBasicFiltering(GameIterator& iter)
{
  iter.setDoFilter(true);
  iter.setDoFilterLemmings(true);
  iter.setDoFilterWinsInTwo(true);
  iter.setNumInitialToFilter(2);
  iter.setNumLoserToFilter(2);
  iter.setDoFilterManyShortMoves(true);
  iter.setMinPlaUnfilteredMoves(6);
  //Do NOT filter wins - those we still train on.
}

int MainFuncs::learnMoveOrdering(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile outfile "
      "<-restartfrom file>"
      "<-iters n> "
      "<-root or -lite or -litereal>"
      "<-ratedonly>"
      "<-minrating rating>"
      "<-poskeepprop prop>"
      "<-botkeepprop prop>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>"
      "<-movekeepprop prop (default 0.005)>";
  const char* required = "iters";
  const char* allowed = "restartfrom previters root lite litereal ratedonly minrating poskeepprop botkeepprop botgameweight botposweight fancyweight movekeepprop";
  const char* empty = "root lite litereal ratedonly fancyweight";
  const char* nonempty = "restartfrom previters iters minrating poskeepprop botkeepprop botgameweight botposweight movekeepprop";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 3)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];
  string outfile = mainCommand[2];

  bool root = Command::isSet(flags,"root");
  bool lite = Command::isSet(flags,"lite");
  bool litereal = Command::isSet(flags,"litereal");
  if(!root && !lite && !litereal)
    Global::fatalError("Must specify -root or -lite or -litereal");
  int iterations = Command::getInt(flags,"iters");

  bool ratedOnly = Command::isSet(flags,"ratedonly");
  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double posKeepProp = Command::getDouble(flags,"poskeepprop",1.0);
  double botKeepProp = Command::getDouble(flags,"botkeepprop",1.0);
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);
  double moveKeepProp = Command::getDouble(flags,"movekeepprop",0.005);

  //Learning Initialization--------------
  ClockTimer timer;

  ArimaaFeatureSet afset = root ? MoveFeature::getArimaaFeatureSet() :
                           lite ? MoveFeatureLite::getArimaaFeatureSetForTraining() :
                                  MoveFeatureLite::getArimaaFeatureSet();

  cout << infile << " " << outfile << " " << Command::gitRevisionId() << endl;
  string partialFilePrefix = outfile;

  GameIterator iter(infile);
  setBasicFiltering(iter);
  iter.setPosKeepProp(posKeepProp);
  iter.setMoveKeepProp(moveKeepProp);
  iter.setMoveKeepBase(20);
  iter.setMinRating(minRating);
  iter.setRatedOnly(ratedOnly);
  iter.setBotPosKeepProp(botKeepProp);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  iter.setDoPrint(true);

  //NaiveBayes learner(MoveFeature::getFeatureSet().numFeatures,1);
  BradleyTerry* learner;

  DEBUGASSERT(Command::isSet(flags,"restartfrom") == Command::isSet(flags,"previters"))
  if(Command::isSet(flags,"restartfrom"))
  {
    string restartFrom = Command::getString(flags,"restartfrom");
    learner = new BradleyTerry(BradleyTerry::inputFromFile(afset,restartFrom.c_str()));
    string restartFromLastType = Command::getString(flags,"restartfrom") + ".lasttype";
    string restartFromDeltas = Command::getString(flags,"restartfrom") + ".deltas";
    int prevIters = Command::getInt(flags,"previters");
    learner->resumeTraining(iter,prevIters,iterations,restartFromLastType,restartFromDeltas,partialFilePrefix);
  }
  else
  {
    learner = new BradleyTerry(afset);
    learner->train(iter,iterations,partialFilePrefix);
  }

  cout << "Training Done! Outputting..." << endl;
  double trainingTime = timer.getSeconds();
  cout << "Training time: " << trainingTime << endl;

  string notes = "";
  for(int i = 0; i<argc; i++)
    notes += string(argv[i]) + " ";
  learner->outputToFile(iter,outfile.c_str(),notes.c_str());
  delete learner;

  return EXIT_SUCCESS;
}

int MainFuncs::testMoveOrdering(int argc, const char* const *argv)
{
  const char* usage =
      "weightsfile movesfile "
      "<-root or -lite or -litereal> "
      "<-absolute> "
      "<-nofilter>"
      "<-ratedonly>"
      "<-minrating rating>"
      "<-botkeepprop prop>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>"
      "<-poskeepprop prop>";
  const char* required = "";
  const char* allowed = "root lite litereal absolute nofilter ratedonly minrating botkeepprop botgameweight botposweight fancyweight poskeepprop";
  const char* empty = "root lite litereal absolute nofilter ratedonly fancyweight";
  const char* nonempty = "minrating botkeepprop botgameweight botposweight poskeepprop";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 3)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  bool root = Command::isSet(flags,"root");
  bool lite = Command::isSet(flags,"lite");
  bool litereal = Command::isSet(flags,"litereal");
  if(!root && !lite && !litereal)
    Global::fatalError("Must specify -root or -lite or -litereal");

  bool nofilter = Command::isSet(flags,"nofilter");

  bool ratedOnly = Command::isSet(flags,"ratedonly");
  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double botKeepProp = Command::getDouble(flags,"botkeepprop",1.0);
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);
  double posKeepProp = Command::getDouble(flags,"poskeepprop",1.0);

  string infile = mainCommand[1];
  string testfile = mainCommand[2];

  ArimaaFeatureSet afset = root ? MoveFeature::getArimaaFeatureSet() :
                           lite ? MoveFeatureLite::getArimaaFeatureSetForTraining() :
                                  MoveFeatureLite::getArimaaFeatureSet();

  cout << "Beginning testing!" << endl;
  cout << testfile << " " << Command::gitRevisionId() << endl;
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;

  ClockTimer timer;

  GameIterator iter(testfile);
  if(!nofilter) {
    setBasicFiltering(iter);
    iter.setDoFilterWins(true); //don't bother testing to see if we can predict winning moves
  }

  iter.setPosKeepProp(posKeepProp);
  iter.setMoveKeepProp(1.0);
  iter.setMoveKeepBase(20);
  iter.setMinRating(minRating);
  iter.setRatedOnly(ratedOnly);
  iter.setBotPosKeepProp(botKeepProp);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  iter.setDoPrint(true);

  BradleyTerry learner = BradleyTerry::inputFromFile(afset,infile.c_str());

  if(Command::isSet(flags,"absolute"))
    testMoveLearnerAbsolute(afset, iter, &learner);
  else
    testMoveLearner(afset, iter, &learner);
  cout << "Testing time: " << timer.getSeconds() << endl;
  cout << testfile << " " << Command::gitRevisionId() << endl;

  return EXIT_SUCCESS;
}

struct WeightCount
{
  double weight;
  int64_t count;
  string name;
  WeightCount()
  :weight(0),count(0),name()
  {}
  WeightCount(double weight, int count, const string& name)
  :weight(weight),count(count),name(name)
  {}

  bool operator<(const WeightCount& other) const
  {return weight < other.weight;}
  bool operator>(const WeightCount& other) const
  {return weight > other.weight;}
  bool operator<=(const WeightCount& other) const
  {return weight <= other.weight;}
  bool operator>=(const WeightCount& other) const
  {return weight >= other.weight;}
};

int MainFuncs::testIteratorWeights(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile "
      "<-nofilter>"
      "<-ratedonly>"
      "<-minrating rating>"
      "<-botkeepprop prop>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>";
  const char* required = "";
  const char* allowed = "nofilter ratedonly minrating botkeepprop botgameweight botposweight fancyweight";
  const char* empty = "nofilter ratedonly fancyweight";
  const char* nonempty = "minrating botkeepprop botgameweight botposweight";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  bool nofilter = Command::isSet(flags,"nofilter");

  bool ratedOnly = Command::isSet(flags,"ratedonly");
  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double botKeepProp = Command::getDouble(flags,"botkeepprop",1.0);
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);

  string infile = mainCommand[1];

  GameIterator iter(infile);
  if(!nofilter) {
    setBasicFiltering(iter);
    iter.setDoFilterWins(true);
  }

  iter.setPosKeepProp(1.0);
  iter.setMoveKeepProp(1.0);
  iter.setMoveKeepBase(20);
  iter.setMinRating(minRating);
  iter.setRatedOnly(ratedOnly);
  iter.setBotPosKeepProp(botKeepProp);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  //iter.setDoPrint(true);

  map<string,int64_t> countsByName;
  map<string,double> weightsByName;
  double totalWeight = 0;
  int64_t totalCount = 0;
  double botWeight = 0;
  int64_t botCount = 0;
  double hvhWeight = 0;
  int64_t hvhCount = 0;
  while(iter.next())
  {
    double posWeight = iter.getPosWeight();
    string userName = iter.getUsername(iter.getBoard().player);
    countsByName[userName]++;
    weightsByName[userName] += posWeight;
    if(iter.isBot(iter.getBoard().player))
    {
      botCount++;
      botWeight += posWeight;
    }
    if(!iter.isBot(GOLD) && !iter.isBot(SILV))
    {
      hvhCount++;
      hvhWeight += posWeight;
    }
    totalCount++;
    totalWeight += posWeight;
  }

  vector<string> iterNotes = iter.getTrainingPropertiesComments();
  for(size_t i = 0; i<iterNotes.size(); i++)
    cout << "#"<< iterNotes[i] << endl;

  cout << "Total instances: " << totalCount << endl;
  cout << "Total weight: " << totalWeight << endl;
  cout << "Bot instances: " << botCount << Global::strprintf(" (%.1f%%)",100.0*botCount/totalCount) << endl;
  cout << "Bot weight: " << botWeight << Global::strprintf(" (%.1f%%)",100.0*botWeight/totalWeight) << endl;
  cout << "Hvh instances: " << hvhCount << Global::strprintf(" (%.1f%%)",100.0*hvhCount/totalCount) << endl;
  cout << "Hvh weight: " << hvhWeight << Global::strprintf(" (%.1f%%)",100.0*hvhWeight/totalWeight) << endl;
  vector<WeightCount> weightCounts;

  map<string,int64_t>::const_iterator mapIter = countsByName.begin();
  while(mapIter != countsByName.end())
  {
    string name = mapIter->first;
    int64_t count = mapIter->second;
    double weight = weightsByName[name];
    weightCounts.push_back(WeightCount(weight,count,name));
    ++mapIter;
  }

  std::sort(weightCounts.begin(), weightCounts.end());

  for(int i = (int)weightCounts.size()-1; i>=0; i--)
  {
    const WeightCount& w = weightCounts[i];
    printf("%25s weight %6.0f (%.1f%%) count %6.0f (%.1f%%) (x%.2f)",
        w.name.c_str(),
        w.weight,100.0*w.weight/totalWeight,
        (double)w.count,100.0*w.count/totalCount,
        w.weight / w.count
        );
    cout << endl;
  }

  return EXIT_SUCCESS;
}



int MainFuncs::testGameIterator(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile";
  const char* required = "";
  const char* allowed = "filter nomovegen";
  const char* empty = "filter nomovegen";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string testfile = mainCommand[1];

  GameIterator iter(testfile);
  iter.setDoPrint(true);

  if(Command::isSet(flags,"filter"))
    setBasicFiltering(iter);

  bool moveGen = !Command::isSet(flags,"nomovegen");

  int64_t count = 0;
  int64_t moveCount = 0;
  int winningMoveIdx;
  vector<move_t> moves;
  vector<move_t> moves2;
  while(iter.next())
  {
    if(moveGen)
    {
      iter.getNextMoves(winningMoveIdx,moves);
      count++;
      moveCount += moves.size();
    }
  }

  cout << "Read " << count << " positions" << endl;
  cout << "Total " << moveCount << " legal moves" << endl;
  cout << "Done" << endl;

  return EXIT_SUCCESS;
}


int MainFuncs::viewMoveFeatures(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile <-root or -lite or -litereal>";
  const char* required = "";
  const char* allowed = "root lite litereal";
  const char* empty = "root lite litereal";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];

  bool root = Command::isSet(flags,"root");
  bool lite = Command::isSet(flags,"lite");
  bool litereal = Command::isSet(flags,"litereal");
  if(!root && !lite && !litereal)
    Global::fatalError("Must specify -root or -lite or -litereal");

  GameIterator iter(infile);
  iter.setPosKeepProp(1.0);
  iter.setMoveKeepProp(0.005);
  iter.setMoveKeepBase(20);
  iter.setDoPrint(true);

  ArimaaFeatureSet afset = root ? MoveFeature::getArimaaFeatureSet() :
                           lite ? MoveFeatureLite::getArimaaFeatureSetForTraining() :
                                  MoveFeatureLite::getArimaaFeatureSet();

  while(iter.next())
  {
    cout << iter.getBoard() << endl;
    cout << Board::writeMove(iter.getBoard(),iter.getNextMove()) << endl;
    Board b = iter.getBoard();
    void* data = afset.getPosData(b,iter.getHist(),iter.getBoard().player);
    b.makeMove(iter.getNextMove());
    vector<findex_t> features = afset.getFeatures(b, data, iter.getBoard().player, iter.getNextMove(), iter.getHist());
    afset.freePosData(data);

    int featuresSize = features.size();
    for(int i = 0; i<featuresSize; i++)
    {
      const string& str = afset.fset->names[features[i]];
      cout << str << endl;
    }
    cout << endl;

    Global::pauseForKey();
    /*
    int winningMoveIdx;
    vector<move_t> moves;
    iter.getNextMoves(winningMoveIdx,moves);
    for(int i = 0; i<(int)moves.size(); i++)
    {
      b = iter.getBoard();
      b.makeMove(moves[i]);
      vector<findex_t> features = afset.getFeatures(b, data, iter.getBoard().player, moves[i], iter.getHist());

      cout << iter.getBoard() << endl;
      cout << i << " " << Board::writeMove(iter.getBoard(),moves[i]) << endl;
      int featuresSize = features.size();
      for(int j = 0; j<featuresSize; j++)
      {
        const string& str = afset.fset->names[features[j]];
        cout << str << endl;
      }
      cout << endl;
      Global::pauseForKey();
    }*/
  }

  return EXIT_SUCCESS;
}

int MainFuncs::testMoveFeatureSpeed(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile <-root or -lite or -litereal>";
  const char* required = "";
  const char* allowed = "root lite litereal";
  const char* empty = "root lite litereal";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];

  bool root = Command::isSet(flags,"root");
  bool lite = Command::isSet(flags,"lite");
  bool litereal = Command::isSet(flags,"litereal");
  if(!root && !lite && !litereal)
    Global::fatalError("Must specify -root or -lite or -litereal");

  cout << "Beginning testing!" << endl;
  cout << infile << " " << Command::gitRevisionId() << endl;
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;

  GameIterator iter(infile);
  iter.setPosKeepProp(1.0);
  iter.setMoveKeepProp(1.0);
  iter.setMoveKeepBase(20);
  iter.setDoPrint(true);

  ArimaaFeatureSet afset = root ? MoveFeature::getArimaaFeatureSet() :
                           lite ? MoveFeatureLite::getArimaaFeatureSetForTraining() :
                                  MoveFeatureLite::getArimaaFeatureSet();

  int numFeatures = afset.fset->numFeatures;
  vector<double> fakeFeatureWeights;
  fakeFeatureWeights.resize(numFeatures);
  for(int i = 0; i<numFeatures; i++)
    fakeFeatureWeights[i] = log(i+2);

  double fsum = 0.0;

  ClockTimer timer;
  double totalSecs = 0;
  int64_t totalPositions = 0;
  int64_t totalCalls = 0;

  while(iter.next())
  {
    int winningMoveIdx;
    vector<move_t> moves;
    iter.getNextMoves(winningMoveIdx,moves);
    int numMoves = moves.size();

    timer.reset();
    void* data;

    {
      Board b = iter.getBoard();
      data = afset.getPosData(b,iter.getHist(),iter.getBoard().player);
    }

    for(int i = 0; i<numMoves; i++)
    {
      Board b = iter.getBoard();
      b.makeMove(moves[i]);
      fsum += afset.getFeatureSum(b,data,iter.getBoard().player,moves[i],iter.getHist(),fakeFeatureWeights);
    }

    afset.freePosData(data);

    double secs = timer.getSeconds();
    totalSecs += secs;
    totalPositions++;
    totalCalls += numMoves;


    if(totalPositions % 100 == 0)
    {
      cout << endl;
      cout << "Total Positions: " << totalPositions << endl;
      cout << "Total Calls: " << totalCalls << endl;
      cout << "Total Time: " << totalSecs << endl;
      cout << "Positions/Sec " << totalPositions / totalSecs << endl;
      cout << "Calls/Sec " << totalCalls / totalSecs << endl;
      printf("%.19f\n", fsum);
      cout << endl;
    }
  }

  cout << "FSum " << fsum << endl;

  return EXIT_SUCCESS;
}



int MainFuncs::testEvalOrdering(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string testfile = mainCommand[1];

  cout << "Beginning testing!" << endl;
  GameIterator iter(testfile);
  setBasicFiltering(iter);
  iter.setDoPrint(true);
  testEvalOrderingHelper(iter);

  return EXIT_SUCCESS;
}


static void testMoveLearner(ArimaaFeatureSet afset, GameIterator& iter, BradleyTerry* learner)
{
  static const int moveRankCumFreqLen = 1000;
  double moveRankCumFreq[moveRankCumFreqLen];
  double percentageCumFreq[100];
  long long maxMoves = 0;
  long long totalMoves = 0;
  double totalWeightedInstances = 0;
  long long totalInstances = 0;

  for(int i = 0; i<moveRankCumFreqLen; i++)
    moveRankCumFreq[i] = 0;
  for(int i = 0; i<100; i++)
    percentageCumFreq[i] = 0;

  while(true)
  {
    bool suc = iter.next();
    if(!suc || (totalInstances % 500 == 0 && totalInstances > 0))
    {
      cout << "Testing: " << totalInstances << endl;
      cout << "Top X" << endl;
      cout << "1\t " << (moveRankCumFreq[0]/totalWeightedInstances*100.0) << endl;
      cout << "2\t " << (moveRankCumFreq[1]/totalWeightedInstances*100.0) << endl;
      cout << "5\t " << (moveRankCumFreq[4]/totalWeightedInstances*100.0) << endl;
      cout << "10\t " << (moveRankCumFreq[9]/totalWeightedInstances*100.0) << endl;
      cout << "15\t " << (moveRankCumFreq[14]/totalWeightedInstances*100.0) << endl;
      cout << "20\t " << (moveRankCumFreq[19]/totalWeightedInstances*100.0) << endl;
      cout << "50\t " << (moveRankCumFreq[49]/totalWeightedInstances*100.0) << endl;
      cout << "100\t " << (moveRankCumFreq[99]/totalWeightedInstances*100.0) << endl;
      cout << "200\t " << (moveRankCumFreq[199]/totalWeightedInstances*100.0) << endl;
      cout << "500\t " << (moveRankCumFreq[499]/totalWeightedInstances*100.0) << endl;
      cout << "1000\t " << (moveRankCumFreq[999]/totalWeightedInstances*100.0) << endl;
      cout << "Top %" << endl;
      cout << " 1\t " << (percentageCumFreq[0]/totalWeightedInstances*100.0) << endl;
      cout << " 2\t " << (percentageCumFreq[1]/totalWeightedInstances*100.0) << endl;
      cout << " 5\t " << (percentageCumFreq[4]/totalWeightedInstances*100.0) << endl;
      cout << "10\t " << (percentageCumFreq[9]/totalWeightedInstances*100.0) << endl;
      cout << "20\t " << (percentageCumFreq[19]/totalWeightedInstances*100.0) << endl;
      cout << "30\t " << (percentageCumFreq[29]/totalWeightedInstances*100.0) << endl;
      cout << "40\t " << (percentageCumFreq[39]/totalWeightedInstances*100.0) << endl;
      cout << "50\t " << (percentageCumFreq[49]/totalWeightedInstances*100.0) << endl;
      cout << "60\t " << (percentageCumFreq[59]/totalWeightedInstances*100.0) << endl;
      cout << "70\t " << (percentageCumFreq[69]/totalWeightedInstances*100.0) << endl;
      cout << "80\t " << (percentageCumFreq[79]/totalWeightedInstances*100.0) << endl;
      cout << "90\t " << (percentageCumFreq[89]/totalWeightedInstances*100.0) << endl;
      cout << "Total Instances: " << totalInstances << endl;
      cout << "Total Weighted Instances: " << totalWeightedInstances << endl;
      cout << "Avg Branching: " << ((double)totalMoves/totalInstances) << endl;
      cout << "Max Branching: " << maxMoves << endl;
    }

    if(!suc)
      break;

    int winningTeam;
    vector<vector<findex_t> > teams;
    vector<double> matchParallelFactors;
    iter.getNextMoveFeatures(afset,winningTeam,teams,matchParallelFactors);
    double posWeight = iter.getPosWeight();

    //Evaluate all the moves and find the rank of the winner
    double winningEval = learner->evaluate(teams[winningTeam],matchParallelFactors);

    int numTeams = teams.size();
    int numBetter = 0;
    double bestEval = -1e100;
    //int bestIndex = 0;
    for(int i = 0; i<numTeams; i++)
    {
      if(i == winningTeam)
        continue;
      double eval = learner->evaluate(teams[i],matchParallelFactors);
      if(eval > winningEval)
        numBetter++;

      if(eval > bestEval)
      {
        bestEval = eval;
        //bestIndex = i;
      }
    }

    int rank = numBetter;

    //Update stats
    totalMoves += numTeams;
    totalInstances++;
    totalWeightedInstances += posWeight;
    if(numTeams > maxMoves)
      maxMoves = numTeams;
    for(int i = rank; i<moveRankCumFreqLen; i++)
      moveRankCumFreq[i] += posWeight;
    int percentRank = rank*100/numTeams;
    for(int i = percentRank; i<100; i++)
      percentageCumFreq[i] += posWeight;

    //Output major errors
    /*
    if(percentRank >= 10)
    {
      vector<move_t> moves;
      int winningMoveIdx;
      iter.computeNextMoves(winningMoveIdx,moves);

      cout << iter.board << endl;
      cout << "Predicted best move: " << ArimaaIO::writeMove(iter.board,moves[bestIndex]);
      cout << " Eval " << bestEval << endl;
      cout << "Actual move: " << ArimaaIO::writeMove(iter.board,moves[winningTeam]);
      cout << " Eval " << winningEval << " Rank " << rank << " Percent " << percentRank << endl;
    }*/
  }
}


static void testMoveLearnerAbsolute(ArimaaFeatureSet afset, GameIterator& iter, BradleyTerry* learner)
{
  static const int moveValueCumFreqLen = 81;
  double range = 8;
  double moveValueCumFreq[moveValueCumFreqLen];
  double totalValueCumFreq[moveValueCumFreqLen];
  long long maxMoves = 0;
  long long totalMoves = 0;
  double totalWeightedMoves = 0;
  long long totalInstances = 0;
  double totalWeightedInstances = 0;

  for(int i = 0; i<moveValueCumFreqLen; i++)
  {
    moveValueCumFreq[i] = 0;
    totalValueCumFreq[i] = 0;
  }

  while(true)
  {
    bool suc = iter.next();
    if(!suc || (totalInstances % 500 == 0 && totalInstances > 0))
    {
      cout << "Testing: " << totalInstances << endl;
      cout << "Cutoff " << endl;
      for(int i = 0; i<moveValueCumFreqLen; i++)
      {
        double cutOff = (i - moveValueCumFreqLen/2) * range * 2 / (moveValueCumFreqLen-1);
        cout << Global::strprintf("%.3f",cutOff) << ": keepnotbest "
             << ((double)totalValueCumFreq[i]/totalWeightedMoves*100.0) << ", keepbest "
             << ((double)moveValueCumFreq[i]/totalWeightedInstances*100.0) << endl;
      }
      cout << "Total Instances: " << totalInstances << endl;
      cout << "Total Weighted Instances: " << totalWeightedInstances << endl;
      cout << "Avg Branching: " << ((double)totalMoves/totalInstances) << endl;
      cout << "Max Branching: " << maxMoves << endl;
    }

    if(!suc)
      break;

    int winningTeam;
    vector<vector<findex_t> > teams;
    vector<double> matchParallelFactors;
    iter.getNextMoveFeatures(afset,winningTeam,teams,matchParallelFactors);
    double posWeight = iter.getPosWeight();

    //Evaluate all the moves and find the rank of the winner
    double winningEval = learner->evaluate(teams[winningTeam],matchParallelFactors);
    for(int i = 0; i<moveValueCumFreqLen; i++)
    {
      double cutOff = (i - moveValueCumFreqLen/2) * range * 2 / (moveValueCumFreqLen-1);
      if(winningEval >= cutOff)
        moveValueCumFreq[i] += posWeight;
    }
    int numTeams = teams.size();
    for(int i = 0; i<numTeams; i++)
    {
      double eval = learner->evaluate(teams[i],matchParallelFactors);
      for(int j = 0; j<moveValueCumFreqLen; j++)
      {
        double cutOff = (j - moveValueCumFreqLen/2) * range * 2 / (moveValueCumFreqLen-1);
        if(eval >= cutOff)
          totalValueCumFreq[j] += posWeight;
      }
    }

    //Update stats
    totalMoves += numTeams;
    totalWeightedMoves += numTeams * posWeight;
    totalInstances++;
    totalWeightedInstances += posWeight;
    if(numTeams > maxMoves)
      maxMoves = numTeams;
  }
}

static void testEvalOrderingHelper(GameIterator& iter)
{
  static const int moveRankCumFreqLen = 1000;
  int moveRankCumFreq[moveRankCumFreqLen];
  int percentageCumFreq[20];
  long long maxMoves = 0;
  long long totalMoves = 0;
  long long totalInstances = 0;

  for(int i = 0; i<moveRankCumFreqLen; i++)
    moveRankCumFreq[i] = 0;
  for(int i = 0; i<20; i++)
    percentageCumFreq[i] = 0;

  while(iter.next())
  {
    if(totalInstances % 500 == 1)
    {
      cout << "Testing: " << totalInstances << endl;
      cout << "Top X" << endl;
      cout << "1\t " << ((double)moveRankCumFreq[0]/totalInstances*100.0) << endl;
      cout << "2\t " << ((double)moveRankCumFreq[1]/totalInstances*100.0) << endl;
      cout << "5\t " << ((double)moveRankCumFreq[4]/totalInstances*100.0) << endl;
      cout << "10\t " << ((double)moveRankCumFreq[9]/totalInstances*100.0) << endl;
      cout << "15\t " << ((double)moveRankCumFreq[14]/totalInstances*100.0) << endl;
      cout << "20\t " << ((double)moveRankCumFreq[19]/totalInstances*100.0) << endl;
      cout << "50\t " << ((double)moveRankCumFreq[49]/totalInstances*100.0) << endl;
      cout << "100\t " << ((double)moveRankCumFreq[99]/totalInstances*100.0) << endl;
      cout << "200\t " << ((double)moveRankCumFreq[199]/totalInstances*100.0) << endl;
      cout << "500\t " << ((double)moveRankCumFreq[499]/totalInstances*100.0) << endl;
      cout << "1000\t " << ((double)moveRankCumFreq[999]/totalInstances*100.0) << endl;
      cout << "Top %" << endl;
      cout << " 5\t " << ((double)percentageCumFreq[0]/totalInstances*100.0) << endl;
      cout << "10\t " << ((double)percentageCumFreq[1]/totalInstances*100.0) << endl;
      cout << "20\t " << ((double)percentageCumFreq[3]/totalInstances*100.0) << endl;
      cout << "30\t " << ((double)percentageCumFreq[5]/totalInstances*100.0) << endl;
      cout << "40\t " << ((double)percentageCumFreq[7]/totalInstances*100.0) << endl;
      cout << "50\t " << ((double)percentageCumFreq[9]/totalInstances*100.0) << endl;
      cout << "60\t " << ((double)percentageCumFreq[11]/totalInstances*100.0) << endl;
      cout << "70\t " << ((double)percentageCumFreq[13]/totalInstances*100.0) << endl;
      cout << "80\t " << ((double)percentageCumFreq[15]/totalInstances*100.0) << endl;
      cout << "90\t " << ((double)percentageCumFreq[17]/totalInstances*100.0) << endl;
      cout << "Total Instances: " << totalInstances << endl;
      cout << "Avg Branching: " << ((double)totalMoves/totalInstances) << endl;
      cout << "Max Branching: " << maxMoves << endl;
    }

    int bestMoveIdx;
    vector<move_t> moves;
    iter.getNextMoves(bestMoveIdx,moves);

    //Evaluate all the moves and find the rank of the winner
    Board bCopy = iter.getBoard();
    bool suc = bCopy.makeMoveLegalNoUndo(iter.getNextMove());
    if(!suc)
      Global::fatalError("Illegal move!");
    int winningEval = -Eval::evaluate(bCopy,bCopy.player,0,NULL);

    int numTeams = moves.size();
    int numBetter = 0;
    double bestEval = -1e100;
    //int bestIndex = 0;
    for(int i = 0; i<numTeams; i++)
    {
      if(i == bestMoveIdx)
        continue;

      bCopy = iter.getBoard();
      suc = bCopy.makeMoveLegalNoUndo(moves[i]);
      if(!suc)
        Global::fatalError("Illegal move!");
      int eval = -Eval::evaluate(bCopy,bCopy.player,0,NULL);
      if(eval > winningEval)
        numBetter++;

      if(eval > bestEval)
      {
        bestEval = eval;
        //bestIndex = i;
      }
    }

    int rank = numBetter;

    //Update stats
    totalMoves += numTeams;
    totalInstances++;
    if(numTeams > maxMoves)
      maxMoves = numTeams;
    for(int i = rank; i<moveRankCumFreqLen; i++)
      moveRankCumFreq[i]++;
    int percentRank = rank*20/numTeams ;
    for(int i = percentRank; i<20; i++)
      percentageCumFreq[i]++;

    //Output major errors
    /*
    if(totalInstances % 1 == 0 && percentRank >= 0)
    {
      cout << iter.currentBoard << endl;
      cout << "Predicted best move: " << writeMove(iter.currentBoard,iter.nextMoves[bestIndex]);
      cout << " Eval " << bestEval << endl;
      cout << "Actual move: " << writeMove(iter.currentBoard,iter.nextMoves[winningTeam]);
      cout << " Eval " << winningEval << " Rank " << rank << " PerTen " << percentRank << endl;
      cout << "Full actual move: " << writeMove(iter.startBoard,iter.games[iter.currentGame].second[iter.currentMove]) << endl;
    }*/
  }
  cout << "Top X" << endl;
  cout << "1\t " << ((double)moveRankCumFreq[0]/totalInstances*100.0) << endl;
  cout << "2\t " << ((double)moveRankCumFreq[1]/totalInstances*100.0) << endl;
  cout << "5\t " << ((double)moveRankCumFreq[4]/totalInstances*100.0) << endl;
  cout << "10\t " << ((double)moveRankCumFreq[9]/totalInstances*100.0) << endl;
  cout << "15\t " << ((double)moveRankCumFreq[14]/totalInstances*100.0) << endl;
  cout << "20\t " << ((double)moveRankCumFreq[19]/totalInstances*100.0) << endl;
  cout << "50\t " << ((double)moveRankCumFreq[49]/totalInstances*100.0) << endl;
  cout << "100\t " << ((double)moveRankCumFreq[99]/totalInstances*100.0) << endl;
  cout << "200\t " << ((double)moveRankCumFreq[199]/totalInstances*100.0) << endl;
  cout << "500\t " << ((double)moveRankCumFreq[499]/totalInstances*100.0) << endl;
  cout << "1000\t " << ((double)moveRankCumFreq[999]/totalInstances*100.0) << endl;
  cout << "Top %" << endl;
  cout << " 5\t " << ((double)percentageCumFreq[0]/totalInstances*100.0) << endl;
  cout << "10\t " << ((double)percentageCumFreq[1]/totalInstances*100.0) << endl;
  cout << "20\t " << ((double)percentageCumFreq[3]/totalInstances*100.0) << endl;
  cout << "30\t " << ((double)percentageCumFreq[5]/totalInstances*100.0) << endl;
  cout << "40\t " << ((double)percentageCumFreq[7]/totalInstances*100.0) << endl;
  cout << "50\t " << ((double)percentageCumFreq[9]/totalInstances*100.0) << endl;
  cout << "60\t " << ((double)percentageCumFreq[11]/totalInstances*100.0) << endl;
  cout << "70\t " << ((double)percentageCumFreq[13]/totalInstances*100.0) << endl;
  cout << "80\t " << ((double)percentageCumFreq[15]/totalInstances*100.0) << endl;
  cout << "90\t " << ((double)percentageCumFreq[17]/totalInstances*100.0) << endl;
  cout << "Total Instances: " << totalInstances << endl;
  cout << "Avg Branching: " << ((double)totalMoves/totalInstances) << endl;
  cout << "Max Branching: " << maxMoves << endl;
}

static void writeMovesHeader(ofstream& outMoves)
{
  outMoves
  << "gidx,"
  << "gid,"
  << "isRated,"
  << "winner,"
  << "metaWinner,"
  << "gname,"
  << "sname,"
  << "pla,"
  << "turn,"
  << "move,"
  << "pct,"
  << "invgain,"
  << "hash,"
  << endl;
}
static void writeGamesHeader(ofstream& outGames)
{
  outGames
  << "gidx,"
  << "gid,"
  << "isRated,"
  << "winner,"
  << "metaWinner,"
  << "gname,"
  << "sname,"
  << "gnmoves,"
  << "snmoves,"
  << "gavglp,"
  << "savglp,"
  << endl;
}

static void findBlundersInGame(int gameIdx,
    vector<Board>& boards, const vector<move_t>& moves, const vector<bool>& filter,
    const vector<double>& percentileOrder, const vector<double>& moveGainFactor,
    const double sumLogProb[2], const int numUnfilteredMoves[2], const string username[2],
    pla_t winner, pla_t metaWinner, bool isRated, int metaGameId,
    ofstream* outMoves, ofstream* outGames)
{
  int numBoards = boards.size();
  int numMoves = moves.size();
  DEBUGASSERT(numBoards == numMoves);
  DEBUGASSERT(numBoards > 0);
  DEBUGASSERT((int)percentileOrder.size() == numMoves);
  DEBUGASSERT((int)moveGainFactor.size() == numMoves);

  //Add on the final position resulting from the final move
  {
    Board finalBoard = boards[boards.size() - 1];
    bool suc = finalBoard.makeMoveLegalNoUndo(moves[boards.size()-1]);
    DEBUGASSERT(suc);
    boards.push_back(finalBoard);
    numBoards++;
  }

  cout
  << "gidx " << gameIdx
  << " gid " << metaGameId
  << " isRated " << isRated
  << " winner " << Board::writePla(winner)
  << " metaWinner " << Board::writePla(metaWinner)
  << " gname " << username[GOLD]
  << " sname " << username[SILV]
  << " gnmoves " << numUnfilteredMoves[GOLD]
  << " snmoves " << numUnfilteredMoves[SILV]
  << " gavglp " << sumLogProb[GOLD]/numUnfilteredMoves[GOLD]
  << " savglp " << sumLogProb[SILV]/numUnfilteredMoves[SILV]
  << endl;

  if(outGames != NULL)
  {
    (*outGames)
    << gameIdx << ","
    << metaGameId << ","
    << isRated << ","
    << Board::writePla(winner) << ","
    << Board::writePla(metaWinner) << ","
    << username[GOLD] << ","
    << username[SILV] << ","
    << numUnfilteredMoves[GOLD] << ","
    << numUnfilteredMoves[SILV] << ","
    << sumLogProb[GOLD]/numUnfilteredMoves[GOLD] << ","
    << sumLogProb[SILV]/numUnfilteredMoves[SILV] << ","
    << endl;
  }

  for(int i = 0; i < numMoves; i++)
  {
    if(filter[i])
      continue;
    if(percentileOrder[i] < 10)
      continue;

    cout
    << "gidx " << gameIdx
    << " gid " << metaGameId
    << " isRated " << isRated
    << " winner " << Board::writePla(winner)
    << " metaWinner " << Board::writePla(metaWinner)
    << " gname " << username[GOLD]
    << " sname " << username[SILV]
    << endl
    << "pla " << Board::writePla(boards[i].player)
    << " move " << Board::writeMove(boards[i],moves[i])
    << " pct " << percentileOrder[i]
    << " invgain " << 1/moveGainFactor[i]
    << endl;
    cout << boards[i] << endl;
    cout << endl;

    if(outMoves != NULL)
    {
      (*outMoves)
      << gameIdx << ","
      << metaGameId << ","
      << isRated << ","
      << Board::writePla(winner) << ","
      << Board::writePla(metaWinner) << ","
      << username[GOLD] << ","
      << username[SILV] << ","
      << Board::writePla(boards[i].player) << ","
      << Board::writePlaTurn(boards[i].turnNumber) << ","
      << Board::writeMove(boards[i],moves[i]) << ","
      << percentileOrder[i] << ","
      << 1/moveGainFactor[i] << ","
      << boards[i].sitCurrentHash << ","
      << endl;
    }
  }


  /*
  vector<eval_t> evals;
  for(int i = 0; i < numBoards; i++)
  {
    Board b = boards[i];

    pla_t curWinner = b.getWinner();
    if(curWinner != NPLA)
    {
      DEBUGASSERT(curWinner == winner);
      evals.push_back(curWinner == GOLD ? Eval::WIN : Eval::LOSE);
    }
    else
    {
      eval_t eval = Eval::evaluate(b,b.player,0,params,NULL);
      evals.push_back(b.player == GOLD ? eval : -eval);
    }
  }
  */

  //Get moves that immediately drop in eval after the opponent's reply and stay dropped
  /*
  eval_t threshold = 1000;
  for(int i = 0; i<numMoves-1; i++)
  {
    if(filter[i])
      continue;

    pla_t pla = boards[i].player;
    int plaFactor = pla == GOLD ? 1 : -1;
    eval_t baseEval = evals[i] * plaFactor;

    bool allWorse = true;
    eval_t minDiff = Eval::WIN;
    for(int j = i+2; j<numBoards; j++)
    {
      eval_t laterEval = evals[j] * plaFactor;
      if(laterEval + threshold >= baseEval)
      {
        allWorse = false;
        break;
      }
      eval_t diff = baseEval - laterEval;
      if(diff < minDiff)
        minDiff = diff;
    }
    if(allWorse)
    {
      cout << "Permanent loss by " << Board::writePla(pla)
           << " idx " << gameIdx << " gid " << metaGameId << " isRated " << isRated << " minDelta " << minDiff
           << " winner " << Board::writePla(winner) << " metaWinner " << Board::writePla(metaWinner) << endl;
      cout << "Move: " << Board::writeMove(boards[i],moves[i]) << endl;
      cout << "Reply move: " << Board::writeMove(boards[i+1],moves[i+1]) << endl;
      cout << boards[i] << endl;
    }
  }
  */
}

int MainFuncs::findBlunders(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile "
      "<-nofilter>"
      "<-outprefix FILE>";
  const char* required = "";
  const char* allowed = "nofilter outprefix";
  const char* empty = "nofilter";
  const char* nonempty = "outprefix";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  bool nofilter = Command::isSet(flags,"nofilter");
  ofstream* outMoves = NULL;
  ofstream* outGames = NULL;
  if(Command::isSet(flags,"outprefix"))
  {
    string outMovesName = flags["outprefix"] + "_filtermoves.csv";
    string outGamesName = flags["outprefix"] + "_filtergames.csv";
    outMoves = new ofstream(outMovesName.c_str());
    outGames = new ofstream(outGamesName.c_str());

    writeMovesHeader(*outMoves);
    writeGamesHeader(*outGames);
  }

  string infile = mainCommand[1];

  cout << Command::gitRevisionId() << endl;
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;

  cout << "Reading " << infile << endl;

  ClockTimer timer;

  GameIterator iter(infile);
  if(!nofilter) {
    setBasicFiltering(iter);
    iter.setDoFilterWins(true);
    iter.setDoFilter(false); //Don't apply filter, merely compute it.
  }

  cout << "Loaded " << iter.getTotalNumGames() << " games" << endl;

  ArimaaFeatureSet afset = MoveFeature::getArimaaFeatureSet();
  BradleyTerry learner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());

  int gameIdx = -1;
  vector<Board> gameBoards;
  vector<move_t> gameMoves;
  vector<bool> gameFilter;

  vector<double> percentileOrder; //percentile of root move ordering, lower is better
  vector<double> moveGainFactor;  //modeled probability multiplied by number of legal moves
  double sumLogProb[2] = {0,0};      //sum of logprobabilities of unfiltered moves in the game by pla
  int numUnfilteredMoves[2] = {0,0};

  pla_t winner = NPLA;
  pla_t metaWinner = NPLA;
  bool isRated = false;
  int metaGameId = -1;
  string username[2];

  while(iter.next())
  {
    if(iter.getGameIdx() != gameIdx)
    {
      //When the game changes, process the game just finished
      if(gameBoards.size() > 0)
        findBlundersInGame(gameIdx,gameBoards,gameMoves,gameFilter,
            percentileOrder,moveGainFactor,sumLogProb,numUnfilteredMoves,username,
            winner,metaWinner,isRated,metaGameId,outMoves,outGames);

      //Then clear and prepare for the next game
      gameIdx = iter.getGameIdx();
      gameBoards.clear();
      gameMoves.clear();
      gameFilter = iter.getCurrentFilter();
      if(nofilter)
      {for(int i = 0; i<(int)gameFilter.size(); i++) gameFilter[i] = false;}

      percentileOrder.clear();
      moveGainFactor.clear();
      sumLogProb[GOLD] = 0;
      sumLogProb[SILV] = 0;
      numUnfilteredMoves[GOLD] = 0;
      numUnfilteredMoves[SILV] = 0;

      winner = iter.getGameWinner();
      metaWinner = iter.getMetaGameWinner();
      isRated = iter.isGameRated();
      metaGameId = iter.getMetaGameId();
      username[GOLD] = iter.getUsername(GOLD);
      username[SILV] = iter.getUsername(SILV);
    }

    //Record game position and move
    gameBoards.push_back(iter.getBoard());
    gameMoves.push_back(iter.getNextMove());

    //Compute modeled probabilities of the chosen move from existing move features, excluding filtered stuff
    if(gameFilter[gameBoards.size()-1])
    {
      percentileOrder.push_back(0);
      moveGainFactor.push_back(0);
      (void)sumLogProb;
      (void)numUnfilteredMoves;
    }
    else
    {
      int winningTeam;
      vector<vector<findex_t> > teams;
      vector<double> matchParallelFactors;
      iter.getNextMoveFeatures(afset,winningTeam,teams,matchParallelFactors);

      double winningLogProb = learner.evaluate(teams[winningTeam],matchParallelFactors);
      double sumProb = 0;

      int numTeams = teams.size();
      int numBetter = 0;
      for(int i = 0; i<numTeams; i++)
      {
        if(i == winningTeam)
          continue;
        double logProb = learner.evaluate(teams[i],matchParallelFactors);
        if(logProb > winningLogProb)
          numBetter++;
        sumProb += exp(logProb);
      }

      //Normalizing factor to subtract that makes probabilities sum to 1
      double normalization = log(sumProb);

      //Update stats
      percentileOrder.push_back((double)numBetter/numTeams * 100.0);
      moveGainFactor.push_back(exp(winningLogProb - normalization) * numTeams);
      sumLogProb[iter.getBoard().player] += winningLogProb - normalization;
      numUnfilteredMoves[iter.getBoard().player]++;
    }
  }

  //Process the final game
  if(gameBoards.size() > 0)
    findBlundersInGame(gameIdx,gameBoards,gameMoves,gameFilter,
        percentileOrder,moveGainFactor,sumLogProb,numUnfilteredMoves,username,
        winner,metaWinner,isRated,metaGameId,outMoves,outGames);

  if(outMoves != NULL)
  {
    outMoves->close();
    outGames->close();
    delete outMoves;
    delete outGames;
  }

  return EXIT_SUCCESS;
}
