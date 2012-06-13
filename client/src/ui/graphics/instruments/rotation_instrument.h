#ifndef QN_ROTATION_INSTRUMENT_H
#define QN_ROTATION_INSTRUMENT_H

#include "drag_processing_instrument.h"
#include <QWeakPointer>

class QGraphicsWidget;

class RotationItem;

class RotationInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;
public:
    RotationInstrument(QObject *parent = NULL);
    virtual ~RotationInstrument();

    qreal rotationItemZValue() const {
        return m_rotationItemZValue;
    }

    void setRotationItemZValue(qreal rotationItemZValue);

signals:
    void rotationProcessStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void rotationStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void rotationFinished(QGraphicsView *view, QGraphicsWidget *widget);
    void rotationProcessFinished(QGraphicsView *view, QGraphicsWidget *widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

    RotationItem *rotationItem() const {
        return m_rotationItem.data();
    }

    QGraphicsWidget *target() const {
        return m_target.data();
    }

private:
    QWeakPointer<RotationItem> m_rotationItem;
    QWeakPointer<QGraphicsWidget> m_target;
    bool m_rotationStartedEmitted;
    qreal m_originAngle;
    qreal m_lastRotation;
    qreal m_rotationItemZValue;
};

#endif // QN_ROTATION_INSTRUMENT_H
