#ifndef QN_RELATIVE_PTZ_INSTRUMENT_H
#define QN_RELATIVE_PTZ_INSTRUMENT_H

#include <QtCore/QWeakPointer>
#include <QtCore/QBasicTimer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <api/api_fwd.h>

#include "drag_processing_instrument.h"

class QnMediaResourceWidget;

class PtzArrowItem;

class RelativePtzInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;
public:
    RelativePtzInstrument(QObject *parent = NULL);
    virtual ~RelativePtzInstrument();

    qreal ptzItemZValue() const {
        return m_ptzItemZValue;
    }

    void setPtzItemZValue(qreal ptzItemZValue);

signals:
    void ptzProcessStarted(QnMediaResourceWidget *widget);
    void ptzStarted(QnMediaResourceWidget *widget);
    void ptzFinished(QnMediaResourceWidget *widget);
    void ptzProcessFinished(QnMediaResourceWidget *widget);

protected:
    virtual void timerEvent(QTimerEvent *event) override;

    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual bool wheelEvent(QGraphicsScene *scene, QGraphicsSceneWheelEvent *event) override;

    virtual bool hoverEnterEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) override;
    virtual bool hoverMoveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) override;
    virtual bool hoverLeaveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) override;
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private slots:
    void at_replyReceived(int status, int handle);
    void at_target_optionsChanged();

private:
    friend class PtzArrowItem;

    PtzArrowItem *arrowItem() const {
        return m_ptzItem.data();
    }

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target);
    
    bool isTargetUnderMouse() const {
        return m_targetUnderMouse;
    }
    
    void setTargetUnderMouse(bool underMouse);
    
    void updatePtzItemOpacity();

    const QVector3D &serverSpeed() const {
        return m_localSpeed;
    }

    void setServerSpeed(const QVector3D &speed, bool force = false);

private:
    QBasicTimer m_timer;

    QWeakPointer<PtzArrowItem> m_ptzItem;
    qreal m_ptzItemZValue;

    QWeakPointer<QnMediaResourceWidget> m_target;
    bool m_targetUnderMouse;

    QVector3D m_localSpeed, m_remoteSpeed;
    QnNetworkResourcePtr m_camera;
    QnMediaServerConnectionPtr m_connection;
};


#endif // QN_RELATIVE_PTZ_INSTRUMENT_H
