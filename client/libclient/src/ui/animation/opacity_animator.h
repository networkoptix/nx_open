#ifndef QN_OPACITY_ANIMATOR_H
#define QN_OPACITY_ANIMATOR_H

#include "variant_animator.h"

class QGraphicsObject;

VariantAnimator *opacityAnimator(QGraphicsObject *item, qreal speed = 1.0);
bool hasOpacityAnimator(QGraphicsObject *item);

#endif // QN_OPACITY_ANIMATOR_H
