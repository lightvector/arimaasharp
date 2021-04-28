/*
 * maintacticrel.cpp
 * Author: davidwu
 */

#include <fstream>
#include <set>
#include "../core/global.h"
#include "../core/rand.h"
#include "../core/timer.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/featuremove.h"
#include "../learning/gameiterator.h"
#include "../search/search.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"


int MainFuncs::testTactics(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile "
      "<-output file>"
      "<-depth depth>"
      "<-minrating rating>"
      "<-poskeepprop prop>"
      "<-randseed seed>"
      "<-maxnonreltactics n>";
  const char* required = "";
  const char* allowed = "minrating output depth idx turn poskeepprop randseed maxnonreltactics";
  const char* empty = "";
  const char* nonempty = "minrating output depth idx turn poskeepprop randseed maxnonreltactics";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 2)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  ClockTimer timer;

  string infile = mainCommand[1];
  int minRating = Command::getInt(flags,"minrating",0);
  double posKeepProp = Command::getDouble(flags,"poskeepprop",1.0);
  int depth = Command::getInt(flags,"depth",5);
  int maxNonRelTactics = Command::getInt(flags,"maxnonreltactics",0);

  ofstream* out = NULL;
  if(Command::isSet(flags,"output"))
  {
    string outfile = Command::getString(flags,"output",string());
    out = new ofstream(outfile.c_str());
  }

  int targetIdx = Command::getInt(flags,"idx",-1);
  int targetTurn = Command::isSet(flags,"turn") ? Board::readPlaTurnOrInt(flags["turn"]) : -1;
  DEBUGASSERT((targetIdx != -1) == (targetTurn != -1));
  bool verbose = targetIdx == -1;

  if(verbose)
    cout << infile << " " << Command::gitRevisionId() << endl;

  GameIterator iter(infile);
  iter.setDoFilter(true);
  iter.setDoFilterWins(true);
  iter.setDoFilterLemmings(true);
  iter.setDoFilterWinsInTwo(false);
  iter.setNumLoserToFilter(2);
  iter.setMinRating(minRating);
  iter.setDoPrint(false);
  iter.setPosKeepProp(posKeepProp);
  if(Command::isSet(flags,"randseed"))
    iter.setRandSeed(Command::getUInt64(flags,"randseed"));
  if(verbose)
    cout << "Loaded " << iter.getTotalNumGames() << " games" << endl;

  vector<Board> boards;
  vector<move_t> moves;
  vector<int> idxs;
  while(iter.next())
  {
    const Board& b = iter.getBoard();
    move_t afterMove = iter.getNextMove();
    if(afterMove == ERRMOVE)
      continue;
    if(targetIdx != -1  && (iter.getGameIdx() != targetIdx || b.turnNumber != targetTurn))
      continue;
    boards.push_back(b);
    moves.push_back(afterMove);
    idxs.push_back(iter.getGameIdx());
  }
  if(verbose)
    cout << "Running on " << boards.size() << " positions" << endl;

  SearchParams params;
  BradleyTerry rootLearner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(rootLearner);
  params.setDefaultMaxDepthTime(depth,SearchParams::AUTO_TIME);
  params.setRootFancyPrune(true);
  params.setUnsafePruneEnable(true);
  params.setNullMoveEnable(false);
  Searcher searcher(params);

  uint64_t sumAfterValue = 0;
  uint64_t sumTacticsValue = 0;
  uint64_t sumTacticGap = 0;
  uint64_t sumTacticMoves = 0;
  uint64_t sumTotalMoves = 0;
  uint64_t count = 0;

  bool first = true;
  for(int i = 0; i<(int)boards.size(); i++)
  {
    const Board& b = boards[i];
    move_t afterMove = moves[i];

    Board after(b);
    bool suc = after.makeMoveLegalNoUndo(afterMove);
    DEBUGASSERT(suc);
    BoardHistory afterhist(after);
    searcher.searchID(after,afterhist,false);
    eval_t afterEval = -searcher.stats.finalEval;

    Board passed(b);
    suc = passed.makeMoveLegalNoUndo(QPASSMOVE);
    DEBUGASSERT(suc);
    BoardHistory passedHist(passed);
    searcher.searchID(passed,passedHist,false);
    eval_t passedEval = -searcher.stats.finalEval;

    Board tacticsPre(b);
    BoardHistory tacticsPreHist(tacticsPre);
    searcher.params.rootMaxNonRelTactics = maxNonRelTactics;
    searcher.searchID(tacticsPre,tacticsPreHist,depth+4,SearchParams::AUTO_TIME,false);
    move_t tacticsMove = searcher.getMove();
    int numTacticsLegal = searcher.getNumRootMoves();
    vector<move_t> moves;
    searcher.getSortedRootMoves(moves);
    searcher.params.rootMaxNonRelTactics = 4;
    tacticsMove = tacticsMove == ERRMOVE ? QPASSMOVE : tacticsMove;
    numTacticsLegal++; //counting the QPASSMOVE as "legal"

    searcher.searchID(tacticsPre,tacticsPreHist,4,SearchParams::AUTO_TIME,false);
    int numTotalLegal = searcher.getNumRootMoves();
    numTotalLegal++; //counting the QPASSMOVE as "legal"

    Board tactics(b);
    suc = tactics.makeMoveLegalNoUndo(tacticsMove);
    DEBUGASSERT(suc);
    DEBUGASSERT(tactics.step == 0 && tactics.player == after.player);
    BoardHistory tacticsHist(tactics);
    searcher.searchID(tactics,tacticsHist,false);
    eval_t tacticsEval = -searcher.stats.finalEval;

    //Don't penalize if we generated the actual played move but search instability
    //caused us to think another move was better
    if(tacticsEval < afterEval)
    {
      set<hash_t> hashes;
      Board bb(b);
      UndoMoveData uData;
      for(int i = 0; i<(int)moves.size(); i++)
      {
        bool suc = bb.makeMoveLegalNoUndo(moves[i],uData);
        DEBUGASSERT(suc);
        hashes.insert(bb.sitCurrentHash);
        bb.undoMove(uData);
      }
      bb.makeMoveLegalNoUndo(afterMove);
      if(hashes.find(bb.sitCurrentHash) != hashes.end())
        tacticsEval = afterEval;
    }

    afterEval = afterEval < passedEval ? passedEval : afterEval;
    tacticsEval = tacticsEval < passedEval ? passedEval : tacticsEval;

    eval_t afterDiff = afterEval - passedEval;
    if(afterDiff > 6000) afterDiff = 6000;
    if(afterDiff < -6000) afterDiff = -6000;

    eval_t tacticsDiff = tacticsEval - passedEval;
    if(tacticsDiff > 6000) tacticsDiff = 6000;
    if(tacticsDiff < -6000) tacticsDiff = -6000;

    eval_t diffDiff = afterEval - tacticsEval;
    if(diffDiff > 6000) diffDiff = 6000;
    if(diffDiff < 0) diffDiff = 0;

    sumAfterValue += afterDiff;
    sumTacticsValue += tacticsDiff;
    sumTacticGap += diffDiff;
    sumTacticMoves += numTacticsLegal;
    sumTotalMoves += numTotalLegal;
    count++;

    string turnStr = Board::writePlaTurn(b.player,b.turnNumber);
    string afterMoveStr = Board::writeMove(b,afterMove);
    string tacticsMoveStr = Board::writeMove(b,tacticsMove);
    if(first && out)
    {
      *out
      << "gidx" << "," << "turn" << ","
      << "peval" << "," << "aeval" << "," << "teval" << ","
      << "adiff" << "," << "tdiff" << ","
      << "dd" << ","
      << "numt" << "," << "total" << ","
      << "amove" << "," << "tmove"
      << endl;
      first = false;
    }
    if(out)
      *out
      << idxs[i] << "," << b.turnNumber << ","
      << passedEval << "," << afterEval << "," << tacticsEval << ","
      << afterDiff << "," << tacticsDiff << ","
      << diffDiff << ","
      << numTacticsLegal << "," << numTotalLegal << ","
      << afterMoveStr << "," << tacticsMoveStr
      << endl;

    printf("%5d %4s %7d %7d %7d # %5d %5d # %5d # %5d %5d # %s # %s",
        idxs[i], turnStr.c_str(),
        passedEval, afterEval, tacticsEval,
        afterDiff, tacticsDiff,
        diffDiff,
        numTacticsLegal, numTotalLegal,
        afterMoveStr.c_str(), tacticsMoveStr.c_str()
    );
    cout << endl;
  }

  if(verbose)
  {
    cout << "instances " << count << endl;
    cout << "avg value of move " << (double)sumAfterValue/(double)count << endl;
    cout << "avg value of tactics gen " << (double)sumTacticsValue/(double)count << endl;
    cout << "avg value missed by tactic gen " << (double)sumTacticGap/(double)count << endl;
    cout << "avg tactic branchy " << (double)sumTacticMoves/(double)count << endl;
    cout << "total branchy " << (double)sumTotalMoves/(double)count << endl;

    double time = timer.getSeconds();
    cout << "Time taken: " << time << endl;
  }

  delete out;

  return EXIT_SUCCESS;
}

