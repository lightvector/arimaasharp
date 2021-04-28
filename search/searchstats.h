
/*
 * searchstats.h
 * Author: davidwu
 */

#ifndef SEARCHSTATS_H_
#define SEARCHSTATS_H_

#include "../core/global.h"

using namespace std;

struct SearchStats
{
  //Basic stats
  int64_t mNodes;        //Main number of nodes searched (including leaves of main search)
  int64_t qNodes;        //Quiescence nodes added
  int64_t evalCalls;     //Number of calls to eval
  int64_t mHashCuts;     //Hash cutoffs made in internal search (not including leaves of main search)
  int64_t qHashCuts;     //Hash cutoffs made in quiescence (including leaves of main search)
  int64_t betaCuts;      //Beta cutoffs anywhere
  int64_t bestMoveCount; //Total number of times we generated and recursed on moves
  int64_t bestMoveSum;   //Total sum of the indices of the best moves (0 = hashmove, 1 = first ordinary move..)

  //Threading-related stats
  int64_t publicWorkRequests;  //Number of times a thread got public work
  int64_t publicWorkDepthSum;  //Sum of the cDepths of the nodes gotten as public work
  int64_t threadAborts;        //Number of times a thread got aborted with wasted work
  int64_t abortedBranches;     //Estimated number of branches that were wasted work due to abort

  //Statistics updated at end of search
  double timeTaken;     //Total time taken for search
  double depthReached;  //Deepest depth search finished
  eval_t finalEval;     //Final evaluation of position
  string pvString;      //Principal variation in text format
  uint64_t randSeed;    //Seed for search randomization

  SearchStats();

  friend ostream& operator<<(ostream& out, const SearchStats& stats);
  static void printBasic(ostream& out, const SearchStats& stats);
  static void printBasic(ostream& out, const SearchStats& stats, int i);

  SearchStats& operator+=(const SearchStats& rhs);

  void overwriteAggregates(const SearchStats& rhs);

};

#endif /* SEARCHSTATS_H_ */
