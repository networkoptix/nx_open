// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// This include is located here for usage consistency.
#include <ui/animation/variant_animator.h>

class QWidget;
class AnimationTimer;

bool hasOpacityAnimator(QWidget* widget);
VariantAnimator* takeWidgetOpacityAnimator(QWidget* widget);
VariantAnimator* widgetOpacityAnimator(QWidget* widget, AnimationTimer* timer, qreal speed = 1.0);
