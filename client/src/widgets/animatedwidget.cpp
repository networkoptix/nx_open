#include "animatedwidget.h"

#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QApplication>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QPainter>

#include <qdebug.h>

class AnimatedWidgetPrivate
{
public:
    AnimatedWidgetPrivate();
    ~AnimatedWidgetPrivate();

    void init();

    AnimatedWidget *q;

    inline QPointF instantPos() const { return q->pos(); }
    inline void setInstantPos(const QPointF &pos)
    {
        inSetGeometry = true;
        q->setPos(pos);
        inSetGeometry = false;
    }

    inline QSizeF instantSize() const { return q->size(); }
    inline void setInstantSize(const QSizeF &size)
    {
        inSetGeometry = true;
        q->resize(size);
        inSetGeometry = false;
    }

    inline qreal instantScale() const { return q->scale(); }
    inline void setInstantScale(const qreal &scale)
    {
        inSetScale = true;
        q->setScale(scale);
        inSetScale = false;
    }

    inline qreal instantRotation() const { return q->rotation(); }
    inline void setInstantRotation(const qreal &rotation)
    {
        inSetRotation = true;
        q->setRotation(rotation);
        inSetRotation = false;
    }

    inline qreal instantOpacity() const { return q->opacity(); }
    inline void setInstantOpacity(const qreal &opacity)
    {
        inSetOpacity = true;
        q->setOpacity(opacity);
        inSetOpacity = false;
    }

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
    Qt::WindowFlags wFlags = Qt::Window | Qt::FramelessWindowHint;
    //wFlags |= Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
    q->setWindowFlags(wFlags);
    q->setAttribute(Qt::WA_DeleteOnClose);

    q->setFlag(QGraphicsItem::ItemSendsGeometryChanges);

    q->setInteractive(false);

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


AnimatedWidget::AnimatedWidget(QGraphicsItem *parent)
    : QGraphicsWidget(parent), d(new AnimatedWidgetPrivate)
{
    d->q = this;
    d->init();
}

AnimatedWidget::~AnimatedWidget()
{
    delete d;
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

QPainterPath AnimatedWidget::opaqueArea() const
{
    if (qFuzzyCompare(effectiveOpacity(), 1.0))
        return shape(); // optimization

    return QGraphicsWidget::opaqueArea();
}

void AnimatedWidget::setGeometry(const QRectF &rect)
{
    if ((flags() & ItemSendsGeometryChanges) == 0)
    {
        // resize and repositition, if needed
        QGraphicsWidget::setGeometry(rect);
        return;
    }

    // notify the item that the size is changing
    const QVariant newSizeVariant(itemChange(GraphicsItemChange(ItemSizeChange), rect.size()));
    const QSizeF newSize = newSizeVariant.toSizeF();
    const QRectF newGeom = QRectF(rect.topLeft(), newSize);
    if (newGeom == geometry())
        return;

    // resize and repositition
    QGraphicsWidget::setGeometry(newGeom);

    // send post-notification
    itemChange(GraphicsItemChange(ItemSizeHasChanged), newSizeVariant);
}

QVariant AnimatedWidget::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (int(change))
    {
    case ItemFlagsChange:
        return (value.toUInt() | ItemSendsGeometryChanges | ItemUsesExtendedStyleOption)/* & ~ItemIsPanel*/;

    case ItemPositionChange:
        if (!d->inSetGeometry && d->posAnimation && scene())
        {
            const QPointF newPos = value.toPointF();

            if(d->posAnimation->state() != QAbstractAnimation::Stopped && d->posAnimation->endValue().toPointF() == newPos)
                return pos();

            d->posAnimation->stop();

            /* NOTE: 
             * The following code won't work if the animation is not stopped and newPos equals animation's end value.
             * Thus the early return and the stop() call above.
             * 
             * If the current 'real' progress determined by the easing curve lies outside the [0, 1] interval, 
             * code in QVariantAnimationPrivate::recalculateCurrentInterval goes the wrong path, 
             * ending up dividing by zero in QVariantAnimationPrivate::setCurrentValueForProgress
             * and passing the result (1.#INF) to interpolator. 
             * The result is a graphics item that is positioned at +inf.
             */

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

            if (d->sizeAnimation->state() != QAbstractAnimation::Stopped && d->sizeAnimation->endValue().toSizeF() != newSize)
                d->sizeAnimation->stop();

            QGraphicsItem *mouseGrabberItem = scene()->mouseGrabberItem();
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
            qreal newScale = value.toReal();
            if (qIsFinite(newScale))
            {
                newScale = qBound(1.0, newScale, 5.0);

                if (d->scaleAnimation->state() != QAbstractAnimation::Stopped && d->scaleAnimation->endValue().toReal() != newScale)
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
            const qreal newRotation = value.toReal();
            if (qIsFinite(newRotation))
            {
                if (d->rotationAnimation->state() != QAbstractAnimation::Stopped && d->rotationAnimation->endValue().toReal() != newRotation)
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
            const qreal newOpacity = value.toReal();
            if (qIsFinite(newOpacity))
            {
                if (d->opacityAnimation->state() != QAbstractAnimation::Stopped && d->opacityAnimation->endValue().toReal() != newOpacity)
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

    return QGraphicsWidget::itemChange(change, value);
}

bool AnimatedWidget::windowFrameEvent(QEvent *event)
{
    if ((flags() & ItemIsPanel))
    {
        switch (event->type())
        {
        case QEvent::GraphicsSceneMousePress:
        case QEvent::GraphicsSceneMouseRelease:
            return QGraphicsWidget::windowFrameEvent(event)
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

    return QGraphicsWidget::windowFrameEvent(event);
}

void AnimatedWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsWidget::mousePressEvent(event);

    if (event->button() == Qt::LeftButton && !isInteractive())
        event->accept();
}

void AnimatedWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsWidget::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton && !isInteractive())
    {
        event->accept();
        if ((event->pos() - event->buttonDownPos(Qt::LeftButton)).manhattanLength() < QApplication::startDragDistance())
            QMetaObject::invokeMethod(this, "clicked", Qt::QueuedConnection);
    }
}

void AnimatedWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isInteractive())
        QMetaObject::invokeMethod(this, "doubleClicked", Qt::QueuedConnection);
    else
        QGraphicsWidget::mouseDoubleClickEvent(event);
}

void AnimatedWidget::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (isInteractive())
    {
        setTransformOriginPoint(rect().center());
        if (event->modifiers() & Qt::AltModifier)
            setRotation(rotation() + ((event->delta() / 8) / 15) * 15);
        else
            setScale(scale() + qreal(event->delta()) / 240);
    }
}

#include "moc_animatedwidget.cpp"
