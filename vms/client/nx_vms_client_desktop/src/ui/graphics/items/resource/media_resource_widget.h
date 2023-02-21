// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <functional>

#include <QtCore/QByteArray>
#include <QtGui/QStaticText>

#include <api/server_rest_connection_fwd.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <camera/iomodule/iomodule_monitor.h>
#include <client/client_globals.h>
#include <core/ptz/ptz_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/client_resource_fwd.h>
#include <core/resource/motion_window.h>
#include <core/resource/resource_fwd.h>
#include <nx/core/watermark/watermark.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <nx/vms/client/core/common/data/motion_selection.h>
#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/core/software_trigger/software_triggers_watcher.h>
#include <nx/vms/client/desktop/camera/camera_fwd.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/types.h>
#include <nx/vms/common/ptz/datafield.h>
#include <nx/vms/event/event_fwd.h>
#include <ui/common/speed_range.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/help/help_topics.h>
#include <utils/media/sse_helper.h>

#include "resource_widget.h"

struct QnMetaDataV1;
using QnMetaDataV1Ptr = std::shared_ptr<QnMetaDataV1>;

namespace nx::vms::client::desktop {

class ObjectTrackingButtonController;
class CameraButtonController;
class RecordingStatusHelper;
class MediaResourceWidgetPrivate;
class AnalyticsOverlayWidget;
class AreaSelectOverlayWidget;
class RoiFiguresOverlayWidget;
class WatermarkPainter;
class SoftwareTriggerButton;
class EncryptedArchivePasswordDialog;

} // namespace nx::vms::client::desktop

class QnResourceDisplay;
class QnResourceWidgetRenderer;
class QnFisheyeHomePtzController;
class QnIoModuleOverlayWidget;
class QnScrollableItemsWidget;
class QnScrollableTextItemsWidget;
class QnGraphicsStackedWidget;

struct QnHtmlTextItemOptions;

namespace nx::analytics::db { struct Filter; }

namespace nx::vms::client::core { class SoftwareTriggersController; }

/**
 * Widget to show media from a camera or disk file (from QnMediaResource).
 */
class QnMediaResourceWidget: public QnResourceWidget
{
    Q_OBJECT
    typedef QnResourceWidget base_type;
    using MotionSelection = nx::vms::client::core::MotionSelection;

public:
    enum class PtzEnabledBy
    {
        nothing,
        joystick,
        user
    };

public:
    QnMediaResourceWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);
    virtual ~QnMediaResourceWidget();

    /**
     * Disconnect all data changes handlers to avoid unneeded processing while saving layout state.
     */
    void beforeDestroy();

    /**
     * @return Resource associated with this widget.
     */
    const QnMediaResourcePtr& resource() const;

    /**
     * @return Display associated with this widget.
     */
    QnResourceDisplayPtr display() const;

    QnResourceWidgetRenderer* renderer() const;

    /** Returns default camera rotation angle or 0 if does not exist or invalid. */
    int defaultRotation() const;

    /** Returns default camera rotation plus fisheye rotation if needed. */
    int defaultFullRotation() const;

    /**
     * @param itemPos Point in item coordinates to map to grid coordinates.
     * @return Coordinates of the motion cell that the given point belongs to. Note that motion
     *     grid is finite, so even if the passed coordinate lies outside the item boundary, the
     *     returned joint will lie inside it.
     */
    QPoint mapToMotionGrid(const QPointF& itemPos);

    /**
     * @param gridPos Coordinate of the motion grid cell.
     * @return Position in scene coordinates of the top left corner of the grid cell.
     */
    QPointF mapFromMotionGrid(const QPoint& gridPos);

    /**
     * @param gridRect Rectangle in grid coordinates to add to the selected motion region of this
     *     widget.
     */
    void addToMotionSelection(const QRect& gridRect);

    /**
     * Clears this widget's motion selection region.
     */
    void clearMotionSelection(bool sendMotionChanged = true);

    bool isMotionSelectionEmpty() const;

    void setMotionSelection(const MotionSelection& regions);

    /**
     * Current motion selection regions.
     */
    const MotionSelection& motionSelection() const;

    nx::vms::api::ImageCorrectionData imageEnhancement() const;
    void setImageEnhancement(const nx::vms::api::ImageCorrectionData& imageEnhancement);

    /**
     * @return PTZ controller associated with this widget. Never returns null.
     */
    QnPtzControllerPtr ptzController() const;

    bool supportsBasicPtz() const;
    bool canControlPtzFocus() const;
    bool canControlPtzMove() const;
    bool canControlPtzZoom() const;

    nx::vms::api::dewarping::MediaData dewarpingParams() const;
    void setDewarpingParams(const nx::vms::api::dewarping::MediaData& params);

    /** Check if the widget has video. It can be absent in I/O Module, for example. */
    bool hasVideo() const;

    QnScrollableTextItemsWidget* bookmarksContainer();

    void hideTextOverlay(const QnUuid& id);
    void showTextOverlay(
        const QnUuid& id,
        const QString& text,
        const QnHtmlTextItemOptions& options);

    void setZoomWindowCreationModeEnabled(bool enabled);
    void setMotionSearchModeEnabled(bool enabled);
    bool isMotionSearchModeEnabled() const;

    void setPtzMode(bool value);

    PtzEnabledBy ptzActivationReason();
    void setPtzActivationReason(PtzEnabledBy reason);

    QnSpeedRange speedRange() const;
    static const QnSpeedRange& availableSpeedRange();

    bool isScheduleEnabled() const;

    bool isRoiVisible() const;
    void setRoiVisible(bool visible, bool animate);

    bool isAnalyticsSupported() const;

    bool isAnalyticsObjectsVisible() const;
    void setAnalyticsObjectsVisible(bool visible, bool animate = true);
    bool isAnalyticsObjectsVisibleForcefully() const;
    /**
     * The same as setAnalyticsObjectsVisible, but the state cannot be deactivated by
     * setAnalyticsObjectsVisible and it is not saved in the layout.
     * Useful to temporarily enable objects display, e.g. when analytics search is enabled in the
     * application.
     */
    void setAnalyticsObjectsVisibleForcefully(bool visible, bool animate = true);

    bool isAnalyticsModeEnabled() const;

    enum class AreaType
    {
        none,
        motion,
        analytics
    };

    AreaType areaSelectionType() const;
    void setAreaSelectionType(AreaType value);
    void unsetAreaSelectionType(AreaType value);

    bool areaSelectionEnabled() const;
    void setAreaSelectionEnabled(bool value);
    void setAreaSelectionEnabled(AreaType areaType, bool value);
    QRectF analyticsFilterRect() const;
    void setAnalyticsFilterRect(const QRectF& value);

    nx::vms::client::core::AbstractAnalyticsMetadataProviderPtr analyticsMetadataProvider() const;

    void setAnalyticsFilter(const nx::analytics::db::Filter& value);

    bool isPlayingLive() const;

    void setPosition(qint64 timestampMs);
    std::chrono::milliseconds position() const;

    void setSpeed(double value);
    double speed() const;

    QnIOModuleMonitorPtr getIOModuleMonitor();

signals:
    void speedChanged();
    void motionSelectionChanged();
    void areaSelectionTypeChanged();
    void areaSelectionEnabledChanged();
    void analyticsFilterRectChanged();
    void areaSelectionFinished();
    void displayChanged();
    void fisheyeChanged();
    void dewarpingParamsChanged();
    void colorsChanged();
    void positionChanged(qint64 positionUtcMs);
    void motionSearchModeEnabled(bool enabled);
    void zoomWindowCreationModeEnabled(bool enabled);
    void licenseStatusChanged();
    void ptzControllerChanged();
    void analyticsSupportChanged();

protected:
    virtual int helpTopicAt(const QPointF& pos) const override;

    virtual void channelLayoutChangedNotify() override;
    virtual void channelScreenSizeChangedNotify() override;
    virtual void optionsChangedNotify(Options changedFlags) override;

    virtual QString calculateDetailsText() const override;
    virtual QString calculatePositionText() const override;
    virtual QString calculateTitleText() const override;
    virtual int calculateButtonsVisibility() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual Qn::ResourceOverlayButton calculateOverlayButton(
        Qn::ResourceStatusOverlay statusOverlay) const override;

    virtual QString overlayCustomButtonText(
        Qn::ResourceStatusOverlay statusOverlay) const override;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    virtual Qn::RenderStatus paintChannelBackground(
        QPainter* painter,
        int channel,
        const QRectF& channelRect,
        const QRectF& paintRect) override;

    virtual void paintChannelForeground(
        QPainter* painter,
        int channel,
        const QRectF& rect) override;

    virtual void paintEffects(QPainter* painter) override;

    void paintMotionGrid(
        QPainter* painter,
        int channel,
        const QRectF& rect,
        const QnMetaDataV1Ptr& motion);

    void paintWatermark(QPainter* painter, const QRectF& rect);

    void paintFilledRegionPath(
        QPainter* painter,
        const QRectF& rect,
        const QPainterPath& path,
        const QColor& color,
        const QColor& penColor);

    void paintProgress(QPainter* painter, const QRectF& rect, int progress);

    void paintAnalyticsObjectsDebugOverlay(
        std::chrono::milliseconds timestamp,
        QPainter* painter,
        const QRectF& rect);

    void ensureMotionSelectionCache();
    void invalidateMotionSelectionCache();

    QSize motionGridSize() const;
    QPoint channelGridOffset(int channel) const;

    Q_SIGNAL void updateInfoTextLater();

    void suspendHomePtzController();
    void resumeHomePtzController();

    virtual bool forceShowPosition() const override;
    virtual void updateHud(bool animate) override;

    void updateCameraButtons();
    bool animationAllowed() const;

    rest::Handle invokeTrigger(
        const QString& id,
        std::function<void(bool, rest::Handle)> resultHandler,
        nx::vms::api::EventState toggleState = nx::vms::api::EventState::undefined);

    void setAnalyticsModeEnabled(bool enabled, bool animate);

    virtual void at_itemDataChanged(int role) override;

    QnClientCameraResourcePtr camera() const;

private slots:
    void at_resource_propertyChanged(const QnResourcePtr& resource, const QString& key);
    void at_screenshotButton_clicked();
    void at_fishEyeButton_toggled(bool checked);
    void at_imageEnhancementButton_toggled(bool checked);
    void at_ioModuleButton_toggled(bool checked);
    void at_camDisplay_liveChanged();
    void processSettingsRequest();
    void processDiagnosticsRequest();
    void processEnableLicenseRequest();
    void processMoreLicensesRequest();
    void processEncryptedArchiveUnlockRequst();
    void at_renderWatcher_widgetChanged(QnResourceWidget* widget);
    void at_zoomRectChanged();
    void at_ptzController_changed(nx::vms::common::ptz::DataFields fields);

    void at_item_imageEnhancementChanged();
    void at_videoLayoutChanged();

    void at_triggerRemoved(QnUuid id);

    void at_triggerAdded(
        QnUuid id,
        const QString& iconPath,
        const QString& name,
        bool prolonged,
        bool enabled);

    void at_triggerFieldsChanged(
        QnUuid id,
        nx::vms::client::core::SoftwareTriggersWatcher::TriggerFields fields);

private:
    void handleItemDataChanged(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);
    void handleDewarpingParamsChanged();

    void setDisplay(const QnResourceDisplayPtr& display);
    void createButtons();

    void updatePtzController();

    qreal calculateVideoAspectRatio() const;

    Q_SLOT void updateDisplay();
    Q_SLOT void updateAspectRatio();
    Q_SLOT void updateIconButton();
    Q_SLOT void updateRendererEnabled();
    Q_SLOT void updateFisheye();
    Q_SLOT void updateDewarpingParams();
    Q_SLOT void updateCustomAspectRatio();
    Q_SLOT void updateIoModuleVisibility(bool animate);
    Q_SLOT void updateAnalyticsVisibility(bool animate = false);

    void updateCompositeOverlayMode();

    qint64 getDisplayTimeUsec() const;
    qint64 getUtcCurrentTimeUsec() const;
    qint64 getUtcCurrentTimeMs() const;

    void updateCurrentUtcPosMs();

    void updateZoomWindowDewarping();

    void setupHud();

    void setTextOverlayParameters(
        const QnUuid& id,
        bool visible,
        const QString& text,
        const QnHtmlTextItemOptions& options);

    Qn::RenderStatus paintVideoTexture(
        QPainter* painter,
        int channel,
        const QRectF& sourceSubRect,
        const QRectF& targetRect);

    bool capabilityButtonsAreVisible() const;
    void updateCapabilityButtons() const;
    void updateTwoWayAudioButton() const;
    void updateIntercomButtons();

private:
    struct SoftwareTriggerInfo
    {
        QnUuid ruleId;
        QString name;
        QString icon;
        bool enabled = false;
        bool prolonged = false;
        std::function<void()> clientSideHandler;
    };

    void initRenderer();
    void initDisplay();
    void initSoftwareTriggers();
    void initIoModuleOverlay();
    void initAreaSelectOverlay();
    void initAnalyticsOverlays();
    void initStatusOverlayController();

    void createTrigger(const SoftwareTriggerInfo& info);

    void configureTriggerButton(
        nx::vms::client::desktop::SoftwareTriggerButton* button,
        const SoftwareTriggerInfo& info);

    int triggerIndex(const QnUuid& ruleId) const;

    void removeTrigger(int index);

    void updateTriggerButtonTooltip(
        nx::vms::client::desktop::SoftwareTriggerButton* button,
        const SoftwareTriggerInfo& info);

    void updateWatermark();

    using ButtonHandler = void (QnMediaResourceWidget::*)(bool checked);
    QAction* createActionAndButton(
        const QString& iconName,
        bool checked,
        const QKeySequence& shortcut,
        const QString& toolTip,
        Qn::HelpTopic helpTopic,
        Qn::WidgetButtons buttonId,
        const QString& buttonName,
        ButtonHandler executor);

    void updateSelectedArea();
    void handleSelectedAreaChanged();
    void traceFps() const;

private:
    nx::utils::ImplPtr<nx::vms::client::desktop::MediaResourceWidgetPrivate> d;

    /** Associated renderer. */
    QnResourceWidgetRenderer* m_renderer = nullptr;

    /** Selected region for search-by-motion, in parrots. */
    MotionSelection m_motionSelection;

    /** Painter path cache for the list of selected regions. */
    QVector<QPainterPath> m_motionSelectionPathCache;

    QVector<bool> m_paintedChannels;

    /** Whether motion selection cached paths are valid. */
    mutable bool m_motionSelectionCacheValid = false;

    nx::vms::client::desktop::RecordingStatusHelper* m_recordingStatusHelper;

    QnPtzControllerPtr m_ptzController;
    QnFisheyeHomePtzController* m_homePtzController = nullptr;

    PtzEnabledBy m_ptzActivationReason = PtzEnabledBy::nothing;

    nx::vms::api::dewarping::MediaData m_dewarpingParams;
    nx::vms::api::dewarping::ViewData m_itemDewarpingStoredParams;

    QnIoModuleOverlayWidget* m_ioModuleOverlayWidget = nullptr;
    bool m_ioCouldBeShown = false;

    qint64 m_posUtcMs;

    QnScrollableItemsWidget* m_objectTrackingButtonContainer = nullptr;
    QnScrollableItemsWidget* m_triggersContainer = nullptr;
    QnScrollableTextItemsWidget* m_bookmarksContainer = nullptr;
    QnScrollableTextItemsWidget* m_textOverlayWidget = nullptr;
    QnGraphicsStackedWidget* m_compositeOverlay = nullptr;

    QScopedPointer<nx::vms::client::desktop::WatermarkPainter> m_watermarkPainter;

    nx::vms::client::desktop::RoiFiguresOverlayWidget* m_roiFiguresOverlayWidget = nullptr;
    nx::vms::client::desktop::AnalyticsOverlayWidget* m_analyticsOverlayWidget = nullptr;
    nx::vms::client::desktop::AreaSelectOverlayWidget* m_areaSelectOverlayWidget = nullptr;
    QScopedPointer<nx::vms::client::desktop::EncryptedArchivePasswordDialog>
        m_encryptedArchivePasswordDialog;
    mutable QByteArray m_encryptedArchiveData;

    AreaType m_areaSelectionType{AreaType::none};
    QRectF m_analyticsFilterRect;

    QnUuid m_itemId;

    QList<SoftwareTriggerInfo> m_triggers;
    nx::vms::client::core::SoftwareTriggersWatcher* m_triggerWatcher = nullptr;
    nx::vms::client::core::SoftwareTriggersController* m_triggersController = nullptr;

    nx::vms::client::desktop::CameraButtonController* m_buttonController = nullptr;
    nx::vms::client::desktop::ObjectTrackingButtonController* m_objectTrackingButtonController = nullptr;

    QAction* const m_toggleImageEnhancementAction;
};

Q_DECLARE_METATYPE(QnMediaResourceWidget *)
