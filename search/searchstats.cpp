/*
 * searchstats.cpp
 * Author: davidwu
 */

#include "../search/search.h"
#include "../program/arimaaio.h"

SearchStats::SearchStats()
{
  mNodes = 0;
  qNodes = 0;
  evalCalls = 0;
  mHashCuts = 0;
  qHashCuts = 0;
  betaCuts = 0;
  bestMoveCount = 0;
  bestMoveSum = 0;
  publicWorkRequests = 0;
  publicWorkDepthSum = 0;
  threadAborts = 0;
  abortedBranches = 0;

  timeTaken = 0;
  depthReached = 0;
  finalEval = 0;
  randSeed = 0;
}

ostream& operator<<(ostream& out, const SearchStats& stats)
{
  out
  << "Depth " << stats.depthReached
  << " Time " << stats.timeTaken
  << " Eval " << ArimaaIO::writeEval(stats.finalEval)
  << " MNodes " << stats.mNodes
  << " QNodes " << stats.qNodes
  << " Evals " << stats.evalCalls
  << " BetaCut " << stats.betaCuts
  << " MHashCut " << stats.mHashCuts
  << " QHashCut " << stats.qHashCuts
  << " Ordering " << (stats.bestMoveCount == 0 ? 0 : (double)stats.bestMoveSum/stats.bestMoveCount)
  << " PubWorkReq " << stats.publicWorkRequests
  << " PubWorkAvgDepth " << (stats.publicWorkRequests == 0 ? 0 : (double)stats.publicWorkDepthSum/stats.publicWorkRequests)
  << " ThreadAborts " << stats.threadAborts
  << " AbortedBranches " << stats.abortedBranches
  << " Seed " << Global::uint64ToHexString(stats.randSeed)
  << endl
  << "PV: " << stats.pvString;

  return out;
}

void SearchStats::printBasic(ostream& out, const SearchStats& stats)
{
  out
  << stats.depthReached << "\t"
  << stats.finalEval << "\t"
  << (stats.mNodes) << "\t"
  << (stats.qNodes) << "\t"
  << (stats.evalCalls) << "\t"
  << stats.timeTaken << "\t"
  << stats.pvString;
  out << endl;
}

void SearchStats::printBasic(ostream& out, const SearchStats& stats, int i)
{
  out
  << i << "\t"
  << stats.depthReached << "\t"
  << stats.finalEval << "\t"
  << (stats.mNodes) << "\t"
  << (stats.qNodes) << "\t"
  << (stats.evalCalls) << "\t"
  << stats.timeTaken << "\t"
  << stats.pvString;
  out << endl;
}

SearchStats& SearchStats::operator+=(const SearchStats& rhs)
{
  mNodes += rhs.mNodes;
  qNodes += rhs.qNodes;
  evalCalls += rhs.evalCalls;
  mHashCuts += rhs.mHashCuts;
  qHashCuts += rhs.qHashCuts;
  betaCuts += rhs.betaCuts;
  bestMoveCount += rhs.bestMoveCount;
  bestMoveSum += rhs.bestMoveSum;
  publicWorkRequests += rhs.publicWorkRequests;
  publicWorkDepthSum += rhs.publicWorkDepthSum;
  threadAborts += rhs.threadAborts;
  abortedBranches += rhs.abortedBranches;

  return *this;
}

void SearchStats::overwriteAggregates(const SearchStats& rhs)
{
  mNodes = rhs.mNodes;
  qNodes = rhs.qNodes;
  evalCalls = rhs.evalCalls;
  mHashCuts = rhs.mHashCuts;
  qHashCuts = rhs.qHashCuts;
  betaCuts = rhs.betaCuts;
  bestMoveCount = rhs.bestMoveCount;
  bestMoveSum = rhs.bestMoveSum;
  publicWorkRequests = rhs.publicWorkRequests;
  publicWorkDepthSum = rhs.publicWorkDepthSum;
  threadAborts = rhs.threadAborts;
  abortedBranches = rhs.abortedBranches;
}


