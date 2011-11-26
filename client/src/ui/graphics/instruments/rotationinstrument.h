#ifndef QN_ROTATION_INSTRUMENT_H
#define QN_ROTATION_INSTRUMENT_H

#include "dragprocessinginstrument.h"
#include <QWeakPointer>

class QnResourceWidget;

class RotationItem;

class RotationInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    RotationInstrument(QObject *parent = NULL);
    virtual ~RotationInstrument();

    qreal rotationItemZValue() const {
        return m_rotationItemZValue;
    }

    void setRotationItemZValue(qreal rotationItemZValue);

signals:
    void rotationProcessStarted(QGraphicsView *view, QnResourceWidget *widget);
    void rotationStarted(QGraphicsView *view, QnResourceWidget *widget);
    void rotationFinished(QGraphicsView *view, QnResourceWidget *widget);
    void rotationProcessFinished(QGraphicsView *view, QnResourceWidget *widget);

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

    QnResourceWidget *target() const {
        return m_target.data();
    }

private:
    QWeakPointer<RotationItem> m_rotationItem;
    QWeakPointer<QnResourceWidget> m_target;
    bool m_rotationStartedEmitted;
    QPointF m_origin;
    qreal m_lastAngle;
    qreal m_rotationItemZValue;
};

#endif // QN_ROTATION_INSTRUMENT_H
