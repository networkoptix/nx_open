#include "busy_indicator.h"

#include <QtWidgets/QStyle>
#include <cmath>

#include <utils/common/scoped_painter_rollback.h>
#include <ui/animation/animation_timer.h>
#include <nx/client/core/utils/geometry.h>

using nx::client::core::utils::Geometry;

/*
* QnBusyIndicatorBase
*/

QnBusyIndicatorBase::QnBusyIndicatorBase(QObject* parent) :
    base_type(parent)
{
    auto animationTimer = new QAnimationTimer(this);
    animationTimer->addListener(this);
    startListening();
}

QnBusyIndicatorBase::~QnBusyIndicatorBase()
{
}


/*
* QnBusyIndicatorPrivate
*/

class QnBusyIndicatorPrivate
{
public:
    QnBusyIndicatorPrivate() :
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

QnBusyIndicator::QnBusyIndicator(QObject* parent) :
    base_type(parent),
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
    emit sizeChanged();
}

qreal QnBusyIndicator::dotRadius() const
{
    Q_D(const QnBusyIndicator);
    return d->dotRadius;
}

void QnBusyIndicator::setDotRadius(qreal radius)
{
    Q_D(QnBusyIndicator);
    radius = qMax(0.0, radius);

    QSize oldSize = d->indicatorSize;

    d->dotRadius = radius;
    d->sizeChanged();

    if (oldSize != d->indicatorSize)
        emit sizeChanged();

    emit updated();
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
    emit sizeChanged();
}

qreal QnBusyIndicator::minimumOpacity() const
{
    Q_D(const QnBusyIndicator);
    return d->minimumOpacity;
}

void QnBusyIndicator::setMinimumOpacity(qreal opacity)
{
    Q_D(QnBusyIndicator);
    d->minimumOpacity = opacity;
    emit updated();
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
    emit updated();
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
    emit updated();
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
    emit updated();
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
    emit updated();
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
    emit updated();
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
    {
        d->timeChanged();
        emit updated();
    }
}

QSize QnBusyIndicator::size() const
{
    Q_D(const QnBusyIndicator);
    return d->indicatorSize;
}

void QnBusyIndicator::paint(QPainter* painter)
{
    Q_D(const QnBusyIndicator);

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

void QnBusyIndicator::tick(int deltaMs)
{
    Q_D(QnBusyIndicator);
    if (!d->totalTimeMs)
        return;

    d->currentTimeMs = (d->currentTimeMs + deltaMs) % d->totalTimeMs;
    emit updated();
}

/*
* QnBusyIndicatorWidget
*/

QnBusyIndicatorWidget::QnBusyIndicatorWidget(QWidget* parent) :
    base_type(parent),
    m_indicator(new QnBusyIndicator()),
    m_indicatorRole(QPalette::Foreground),
    m_borderRole(QPalette::NoRole)
{
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    connect(m_indicator, &QnBusyIndicator::sizeChanged, this, &QWidget::updateGeometry);
    connect(m_indicator, &QnBusyIndicator::updated, this, &QnBusyIndicatorWidget::updateIndicator);
}

QnBusyIndicator* QnBusyIndicatorWidget::dots() const
{
    return m_indicator.data();
}

QPalette::ColorRole QnBusyIndicatorWidget::indicatorRole() const
{
    return m_indicatorRole;
}

void QnBusyIndicatorWidget::setIndicatorRole(QPalette::ColorRole role)
{
    if (m_indicatorRole == role)
        return;

    m_indicatorRole = role;
    updateIndicator();
}

QPalette::ColorRole QnBusyIndicatorWidget::borderRole() const
{
    return m_borderRole;
}

void QnBusyIndicatorWidget::setBorderRole(QPalette::ColorRole role)
{
    if (m_borderRole == role)
        return;

    m_borderRole = role;
    updateIndicator();
}

QSize QnBusyIndicatorWidget::minimumSizeHint() const
{
    QMargins margins = contentsMargins();
    QSize sizeOfMargins(margins.left() + margins.right(), margins.top() + margins.bottom());
    return m_indicator->size() + sizeOfMargins;
}

QSize QnBusyIndicatorWidget::sizeHint() const
{
    /* Make sure we don't call here minimumSizeHint overridden in a descendant: */
    return QnBusyIndicatorWidget::minimumSizeHint();
}

void QnBusyIndicatorWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

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

void QnBusyIndicatorWidget::paint(QPainter* painter)
{
    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(indicatorRect().topLeft());
    m_indicator->paint(painter);
}

void QnBusyIndicatorWidget::updateIndicator()
{
    update(indicatorRect());
}

QRect QnBusyIndicatorWidget::indicatorRect() const
{
    return QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
        m_indicator->size(), contentsRect());
}

/*
* QnBusyIndicatorGraphicsWidget
*/

QnBusyIndicatorGraphicsWidget::QnBusyIndicatorGraphicsWidget(QGraphicsItem* parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    m_indicator(new QnBusyIndicator()),
    m_indicatorColor(palette().color(QPalette::Foreground)),
    m_borderColor(QColor())
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    connect(m_indicator, &QnBusyIndicator::sizeChanged, this, &QnBusyIndicatorGraphicsWidget::updateGeometry);
    connect(m_indicator, &QnBusyIndicator::updated, this, &QnBusyIndicatorGraphicsWidget::updateIndicator);
}

void QnBusyIndicatorGraphicsWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

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

QnBusyIndicator* QnBusyIndicatorGraphicsWidget::dots() const
{
    return m_indicator.data();
}

QColor QnBusyIndicatorGraphicsWidget::indicatorColor() const
{
    return m_indicatorColor;
}

void QnBusyIndicatorGraphicsWidget::setIndicatorColor(QColor color)
{
    if (m_indicatorColor == color)
        return;

    m_indicatorColor = color;
    updateIndicator();
}

QColor QnBusyIndicatorGraphicsWidget::borderColor() const
{
    return m_borderColor;
}

void QnBusyIndicatorGraphicsWidget::setBorderColor(QColor color)
{
    if (m_borderColor == color)
        return;

    m_borderColor = color;
    updateIndicator();
}

QSizeF QnBusyIndicatorGraphicsWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    switch (which)
    {
        case Qt::MinimumSize:
        case Qt::PreferredSize:
        {
            QMarginsF margins = contentsMargins();
            QSizeF sizeOfMargins(margins.left() + margins.right(), margins.top() + margins.bottom());
            return QSizeF(m_indicator->size() + sizeOfMargins).expandedTo(constraint);
        }

        default:
        {
            return base_type::sizeHint(which, constraint);
        }
    }
}

void QnBusyIndicatorGraphicsWidget::updateIndicator()
{
    update(indicatorRect());
}

QRectF QnBusyIndicatorGraphicsWidget::indicatorRect() const
{
    return Geometry::aligned(QSizeF(m_indicator->size()), contentsRect());
}
