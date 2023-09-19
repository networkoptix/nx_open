// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtGui/QMatrix4x4>
#include <QtGui/QTransform>

QTransform sharpTransform(const QTransform &transform, bool *corrected = nullptr);

/**
 * Rounds the translation to the nearest integers if the `matrix` is an XY translation matrix.
 * If provided matrix is not a translation matrix then the original `matrix` value is returned.
 * The flag `corrected` is set accordingly.
 */
QMatrix4x4 sharpMatrix(const QMatrix4x4& matrix, bool* corrected = nullptr);

/**
 * Utility method to paint something on the painter leaving it sharp.
 * Actual if we are painting text or small details.
 */
void paintSharp(QPainter* painter, std::function<void(QPainter*)> paint);

/**
 * Utility method to paint a pixmap on the painter leaving it sharp.
 * Actual if the bitmap contains text or small details.
 */
void paintPixmapSharp(
    QPainter* painter,
    const QPixmap& pixmap,
    const QPointF& position = QPointF());

/**
 * Utility method to paint a pixmap on the painter leaving it sharp.
 * Actual if the bitmap contains text or small details.
 */
void paintPixmapSharp(
    QPainter* painter,
    const QPixmap& pixmap,
    const QRectF& rect,
    const QRect& sourceRect = QRect());
