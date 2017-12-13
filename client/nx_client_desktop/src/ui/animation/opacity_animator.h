#pragma once

// This include is located here for usage consistency.
#include <ui/animation/variant_animator.h>

class QGraphicsObject;

bool hasOpacityAnimator(QGraphicsObject* item);
VariantAnimator* takeOpacityAnimator(QGraphicsObject* item);
VariantAnimator* opacityAnimator(QGraphicsObject* item, qreal speed = 1.0);
