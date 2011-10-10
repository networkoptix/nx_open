#include "animatedwidget.h"

#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QApplication>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QPainter>

#include "notifyingwidget_p.h"

class AnimatedWidgetPrivate: public NotifyingWidgetPrivate
{
public:
    AnimatedWidgetPrivate();
    ~AnimatedWidgetPrivate();

private:
    void init();

    void drawRotationHelper(QPainter *painter, const QPointF &m_rotation_center, const QPointF &m_rotation_hand);

    inline QPointF instantPos() const { return q_func()->pos(); }
    inline void setInstantPos(const QPointF &pos)
    {
        inSetGeometry = true;
        q_func()->setPos(pos);
        inSetGeometry = false;
    }

    inline QSizeF instantSize() const { return q_func()->size(); }
    inline void setInstantSize(const QSizeF &size)
    {
        inSetGeometry = true;
        q_func()->resize(size);
        inSetGeometry = false;
    }

    inline float instantScale() const { return q_func()->scale(); }
    inline void setInstantScale(float scale)
    {
        inSetScale = true;
        q_func()->setScale(scale);
        inSetScale = false;
    }

    inline float instantRotation() const { return q_func()->rotation(); }
    inline void setInstantRotation(float rotation)
    {
        inSetRotation = true;
        q_func()->setRotation(rotation);
        inSetRotation = false;
    }

    inline float instantOpacity() const { return q_func()->opacity(); }
    inline void setInstantOpacity(float opacity)
    {
        inSetOpacity = true;
        q_func()->setOpacity(opacity);
        inSetOpacity = false;
    }

private:
    ushort inSetGeometry : 1;
    ushort inSetScale : 1;
    ushort inSetRotation : 1;
    ushort inSetOpacity : 1;

    ushort childIsGrabberItem : 1;

    QParallelAnimationGroup *animationGroup;
    QPropertyAnimation *posAnimation;
    QPropertyAnimation *sizeAnimation;
    QPropertyAnimation *scaleAnimation;
    QPropertyAnimation *rotationAnimation;
    QPropertyAnimation *opacityAnimation;

    Q_DECLARE_PUBLIC(AnimatedWidget);
};

AnimatedWidgetPrivate::AnimatedWidgetPrivate()
    : inSetGeometry(false), inSetScale(false), inSetRotation(false), inSetOpacity(false),
      childIsGrabberItem(false),
      animationGroup(0),
      posAnimation(0), sizeAnimation(0), scaleAnimation(0), rotationAnimation(0), opacityAnimation(0)
{
}

AnimatedWidgetPrivate::~AnimatedWidgetPrivate()
{
    if (animationGroup)
    {
        animationGroup->stop();
        delete animationGroup;
    }
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

    animationGroup = new QParallelAnimationGroup(q);

    posAnimation = new QPropertyAnimation(q, "instantPos", animationGroup);
    posAnimation->setDuration(1000);
    posAnimation->setEasingCurve(QEasingCurve::InOutBack);
    animationGroup->addAnimation(posAnimation);

    sizeAnimation = new QPropertyAnimation(q, "instantSize", animationGroup);
    sizeAnimation->setDuration(1000);
    sizeAnimation->setEasingCurve(QEasingCurve::InOutQuint);
    animationGroup->addAnimation(sizeAnimation);

    scaleAnimation = new QPropertyAnimation(q, "instantScale", animationGroup);
    scaleAnimation->setDuration(500);
    scaleAnimation->setEasingCurve(QEasingCurve::InOutBack);
    animationGroup->addAnimation(scaleAnimation);

    rotationAnimation = new QPropertyAnimation(q, "instantRotation", animationGroup);
    rotationAnimation->setDuration(500);
    rotationAnimation->setEasingCurve(QEasingCurve::InOutBack);
    animationGroup->addAnimation(rotationAnimation);

    opacityAnimation = new QPropertyAnimation(q, "instantOpacity", animationGroup);
    opacityAnimation->setDuration(500);
    opacityAnimation->setEasingCurve(QEasingCurve::InOutSine);
    animationGroup->addAnimation(opacityAnimation);
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
    : base_type(parent, new AnimatedWidgetPrivate())
{
    Q_D(AnimatedWidget);

    d->init();
}

AnimatedWidget::~AnimatedWidget()
{
    return;
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
}

QAnimationGroup *AnimatedWidget::animationGroup() const
{
    Q_D(const AnimatedWidget);

    return d->animationGroup;
}

QPainterPath AnimatedWidget::opaqueArea() const
{
    if (qFuzzyCompare(effectiveOpacity(), 1.0))
        return shape(); // optimization

    return base_type::opaqueArea();
}

void AnimatedWidget::setGeometry(const QRectF &rect)
{
    if ((flags() & ItemSendsGeometryChanges) == 0)
    {
        // resize and repositition, if needed
        base_type::setGeometry(rect);
        return;
    }

    // notify the item that the size is changing
    const QVariant newSizeVariant(itemChange(GraphicsItemChange(ItemSizeChange), rect.size()));
    const QSizeF newSize = newSizeVariant.toSizeF();
    const QRectF newGeom = QRectF(rect.topLeft(), newSize);
    if (newGeom == geometry())
        return;

    // resize and repositition
    base_type::setGeometry(newGeom);

    // send post-notification
    itemChange(GraphicsItemChange(ItemSizeHasChanged), newSizeVariant);
}

void AnimatedWidget::updateGeometry()
{
    Q_D(AnimatedWidget);

    d->inSetGeometry = true;
    base_type::updateGeometry();
    d->inSetGeometry = false;
}

QVariant AnimatedWidget::itemChange(GraphicsItemChange change, const QVariant &value)
{
    Q_D(AnimatedWidget);

    switch (int(change))
    {
    case ItemFlagsChange:
        return (value.toUInt() | ItemSendsGeometryChanges | ItemUsesExtendedStyleOption)/* & ~ItemIsPanel*/;

    case ItemPositionChange:
        if (!d->inSetGeometry && d->posAnimation && scene())
        {
            const QPointF newPos = value.toPointF();

            if (d->posAnimation->state() != QAbstractAnimation::Stopped && d->posAnimation->endValue().toPointF() == newPos)
                return pos();

            d->posAnimation->stop();

            QGraphicsItem *mouseGrabberItem = scene()->mouseGrabberItem();
            if (!isInteractive() || (!isSelected() && (!mouseGrabberItem || (mouseGrabberItem != this/* && !isAncestorOf(mouseGrabberItem)*/))))
            {
                d->posAnimation->setEndValue(newPos);
                d->posAnimation->start();

                return pos();
            }
        }
        break;

    case ItemSizeChange:
        if (!d->inSetGeometry && d->sizeAnimation && scene())
        {
            const QSizeF newSize = value.toSizeF();

            if (d->sizeAnimation->state() != QAbstractAnimation::Stopped && d->sizeAnimation->endValue().toSizeF() == newSize)
                return size();

            d->sizeAnimation->stop();

            QGraphicsItem *mouseGrabberItem = scene()->mouseGrabberItem();
            //if (!isInteractive() || !isSelected() || !mouseGrabberItem/* || (mouseGrabberItem != this && !isAncestorOf(mouseGrabberItem))*/)
            if (!isInteractive() || (!isSelected() && (!mouseGrabberItem || (mouseGrabberItem != this/* && !isAncestorOf(mouseGrabberItem)*/))))
            {
                d->sizeAnimation->setEndValue(newSize);
                d->sizeAnimation->start();

                return size();
            }
        }
        break;

    case ItemScaleChange:
        if (!d->inSetScale && d->scaleAnimation)
        {
            float newScale = value.toFloat();
            if (qIsFinite(newScale))
            {
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
        if (!d->inSetRotation && d->rotationAnimation && scene())
        {
            const float newRotation = value.toFloat();
            if (qIsFinite(newRotation))
            {
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
        if (!d->inSetOpacity && d->opacityAnimation && scene())
        {
            const float newOpacity = value.toFloat();
            if (qIsFinite(newOpacity))
            {
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

    return base_type::itemChange(change, value);
}

bool AnimatedWidget::windowFrameEvent(QEvent *event)
{
#if 0
    if ((flags() & ItemIsPanel))
    {
        switch (event->type())
        {
        case QEvent::GraphicsSceneMousePress:
        case QEvent::GraphicsSceneMouseRelease:
            return base_type::windowFrameEvent(event)
                   && windowFrameSectionAt(static_cast<QGraphicsSceneMouseEvent *>(event)->pos()) != Qt::TitleBarArea;

        case QEvent::GraphicsSceneMouseMove:
            if (windowFrameSectionAt(static_cast<QGraphicsSceneMouseEvent *>(event)->pos()) == Qt::TitleBarArea)
            {
                event->accept();
                return false;
            }
            break;

        default:
            break;
        }
    }
#endif

    return base_type::windowFrameEvent(event);
}

void AnimatedWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mousePressEvent(event);

#if 0
    if (event->button() == Qt::LeftButton && !isInteractive())
        event->accept();
#endif
}

void AnimatedWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mouseReleaseEvent(event);

#if 0
    if (event->button() == Qt::LeftButton && !isInteractive())
    {
        event->accept();
        if ((event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength() < QApplication::startDragDistance())
            QMetaObject::invokeMethod(this, "clicked", Qt::QueuedConnection);
    }
#endif
}

void AnimatedWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
#if 1
    base_type::mouseDoubleClickEvent(event);
#else
    if (event->button() == Qt::LeftButton && !isInteractive())
        QMetaObject::invokeMethod(this, "doubleClicked", Qt::QueuedConnection);
    else
        base_type::mouseDoubleClickEvent(event);
#endif
}

void AnimatedWidget::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (isInteractive())
    {
        setTransformOriginPoint(rect().center());
        if (event->modifiers() & Qt::AltModifier)
            setRotation(rotation() + ((event->delta() / 8) / 15) * 15);
        else
            setScale(scale() + float(event->delta()) / 240);
    }
}

#include "moc_animatedwidget.cpp"
