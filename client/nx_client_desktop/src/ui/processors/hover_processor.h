#ifndef QN_HOVER_FOCUS_PROCESSOR_H
#define QN_HOVER_FOCUS_PROCESSOR_H

#include <QtWidgets/QGraphicsObject>

#include <ui/common/weak_graphics_item_pointer.h>

/**
 * This processor translates graphics item's hover events into hover signals.
 * 
 * This processor is not as generic as others as there is no real need in
 * generic hover processor.
 */
class HoverFocusProcessor: public QGraphicsObject {
    Q_OBJECT;

    typedef QGraphicsObject base_type;

public:
    HoverFocusProcessor(QGraphicsItem *parent = NULL);

    virtual ~HoverFocusProcessor();

    void addTargetItem(QGraphicsItem *item);

    void removeTargetItem(QGraphicsItem *item);

    QList<QGraphicsItem *> targetItems() const;

    int hoverEnterDelay() const {
        return m_hoverEnterDelay;
    }

    void setHoverEnterDelay(int hoverEnterDelayMSec);

    int hoverLeaveDelay() const {
        return m_hoverLeaveDelay;
    }

    void setHoverLeaveDelay(int hoverLeaveDelayMSec);

    int focusEnterDelay() const {
        return m_hoverEnterDelay;
    }

    void setFocusEnterDelay(int focusEnterDelayMSec);

    int focusLeaveDelay() const {
        return m_hoverLeaveDelay;
    }

    void setFocusLeaveDelay(int focusLeaveDelayMSec);

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

    bool isHovered() const {
        return m_hovered;
    }

    bool isFocused() const {
        return m_focused;
    }

public slots:
    void forceHoverLeave();
    void forceHoverEnter();
    void forceFocusLeave();
    void forceFocusEnter();

signals:
    /**
     * This signal is emitted whenever mouse enters one of the target items.
     */
    void hoverEntered();

    /**
     * This signal is emitted whenever mouse leaves one of the target items. 
     */
    void hoverLeft();

    /**
     * This signal is emitted whenever one of the target items gets input focus.
     */
    void focusEntered();

    /**
     * This signal is emitted whenever one of the target items loses input focus.
     */
    void focusLeft();

    /**
     * This signal is emitted whenever target items get both input focus and
     * mouse hover.
     */
    void hoverFocusEntered();

    /**
     * This signal is emitted whenever target items lose both input focus and
     * mouse hover.
     */
    void hoverFocusLeft();

protected:
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void processHoverEnter();
    void processHoverLeave();
    void processFocusEnter();
    void processFocusLeave();

    void killHoverEnterTimer();
    void killHoverLeaveTimer();
    void killFocusEnterTimer();
    void killFocusLeaveTimer();

private:
    int m_hoverEnterDelay;
    int m_hoverLeaveDelay;
    int m_focusEnterDelay;
    int m_focusLeaveDelay;
    int m_hoverEnterTimerId; // TODO: #Elric use QBasicTimer
    int m_hoverLeaveTimerId;
    int m_focusEnterTimerId;
    int m_focusLeaveTimerId;
    bool m_hovered;
    bool m_focused;
    WeakGraphicsItemPointerList m_items;
};


#endif // QN_HOVER_FOCUS_PROCESSOR_H
