// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGlobal>

class QnViewportBoundWidget;
class VariantAnimator;

bool hasMarginsAnimator(QnViewportBoundWidget* item);
VariantAnimator* takeMarginsAnimator(QnViewportBoundWidget* item);
VariantAnimator* marginsAnimator(QnViewportBoundWidget* item, qreal speed = 1.0);
