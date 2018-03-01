#include "ubjson_functions.h"

#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QVector>

#include <nx/fusion/fusion/fusion_adaptors.h>

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
(QSize)(QSizeF)(QRect)(QRectF)(QPoint)(QPointF)(QVector2D)(QVector3D)(QVector4D),
(ubjson)
)