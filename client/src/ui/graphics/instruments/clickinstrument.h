#ifndef QN_CLICK_INSTRUMENT_H
#define QN_CLICK_INSTRUMENT_H

#include <QPoint>
#include "dragprocessinginstrument.h"

class ClickInfoPrivate;

class ClickInfo {
public:
    Qt::MouseButton button() const;
    Qt::MouseButtons buttons() const;
    QPointF scenePos() const;
    QPoint screenPos() const;
    Qt::KeyboardModifiers modifiers() const;

protected:
    friend class ClickInstrument;

    ClickInfo(ClickInfoPrivate *dd): d(dd) {}

private:
    ClickInfoPrivate *d;
};


/**
 * This instrument listens to click events and emits corresponding signals
 * when the mouse button is released. Note that it uses application's 
 * start drag distance to distinguish click from a drag.
 */
class ClickInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    /**
     * \param buttons                   Mouse buttons to handle.
     * \param watchedType               Type of click events that this instrument will watch.
     *                                  Note that only SCENE and ITEM types are supported.
     * \param parent                    Parent object for this instrument.
     */
    ClickInstrument(Qt::MouseButtons buttons, WatchedType watchedType, QObject *parent = NULL);
    virtual ~ClickInstrument();

signals:
    void clicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);
    void doubleClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);

    void clicked(QGraphicsView *view, const ClickInfo &info);
    void doubleClicked(QGraphicsView *view, const ClickInfo &info);

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

    virtual void startDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

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
    Qt::MouseButtons m_buttons;
    bool m_isClick;
    bool m_isDoubleClick;
};

#endif // QN_CLICK_INSTRUMENT_H
