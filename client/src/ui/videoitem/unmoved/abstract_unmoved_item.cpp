#include "abstract_unmoved_item.h"

#include "ui/animation/property_animation.h"

CLAbstractUnmovedItem::CLAbstractUnmovedItem(QString name, QGraphicsItem* parent) :
    CLAbstractSubItemContainer(parent),
    m_name(name),
    m_view(0),
    m_underMouse(false),
    m_animation(0)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    //setFlag(QGraphicsItem::ItemIsMovable);

    setAcceptHoverEvents(true);
}

CLAbstractUnmovedItem::~CLAbstractUnmovedItem()
{
    stopAnimation();
}

QString CLAbstractUnmovedItem::getName() const
{
    return m_name;
}

QPoint CLAbstractUnmovedItem::staticPos() const
{
    return m_pos;
}

void CLAbstractUnmovedItem::setStaticPos(const QPoint &staticPos)
{
    m_pos = staticPos;
}

void CLAbstractUnmovedItem::adjust()
{
    if (!scene())
        return;

    if (!m_view)
        m_view = scene()->views().at(0);

    setPos(m_view->mapToScene(m_pos));
}

void CLAbstractUnmovedItem::hideIfNeeded(int duration)
{
    hide(duration);
}

void CLAbstractUnmovedItem::setVisible(bool visible, int duration)
{
    Q_UNUSED(duration)
    QGraphicsItem::setVisible(visible);
}

void CLAbstractUnmovedItem::changeOpacity(qreal new_opacity, int duration)
{
    stopAnimation();

    if (duration == 0 || qFuzzyCompare(new_opacity, opacity()))
    {
        setOpacity(new_opacity);
    }
    else
    {
        m_animation = AnimationManager::instance().addAnimation(this, "opacity");
        m_animation->setDuration(duration);
        m_animation->setStartValue(opacity());
        m_animation->setEndValue(new_opacity);
        m_animation->start();

        connect(m_animation, SIGNAL(finished()), this, SLOT(stopAnimation()));
    }
}

void CLAbstractUnmovedItem::stopAnimation()
{
    if (m_animation)
    {
        m_animation->stop();
        m_animation->deleteLater();
        m_animation = 0;
    }
}
