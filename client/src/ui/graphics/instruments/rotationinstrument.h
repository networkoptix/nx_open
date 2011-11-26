#ifndef QN_ROTATION_INSTRUMENT_H
#define QN_ROTATION_INSTRUMENT_H

#include "dragprocessinginstrument.h"
#include <QWeakPointer>

class QnResourceWidget;

class RotationItem;

class RotationInstrument: public DragProcessingInstrument {
public:
    RotationInstrument(QObject *parent = NULL);
    virtual ~RotationInstrument();

    qreal rotationItemZValue() const {
        return m_rotationItemZValue;
    }

    void setRotationItemZValue(qreal rotationItemZValue);


protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;

    RotationItem *rotationItem() const {
        return m_rotationItem.data();
    }

private:
    QWeakPointer<RotationItem> m_rotationItem;
    QWeakPointer<QnResourceWidget> m_target;
    QPointF m_origin;
    qreal m_lastAngle;
    qreal m_rotationItemZValue;
};

#endif // QN_ROTATION_INSTRUMENT_H
