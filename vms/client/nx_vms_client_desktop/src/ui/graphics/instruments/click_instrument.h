// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "click_info.h"
#include "drag_processing_instrument.h"

/**
 * This instrument listens to click events and emits corresponding signals
 * when the mouse button is released. Note that it uses application's
 * start drag distance to distinguish click from a drag.
 */
class ClickInstrument: public DragProcessingInstrument
{
    Q_OBJECT
    using base_type = DragProcessingInstrument;

public:
    /**
     * @param buttons Mouse buttons to handle.
     * @param clickDelayMSec Delay in milliseconds to wait for the double click before emitting the
     *     click signal. If the double click arrives before the delay times out, click signal won't
     *     be emitted at all.
     * @param watchedType Type of click events that this instrument will watch. Note that only
     *     SCENE and ITEM types are supported.
     * @param parent Parent object for this instrument.
     */
    ClickInstrument(
        Qt::MouseButtons buttons,
        int clickDelayMSec,
        WatchedType watchedType,
        QObject* parent = nullptr);
    virtual ~ClickInstrument();

signals:
    /**
     * This signal is emitted when the mouse is pressed over an item. After that itemClicked,
     * itemDoubleClicked or both may be emitted, depending on the click delay.
     *
     * @param view View where the click originated.
     * @param item Item that was clicked.
     * @param info Additional click information.
     */
    void itemPressed(QGraphicsView* view, QGraphicsItem *item, ClickInfo info);
    void itemClicked(QGraphicsView* view, QGraphicsItem *item, ClickInfo info);
    void itemDoubleClicked(QGraphicsView* view, QGraphicsItem *item, ClickInfo info);

    void scenePressed(QGraphicsView* view, ClickInfo info);
    void sceneClicked(QGraphicsView* view, ClickInfo info);
    void sceneDoubleClicked(QGraphicsView* view, ClickInfo info);

protected:
    virtual void timerEvent(QTimerEvent *event) override;

    virtual void aboutToBeDisabledNotify() override;

    virtual bool mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;

    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual bool registeredNotify(QGraphicsItem *) override { return true; }

    virtual void startDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    void storeClickData(QGraphicsView *view, QGraphicsItem *item, QGraphicsSceneMouseEvent *event);
    void storeClickData(QGraphicsView *view, QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);
    void killClickTimer();
    void restartClickTimer();

    template<class T>
    bool mousePressEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    template<class T>
    bool mouseDoubleClickEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    template<class T>
    bool mouseMoveEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    template<class T>
    bool mouseReleaseEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    void emitSignals(QGraphicsView* view, QGraphicsItem* item, ClickInfo info);
    void emitSignals(QGraphicsView* view, QGraphicsScene* scene, ClickInfo info);

    void emitInitialSignal(QGraphicsView* view, QGraphicsItem* item, ClickInfo info);
    void emitInitialSignal(QGraphicsView* view, QGraphicsScene* scene, ClickInfo info);

private:
    struct ClickData;

    Qt::MouseButtons m_buttons;
    int m_clickDelayMSec;
    int m_clickTimer;
    QScopedPointer<ClickData> m_clickData;
    bool m_isClick;
    bool m_isDoubleClick;
    bool m_nextDoubleClickIsClick;
};
