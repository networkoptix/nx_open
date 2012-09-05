#ifndef QN_PTZ_INSTRUMENT_H
#define QN_PTZ_INSTRUMENT_H

#include <QtCore/QWeakPointer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <api/api_fwd.h>

#include "drag_processing_instrument.h"

class QnMediaResourceWidget;

class PtzInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;
public:
    PtzInstrument(QObject *parent = NULL);
    virtual ~PtzInstrument();

signals:
    void ptzProcessStarted(QnMediaResourceWidget *widget);
    void ptzStarted(QnMediaResourceWidget *widget);
    void ptzFinished(QnMediaResourceWidget *widget);
    void ptzProcessFinished(QnMediaResourceWidget *widget);

protected:
    virtual bool registeredNotify(QGraphicsItem *item) override;

    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private slots:
    void at_replyReceived(int status, int handle);

private:
    QnMediaResourceWidget *target() const { 
        return m_target.data(); 
    }

private:
    QWeakPointer<QnMediaResourceWidget> m_target;
    QVector3D m_serverSpeed, m_localSpeed;

    QnNetworkResourcePtr m_camera;
    QnVideoServerConnectionPtr m_connection;
};


#endif // QN_PTZ_INSTRUMENT_H
