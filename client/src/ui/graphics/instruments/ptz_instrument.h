#ifndef QN_ABSOLUTE_PTZ_INSTRUMENT_H
#define QN_ABSOLUTE_PTZ_INSTRUMENT_H

#include <QtCore/QWeakPointer>
#include <QtCore/QBasicTimer>
#include <QtCore/QVector>
#include <QtGui/QVector3D>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <api/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include "drag_processing_instrument.h"

class PtzSplashItem;
class PtzSelectionItem;
class PtzOverlayWidget;
class PtzManipulatorWidget;

class QnWorkbenchPtzController;
class QnWorkbenchPtzMapperWatcher;
class QnMediaResourceWidget;

class PtzInstrument: public DragProcessingInstrument, public QnWorkbenchContextAware {
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
    virtual void installedNotify() override;
    virtual void aboutToBeDisabledNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual void timerEvent(QTimerEvent *event) override;

    virtual bool animationEvent(AnimationEvent *event) override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private slots:
    void at_display_resourceAdded(const QnResourcePtr &resource);
    void at_display_resourceAboutToBeRemoved(const QnResourcePtr &resource);
    void at_mapperWatcher_mapperChanged(const QnVirtualCameraResourcePtr &resource);
    void at_ptzController_positionChanged(const QnVirtualCameraResourcePtr &camera);

    void at_splashItem_destroyed();

    void at_zoomInButton_pressed();
    void at_zoomInButton_released();
    void at_zoomOutButton_pressed();
    void at_zoomOutButton_released();
    void at_zoomButton_activated(qreal speed);

    void updateOverlayWidget();
    void updateOverlayWidget(QnMediaResourceWidget *widget);
    void updateCapabilities(const QnSecurityCamResourcePtr &resource);
    void updateCapabilities(QnMediaResourceWidget *widget);

private:
    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    PtzManipulatorWidget *manipulator() const {
        return m_manipulator.data();
    }

    PtzSplashItem *newSplashItem(QGraphicsItem *parentItem);

    PtzSelectionItem *selectionItem() const {
        return m_selectionItem.data();
    }

    PtzOverlayWidget *overlayWidget(QnMediaResourceWidget *widget) const;
    PtzOverlayWidget *ensureOverlayWidget(QnMediaResourceWidget *widget);
    void ensureSelectionItem();

    void ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos);
    void ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect);
    void ptzUnzoom(QnMediaResourceWidget *widget);
    void ptzUpdate(QnMediaResourceWidget *widget);
    void ptzMove(QnMediaResourceWidget *widget, const QVector3D &speed, bool instant = false);

    void processPtzClick(const QPointF &pos);
    void processPtzDrag(const QRectF &rect);
    void processPtzDoubleClick();

private:
    struct PtzData {
        PtzData(): capabilities(0), overlayWidget(NULL) {}

        Qn::CameraCapabilities capabilities;
        QVector3D currentSpeed;
        QVector3D requestedSpeed;
        QRectF pendingAbsoluteMove;
        PtzOverlayWidget *overlayWidget;
    };

    QnWorkbenchPtzController *m_ptzController;
    QnWorkbenchPtzMapperWatcher *m_mapperWatcher;

    int m_clickDelayMSec;
    qreal m_expansionSpeed;

    QWeakPointer<PtzSelectionItem> m_selectionItem;
    QWeakPointer<QWidget> m_viewport;
    QWeakPointer<QnMediaResourceWidget> m_target;
    QWeakPointer<PtzManipulatorWidget> m_manipulator;
    QHash<QObject *, PtzData> m_dataByWidget;
    QBasicTimer m_movementTimer;

    bool m_isClick;
    bool m_isDoubleClick;
    bool m_ptzStartedEmitted;
    bool m_skipNextAction;

    QBasicTimer m_clickTimer;
    QPointF m_clickPos;

    struct SplashItemAnimation {
        SplashItemAnimation(): item(NULL), fadingIn(true), expansionMultiplier(0.0), opacityMultiplier(0.0) {}
        SplashItemAnimation(PtzSplashItem *item): item(item), fadingIn(true), expansionMultiplier(0.0), opacityMultiplier(0.0) {}
        SplashItemAnimation(PtzSplashItem *item, qreal expansionMultiplier, qreal opacityMultiplier): item(item), fadingIn(true), expansionMultiplier(expansionMultiplier), opacityMultiplier(opacityMultiplier) {}

        PtzSplashItem *item;
        bool fadingIn;
        qreal expansionMultiplier; /**< Expansion speed relative to standard expansion speed. */
        qreal opacityMultiplier;

        friend bool operator==(const SplashItemAnimation &l, const SplashItemAnimation &r) { return l.item == r.item; }
    };

    QList<SplashItemAnimation> m_freeAnimations, m_activeAnimations;
};


#endif // QN_ABSOLUTE_PTZ_INSTRUMENT_H
