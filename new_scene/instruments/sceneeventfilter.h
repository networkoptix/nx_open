#ifndef QN_PROXY_FILTER_ITEM_H
#define QN_PROXY_FILTER_ITEM_H

#include <QGraphicsItem>

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
        mEventFilter(NULL),
        mDestructionListener(NULL)
    {
        setFlags(ItemHasNoContents);
        setAcceptedMouseButtons(0);
        setEnabled(false);
        setVisible(false);
    }

    virtual ~SceneEventFilterItem() {
        if (mDestructionListener != NULL)
            mDestructionListener->destroyed(this);
    }

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

    void setEventFilter(SceneEventFilter *filter) {
        mEventFilter = filter;
    }

    SceneEventFilter *eventFilter() const {
        return mEventFilter;
    }

    void setDestructionListener(SceneDestructionListener *listener) {
        mDestructionListener = listener;
    }

    SceneDestructionListener *destructionListener() const {
        return mDestructionListener;
    }

protected:
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override {
        if (mEventFilter != NULL) {
            return mEventFilter->sceneEventFilter(watched, event);
        } else {
            return false;
        }
    }

private:
    SceneEventFilter *mEventFilter;
    SceneDestructionListener *mDestructionListener;
};



#endif // QN_PROXY_FILTER_ITEM_H

