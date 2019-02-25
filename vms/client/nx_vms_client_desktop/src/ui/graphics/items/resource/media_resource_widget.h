#pragma once

#include <array>
#include <functional>

#include <QtGui/QStaticText>

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/event/event_fwd.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/resource_fwd.h>

#include <core/resource/motion_window.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <core/ptz/ptz_fwd.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

#include <core/ptz/media_dewarping_params.h>

#include <client/client_globals.h>
#include <client/client_color_types.h>
#include <nx/vms/client/desktop/camera/camera_fwd.h>
#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <ui/common/speed_range.h>
#include <ui/customization/customized.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/help/help_topics.h>
#include <utils/media/sse_helper.h>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "resource_widget.h"

struct QnMetaDataV1;
using QnMetaDataV1Ptr = std::shared_ptr<QnMetaDataV1>;

namespace nx::vms::client::desktop {

class RecordingStatusHelper;
class EntropixImageEnhancer;
class MediaResourceWidgetPrivate;
class AreaHighlightOverlayWidget;
class AreaSelectOverlayWidget;
class WatermarkPainter;
class SoftwareTriggerButton;

} // namespace nx::vms::client::desktop

class QnResourceDisplay;
class QnResourceWidgetRenderer;
class QnFisheyeHomePtzController;
class QnIoModuleOverlayWidget;
class QnScrollableItemsWidget;
class QnScrollableTextItemsWidget;
class QnGraphicsStackedWidget;
class QnTwoWayAudioWidget;

struct QnHtmlTextItemOptions;

class QnMediaResourceWidget: public Customized<QnResourceWidget>
{
    Q_OBJECT
    typedef Customized<QnResourceWidget> base_type;

    Q_PROPERTY(QVector<QColor> motionSensitivityColors READ motionSensitivityColors
        WRITE setMotionSensitivityColors);

public:
    QnMediaResourceWidget(
        QnWorkbenchContext* context,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);
    virtual ~QnMediaResourceWidget();

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
    void clearMotionSelection();

    bool isMotionSelectionEmpty() const;

    void setMotionSelection(const QList<QRegion>& regions);

    /**
     * Current motion selection regions.
     */
    const QList<QRegion>& motionSelection() const;

    bool addToMotionSensitivity(const QRect& gridRect, int sensitivity);

    bool setMotionSensitivityFilled(const QPoint& gridPos, int sensitivity);

    void clearMotionSensitivity();

    const QList<QnMotionRegion>& motionSensitivity() const;

    nx::vms::api::ImageCorrectionData imageEnhancement() const;
    void setImageEnhancement(const nx::vms::api::ImageCorrectionData& imageEnhancement);

    /**
     * @return PTZ controller associated with this widget. Never returns null.
     */
    QnPtzControllerPtr ptzController() const;

    QnMediaDewarpingParams dewarpingParams() const;
    void setDewarpingParams(const QnMediaDewarpingParams& params);

    /** Check if the widget has video. It can be absent in I/O Module, for example. */
    bool hasVideo() const;

    QnScrollableTextItemsWidget* bookmarksContainer();

    void hideTextOverlay(const QnUuid& id);
    void showTextOverlay(
        const QnUuid& id,
        const QString& text,
        const QnHtmlTextItemOptions& options);

    QVector<QColor> motionSensitivityColors() const;
    void setMotionSensitivityColors(const QVector<QColor>& value);

    void setZoomWindowCreationModeEnabled(bool enabled);
    void setMotionSearchModeEnabled(bool enabled);
    bool isMotionSearchModeEnabled() const;

    void setPtzMode(bool value);

    QnSpeedRange speedRange() const;
    static const QnSpeedRange& availableSpeedRange();

    bool isLicenseUsed() const;

    bool isAnalyticsSupported() const;
    bool isAnalyticsEnabled() const;
    void setAnalyticsEnabled(bool analyticsEnabled);

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

signals:
    void motionSelectionChanged();
    void areaSelectionTypeChanged();
    void areaSelectionEnabledChanged();
    void analyticsFilterRectChanged();
    void displayChanged();
    void fisheyeChanged();
    void dewarpingParamsChanged();
    void colorsChanged();
    void positionChanged(qint64 positionUtcMs);
    void motionSearchModeEnabled(bool enabled);
    void zoomWindowCreationModeEnabled(bool enabled);
    void zoomWindowRectangleVisibleChanged();
    void licenseStatusChanged();
    void ptzControllerChanged();

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

    void paintMotionSensitivityIndicators(QPainter* painter, int channel, const QRectF& rect);

    void paintMotionGrid(
        QPainter* painter,
        int channel,
        const QRectF& rect,
        const QnMetaDataV1Ptr& motion);

    void paintMotionSensitivity(QPainter* painter, int channel, const QRectF& rect);
    void paintWatermark(QPainter* painter, const QRectF& rect);

    void paintFilledRegionPath(
        QPainter* painter,
        const QRectF& rect,
        const QPainterPath& path,
        const QColor& color,
        const QColor& penColor);

    void paintProgress(QPainter* painter, const QRectF& rect, int progress);

    void ensureMotionSensitivity() const;
    Q_SLOT void invalidateMotionSensitivity();

    void ensureBinaryMotionMask() const;
    void invalidateBinaryMotionMask() const;

    void ensureMotionSelectionCache();
    void invalidateMotionSelectionCache();

    void ensureMotionLabelPositions() const;
    void invalidateMotionLabelPositions() const;

    QSize motionGridSize() const;
    QPoint channelGridOffset(int channel) const;

    Q_SIGNAL void updateInfoTextLater();

    void suspendHomePtzController();
    void resumeHomePtzController();

    virtual bool forceShowPosition() const override;
    virtual void updateHud(bool animate) override;

    void updateTwoWayAudioWidget();
    bool animationAllowed() const;

    rest::Handle invokeTrigger(
        const QString& id,
        std::function<void(bool, rest::Handle)> resultHandler,
        nx::vms::api::EventState toggleState = nx::vms::api::EventState::undefined);

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
    void at_renderWatcher_widgetChanged(QnResourceWidget* widget);
    void at_zoomRectChanged();
    void at_ptzController_changed(Qn::PtzDataFields fields);

    void at_entropixEnhancementButton_clicked();
    void at_entropixImageLoaded(const QImage& image);

    void at_item_imageEnhancementChanged();
    void at_videoLayoutChanged();

    void at_eventRuleAddedOrUpdated(const nx::vms::event::RulePtr& rule);
    void at_eventRuleRemoved(const QnUuid& ruleId);

    void clearEntropixEnhancedImage();

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
    Q_SLOT void updateAreaHighlightVisibility();

    void updateCompositeOverlayMode();

    qint64 getDisplayTimeUsec() const;
    qint64 getUtcCurrentTimeUsec() const;
    qint64 getUtcCurrentTimeMs() const;

    void updateCurrentUtcPosMs();

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

private:
    struct SoftwareTriggerInfo
    {
        QString triggerId;
        QString name;
        QString icon;
        bool prolonged = false;

        bool operator ==(const SoftwareTriggerInfo& other) const
        {
            return triggerId == other.triggerId
                && name == other.name
                && icon == other.icon
                && prolonged == other.prolonged;
        }
    };

    struct SoftwareTrigger
    {
        QnUuid ruleId;
        SoftwareTriggerInfo info;
        QnUuid overlayItemId;
    };

    void initRenderer();
    void initDisplay();
    void initSoftwareTriggers();
    void initIoModuleOverlay();
    void initAreaSelectOverlay();
    void initAreaHighlightOverlay();
    void initStatusOverlayController();

    inline SoftwareTriggerInfo makeTriggerInfo(const nx::vms::event::RulePtr& rule) const;

    void createTriggerIfRelevant(const nx::vms::event::RulePtr& rule);
    bool isRelevantTriggerRule(const nx::vms::event::RulePtr& rule) const;

    void configureTriggerButton(
        nx::vms::client::desktop::SoftwareTriggerButton* button,
        const SoftwareTriggerInfo& info,
        std::function<void()> clientSideHandler = std::function<void()>());

    void resetTriggers();

    void updateTriggersAvailability();
    void updateTriggerAvailability(const nx::vms::event::RulePtr& rule);

    void updateTriggerButtonTooltip(
        nx::vms::client::desktop::SoftwareTriggerButton* button,
        const SoftwareTriggerInfo& info,
        bool enabledBySchedule);

	void updateWatermark();

    using ButtonHandler = void (QnMediaResourceWidget::*)(bool checked);
    void createActionAndButton(
        const char* iconName,
        bool checked,
        const QKeySequence& shortcut,
        const QString& toolTip,
        Qn::HelpTopic helpTopic,
        Qn::WidgetButtons buttonId,
        const QString& buttonName,
        ButtonHandler executor);

    using TriggerDataList = QList<SoftwareTrigger>;
    TriggerDataList::iterator lowerBoundbyTriggerName(const nx::vms::event::RulePtr& rule);

    void updateSelectedArea();
    void handleSelectedAreaChanged();

private:
    nx::utils::ImplPtr<nx::vms::client::desktop::MediaResourceWidgetPrivate> d;

    /** Associated renderer. */
    QnResourceWidgetRenderer* m_renderer = nullptr;

    /** Selected region for search-by-motion, in parrots. */
    QList<QRegion> m_motionSelection;

    /** Painter path cache for the list of selected regions. */
    QList<QPainterPath> m_motionSelectionPathCache;

    QVector<bool> m_paintedChannels;

    /** Image region where motion is currently present, in parrots. */
    mutable QList<QnMotionRegion> m_motionSensitivity;

    /** Whether the motion sensitivity is valid. */
    mutable bool m_motionSensitivityValid = false;

    /** Binary mask for the current motion region. */
    mutable QList<simd128i *> m_binaryMotionMask;

    /** Whether motion mask binary data is valid. */
    mutable bool m_binaryMotionMaskValid = false;

    /** Whether motion selection cached paths are valid. */
    mutable bool m_motionSelectionCacheValid = false;

    /** Position for text labels for all motion sensitivity regions. */
    /*   m_motionLabelPositions[channel][sensitivity][polygonIndex]  */
    mutable QVector<std::array<QVector<QPoint>, QnMotionRegion::kSensitivityLevelCount>>
    m_motionLabelPositions;

    /** Whether motion label positions data is valid. */
    mutable bool m_motionLabelPositionsValid = false;

    QStaticText m_sensStaticText[QnMotionRegion::kSensitivityLevelCount];

    nx::vms::client::desktop::RecordingStatusHelper* m_recordingStatusHelper;

    QnPtzControllerPtr m_ptzController;
    QnFisheyeHomePtzController* m_homePtzController = nullptr;

    QnMediaDewarpingParams m_dewarpingParams;

    QnIoModuleOverlayWidget* m_ioModuleOverlayWidget = nullptr;
    bool m_ioCouldBeShown = false;

    qint64 m_posUtcMs;

    QVector<QColor> m_motionSensitivityColors;

    QnScrollableItemsWidget* m_triggersContainer = nullptr;
    QnScrollableTextItemsWidget* m_bookmarksContainer = nullptr;
    QnScrollableTextItemsWidget* m_textOverlayWidget = nullptr;
    QnGraphicsStackedWidget* m_compositeOverlay = nullptr;

    QScopedPointer<nx::vms::client::desktop::WatermarkPainter> m_watermarkPainter;

    nx::vms::client::desktop::AreaHighlightOverlayWidget* m_areaHighlightOverlayWidget = nullptr;
    nx::vms::client::desktop::AreaSelectOverlayWidget* m_areaSelectOverlayWidget = nullptr;

    AreaType m_areaSelectionType{AreaType::none};
    QRectF m_analyticsFilterRect;

    TriggerDataList m_triggers;

    QScopedPointer<nx::vms::client::desktop::EntropixImageEnhancer> m_entropixEnhancer;
    QImage m_entropixEnhancedImage;
    int m_entropixProgress = -1;
    QnUuid m_itemId;
};

Q_DECLARE_METATYPE(QnMediaResourceWidget *)
