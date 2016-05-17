#include "media_resource_widget.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>

#include <api/app_server_connection.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>
#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>  //TODO: #GDM remove this dependency

#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_runtime_settings.h>

#include <core/resource/media_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resources_changes_manager.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/tour_ptz_controller.h>
#include <core/ptz/fallback_ptz_controller.h>
#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/home_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/fisheye_home_ptz_controller.h>

#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/abstract_archive_stream_reader.h>

#include <ui/actions/action_manager.h>
#include <ui/common/recording_status_helper.h>
#include <ui/fisheye/fisheye_ptz_controller.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/graphics/items/overlays/io_module_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/composite_text_overlay.h>
#include <ui/graphics/items/overlays/buttons_overlay.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workaround/gl_native_painting.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include "ui/workbench/workbench_item.h"

#include <utils/aspect_ratio.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/common/collection.h>
#include <utils/common/string.h>
#include <utils/common/delayed.h>
#include <utils/license_usage_helper.h>
#include <utils/math/color_transformations.h>
#include <api/common_message_processor.h>
#include <business/actions/abstract_business_action.h>
#include <utils/media/sse_helper.h>

namespace
{
    enum { kMicroInMilliSeconds = 1000 };

    const qreal kTwoWayAudioButtonSize = 44.0;

    bool isSpecialDateTimeValueUsec(qint64 dateTimeUsec)
    {
        return ((dateTimeUsec == DATETIME_NOW)
            || (dateTimeUsec == AV_NOPTS_VALUE)
            || (dateTimeUsec == kNoTimeValue));
    }

    bool getPtzObjectName(const QnPtzControllerPtr &controller, const QnPtzObject &object, QString *name) {
        switch(object.type) {
        case Qn::PresetPtzObject: {
            QnPtzPresetList presets;
            if(!controller->getPresets(&presets))
                return false;

            foreach(const QnPtzPreset &preset, presets) {
                if(preset.id == object.id) {
                    *name = preset.name;
                    return true;
                }
            }

            return false;
        }
        case Qn::TourPtzObject: {
            QnPtzTourList tours;
            if(!controller->getTours(&tours))
                return false;

            foreach(const QnPtzTour &tour, tours) {
                if(tour.id == object.id) {
                    *name = tour.name;
                    return true;
                }
            }

            return false;
        }
        default:
            return false;
        }
    }

} // anonymous namespace

QnMediaResourceWidget::QnMediaResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent)
    : QnResourceWidget(context, item, parent)
    , m_resource(base_type::resource().dynamicCast<QnMediaResource>())
    , m_camera(base_type::resource().dynamicCast<QnVirtualCameraResource>())
    , m_display(nullptr)
    , m_renderer(nullptr)
    , m_motionSelection()
    , m_motionSelectionPathCache()
    , m_paintedChannels()
    , m_motionSensitivity()
    , m_motionSensitivityValid(false)
    , m_binaryMotionMask()
    , m_binaryMotionMaskValid(false)
    , m_motionSelectionCacheValid(false)
    , m_sensStaticText()
    , m_ptzController(nullptr)
    , m_homePtzController(nullptr)
    , m_dewarpingParams()
    , m_compositeTextOverlay(new QnCompositeTextOverlay(m_camera, navigator()
        , [this](){ return getUtcCurrentTimeMs(); }, this))
    , m_ioModuleOverlayWidget(nullptr)
    , m_ioCouldBeShown(false)
    , m_ioLicenceStatusHelper() /// Will be created only for I/O modules

    , m_posUtcMs(DATETIME_INVALID)
{
    if(!m_resource)
        qnCritical("Media resource widget was created with a non-media resource.");

    // TODO: #Elric
    // Strictly speaking, this is a hack.
    // We shouldn't be using OpenGL context in class constructor.
    QGraphicsView *view = QnWorkbenchContextAware::display()->view();
    const QGLWidget *viewport = qobject_cast<const QGLWidget *>(view ? view->viewport() : NULL);
    m_renderer = new QnResourceWidgetRenderer(NULL, viewport ? viewport->context() : NULL);
    connect(m_renderer,                 &QnResourceWidgetRenderer::sourceSizeChanged,   this, &QnMediaResourceWidget::updateAspectRatio);
    connect(base_type::resource(),      &QnResource::propertyChanged,                   this, &QnMediaResourceWidget::at_resource_propertyChanged);
    connect(base_type::resource(),      &QnResource::mediaDewarpingParamsChanged,       this, &QnMediaResourceWidget::updateDewarpingParams);
    connect(this,                       &QnResourceWidget::zoomTargetWidgetChanged,     this, &QnMediaResourceWidget::updateDisplay);
    connect(item,                       &QnWorkbenchItem::dewarpingParamsChanged,       this, &QnMediaResourceWidget::updateFisheye);
    connect(item,                       &QnWorkbenchItem::imageEnhancementChanged,      this, &QnMediaResourceWidget::at_item_imageEnhancementChanged);
    connect(this,                       &QnMediaResourceWidget::dewarpingParamsChanged, this, &QnMediaResourceWidget::updateFisheye);
    connect(this,                       &QnResourceWidget::zoomRectChanged,             this, &QnMediaResourceWidget::updateFisheye);
    connect(this,                       &QnMediaResourceWidget::dewarpingParamsChanged, this, &QnMediaResourceWidget::updateButtonsVisibility);
    if (m_camera)
        connect(m_camera,               &QnVirtualCameraResource::motionRegionChanged,  this, &QnMediaResourceWidget::invalidateMotionSensitivity);
    connect(navigator(),        &QnWorkbenchNavigator::bookmarksModeEnabledChanged,     this, &QnMediaResourceWidget::updateCompositeOverlayMode);

    const auto messageProcessor = QnCommonMessageProcessor::instance();
    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived,
            this, [this](const QnAbstractBusinessActionPtr &businessAction)
    {
        if (businessAction->actionType() != QnBusiness::ExecutePtzPresetAction)
            return;
        const auto &actionParams = businessAction->getParams();
        if (actionParams.actionResourceId != m_resource->toResource()->getId())
            return;
        if (m_ptzController)
            m_ptzController->activatePreset(actionParams.presetId, QnAbstractPtzController::MaxPtzSpeed);
    });


    updateDisplay();
    updateDewarpingParams();

    /* Set up static text. */
    for (int i = 0; i < 10; ++i) {
        m_sensStaticText[i].setText(QString::number(i));
        m_sensStaticText[i].setPerformanceHint(QStaticText::AggressiveCaching);
    }

    updateAspectRatio();

    /* Set up PTZ controller. */
    QnPtzControllerPtr fisheyeController;
    fisheyeController.reset(new QnFisheyePtzController(this), &QObject::deleteLater);
    fisheyeController.reset(new QnViewportPtzController(fisheyeController));
    fisheyeController.reset(new QnPresetPtzController(fisheyeController));
    fisheyeController.reset(new QnTourPtzController(fisheyeController));
    fisheyeController.reset(new QnActivityPtzController(QnActivityPtzController::Local, fisheyeController));

    // Small hack because widget's zoomRect is set only in Synchronize method, not instantly --gdm
    if (item && item->zoomRect().isNull()) { // zoom items are not allowed to return home
        m_homePtzController = new QnFisheyeHomePtzController(fisheyeController);
        fisheyeController.reset(m_homePtzController);
    }

    if (m_camera) {
        if (QnPtzControllerPtr serverController = qnPtzPool->controller(m_camera)) {
            serverController.reset(new QnActivityPtzController(QnActivityPtzController::Client, serverController));
            m_ptzController.reset(new QnFallbackPtzController(fisheyeController, serverController));
        } else {
            m_ptzController = fisheyeController;
        }
    }
    else {
        m_ptzController = fisheyeController;
    }

    connect(m_ptzController, &QnAbstractPtzController::changed, this, &QnMediaResourceWidget::at_ptzController_changed);

    /* Set up info updates. */
    connect(this, &QnMediaResourceWidget::updateInfoTextLater, this, &QnMediaResourceWidget::updateInfoText, Qt::QueuedConnection);

    /* TODO: #GDM if (m_camera)? */
    {
        m_compositeTextOverlay->setMaxFillCoeff(QSizeF(0.7, 0.8));
    	addOverlayWidget(m_compositeTextOverlay, detail::OverlayParams(UserVisible, true, true));
        auto updateContentsMargins = [this]()
        {
            auto positionOverlayGeometry = overlayWidgets()->positionOverlay->contentSize();
            auto margins = m_compositeTextOverlay->contentsMargins();
            margins.setBottom(positionOverlayGeometry.height() + 4);
            m_compositeTextOverlay->setContentsMargins(margins);
        };

        connect(overlayWidgets()->positionOverlay, &GraphicsWidget::geometryChanged, this, updateContentsMargins);
        connect(overlayWidgets()->positionOverlay, &QnScrollableOverlayWidget::contentSizeChanged, this, updateContentsMargins);
        /* Let widgets to be displayed before updating margins. */
        executeDelayed(updateContentsMargins);
    }


    /* Set up overlays */
    if (m_camera && m_camera->hasFlags(Qn::io_module))
    {
        m_ioLicenceStatusHelper.reset(new QnSingleCamLicenceStatusHelper(m_camera));
        m_ioModuleOverlayWidget = new QnIoModuleOverlayWidget();
        m_ioModuleOverlayWidget->setCamera(m_camera);
        m_ioModuleOverlayWidget->setAcceptedMouseButtons(0);
        addOverlayWidget(m_ioModuleOverlayWidget
            , detail::OverlayParams(Visible, true, true));

        connect(m_ioLicenceStatusHelper, &QnSingleCamLicenceStatusHelper::licenceStatusChanged, this
            , [this]() { updateIoModuleVisibility(true); });

        updateButtonsVisibility();
        updateIoModuleVisibility(false);
    }

    if (m_camera && m_camera->hasTwoWayAudio())
    {
        auto twoWayAudioItem = new QnTwoWayAudioWidget();
        twoWayAudioItem->setCamera(m_camera);
        twoWayAudioItem->setFixedHeight(kTwoWayAudioButtonSize);

        /* Items are ordered left-to-right and top-to bottom, so we are inserting two-way audio item on top. */
        overlayWidgets()->positionOverlay->insertItem(0, twoWayAudioItem);
    }

    /* Set up buttons. */
    createButtons();

    if(m_camera) {
        QTimer *timer = new QTimer(this);

        connect(timer,              &QTimer::timeout,                                   this,   &QnMediaResourceWidget::updateIconButton);
        connect(context->instance<QnWorkbenchServerTimeWatcher>(), &QnWorkbenchServerTimeWatcher::displayOffsetsChanged, this, &QnMediaResourceWidget::updateIconButton);
        connect(m_camera,           &QnResource::statusChanged,                         this,   &QnMediaResourceWidget::updateIconButton);

        if (m_camera->hasFlags(Qn::io_module))
            connect(m_camera, &QnResource::statusChanged, this, [this](){ updateIoModuleVisibility(true); }); /// updateOverlayButton is called by updateIoModuleVisibility
        else
            connect(m_camera, &QnResource::statusChanged, this, &QnMediaResourceWidget::updateOverlayButton);

        connect(m_camera,           &QnSecurityCamResource::scheduleTasksChanged,       this,   &QnMediaResourceWidget::updateIconButton);
        timer->start(1000 * 60); /* Update icon button every minute. */

        connect(statusOverlayWidget(), &QnStatusOverlayWidget::diagnosticsRequested,    this,   &QnMediaResourceWidget::at_statusOverlayWidget_diagnosticsRequested);
        connect(statusOverlayWidget(), &QnStatusOverlayWidget::ioEnableRequested,       this,   &QnMediaResourceWidget::at_statusOverlayWidget_ioEnableRequested);
        connect(statusOverlayWidget(), &QnStatusOverlayWidget::moreLicensesRequested,   this,   &QnMediaResourceWidget::at_statusOverlayWidget_moreLicensesRequested);
        updateOverlayButton();
    }

    connect(resource()->toResource(), &QnResource::resourceChanged, this, &QnMediaResourceWidget::updateButtonsVisibility); //TODO: #GDM #Common get rid of resourceChanged

    connect(this, &QnResourceWidget::zoomRectChanged, this, &QnMediaResourceWidget::at_zoomRectChanged);
    connect(context->instance<QnWorkbenchRenderWatcher>(), &QnWorkbenchRenderWatcher::widgetChanged, this, &QnMediaResourceWidget::at_renderWatcher_widgetChanged);

    at_camDisplay_liveChanged();
    at_ptzButton_toggled(false);
    at_histogramButton_toggled(item->imageEnhancement().enabled);
    updateButtonsVisibility();
    updateIconButton();

    updateTitleText();
    updateInfoText();
    updateDetailsText();
    updatePositionText();
    updateCompositeOverlayMode();
    updateCursor();
    updateFisheye();
    setImageEnhancement(item->imageEnhancement());

    connect(this, &QnMediaResourceWidget::updateInfoTextLater
        , this, &QnMediaResourceWidget::updateCurrentUtcPosMs);
}

QnMediaResourceWidget::~QnMediaResourceWidget() {
    ensureAboutToBeDestroyedEmitted();

    if (m_display)
        m_display->removeRenderer(m_renderer);

    m_renderer->destroyAsync();

    for (auto* data : m_binaryMotionMask)
        qFreeAligned(data);
    m_binaryMotionMask.clear();
}

void QnMediaResourceWidget::createButtons() {
    {
        QnImageButtonWidget *screenshotButton = new QnImageButtonWidget(lit("media_widget_screenshot"));
        screenshotButton->setIcon(qnSkin->icon("item/screenshot.png"));
        screenshotButton->setCheckable(false);
        screenshotButton->setProperty(Qn::NoBlockMotionSelection, true);
        screenshotButton->setToolTip(tr("Screenshot"));
        setHelpTopic(screenshotButton, Qn::MainWindow_MediaItem_Screenshot_Help);
        connect(screenshotButton, &QnImageButtonWidget::clicked, this, &QnMediaResourceWidget::at_screenshotButton_clicked);
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::ScreenshotButton, screenshotButton);
    }

    {
        QnImageButtonWidget *searchButton = new QnImageButtonWidget(lit("media_widget_msearch"));
        searchButton->setIcon(qnSkin->icon("item/search.png"));
        searchButton->setCheckable(true);
        searchButton->setProperty(Qn::NoBlockMotionSelection, true);
        searchButton->setToolTip(tr("Smart Search"));
        setHelpTopic(searchButton, Qn::MainWindow_MediaItem_SmartSearch_Help);
        connect(searchButton, &QnImageButtonWidget::toggled, this, &QnMediaResourceWidget::at_searchButton_toggled);
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::MotionSearchButton, searchButton);
    }

    {
        QnImageButtonWidget *ptzButton = new QnImageButtonWidget(lit("media_widget_ptz"));
        ptzButton->setIcon(qnSkin->icon("item/ptz.png"));
        ptzButton->setCheckable(true);
        ptzButton->setProperty(Qn::NoBlockMotionSelection, true);
        ptzButton->setToolTip(tr("PTZ"));
        setHelpTopic(ptzButton, Qn::MainWindow_MediaItem_Ptz_Help);
        connect(ptzButton, &QnImageButtonWidget::toggled, this, &QnMediaResourceWidget::at_ptzButton_toggled);
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::PtzButton, ptzButton);
    }

    {
        QnImageButtonWidget *fishEyeButton = new QnImageButtonWidget(lit("media_widget_fisheye"));
        fishEyeButton->setIcon(qnSkin->icon("item/fisheye.png"));
        fishEyeButton->setCheckable(true);
        fishEyeButton->setProperty(Qn::NoBlockMotionSelection, true);
        fishEyeButton->setToolTip(tr("Dewarping"));
        fishEyeButton->setChecked(item()->dewarpingParams().enabled);
        setHelpTopic(fishEyeButton, Qn::MainWindow_MediaItem_Dewarping_Help);
        connect(fishEyeButton, &QnImageButtonWidget::toggled, this, &QnMediaResourceWidget::at_fishEyeButton_toggled);
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::FishEyeButton, fishEyeButton);
    }

    {
        QnImageButtonWidget *zoomWindowButton = new QnImageButtonWidget(lit("media_widget_zoom"));
        zoomWindowButton->setIcon(qnSkin->icon("item/zoom_window.png"));
        zoomWindowButton->setCheckable(true);
        zoomWindowButton->setProperty(Qn::NoBlockMotionSelection, true);
        zoomWindowButton->setToolTip(tr("Create Zoom Window"));
        setHelpTopic(zoomWindowButton, Qn::MainWindow_MediaItem_ZoomWindows_Help);
        connect(zoomWindowButton, &QnImageButtonWidget::toggled, this, &QnMediaResourceWidget::at_zoomWindowButton_toggled);
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::ZoomWindowButton, zoomWindowButton);
    }

    {
        QnImageButtonWidget *enhancementButton = new QnImageButtonWidget(lit("media_widget_enchancement"));
        enhancementButton->setIcon(qnSkin->icon("item/image_enhancement.png"));
        enhancementButton->setCheckable(true);
        enhancementButton->setProperty(Qn::NoBlockMotionSelection, true);
        enhancementButton->setToolTip(tr("Image Enhancement"));
        enhancementButton->setChecked(item()->imageEnhancement().enabled);
        setHelpTopic(enhancementButton, Qn::MainWindow_MediaItem_ImageEnhancement_Help);
        connect(enhancementButton, &QnImageButtonWidget::toggled, this, &QnMediaResourceWidget::at_histogramButton_toggled);
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::EnhancementButton, enhancementButton);
    }

    {
        QnImageButtonWidget *ioModuleButton = new QnImageButtonWidget(lit("media_widget_io_module"));
        ioModuleButton->setIcon(qnSkin->icon("item/io.png"));
        ioModuleButton->setCheckable(true);
        ioModuleButton->setChecked(false);
        ioModuleButton->setProperty(Qn::NoBlockMotionSelection, true);
        ioModuleButton->setToolTip(tr("I/O Module"));
        connect(ioModuleButton, &QnImageButtonWidget::toggled, this, &QnMediaResourceWidget::at_ioModuleButton_toggled);
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::IoModuleButton, ioModuleButton);
    }

    if (qnRuntime->isDevMode()) {
        QnImageButtonWidget *debugScreenshotButton = new QnImageButtonWidget(lit("media_widget_debug_screenshot"));
        debugScreenshotButton->setIcon(qnSkin->icon("item/screenshot.png"));
        debugScreenshotButton->setCheckable(false);
        debugScreenshotButton->setProperty(Qn::NoBlockMotionSelection, true);
        debugScreenshotButton->setToolTip(lit("Debug set of screenshots"));
        connect(debugScreenshotButton, &QnImageButtonWidget::clicked, this, [this] {
            menu()->trigger(QnActions::TakeScreenshotAction, QnActionParameters(this).withArgument<QString>(Qn::FileNameRole, lit("_DEBUG_SCREENSHOT_KEY_")));
        });
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::DbgScreenshotButton, debugScreenshotButton);
    }

}

const QnMediaResourcePtr &QnMediaResourceWidget::resource() const {
    return m_resource;
}

bool QnMediaResourceWidget::hasVideo() const
{
    return m_resource
        && m_resource->hasVideo(m_display ? m_display->mediaProvider() : nullptr);
}


QPoint QnMediaResourceWidget::mapToMotionGrid(const QPointF &itemPos) {
    QPointF gridPosF(cwiseDiv(itemPos, cwiseDiv(size(), motionGridSize())));
    QPoint gridPos(qFuzzyFloor(gridPosF.x()), qFuzzyFloor(gridPosF.y()));

    return bounded(gridPos, QRect(QPoint(0, 0), motionGridSize()));
}

QPointF QnMediaResourceWidget::mapFromMotionGrid(const QPoint &gridPos) {
    return cwiseMul(gridPos, cwiseDiv(size(), motionGridSize()));
}

QSize QnMediaResourceWidget::motionGridSize() const {
    return cwiseMul(channelLayout()->size(), QSize(MD_WIDTH, MD_HEIGHT));
}

QPoint QnMediaResourceWidget::channelGridOffset(int channel) const {
    return cwiseMul(channelLayout()->position(channel), QSize(MD_WIDTH, MD_HEIGHT));
}

void QnMediaResourceWidget::suspendHomePtzController() {
    if (m_homePtzController)
        m_homePtzController->suspend();
}

void QnMediaResourceWidget::updateHud(bool animate)
{
    const auto compositeOverlayCouldBeVisible
        = !options().testFlag(QnResourceWidget::InfoOverlaysForbidden);

    setOverlayWidgetVisible(m_compositeTextOverlay, compositeOverlayCouldBeVisible, animate);

    QnResourceWidget::updateHud(animate);
}

void QnMediaResourceWidget::resumeHomePtzController() {
    if (m_homePtzController && options().testFlag(DisplayDewarped) && display()->camDisplay()->isRealTimeSource())
        m_homePtzController->resume();
}

const QList<QRegion> &QnMediaResourceWidget::motionSelection() const {
    return m_motionSelection;
}

bool QnMediaResourceWidget::isMotionSelectionEmpty() const {
    foreach(const QRegion &region, m_motionSelection)
        if(!region.isEmpty())
            return false;
    return true;
}

void QnMediaResourceWidget::addToMotionSelection(const QRect &gridRect) {
    ensureMotionSensitivity();

    bool changed = false;

    for (int i = 0; i < channelCount(); ++i) {
        QRect rect = gridRect.translated(-channelGridOffset(i)).intersected(QRect(0, 0, MD_WIDTH, MD_HEIGHT));
        if (rect.isEmpty())
            continue;

        QRegion selection;
        selection += rect;
        selection -= m_motionSensitivity[i].getMotionMask();

        if(!selection.isEmpty()) {
            if(changed) {
                /* In this case we don't need to bother comparing old & new selection regions. */
                m_motionSelection[i] += selection;
            } else {
                QRegion oldSelection = m_motionSelection[i];
                m_motionSelection[i] += selection;
                changed |= (oldSelection != m_motionSelection[i]);
            }
        }
    }

    if(changed){
        invalidateMotionSelectionCache();
        emit motionSelectionChanged();
    }
}

void QnMediaResourceWidget::clearMotionSelection() {
    if(isMotionSelectionEmpty())
        return;

    for (int i = 0; i < m_motionSelection.size(); ++i)
        m_motionSelection[i] = QRegion();

    invalidateMotionSelectionCache();
    emit motionSelectionChanged();
}

void QnMediaResourceWidget::setMotionSelection(const QList<QRegion> &regions) {
    if (regions.size() != m_motionSelection.size()) {
        qWarning() << "invalid motion selection list";
        return;
    }

    m_motionSelection = regions;
    invalidateMotionSelectionCache();
    emit motionSelectionChanged();
}

void QnMediaResourceWidget::invalidateMotionSensitivity() {
    m_motionSensitivityValid = false;
}

void QnMediaResourceWidget::ensureMotionSensitivity() const {
    if(m_motionSensitivityValid)
        return;

    if (m_camera) {
        m_motionSensitivity = m_camera->getMotionRegionList();

        if(m_motionSensitivity.size() != channelCount()) {
            qnWarning("Camera '%1' returned a motion sensitivity list of invalid size.", m_camera->getName());
            qnResizeList(m_motionSensitivity, channelCount());
        }
    } else if(m_resource->toResource()->hasFlags(Qn::motion)) {
        for(int i = 0, count = channelCount(); i < count; i++)
            m_motionSensitivity.push_back(QnMotionRegion());
    } else {
        m_motionSensitivity.clear();
    }

    m_motionSensitivityValid = true;
}

bool QnMediaResourceWidget::addToMotionSensitivity(const QRect &gridRect, int sensitivity) {
    ensureMotionSensitivity();

    bool changed = false;
    if (m_camera) {
        QnConstResourceVideoLayoutPtr layout = m_camera->getVideoLayout();

        for (int i = 0; i < layout->channelCount(); ++i) {
            QRect r(0, 0, MD_WIDTH, MD_HEIGHT);
            r.translate(channelGridOffset(i));
            r = gridRect.intersected(r);
            r.translate(-channelGridOffset(i));
            if (!r.isEmpty()) {
                m_motionSensitivity[i].addRect(sensitivity, r);
                changed = true;
            }
        }
    }

    if(sensitivity == 0)
        invalidateBinaryMotionMask();

    return changed;
}

bool QnMediaResourceWidget::setMotionSensitivityFilled(const QPoint &gridPos, int sensitivity) {
    ensureMotionSensitivity();

    int channel =0;
    QPoint channelPos = gridPos;
    if (m_camera) {
        QnConstResourceVideoLayoutPtr layout = m_camera->getVideoLayout();

        for (int i = 0; i < layout->channelCount(); ++i) {
            QRect r(channelGridOffset(i), QSize(MD_WIDTH, MD_HEIGHT));
            if (r.contains(channelPos)) {
                channelPos -= r.topLeft();
                channel = i;
                break;
            }
        }
    }
    return m_motionSensitivity[channel].updateSensitivityAt(channelPos, sensitivity);
}

void QnMediaResourceWidget::clearMotionSensitivity() {
    for(int i = 0; i < channelCount(); i++)
        m_motionSensitivity[i] = QnMotionRegion();
    m_motionSensitivityValid = true;

    invalidateBinaryMotionMask();
}

const QList<QnMotionRegion> &QnMediaResourceWidget::motionSensitivity() const {
    ensureMotionSensitivity();

    return m_motionSensitivity;
}

void QnMediaResourceWidget::ensureBinaryMotionMask() const {
    if(m_binaryMotionMaskValid)
        return;

    ensureMotionSensitivity();
    for (int i = 0; i < channelCount(); ++i)
        QnMetaDataV1::createMask(m_motionSensitivity[i].getMotionMask(), reinterpret_cast<char *>(m_binaryMotionMask[i]));
}

void QnMediaResourceWidget::invalidateBinaryMotionMask() {
    m_binaryMotionMaskValid = false;
}

void QnMediaResourceWidget::ensureMotionSelectionCache() {
    if (m_motionSelectionCacheValid)
        return;

    for (int i = 0; i < channelCount(); ++i){
        QPainterPath path;
        path.addRegion(m_motionSelection[i]);
        m_motionSelectionPathCache[i] = path.simplified();
    }
}

void QnMediaResourceWidget::invalidateMotionSelectionCache() {
    m_motionSelectionCacheValid = false;
}

void QnMediaResourceWidget::setDisplay(const QnResourceDisplayPtr &display) {
    if(display == m_display)
        return;

    if(m_display) {
        m_display->removeRenderer(m_renderer);
        disconnect(m_display->camDisplay(), NULL, this, NULL);
    }

    m_display = display;

    if(m_display) {
        connect(m_display->camDisplay(), SIGNAL(stillImageChanged()), this, SLOT(updateButtonsVisibility()));
        connect(m_display->camDisplay(), SIGNAL(liveMode(bool)), this, SLOT(at_camDisplay_liveChanged()));
        connect(m_resource->toResource(),SIGNAL(videoLayoutChanged(const QnResourcePtr &)), this, SLOT(at_videoLayoutChanged()));

        connect(m_display->camDisplay(), &QnCamDisplay::liveMode, this, [this](bool /* live */)
        {
            if (m_camera && m_camera->hasFlags(Qn::io_module))
                updateIoModuleVisibility(true);
        });

        setChannelLayout(m_display->videoLayout());
        m_display->addRenderer(m_renderer);
        m_renderer->setChannelCount(m_display->videoLayout()->channelCount());
        updateCustomAspectRatio();
    } else {
        setChannelLayout(QnConstResourceVideoLayoutPtr(new QnDefaultResourceVideoLayout()));
        m_renderer->setChannelCount(0);
    }

    setOption(QnResourceWidget::WindowRotationForbidden, !hasVideo());

    emit displayChanged();
}

void QnMediaResourceWidget::at_videoLayoutChanged()
{
    setChannelLayout(m_display->videoLayout());
}

void QnMediaResourceWidget::updateDisplay() {
    QnMediaResourceWidget *zoomTargetWidget = dynamic_cast<QnMediaResourceWidget *>(this->zoomTargetWidget());

    QnResourceDisplayPtr display;
    if (zoomTargetWidget) {
        display = zoomTargetWidget->display();
    } else {
        display = QnResourceDisplayPtr(new QnResourceDisplay(m_resource->toResourcePtr(), this));
    }

    setDisplay(display);
}

void QnMediaResourceWidget::updateIconButton()
{
    auto buttonsBar = buttonsOverlay()->leftButtonsBar();
    if (!zoomRect().isNull())
    {
        auto iconButton = buttonsBar->button(Qn::RecordingStatusIconButton);
        iconButton->setIcon(qnSkin->icon("item/zoom_window_hovered.png"));
        iconButton->setToolTip(tr("Zoom Window"));

        buttonsBar->setButtonsVisible(Qn::RecordingStatusIconButton, true);
        return;
    }

    if(!m_camera)
    {
        buttonsBar->setButtonsVisible(Qn::RecordingStatusIconButton, false);
        return;
    }

    int recordingMode = QnRecordingStatusHelper::currentRecordingMode(m_camera);
    QIcon recIcon = QnRecordingStatusHelper::icon(recordingMode);

    buttonsBar->setButtonsVisible(Qn::RecordingStatusIconButton, !recIcon.isNull());

    auto iconButton = buttonsBar->button(Qn::RecordingStatusIconButton);
    iconButton->setIcon(recIcon);
    iconButton->setToolTip(QnRecordingStatusHelper::tooltip(recordingMode));
}

void QnMediaResourceWidget::updateRendererEnabled() {
    if(m_resource->toResourcePtr()->flags() & Qn::still_image)
        return;

    for(int channel = 0; channel < channelCount(); channel++)
        m_renderer->setEnabled(channel, !exposedRect(channel, true, true, false).isEmpty());
}

ImageCorrectionParams QnMediaResourceWidget::imageEnhancement() const {
    return item()->imageEnhancement();
}

void QnMediaResourceWidget::setImageEnhancement(const ImageCorrectionParams &imageEnhancement) {
    buttonsOverlay()->rightButtonsBar()->button(Qn::EnhancementButton)->setChecked(imageEnhancement.enabled);
    item()->setImageEnhancement(imageEnhancement);
    m_renderer->setImageCorrection(imageEnhancement);
}

// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnMediaResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);

    updateRendererEnabled();

    for(int channel = 0; channel < channelCount(); channel++)
        m_renderer->setDisplayedRect(channel, exposedRect(channel, true, true, true));

    updateInfoTextLater();
}

Qn::RenderStatus QnMediaResourceWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) {
    QnGlNativePainting::begin(m_renderer->glContext(),painter);

    qreal opacity = effectiveOpacity();
    bool opaque = qFuzzyCompare(opacity, 1.0);
    // always use blending for images --gdm
    if(!opaque || (resource()->toResource()->flags() & Qn::still_image)) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    QRectF sourceRect = toSubRect(channelRect, paintRect);
    Qn::RenderStatus result = m_renderer->paint(channel, sourceRect, paintRect, effectiveOpacity());
    m_paintedChannels[channel] = true;

    /* There is no need to restore blending state before invoking endNativePainting. */
    QnGlNativePainting::end(painter);

    if(result != Qn::NewFrameRendered && result != Qn::OldFrameRendered)
        painter->fillRect(paintRect, palette().color(QPalette::Window));

    return result;
}

void QnMediaResourceWidget::paintChannelForeground(QPainter *painter, int channel, const QRectF &rect) {
    if (options() & DisplayMotion) {
        ensureMotionSelectionCache();

        paintMotionGrid(painter, channel, rect, m_renderer->lastFrameMetadata(channel));
        paintMotionSensitivity(painter, channel, rect);

        /* Motion selection. */
        if(!m_motionSelection[channel].isEmpty()) {
            QColor color = toTransparent(qnGlobals->mrsColor(), 0.2);
            paintFilledRegionPath(painter, rect, m_motionSelectionPathCache[channel], color, color);
        }
    }
}

void QnMediaResourceWidget::paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion) {
    // 5-7 fps

    ensureMotionSensitivity();

    qreal xStep = rect.width() / MD_WIDTH;
    qreal yStep = rect.height() / MD_HEIGHT;

    QVector<QPointF> gridLines[2];

    if (motion && motion->channelNumber == (quint32)channel) {
        // 2-3 fps

        ensureBinaryMotionMask();
        motion->removeMotion(m_binaryMotionMask[channel]);

        /* Horizontal lines. */
        for (int y = 1; y < MD_HEIGHT; ++y) {
            bool isMotion = motion->isMotionAt(0, y - 1) || motion->isMotionAt(0, y);
            gridLines[isMotion] << QPointF(0, y * yStep);
            int x = 1;
            while(x < MD_WIDTH){
                while (x < MD_WIDTH && isMotion == (motion->isMotionAt(x, y - 1) || motion->isMotionAt(x, y)) )
                    x++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (x < MD_WIDTH){
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }

        /* Vertical lines. */
        for (int x = 1; x < MD_WIDTH; ++x) {
            bool isMotion = motion->isMotionAt(x - 1, 0) || motion->isMotionAt(x, 0);
            gridLines[isMotion] << QPointF(x * xStep, 0);
            int y = 1;
            while(y < MD_HEIGHT){
                while (y < MD_HEIGHT && isMotion == (motion->isMotionAt(x - 1, y) || motion->isMotionAt(x, y)) )
                    y++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (y < MD_HEIGHT){
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }
    } else {
        for (int x = 1; x < MD_WIDTH; ++x)
            gridLines[0] << QPointF(x * xStep, 0.0) << QPointF(x * xStep, rect.height());
        for (int y = 1; y < MD_HEIGHT; ++y)
            gridLines[0] << QPointF(0.0, y * yStep) << QPointF(rect.width(), y * yStep);
    }


    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(rect.topLeft());

    QnScopedPainterPenRollback penRollback(painter);
    painter->setPen(QPen(QColor(255, 255, 255, 16), 0.0));
    painter->drawLines(gridLines[0]);

    painter->setPen(QPen(QColor(255, 0, 0, 128), 0.0));
    painter->drawLines(gridLines[1]);
}

void QnMediaResourceWidget::paintFilledRegionPath(QPainter *painter, const QRectF &rect, const QPainterPath &path, const QColor &color, const QColor &penColor) {
    // 4-6 fps

    QnScopedPainterTransformRollback transformRollback(painter); Q_UNUSED(transformRollback);
    QnScopedPainterBrushRollback brushRollback(painter, color); Q_UNUSED(brushRollback);
    QnScopedPainterPenRollback penRollback(painter); Q_UNUSED(penRollback);

    painter->translate(rect.topLeft());
    painter->scale(rect.width() / MD_WIDTH, rect.height() / MD_HEIGHT);
    painter->setPen(QPen(penColor, 0.0));
    painter->drawPath(path);
}

void QnMediaResourceWidget::paintMotionSensitivityIndicators(QPainter *painter, int channel, const QRectF &rect, const QnMotionRegion &region) {
    QPoint channelOffset = channelGridOffset(channel);

    qreal xStep = rect.width() / MD_WIDTH;
    qreal yStep = rect.height() / MD_HEIGHT;
    qreal xOffset = channelOffset.x() + 0.1;
    qreal yOffset = channelOffset.y();
    qreal fontIncrement = 1.2;

    painter->setPen(Qt::black);
    QFont font;
    font.setPointSizeF(yStep * fontIncrement);
    font.setBold(true);
    painter->setFont(font);

    /* Zero sensitivity is skipped as there should not be painted zeros */
    for (int sensitivity = QnMotionRegion::MIN_SENSITIVITY + 1; sensitivity <= QnMotionRegion::MAX_SENSITIVITY; ++sensitivity) {
        auto rects = region.getRectsBySens(sensitivity);
        if (rects.isEmpty())
            continue;

        m_sensStaticText[sensitivity].prepare(painter->transform(), font);
        foreach(const QRect &rect, rects) {
            if (rect.width() < 2 || rect.height() < 2)
                continue;

            qreal x = rect.left(), y = rect.top();
            painter->drawStaticText((x + xOffset) * xStep , (y + yOffset) * yStep , m_sensStaticText[sensitivity]);
        }
    }
}

void QnMediaResourceWidget::paintMotionSensitivity(QPainter *painter, int channel, const QRectF &rect) {
    ensureMotionSensitivity();

    if (options() & DisplayMotionSensitivity) {
        for (int i = QnMotionRegion::MIN_SENSITIVITY; i <= QnMotionRegion::MAX_SENSITIVITY; ++i) {
            QColor color = i > 0 ? QColor(100 +  i * 3, 16 * (10 - i), 0, 96 + i * 2) : qnGlobals->motionMaskColor();
            QPainterPath path = m_motionSensitivity[channel].getRegionBySensPath(i);
            paintFilledRegionPath(painter, rect, path, color, Qt::black);
        }

        paintMotionSensitivityIndicators(painter, channel, rect, m_motionSensitivity[channel]);
    } else {
        paintFilledRegionPath(painter, rect, m_motionSensitivity[channel].getMotionMaskPath(), qnGlobals->motionMaskColor(), qnGlobals->motionMaskColor());
    }
}

QnPtzControllerPtr QnMediaResourceWidget::ptzController() const {
    return m_ptzController;
}

QnMediaDewarpingParams QnMediaResourceWidget::dewarpingParams() const {
    return m_dewarpingParams;
}

void QnMediaResourceWidget::setDewarpingParams(const QnMediaDewarpingParams &params) {
    if (m_dewarpingParams == params)
        return;
    m_dewarpingParams = params;

    emit dewarpingParamsChanged();
}

float QnMediaResourceWidget::visualAspectRatio() const {
    if (!resource())
        return base_type::visualAspectRatio();

    qreal customAspectRatio = resource()->customAspectRatio();
    if (qFuzzyIsNull(customAspectRatio))
        return base_type::visualAspectRatio();

    qreal aspectRatio = customAspectRatio;
    if (zoomRect().isNull())
        aspectRatio *= QnGeometry::aspectRatio(channelLayout()->size());

    return QnAspectRatio::isRotated90(rotation()) ? 1 / aspectRatio : aspectRatio;
}

float QnMediaResourceWidget::defaultVisualAspectRatio() const {
    if (!item())
        return base_type::defaultVisualAspectRatio();

    if (item()->layout() && item()->layout()->hasCellAspectRatio())
        return item()->layout()->cellAspectRatio();

    return qnGlobals->defaultLayoutCellAspectRatio();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
int QnMediaResourceWidget::helpTopicAt(const QPointF &) const {

    auto isIoModule = [this]() {
        if (!m_resource->toResource()->flags().testFlag(Qn::io_module))
            return false;

        if (m_camera
            && m_display
            && !m_camera->hasVideo(m_display->mediaProvider()))
                return true;

        return (m_ioModuleOverlayWidget && overlayWidgetVisibility(m_ioModuleOverlayWidget) == OverlayVisibility::Visible);
    };

    if (action(QnActions::ToggleTourModeAction)->isChecked())
        return Qn::MainWindow_Scene_TourInProgress_Help;

    Qn::ResourceStatusOverlay statusOverlay = statusOverlayWidget()->statusOverlay();

    if (statusOverlay == Qn::AnalogWithoutLicenseOverlay) {
        return Qn::MainWindow_MediaItem_AnalogLicense_Help;
    } else if (statusOverlay == Qn::OfflineOverlay) {
        return Qn::MainWindow_MediaItem_Diagnostics_Help;
    } else if(statusOverlay == Qn::UnauthorizedOverlay) {
        return Qn::MainWindow_MediaItem_Unauthorized_Help;
    } else if (statusOverlay == Qn::IoModuleDisabledOverlay) {
        return Qn::IOModules_Help;
    } else if(options() & ControlPtz) {
        if(m_dewarpingParams.enabled) {
            return Qn::MainWindow_MediaItem_Dewarping_Help;
        } else {
            return Qn::MainWindow_MediaItem_Ptz_Help;
        }
        return Qn::MainWindow_MediaItem_Ptz_Help;
    } else if(!zoomRect().isNull()) {
        return Qn::MainWindow_MediaItem_ZoomWindows_Help;
    } else if(options() & DisplayMotionSensitivity) {
        return Qn::CameraSettings_Motion_Help;
    } else if(options() & DisplayMotion) {
        return Qn::MainWindow_MediaItem_SmartSearch_Help;
    } else if (isIoModule()){
        return Qn::IOModules_Help;
    } else if(m_resource->toResource()->flags() & Qn::local) {
        return Qn::MainWindow_MediaItem_Local_Help;
    } else if (m_camera && m_camera->isDtsBased()) {
        return Qn::MainWindow_MediaItem_AnalogCamera_Help;
    }
    else {
        return Qn::MainWindow_MediaItem_Help;
    }
}

void QnMediaResourceWidget::channelLayoutChangedNotify() {
    base_type::channelLayoutChangedNotify();

    qnResizeList(m_motionSelection, channelCount());
    qnResizeList(m_motionSelectionPathCache, channelCount());
    qnResizeList(m_motionSensitivity, channelCount());
    qnResizeList(m_paintedChannels, channelCount());

    while(m_binaryMotionMask.size() > channelCount()) {
        qFreeAligned(m_binaryMotionMask.back());
        m_binaryMotionMask.pop_back();
    }
    while(m_binaryMotionMask.size() < channelCount()) {
        m_binaryMotionMask.push_back(static_cast<simd128i *>(qMallocAligned(MD_WIDTH * MD_HEIGHT / 8, 32)));
        memset(m_binaryMotionMask.back(), 0, MD_WIDTH * MD_HEIGHT / 8);
    }

    updateAspectRatio();
}

void QnMediaResourceWidget::channelScreenSizeChangedNotify() {
    base_type::channelScreenSizeChangedNotify();

    m_renderer->setChannelScreenSize(channelScreenSize());
}

void QnMediaResourceWidget::optionsChangedNotify(Options changedFlags) {
    if(changedFlags & DisplayMotion) {
        if (QnAbstractArchiveStreamReader *reader = m_display->archiveReader())
            reader->setSendMotion(options() & DisplayMotion);

        buttonsOverlay()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton, options() & DisplayMotion);

        if(options() & DisplayMotion) {
            setProperty(Qn::MotionSelectionModifiers, 0);
        } else {
            setProperty(Qn::MotionSelectionModifiers, QVariant()); /* Use defaults. */
        }
    }

    if(changedFlags & (DisplayMotion | DisplayMotionSensitivity | ControlZoomWindow))
        updateCursor();

    base_type::optionsChangedNotify(changedFlags);
}

QString QnMediaResourceWidget::calculateDetailsText() const {
    qreal fps = 0.0;
    qreal mbps = 0.0;

    for(int i = 0; i < channelCount(); i++) {
        const QnMediaStreamStatistics *statistics = m_display->mediaProvider()->getStatistics(i);
        if (statistics->isConnectionLost()) //TODO: #GDM check does not work, case #3993
            continue;
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
        mbps += statistics->getBitrateMbps();
    }

    QSize size = m_display->camDisplay()->getRawDataSize();
    size.setWidth(size.width() * m_display->camDisplay()->channelsCount());

    QString codecString;
    if(QnConstMediaContextPtr codecContext = m_display->mediaProvider()->getCodecContext())
        codecString = codecContext->getCodecName();


    QString hqLqString;
    if (hasVideo() && !m_resource->toResource()->hasFlags(Qn::local))
        hqLqString = (m_renderer->isLowQualityImage(0)) ? tr("Low-Res") : tr("Hi-Res");

    enum {
        kDetailsTextPixelSize = 11
    };

    QString result;
    if (hasVideo()) {
        result.append(htmlFormattedParagraph(lit("%1x%2").arg(size.width()).arg(size.height()), kDetailsTextPixelSize, true));
        result.append(htmlFormattedParagraph(lit("%1fps").arg(fps, 0, 'f', 2), kDetailsTextPixelSize, true));
    }
    result.append(htmlFormattedParagraph(lit("%1Mbps").arg(mbps, 0, 'f', 2), kDetailsTextPixelSize, true));
    result.append(htmlFormattedParagraph(codecString, kDetailsTextPixelSize, true));
    result.append(htmlFormattedParagraph(hqLqString, kDetailsTextPixelSize, true));

    return result;
}

void QnMediaResourceWidget::updateCurrentUtcPosMs()
{
    const qint64 usec = getUtcCurrentTimeUsec();
    qint64 newPos = DATETIME_INVALID;
    if (!isSpecialDateTimeValueUsec(usec))
        newPos = usec / kMicroInMilliSeconds;

    if (newPos == m_posUtcMs)
        return;

    m_posUtcMs = newPos;
    emit positionChanged(m_posUtcMs);
}

QString QnMediaResourceWidget::calculatePositionText() const
{
    /* Do not show time for regular media files. */
    if (!m_resource->toResourcePtr()->flags().testFlag(Qn::utc))
        return QString();

    static const auto extractTime = [](qint64 dateTimeUsec)
    {
        if (isSpecialDateTimeValueUsec(dateTimeUsec))
            return QString();

        static const auto kOutputFormat = lit("yyyy-MM-dd hh:mm:ss");

        const auto dateTimeMs = dateTimeUsec / kMicroInMilliSeconds;
        return QDateTime::fromMSecsSinceEpoch(dateTimeMs).toString(kOutputFormat);
    };

    const QString timeString = (m_display->camDisplay()->isRealTimeSource()
        ? tr("LIVE") : extractTime(getDisplayTimeUsec()));

    enum { kPositionTextPixelSize = 14 };
    return htmlFormattedParagraph(timeString, kPositionTextPixelSize, true);
}


QString QnMediaResourceWidget::calculateTitleText() const {

    QString resourceName = base_type::calculateTitleText();

    QnPtzObject activeObject;
    QString activeObjectName;
    if(m_ptzController->getActiveObject(&activeObject) && activeObject.type == Qn::TourPtzObject && getPtzObjectName(m_ptzController, activeObject, &activeObjectName)) {
        return tr("%1 (Tour \"%2\" is active)").arg(resourceName).arg(activeObjectName);
    } else {
        return resourceName;
    }
}

int QnMediaResourceWidget::calculateButtonsVisibility() const
{
    int result = base_type::calculateButtonsVisibility();
    bool hasVideo = this->hasVideo(); //m_resource->hasVideo(m_display->mediaProvider()); //TODO: #GDM #high check merge correctness

    if (qnRuntime->isDevMode())
        result |= Qn::DbgScreenshotButton;

    if(hasVideo && !(resource()->toResource()->flags() & Qn::still_image))
        result |= Qn::ScreenshotButton;

    bool rgbImage = false;
    QString url = resource()->toResource()->getUrl().toLower();

    // TODO: #Elric totally evil. Button availability should be based on actual
    // colorspace value, better via some function in enhancement implementation,
    // and not on file extension checks!
    if(((resource()->toResource()->flags() & Qn::still_image)) && !url.endsWith(lit(".jpg")) && !url.endsWith(lit(".jpeg")))
        rgbImage = true;
    if (!rgbImage && hasVideo)
        result |= Qn::EnhancementButton;

    if (!zoomRect().isNull())
        return result;

    if (hasVideo && resource()->toResource()->hasFlags(Qn::motion))
        result |= Qn::MotionSearchButton;

    bool isExportedLayout = item()
        && item()->layout()
        && item()->layout()->resource()
        && item()->layout()->resource()->isFile();

    bool isPreviewSearchLayout = item()
        && item()->layout()
        && item()->layout()->isSearchLayout();

    if(m_camera
        && m_camera->hasPtzCapabilities(Qn::ContinuousPtzCapabilities)
        && !m_camera->hasPtzCapabilities(Qn::VirtualPtzCapability)
        && accessController()->hasPermissions(m_resource->toResourcePtr(), Qn::WritePtzPermission)
        && !isExportedLayout
        && !isPreviewSearchLayout
    ) {
        result |= Qn::PtzButton;
    }

    if (m_dewarpingParams.enabled) {
        result |= Qn::FishEyeButton;
        result &= ~Qn::PtzButton;
    }

    if ((resource()->toResource()->hasFlags(Qn::io_module)))
    {
        if (hasVideo)
            result |= Qn::IoModuleButton;
    }

    if (!(qnSettings->lightMode() & Qn::LightModeNoZoomWindows) && hasVideo) {
        if(item()
                && item()->layout()
                && accessController()->hasPermissions(item()->layout()->resource(), Qn::WritePermission | Qn::AddRemoveItemsPermission)
                )
            result |= Qn::ZoomWindowButton;
    }

    return result;
}

QCursor QnMediaResourceWidget::calculateCursor() const {
    if((options() & (DisplayMotion | DisplayMotionSensitivity | ControlZoomWindow)) || (QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
        return Qt::CrossCursor;
    } else {
        return base_type::calculateCursor();
    }
}

Qn::ResourceStatusOverlay QnMediaResourceWidget::calculateStatusOverlay() const {
    if (qnRuntime->isVideoWallMode() && !QnVideoWallLicenseUsageHelper().isValid())
        return Qn::VideowallWithoutLicenseOverlay;

    QnResourcePtr resource = m_display->resource();

    /// TODO: #ynikitenkov It needs to refactor error\status overlays totally!
    const ResourceStates states = getResourceStates();

    if (m_camera && m_camera->hasFlags(Qn::io_module))
    {
        if (states.isOffline)
            return Qn::OfflineOverlay;

        if (states.isUnauthorized)
            return Qn::UnauthorizedOverlay;

        if (!states.isRealTimeSource)
            return Qn::NoVideoDataOverlay;

        if (m_ioCouldBeShown) /// If widget could be shown then licenses Ok
            return Qn::EmptyOverlay;


        const bool buttonIsVisible = (buttonsOverlay()->rightButtonsBar()->visibleButtons() & Qn::IoModuleButton);
        const QnImageButtonWidget * const button = buttonsOverlay()->rightButtonsBar()->button(Qn::IoModuleButton);
        const bool licenseError = (!button || button->isChecked() || !buttonIsVisible); /// Io is invisible in this case if license error
        const bool isZoomWindow = !zoomRect().isNull();

        if (licenseError && !isZoomWindow)
            return Qn::IoModuleDisabledOverlay;
    }

    if (resource->hasFlags(Qn::local_image)) {
        if (resource->getStatus() == Qn::Offline)
            return Qn::NoDataOverlay;
        if (m_display->camDisplay()->isStillImage() && m_display->camDisplay()->isEOFReached())
            return Qn::NoDataOverlay;
        return Qn::EmptyOverlay;
    } else if (resource->hasFlags(Qn::local_video) && resource->getStatus() == Qn::Offline) {
        return Qn::NoDataOverlay;


    } else if (states.isOffline) {
        return Qn::OfflineOverlay;
    } else if (states.isUnauthorized) {
        return Qn::UnauthorizedOverlay;
    } else if (m_camera && m_camera->isDtsBased() && !m_camera->isLicenseUsed()) {
        return Qn::AnalogWithoutLicenseOverlay;
    } else if (options().testFlag(DisplayActivity) && m_display->isPaused()) {
        if (!qnRuntime->isVideoWallMode())
            return Qn::PausedOverlay;
        return Qn::EmptyOverlay;
    } else if (m_display->camDisplay()->isLongWaiting()) {
        if (m_display->camDisplay()->isEOFReached())
            return Qn::NoDataOverlay;
        QnCachingCameraDataLoader *loader = context()->instance<QnCameraDataManager>()->loader(m_resource, false);
        if (loader && loader->periods(Qn::RecordingContent).containTime(m_display->camDisplay()->getExternalTime() / 1000))
            return base_type::calculateStatusOverlay(Qn::Online, states.hasVideo);
        else
            return Qn::NoDataOverlay;
    } else if (m_display->isPaused()) {
        if (m_display->camDisplay()->isEOFReached())
            return Qn::NoDataOverlay;
        else if (!states.hasVideo)
            return Qn::NoVideoDataOverlay;
        else
            return Qn::EmptyOverlay;
    } else {
        return base_type::calculateStatusOverlay(Qn::Online, states.hasVideo);
    }
}

void QnMediaResourceWidget::at_resource_resourceChanged() {
    invalidateMotionSensitivity();
}

void QnMediaResourceWidget::at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key) {
    Q_UNUSED(resource);
    if (key == QnMediaResource::customAspectRatioKey())
        updateCustomAspectRatio();
}

void QnMediaResourceWidget::updateAspectRatio() {
    if(!m_renderer)
        return; /* Not yet initialized. */

    QSize sourceSize = m_renderer->sourceSize();

    qreal dewarpingRatio = item() && item()->dewarpingParams().enabled && m_dewarpingParams.enabled
            ? item()->dewarpingParams().panoFactor
            : 1.0;

    QString resourceId;
    if (const QnNetworkResource *networkResource = dynamic_cast<const QnNetworkResource*>(resource()->toResource()))
        resourceId = networkResource->getPhysicalId();

    if(sourceSize.isEmpty()) {
        qreal aspectRatio = resourceId.isEmpty()
                            ? defaultAspectRatio()
                            : qnSettings->resourceAspectRatios().value(resourceId, defaultAspectRatio());
        if (dewarpingRatio > 1)
            setAspectRatio(dewarpingRatio);
        else
            setAspectRatio(aspectRatio);
    } else {
        qreal aspectRatio = QnGeometry::aspectRatio(sourceSize) *
                            QnGeometry::aspectRatio(channelLayout()->size()) *
                            (zoomRect().isNull() ? 1.0 : QnGeometry::aspectRatio(zoomRect()));


        if (dewarpingRatio > 1)
            setAspectRatio(dewarpingRatio);
        else
            setAspectRatio(aspectRatio);

        if (!resourceId.isEmpty()) {
            QnAspectRatioHash aspectRatios = qnSettings->resourceAspectRatios();
            aspectRatios.insert(resourceId, aspectRatio);
            qnSettings->setResourceAspectRatios(aspectRatios);
        }
    }
}

void QnMediaResourceWidget::at_camDisplay_liveChanged() {
    bool isLive = m_display->camDisplay()->isRealTimeSource();

    if (!isLive) {
        buttonsOverlay()->rightButtonsBar()->setButtonsChecked(Qn::PtzButton, false);
        suspendHomePtzController();
    } else {
        resumeHomePtzController();
    }

    updateCompositeOverlayMode();
}

void QnMediaResourceWidget::at_screenshotButton_clicked() {
    menu()->trigger(QnActions::TakeScreenshotAction, this);
}

void QnMediaResourceWidget::at_searchButton_toggled(bool checked) {
    setOption(DisplayMotion, checked);

    if(checked)
        buttonsOverlay()->rightButtonsBar()->setButtonsChecked(Qn::PtzButton | Qn::FishEyeButton | Qn::ZoomWindowButton, false);
}

void QnMediaResourceWidget::at_ptzButton_toggled(bool checked) {
    bool ptzEnabled =
        checked && (m_camera && (m_camera->getPtzCapabilities() & Qn::ContinuousPtzCapabilities));

    setOption(ControlPtz, ptzEnabled);
    setOption(DisplayCrosshair, ptzEnabled);
    if(checked) {
        buttonsOverlay()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton | Qn::ZoomWindowButton, false);
        action(QnActions::JumpToLiveAction)->trigger(); // TODO: #Elric evil hack! Won't work if SYNC is off and this item is not selected?
    }
}

void QnMediaResourceWidget::at_fishEyeButton_toggled(bool checked) {
    QnItemDewarpingParams params = item()->dewarpingParams();
    params.enabled = checked;
    item()->setDewarpingParams(params); // TODO: #Elric #PTZ move to instrument

    setOption(DisplayDewarped, checked);
    if (checked) {
        setOption(DisplayMotion, false);
        resumeHomePtzController();
    } else {
        /* Stop all ptz activity. */
        ptzController()->continuousMove(QVector3D(0, 0, 0));
        suspendHomePtzController();
    }

    updateButtonsVisibility();
}

void QnMediaResourceWidget::at_zoomWindowButton_toggled(bool checked) {
    setOption(ControlZoomWindow, checked);

    if(checked)
    {
        buttonsOverlay()->rightButtonsBar()->setButtonsChecked(
        Qn::PtzButton | Qn::FishEyeButton | Qn::MotionSearchButton, false);
    }
}

void QnMediaResourceWidget::at_histogramButton_toggled(bool checked) {
    ImageCorrectionParams params = item()->imageEnhancement();
    if (params.enabled == checked)
        return;

    params.enabled = checked;
    setImageEnhancement(params);
}

void QnMediaResourceWidget::at_ioModuleButton_toggled(bool checked) {
    Q_UNUSED(checked);
    if (m_ioModuleOverlayWidget)
        updateIoModuleVisibility(true);
}

void QnMediaResourceWidget::at_renderWatcher_widgetChanged(QnResourceWidget *widget) {
    if(widget == this)
        updateRendererEnabled();
}

void QnMediaResourceWidget::at_zoomRectChanged() {
    updateButtonsVisibility();
    updateAspectRatio();
    updateIconButton();

    // TODO: #PTZ probably belongs to instrument.
    if (options() & DisplayDewarped)
        m_ptzController->absoluteMove(Qn::LogicalPtzCoordinateSpace, QnFisheyePtzController::positionFromRect(m_dewarpingParams, zoomRect()), 2.0);
}

void QnMediaResourceWidget::at_ptzController_changed(Qn::PtzDataFields fields) {
    if(fields & Qn::CapabilitiesPtzField)
        updateButtonsVisibility();
    if(fields & (Qn::ActiveObjectPtzField | Qn::ToursPtzField))
        updateTitleText();
}

void QnMediaResourceWidget::updateDewarpingParams() {
    if (m_dewarpingParams == m_resource->getDewarpingParams())
        return;

    m_dewarpingParams = m_resource->getDewarpingParams();
    emit dewarpingParamsChanged();
}

void QnMediaResourceWidget::updateFisheye() {
    QnItemDewarpingParams itemParams = item()->dewarpingParams();
    bool enabled = itemParams.enabled;

    bool fisheyeEnabled = enabled && m_dewarpingParams.enabled;

    setOption(ControlPtz, fisheyeEnabled && zoomRect().isEmpty());
    setOption(DisplayCrosshair, fisheyeEnabled && zoomRect().isEmpty());
    setOption(DisplayDewarped, fisheyeEnabled);
    if (fisheyeEnabled && buttonsOverlay()->rightButtonsBar()->button(Qn::FishEyeButton))
        buttonsOverlay()->rightButtonsBar()->button(Qn::FishEyeButton)->setChecked(fisheyeEnabled);
    if(enabled)
        buttonsOverlay()->rightButtonsBar()->setButtonsChecked(Qn::MotionSearchButton | Qn::ZoomWindowButton, false);

    bool flip = fisheyeEnabled
            && m_dewarpingParams.viewMode == QnMediaDewarpingParams::VerticalDown;

    const QList<int> allowedPanoFactorValues = m_dewarpingParams.allowedPanoFactorValues();
    if (!allowedPanoFactorValues.contains(itemParams.panoFactor)) {
        itemParams.panoFactor = allowedPanoFactorValues.last();
        item()->setDewarpingParams(itemParams);
    }
    item()->setData(Qn::ItemFlipRole, flip);

    updateAspectRatio();
    if (display() && display()->camDisplay())
        display()->camDisplay()->setFisheyeEnabled(fisheyeEnabled);

    emit fisheyeChanged();

    if(buttonsOverlay()->rightButtonsBar()->visibleButtons() & Qn::PtzButton)
        at_ptzButton_toggled(buttonsOverlay()->rightButtonsBar()->checkedButtons() & Qn::PtzButton); // TODO: #Elric doesn't belong here, hack
}

void QnMediaResourceWidget::updateCustomAspectRatio() {
    if (!m_display || !m_resource)
        return;

    m_display->camDisplay()->setOverridenAspectRatio(m_resource->customAspectRatio());
}

QnMediaResourceWidget::ResourceStates QnMediaResourceWidget::getResourceStates() const
{
    const auto camDisplay = (m_display ? m_display->camDisplay() : nullptr);
    const auto resource = (m_display ? m_display->resource() : QnResourcePtr());

    ResourceStates result;
    result.isRealTimeSource = (camDisplay ? camDisplay->isRealTimeSource() : false);
    result.isOffline = (result.isRealTimeSource && (!resource || (resource->getStatus() == Qn::Offline)));
    result.isUnauthorized = (result.isRealTimeSource && (resource && (resource->getStatus() == Qn::Unauthorized)));
    result.hasVideo = hasVideo();

    return result;
}

void QnMediaResourceWidget::updateIoModuleVisibility(bool animate) {
    NX_ASSERT(m_camera && m_camera->hasFlags(Qn::io_module) && m_ioLicenceStatusHelper
        , Q_FUNC_INFO, "updateIoModuleVisibility should be called only for I/O modules");

    if (!m_camera || !m_camera->hasFlags(Qn::io_module) || !m_ioLicenceStatusHelper)
        return;

    const QnImageButtonWidget * const button = buttonsOverlay()->rightButtonsBar()->button(Qn::IoModuleButton);
    const bool ioBtnChecked = (button && button->isChecked());
    const bool onlyIoData = !hasVideo();
    const bool correctLicenceStatus = (m_ioLicenceStatusHelper->status() == QnSingleCamLicenceStatusHelper::LicenseUsed);

    const auto resource = m_display->resource();

    /// TODO: #ynikitenkov It needs to refactor error\status overlays totally!

    m_ioCouldBeShown = ((ioBtnChecked || onlyIoData) && correctLicenceStatus);
    const ResourceStates states = getResourceStates();
    const bool correctState = (!states.isOffline && !states.isUnauthorized && states.isRealTimeSource);
    const OverlayVisibility visibility =  (m_ioCouldBeShown && correctState ? Visible : Invisible);
    setOverlayWidgetVisibility(m_ioModuleOverlayWidget, visibility, animate);
    updateOverlayWidgetsVisibility(animate);

    setStatusOverlay(calculateStatusOverlay(), animate);
    updateOverlayButton();
}

void QnMediaResourceWidget::updateOverlayButton() {

    if (m_camera) {
        Qn::ResourceStatusOverlay overlay = calculateStatusOverlay();

        if (overlay == Qn::OfflineOverlay) {
            if (menu()->canTrigger(QnActions::CameraDiagnosticsAction, m_camera)) {
                statusOverlayWidget()->setButtonType(QnStatusOverlayWidget::DiagnosticsButton);
                return;
            }
        } else if (overlay == Qn::IoModuleDisabledOverlay) {

            NX_ASSERT(m_ioLicenceStatusHelper, Q_FUNC_INFO, "Query I/O status overlay for resource widget which is not containing I/O module");

            if (!m_ioLicenceStatusHelper)
                return;

            switch (m_ioLicenceStatusHelper->status()) {
            case QnSingleCamLicenceStatusHelper::LicenseNotUsed:
                statusOverlayWidget()->setButtonType(QnStatusOverlayWidget::IoEnableButton);
                return;
            case QnSingleCamLicenceStatusHelper::LicenseOverflow:
                statusOverlayWidget()->setButtonType(QnStatusOverlayWidget::MoreLicensesButton);
                return;
            default:
                break;
            }
        }
    }

    statusOverlayWidget()->setButtonType(QnStatusOverlayWidget::NoButton);
}

void QnMediaResourceWidget::at_statusOverlayWidget_diagnosticsRequested() {
    if (m_camera)
        menu()->trigger(QnActions::CameraDiagnosticsAction, m_camera);
}

void QnMediaResourceWidget::at_statusOverlayWidget_ioEnableRequested() {
    NX_ASSERT(m_ioLicenceStatusHelper, Q_FUNC_INFO
        , "at_statusOverlayWidget_ioEnableRequested could not be processed for non-I/O modules");

    if (!m_ioLicenceStatusHelper)
        return;

    const auto licenceStatus = m_ioLicenceStatusHelper->status();
    if (licenceStatus != QnSingleCamLicenceStatusHelper::LicenseNotUsed)
        return;

    qnResourcesChangesManager->saveCamera(m_camera, [](const QnVirtualCameraResourcePtr &camera){
        camera->setLicenseUsed(true);
    });

    updateIoModuleVisibility(true);
}

void QnMediaResourceWidget::at_statusOverlayWidget_moreLicensesRequested() {
    menu()->trigger(QnActions::PreferencesLicensesTabAction);
}

void QnMediaResourceWidget::at_item_imageEnhancementChanged() {
    setImageEnhancement(item()->imageEnhancement());
}

void QnMediaResourceWidget::updateCompositeOverlayMode()
{
    const bool isLive = (m_display && m_display->camDisplay()
        ? m_display->camDisplay()->isRealTimeSource() : false);

    const bool bookmarksEnabled = (!isLive && navigator()->bookmarksModeEnabled() && !m_camera.isNull());
    const auto mode = (bookmarksEnabled ? QnCompositeTextOverlay::kBookmarksMode
        : (isLive ? QnCompositeTextOverlay::kTextOutputMode : QnCompositeTextOverlay::kUndefinedMode));
    m_compositeTextOverlay->setMode(mode);
}

qint64 QnMediaResourceWidget::getUtcCurrentTimeUsec() const {
    // get timestamp from the first channel that was painted
    int channel = std::distance(m_paintedChannels.cbegin(),
        std::find_if(m_paintedChannels.cbegin(), m_paintedChannels.cend(), [](bool value){ return value; }));
    if (channel >= channelCount())
        channel = 0;

    qint64 timestampUsec = hasVideo()
        ? m_renderer->getTimestampOfNextFrameToRender(channel)
        : display()->camDisplay()->getCurrentTime();

    return timestampUsec;
}

qint64 QnMediaResourceWidget::getUtcCurrentTimeMs() const
{
    qint64 datetimeUsec = getUtcCurrentTimeUsec();
    if (datetimeUsec == DATETIME_NOW)
        return qnSyncTime->currentMSecsSinceEpoch();
    else if (datetimeUsec == AV_NOPTS_VALUE)
        return 0;
    else
        return datetimeUsec/1000;
}

qint64 QnMediaResourceWidget::getDisplayTimeUsec() const
{
    qint64 result = getUtcCurrentTimeUsec();
    if (!isSpecialDateTimeValueUsec(result))
        result += context()->instance<QnWorkbenchServerTimeWatcher>()->displayOffset(m_resource) * 1000ll;
    return result;
}

QnCompositeTextOverlay *QnMediaResourceWidget::compositeTextOverlay()
{
    return m_compositeTextOverlay;
}

