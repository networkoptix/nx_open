#ifndef QN_HOVER_PROCESSOR_H
#define QN_HOVER_PROCESSOR_H

#include <QGraphicsObject>
#include <ui/common/weak_graphics_item_pointer.h>

/**
 * This processor translates graphics item's hover events into hover signals.
 * It can be used with only one graphics item at a time.
 * 
 * This processor is not as generic as others as there is no real need in
 * generic hover processor.
 */
class HoverProcessor: public QGraphicsObject {
    Q_OBJECT;

    typedef QGraphicsObject base_type;

public:
    HoverProcessor(QGraphicsItem *parent = NULL);

    virtual ~HoverProcessor();

    void setTargetItem(QGraphicsItem *item);

    QGraphicsItem *targetItem() const {
        return m_item.data();
    }

    int hoverEnterDelay() const {
        return m_hoverEnterDelay;
    }

    int hoverLeaveDelay() const {
        return m_hoverLeaveDelay;
    }

    void setHoverEnterDelay(int hoverEnterDelayMSec);

    void setHoverLeaveDelay(int hoverLeaveDelayMSec);

    void forceHoverLeave();

    void forceHoverEnter();

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

signals:
    void hoverEntered(QGraphicsItem *item);
    void hoverLeft(QGraphicsItem *item);

protected:
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void processEnter();
    void processLeave();

    void killHoverEnterTimer();
    void killHoverLeaveTimer();

private:
    int m_hoverEnterDelay;
    int m_hoverLeaveDelay;
    int m_hoverEnterTimerId;
    int m_hoverLeaveTimerId;
    WeakGraphicsItemPointer m_item;
};


#endif // QN_HOVER_PROCESSOR_H
