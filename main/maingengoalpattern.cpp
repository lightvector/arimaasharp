/*
 * maingengoalpattern.cpp
 * Author: davidwu
 */

#include <fstream>
#include <set>
#include <sstream>
#include <cstdlib>
#include "../core/global.h"
#include "../core/rand.h"
#include "../core/timer.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/boardtrees.h"
#include "../pattern/gengoalpatterns.h"
#include "../pattern/patternsolver.h"
#include "../learning/featurearimaa.h"
#include "../learning/featuremove.h"
#include "../eval/eval.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../search/searchutils.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"
#include "../program/init.h"
#include "../program/command.h"
#include "../main/main.h"

/*
int MainFuncs::genGoalPatterns(int argc, const char* const *argv)
{
  cout << "SEED " << Init::getSeed() << endl;
  if(argc != 4)
    return EXIT_FAILURE;

  const char* infile = argv[1];
  const char* outfile = argv[2];
  int numOppSteps = Global::stringToInt(argv[3]);

  vector<GoalPatternInput> gpats = ArimaaIO::readMultiGoalPatternFile(infile);

  ofstream out(outfile);

  //Every distinct pattern of rabbits gets its own category name and its own index
  int numMultiRabbitPatterns = 0;
  map<uint64_t,int> rabMapToIndex;
  map<uint64_t,string> rabMapToName;

  ClockTimer timer;
  int size = gpats.size();
  for(int i = 0; i<size; i++)
  {
    //Compute the rabbit pattern
    Bitmap rMap;
    for(int loc = 0; loc<64; loc++)
      if(gpats[i].pattern[loc] == 'R')
        rMap.setOn(loc);

    //New pattern!
    if(rabMapToName.find(rMap.bits) == rabMapToName.end())
    {
      //Count rabbits
      int numPlaRabbits = 0;
      loc_t plaRabbitLoc = ERRLOC;
      for(int loc = 0; loc<64; loc++)
      {
        if(gpats[i].pattern[loc] == 'R')
        {numPlaRabbits++; plaRabbitLoc = loc;}
      }

      //Compute the category name
      string categoryName;
      if(numPlaRabbits == 1)
        categoryName = ArimaaIO::writeLoc(plaRabbitLoc) + "_s" + Global::intToString(numOppSteps+4);
      else
        categoryName = "multiRab" + Global::intToString(numMultiRabbitPatterns++) + "_s" + Global::intToString(numOppSteps+4);

      rabMapToName[rMap.bits] = categoryName;
    }

    string coutstring;
    string pattern = genGoalPatternSpiralFrozenLast(gpats[i],numOppSteps,false,coutstring);
    out << "###############################" << endl;
    out << "Name: " << rabMapToName[rMap.bits] << "_" << rabMapToIndex[rMap.bits] << endl;
    out << pattern << ";" << endl;
    rabMapToIndex[rMap.bits]++;
    cout << coutstring << endl;

    string pattern2 = genGoalPatternSpiralFrozenLast(gpats[i],numOppSteps,true,coutstring);
    if(pattern2 != pattern)
    {
      out << "###############################" << endl;
      out << "Name: " << rabMapToName[rMap.bits] << "_" << rabMapToIndex[rMap.bits] << endl;
      out << pattern2 << ";" << endl;
      rabMapToIndex[rMap.bits]++;
      cout << coutstring << endl;
    }
  }
  out.close();

  double time = timer.getSeconds();
  cout << "Generated " << size << " patterns" << endl;
  cout << "Total time: " << time << endl;
  cout << "Time per pattern: " << (time/size) << endl;

  return EXIT_SUCCESS;
}
*/

namespace {
struct WinState
{
  bool isLoss;
  int distanceInMoves;
  loc_t mostAdvancedRabbit;

  bool operator<(const WinState& other) const
  {
    if(isLoss < other.isLoss) return true;
    else if(other.isLoss < isLoss) return false;
    else if(distanceInMoves < other.distanceInMoves) return true;
    else if(other.distanceInMoves < distanceInMoves) return false;
    else if(mostAdvancedRabbit < other.mostAdvancedRabbit) return true;
    else if(other.mostAdvancedRabbit < mostAdvancedRabbit) return false;
    else return false;
  }
};
}

int MainFuncs::findAndSortGoals(int argc, const char* const *argv)
{
  const char* usage =
      "file outprefix";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 3)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  SearchParams params;
  params.mainHashExp = 18;
  params.fullMoveHashExp = 17;
  params.rootMoveFeatureSet = ArimaaFeatureSet();
  Searcher searcher(params);

  map<WinState,ofstream*> winFiles;

  vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[1]);
  string outPrefix = mainCommand[2];

  int numGames = games.size();
  for(int g = 0; g<numGames; g++)
  {
    cout << "Game " << g << "/" << numGames << endl;

    const GameRecord& game = games[g];
    BoardHistory hist(game);
    DEBUGASSERT(hist.maxTurnNumber == hist.maxTurnBoardNumber);
    for(int j = hist.minTurnNumber; j<hist.maxTurnNumber; j++)
    {
      const Board& turnBoard = hist.turnBoard[j];
      move_t turnMove = hist.turnMove[j];
      int ns = numStepsInMove(turnMove);
      for(int i = 0; i<ns; i++)
      {
        Board brd = turnBoard;
        for(int s = 0; s<i+1; s++)
          brd.makeStep(getStep(turnMove,s));

        if(brd.getWinner() != NPLA)
          continue;

        for(int fakeStep = 0; fakeStep<4; fakeStep++)
        {
          for(int fakePla = 0; fakePla <= 1; fakePla++)
          {
            Board b = brd;
            b.setPlaStep(fakePla,fakeStep);
            b.refreshStartHash();
            BoardHistory subHist = BoardHistory(b);

            searcher.searchID(b,subHist,4-b.step,0);
            eval_t eval = searcher.stats.finalEval;
            if(!SearchUtils::isTerminalEval(eval))
              continue;
            move_t bestMove = searcher.getMove();

            WinState state;
            int distance;
            pla_t winner;
            if(eval < 0)
            {
              state.isLoss = true;
              winner = gOpp(b.player);
              distance = eval - Eval::LOSE + b.step;
            }
            else
            {
              state.isLoss = false;
              winner = b.player;
              distance = Eval::WIN - eval + b.step;
            }

            if(distance <= 0 || distance > 20)
              continue;

            state.distanceInMoves = (distance+3)/4;

            int rabLoc = ERRLOC;
            int ystart, yend, yinc;
            bool done = false;
            if(winner == GOLD)
            {ystart = 7; yend = -1; yinc = -1;}
            else
            {ystart = 0; yend = 8; yinc = 1;}
            for(int y = ystart; y != yend && !done; y+=yinc)
            {
              for(int x = 0; x<4 && !done; x++)
              {
                loc_t loc = gLoc(x,y);
                loc_t rLoc = loc;
                if(b.owners[loc] == winner && b.pieces[loc] == RAB)
                {
                  if(rabLoc == ERRLOC)
                    rabLoc = rLoc;
                  Board copy = b;
                  if(BoardTrees::goalDist(copy,winner,4,loc) < 5)
                  {rabLoc = rLoc; done = true; break;}
                }
                loc = gLoc(7-x,y);
                if(b.owners[loc] == winner && b.pieces[loc] == RAB)
                {
                  if(rabLoc == ERRLOC)
                    rabLoc = rLoc;
                  Board copy = b;
                  if(BoardTrees::goalDist(copy,winner,4,loc) < 5)
                  {rabLoc = rLoc; done = true; break;}
                }
              }
            }
            if(rabLoc == ERRLOC)
              continue;

            if(winner == GOLD)
              rabLoc = (7-rabLoc/8)*8 + (rabLoc%8);

            state.mostAdvancedRabbit = rabLoc;

            //If no file for it yet, open it
            if(winFiles.find(state) == winFiles.end())
            {
              string fname;
              fname += outPrefix;
              fname += state.isLoss ? "lose" : "win";
              fname += Global::intToString(state.distanceInMoves);
              fname += "_" + Board::writeLoc(state.mostAdvancedRabbit);
              fname += ".txt";
              ofstream* file = new ofstream(fname.c_str(),ofstream::out | ofstream::app);
              winFiles[state] = file;
            }

            ofstream& file = *(winFiles[state]);
            file << "#" << i << endl;
            file << "GOALMOVE = " << Board::writeMove(b,bestMove) << endl;
            file << b << endl;
            file << ";" << endl;
          }
        }
      }
    }
  }

  for(map<WinState,ofstream*>::iterator it = winFiles.begin(); it != winFiles.end(); ++it)
  {
    it->second->close();
    delete it->second;
  }
  return EXIT_SUCCESS;
}

namespace {
struct IntByStep
{
  int d[4];
  IntByStep() {for(int i = 0; i<4; i++) d[i] = 0;}
};
}

/*
int MainFuncs::testGoalLoseInOnePatterns(int argc, const char* const *argv)
{
  const char* usage =
      "-patterns (file to load patterns from) ";
  const char* required = "patterns";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "patterns";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
    return EXIT_FAILURE;

  istringstream filesIn(flags["patterns"]);
  string file;
  vector<PatternRecord> loadedPatterns; //gold to move and lose
  while(filesIn.good())
  {
    filesIn >> file;
    if(filesIn.bad() || file.length() <= 0)
      break;
    vector<PatternRecord> filePatterns = ArimaaIO::readPatternFile(file);
    loadedPatterns.insert(loadedPatterns.end(),filePatterns.begin(),filePatterns.end());
  }

  vector<pair<PatternRecord,int> > goldPatterns;
  set<string> patternNames;
  int psize = loadedPatterns.size();
  for(int i = 0; i<psize; i++)
  {
    if(loadedPatterns[i].keyValues.find("NAME") == loadedPatterns[i].keyValues.end())
      Global::fatalError("Goal pattern without NAME keyvalue");
    if(loadedPatterns[i].keyValues.find("STEPS_LEFT") == loadedPatterns[i].keyValues.end())
      Global::fatalError("Goal pattern without STEPS_LEFT keyvalue");
    int step = 4-Global::stringToInt(loadedPatterns[i].keyValues["STEPS_LEFT"]);
    goldPatterns.push_back(make_pair(loadedPatterns[i],step));
    patternNames.insert(loadedPatterns[i].keyValues["NAME"]);
  }

  psize = goldPatterns.size();
  for(int i = 0; i<psize; i++)
  {
    PatternRecord& r = goldPatterns[i].first;
    goldPatterns.push_back(make_pair(PatternRecord(r.pattern.lrFlip(),r.keyValues),goldPatterns[i].second));
  }

  vector<pair<PatternRecord,int> > silvPatterns;
  psize = goldPatterns.size();
  for(int i = 0; i<psize; i++)
  {
    PatternRecord& r = goldPatterns[i].first;
    silvPatterns.push_back(make_pair(PatternRecord(r.pattern.udColorFlip(),r.keyValues),goldPatterns[i].second));
  }

  vector<BoardRecord> boardRecords = ArimaaIO::readBoardRecordFile(mainCommand[1]);
  int numBoards = boardRecords.size();
  int numTotal = 0;
  int numMatches = 0;
  int numNameMatches = 0;
  int numTotalByStep[4] = {0,0,0,0};
  int numMatchesByStep[4] = {0,0,0,0};
  map<string,int> numMatchesByName;
  map<string,int> numUniqueMatchesByName;
  map<string,IntByStep> numMatchesByNameByStep;
  map<string,IntByStep> numUniqueMatchesByNameByStep;
  cout << "Loaded " << numBoards << " boards..." << endl;
  for(int i = 0; i<numBoards; i++)
  {
    if(i > 0 && i%100000 == 0)
      cout << "Iterating " << i << endl;
    const Board& b = boardRecords[i].board;
    vector<pair<PatternRecord,int> >& patterns = (b.player == GOLD) ? goldPatterns : silvPatterns;
    psize = patterns.size();

    numTotal++;
    numTotalByStep[b.step]++;

    int matchCount = 0;
    int uniqueJ = 0;
    for(int j = 0; j<psize; j++)
    {
      if(patterns[j].second == b.step)
      {
        if(patterns[j].first.pattern.matches(b))
        {
          matchCount++;
          numNameMatches++;
          uniqueJ = j;
          numMatchesByName[patterns[j].first.keyValues["NAME"]]++;
          numMatchesByNameByStep[patterns[j].first.keyValues["NAME"]].d[b.step]++;
          //cout << "MATCH!" << endl << b << endl << patterns[j].first << endl;
          //break;
        }
      }
    }
    if(matchCount > 0)
    {
      numMatches++;
      numMatchesByStep[b.step]++;
      if(matchCount == 1)
      {
        numUniqueMatchesByName[patterns[uniqueJ].first.keyValues["NAME"]]++;
        numUniqueMatchesByNameByStep[patterns[uniqueJ].first.keyValues["NAME"]].d[b.step]++;
      }
    }
    else
    {
      //if(b.step == 0 && Rand::rand.nextUInt(15) == 0)
      //  cout << b << endl;
    }
  }

  cout << "Total " << numMatches << "/" << numTotal << endl;
  cout << "Matches by step" << endl;
  for(int i = 0; i<4; i++)
    cout << "Step " << i << " " << numMatchesByStep[i] << "/" << numTotalByStep[i] << endl;
  cout << "Matches by name (" << numNameMatches << " total)" << endl;
  for(set<string>::iterator iter = patternNames.begin(); iter != patternNames.end(); ++iter)
    cout << *iter << " "
    << numMatchesByName[*iter] << " ("
    << numMatchesByNameByStep[*iter].d[0] << " "
    << numMatchesByNameByStep[*iter].d[1] << " "
    << numMatchesByNameByStep[*iter].d[2] << " "
    << numMatchesByNameByStep[*iter].d[3] << ")"
    << " unique "
    << numUniqueMatchesByName[*iter] << " ("
    << numUniqueMatchesByNameByStep[*iter].d[0] << " "
    << numUniqueMatchesByNameByStep[*iter].d[1] << " "
    << numUniqueMatchesByNameByStep[*iter].d[2] << " "
    << numUniqueMatchesByNameByStep[*iter].d[3] << ")"
    << endl;

  return EXIT_SUCCESS;
}

// 8  9 10 11 12 13 14 15
// 0  1  2  3  4  5  6  7

int MainFuncs::verifyGoalPatterns(int argc, const char* const *argv)
{
  const char* usage =
      "<-verbosity level> ";
  const char* required = "";
  const char* allowed = "verbosity";
  const char* empty = "";
  const char* nonempty = "verbosity";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  vector<PatternRecord> records = ArimaaIO::readPatternFile(mainCommand[1]);

  int verbosity = Command::getInt(flags,"verbosity",2);

  //for(int i = 0; i<(int)records.size(); i++)
  //{
  //  cout << records[i] << endl;
  //  cout << ";" << endl;
  //}
  //return EXIT_SUCCESS;

  //cout << records[0].pattern << endl;
  //KB kb(records[0].pattern);
  //cout << kb.canDefinitelyGoal(kb,SILV,4,verbosity) << endl;
  //return EXIT_SUCCESS;

  for(int i = 0; i<(int)records.size(); i++)
  {
    cout << "CHECKING " << i << ", Name: " << records[i].keyValues["NAME"] << " Steps Left: " << records[i].keyValues["STEPS_LEFT"] << endl;

    int steps = 5;
    if(records[i].keyValues["STEPS_LEFT"] == string("0"))
      steps = 0;
    if(records[i].keyValues["STEPS_LEFT"] == string("1"))
      steps = 1;
    else if(records[i].keyValues["STEPS_LEFT"] == string("2"))
      steps = 2;
    else if(records[i].keyValues["STEPS_LEFT"] == string("3"))
      steps = 3;
    else if(records[i].keyValues["STEPS_LEFT"] == string("4"))
      steps = 4;
    if(steps <= 4)
    {
      cout << "CHECKING " << i << ", Name: " << records[i].keyValues["NAME"] << " Steps Left: " << records[i].keyValues["STEPS_LEFT"] << endl;
      cout << records[i].pattern << endl;
      Pattern pattern = records[i].pattern;
      pattern.simplifyGoalPattern(true);
      KB kb(pattern);
      if(KB::canMaybeStopGoal(kb,GOLD,steps,verbosity))
      {
        cout << "----Goal stopped----------------------------" << endl;
        //kb2.maybeCanStopGoal(GOLD,steps,2);
        //cout << "CHECKING " << i << ", Name: " << records[i].keyValues["NAME"] << " Steps Left: " << records[i].keyValues["STEPS_LEFT"] << endl;
        //cout << records[i].pattern << endl;
        //cout << "----GOAL STOPPED----------------------------" << endl;
        //break;
      }
      else
      {
        cout << "----Goal NOT stopped---------------------------" << endl;

      }
    }
  }
  cout << "DONE" << endl;
  return EXIT_SUCCESS;
}

int MainFuncs::genGoalPatterns(int argc, const char* const *argv)
{
  const char* usage =
      "-stepsleft (steps)"
      "-out (output file)"
      "<-reps num of reps to test> "
      "<-verbosity level> ";
  const char* required = "stepsleft out";
  const char* allowed = "reps verbosity";
  const char* empty = "";
  const char* nonempty = "stepsleft reps out verbosity";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  vector<PatternRecord> records = ArimaaIO::readPatternFile(mainCommand[1]);
  int stepsLeft = Global::stringToInt(flags["stepsleft"]);
  string outFile = flags["out"];
  int repsPerCond = Command::getInt(flags,"reps",0);
  int printVerbosity = Command::getInt(flags,"verbosity",1);

  ofstream out;
  out.open(outFile.c_str(),ios::out);

  uint64_t seed = 0x298af932ULL;
  for(int i = 0; i<(int)records.size(); i++)
  {
    cout << "----------------------------------------------------------------------" << endl;
    cout << "Generating: " << i << ", Name: " << records[i].keyValues["NAME"] << " Stepsleft: " << stepsLeft << " SEED " << seed << endl;
    loc_t center = Board::readLoc(records[i].keyValues["CENTER"]);

    PatternRecord outPatternRecord;
    bool result = GenGoalPatterns::genGoalPattern(records[i].pattern, seed, stepsLeft,
        center, repsPerCond, printVerbosity, outPatternRecord.pattern);
    if(!result)
    {
      cout << "Pattern gen failed!" << endl << outPatternRecord.pattern << endl;
      continue;
    }
    outPatternRecord.keyValues["NAME"] = records[i].keyValues["NAME"];
    outPatternRecord.keyValues["STEPS_LEFT"] = Global::intToString(stepsLeft);
    outPatternRecord.keyValues["CENTER"] = records[i].keyValues["CENTER"];
    outPatternRecord.keyValues["REPS"] = Global::intToString(repsPerCond);
    outPatternRecord.keyValues["SEED"] = Global::uint64ToHexString(seed);
    outPatternRecord.keyValues["WL"] = "-8";
    out << outPatternRecord << endl << ";" << endl;
  }
  out.close();

  cout << "DONE" << endl;
  return EXIT_SUCCESS;
}


int MainFuncs::buildGoalTree(int argc, const char* const *argv)
{
  const char* usage =
      "[input file] [input file] ... "
      "-out (output file)";
  const char* required = "out";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "out";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  vector<pair<Pattern,int> > patterns;
  for(int i = 1; i<(int)mainCommand.size(); i++)
  {
    vector<PatternRecord> records = ArimaaIO::readPatternFile(mainCommand[i]);
    for(int j = 0; j<(int)records.size(); j++)
    {
      patterns.push_back(make_pair(records[j].pattern,1));
    }
  }

  DecisionTree tree;
  DecisionTree::compile(tree,patterns,0);

  string outFile = flags["out"];
  ofstream out;
  out.open(outFile.c_str(),ios::out);
  out << tree << endl;
  out.close();

  int pathLengthSum = 0;
  Rand rand(10923113);
  for(int i = 0; i<1000; i++)
    pathLengthSum += tree.randomPathLength(rand);

  cout << "Number of patterns: " << patterns.size() << endl;
  cout << "Number of nodes: " << tree.getSize() << endl;
  cout << "Nodes/pattern: " << (double)tree.getSize()/patterns.size() << endl;
  cout << "Average random path length: " << (double)pathLengthSum / 1000 << endl;
  cout << "DONE" << endl;
  return EXIT_SUCCESS;
}

*/

static void printGoalInTwoBoard(int count, int game, int numGames, const Board& b, move_t move)
{
  cout << "#" << count << endl;
  cout << "#Game " << game << "/" << numGames << endl;
  cout << "#MOVE = " << Board::writeMove(b, move) << endl;
  cout << b << endl;
  cout << ";" << endl;
  cout << endl;
}

static void printGoalInThreeBoard(int count, int game, int numGames, const Board& b, move_t move, double searchTime, const GameRecord& record)
{
  cout << "#" << count << endl;
  cout << "#Game " << game << "/" << numGames << endl;
  cout << "#MOVE = " << Board::writeMove(b, move) << endl;
  cout << "#SEARCHTIME = " << searchTime << endl;
  cout << GameRecord::writeKeyValues(record) << endl;
  cout << b << endl;
  cout << ";" << endl;
  cout << endl;
}

int MainFuncs::findActualGoalsInTwo(int argc, const char* const *argv)
{
  const char* usage =
      "[games file] "
      "<-startIdx (starting game index)>"
      "<-maxPerGame (max number of positions to include per game)>"
      "<-slow using a searcher rather than a fast check>";
  const char* required = "";
  const char* allowed = "startIdx maxPerGame slow";
  const char* empty = "slow";
  const char* nonempty = "startIdx maxPerGame";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  cout << "#";
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;
  cout << "#" << Command::gitRevisionId() << endl;

  vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[1]);
  int startIdx = Command::getInt(flags,"startIdx",0);
  int maxPerGame = Command::getInt(flags,"maxPerGame",0x7FFFFFFF);

  bool fast = !Command::isSet(flags,"slow");
  ExistsHashTable* fullMoveHash = NULL;
  if(!fast)
    fullMoveHash = new ExistsHashTable(21);
  vector<move_t> fullMoves;

  int numGames = games.size();
  int count = 0;
  for(int g = startIdx; g<numGames; g++)
  {
    const GameRecord& game = games[g];
    BoardHistory hist(game);
    DEBUGASSERT(hist.maxTurnNumber == hist.maxTurnBoardNumber);
    int numThisGame = 0;
    for(int j = hist.minTurnNumber+1; j<hist.maxTurnNumber; j++)
    {
      Board turnBoard = hist.turnBoard[j];
      DEBUGASSERT(turnBoard.step == 0);
      pla_t pla = turnBoard.player;
      pla_t opp = gOpp(pla);

      if(fast)
      {
        if(BoardTrees::goalDist(turnBoard,opp,4) >= 5)
          continue;
        if(BoardTrees::goalDist(turnBoard,pla,4) <= 4)
          continue;
        Board prevBoard = hist.turnBoard[j-1];
        DEBUGASSERT(prevBoard.step == 0);
        if(BoardTrees::goalDist(prevBoard,opp,4) <= 4)
          continue;
        bool forcedLoss = SearchMoveGen::definitelyForcedLossInTwo(turnBoard);
        if(!forcedLoss)
          continue;
        printGoalInTwoBoard(count,g,numGames,prevBoard,hist.turnMove[j-1]);
        count++;
      }
      else
      {
        Board copy = turnBoard;
        BoardHistory hist2(copy);

        if(BoardTrees::goalDist(copy,copy.player,4) >= 5)
        {
          fullMoves.clear();
          SearchMoveGen::genFullMoves(copy,hist2,fullMoves,4,true,4,fullMoveHash,NULL, NULL);
          int num = fullMoves.size();
          UndoMoveData udata;
          move_t found = ERRMOVE;
          bool anyDirectWinFound = false;
          for(int i = 0; i<num; i++)
          {
            bool suc = copy.makeMoveLegalNoUndo(fullMoves[i],udata);
            DEBUGASSERT(suc);
            if(found == ERRMOVE)
            {
              bool forcedLoss = SearchMoveGen::definitelyForcedLossInTwo(copy);
              if(forcedLoss)
                found = fullMoves[i];
            }
            if(copy.getWinner() == gOpp(copy.player))
              anyDirectWinFound = true;
            copy.undoMove(udata);
          }
          if(!anyDirectWinFound && found != ERRMOVE)
          {
            printGoalInTwoBoard(count,g,numGames,copy,found);
            count++;
            numThisGame++;
            if(numThisGame > maxPerGame)
              break;
          }
        }

        copy = turnBoard;
        copy.setPlaStep(gOpp(turnBoard.player),0);
        copy.refreshStartHash();
        BoardHistory hist3(copy);
        if(BoardTrees::goalDist(copy,copy.player,4) >= 5)
        {
          fullMoves.clear();
          SearchMoveGen::genFullMoves(copy,hist3,fullMoves,4,true,4,fullMoveHash,NULL, NULL);
          int num = fullMoves.size();
          UndoMoveData udata;
          move_t found = ERRMOVE;
          bool anyDirectWinFound = false;
          for(int i = 0; i<num; i++)
          {
            bool suc = copy.makeMoveLegalNoUndo(fullMoves[i],udata);
            DEBUGASSERT(suc);
            if(found == ERRMOVE)
            {
              bool forcedLoss = SearchMoveGen::definitelyForcedLossInTwo(copy);
              if(forcedLoss)
                found = fullMoves[i];
            }
            if(copy.getWinner() == gOpp(copy.player))
              anyDirectWinFound = true;
            copy.undoMove(udata);
          }
          if(!anyDirectWinFound && found != ERRMOVE)
          {
            printGoalInTwoBoard(count,g,numGames,copy,found);
            count++;
            numThisGame++;
            if(numThisGame > maxPerGame)
              break;
          }
        }
      }
    }
  }

  if(fullMoveHash != NULL)
    delete fullMoveHash;

  return EXIT_SUCCESS;
}

int MainFuncs::findActualGoalsInThree(int argc, const char* const *argv)
{
  const char* usage =
      "[games file] "
      "<-startIdx (starting game index)>"
      "<-maxPerGame (max number of positions to include per game)>";
  const char* required = "";
  const char* allowed = "startIdx maxPerGame slow";
  const char* empty = "slow";
  const char* nonempty = "startIdx maxPerGame";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);

  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  cout << "#";
  for(int i = 0; i<argc; i++)
    cout << argv[i] << " ";
  cout << endl;
  cout << "#" << Command::gitRevisionId() << endl;

  vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[1]);
  int startIdx = Command::getInt(flags,"startIdx",0);
  int maxPerGame = Command::getInt(flags,"maxPerGame",0x7FFFFFFF);
  bool slow = Command::isSet(flags,"slow");

  SearchParams params;
  params.setMoveImmediatelyIfOnlyOneNonlosing(false);
  BradleyTerry learner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(learner);
  params.setRootFancyPrune(true);
  params.setMoveImmediatelyIfOnlyOneNonlosing(true);
  params.setUnsafePruneEnable(true);
  params.setNullMoveEnable(true);
  Searcher searcher(params);

  int numGames = games.size();
  int count = 0;
  for(int g = startIdx; g<numGames; g++)
  {
    cout << "#GAME" << g << endl;
    const GameRecord& game = games[g];
    BoardHistory hist(game);
    DEBUGASSERT(hist.maxTurnNumber == hist.maxTurnBoardNumber);
    int numThisGame = 0;
    bool detectedDirectly = false;
    for(int j = hist.minTurnNumber+1; j<hist.maxTurnNumber; j++)
    {
      Board turnBoard = hist.turnBoard[j];
      DEBUGASSERT(turnBoard.step == 0);

      BoardHistory hist2(turnBoard);
      if(slow)
        searcher.searchID(turnBoard,hist2,12,40.0,false);
      else
        searcher.searchID(turnBoard,hist2,8,20.0,false);

      //Win in 3 detected directly
      if(searcher.stats.finalEval == Eval::WIN - 12)
      {
        detectedDirectly = true;
        printGoalInThreeBoard(count,g,numGames,turnBoard,searcher.getMove(),searcher.stats.timeTaken,game);
        count++;
        numThisGame++;
        if(numThisGame > maxPerGame)
          break;
        continue;
      }

      //Don't check the previous one if it was detected directly on the previous iter
      if(detectedDirectly)
      {
        detectedDirectly = false;
        continue;
      }
      detectedDirectly = false;

      //Lost in 2
      if(searcher.stats.finalEval != Eval::LOSE + 8)
        continue;
      if(j-1 < hist.minTurnNumber)
        continue;

      //Verify that the previous position is a win in 3 as opposed to the player playing a suboptimal attack
      Board prev = hist.turnBoard[j-1];
      BoardHistory hist3(prev);
      searcher.searchID(prev,hist3,12,40.0,false);

      if(searcher.stats.finalEval == Eval::WIN - 12)
      {
        printGoalInThreeBoard(count,g,numGames,prev,searcher.getMove(),searcher.stats.timeTaken,game);
        count++;
        numThisGame++;
        if(numThisGame > maxPerGame)
          break;
        continue;
      }
    }
  }

  return EXIT_SUCCESS;
}
