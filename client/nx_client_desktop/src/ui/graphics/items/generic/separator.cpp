#include "separator.h"

#include <numeric>

#include <QtGui/QPainter>

namespace
{
    enum { kDefaultSeparatorWidth = 1 };
}

SeparatorAreaProperties::SeparatorAreaProperties()
    : width(kDefaultSeparatorWidth)
    , color(Qt::yellow)
{
}

SeparatorAreaProperties::SeparatorAreaProperties(qreal initWidth
    , const QColor &initColor)
    : width(initWidth)
    , color(initColor)
{
}

///

QnSeparator::QnSeparator(qreal width
    , const QColor &color
    , QGraphicsItem *parent)
    : QGraphicsWidget(parent)
    , m_areas(1, SeparatorAreaProperties(width, color))
    , m_pixmap()
{
    init();
}

QnSeparator::QnSeparator(const SeparatorAreas &areas
    , QGraphicsItem *parent)
    : QGraphicsWidget(parent)
    , m_areas(areas)
    , m_pixmap()
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
    const auto width = geometry().width();
    if (m_pixmap.width() == qRound(width))
        return;

    const auto height = std::accumulate(m_areas.begin(), m_areas.end(), 0
        , [](qreal init, const SeparatorAreaProperties &props)
    {
        return init + props.width;
    });

    const auto size = QSize(width, height);
    m_pixmap = QPixmap(size);
    m_pixmap.fill(Qt::red);

    QPainter painter(&m_pixmap);
    painter.setPen(Qt::NoPen);

    qreal y = 0;
    for(const auto &area: m_areas)
    {
        const auto areaRect = QRectF(QPointF(0, y), QPointF(width, y + area.width));
        painter.setBrush(area.color);
        painter.drawRect(areaRect);
        y += area.width;
    }

    resize(size);
    setMaximumHeight(size.height());

    update();
}

void QnSeparator::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (!m_pixmap.isNull())
        painter->drawPixmap(0, 0, m_pixmap);
}
