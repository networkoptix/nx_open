#ifndef QN_CLICK_INSTRUMENT_H
#define QN_CLICK_INSTRUMENT_H

#include <QPoint>
#include "dragprocessinginstrument.h"

/**
 * This instrument listens to click events and emits corresponding signals
 * when the mouse button is released. Note that it uses application's 
 * start drag distance to distinguish click from a drag.
 */
class ClickInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    /**
     * \param button                    Mouse button to handle.
     * \param watchedType               Type of click events that this instrument will watch.
     *                                  Note that only SCENE and ITEM types are supported.
     * \param parent                    Parent object for this instrument.
     */
    ClickInstrument(Qt::MouseButton button, WatchedType watchedType, QObject *parent = NULL);
    virtual ~ClickInstrument();

signals:
    void clicked(QGraphicsView *view, QGraphicsItem *item, const QPoint &screenPos);
    void doubleClicked(QGraphicsView *view, QGraphicsItem *item, const QPoint &screenPos);

    void clicked(QGraphicsView *view, const QPoint &screenPos);
    void doubleClicked(QGraphicsView *view, const QPoint &screenPos);

protected:
    virtual bool mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;

    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual bool registeredNotify(QGraphicsItem *) override { return true; }

    virtual void startDrag() override;
    virtual void finishDragProcess() override;

private:
    template<class T>
    bool mousePressEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    template<class T>
    bool mouseDoubleClickEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    template<class T>
    bool mouseMoveEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    template<class T>
    bool mouseReleaseEventInternal(T *object, QGraphicsSceneMouseEvent *event);

    void emitSignals(QGraphicsView *view, QGraphicsItem *item, QGraphicsSceneMouseEvent *event);
    void emitSignals(QGraphicsView *view, QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

private:
    Qt::MouseButton m_button;
    bool m_isClick;
    bool m_isDoubleClick;
};

#endif // QN_CLICK_INSTRUMENT_H
