#include "busy_indicator.h"

#include <QtWidgets/QStyle>
#include <cmath>

#include <utils/common/scoped_painter_rollback.h>
#include <ui/animation/animation_timer.h>
#include <ui/common/geometry.h>


class QnBusyIndicatorPrivate
{
public:
    QnBusyIndicatorPrivate() :
        dotCount(3),
        dotSpacing(8),
        dotRadius(4.0),
        dotDiameter(roundedDiameter(dotRadius)),
        minimumOpacity(0.0),
        opacityRange(1.0 - minimumOpacity),
        dotLagMs(200),
        downTimeMs(300),
        fadeInTimeMs(500),
        upTimeMs(0),
        fadeOutTimeMs(500),
        totalTimeMs(0)
    {
        sizeChanged();
        timeChanged();
    }

    void sizeChanged()
    {
        indicatorSize = QSize(
            dotDiameter * dotCount + dotSpacing * (dotCount - 1),
            dotDiameter);
    }

    void timeChanged()
    {
        totalTimeMs = qMax(downTimeMs + fadeInTimeMs + upTimeMs + fadeOutTimeMs, 1U);
    }

    void setDotRadius(qreal radius)
    {
        dotRadius = radius;
        dotDiameter = roundedDiameter(radius);
    }

    void setMinimumOpacity(qreal opacity)
    {
        minimumOpacity = qBound(0.0, opacity, 1.0);
        opacityRange = 1.0 - minimumOpacity;
    }

    qreal opacityFromTime(int timeMs) const
    {
        timeMs %= totalTimeMs;
        unsigned int ms = timeMs < 0 ? timeMs += totalTimeMs : timeMs;

        if (ms <= downTimeMs)
            return minimumOpacity;

        ms -= downTimeMs;

        if (ms <= fadeInTimeMs)
            return minimumOpacity + (static_cast<qreal>(ms) / fadeInTimeMs) * opacityRange;

        ms -= fadeInTimeMs;

        if (ms <= upTimeMs)
            return 1.0;

        ms -= upTimeMs;
        return 1.0 - (static_cast<qreal>(ms) / fadeOutTimeMs) * opacityRange;
    }

    static unsigned int roundedDiameter(qreal radius)
    {
        return static_cast<unsigned int>(std::ceil(radius * 2.0));
    }

public:
    unsigned int dotCount;
    unsigned int dotSpacing;
    qreal dotRadius;
    unsigned int dotDiameter;

    qreal minimumOpacity;
    qreal opacityRange;

    int dotLagMs;

    unsigned int downTimeMs;
    unsigned int fadeInTimeMs;
    unsigned int upTimeMs;
    unsigned int fadeOutTimeMs;
    unsigned int totalTimeMs;

    int currentTimeMs;

    QSize indicatorSize;
};

/*
* QnBusyIndicator
*/

QnBusyIndicator::QnBusyIndicator() :
    d_ptr(new QnBusyIndicatorPrivate())
{
}

QnBusyIndicator::~QnBusyIndicator()
{
}

unsigned int QnBusyIndicator::dotCount() const
{
    Q_D(const QnBusyIndicator);
    return d->dotCount;
}

void QnBusyIndicator::setDotCount(unsigned int count)
{
    Q_D(QnBusyIndicator);
    if (d->dotCount == count)
        return;

    d->dotCount = count;

    d->sizeChanged();
    indicatorSizeChanged();
}

qreal QnBusyIndicator::dotRadius() const
{
    Q_D(const QnBusyIndicator);
    return d->dotRadius;
}

void QnBusyIndicator::setDotRadius(qreal radius)
{
    Q_D(QnBusyIndicator);
    radius = qMax(0.1, radius);

    unsigned int oldDiameter = d->dotDiameter;

    d->setDotRadius(radius);

    if (oldDiameter != d->dotDiameter)
    {
        d->sizeChanged();
        indicatorSizeChanged();
    }
}

unsigned int QnBusyIndicator::dotSpacing() const
{
    Q_D(const QnBusyIndicator);
    return d->dotSpacing;
}

void QnBusyIndicator::setDotSpacing(unsigned int spacing)
{
    Q_D(QnBusyIndicator);
    if (d->dotSpacing == spacing)
        return;

    d->dotSpacing = spacing;

    d->sizeChanged();
    indicatorSizeChanged();
}

qreal QnBusyIndicator::minimumOpacity() const
{
    Q_D(const QnBusyIndicator);
    return d->minimumOpacity;
}

void QnBusyIndicator::setMinimumOpacity(qreal opacity)
{
    Q_D(QnBusyIndicator);
    d->setMinimumOpacity(opacity);
}

int QnBusyIndicator::dotLagMs() const
{
    Q_D(const QnBusyIndicator);
    return d->dotLagMs;
}

void QnBusyIndicator::setDotLagMs(int lag)
{
    Q_D(QnBusyIndicator);
    if (d->dotLagMs == lag)
        return;

    d->dotLagMs = lag;
}

unsigned int QnBusyIndicator::downTimeMs() const
{
    Q_D(const QnBusyIndicator);
    return d->downTimeMs;
}

void QnBusyIndicator::setDownTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicator);
    if (d->downTimeMs == ms)
        return;

    d->downTimeMs = ms;
    d->timeChanged();
}

unsigned int QnBusyIndicator::fadeInTimeMs() const
{
    Q_D(const QnBusyIndicator);
    return d->fadeInTimeMs;
}

void QnBusyIndicator::setFadeInTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicator);
    if (d->fadeInTimeMs == ms)
        return;

    d->fadeInTimeMs = ms;
    d->timeChanged();
}

unsigned int QnBusyIndicator::upTimeMs() const
{
    Q_D(const QnBusyIndicator);
    return d->upTimeMs;
}

void QnBusyIndicator::setUpTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicator);
    if (d->upTimeMs == ms)
        return;

    d->upTimeMs = ms;
    d->timeChanged();
}

unsigned int QnBusyIndicator::fadeOutTimeMs() const
{
    Q_D(const QnBusyIndicator);
    return d->fadeOutTimeMs;
}

void QnBusyIndicator::setFadeOutTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicator);
    if (d->fadeOutTimeMs == ms)
        return;

    d->fadeOutTimeMs = ms;
    d->timeChanged();
}

unsigned int QnBusyIndicator::totalTimeMs() const
{
    Q_D(const QnBusyIndicator);
    return d->totalTimeMs;
}

void QnBusyIndicator::setAnimationTimesMs(unsigned int downTimeMs, unsigned int fadeInTimeMs, unsigned int upTimeMs, unsigned int fadeOutTimeMs)
{
    Q_D(QnBusyIndicator);
    bool changed = downTimeMs != d->downTimeMs || fadeInTimeMs != d->fadeInTimeMs ||
                   upTimeMs != d->upTimeMs || fadeOutTimeMs != d->fadeOutTimeMs;

    d->downTimeMs = downTimeMs;
    d->fadeInTimeMs = fadeInTimeMs;
    d->upTimeMs = upTimeMs;
    d->fadeOutTimeMs = fadeOutTimeMs;

    if (changed)
        d->timeChanged();
}

QSize QnBusyIndicator::indicatorSize() const
{
    Q_D(const QnBusyIndicator);
    return d->indicatorSize;
}

void QnBusyIndicator::paintIndicator(QPainter* painter, const QPointF& origin, int currentTimeMs)
{
    Q_D(const QnBusyIndicator);

    QnScopedPainterOpacityRollback opacityRollback(painter);

    qreal radius = d->dotDiameter * 0.5;
    QPointF center(origin.x() + radius, origin.y() + radius);

    unsigned int delta = d->dotDiameter + d->dotSpacing;
    int timeMs = currentTimeMs;

    for (unsigned int i = 0; i != d->dotCount; ++i)
    {
        painter->setOpacity(d->opacityFromTime(timeMs));
        painter->drawEllipse(center, d->dotRadius, d->dotRadius);

        center.setX(center.x() + delta);
        timeMs -= d->dotLagMs;
    }
}

void QnBusyIndicator::indicatorSizeChanged()
{
}

/*
* QnBusyIndicatorWidget
*/

QnBusyIndicatorWidget::QnBusyIndicatorWidget(QWidget* parent) :
    QWidget(parent),
    m_currentTimeMs(0)
{
    setFocusPolicy(Qt::NoFocus);

    QAnimationTimer* animationTimer = new QAnimationTimer(this);
    setTimer(animationTimer);
    startListening();
}

QSize QnBusyIndicatorWidget::minimumSizeHint() const
{
    QMargins margins = contentsMargins();
    QSize sizeOfMargins(margins.left() + margins.right(), margins.top() + margins.bottom());
    return indicatorSize() + sizeOfMargins;
}

QSize QnBusyIndicatorWidget::sizeHint() const
{
    return minimumSizeHint();
}

void QnBusyIndicatorWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(palette().color(foregroundRole()));
    painter.setPen(Qt::NoPen);

    paintIndicator(&painter, indicatorRect().topLeft(), m_currentTimeMs);
}

void QnBusyIndicatorWidget::indicatorSizeChanged()
{
    updateGeometry();
}

void QnBusyIndicatorWidget::tick(int deltaMs)
{
    m_currentTimeMs = (m_currentTimeMs + deltaMs) % totalTimeMs();
    update(indicatorRect());
}

QRect QnBusyIndicatorWidget::indicatorRect() const
{
    return QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, indicatorSize(), contentsRect());
}

/*
* QnBusyIndicatorGraphicsWidget
*/

QnBusyIndicatorGraphicsWidget::QnBusyIndicatorGraphicsWidget(QGraphicsItem* parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags)
{
    setFocusPolicy(Qt::NoFocus);

    registerAnimation(this);
    startListening();
}

void QnBusyIndicatorGraphicsWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QnScopedPainterBrushRollback brushRollback(painter, palette().color(QPalette::WindowText));
    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);

    paintIndicator(painter, indicatorRect().topLeft(), m_currentTimeMs);
}

QSizeF QnBusyIndicatorGraphicsWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    switch (which)
    {
        case Qt::MinimumSize:
        case Qt::PreferredSize:
        {
            MarginsF margins = contentsMargins();
            QSizeF sizeOfMargins(margins.left() + margins.right(), margins.top() + margins.bottom());
            return QSizeF(indicatorSize() + sizeOfMargins).expandedTo(constraint);
        }

        default:
        {
            return base_type::sizeHint(which, constraint);
        }
    }
}

void QnBusyIndicatorGraphicsWidget::indicatorSizeChanged()
{
    updateGeometry();
}

void QnBusyIndicatorGraphicsWidget::tick(int deltaMs)
{
    m_currentTimeMs = (m_currentTimeMs + deltaMs) % totalTimeMs();
    update(indicatorRect());
}

QRectF QnBusyIndicatorGraphicsWidget::indicatorRect() const
{
    return QnGeometry::aligned(QSizeF(indicatorSize()), contentsRect());
}
