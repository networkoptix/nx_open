#ifndef abstract_unmoved_item_h_1753
#define abstract_unmoved_item_h_1753

#include "../subitem/abstract_sub_item_container.h"

class QGraphicsView;
class QPropertyAnimation;

class CLAbstractUnmovedItem : public CLAbstractSubItemContainer
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity FINAL)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible FINAL)
    Q_PROPERTY(QPointF pos READ pos WRITE setPos FINAL)
    Q_PROPERTY(qreal x READ x WRITE setX FINAL)
    Q_PROPERTY(qreal y READ y WRITE setY FINAL)
    Q_PROPERTY(qreal z READ zValue WRITE setZValue FINAL)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)
    Q_PROPERTY(qreal scale READ scale WRITE setScale)
    Q_PROPERTY(QPointF transformOriginPoint READ transformOriginPoint WRITE setTransformOriginPoint)

public:
    CLAbstractUnmovedItem(QString name = QString(), QGraphicsItem* parent = 0);
    virtual ~CLAbstractUnmovedItem();

    QString getName() const;

    QPoint staticPos() const; // pos in term of point view coordinates
    void setStaticPos(const QPoint &staticPos); // sets pos in term of point view coordinates
    void adjust(); // adjusts position and size of the item on the scene after scene transformation is done

    // isUnderMouse() replacement;
    // qt bug 18797 When setting the flag ItemIgnoresTransformations for an item, it will receive mouse events as if it was transformed by the view.
    inline bool isMouseOver() const { return m_underMouse; }

    virtual void hideIfNeeded(int duration);

    virtual void setVisible(bool visible, int duration = 0);
    inline void hide(int duration = 0) { setVisible(false, duration); }
    inline void show(int duration = 0) { setVisible(true, duration); }

    void changeOpacity(qreal new_opacity, int duration = 0);

public Q_SLOTS:
    void stopAnimation();

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event)
    {
        CLAbstractSubItemContainer::hoverEnterEvent(event);
        m_underMouse = true;
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
    {
        CLAbstractSubItemContainer::hoverLeaveEvent(event);
        m_underMouse = false;
    }

protected:
    const QString m_name;

    QPoint m_pos;
    QGraphicsView *m_view;

    bool m_underMouse;

    QPropertyAnimation *m_animation;
};

#endif //abstract_unmoved_item_h_1753

