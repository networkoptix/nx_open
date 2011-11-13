#include "animatedwidget.h"

#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QApplication>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QPainter>

#include "graphicswidget_p.h"

class AnimatedWidgetPrivate: public GraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(AnimatedWidget)

public:
    AnimatedWidgetPrivate();
    ~AnimatedWidgetPrivate();

private:
    void init();

    void drawRotationHelper(QPainter *painter, const QPointF &m_rotation_center, const QPointF &m_rotation_hand);

    inline void stopAllAnimations()
    {
        posAnimation->stop();
        sizeAnimation->stop();
        scaleAnimation->stop();
        rotationAnimation->stop();
        opacityAnimation->stop();
    }

private:
    ushort animationsEnabled : 1;

    ushort inSetGeometry : 1;
    ushort inSetScale : 1;
    ushort inSetRotation : 1;
    ushort inSetOpacity : 1;

    QPropertyAnimation *posAnimation;
    QPropertyAnimation *sizeAnimation;
    QPropertyAnimation *scaleAnimation;
    QPropertyAnimation *rotationAnimation;
    QPropertyAnimation *opacityAnimation;
};

AnimatedWidgetPrivate::AnimatedWidgetPrivate()
    : GraphicsWidgetPrivate(),
      animationsEnabled(false),
      inSetGeometry(false), inSetScale(false), inSetRotation(false), inSetOpacity(false),
      posAnimation(0), sizeAnimation(0), scaleAnimation(0), rotationAnimation(0), opacityAnimation(0)
{
}

AnimatedWidgetPrivate::~AnimatedWidgetPrivate()
{
}

void AnimatedWidgetPrivate::init()
{
    Q_Q(AnimatedWidget);

    Qt::WindowFlags wFlags = Qt::Window | Qt::FramelessWindowHint;
    //wFlags |= Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
    q->setWindowFlags(wFlags);
    q->setAttribute(Qt::WA_DeleteOnClose);

    q->setFlag(QGraphicsItem::ItemSendsGeometryChanges);

    q->setInteractive(false);

    //q->setFiltersChildEvents(true); ### investigate
    q->setHandlesChildEvents(true);

    posAnimation = new QPropertyAnimation(q, "instantPos", q);
    posAnimation->setDuration(1000);
    posAnimation->setEasingCurve(QEasingCurve::InOutBack);

    sizeAnimation = new QPropertyAnimation(q, "instantSize", q);
    sizeAnimation->setDuration(1000);
    sizeAnimation->setEasingCurve(QEasingCurve::InOutQuint);

    scaleAnimation = new QPropertyAnimation(q, "instantScale", q);
    scaleAnimation->setDuration(500);
    scaleAnimation->setEasingCurve(QEasingCurve::InOutBack);

    rotationAnimation = new QPropertyAnimation(q, "instantRotation", q);
    rotationAnimation->setDuration(500);
    rotationAnimation->setEasingCurve(QEasingCurve::InOutBack);

    opacityAnimation = new QPropertyAnimation(q, "instantOpacity", q);
    opacityAnimation->setDuration(500);
    opacityAnimation->setEasingCurve(QEasingCurve::InOutSine);
}

void AnimatedWidgetPrivate::drawRotationHelper(QPainter *painter, const QPointF &m_rotation_center, const QPointF &m_rotation_hand)
{
    static QColor color1(255, 0, 0, 250);
    static QColor color2(255, 0, 0, 100);

    static const int r = 30;
    static const int penWidth = 6;
    static const int p_line_len = 220;
    static const int arrowSize = 30;

    QRadialGradient gradient(m_rotation_center, r);
    gradient.setColorAt(0, color1);
    gradient.setColorAt(1, color2);

    painter->save();

    painter->setPen(QPen(color2, 0, Qt::SolidLine));

    painter->setBrush(gradient);
    painter->drawEllipse(m_rotation_center, r, r);

    painter->setPen(QPen(color2, penWidth, Qt::SolidLine));
    painter->drawLine(m_rotation_center, m_rotation_hand);

    // building new line
    QLineF line(m_rotation_hand, m_rotation_center);
    QLineF line_p = line.unitVector().normalVector();
    line_p.setLength(p_line_len/2);

    line_p = QLineF(line_p.p2(),line_p.p1());
    line_p.setLength(p_line_len);

    painter->drawLine(line_p);

    double angle = qAcos(line_p.dx() / line_p.length());
    if (line_p.dy() >= 0)
        angle = M_PI * 2 - angle;

    qreal s = 2.5;

    QPointF sourceArrowP1 = line_p.p1() + QPointF(qSin(angle + M_PI / s) * arrowSize, qCos(angle + M_PI / s) * arrowSize);
    QPointF sourceArrowP2 = line_p.p1() + QPointF(qSin(angle + M_PI - M_PI / s) * arrowSize, qCos(angle + M_PI - M_PI / s) * arrowSize);
    QPointF destArrowP1 = line_p.p2() + QPointF(qSin(angle - M_PI / s) * arrowSize, qCos(angle - M_PI / s) * arrowSize);
    QPointF destArrowP2 = line_p.p2() + QPointF(qSin(angle - M_PI + M_PI / s) * arrowSize, qCos(angle - M_PI + M_PI / s) * arrowSize);

    painter->setBrush(color2);
    painter->drawPolygon(QPolygonF() << line_p.p1() << sourceArrowP1 << sourceArrowP2);
    painter->drawPolygon(QPolygonF() << line_p.p2() << destArrowP1 << destArrowP2);

    painter->restore();
}


AnimatedWidget::AnimatedWidget(QGraphicsItem *parent)
    : GraphicsWidget(*new AnimatedWidgetPrivate, parent)
{
    Q_D(AnimatedWidget);
    d->init();
}

/*!
    \internal
*/
AnimatedWidget::AnimatedWidget(AnimatedWidgetPrivate &dd, QGraphicsItem *parent)
    : GraphicsWidget(dd, parent)
{
    Q_D(AnimatedWidget);
    d->init();
}

AnimatedWidget::~AnimatedWidget()
{
}

bool AnimatedWidget::isInteractive() const
{
    return (windowFlags() & Qt::FramelessWindowHint) == 0;
}

void AnimatedWidget::setInteractive(bool interactive)
{
    if (interactive)
        setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
    else
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setFlag(ItemIsMovable, interactive);
    //setExtraFlag(ItemIsDraggable, interactive);
    //setExtraFlag(ItemIsResizable, interactive);
}

bool AnimatedWidget::isAnimationsEnabled() const
{
    Q_D(const AnimatedWidget);

    return d->animationsEnabled;
}

void AnimatedWidget::setAnimationsEnabled(bool enable)
{
    Q_D(AnimatedWidget);

    if (d->animationsEnabled == enable)
        return;

    d->animationsEnabled = enable;
    if (!d->animationsEnabled) {
        // ### finish active transitions at once?

        d->stopAllAnimations();
    }
}

QPainterPath AnimatedWidget::opaqueArea() const
{
    if (qFuzzyCompare(effectiveOpacity(), 1.0))
        return shape(); // optimization

    return GraphicsWidget::opaqueArea();
}

void AnimatedWidget::setGeometry(const QRectF &rect)
{
    if ((flags() & ItemSendsGeometryChanges) == 0) {
        // resize and repositition, if needed
        GraphicsWidget::setGeometry(rect);
        return;
    }

    // notify the item that the size is changing
    const QVariant newSizeVariant(itemChange(ItemSizeChange, rect.size()));
    const QSizeF newSize = newSizeVariant.toSizeF();
    const QRectF newGeom = QRectF(rect.topLeft(), newSize);
    if (newGeom == geometry())
        return;

    // resize and repositition
    GraphicsWidget::setGeometry(newGeom);

    // send post-notification
    itemChange(ItemSizeHasChanged, newSizeVariant);
}

void AnimatedWidget::updateGeometry()
{
    Q_D(AnimatedWidget);

    d->inSetGeometry = true;
    GraphicsWidget::updateGeometry();
    d->inSetGeometry = false;
}

QVariant AnimatedWidget::itemChange(GraphicsItemChange change, const QVariant &value)
{
    Q_D(AnimatedWidget);

    switch (change) {
    case ItemFlagsChange:
        return (value.toUInt() | ItemSendsGeometryChanges | ItemUsesExtendedStyleOption)/* & ~ItemIsPanel*/;

    case ItemPositionChange:
        if (d->animationsEnabled && !d->inSetGeometry && d->posAnimation) {
            const QPointF newPos = value.toPointF();
            if (d->posAnimation->state() != QAbstractAnimation::Stopped && d->posAnimation->endValue().toPointF() == newPos)
                return pos();

            d->posAnimation->stop();

            d->posAnimation->setEndValue(newPos);
            d->posAnimation->start();

            return pos();
        }
        break;

    case ItemSizeChange:
        if (d->animationsEnabled && !d->inSetGeometry && d->sizeAnimation) {
            const QSizeF newSize = value.toSizeF();
            if (d->sizeAnimation->state() != QAbstractAnimation::Stopped && d->sizeAnimation->endValue().toSizeF() == newSize)
                return size();

            d->sizeAnimation->stop();

            d->sizeAnimation->setEndValue(newSize);
            d->sizeAnimation->start();

            return size();
        }
        break;

    case ItemScaleChange:
        if (d->animationsEnabled && !d->inSetScale && d->scaleAnimation) {
            float newScale = value.toFloat();
            if (qIsFinite(newScale)) {
                newScale = qBound(1.0f, newScale, 5.0f);
                if (d->scaleAnimation->state() != QAbstractAnimation::Stopped && qFuzzyCompare(d->scaleAnimation->endValue().toFloat(), newScale))
                    return scale();

                d->scaleAnimation->stop();

                d->scaleAnimation->setEndValue(newScale);
                d->scaleAnimation->start();
            }

            return scale();
        }
        break;

    case ItemRotationChange:
        if (d->animationsEnabled && !d->inSetRotation && d->rotationAnimation) {
            const float newRotation = value.toFloat();
            if (qIsFinite(newRotation)) {
                if (d->rotationAnimation->state() != QAbstractAnimation::Stopped && qFuzzyCompare(d->rotationAnimation->endValue().toFloat(), newRotation))
                    return rotation();

                d->rotationAnimation->stop();

                d->rotationAnimation->setEndValue(newRotation);
                d->rotationAnimation->start();
            }

            return rotation();
        }
        break;

    case ItemOpacityChange:
        if (!d->inSetOpacity && d->opacityAnimation) {
            const float newOpacity = value.toFloat();
            if (qIsFinite(newOpacity)) {
                if (d->opacityAnimation->state() != QAbstractAnimation::Stopped && qFuzzyCompare(d->opacityAnimation->endValue().toFloat(), newOpacity))
                    return opacity();

                d->opacityAnimation->stop();

                d->opacityAnimation->setEndValue(newOpacity);
                d->opacityAnimation->start();
            }

            return opacity();
        }
        break;

    default:
        break;
    }

    return GraphicsWidget::itemChange(change, value);
}

QPointF AnimatedWidget::instantPos() const
{
    return pos();
}

QSizeF AnimatedWidget::instantSize() const
{
    return size();
}

qreal AnimatedWidget::instantScale() const
{
    return scale();
}

qreal AnimatedWidget::instantRotation() const
{
    return rotation();
}

qreal AnimatedWidget::instantOpacity() const
{
    return opacity();
}

void AnimatedWidget::setInstantPos(const QPointF &value)
{
    Q_D(AnimatedWidget);

    d->inSetGeometry = true;
    setPos(value);
    d->inSetGeometry = false;
}

void AnimatedWidget::setInstantSize(const QSizeF &value)
{
    Q_D(AnimatedWidget);

    d->inSetGeometry = true;
    resize(value);
    d->inSetGeometry = false;
}

void AnimatedWidget::setInstantScale(qreal value)
{
    Q_D(AnimatedWidget);

    d->inSetScale = true;
    setScale(value);
    d->inSetScale = false;
}

void AnimatedWidget::setInstantRotation(qreal value)
{
    Q_D(AnimatedWidget);

    d->inSetRotation = true;
    setRotation(value);
    d->inSetRotation = false;
}

void AnimatedWidget::setInstantOpacity(qreal value)
{
    Q_D(AnimatedWidget);

    d->inSetOpacity = true;
    setOpacity(value);
    d->inSetOpacity = false;
}

