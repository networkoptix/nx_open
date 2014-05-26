#ifndef QN_PTZ_INSTRUMENT_H
#define QN_PTZ_INSTRUMENT_H

#include <QtCore/QBasicTimer>
#include <QtCore/QVector>
#include <QtGui/QVector3D>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <api/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include "drag_processing_instrument.h"
#include "core/ptz/abstract_ptz_controller.h"

class FixedArSelectionItem;
class PtzOverlayWidget;
class PtzElementsWidget;
class PtzManipulatorWidget;

class QnSplashItem;
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

    void doubleClicked(QnMediaResourceWidget *widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeDisabledNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual void timerEvent(QTimerEvent *event) override;

    virtual bool animationEvent(AnimationEvent *event) override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private slots:
    void at_splashItem_destroyed();

    void at_modeButton_clicked();
    void at_zoomInButton_pressed();
    void at_zoomInButton_released();
    void at_zoomOutButton_pressed();
    void at_zoomOutButton_released();
    void at_zoomButton_activated(qreal speed);

    void updateOverlayWidget();
    void updateOverlayWidgetInternal(QnMediaResourceWidget *widget);
    void updateCapabilities(QnMediaResourceWidget *widget);

private:
    QnMediaResourceWidget *target() const;
    PtzManipulatorWidget *targetManipulator() const;

    QnSplashItem *newSplashItem(QGraphicsItem *parentItem);

    FixedArSelectionItem *selectionItem() const;
    void ensureSelectionItem();

    PtzElementsWidget *elementsWidget() const;
    void ensureElementsWidget();

    PtzOverlayWidget *overlayWidget(QnMediaResourceWidget *widget) const;
    PtzOverlayWidget *ensureOverlayWidget(QnMediaResourceWidget *widget);

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

        bool hasCapabilities(Qn::PtzCapabilities capabilities) const { return (this->capabilities & capabilities) == capabilities; }

        Qn::PtzCapabilities capabilities;
        QVector3D currentSpeed;
        QVector3D requestedSpeed;
        PtzOverlayWidget *overlayWidget;
        QMetaObject::Connection capabilitiesConnection;
    };

    struct SplashItemAnimation {
        SplashItemAnimation(): item(NULL), fadingIn(true), expansionMultiplier(0.0), opacityMultiplier(0.0) {}
        SplashItemAnimation(QnSplashItem *item): item(item), fadingIn(true), expansionMultiplier(0.0), opacityMultiplier(0.0) {}
        SplashItemAnimation(QnSplashItem *item, qreal expansionMultiplier, qreal opacityMultiplier): item(item), fadingIn(true), expansionMultiplier(expansionMultiplier), opacityMultiplier(opacityMultiplier) {}

        QnSplashItem *item;
        bool fadingIn;
        qreal expansionMultiplier; /**< Expansion speed relative to standard expansion speed. */
        qreal opacityMultiplier;

        friend bool operator==(const SplashItemAnimation &l, const SplashItemAnimation &r) { return l.item == r.item; }
    };

    enum Movement {
        NoMovement,
        ContinuousMovement,
        ViewportMovement,
        VirtualMovement
    };

    int m_clickDelayMSec;
    qreal m_expansionSpeed;

    QPointer<FixedArSelectionItem> m_selectionItem;
    QPointer<PtzElementsWidget> m_elementsWidget;
    QPointer<QWidget> m_viewport;
    QPointer<QnMediaResourceWidget> m_target;
    QHash<QObject *, PtzData> m_dataByWidget;
    QBasicTimer m_movementTimer;

    Movement m_movement;
    Qt::Orientations m_movementOrientations;

    bool m_isClick;
    bool m_isDoubleClick;
    bool m_ptzStartedEmitted;
    bool m_skipNextAction;

    QBasicTimer m_clickTimer;
    QPointF m_clickPos;
    bool m_pendingMouseReturn;

    QList<SplashItemAnimation> m_freeAnimations, m_activeAnimations;
};


#endif // QN_PTZ_INSTRUMENT_H
