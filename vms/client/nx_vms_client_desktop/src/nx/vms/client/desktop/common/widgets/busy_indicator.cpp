#include "busy_indicator.h"

#include <QtWidgets/QStyle>
#include <cmath>

#include <utils/common/scoped_painter_rollback.h>
#include <ui/animation/animation_timer.h>
#include <nx/client/core/utils/geometry.h>

using nx::vms::client::core::Geometry;

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// BusyIndicatorBase

BusyIndicatorBase::BusyIndicatorBase(QObject* parent) :
    base_type(parent)
{
    auto animationTimer = new QAnimationTimer(this);
    animationTimer->addListener(this);
    startListening();
}

BusyIndicatorBase::~BusyIndicatorBase()
{
}

//-------------------------------------------------------------------------------------------------
// BusyIndicator::Private

struct BusyIndicator::Private
{
    Private()
    {
        sizeChanged();
        timeChanged();
    }

    void sizeChanged()
    {
        unsigned int dotSize = this->dotSize();
        indicatorSize = QSize(
            dotSize * dotCount + dotSpacing * (dotCount - 1),
            dotSize);
    }

    void timeChanged()
    {
        totalTimeMs = downTimeMs + fadeInTimeMs + upTimeMs + fadeOutTimeMs;
    }

    qreal opacityFromTime(int timeMs) const
    {
        if (!totalTimeMs)
            return 1.0;

        timeMs %= totalTimeMs;
        unsigned int ms = timeMs < 0 ? timeMs += totalTimeMs : timeMs;

        if (ms < downTimeMs)
            return minimumOpacity;

        ms -= downTimeMs;

        if (ms < fadeInTimeMs)
            return minimumOpacity + (static_cast<qreal>(ms) / fadeInTimeMs) * (1.0 - minimumOpacity);

        ms -= fadeInTimeMs;

        if (ms < upTimeMs)
            return 1.0;

        ms -= upTimeMs;
        return 1.0 - (static_cast<qreal>(ms) / fadeOutTimeMs) * (1.0 - minimumOpacity);
    }

    unsigned int dotSize() const
    {
        return static_cast<unsigned int>(std::ceil(dotRadius * 2.0));
    }

public:
    unsigned int dotCount = 3;
    unsigned int dotSpacing = 8;
    qreal dotRadius = 4.0;

    qreal minimumOpacity = 0.0;

    int dotLagMs = 200;

    unsigned int downTimeMs = 300;
    unsigned int fadeInTimeMs = 500;
    unsigned int upTimeMs = 0;
    unsigned int fadeOutTimeMs = 500;
    unsigned int totalTimeMs = 0;

    int currentTimeMs = 0;

    QSize indicatorSize;
};

//-------------------------------------------------------------------------------------------------
// BusyIndicator

BusyIndicator::BusyIndicator(QObject* parent) :
    base_type(parent),
    d(new Private())
{
}

BusyIndicator::~BusyIndicator()
{
}

unsigned int BusyIndicator::dotCount() const
{
    return d->dotCount;
}

void BusyIndicator::setDotCount(unsigned int count)
{
    if (d->dotCount == count)
        return;

    d->dotCount = count;

    d->sizeChanged();
    emit sizeChanged();
}

qreal BusyIndicator::dotRadius() const
{
    return d->dotRadius;
}

void BusyIndicator::setDotRadius(qreal radius)
{
    radius = qMax(0.0, radius);

    QSize oldSize = d->indicatorSize;

    d->dotRadius = radius;
    d->sizeChanged();

    if (oldSize != d->indicatorSize)
        emit sizeChanged();

    emit updated();
}

unsigned int BusyIndicator::dotSpacing() const
{
    return d->dotSpacing;
}

void BusyIndicator::setDotSpacing(unsigned int spacing)
{
    if (d->dotSpacing == spacing)
        return;

    d->dotSpacing = spacing;

    d->sizeChanged();
    emit sizeChanged();
}

qreal BusyIndicator::minimumOpacity() const
{
    return d->minimumOpacity;
}

void BusyIndicator::setMinimumOpacity(qreal opacity)
{
    d->minimumOpacity = opacity;
    emit updated();
}

int BusyIndicator::dotLagMs() const
{
    return d->dotLagMs;
}

void BusyIndicator::setDotLagMs(int lag)
{
    if (d->dotLagMs == lag)
        return;

    d->dotLagMs = lag;
    emit updated();
}

unsigned int BusyIndicator::downTimeMs() const
{
    return d->downTimeMs;
}

void BusyIndicator::setDownTimeMs(unsigned int ms)
{
    if (d->downTimeMs == ms)
        return;

    d->downTimeMs = ms;
    d->timeChanged();
    emit updated();
}

unsigned int BusyIndicator::fadeInTimeMs() const
{
    return d->fadeInTimeMs;
}

void BusyIndicator::setFadeInTimeMs(unsigned int ms)
{
    if (d->fadeInTimeMs == ms)
        return;

    d->fadeInTimeMs = ms;
    d->timeChanged();
    emit updated();
}

unsigned int BusyIndicator::upTimeMs() const
{
    return d->upTimeMs;
}

void BusyIndicator::setUpTimeMs(unsigned int ms)
{
    if (d->upTimeMs == ms)
        return;

    d->upTimeMs = ms;
    d->timeChanged();
    emit updated();
}

unsigned int BusyIndicator::fadeOutTimeMs() const
{
    return d->fadeOutTimeMs;
}

void BusyIndicator::setFadeOutTimeMs(unsigned int ms)
{
    if (d->fadeOutTimeMs == ms)
        return;

    d->fadeOutTimeMs = ms;
    d->timeChanged();
    emit updated();
}

unsigned int BusyIndicator::totalTimeMs() const
{
    return d->totalTimeMs;
}

void BusyIndicator::setAnimationTimesMs(unsigned int downTimeMs, unsigned int fadeInTimeMs, unsigned int upTimeMs, unsigned int fadeOutTimeMs)
{
    const bool changed = downTimeMs != d->downTimeMs || fadeInTimeMs != d->fadeInTimeMs ||
                   upTimeMs != d->upTimeMs || fadeOutTimeMs != d->fadeOutTimeMs;

    d->downTimeMs = downTimeMs;
    d->fadeInTimeMs = fadeInTimeMs;
    d->upTimeMs = upTimeMs;
    d->fadeOutTimeMs = fadeOutTimeMs;

    if (changed)
    {
        d->timeChanged();
        emit updated();
    }
}

QSize BusyIndicator::size() const
{
    return d->indicatorSize;
}

void BusyIndicator::paint(QPainter* painter)
{
    QnScopedPainterOpacityRollback opacityRollback(painter);

    unsigned int dotSize = d->dotSize();
    qreal centerDistance = dotSize + d->dotSpacing;

    qreal halfSize = dotSize * 0.5;
    QPointF center(halfSize, halfSize);

    int timeMs = d->currentTimeMs;

    // TODO: #common Will not work with cosmetic pens in scaled painter
    qreal radius = d->dotRadius;
    if (painter->pen().style() != Qt::NoPen)
        radius -= qMax(painter->pen().widthF(), 1.0) * 0.5;

    for (unsigned int i = 0; i != d->dotCount; ++i)
    {
        painter->setOpacity(d->opacityFromTime(timeMs));
        painter->drawEllipse(center, radius, radius);

        center.setX(center.x() + centerDistance);
        timeMs -= d->dotLagMs;
    }
}

void BusyIndicator::tick(int deltaMs)
{
    if (!d->totalTimeMs)
        return;

    d->currentTimeMs = (d->currentTimeMs + deltaMs) % d->totalTimeMs;
    emit updated();
}

//-------------------------------------------------------------------------------------------------
// BusyIndicatorWidget

BusyIndicatorWidget::BusyIndicatorWidget(QWidget* parent):
    base_type(parent),
    m_indicator(new BusyIndicator()),
    m_indicatorRole(QPalette::Foreground),
    m_borderRole(QPalette::NoRole)
{
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    connect(m_indicator, &BusyIndicator::sizeChanged, this, &QWidget::updateGeometry);
    connect(m_indicator, &BusyIndicator::updated, this, &BusyIndicatorWidget::updateIndicator);
}

BusyIndicator* BusyIndicatorWidget::dots() const
{
    return m_indicator.data();
}

QPalette::ColorRole BusyIndicatorWidget::indicatorRole() const
{
    return m_indicatorRole;
}

void BusyIndicatorWidget::setIndicatorRole(QPalette::ColorRole role)
{
    if (m_indicatorRole == role)
        return;

    m_indicatorRole = role;
    updateIndicator();
}

QPalette::ColorRole BusyIndicatorWidget::borderRole() const
{
    return m_borderRole;
}

void BusyIndicatorWidget::setBorderRole(QPalette::ColorRole role)
{
    if (m_borderRole == role)
        return;

    m_borderRole = role;
    updateIndicator();
}

QSize BusyIndicatorWidget::minimumSizeHint() const
{
    const auto margins = contentsMargins();
    const QSize sizeOfMargins(margins.left() + margins.right(), margins.top() + margins.bottom());
    return m_indicator->size() + sizeOfMargins;
}

QSize BusyIndicatorWidget::sizeHint() const
{
    /* Make sure we don't call here minimumSizeHint overridden in a descendant: */
    return BusyIndicatorWidget::minimumSizeHint();
}

void BusyIndicatorWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_indicatorRole != QPalette::NoRole)
        painter.setBrush(palette().color(m_indicatorRole));
    else
        painter.setBrush(Qt::NoBrush);

    if (m_borderRole != QPalette::NoRole)
        painter.setPen(palette().color(m_borderRole));
    else
        painter.setPen(Qt::NoPen);

    paint(&painter);
}

void BusyIndicatorWidget::paint(QPainter* painter)
{
    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(indicatorRect().topLeft());
    m_indicator->paint(painter);
}

void BusyIndicatorWidget::updateIndicator()
{
    update(indicatorRect());
}

QRect BusyIndicatorWidget::indicatorRect() const
{
    return QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
        m_indicator->size(), contentsRect());
}

//-------------------------------------------------------------------------------------------------
// BusyIndicatorGraphicsWidget

BusyIndicatorGraphicsWidget::BusyIndicatorGraphicsWidget(
    QGraphicsItem* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    m_indicator(new BusyIndicator()),
    m_indicatorColor(palette().color(QPalette::Foreground)),
    m_borderColor(QColor())
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    connect(m_indicator, &BusyIndicator::sizeChanged,
        this, &BusyIndicatorGraphicsWidget::updateGeometry);
    connect(m_indicator, &BusyIndicator::updated,
        this, &BusyIndicatorGraphicsWidget::updateIndicator);
}

void BusyIndicatorGraphicsWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    QnScopedPainterBrushRollback brushRollback(painter, m_indicatorColor.isValid()
        ? QBrush(m_indicatorColor)
        : QBrush(Qt::NoBrush));

    QnScopedPainterPenRollback penRollback(painter, m_borderColor.isValid()
        ? QPen(m_borderColor)
        : QPen(Qt::NoPen));

    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(indicatorRect().topLeft());

    m_indicator->paint(painter);
}

BusyIndicator* BusyIndicatorGraphicsWidget::dots() const
{
    return m_indicator.data();
}

QColor BusyIndicatorGraphicsWidget::indicatorColor() const
{
    return m_indicatorColor;
}

void BusyIndicatorGraphicsWidget::setIndicatorColor(QColor color)
{
    if (m_indicatorColor == color)
        return;

    m_indicatorColor = color;
    updateIndicator();
}

QColor BusyIndicatorGraphicsWidget::borderColor() const
{
    return m_borderColor;
}

void BusyIndicatorGraphicsWidget::setBorderColor(QColor color)
{
    if (m_borderColor == color)
        return;

    m_borderColor = color;
    updateIndicator();
}

QSizeF BusyIndicatorGraphicsWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    switch (which)
    {
        case Qt::MinimumSize:
        case Qt::PreferredSize:
        {
            const auto margins = contentsMargins();
            const QSizeF sizeOfMargins(
                margins.left() + margins.right(), margins.top() + margins.bottom());
            return QSizeF(m_indicator->size() + sizeOfMargins).expandedTo(constraint);
        }

        default:
        {
            return base_type::sizeHint(which, constraint);
        }
    }
}

void BusyIndicatorGraphicsWidget::updateIndicator()
{
    update(indicatorRect());
}

QRectF BusyIndicatorGraphicsWidget::indicatorRect() const
{
    return Geometry::aligned(QSizeF(m_indicator->size()), contentsRect());
}

} // namespace nx::vms::client::desktop
