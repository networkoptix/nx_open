#ifndef QN_SCENE_EVENT_FILTER_H
#define QN_SCENE_EVENT_FILTER_H

#include <QtWidgets/QGraphicsItem>

class SceneEventFilterItem;

/**
 * Interface for detached filtering of graphics items' events. 
 * 
 * It can be used in conjunction with <tt>SceneEventFilterItem</tt>.
 */
class SceneEventFilter {
public:
    /**
     * This function performs scene event filtering.
     * 
     * \param watched                  Graphics item that is being watched.
     * \param event                    Filtered event.
     * \returns                        Whether the event was filtered.
     */
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) = 0;
};

/**
 * Interface for receiving destruction notifications from <tt>SceneEventFilterItem</tt>.
 */
class SceneDestructionListener {
public:
    /**
     * This functions is called when scene event filter item is destroyed. 
     * This usually means that the scene is being destroyed.
     * 
     * \param item                     Scene event filter item that is being destroyed.
     */
    virtual void destroyed(SceneEventFilterItem *item) = 0;
};

/**
 * This is a graphics item that can be added to the scene to filter events of 
 * this scene's items.
 */
class SceneEventFilterItem: public QGraphicsItem {
public:
    SceneEventFilterItem():
        m_eventFilter(NULL),
        m_destructionListener(NULL)
    {
        setFlags(ItemHasNoContents);
        setAcceptedMouseButtons(0);
        setEnabled(false);
        setVisible(false);
    }

    virtual ~SceneEventFilterItem() {
        if (m_destructionListener != NULL)
            m_destructionListener->destroyed(this);
    }

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

    void setEventFilter(SceneEventFilter *filter) {
        m_eventFilter = filter;
    }

    SceneEventFilter *eventFilter() const {
        return m_eventFilter;
    }

    void setDestructionListener(SceneDestructionListener *listener) {
        m_destructionListener = listener;
    }

    SceneDestructionListener *destructionListener() const {
        return m_destructionListener;
    }

protected:
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override {
        if (m_eventFilter != NULL) {
            return m_eventFilter->sceneEventFilter(watched, event);
        } else {
            return false;
        }
    }

private:
    SceneEventFilter *m_eventFilter;
    SceneDestructionListener *m_destructionListener;
};



#endif // QN_SCENE_EVENT_FILTER_H

