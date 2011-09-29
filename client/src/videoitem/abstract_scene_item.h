#ifndef _abstarct_scene_item_h_1641
#define _abstarct_scene_item_h_1641

#include "abstract_sub_item.h"

class CLVideoWindowItem;
class CLAbstractSubItem;

class CLAbstractSceneItem : public CLAbstractSubItemContainer
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)

public:
    enum CLSceneItemType { VIDEO, IMAGE, BUTTON, RECORDER, LAYOUT };

    CLAbstractSceneItem(int max_width, int max_height, const QString &name = QString());
    virtual ~CLAbstractSceneItem();

    CLVideoWindowItem* toVideoItem() const;

    bool isEtitable() const;
    void setEditable(bool editable);

    // returns true of added

    QString getName() const;
    void setName(const QString& name);

    CLSceneItemType getType() const;
    void setType(CLSceneItemType t);

    void setMaxSize(QSize size);
    QSize getMaxSize() const;

    virtual bool isZoomable() const;

    virtual QSize onScreenSize() const;

    virtual QRectF boundingRect() const;

    bool needUpdate() const
    {
        return m_needUpdate;
    }

    void needUpdate(bool val)
    {
        m_needUpdate = val;

        if (m_needUpdate/* && (isFullScreen() || isItemSelected())*/)
        {
            QMetaObject::invokeMethod(this, "onNeedToUpdateItem", Qt::QueuedConnection);
            //emit onNeedToUpdate(this);
        }
    }

    virtual int height() const;
    virtual int width() const;

    virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);
    bool isItemSelected() const;

    virtual void setFullScreen(bool full);
    bool isFullScreen() const;

    void setArranged(bool arr);
    bool isArranged() const;

    //==========grid editing ==================
    //flag is used to draw selection with different color. true if item is selected and can be drop on top of the other in case of mouse release
    void setCanDrop(bool val);
    QPointF getOriginalPos() const;
    void setOriginalPos(const QPointF &pos);
    bool getOriginallyArranged() const;
    void setOriginallyArranged(bool val);

signals:
    void onPressed(CLAbstractSceneItem*);
    void onDoubleClick(CLAbstractSceneItem*);
    void onFullScreen(CLAbstractSceneItem*);
    void onSelected(CLAbstractSceneItem*);
    void onNeedToUpdate(CLAbstractSceneItem*);

protected:
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    virtual void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mouseDoubleClickEvent ( QGraphicsSceneMouseEvent * event );

protected Q_SLOTS:
    void onNeedToUpdateItem()
    {
        needUpdate(false);
        update();
    }

protected:
    bool m_arranged;
    bool m_fullscreen; // could be only if m_selected
    bool m_selected;

    bool m_mouse_over;
    bool m_zoomOnhover;

    int m_z;

    int m_max_width, m_max_height;

    CLSceneItemType m_type;
    QString mName;

    bool m_editable;
    bool m_needUpdate;

    bool m_can_be_droped; // flag is used to draw selection. true if item is selected and can be drop on top of the other in case of mouse release

    QPointF m_originalPos;
    bool m_originallyArranged;
};

#endif //_abstarct_scene_item_h_1641
