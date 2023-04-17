// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include <nx/reflect/instrument.h>

NX_REFLECTION_INSTRUMENT_GSN(QSize,
    ((&QSize::width, &QSize::setWidth, "width"))
    ((&QSize::height, &QSize::setHeight, "height"))
)

NX_REFLECTION_INSTRUMENT_GSN(QSizeF,
    ((&QSizeF::width, &QSizeF::setWidth, "width"))
    ((&QSizeF::height, &QSizeF::setHeight, "height"))
)

NX_REFLECTION_INSTRUMENT_GSN(QRect,
    ((&QRect::left, &QRect::setLeft, "x"))
    ((&QRect::top, &QRect::setTop, "y"))
    ((&QRect::width, &QRect::setWidth, "width"))
    ((&QRect::height, &QRect::setHeight, "height"))
)

NX_REFLECTION_INSTRUMENT_GSN(QRectF,
    ((&QRectF::left, &QRectF::setLeft, "x"))
    ((&QRectF::top, &QRectF::setTop, "y"))
    ((&QRectF::width, &QRectF::setWidth, "width"))
    ((&QRectF::height, &QRectF::setHeight, "height"))
)

NX_REFLECTION_INSTRUMENT_GSN(QPoint,
    ((&QPoint::x, &QPoint::setX, "x"))
    ((&QPoint::y, &QPoint::setY, "y"))
)

NX_REFLECTION_INSTRUMENT_GSN(QPointF,
    ((&QPointF::x, &QPointF::setX, "x"))
    ((&QPointF::y, &QPointF::setY, "y"))
)

NX_REFLECTION_INSTRUMENT_GSN(QVector2D,
    ((&QVector2D::x, &QVector2D::setX, "x"))
    ((&QVector2D::y, &QVector2D::setY, "y"))
)

NX_REFLECTION_INSTRUMENT_GSN(QVector3D,
    ((&QVector3D::x, &QVector3D::setX, "x"))
    ((&QVector3D::y, &QVector3D::setY, "y"))
    ((&QVector3D::z, &QVector3D::setZ, "z"))
)

NX_REFLECTION_INSTRUMENT_GSN(QVector4D,
    ((&QVector4D::x, &QVector4D::setX, "x"))
    ((&QVector4D::y, &QVector4D::setY, "y"))
    ((&QVector4D::z, &QVector4D::setZ, "z"))
    ((&QVector4D::w, &QVector4D::setW, "w"))
)
