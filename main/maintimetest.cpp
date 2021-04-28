/*
 * mantimetest.cpp
 * Author: davidwu
 */

#include <cmath>
#include "../core/global.h"
#include "../core/rand.h"
#include "../search/timecontrol.h"
#include "../program/command.h"
#include "../main/main.h"

using namespace std;

/*
static void printForTC(TimeControl tc, double alreadyUsed)
{
  int numReservesLeft = 13;
  int reservesLeft[13] =
  {0,2,5,10,15,20,30,40,60,90,120,180,240};

  double min;
  double normal;
  double max;

  cout << "TC " << tc.perMove << "/move " << tc.perMoveMax << "/move max " << tc.percent << "% " << tc.reserveMax << " reservemax" << endl;
  for(int i = 0; i<numReservesLeft; i++)
  {
    tc.alreadyUsed = alreadyUsed;
    tc.reserveCurrent = reservesLeft[i];
    tc.getMinNormalMax(min,normal,max);
    cout << "res " << reservesLeft[i] << " used " << alreadyUsed << " " << min << " " << normal << " " << max << endl;
  }
  cout << endl;
}
*/

int MainFuncs::runTimeTests(int argc, const char* const* argv)
{
  const char* usage = "";
  const char* required = "";
  const char* allowed = "";
  const char* empty = "";
  const char* nonempty = "";
  vector<string> mainCommand = Command::parseCommand(argc, argv, usage, required, allowed, empty, nonempty);
  map<string,string> flags = Command::parseFlags(argc, argv, usage, required, allowed, empty, nonempty);
  (void)mainCommand;
  (void)flags;

  TimeControl cc;
  cc.perMove = 120;
  cc.percent = 100;
  cc.reserveStart = 120;
  cc.reserveMax = 600;
  cc.gameMax = 14400;
  cc.perMoveMax = 3600;
  cc.gameCurrent = 0;

  TimeControl regular;
  regular.perMove = 60;
  regular.percent = 100;
  regular.reserveStart = 240;
  regular.reserveMax = 3600;
  regular.gameMax = 14400;
  regular.perMoveMax = 240;
  regular.gameCurrent = 0;

  TimeControl steady;
  steady.perMove = 45;
  steady.percent = 100;
  steady.reserveStart = 210;
  steady.reserveMax = 3600;
  steady.gameMax = 10800;
  steady.perMoveMax = 210;
  steady.gameCurrent = 0;

  TimeControl fast;
  fast.perMove = 30;
  fast.percent = 100;
  fast.reserveStart = 180;
  fast.reserveMax = 3600;
  fast.gameMax = 7200;
  fast.perMoveMax = 180;
  fast.gameCurrent = 0;

  TimeControl blitz;
  blitz.perMove = 15;
  blitz.percent = 100;
  blitz.reserveStart = 150;
  blitz.reserveMax = 3600;
  blitz.gameMax = 3600;
  blitz.perMoveMax = 150;
  blitz.gameCurrent = 0;

  TimeControl lightning;
  lightning.perMove = 8;
  lightning.percent = 100;
  lightning.reserveStart = 45;
  lightning.reserveMax = 3600;
  lightning.gameMax = 3600;
  lightning.perMoveMax = 45;
  lightning.gameCurrent = 0;

  TimeControl ultra;
  ultra.perMove = 4;
  ultra.percent = 100;
  ultra.reserveStart = 20;
  ultra.reserveMax = 3600;
  ultra.gameMax = 3600;
  ultra.perMoveMax = 30;
  ultra.gameCurrent = 0;

  TimeControl micro;
  micro.perMove = 2;
  micro.percent = 100;
  micro.reserveStart = 10;
  micro.reserveMax = 3600;
  micro.gameMax = 3600;
  micro.perMoveMax = 15;
  micro.gameCurrent = 0;

  TimeControl nano;
  nano.perMove = 1;
  nano.percent = 100;
  nano.reserveStart = 10;
  nano.reserveMax = 3600;
  nano.gameMax = 3600;
  nano.perMoveMax = 10;
  nano.gameCurrent = 0;
  /*
  printForTC(cc,0);
  printForTC(regular,0);
  printForTC(steady,0);
  printForTC(fast,0);
  printForTC(blitz,0);
  printForTC(lightning,0);
  printForTC(ultra,0);
  printForTC(micro,0);

  printForTC(cc,10);
  printForTC(blitz,25);
  printForTC(micro,1);
  */

  int gameLength = 50;
  double timeScaleMean = 1.1;
  double timeScaleScale = 0.1;
  double alreadyUsedScale = 0.2;
  double sendLagScale = 0.2;
  double noTimeQuality = 0.2; //Move quality scales as if you thought this many seconds even if you moved asap.
  TimeControl tc = ultra;
  //tc.perMove = 0.6;
  //tc.reserveStart = 240;
  Rand rand;

  double sumScores[2] = {0,0};
  int sumRunOutOfTimes[2] = {0,0};
  for(int reps = 0; reps < 1000; reps++)
  {
    double timeScales[gameLength];
    double alreadyUseds[gameLength];
    double sendLags[gameLength];
    double totalTimeAvailable = tc.reserveStart;
    double totalScaling = 0;
    for(int i = 0; i<gameLength; i++)
    {
      timeScales[i] = timeScaleMean * exp(timeScaleScale/timeScaleMean * rand.nextLogistic());
      alreadyUseds[i] = fabs(rand.nextLogistic() * alreadyUsedScale);
      sendLags[i] = fabs(rand.nextLogistic() * sendLagScale);
      totalTimeAvailable += tc.perMove - alreadyUseds[i] - sendLags[i] + noTimeQuality;
      totalScaling += timeScales[i];
    }

    if(reps == 0)
    cout << "turn "
         << " used "
         << " ideal "
         << " reserve "
         << " scale "
         << " already "
         << " lag "
         << " min "
         << " nor "
         << " max "
         << " dscore "
         << endl;
    for(int isNew = 0; isNew <= 1; isNew++)
    {
      double score = 0;
      int runOutOfTimes = 0;
      tc.reserveCurrent = tc.reserveStart;
      for(int i = 0; i<gameLength; i++)
      {
        double idealUsed = totalTimeAvailable / totalScaling * timeScales[i];

        tc.alreadyUsed = alreadyUseds[i];
        double min, normal, max;
        if(isNew == 1)
          tc.getMinNormalMax(min,normal,max);
        else
          tc.getMinNormalMaxOld(min,normal,max);

        double used = normal * timeScales[i];
        if(used < min) used = min;
        if(used > max) used = max;

        tc.reserveCurrent += tc.perMove - used - alreadyUseds[i] - sendLags[i];
        if(tc.reserveCurrent <= 0)
        {
          runOutOfTimes++;
          tc.reserveCurrent = 0;
          if(reps == 0)
          cout << "Ran out of time!" << endl;
        }
        double dscore = timeScales[i] * (log(used+noTimeQuality) - log(idealUsed));
        score += dscore;
        if(reps == 0)
        cout << i
             << " " << used
             << " " << idealUsed
             << " " << tc.reserveCurrent
             << " " << timeScales[i]
             << " " << alreadyUseds[i]
             << " " << sendLags[i]
             << " " << min
             << " " << normal
             << " " << max
             << " " << dscore << endl;
      }
      if(reps == 0)
      {
        cout << "score " << score << endl;
        cout << "ran out of time count: " << runOutOfTimes << endl;
      }
      sumScores[isNew] += score;
      sumRunOutOfTimes[isNew] += runOutOfTimes;
    }
  }

  cout << "Old sum scores: " << sumScores[0] << endl;
  cout << "New sum scores: " << sumScores[1] << endl;
  cout << "Old run out of times: " << sumRunOutOfTimes[0] << endl;
  cout << "New run out of times: " << sumRunOutOfTimes[1] << endl;

  return EXIT_SUCCESS;
}







