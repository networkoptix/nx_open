// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_FUSION_ADAPTORS_H
#define QN_FUSION_ADAPTORS_H

#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include <nx/reflect/instrument.h>

#include "fusion.h"
#include "fusion_adaptor.h"

#define ADAPT_CLASS_WITH_FUSION_AND_REFLECT(CLASS, ...) \
    QN_FUSION_ADAPT_CLASS_GSN(CLASS, ##__VA_ARGS__) \
    NX_REFLECTION_INSTRUMENT_GSN(CLASS, ##__VA_ARGS__)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QSize,
    ((&QSize::width, &QSize::setWidth, "width"))
    ((&QSize::height, &QSize::setHeight, "height"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QSizeF,
    ((&QSizeF::width, &QSizeF::setWidth, "width"))
    ((&QSizeF::height, &QSizeF::setHeight, "height"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QRect,
    ((&QRect::left, &QRect::setLeft, "x"))
    ((&QRect::top, &QRect::setTop, "y"))
    ((&QRect::width, &QRect::setWidth, "width"))
    ((&QRect::height, &QRect::setHeight, "height"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QRectF,
    ((&QRectF::left, &QRectF::setLeft, "x"))
    ((&QRectF::top, &QRectF::setTop, "y"))
    ((&QRectF::width, &QRectF::setWidth, "width"))
    ((&QRectF::height, &QRectF::setHeight, "height"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QPoint,
    ((&QPoint::x, &QPoint::setX, "x"))
    ((&QPoint::y, &QPoint::setY, "y"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QPointF,
    ((&QPointF::x, &QPointF::setX, "x"))
    ((&QPointF::y, &QPointF::setY, "y"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QVector2D,
    ((&QVector2D::x, &QVector2D::setX, "x"))
    ((&QVector2D::y, &QVector2D::setY, "y"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QVector3D,
    ((&QVector3D::x, &QVector3D::setX, "x"))
    ((&QVector3D::y, &QVector3D::setY, "y"))
    ((&QVector3D::z, &QVector3D::setZ, "z"))
)

ADAPT_CLASS_WITH_FUSION_AND_REFLECT(QVector4D,
    ((&QVector4D::x, &QVector4D::setX, "x"))
    ((&QVector4D::y, &QVector4D::setY, "y"))
    ((&QVector4D::z, &QVector4D::setZ, "z"))
    ((&QVector4D::w, &QVector4D::setW, "w"))
)

#endif // QN_FUSION_ADAPTORS_H
