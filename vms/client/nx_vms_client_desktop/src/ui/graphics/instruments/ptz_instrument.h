// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBasicTimer>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtGui/QVector3D>

#include <client/client_globals.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>

#include "drag_processing_instrument.h"

class FixedArSelectionItem;
class PtzOverlayWidget;
class PtzElementsWidget;
class PtzManipulatorWidget;

class QnSplashItem;
class QnMediaResourceWidget;
class QnResourceWidget;

namespace nx::vms::client::desktop {
class PtzPromoOverlay;
class SceneBanner;
} // namespace nx::vms::client::desktop

class PtzInstrument:
    public DragProcessingInstrument,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT
    using base_type = DragProcessingInstrument;

public:
    PtzInstrument(
        nx::vms::client::desktop::WindowContext* windowContext,
        QObject* parent = nullptr);

    virtual ~PtzInstrument() override;

    // This instrument operates PTZ via mouse interaction with PTZ overlay.

    // Also, continuous PTZ can be controlled externally using the following function.

    enum class DirectionFlag
    {
        panLeft = 0x01,
        panRight = 0x02,
        tiltUp = 0x04,
        tiltDown = 0x08,
        zoomIn = 0x10,
        zoomOut = 0x20
    };
    Q_FLAG(DirectionFlag) //< For lexical debug output.
    Q_DECLARE_FLAGS(DirectionFlags, DirectionFlag)

    bool supportsContinuousPtz(QnMediaResourceWidget* widget, DirectionFlag direction) const;
    void toggleContinuousPtz(QnMediaResourceWidget* widget, DirectionFlag direction, bool on);

signals:
    void ptzProcessStarted(QnMediaResourceWidget* widget);
    void ptzStarted(QnMediaResourceWidget* widget);
    void ptzFinished(QnMediaResourceWidget* widget);
    void ptzProcessFinished(QnMediaResourceWidget* widget);

    void doubleClicked(QnMediaResourceWidget* widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeDisabledNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual bool registeredNotify(QGraphicsItem* item) override;
    virtual void unregisteredNotify(QGraphicsItem* item) override;

    virtual void timerEvent(QTimerEvent* event) override;

    virtual bool animationEvent(AnimationEvent* event) override;

    virtual bool mousePressEvent(QWidget* viewport, QMouseEvent* event) override;
    virtual bool mousePressEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event) override;

    virtual void startDragProcess(DragInfo* info) override;
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDrag(DragInfo* info) override;
    virtual void finishDragProcess(DragInfo* info) override;

    virtual bool wheelEvent(QGraphicsScene* scene, QGraphicsSceneWheelEvent* event) override;

private slots:
    void at_splashItem_destroyed();

    void at_modeButton_clicked();

    void at_zoomInButton_pressed();
    void at_zoomInButton_released();
    void at_zoomOutButton_pressed();
    void at_zoomOutButton_released();
    void at_zoomButton_activated(qreal speed);

    void at_focusInButton_pressed();
    void at_focusInButton_released();
    void at_focusOutButton_pressed();
    void at_focusOutButton_released();
    void at_focusButton_activated(qreal speed);

    void at_focusAutoButton_clicked();

    void at_display_widgetAboutToBeChanged(Qn::ItemRole role);

    void resetIfTargetIsInvisible();

    void updateOverlayWidget();
    void updateWidgetPtzController(QnMediaResourceWidget* widget);
    void updateOverlayWidgetInternal(QnMediaResourceWidget* widget);
    void updateOverlayCursor(QnMediaResourceWidget* widget);
    void updateCapabilities(QnMediaResourceWidget* widget);
    void updateTraits(QnMediaResourceWidget* widget);

private:
    QnMediaResourceWidget* target() const;
    void setTarget(QnMediaResourceWidget* target);
    PtzManipulatorWidget* targetManipulator() const;

    QnSplashItem* newSplashItem(QGraphicsItem* parentItem);

    FixedArSelectionItem* selectionItem() const;
    void ensureSelectionItem();

    PtzElementsWidget* elementsWidget() const;
    void ensureElementsWidget();

    PtzOverlayWidget* overlayWidget(QnMediaResourceWidget* widget) const;
    PtzOverlayWidget* ensureOverlayWidget(QnMediaResourceWidget* widget);

    /**
     * Check if current camera is a static sensor of a ptz-redirected camera.
     */
    void handlePtzRedirect(QnMediaResourceWidget* widget);
    bool ensurePtzRedirectedCameraIsOnLayout(
        QnMediaResourceWidget* staticSensor,
        const QnVirtualCameraResourcePtr& dynamicSensor);

    bool processMousePress(QGraphicsItem* item, QGraphicsSceneMouseEvent* event);

    void ptzMoveTo(QnMediaResourceWidget* widget, const QPointF& pos);
    void ptzMoveTo(QnMediaResourceWidget* widget, const QRectF& rect, bool unzooming = false);
    void ptzUnzoom(QnMediaResourceWidget* widget);
    void ptzMove(
        QnMediaResourceWidget* widget,
        const nx::vms::common::ptz::Vector& speed,
        bool instant = false);

    void focusMove(QnMediaResourceWidget* widget, qreal speed);
    void focusAuto(QnMediaResourceWidget* widget);

    void processPtzClick(const QPointF& pos);
    void processPtzDrag(const QRectF& rect);
    void processPtzDoubleClick();

    QnResourceWidget* findPtzWidget(const QPointF& scenePos) const;
    void updateExternalPtzSpeed();

    QString actionText(QnMediaResourceWidget* widget) const;
    void updateActionText(QnMediaResourceWidget* widget);

    bool checkPlayingLive(QnMediaResourceWidget* widget) const;
    void updatePromo(QnMediaResourceWidget* widget);

private:
    struct PtzData
    {
        bool hasCapabilities(Ptz::Capabilities value) const;
        bool isFisheye() const;
        bool hasContinuousPanOrTilt() const;
        bool hasAdvancedPtz() const;

        Ptz::Capabilities capabilities = Ptz::NoPtzCapabilities;
        QnPtzAuxiliaryTraitList traits;
        nx::vms::common::ptz::Vector currentSpeed;
        nx::vms::common::ptz::Vector requestedSpeed;
        PtzOverlayWidget* overlayWidget = nullptr;
        QGraphicsWidget* cursorOverlay = nullptr;
        QPointer<nx::vms::client::desktop::PtzPromoOverlay> promoOverlay;
        QMetaObject::Connection capabilitiesConnection;
    };

    struct SplashItemAnimation
    {
        SplashItemAnimation()
        {}
        SplashItemAnimation(QnSplashItem* item):
            item(item)
        {}
        SplashItemAnimation(QnSplashItem* item, qreal expansionMultiplier, qreal opacityMultiplier):
            item(item),
            expansionMultiplier(expansionMultiplier),
            opacityMultiplier(opacityMultiplier)
        {}

        QnSplashItem* item = nullptr;
        bool fadingIn = true;
        qreal expansionMultiplier = 0.0; //< Expansion speed relative to standard expansion speed.
        qreal opacityMultiplier = 0.0;

        friend bool operator==(const SplashItemAnimation& l, const SplashItemAnimation& r)
        {
            return l.item == r.item;
        }
    };

    enum Movement
    {
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
    QHash<QObject*, PtzData> m_dataByWidget;
    QBasicTimer m_movementTimer;

    Movement m_movement;
    Qt::Orientations m_movementOrientations;

    bool m_isClick = false;
    bool m_isDoubleClick = false;
    bool m_ptzStartedEmitted = false;
    bool m_skipNextAction = false;

    QBasicTimer m_clickTimer;
    QPointF m_clickPos;

    class MovementFilter;
    QScopedPointer<MovementFilter> m_movementFilter;

    QList<SplashItemAnimation> m_freeAnimations, m_activeAnimations;

    DirectionFlags m_externalPtzDirections;

    int m_wheelZoomDirection = 0;
    QTimer* const m_wheelZoomTimer;
    QHash<QnMediaResourceWidget*, QTimer*> m_hideActionTextTimers;

    QPointer<nx::vms::client::desktop::SceneBanner> m_noAdvancedPtzWarning;
    mutable QPointer<nx::vms::client::desktop::SceneBanner> m_ptzInArchiveMessageBox;
};
