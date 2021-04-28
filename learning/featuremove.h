
/*
 * featuremove.h
 * Author: davidwu
 */

#ifndef FEATUREMOVE_H_
#define FEATUREMOVE_H_

#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../board/boardhistory.h"
#include "../eval/threats.h"
#include "../learning/feature.h"
#include "../learning/featurearimaa.h"

struct FeaturePosData;

namespace MoveFeature
{
  void initFeatureSet();
  const FeatureSet& getFeatureSet();
  ArimaaFeatureSet getArimaaFeatureSet();
}

namespace MoveFeatureLite
{
  void initFeatureSet();
  const FeatureSet& getFeatureSet();
  ArimaaFeatureSet getArimaaFeatureSet();
  ArimaaFeatureSet getArimaaFeatureSetForTraining();
}

#endif
