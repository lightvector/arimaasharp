/*
 * init.cpp
 *
 *  Created on: May 9, 2011
 *      Author: davidwu
 */

#include "../core/global.h"
#include "../core/rand.h"
#include "../board/board.h"
#include "../pattern/pattern.h"
#include "../pattern/patternsolver.h"
#include "../learning/featuremove.h"
#include "../learning/featurearimaa.h"
#include "../eval/evalparams.h"
#include "../eval/eval.h"
#include "../program/init.h"

static bool initCalled = false;

bool Init::ARIMAA_DEV = false;

void Init::init(bool isDev)
{
  if(initCalled)
    cout << "INITIALIZING AGAIN!" << endl;
  initCalled = true;

  ARIMAA_DEV = isDev;

  Board::initData();
  Eval::init();
  MoveFeature::initFeatureSet();
  MoveFeatureLite::initFeatureSet();
  EvalParams::init();
  ArimaaFeature::init();

  if(ARIMAA_DEV)
  {
    Condition::init();
    Pattern::init();
    KB::init();
  }
}

