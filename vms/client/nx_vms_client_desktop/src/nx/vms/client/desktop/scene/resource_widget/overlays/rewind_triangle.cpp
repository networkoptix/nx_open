// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rewind_triangle.h"

#include <QtGui/QPainter>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>

RewindTriangle::RewindTriangle(bool fastForward, QGraphicsWidget* parent):
    QGraphicsWidget(parent)
{
    const auto ffIcon = qnSkin->icon("item/rewind_triangle.svg");
    m_pixmap = fastForward
        ? ffIcon.pixmap(nx::style::Metrics::kRewindArrowSize)
        : ffIcon.pixmap(nx::style::Metrics::kRewindArrowSize)
            .transformed(QTransform().scale(-1, 1));

    setMaximumSize(nx::style::Metrics::kRewindArrowSize);
    setMinimumSize(nx::style::Metrics::kRewindArrowSize);
    setVisible(true);
    setOpacity(0.0);
}

void RewindTriangle::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    painter->setOpacity(opacity());
    painter->drawPixmap(0, 0, m_pixmap);
}
