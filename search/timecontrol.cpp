/*
 * timecontrol.cpp
 * Author: davidwu
 */

#include <sstream>
#include <cmath>
#include "../core/global.h"
#include "../search/searchparams.h"
#include "../search/timecontrol.h"

TimeControl::TimeControl()
{
  double autoTime = SearchParams::AUTO_TIME;
  perMove = autoTime;
  percent = 100;
  reserveStart = autoTime;
  reserveMax = autoTime;
  gameMax = autoTime;
  perMoveMax = autoTime;

  alreadyUsed = 0;
  reserveCurrent = autoTime;
  gameCurrent = 0;

  isOpponentPonder = false;
  isPlaPonder = false;
}

void TimeControl::validate() const
{
  if(perMove < 0)
    Global::fatalError("ArimaaIO: negative per-move in time control");
  if(percent < 0 || percent > 100)
    Global::fatalError("ArimaaIO: tc percent is not in [0,100]");
  if(reserveStart < 0)
    Global::fatalError("ArimaaIO: negative reserve start in time control");
  if(reserveMax < 0)
    Global::fatalError("ArimaaIO: negative reserve max in time control");
  if(gameCurrent < 0)
    Global::fatalError("ArimaaIO: negative game current in time control");
  if(gameMax < 0)
    Global::fatalError("ArimaaIO: negative game max in time control");
  if(perMoveMax < 0)
    Global::fatalError("ArimaaIO: negative per-move max in time control");
  if(alreadyUsed < 0)
    Global::fatalError("ArimaaIO: negative already used in time control");
  if(reserveCurrent < 0)
    Global::fatalError("ArimaaIO: negative reserve current in time control");
}

bool TimeControl::isValid() const
{
  if(perMove < 0)
    return false;
  if(percent < 0 || percent > 100)
    return false;
  if(reserveStart < 0)
    return false;
  if(reserveMax < 0)
    return false;
  if(gameCurrent < 0)
    return false;
  if(gameMax < 0)
    return false;
  if(perMoveMax < 0)
    return false;
  if(alreadyUsed < 0)
    return false;
  if(reserveCurrent < 0)
    return false;
  return true;
}

void TimeControl::restart()
{
  alreadyUsed = 0;
  reserveCurrent = reserveStart;
  gameCurrent = 0;
}

void TimeControl::addForPonder(double ponderTime)
{
  //Be very slightly conservative and search slightly less than you would
  //if you actually got that much of an effective boost in reserve
  //Not too much though, don't want problems like overflowing reserve.
  ponderTime *= 0.95;

  reserveCurrent += ponderTime;
  perMoveMax += ponderTime;
  gameCurrent += ponderTime;
}


//TIME POLICY---------------------------------------------------------------

//DESIRED TIME-----------------------------------------------------
//Desired amount of time in reserve:
//RESERVE_BASE + perMove * RESERVE_PERMOVE_FACTOR_DESIRED;
static const int RESERVE_BASE = 8;
static constexpr double RESERVE_PERMOVE_FACTOR_DESIRED = 1.6;

//How much time should we target using, based on the reserve level.
static constexpr double PERMOVE_PROP_MIN = 0.70;
static constexpr double PERMOVE_TO_TARGET_PER_RESERVE = 0.35;

//Add this proportion of the reserve into the target time.
//Mainly for time controls where the reserve is very large and the permove is small
static constexpr double RESERVE_TARGET_PROP = 0.010;

//MAX TIME---------------------------------------------------------

//Keep BASE_LAG_BUFFER + 1/6 of permove as a buffer against lag, up to MAX_LAG_BUFFER.
static constexpr double BASE_LAG_BUFFER = 5;
static constexpr double MAX_LAG_BUFFER = 12;

void TimeControl::getMinNormalMax(double& minTimeBuf, double& normalTimeBuf, double& maxTimeBuf) const
{
  //Check for permissive
  double autoTime = SearchParams::AUTO_TIME;
  if(perMove - alreadyUsed >= autoTime)
  {
    //Special case for ponder time controls, so that we don't spend forever on the opp's move
    if(isOpponentPonder)
      autoTime = 2.0;

    minTimeBuf = autoTime;
    normalTimeBuf = autoTime;
    maxTimeBuf = autoTime;
    return;
  }

  //Ponder time controls
  if(isOpponentPonder)
  {
    //Ponder 1 second + 1/50th of the permove
    double time = perMove * 0.02 + 1.0;
    minTimeBuf = time;
    normalTimeBuf = time;
    maxTimeBuf = time;
    return;
  }

  double reserveSecs = reserveCurrent;
  double used = alreadyUsed;
  double perMax = perMoveMax;
  double ponderExtra = 0;
  if(isPlaPonder)
  {
    //Search the amount of time that we would search if the reserve was maximized, plus a little
    reserveSecs = max(reserveCurrent,min(reserveMax,reserveCurrent + perMove*5));
    used = 0;
    perMax = SearchParams::AUTO_TIME;
    ponderExtra = perMove * 0.8;
  }

  //DESIRED TIME-----------------------------------------------------------------------

  //Figure how much time we should target for remaining in the reserve
  double targetReserveTime = RESERVE_BASE + perMove * RESERVE_PERMOVE_FACTOR_DESIRED;

  //Figure out how much time we want to target
  //For excess reserve, take a curve matching the derivative at 1 but increases strongly sublinearly,
  //so that we modestly use and don't burn up excess time.
  double reserveProp = (reserveSecs - used) / targetReserveTime;
  double curve = reserveProp <= 1.0 ? reserveProp : 4.0*pow(reserveProp, 0.25)-3.0;
  double desiredTime =
      perMove
      + perMove * (curve - 1.0)  * PERMOVE_TO_TARGET_PER_RESERVE
      + (reserveSecs - used) * RESERVE_TARGET_PROP;

  //Bound targeted time
  if(desiredTime <= perMove * PERMOVE_PROP_MIN)
    desiredTime = perMove * PERMOVE_PROP_MIN;

  //Add a little bit extra if pondering
  desiredTime += ponderExtra;

  //HARD MAX TIME------------------------------------------------------------------------

  //Make sure we won't run out of time, leaving a buffer
  double maxTimeFromTC = min(perMax, perMove + reserveSecs) - used;
  double lagBuffer = min(perMax / 4.0, min(MAX_LAG_BUFFER, BASE_LAG_BUFFER + perMove / 6.0));
  //Apply the lag buffer, but allow the buffer to soften slightly if we're actually below already
  //to try to get back above without having to do too-short searches.
  double hardMaxTime = maxTimeFromTC - lagBuffer;
  //If we're capped at 70% of the permove, make up half of the difference, but keep half of the buffer always.
  if(hardMaxTime < perMove*0.7)
    hardMaxTime += 0.5 * min(lagBuffer, perMove*0.7 - hardMaxTime);

  //HARD MIN TIME------------------------------------------------------------------------

  //Make sure we won't waste time by overflowing the reserve
  double hardMinTime = reserveMax <= 0 ? 0 : reserveSecs + perMove - used - (reserveMax - 1);

  //Bound times to something sensible
  if(hardMinTime < 0)
    hardMinTime = 0;
  if(hardMaxTime < 0.000001)
    hardMaxTime = 0.000001;
  if(hardMinTime > hardMaxTime)
    hardMinTime = hardMaxTime;
  if(desiredTime < hardMinTime)
    desiredTime = hardMinTime;
  if(desiredTime > hardMaxTime)
    desiredTime = hardMaxTime;

  minTimeBuf = hardMinTime;
  normalTimeBuf = desiredTime;
  maxTimeBuf = hardMaxTime;
}

static const int OLD_TIME_RESERVE_MIN = 8;       //Min base time we aim to leave in reserve
static const int OLD_TIME_RESERVE_PERMOVE_FACTOR = 1; //Proportion of permove that we desire in the reserve.
static constexpr double OLD_TIME_RESERVE_POS_PROP = 0.12; //Prop of reserve time we should use when above desired
static constexpr double OLD_TIME_RESERVE_NEG_PROP = 0.16; //Prop of (neg) reserve time we should use, when below desired
static constexpr double OLD_TIME_PERMOVE_PROP = 0.95; //Prop of permove time to use
static constexpr double OLD_TIME_FORCED_PROP = 0.75;  //Try to spend at least this prop of the permove.
static const int OLD_TIME_LAG_ALLOWANCE = 1;    //And then spend this many fewer seconds than we intended.
static const int OLD_PER_MOVE_MAX_ALLOWANCE = 5;  //And spend at most this many seconds fewer than perMoveMax or total time counting reserve
static const int OLD_TIME_MIN = 3;                //But spend at least this amount of time no matter what

void TimeControl::getMinNormalMaxOld(double& minTimeBuf, double& normalTimeBuf, double& maxTimeBuf) const
{
  //Check for permissive
  double autoTime = SearchParams::AUTO_TIME;
  if(perMove - alreadyUsed >= autoTime)
  {
    //Special case for ponder time controls, so that we don't spend forever on the opp's move
    if(isOpponentPonder)
      autoTime = 2.0;

    minTimeBuf = autoTime;
    normalTimeBuf = autoTime;
    maxTimeBuf = autoTime;
    return;
  }

  //Ponder time controls
  if(isOpponentPonder)
  {
    //Ponder 1 second + 1/50th of the permove
    double time = perMove * 0.02 + 1.0;
    minTimeBuf = time;
    normalTimeBuf = time;
    maxTimeBuf = time;
    return;
  }

  double reserveSecs = reserveCurrent;
  double used = alreadyUsed;
  double perMax = perMoveMax;
  double ponderExtra = 0;
  if(isPlaPonder)
  {
    //Search the amount of time that we would search if the reserve was maximized, plus a little
    reserveSecs = max(reserveCurrent,min(reserveMax,perMove*5));
    used = 0;
    perMax = SearchParams::AUTO_TIME;
    ponderExtra = perMove * 0.8;
  }
  double reserveDelta = reserveSecs - (OLD_TIME_RESERVE_PERMOVE_FACTOR * perMove + OLD_TIME_RESERVE_MIN);

  //Figure base amount out from reserve and permove
  double timeToSearch;
  if(reserveDelta < 0)
    timeToSearch = (perMove - used)*OLD_TIME_PERMOVE_PROP + reserveDelta*OLD_TIME_RESERVE_NEG_PROP;
  else
    timeToSearch = (perMove - used)*OLD_TIME_PERMOVE_PROP + reserveDelta*OLD_TIME_RESERVE_POS_PROP;

  //Try to make at least some proportion of the permove, even if low reserve
  if(timeToSearch < OLD_TIME_FORCED_PROP * perMove)
    timeToSearch = OLD_TIME_FORCED_PROP * perMove;

  //Use a little less
  timeToSearch -= OLD_TIME_LAG_ALLOWANCE;

  //Make sure we won't run out of time, leaving a buffer
  double maxTimeFromTC = min(perMax - used, perMove - used + reserveSecs);
  maxTimeFromTC -= min(perMax / 4.0, (double)OLD_PER_MOVE_MAX_ALLOWANCE);
  if(timeToSearch > maxTimeFromTC)
    timeToSearch = maxTimeFromTC;

  //But take some minimum amount of time anyways
  if(timeToSearch < OLD_TIME_MIN)
    timeToSearch = OLD_TIME_MIN;

  //Add a little bit extra if pondering
  timeToSearch += ponderExtra;

  //Make sure we won't waste time by overflowing the reserve
  double hardMinTime = reserveMax <= 0 ? 0 : reserveSecs + perMove - used - (reserveMax - 1);
  double normalTime = timeToSearch;
  double hardMaxTime = maxTimeFromTC;

  //Bound times to something sensible
  if(hardMinTime < 0)
    hardMinTime = 0;
  if(hardMaxTime < 0.000001)
    hardMaxTime = 0.000001;
  if(hardMinTime > hardMaxTime)
    hardMinTime = hardMaxTime;
  if(normalTime < hardMinTime)
    normalTime = hardMinTime;
  if(normalTime > hardMaxTime)
    normalTime = hardMaxTime;

  minTimeBuf = hardMinTime;
  normalTimeBuf = normalTime;
  maxTimeBuf = hardMaxTime;
}

string TimeControl::getMinNormalMaxStr() const
{
  double min = 0.0;
  double normal = 0.0;
  double max = 0.0;
  getMinNormalMax(min,normal,max);
  ostringstream out;
  out << min << "/" << normal << "/" << max;
  return out.str();
}
