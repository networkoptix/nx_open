#ifndef abstract_sub_item_h1031
#define abstract_sub_item_h1031

#include <QtCore/QObject>

#include <QtGui/QGraphicsItem>

class QGraphicsView;

class CLAbstractSceneItem;
class CLAbstractSubItem;

enum CLSubItemType {CloseSubItem, ArchiveNavigatorSubItem, RecordingSubItem, PlaySubItem, PauseSubItem, StepForwardSubItem, StepBackwardSubItem, RewindBackwardSubItem, RewindForwardSubItem};

class CLAbstractSubItemContainer : public QObject, public QGraphicsItem
{
    Q_OBJECT

public:
    CLAbstractSubItemContainer(QGraphicsItem* parent);
    ~CLAbstractSubItemContainer();

    QList<CLAbstractSubItem*> subItemList() const;

    void addSubItem(CLAbstractSubItem *item);
    void removeSubItem(CLAbstractSubItem *item);

    bool addSubItem(CLSubItemType type);
    virtual void removeSubItem(CLSubItemType type);
    virtual QPointF getBestSubItemPos(CLSubItemType type);

    void addToEevntTransparetList(QGraphicsItem* item);
    bool isInEventTransparetList(QGraphicsItem* item) const;

signals:
    void onClose(CLAbstractSubItemContainer*);

public slots:
    virtual void onResize();

protected slots:
    virtual void onSubItemPressed(CLAbstractSubItem* subitem);

protected:
    QList<QGraphicsItem*> m_eventtransperent_list;
    QList<CLAbstractSubItem*> m_subItems;
};


class CLAbstractUnmovedItem : public CLAbstractSubItemContainer
{
    Q_OBJECT

public:
    CLAbstractUnmovedItem(QString name = QString(), QGraphicsItem* parent = 0);
    virtual ~CLAbstractUnmovedItem();

    void setStaticPos(const QPoint &p); // sets pos in term of point view coordinates
    void adjust(); // adjusts position and size of the item on the scene after scene transformation is done

    QString getName() const;

    virtual void hideIfNeeded(int duration);

    virtual bool preferNonSteadyMode() const;

    virtual void hide(int duration) = 0;
    virtual void show(int duration) = 0;

protected:
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

protected:
    QString m_name;
    QGraphicsView* m_view;
    QPoint m_pos;

    bool m_underMouse;
};


class CLAbstractUnMovedOpacityItem: public CLAbstractUnmovedItem
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity  READ opacity   WRITE setOpacity)
public:
    CLAbstractUnMovedOpacityItem(QString name, QGraphicsItem* parent);
    ~CLAbstractUnMovedOpacityItem();
protected:
    virtual void changeOpacity(qreal new_opacity, int duration_ms);
};


class CLUnMovedInteractiveOpacityItem: public CLAbstractUnMovedOpacityItem
{
    Q_OBJECT
public:
    CLUnMovedInteractiveOpacityItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity);
    ~CLUnMovedInteractiveOpacityItem();

    virtual void hide(int duration);
    virtual void show(int duration);

protected:
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

protected:
    qreal m_normal_opacity;
    qreal m_active_opacity;
};


class CLAbstractSubItem : public CLUnMovedInteractiveOpacityItem
{
    Q_OBJECT

public:
    CLAbstractSubItem(CLAbstractSubItemContainer* parent, qreal normal_opacity, qreal active_opacity);
    virtual ~CLAbstractSubItem();

    virtual CLSubItemType getType() const;

    virtual void onResize() = 0;

signals:
    void onPressed(CLAbstractSubItem*);

protected:
    void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
    void mousePressEvent ( QGraphicsSceneMouseEvent * event );

protected:
    CLSubItemType mType;
    CLAbstractSubItemContainer* m_parent;
};

#endif //abstract_sub_item_h1031
