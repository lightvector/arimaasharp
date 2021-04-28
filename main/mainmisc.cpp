
/*
 * mainmisc.cpp
 * Author: davidwu
 */

#include <fstream>
#include <cstdlib>
#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../board/gamerecord.h"
#include "../learning/gameiterator.h"
#include "../eval/eval.h"
#include "../search/search.h"
#include "../search/searchutils.h"
#include "../search/searchmovegen.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

using namespace std;

int MainFuncs::version(int argc, const char* const *argv)
{
  (void)argc;
  (void)argv;
  cout << Command::gitRevisionId() << endl;
  cout << "Compiled " << __DATE__ << " " << __TIME__ << endl;
  return EXIT_SUCCESS;
}

int MainFuncs::createEvalBadGoodTest(int argc, const char* const *argv)
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

  Rand rand;
  vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[1]);

  const char* outfileposval = "evalTestGB_posval.txt";
  ofstream outposval(outfileposval);
  if(!outposval.good())  Global::fatalError(string("Could not open outfile: ") + outfileposval);
  const char* outfilebtrand = "evalTestGB_btrand.txt";
  ofstream outbtrand(outfilebtrand);
  if(!outbtrand.good())  Global::fatalError(string("Could not open outfile: ") + outfilebtrand);
  const char* outfilebtsharp = "evalTestGB_btsharp.txt";
  ofstream outbtsharp(outfilebtsharp);
  if(!outbtsharp.good())  Global::fatalError(string("Could not open outfile: ") + outfilebtsharp);

  int numPosVal = 0;
  int numBtRand = 0;
  int numBtSharp = 0;

  Searcher searcher;
  ExistsHashTable* ehashTable = new ExistsHashTable(21);

  int numFiltered = 0;
  int numNoWeight = 0;
  int numSharpTerminal = 0;
  int numGood = 0;

  for(int i = 0; i<(int)games.size(); i++)
  {
    cout << "Game " << i << "/" << (int)games.size()
        << " Good " << numGood
        << " NoWeight " << numNoWeight
        << " Filtered " << numFiltered
        << " SharpTerminal " << numSharpTerminal
        << endl;
    GameRecord& record = games[i];
    double goldWeight = Global::stringToDouble(map_get(record.keyValues,"GOLD_WEIGHT"));
    double silvWeight = Global::stringToDouble(map_get(record.keyValues,"SILV_WEIGHT"));
    string gameInfo = record.keyValues["GAME_INFO"];

    BoardHistory recordHist(record);
    vector<bool> doFilter = GameIterator::getFiltering(record,recordHist,0,1,false,false,false,false,0);
    DEBUGASSERT(doFilter.size() == record.moves.size());

    Board b = record.board;
    bool suc;
    for(int j = 0; j<(int)record.moves.size(); j++)
    {
      BoardHistory hist = BoardHistory(b);

      move_t move = record.moves[j];
      if((b.player == GOLD && goldWeight <= 0) || (b.player == SILV && silvWeight <= 0))
      {suc = b.makeMoveLegalNoUndo(move); DEBUGASSERT(suc); numNoWeight++; continue;}

      if(doFilter[j])
      {suc = b.makeMoveLegalNoUndo(move); DEBUGASSERT(suc); numFiltered++; continue;}

      searcher.searchID(b,hist,5,100);
      eval_t sharpEval = searcher.stats.finalEval;
      move_t sharpMove = searcher.getMove();
      if(SearchUtils::isTerminalEval(sharpEval))
      {suc = b.makeMoveLegalNoUndo(move); DEBUGASSERT(suc); numSharpTerminal++; continue;}

      numGood++;

      const char* forpla = (b.player == GOLD ? "gold" : "silv");
      double weight = (b.player == GOLD ? goldWeight : silvWeight);
      if(b.player == record.winner)
        weight *= 1.3;

      Board nextB = b;
      suc = nextB.makeMoveLegalNoUndo(move);
      DEBUGASSERT(suc);

      Board sharpB = b;
      suc = sharpB.makeMoveLegalNoUndo(sharpMove);
      DEBUGASSERT(suc);
      outbtsharp << "BAD" << endl;
      outbtsharp << "FORPLA = " << forpla << endl;
      outbtsharp << "DIFF = -100" << endl;
      outbtsharp << "WEIGHT = " << weight << endl;
      outbtsharp << "IDX = " << numBtSharp++ << endl;
      if(gameInfo.length() > 0) outbtsharp << "GAME_INFO = " << gameInfo << endl;
      outbtsharp << "SHARPMOVE = " << Board::writeMove(b,sharpMove,true) << endl;
      outbtsharp << "MOVE = " << Board::writeMove(b,move,true) << endl;
      outbtsharp << "WINNER = " << Board::writePla(record.winner) << endl;
      outbtsharp << sharpB << endl;
      outbtsharp << "GOOD" << endl;
      outbtsharp << nextB << endl;
      outbtsharp << ";" << endl << endl;

      outposval << "BAD" << endl;
      outposval << "FORPLA = " << forpla << endl;
      outposval << "DIFF = 50" << endl;
      outposval << "WEIGHT = " << weight << endl;
      outposval << "IDX = " << numPosVal++ << endl;
      if(gameInfo.length() > 0) outposval << "GAME_INFO = " << gameInfo << endl;
      outposval << "MOVE = " << Board::writeMove(b,move,true) << endl;
      outposval << "WINNER = " << Board::writePla(record.winner) << endl;
      outposval << b << endl;
      outposval << "GOOD" << endl;
      outposval << nextB << endl;
      outposval << ";" << endl << endl;

      vector<move_t> allMoves;
      SearchMoveGen::genFullMoves(b,hist,allMoves,4,false,4,ehashTable,NULL, NULL);
      //8 Random moves
      for(int r = 0; r<8 && r+1 < (int)allMoves.size() / 10; r++)
      {
        int randIdx = rand.nextUInt(allMoves.size()-r)+r;
        move_t randMove = allMoves[randIdx];
        Board randB = b;
        suc = randB.makeMoveLegalNoUndo(randMove);
        DEBUGASSERT(suc);
        outbtrand << "BAD" << endl;
        outbtrand << "FORPLA = " << forpla << endl;
        outbtrand << "DIFF = -50" << endl;
        outbtrand << "WEIGHT = " << weight << endl;
        outbtrand << "IDX = " << numBtRand++ << endl;
        if(gameInfo.length() > 0) outbtrand << "GAME_INFO = " << gameInfo << endl;
        outbtrand << "RANDMOVE = " << Board::writeMove(b,randMove,true) << endl;
        outbtrand << "MOVE = " << Board::writeMove(b,move,true) << endl;
        outbtrand << "WINNER = " << Board::writePla(record.winner) << endl;
        outbtrand << randB << endl;
        outbtrand << "GOOD" << endl;
        outbtrand << nextB << endl;
        outbtrand << ";" << endl << endl;
      }

      suc = b.makeMoveLegalNoUndo(move);
      DEBUGASSERT(suc);
    }
  }

  delete ehashTable;

  return EXIT_SUCCESS;
}

int MainFuncs::randomizePosFile(int argc, const char* const *argv)
{
  const char* usage =
      "posfile outputfile";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 3)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  cout << "Loading..." << endl;
  vector<BoardRecord> boards = ArimaaIO::readBoardRecordFile(mainCommand[1]);

  string outfile = mainCommand[2];
  ofstream out(outfile.c_str());
  if(!out.good())
    Global::fatalError(string("Could not open outfile: ") + outfile);

  cout << "Randomizing..." << endl;
  Rand rand;
  for(int j = (int)boards.size() - 1; j > 0; j--)
  {
    int r = rand.nextUInt(j+1);
    BoardRecord temp = boards[j];
    boards[j] = boards[r];
    boards[r] = temp;
  }
  cout << "Outputting..." << endl;
  for(int i = 0; i<(int)boards.size(); i++)
    out << BoardRecord::write(boards[i]) << ";" << endl;
  cout << "Done" << endl;
  return EXIT_SUCCESS;
}

int MainFuncs::createBenchmark(int argc, const char* const *argv)
{
  const char* usage =
      "numdesiredboards outputfile [inputfile] [inputfile] ...";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() < 4)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  int numDesiredBoards = Global::stringToInt(mainCommand[1]);

  Rand rand;
  uint64_t seed = rand.nextUInt64();
  rand.init(seed);

  string outfile = mainCommand[2];
  ofstream out(outfile.c_str());
  if(!out.good())
    Global::fatalError(string("Could not open outfile: ") + outfile);

  int minimumTurnBound = 5;
  int numGameSets = mainCommand.size()-3;

  int numLegalBoardsSeen = 0;
  vector<Board> keptBoards;

  for(int i = 0; i<numGameSets; i++)
  {
    vector<GameRecord> games = ArimaaIO::readMovesFile(mainCommand[i+3]);

    for(int j = 0; j<(int)games.size(); j++)
    {
      GameRecord& game = games[j];
      BoardHistory hist(game);
      DEBUGASSERT(hist.maxTurnNumber == hist.maxTurnBoardNumber);
      for(int k = hist.minTurnNumber; k <= hist.maxTurnNumber; k++)
      {
        if(k < minimumTurnBound)
          continue;
        const Board& board = hist.turnBoard[k];
        if(board.getWinner() != NPLA)
          continue;

        numLegalBoardsSeen++;
        if(numLegalBoardsSeen <= numDesiredBoards)
          keptBoards.push_back(board);
        else
        {
          int randomSlot = rand.nextUInt(numLegalBoardsSeen);
          if(randomSlot < numDesiredBoards)
            keptBoards[randomSlot] = board;
        }
      }
    }
  }

  //Randomize boards
  cout << "Randomizing..." << endl;
  for(int i = numDesiredBoards-1; i>=1; i--)
  {
    int j = rand.nextUInt(i+1);
    Board temp = keptBoards[i];
    keptBoards[i] = keptBoards[j];
    keptBoards[j] = temp;
  }

  /*
  //Sort boards by turn number by selection sort
  cout << "Sorting..." << endl;
  for(int i = 0; i<numDesiredBoards; i++)
  {
    int best = i;
    for(int j = i+1; j<numDesiredBoards; j++)
    {
      if(keptBoards[j].turnNumber < keptBoards[best].turnNumber)
        best = j;
    }

    Board temp = keptBoards[best];
    keptBoards[best] = keptBoards[i];
    keptBoards[i] = temp;
  }*/

  cout << "Outputting..." << endl;

  //Output
  out << "# numDesiredBoards " << numDesiredBoards << endl;
  out << "# seed " << Global::uint64ToHexString(seed) << endl;
  for(int i = 0; i<numGameSets; i++)
    out << "# games " << mainCommand[i+3] << endl;

  for(int i = 0; i<numDesiredBoards; i++)
  {
    out << "# " << i << endl;
    out << "# " << Board::writePlaTurn(keptBoards[i].turnNumber) << endl;
    out << keptBoards[i] << endl;
    out << ";" << endl;
  }

  cout << "Done, " << numDesiredBoards << " out of " << numLegalBoardsSeen << " total" << endl;

  out.close();

  return EXIT_SUCCESS;
}

int MainFuncs::winProbByEval(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile"
      "-grid INT spacing"
      "-numposbuckets INT number of buckets above zero"
      "<-ratedonly>"
      "<-nofilter>"
      "<-minrating rating>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>"
      "<-materialonly>"
      "<-metawinner>";
  const char* required = "grid numposbuckets";
  const char* allowed = "nofilter ratedonly minrating botgameweight botposweight fancyweight metawinner materialonly";
  const char* empty = "nofilter ratedonly fancyweight metawinner materialonly";
  const char* nonempty = "grid numposbuckets minrating botgameweight botposweight";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string infile = mainCommand[1];

  bool ratedOnly = Command::isSet(flags,"ratedonly");
  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);
  bool noFilter = Command::isSet(flags,"nofilter");
  bool metaWinner = Command::isSet(flags,"metawinner");
  bool materialOnly = Command::isSet(flags,"materialonly");

  cout << infile << " " << Command::gitRevisionId() << endl;

  GameIterator iter(infile);
  if(!noFilter)
  {
    iter.setDoFilter(true);
    iter.setDoFilterWins(true);
    iter.setNumInitialToFilter(2);
    iter.setNumLoserToFilter(1);
    iter.setDoFilterManyShortMoves(true);
    iter.setMinPlaUnfilteredMoves(6);
  }

  iter.setMoveKeepBase(20);
  iter.setMinRating(minRating);
  iter.setRatedOnly(ratedOnly);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  iter.setDoPrint(false);

  //Bucket -1 goes from [-3grid/2,-grid/2)
  //Bucket  0 goes from [-grid/2,grid/2]
  //Bucket  1 goes from (grid/2,3grid/2]
  int grid = Command::getInt(flags,"grid");
  int numPosBuckets = Command::getInt(flags,"numposbuckets");
  int numBuckets = numPosBuckets * 2 + 1;
  int zeroIdx = numPosBuckets;
  int lowerBound[numBuckets];
  int upperBound[numBuckets];
  int mid[numBuckets];
  int64_t winCounts[numBuckets];
  int64_t totalCounts[numBuckets];

  for(int i = 0; i<numBuckets; i++)
  {
    winCounts[i] = 0;
    totalCounts[i] = 0;
    mid[i] = (i-zeroIdx) * grid;
    lowerBound[i] = i <= zeroIdx ? -(grid*((zeroIdx-i)*2+1)/2)     : grid*((i-zeroIdx)*2-1)/2 + 1;
    upperBound[i] = i < zeroIdx  ? -(grid*((zeroIdx-i)*2-1)/2) - 1 : grid*((i-zeroIdx)*2+1)/2;
  }
  if(lowerBound[0] > -1000000)
    lowerBound[0] = -1000000;
  if(upperBound[numBuckets-1] < 1000000)
    upperBound[numBuckets-1] = 1000000;

  while(iter.next())
  {
    Board b = iter.getBoard();
    pla_t winner = metaWinner ? iter.getMetaGameWinner() : iter.getGameWinner();
    if(winner == NPLA)
      continue;

    int eval;
    if(materialOnly)
      eval = Eval::getMaterialScore(b,b.player);
    else
      eval = Eval::evaluate(b,NPLA,0,NULL);

    int idx;
    if(eval >= -(grid/2) && eval <= grid/2)
      idx = zeroIdx;
    else if(eval > 0)
      idx = zeroIdx + (eval + (grid-1)/2)/grid;
    else
      idx = zeroIdx - (-eval + (grid-1)/2)/grid;
    if(idx < 0)
      idx = 0;
    if(idx >= numBuckets)
      idx = numBuckets - 1;

    if(winner == b.player)
      winCounts[idx]++;
    totalCounts[idx]++;
  }


  vector<string> comments = iter.getTrainingPropertiesComments();
  for(int i = 0; i<(int)comments.size(); i++)
    cout << comments[i] << endl;

  cout << "Lower\tUpper\tMid\tWins\tTotal\tProb" << endl;
  for(int i = 0; i<numBuckets; i++)
  {
    cout
    << lowerBound[i] << "\t"
    << upperBound[i] << "\t"
    << mid[i] << "\t"
    << winCounts[i] << "\t"
    << totalCounts[i] << "\t"
    << (double)winCounts[i]/totalCounts[i] << endl;
  }

  return EXIT_SUCCESS;
}
