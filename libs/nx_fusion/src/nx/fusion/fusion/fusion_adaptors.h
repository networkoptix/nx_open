// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_FUSION_ADAPTORS_H
#define QN_FUSION_ADAPTORS_H

#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QSizeF>

#include "fusion.h"
#include "fusion_adaptor.h"

QN_FUSION_ADAPT_CLASS_GSN(QSize,
    ((&QSize::width, &QSize::setWidth, "width"))
    ((&QSize::height, &QSize::setHeight, "height"))
)

QN_FUSION_ADAPT_CLASS_GSN(QSizeF,
    ((&QSizeF::width, &QSizeF::setWidth, "width"))
    ((&QSizeF::height, &QSizeF::setHeight, "height"))
)

QN_FUSION_ADAPT_CLASS_GSN(QRect,
    ((&QRect::left, &QRect::setLeft, "x"))
    ((&QRect::top, &QRect::setTop, "y"))
    ((&QRect::width, &QRect::setWidth, "width"))
    ((&QRect::height, &QRect::setHeight, "height"))
)

QN_FUSION_ADAPT_CLASS_GSN(QRectF,
    ((&QRectF::left, &QRectF::setLeft, "x"))
    ((&QRectF::top, &QRectF::setTop, "y"))
    ((&QRectF::width, &QRectF::setWidth, "width"))
    ((&QRectF::height, &QRectF::setHeight, "height"))
)

QN_FUSION_ADAPT_CLASS_GSN(QPoint,
    ((&QPoint::x, &QPoint::setX, "x"))
    ((&QPoint::y, &QPoint::setY, "y"))
)

QN_FUSION_ADAPT_CLASS_GSN(QPointF,
    ((&QPointF::x, &QPointF::setX, "x"))
    ((&QPointF::y, &QPointF::setY, "y"))
)

#endif // QN_FUSION_ADAPTORS_H
