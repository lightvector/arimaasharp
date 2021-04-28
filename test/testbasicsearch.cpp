/*
 * testbasicsearch.cpp
 * Author: davidwu
 */

#include "../board/btypes.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../learning/learner.h"
#include "../learning/featuremove.h"
#include "../search/search.h"
#include "../search/searchparams.h"
#include "../search/searchutils.h"
#include "../test/tests.h"

void Tests::testBasicSearch()
{
  SearchParams params;
  BradleyTerry rootLearner = BradleyTerry::inputFromDefault(MoveFeature::getArimaaFeatureSet());
  params.initRootMoveFeatures(rootLearner);
  params.setRootFancyPrune(true);
  params.setUnsafePruneEnable(true);
  params.setNullMoveEnable(false);
  Searcher searcher(params);

  cout << "Testing..." << endl;

  Board b;
  BoardHistory hist;

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*..*.."
    "........"
    "........"
    "..*..*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(searcher.getMove() == ERRMOVE);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*.E*.."
    "........"
    "........"
    "..*.e*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(searcher.getMove() == ERRMOVE);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*.E*.."
    "........"
    "........"
    "..*Re*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(searcher.getMove() == ERRMOVE);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*rE*.."
    "........"
    "........"
    "..*.e*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(searcher.getMove() == ERRMOVE);

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*..*.."
    "........"
    "R..r...."
    "..*..*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(searcher.getMove() == Board::readMove("Ra4n Ra5n Ra6n Ra7n"));
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));


  b = Board::read(
    "P = 1, S = 0\n"
    ".r......"
    "Rr......"
    "..*..*.."
    "........"
    "........"
    "..*..*.."
    "...r...."
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(searcher.getMove() == Board::readMove("Ra7n pass"));
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*..*.."
    "r......r"
    "R......R"
    "..*..*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(SearchUtils::isLoseEval(searcher.stats.finalEval));


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    ".....RcR"
    "..*..*R."
    ".....CdR"
    "......C."
    "..*..*dR"
    "r......."
    ".......D"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(searcher.getMove() == Board::readMove("Dh1n Rh3n Rh5n Rh7n"));
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));

  b = Board::read(
    "P = 0, S = 0\n"
    "r......."
    "R....RcR"
    "..*..*R."
    ".....CdR"
    "......C."
    "h.*h.*dR"
    "........"
    "D....e.D"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(
     searcher.getMove() == Board::readMove("ef1e ha3n ha4n ha5n")
  || searcher.getMove() == Board::readMove("ha3n ef1e ha4n ha5n")
  || searcher.getMove() == Board::readMove("ha3n ha4n ef1e ha5n")
  || searcher.getMove() == Board::readMove("ha3n ha4n ha5n ef1e")
  );
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));

  b = Board::read(
    "P = 0, S = 0\n"
    ".......r"
    ".....d.R"
    "r.*.cER."
    ".....h.."
    "..r.cDhM"
    "..*..*.."
    "........"
    ".d.m...."
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(
     searcher.getMove() == Board::readMove("rc4e ce4n ce6n df7e")
  );
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));

  b = Board::read(
    "P = 0, S = 0\n"
    "........"
    "e......."
    "R.*..*.."
    "........"
    "....E..."
    "..*.r*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(
     searcher.getMove() == Board::readMove("Ra6e ea7s Rb6e Rc6x Ea6e")
  );
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    "..*..*.."
    "..h....."
    ".dr..e.."
    "..*.CRd."
    ".....c.."
    "..C....."
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(
     searcher.getMove() == Board::readMove("Ce3w Cd3n rc4s rc3x Cd4w")
  );
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "........"
    ".C*..*.."
    "..r.r..."
    "..RrR..."
    "..e..*.."
    ".Cr.r..."
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(
     searcher.getMove() == Board::readMove("rc2s ec3x Cb2e Cc2e rc1n")
  );
  assert(SearchUtils::isWinEval(searcher.stats.finalEval));


  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..c....."
    "rRRd.*.."
    "........"
    "........"
    ".....*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(
     searcher.getMove() == Board::readMove("Rb6n Rc6x pass")
  );
  assert(SearchUtils::isLoseEval(searcher.stats.finalEval));

  b = Board::read(
    "P = 1, S = 0\n"
    "........"
    "..e....."
    "hHRr.*.."
    ".h......"
    "........"
    ".....*.."
    "........"
    "........"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,false);
  assert(
     searcher.getMove() == Board::readMove("Hb6n Rc6x pass")
  );
  assert(SearchUtils::isLoseEval(searcher.stats.finalEval));

  b = Board::read(
    "P = 0, S = 0\n"
    "r.rrrrrr"
    "crdme.hc"
    ".h*..*.."
    "...E.M.."
    "......H."
    "..*..*.."
    "CCDDHR.."
    "RRRRR.RR"
    "\n"
  );
  hist.reset(b);
  searcher.searchID(b,hist,5,100,false);
  assert(
     searcher.getMove() == Board::readMove("ee7s ee6s Mf5n Mf6x ee5e")
  );

  cout << "Success, all searches gave expected results" << endl;

}


