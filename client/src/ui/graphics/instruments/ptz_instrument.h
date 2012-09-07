#ifndef QN_PTZ_INSTRUMENT_H
#define QN_PTZ_INSTRUMENT_H

#include <QtCore/QWeakPointer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <api/api_fwd.h>

#include "drag_processing_instrument.h"

class QnMediaResourceWidget;

class PtzItem;

class PtzInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;
public:
    PtzInstrument(QObject *parent = NULL);
    virtual ~PtzInstrument();

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
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;

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
    friend class PtzItem;

    PtzItem *ptzItem() const {
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

private:
    QWeakPointer<PtzItem> m_ptzItem;
    qreal m_ptzItemZValue;

    QWeakPointer<QnMediaResourceWidget> m_target;
    bool m_targetUnderMouse;

    QVector3D m_serverSpeed;
    QnNetworkResourcePtr m_camera;
    QnVideoServerConnectionPtr m_connection;
};


#endif // QN_PTZ_INSTRUMENT_H
