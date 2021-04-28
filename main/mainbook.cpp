/*
 * mainbook.cpp
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
#include "../book/book.h"
#include "../learning/gameiterator.h"
#include "../program/arimaaio.h"
#include "../program/command.h"
#include "../main/main.h"

static void setBasicFiltering(GameIterator& iter)
{
  iter.setDoFilter(true);
  iter.setDoFilterLemmings(true);
  iter.setDoFilterWinsInTwo(true);
  iter.setNumInitialToFilter(0);
  iter.setNumLoserToFilter(2);
  iter.setDoFilterManyShortMoves(true);
  iter.setMinPlaUnfilteredMoves(6);
  iter.setDoFilterWins(true);
}

int MainFuncs::testBook(int argc, const char* const *argv)
{
  const char* usage =
      "movesfile posfile "
      "<-ratedonly>"
      "<-minrating rating>"
      "<-poskeepprop prop>"
      "<-botkeepprop prop>"
      "<-botgameweight weight>"
      "<-botposweight weight>"
      "<-fancyweight>";
  const char* required = "";
  const char* allowed = "ratedonly minrating poskeepprop botkeepprop botgameweight botposweight fancyweight";
  const char* empty = "ratedonly fancyweight";
  const char* nonempty = "minrating poskeepprop botkeepprop botgameweight botposweight";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  if(mainCommand.size() != 3)
  {Command::printHelp(argc,argv,usage); return EXIT_FAILURE;}

  string movesFile = mainCommand[1];
  string posFile = mainCommand[2];

  bool ratedOnly = Command::isSet(flags,"ratedonly");
  bool fancyWeight = Command::isSet(flags,"fancyweight");
  double posKeepProp = Command::getDouble(flags,"poskeepprop",1.0);
  double botKeepProp = Command::getDouble(flags,"botkeepprop",1.0);
  double botGameWeight = Command::getDouble(flags,"botgameweight",1.0);
  double botPosWeight = Command::getDouble(flags,"botposweight",1.0);
  int minRating = Command::getInt(flags,"minrating",0);

  //Learning Initialization--------------
  ClockTimer timer;

  cout << movesFile << " " << Command::gitRevisionId() << endl;

  GameIterator iter(movesFile);
  setBasicFiltering(iter);
  iter.setPosKeepProp(posKeepProp);
  iter.setMinRating(minRating);
  iter.setRatedOnly(ratedOnly);
  iter.setBotPosKeepProp(botKeepProp);
  iter.setBotGameWeight(botGameWeight);
  iter.setBotPosWeight(botPosWeight);
  iter.setDoFancyWeight(fancyWeight);
  iter.setDoPrint(true);

  vector<Board> boards = ArimaaIO::readBoardFile(posFile);
  for(int i = 0; i<(int)boards.size(); i++)
  {
    iter.reset();
    const Board& b0 = boards[i];
    static const size_t maxClosest = 10;
    vector<pair<double,Board>> closest;
    while(iter.next())
    {
      const Board& b1 = iter.getBoard();
      if(b1.turnNumber >= 20)
        continue;
      if(b1.player != b0.player)
        continue;
      double dist = Book::boardDistance(b0,b1,Bitmap::BMPONES);
      if(closest.size() < maxClosest)
        closest.push_back(make_pair(dist,b1));
      else
      {
        for(int i = 0; i<(int)closest.size(); i++)
        {
          if(dist < closest[i].first)
          {
            closest[i] = make_pair(dist,b1);
            break;
          }
        }
      }
    }

    cout << b0 << endl;
    cout << "------------------------------------------" << endl;
    for(int i = 0; i<(int)closest.size(); i++)
    {
      cout << closest[i].first << endl;
      cout << closest[i].second << endl;
    }
  }

  return EXIT_SUCCESS;
}


