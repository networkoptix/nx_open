#ifndef QN_PTZ_INSTRUMENT_H
#define QN_PTZ_INSTRUMENT_H

#include <QtCore/QWeakPointer>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <api/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include "drag_processing_instrument.h"

class SelectionItem;
class PtzSplashItem;
class PtzSelectionItem;

class QnMediaResourceWidget;
class QnPtzInformation;

class PtzInstrument: public DragProcessingInstrument, public QnWorkbenchContextAware {
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

    virtual bool animationEvent(AnimationEvent *event) override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private slots:
    void at_target_optionsChanged();
    void at_splashItem_destroyed();
    void at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera);
    void at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera);
    void at_ptzGetPos_replyReceived(int status, qreal xPos, qreal yPox, qreal zoomPos, int handle);
    void at_ptzMoveTo_replyReceived(int status, int handle);

    void at_replyReceived(int status, int handle);

private:
    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target);
    
    PtzSplashItem *newSplashItem(QGraphicsItem *parentItem);

    PtzSelectionItem *selectionItem() const {
        return m_selectionItem.data();
    }

    void ensureSelectionItem();

    void ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos);
    void ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect);

private:
    QHash<QString, const QnPtzInformation *> m_infoByModel;

    qreal m_ptzItemZValue;
    qreal m_expansionSpeed;

    QHash<QnVirtualCameraResourcePtr, QVector3D> m_ptzPositionByCamera;
    QHash<int, QnVirtualCameraResourcePtr> m_cameraByHandle;

    QWeakPointer<PtzSelectionItem> m_selectionItem;
    QWeakPointer<QWidget> m_viewport;
    QWeakPointer<QnMediaResourceWidget> m_target;

    bool m_isClick;
    bool m_ptzStartedEmitted;

    QList<PtzSplashItem *> m_freeSplashItems, m_activeSplashItems;

    //QnNetworkResourcePtr m_camera;
    //QnMediaServerConnectionPtr m_connection;
};


#endif // QN_PTZ_INSTRUMENT_H
