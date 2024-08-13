// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "separator.h"

#include <numeric>

#include <QtGui/QPainter>

#include <nx/vms/client/core/skin/color_theme.h>

namespace
{
    enum { kDefaultSeparatorWidth = 1 };
}

SeparatorAreaProperties::SeparatorAreaProperties()
    : width(kDefaultSeparatorWidth)
    , color(nx::vms::client::core::colorTheme()->color("yellow"))
{
}

SeparatorAreaProperties::SeparatorAreaProperties(qreal initWidth
    , const QColor &initColor)
    : width(initWidth)
    , color(initColor)
{
}

///

QnSeparator::QnSeparator(
    Qt::Orientation orientation,
    qreal width,
    const QColor& color,
    QGraphicsItem* parent)
    :
    QGraphicsWidget(parent),
    m_orientation(orientation),
    m_areas(1, SeparatorAreaProperties(width, color))
{
    init();
}

QnSeparator::QnSeparator(
    Qt::Orientation orientation,
    const SeparatorAreas& areas,
    QGraphicsItem* parent)
    :
    QGraphicsWidget(parent),
    m_orientation(orientation),
    m_areas(areas)
{
    init();
}

QnSeparator::~QnSeparator()
{
}

void QnSeparator::init()
{
    connect(this, &QnSeparator::geometryChanged, this, &QnSeparator::updatePixmap);
    updatePixmap();
}

void QnSeparator::updatePixmap()
{
    const bool horizontal = m_orientation == Qt::Horizontal;
    const qreal length = horizontal ? geometry().width() : geometry().height();
    const int pixmapLength = horizontal ? m_pixmap.width() : m_pixmap.height();
    if (pixmapLength == qRound(length))
        return;

    const qreal thickness = std::accumulate(m_areas.begin(), m_areas.end(), 0,
        [](qreal init, const SeparatorAreaProperties& props)
        {
            return init + props.width;
        });

    const auto size = horizontal ? QSize(length, thickness) : QSize(thickness, length);
    m_pixmap = QPixmap(size);
    m_pixmap.fill(Qt::red); //< Used for debug purposes, not visible for a client.

    QPainter painter(&m_pixmap);
    painter.setPen(Qt::NoPen);

    qreal pos = 0;
    for (const SeparatorAreaProperties& area: m_areas)
    {
        const auto areaRect = horizontal
            ? QRectF(0, pos, length, area.width)
            : QRectF(pos, 0, area.width, length);
        painter.setBrush(area.color);
        painter.drawRect(areaRect);
        pos += area.width;
    }

    resize(size);
    if (horizontal)
        setMaximumHeight(size.height());
    else
        setMaximumWidth(size.width());

    update();
}

void QnSeparator::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (!m_pixmap.isNull())
        painter->drawPixmap(0, 0, m_pixmap);
}
