#include "busy_indicator.h"

#include <QtWidgets/QStyle>
#include <cmath>

#include <utils/common/scoped_painter_rollback.h>
#include <ui/animation/animation_timer.h>
#include <ui/common/geometry.h>


class QnBusyIndicatorPainterPrivate
{
public:
    QnBusyIndicatorPainterPrivate() :
        dotCount(3),
        dotSpacing(8),
        dotRadius(4.0),
        minimumOpacity(0.0),
        dotLagMs(200),
        downTimeMs(300),
        fadeInTimeMs(500),
        upTimeMs(0),
        fadeOutTimeMs(500),
        totalTimeMs(0),
        currentTimeMs(0)
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

        if (ms <= downTimeMs)
            return minimumOpacity;

        ms -= downTimeMs;

        if (ms < fadeInTimeMs)
            return minimumOpacity + (static_cast<qreal>(ms) / fadeInTimeMs) * (1.0 - minimumOpacity);

        ms -= fadeInTimeMs;

        if (ms <= upTimeMs)
            return 1.0;

        ms -= upTimeMs;
        return 1.0 - (static_cast<qreal>(ms) / fadeOutTimeMs) * (1.0 - minimumOpacity);
    }

    unsigned int dotSize() const
    {
        return static_cast<unsigned int>(std::ceil(dotRadius * 2.0));
    }

public:
    unsigned int dotCount;
    unsigned int dotSpacing;
    qreal dotRadius;

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

QnBusyIndicatorPainter::QnBusyIndicatorPainter() :
    d_ptr(new QnBusyIndicatorPainterPrivate())
{
}

QnBusyIndicatorPainter::~QnBusyIndicatorPainter()
{
}

unsigned int QnBusyIndicatorPainter::dotCount() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->dotCount;
}

void QnBusyIndicatorPainter::setDotCount(unsigned int count)
{
    Q_D(QnBusyIndicatorPainter);
    if (d->dotCount == count)
        return;

    d->dotCount = count;

    d->sizeChanged();
    indicatorSizeChanged();
}

qreal QnBusyIndicatorPainter::dotRadius() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->dotRadius;
}

void QnBusyIndicatorPainter::setDotRadius(qreal radius)
{
    Q_D(QnBusyIndicatorPainter);
    radius = qMax(0.0, radius);

    QSize oldSize = d->indicatorSize;

    d->dotRadius = radius;
    d->sizeChanged();

    if (oldSize != d->indicatorSize)
        indicatorSizeChanged();
    else
        updateIndicator();
}

unsigned int QnBusyIndicatorPainter::dotSpacing() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->dotSpacing;
}

void QnBusyIndicatorPainter::setDotSpacing(unsigned int spacing)
{
    Q_D(QnBusyIndicatorPainter);
    if (d->dotSpacing == spacing)
        return;

    d->dotSpacing = spacing;

    d->sizeChanged();
    indicatorSizeChanged();
}

qreal QnBusyIndicatorPainter::minimumOpacity() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->minimumOpacity;
}

void QnBusyIndicatorPainter::setMinimumOpacity(qreal opacity)
{
    Q_D(QnBusyIndicatorPainter);
    d->minimumOpacity = opacity;
    updateIndicator();
}

int QnBusyIndicatorPainter::dotLagMs() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->dotLagMs;
}

void QnBusyIndicatorPainter::setDotLagMs(int lag)
{
    Q_D(QnBusyIndicatorPainter);
    if (d->dotLagMs == lag)
        return;

    d->dotLagMs = lag;
    updateIndicator();
}

unsigned int QnBusyIndicatorPainter::downTimeMs() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->downTimeMs;
}

void QnBusyIndicatorPainter::setDownTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicatorPainter);
    if (d->downTimeMs == ms)
        return;

    d->downTimeMs = ms;
    d->timeChanged();
    updateIndicator();
}

unsigned int QnBusyIndicatorPainter::fadeInTimeMs() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->fadeInTimeMs;
}

void QnBusyIndicatorPainter::setFadeInTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicatorPainter);
    if (d->fadeInTimeMs == ms)
        return;

    d->fadeInTimeMs = ms;
    d->timeChanged();
    updateIndicator();
}

unsigned int QnBusyIndicatorPainter::upTimeMs() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->upTimeMs;
}

void QnBusyIndicatorPainter::setUpTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicatorPainter);
    if (d->upTimeMs == ms)
        return;

    d->upTimeMs = ms;
    d->timeChanged();
    updateIndicator();
}

unsigned int QnBusyIndicatorPainter::fadeOutTimeMs() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->fadeOutTimeMs;
}

void QnBusyIndicatorPainter::setFadeOutTimeMs(unsigned int ms)
{
    Q_D(QnBusyIndicatorPainter);
    if (d->fadeOutTimeMs == ms)
        return;

    d->fadeOutTimeMs = ms;
    d->timeChanged();
    updateIndicator();
}

unsigned int QnBusyIndicatorPainter::totalTimeMs() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->totalTimeMs;
}

void QnBusyIndicatorPainter::setAnimationTimesMs(unsigned int downTimeMs, unsigned int fadeInTimeMs, unsigned int upTimeMs, unsigned int fadeOutTimeMs)
{
    Q_D(QnBusyIndicatorPainter);
    bool changed = downTimeMs != d->downTimeMs || fadeInTimeMs != d->fadeInTimeMs ||
                   upTimeMs != d->upTimeMs || fadeOutTimeMs != d->fadeOutTimeMs;

    d->downTimeMs = downTimeMs;
    d->fadeInTimeMs = fadeInTimeMs;
    d->upTimeMs = upTimeMs;
    d->fadeOutTimeMs = fadeOutTimeMs;

    if (changed)
    {
        d->timeChanged();
        updateIndicator();
    }
}

QSize QnBusyIndicatorPainter::indicatorSize() const
{
    Q_D(const QnBusyIndicatorPainter);
    return d->indicatorSize;
}

void QnBusyIndicatorPainter::paintIndicator(QPainter* painter, const QPointF& origin)
{
    Q_D(const QnBusyIndicatorPainter);

    QnScopedPainterOpacityRollback opacityRollback(painter);

    unsigned int dotSize = d->dotSize();
    qreal centerDistance = dotSize + d->dotSpacing;

    qreal halfSize = dotSize * 0.5;
    QPointF center(origin.x() + halfSize, origin.y() + halfSize);

    int timeMs = d->currentTimeMs;

    for (unsigned int i = 0; i != d->dotCount; ++i)
    {
        painter->setOpacity(d->opacityFromTime(timeMs));
        painter->drawEllipse(center, d->dotRadius, d->dotRadius);

        center.setX(center.x() + centerDistance);
        timeMs -= d->dotLagMs;
    }
}

void QnBusyIndicatorPainter::indicatorSizeChanged()
{
}

void QnBusyIndicatorPainter::updateIndicator()
{
}

void QnBusyIndicatorPainter::tick(int deltaMs)
{
    Q_D(QnBusyIndicatorPainter);
    if (!d->totalTimeMs)
        return;

    d->currentTimeMs = (d->currentTimeMs + deltaMs) % d->totalTimeMs;
    updateIndicator();
}

/*
* QnBusyIndicatorWidget
*/

QnBusyIndicatorWidget::QnBusyIndicatorWidget(QWidget* parent) :
    QWidget(parent)
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

    paintIndicator(&painter, indicatorRect().topLeft());
}

void QnBusyIndicatorWidget::indicatorSizeChanged()
{
    updateGeometry();
}

void QnBusyIndicatorWidget::updateIndicator()
{
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

    paintIndicator(painter, indicatorRect().topLeft());
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

void QnBusyIndicatorGraphicsWidget::updateIndicator()
{
    update(indicatorRect());
}

QRectF QnBusyIndicatorGraphicsWidget::indicatorRect() const
{
    return QnGeometry::aligned(QSizeF(indicatorSize()), contentsRect());
}
