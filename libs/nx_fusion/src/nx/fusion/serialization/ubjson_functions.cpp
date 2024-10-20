// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ubjson_functions.h"

#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QVector>

#include <nx/fusion/fusion/fusion_adaptors.h>

QN_FUSION_DEFINE_FUNCTIONS(QSize, (ubjson))
QN_FUSION_DEFINE_FUNCTIONS(QSizeF, (ubjson))
QN_FUSION_DEFINE_FUNCTIONS(QRect, (ubjson))
QN_FUSION_DEFINE_FUNCTIONS(QRectF, (ubjson))
QN_FUSION_DEFINE_FUNCTIONS(QPoint, (ubjson))
QN_FUSION_DEFINE_FUNCTIONS(QPointF, (ubjson))
