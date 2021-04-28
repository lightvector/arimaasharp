
/*
 * timecontrol.h
 * Author: davidwu
 */

#ifndef TIMECONTROL_H
#define TIMECONTROL_H

struct TimeControl
{
  //Fixed per game
  double perMove;          //Seconds per move
  double percent;          //Percent of leftover time saved into reserve
  double reserveStart;     //Initial reserve time
  double reserveMax;       //Maximum reserve time
  double gameMax;          //Total time allowed for game
  double perMoveMax;       //Max time allowed for a move

  //Dynamic
  double alreadyUsed;      //How much time already used for this move?
  double reserveCurrent;   //Current reserve time
  double gameCurrent;      //Current time used for game

  bool isOpponentPonder;   //Are we pondering to guess the opponent's possible moves?
  bool isPlaPonder;        //Are we pondering for ourselves given a guess at the opp move?

  //By default, completely permissive
  TimeControl();

  //Validate the values of parameters and fail if invalid
  void validate() const;
  bool isValid() const;

  //Reset the dynamic values to the initial values based on the other parameters
  void restart();
  //Adjust the time limits to reflect the time spent pondering
  //(That is, takes a TC reflecting the state of the world at the time of
  // the start of real search and updates it so that if a search has already been
  // going for ponderTime, the limits will be appropriate for that search)
  void addForPonder(double ponderTime);

  //Get the recommended min,normal,and max times to search
  //Min - searching less than this amount of time would be wasteful
  //Normal - a basic average or target search time given current reserves
  //Max - searching more than this is likely to lose on time
  void getMinNormalMax(double& minTime, double& normalTime, double& maxTime) const;

  void getMinNormalMaxOld(double& minTime, double& normalTime, double& maxTime) const;

  string getMinNormalMaxStr() const;
};

#endif
