#ifndef QN_ITEM_OPACITY_ANIMATOR_H
#define QN_ITEM_OPACITY_ANIMATOR_H

#include "variant_animator.h"

class QGraphicsObject;

VariantAnimator *opacityAnimator(QGraphicsObject *item, qreal speed = 1.0);

#endif // QN_ITEM_OPACITY_ANIMATOR_H
