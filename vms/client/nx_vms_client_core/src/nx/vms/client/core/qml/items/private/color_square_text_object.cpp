// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_square_text_object.h"

#include <QtGui/QPainter>

#include <nx/vms/client/core/utils/geometry.h>

namespace nx::vms::client::core {

QSizeF ColorSquareTextObject::intrinsicSize(
    QTextDocument* /*doc*/, int /*posInDocument*/, const QTextFormat& /*format*/)
{
    return QSizeF{sLength, sLength};
}

void ColorSquareTextObject::drawObject(
    QPainter* painter,
    const QRectF& rect,
    QTextDocument* /*doc*/,
    int /*posInDocument*/,
    const QTextFormat& format)
{
    QColor color = qvariant_cast<QColor>(format.property(Properties::Color));
    QPen pen{Qt::black};
    pen.setWidth(1);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    painter->setBrush(color);
    painter->drawRoundedRect(Geometry::eroded(rect, 1), 2, 2);
}

} // namespace nx::vms::client::core
