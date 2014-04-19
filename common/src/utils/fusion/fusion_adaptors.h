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

QN_FUSION_ADAPT_CLASS_GSN(QVector2D, 
    ((&QVector2D::x, &QVector2D::setX, "x"))
    ((&QVector2D::y, &QVector2D::setY, "y"))
)

QN_FUSION_ADAPT_CLASS_GSN(QVector3D, 
    ((&QVector3D::x, &QVector3D::setX, "x"))
    ((&QVector3D::y, &QVector3D::setY, "y"))
    ((&QVector3D::z, &QVector3D::setZ, "z"))
)

QN_FUSION_ADAPT_CLASS_GSN(QVector4D, 
    ((&QVector4D::x, &QVector4D::setX, "x"))
    ((&QVector4D::y, &QVector4D::setY, "y"))
    ((&QVector4D::z, &QVector4D::setZ, "z"))
    ((&QVector4D::w, &QVector4D::setW, "w"))
)

#endif // QN_FUSION_ADAPTORS_H
