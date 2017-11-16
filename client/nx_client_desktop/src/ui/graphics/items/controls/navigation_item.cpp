#include "navigation_item.h"

#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QAction>
#include <QtWidgets/QGraphicsLinearLayout>

#include <common/common_globals.h>

#include <nx/streaming/abstract_archive_stream_reader.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/synctime.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/common/palette.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/graphics/items/controls/speed_slider.h>
#include <ui/graphics/items/controls/volume_slider.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/widgets/calendar_widget.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>

#include <core/resource/resource.h>

#include <nx/fusion/model_functions.h>
#include <utils/common/scoped_painter_rollback.h>

#include "time_slider.h"
#include "time_scroll_bar.h"
#include "timeline_placeholder.h"
#include "clock_label.h"

using namespace nx::client::desktop::ui;

QnNavigationItem::QnNavigationItem(QGraphicsItem *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent->toGraphicsObject()),
    m_updatingSpeedSliderFromNavigator(false),
    m_updatingNavigatorFromSpeedSlider(false)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::ArrowCursor);

    setFrameShape(Qn::RectangularFrame);
    setFrameBorders(Qt::TopEdge);
    setFrameColor(palette().color(QPalette::Midlight));
    setFrameWidth(1.0);

    /* Create buttons. */
    m_jumpBackwardButton = newActionButton(action::JumpToStartAction);
    m_jumpBackwardButton->setIcon(qnSkin->icon("slider/navigation/rewind_backward.png"));
    m_jumpBackwardButton->setPreferredSize(32, 32);

    m_stepBackwardButton = newActionButton(action::PreviousFrameAction);
    m_stepBackwardButton->setIcon(qnSkin->icon("slider/navigation/step_backward.png"));
    m_stepBackwardButton->setPreferredSize(32, 32);

    m_playButton = newActionButton(action::PlayPauseAction);
    m_playButton->setIcon(qnSkin->icon("slider/navigation/play.png", "slider/navigation/pause.png"));
    m_playButton->setPreferredSize(32, 32);

    m_stepForwardButton = newActionButton(action::NextFrameAction);
    m_stepForwardButton->setIcon(qnSkin->icon("slider/navigation/step_forward.png"));
    m_stepForwardButton->setPreferredSize(32, 32);

    m_jumpForwardButton = newActionButton(action::JumpToEndAction);
    m_jumpForwardButton->setIcon(qnSkin->icon("slider/navigation/rewind_forward.png"));
    m_jumpForwardButton->setPreferredSize(32, 32);

    m_muteButton = newActionButton(action::ToggleMuteAction);
    m_muteButton->setIcon(qnSkin->icon("slider/buttons/unmute.png", "slider/buttons/mute.png"));
    m_muteButton->setPreferredSize(24, 24);

    m_liveButton = newActionButton(action::JumpToLiveAction);
    m_liveButton->setIcon(qnSkin->icon("slider/buttons/live.png"));
    m_liveButton->setPreferredSize(52, 24);

    m_syncButton = newActionButton(action::ToggleSyncAction);
    m_syncButton->setIcon(qnSkin->icon("slider/buttons/sync.png"));
    m_syncButton->setPreferredSize(52, 24);

    m_bookmarksModeButton = newActionButton(action::BookmarksModeAction);
    m_bookmarksModeButton->setIcon(qnSkin->icon("slider/buttons/bookmarks.png"));
    m_bookmarksModeButton->setPreferredSize(52, 24);

    m_calendarButton = newActionButton(action::ToggleCalendarAction);
    m_calendarButton->setIcon(qnSkin->icon("slider/buttons/calendar.png"));
    m_calendarButton->setPreferredSize(52, 24);

    /* Create clock label. */
    QnClockLabel* clockLabel = new QnClockLabel(this);
    clockLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    clockLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    clockLabel->setPreferredHeight(28.0);

    /* Workaround to avoid size flickering due to our scene scaling: */
    clockLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    /* Create sliders. */
    m_speedSlider = new QnSpeedSlider(this);
    m_speedSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_speedSlider->installEventFilter(this);
    m_speedSlider->setPreferredHeight(28.0);
    context()->statisticsModule()->registerSlider(lit("speed_slider"), m_speedSlider);

    m_volumeSlider = new QnVolumeSlider(this);
    m_volumeSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_volumeSlider->toolTipItem()->setParentItem(parent);
    m_volumeSlider->setPreferredHeight(28.0);

    m_timeSlider = new QnTimeSlider(this, parent);
    m_timeSlider->setOption(QnTimeSlider::UnzoomOnDoubleClick, false);

    GraphicsLabel* timelinePlaceholder = new QnTimelinePlaceholder(this, m_timeSlider);
    timelinePlaceholder->setPerformanceHint(GraphicsLabel::PixmapCaching);

    m_timeScrollBar = new QnTimeScrollBar(this);
    m_timeScrollBar->setPreferredHeight(16);

    m_separators = new QnFramedWidget(this);
    m_separators->setWindowBrush(Qt::NoBrush);
    m_separators->setFrameShape(Qn::RectangularFrame);
    m_separators->setFrameBorders(Qt::LeftEdge | Qt::RightEdge);
    m_separators->setFrameColor(palette().color(QPalette::Midlight));
    m_separators->setFrameWidth(1.0);

    connect(navigator(), &QnWorkbenchNavigator::timelineRelevancyChanged, this,
        [this, timelinePlaceholder](bool isRelevant)
        {
            bool reset = m_timeSlider->isVisible() != isRelevant;
            m_timeSlider->setVisible(isRelevant);
            m_timeSlider->toolTipItem()->setVisible(isRelevant);
            m_timeScrollBar->setVisible(isRelevant);
            timelinePlaceholder->setVisible(!isRelevant);
            m_separators->setFrameColor(palette().color(isRelevant ? QPalette::Shadow : QPalette::Midlight));
            m_separators->setFrameWidth(isRelevant ? 2.0 : 1.0);
            updatePlaybackButtonsEnabled();
            if (reset)
                m_timeSlider->invalidateWindow();
        });

    /* Initialize navigator. */
    navigator()->setTimeSlider(m_timeSlider);
    navigator()->setTimeScrollBar(m_timeScrollBar);

    /* Put it all into layouts. */
    QGraphicsLinearLayout* leftButtonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    leftButtonsLayout->setSpacing(1);
    leftButtonsLayout->addItem(m_jumpBackwardButton);
    leftButtonsLayout->addItem(m_stepBackwardButton);
    leftButtonsLayout->addItem(m_playButton);
    leftButtonsLayout->addItem(m_stepForwardButton);
    leftButtonsLayout->addItem(m_jumpForwardButton);

    QGraphicsLinearLayout* leftLayout = new QGraphicsLinearLayout(Qt::Vertical);
    leftLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    leftLayout->setContentsMargins(6, 0, 6, 0);
    leftLayout->setSpacing(0);
    leftLayout->setMinimumHeight(88.0);
    leftLayout->addItem(m_speedSlider);
    leftLayout->addItem(leftButtonsLayout);
    leftLayout->addItem(clockLabel);

    QGraphicsLinearLayout* rightLayoutTop = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutTop->setContentsMargins(0, 0, 0, 0);
    rightLayoutTop->setSpacing(3);
    rightLayoutTop->addItem(m_muteButton);
    rightLayoutTop->addItem(m_volumeSlider);
    rightLayoutTop->setAlignment(m_muteButton, Qt::AlignLeft | Qt::AlignCenter);

    QGraphicsLinearLayout* rightLayoutMiddle = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutMiddle->setContentsMargins(0, 0, 0, 0);
    rightLayoutMiddle->setSpacing(2);
    rightLayoutMiddle->addItem(m_liveButton);
    rightLayoutMiddle->addItem(m_syncButton);

    QGraphicsLinearLayout* rightLayoutBottom = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutBottom->setContentsMargins(0, 0, 0, 0);
    rightLayoutBottom->setSpacing(2);
    rightLayoutBottom->addItem(m_bookmarksModeButton);
    rightLayoutBottom->addItem(m_calendarButton);

    QGraphicsLinearLayout* rightLayout = new QGraphicsLinearLayout(Qt::Vertical);
    rightLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    rightLayout->setContentsMargins(6, 2, 6, 6);
    rightLayout->setSpacing(2);
    rightLayout->setMinimumHeight(88.0);
    rightLayout->addItem(rightLayoutTop);
    rightLayout->addItem(rightLayoutMiddle);
    rightLayout->addItem(rightLayoutBottom);

    QGraphicsLinearLayout* sliderLayout = new QGraphicsLinearLayout(Qt::Vertical);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    sliderLayout->setSpacing(0);
    sliderLayout->addItem(m_timeSlider);
    sliderLayout->addItem(m_timeScrollBar);
    sliderLayout->addItem(timelinePlaceholder);

    m_timeSlider->hide();
    m_timeScrollBar->hide();

    QGraphicsLinearLayout* mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addItem(leftLayout);
    mainLayout->setAlignment(leftLayout, Qt::AlignLeft | Qt::AlignBottom);
    mainLayout->addItem(sliderLayout);
    mainLayout->addItem(rightLayout);
    mainLayout->setAlignment(rightLayout, Qt::AlignLeft | Qt::AlignBottom);
    setLayout(mainLayout);

    /* Set up handlers. */
    QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
    connect(streamSynchronizer, &QnWorkbenchStreamSynchronizer::runningChanged,     this,           &QnNavigationItem::updateSyncButtonState);
    connect(streamSynchronizer, &QnWorkbenchStreamSynchronizer::effectiveChanged,   this,           &QnNavigationItem::updateSyncButtonState);

    connect(m_speedSlider,      &QnSpeedSlider::roundedSpeedChanged,                this,           &QnNavigationItem::updateNavigatorSpeedFromSpeedSlider);
    connect(m_volumeSlider,     &QnVolumeSlider::valueChanged,                      this,           &QnNavigationItem::updateMuteButtonChecked);

    connect(action(action::PreviousFrameAction), &QAction::triggered,            this,           &QnNavigationItem::at_stepBackwardButton_clicked);
    connect(action(action::NextFrameAction),     &QAction::triggered,            this,           &QnNavigationItem::at_stepForwardButton_clicked);
    connect(action(action::JumpToStartAction),   &QAction::triggered,            navigator(),    &QnWorkbenchNavigator::jumpBackward);
    connect(action(action::JumpToEndAction),     &QAction::triggered,            navigator(),    &QnWorkbenchNavigator::jumpForward);
    connect(action(action::JumpToLiveAction),    &QAction::triggered,            this,           &QnNavigationItem::at_liveButton_clicked);
    connect(action(action::ToggleSyncAction),    &QAction::triggered,            this,           &QnNavigationItem::at_syncButton_clicked);
    connect(action(action::PlayPauseAction),     &QAction::toggled,              navigator(),    &QnWorkbenchNavigator::setPlaying);
    connect(action(action::PlayPauseAction),     &QAction::toggled,              this,           &QnNavigationItem::updateSpeedSliderParametersFromNavigator);
    connect(action(action::PlayPauseAction),     &QAction::toggled,              this,           &QnNavigationItem::updatePlaybackButtonsIcons);

    connect(action(action::ToggleMuteAction),    &QAction::toggled,              m_volumeSlider, &QnVolumeSlider::setMute);
    connect(action(action::VolumeUpAction),      &QAction::triggered,            m_volumeSlider, &QnVolumeSlider::stepForward);
    connect(action(action::VolumeDownAction),    &QAction::triggered,            m_volumeSlider, &QnVolumeSlider::stepBackward);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged, this, [this]()
    {
        const auto currentWidget = navigator()->currentWidget();
        if (currentWidget)
            disconnect(currentWidget, &QnResourceWidget::optionsChanged, this, nullptr);
    });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this, [this]()
    {
        const auto currentWidget = navigator()->currentWidget();
        if (currentWidget)
            connect(currentWidget, &QnResourceWidget::optionsChanged, this, &QnNavigationItem::updateBookButtonEnabled);
    });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged, m_speedSlider, &QnSpeedSlider::finishAnimations);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        &QnNavigationItem::updateSyncButtonState);
    connect(navigator(), &QnWorkbenchNavigator::syncIsForcedChanged, this,
        &QnNavigationItem::updateSyncButtonState);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,       this,   &QnNavigationItem::updateJumpButtonsTooltips);
    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,       this,   &QnNavigationItem::updateBookButtonEnabled);
    connect(navigator(), &QnWorkbenchNavigator::liveChanged,                this,   &QnNavigationItem::updateLiveButtonState);
    connect(navigator(), &QnWorkbenchNavigator::liveChanged,                this,   &QnNavigationItem::updatePlaybackButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::liveSupportedChanged,       this,   &QnNavigationItem::updateLiveButtonState);
    connect(navigator(), &QnWorkbenchNavigator::playingSupportedChanged,    this,   &QnNavigationItem::updatePlaybackButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::playingSupportedChanged,    this,   &QnNavigationItem::updateVolumeButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::speedChanged,               this,   &QnNavigationItem::updateSpeedSliderSpeedFromNavigator);
    connect(navigator(), &QnWorkbenchNavigator::speedRangeChanged,          this,   &QnNavigationItem::updateSpeedSliderParametersFromNavigator);
    connect(navigator(), &QnWorkbenchNavigator::hasArchiveChanged,          this,   &QnNavigationItem::updatePlaybackButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::liveSupportedChanged,       this,   [this]() { m_timeSlider->setLiveSupported(navigator()->isLiveSupported()); });

    /* Play button is not synced with the actual playing state, so we update it only when current widget changes. */
    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,       this,   &QnNavigationItem::updatePlayButtonChecked, Qt::QueuedConnection);

    connect(this, &QGraphicsWidget::geometryChanged, [this, sliderLayout]() { m_separators->setGeometry(sliderLayout->geometry()); });

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this, &QnNavigationItem::updateBookButtonEnabled);

    /* Register actions. */
    addAction(action(action::PreviousFrameAction));
    addAction(action(action::NextFrameAction));
    addAction(action(action::PlayPauseAction));
    addAction(action(action::JumpToStartAction));
    addAction(action(action::JumpToEndAction));

    addAction(action(action::VolumeDownAction));
    addAction(action(action::VolumeUpAction));

    addAction(action(action::JumpToLiveAction));
    addAction(action(action::ToggleMuteAction));
    addAction(action(action::ToggleSyncAction));
    addAction(action(action::ToggleCalendarAction));

    /* Set help topics. */
    setHelpTopic(this,                  Qn::MainWindow_Playback_Help);
    // setHelpTopic(m_bookmarksModeButton, Qn::MainWindow_???_Help); // TODO: #dklychkov Use correct help ID
    setHelpTopic(m_volumeSlider,        Qn::MainWindow_Slider_Volume_Help);
    setHelpTopic(m_muteButton,          Qn::MainWindow_Slider_Volume_Help);
    setHelpTopic(m_liveButton,          Qn::MainWindow_Navigation_Help);
    setHelpTopic(m_playButton,          Qn::MainWindow_Navigation_Help);
    setHelpTopic(m_stepBackwardButton,  Qn::MainWindow_Navigation_Help);
    setHelpTopic(m_stepForwardButton,   Qn::MainWindow_Navigation_Help);
    setHelpTopic(m_jumpBackwardButton,  Qn::MainWindow_Navigation_Help);
    setHelpTopic(m_jumpForwardButton,   Qn::MainWindow_Navigation_Help);
    setHelpTopic(m_syncButton,          Qn::MainWindow_Sync_Help);
    setHelpTopic(m_calendarButton,      Qn::MainWindow_Calendar_Help);
    setHelpTopic(m_bookmarksModeButton, Qn::Bookmarks_Usage_Help);

    /* Run handlers */
    updateMuteButtonChecked();
    updateSyncButtonState();
    updateLiveButtonState();
    updatePlaybackButtonsEnabled();
    updateVolumeButtonsEnabled();

    updatePlaybackButtonsIcons();
    updateSpeedSliderParametersFromNavigator();
    updateSpeedSliderSpeedFromNavigator();

    updateGeometry();
}

QnNavigationItem::~QnNavigationItem()
{
}

// -------------------------------------------------------------------------- //
// Updaters
// -------------------------------------------------------------------------- //
void QnNavigationItem::updateSpeedSliderParametersFromNavigator()
{
    qreal minimalSpeed = navigator()->minimalSpeed();
    qreal maximalSpeed = navigator()->maximalSpeed();

    qreal speedStep, defaultSpeed;
    if (m_playButton->isChecked())
    {
        speedStep = 1.0;
        defaultSpeed = 1.0;
    }
    else
    {
        speedStep = 0.25;
        defaultSpeed = 0.0;
        maximalSpeed = qMin(qMax(1.0, maximalSpeed * speedStep), maximalSpeed);
        minimalSpeed = qMax(qMin(-1.0, minimalSpeed * speedStep), minimalSpeed);
    }

    /* The calls that follow may change speed */
    QN_SCOPED_VALUE_ROLLBACK(&m_updatingSpeedSliderFromNavigator, true);

    m_speedSlider->setSpeedRange(minimalSpeed, maximalSpeed);
    m_speedSlider->setMinimalSpeedStep(speedStep);
    m_speedSlider->setDefaultSpeed(defaultSpeed);
}

void QnNavigationItem::updateSpeedSliderSpeedFromNavigator()
{
    if (m_updatingNavigatorFromSpeedSlider)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updatingSpeedSliderFromNavigator, true);
    m_speedSlider->setSpeed(navigator()->speed());
    updatePlayButtonChecked();
}

void QnNavigationItem::updateNavigatorSpeedFromSpeedSlider()
{
    if (m_updatingSpeedSliderFromNavigator)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updatingNavigatorFromSpeedSlider, true);
    navigator()->setSpeed(m_speedSlider->roundedSpeed());
}

void QnNavigationItem::updatePlaybackButtonsIcons()
{
    bool playing = m_playButton->isChecked();

    // TODO: #Elric this is cheating!
    action(action::PreviousFrameAction)->setText(playing ? tr("Speed Down") : tr("Previous Frame"));
    action(action::NextFrameAction)->setText(playing ? tr("Speed Up") : tr("Next Frame"));

    m_stepBackwardButton->setIcon(playing
        ? qnSkin->icon("slider/navigation/backward.png")
        : qnSkin->icon("slider/navigation/step_backward.png"));
    m_stepForwardButton->setIcon(playing
        ? qnSkin->icon("slider/navigation/forward.png")
        : qnSkin->icon("slider/navigation/step_forward.png"));

    updatePlaybackButtonsEnabled(); // TODO: #Elric remove this once buttonwidget <-> action enabled sync is implemented. OR when we disable actions and not buttons.
}

void QnNavigationItem::updateJumpButtonsTooltips()
{
    bool hasPeriods = navigator()->currentWidgetFlags() & QnWorkbenchNavigator::WidgetSupportsPeriods;

    // TODO: #Elric this is cheating!
    action(action::JumpToStartAction)->setText(hasPeriods ? tr("Previous Chunk") : tr("To Start"));
    action(action::JumpToEndAction)->setText(hasPeriods ? tr("Next Chunk") : tr("To End"));

    updatePlaybackButtonsEnabled(); // TODO: #Elric remove this once buttonwidget <-> action enabled sync is implemented. OR when we disable actions and not buttons.
}

void QnNavigationItem::updateBookButtonEnabled()
{
    const auto currentWidget = navigator()->currentWidget();

    const bool motionSearchMode = (currentWidget
        && currentWidget->options().testFlag(QnResourceWidget::DisplayMotion));

    const bool bookmarksEnabled = accessController()->hasGlobalPermission(Qn::GlobalViewBookmarksPermission)
        && (currentWidget && currentWidget->resource()->flags().testFlag(Qn::live));

    const auto layout = workbench()->currentLayout();
    const bool bookmarkMode = (bookmarksEnabled && !motionSearchMode
        && layout->data(Qn::LayoutBookmarksModeRole).toBool());

    const auto modeAction = action(action::BookmarksModeAction);
    modeAction->setEnabled(bookmarksEnabled);
    modeAction->setChecked(bookmarkMode);
}

void QnNavigationItem::updatePlaybackButtonsEnabled()
{
    /*
     * Method logic: enable the following buttons if current widget is:
     * - local image: no buttons
     * - local audio file or exported audio: play/pause, jump forward and backward
     * - local video file or exported video: all buttons
     * - camera: play button and:
     * - - if has footage: jump backward, step backward
     * - - if NOT on the live (has footage by default): jump forward, step forward
     * - I/O module: play button and:
     * - - if has footage: jump backward
     * - - if NOT on the live (has footage by default): jump forward
     */

    /* If not playable, it is a local image, all buttons must be disabled. */
    bool playable = navigator()->isPlayingSupported();

    /* Makes sense only for cameras. Local files are never live. */
    bool forwardable = !navigator()->isLive();

    /* Side effects are Evil! */
    bool isLocalFile = !navigator()->isLiveSupported();

    /* Will return true if there is at least one camera with archive on the scene, even is local file is selected. */
    bool hasCameraFootage = navigator()->hasArchive();

    /* Check if current widget has real video (not image, not audio file, not I/O module). */
    bool hasVideo = navigator()->currentWidgetHasVideo();

    /* All options except local image. */
    m_playButton->setEnabled(playable);

    /* Local/exported video, camera with footage. */
    bool canStepBackwardOrSpeedDown = playable && (hasCameraFootage || isLocalFile ) && hasVideo;
    m_stepBackwardButton->setEnabled(canStepBackwardOrSpeedDown);

    /* Local/exported video, camera with footage but not on live. */
    bool canStepForwardOrSpeedUp = playable && ((hasCameraFootage && forwardable) || isLocalFile) && hasVideo;
    m_stepForwardButton->setEnabled(canStepForwardOrSpeedUp);

    /* Local/exported file (not image), camera or I/O module with footage. */
    bool canJumpBackward = playable && (hasCameraFootage || isLocalFile);
    m_jumpBackwardButton->setEnabled(canJumpBackward);

    /* Local/exported file (not image), camera or I/O module with footage but not on live. */
    bool canJumpForward = playable && ((hasCameraFootage && forwardable) || isLocalFile);
    m_jumpForwardButton->setEnabled(canJumpForward);

    /*
     * For now, there is a bug when I/O module is placed on the scene together with camera (synced).
     * If we select camera, we can change speed to -16x, but when we select I/O module,
     * client will be in strange state: speed slider allows only 0x and 1x, but current speed is -16x.
     * So we making the slider enabled for I/O module to do not make the situation even stranger.
     */
    m_speedSlider->setEnabled(playable && m_timeSlider->isVisible());
}

void QnNavigationItem::updateVolumeButtonsEnabled()
{
    bool isTimelineVisible = navigator()->isPlayingSupported();
    m_muteButton->setEnabled(isTimelineVisible);
}

void QnNavigationItem::updateMuteButtonChecked()
{
    m_muteButton->setChecked(m_volumeSlider->isMute());
}

void QnNavigationItem::updateLiveButtonState()
{
    /* setEnabled must be called last to avoid update from button's action enabled state. */
    bool enabled = navigator()->isLiveSupported();
    m_liveButton->setChecked(enabled && navigator()->isLive());
    m_liveButton->setEnabled(enabled);
}

void QnNavigationItem::updateSyncButtonState()
{
    const auto streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
    const auto syncForced = navigator()->syncIsForced();

    const auto syncAllowed = streamSynchronizer->isEffective()
        && navigator()->currentWidgetFlags().testFlag(QnWorkbenchNavigator::WidgetSupportsSync);

    // Call setEnabled last to avoid update from button's action enabled state.
    m_syncButton->setChecked(syncAllowed && streamSynchronizer->isRunning());
    m_syncButton->setEnabled(syncAllowed && !syncForced);

    m_syncButton->setToolTip(syncForced
        ? tr("NVR channels cannot be played unsyncronously")
        : action(action::ToggleSyncAction)->toolTip());
}

void QnNavigationItem::updatePlayButtonChecked()
{
    m_playButton->setChecked(navigator()->isPlaying());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnNavigationItem::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_speedSlider && event->type() == QEvent::GraphicsSceneWheel)
        return at_speedSlider_wheelEvent(static_cast<QGraphicsSceneWheelEvent *>(event));

    if (event->type() == QEvent::PaletteChange)
    {
        setFrameColor(palette().color(QPalette::Midlight));
        m_separators->setFrameColor(palette().color(isTimelineRelevant() ? QPalette::Shadow : QPalette::Midlight));
    }

    return base_type::eventFilter(watched, event);
}

bool QnNavigationItem::at_speedSlider_wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (event->delta() > 0)
        m_stepForwardButton->click();
    else if (event->delta() < 0)
        m_stepBackwardButton->click();
    else
        return false;

    return true;
}

void QnNavigationItem::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    base_type::wheelEvent(event);
    event->accept(); /* Don't let wheel events escape into the scene. */
}

void QnNavigationItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mousePressEvent(event);
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnNavigationItem::at_liveButton_clicked()
{
    /* Reset speed. It MUST be done before setLive(true) is called. */
    navigator()->setSpeed(1.0);
    navigator()->setLive(true);
    action(action::PlayPauseAction)->setChecked(true);

    /* Move time scrollbar so that maximum is visible. */
    m_timeSlider->finishAnimations();
    m_timeScrollBar->setValue(m_timeScrollBar->maximum());

    /* Reset button's checked state. */
    if (!m_liveButton->isChecked())
        m_liveButton->setChecked(true); /* Cannot go out of live mode by pressing 'live' button. */
}

void QnNavigationItem::at_syncButton_clicked()
{
    const auto streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();

    if (m_syncButton->isChecked())
        streamSynchronizer->setState(navigator()->currentWidget());
    else
        streamSynchronizer->setState(nullptr);
}

void QnNavigationItem::at_stepBackwardButton_clicked()
{
    if (!m_stepBackwardButton->isEnabled())
        return;

    if (m_playButton->isChecked())
    {
        m_speedSlider->speedDown();
        if (qFuzzyIsNull(m_speedSlider->speed()))
            m_speedSlider->speedDown(); /* Skip 'pause'. */
    }
    else
    {
        navigator()->stepBackward();
    }
}

void QnNavigationItem::at_stepForwardButton_clicked()
{
    if (!m_stepForwardButton->isEnabled())
        return;

    if (m_playButton->isChecked())
    {
        m_speedSlider->speedUp();
        if (qFuzzyIsNull(m_speedSlider->speed()))
            m_speedSlider->speedUp(); /* Skip 'pause'. */
    }
    else
    {
        navigator()->stepForward();
    }
}

QnImageButtonWidget *QnNavigationItem::newActionButton(action::IDType id)
{
    const auto statAlias = lit("%1_%2").arg(lit("navigation_item"), QnLexical::serialized(id));
    QnImageButtonWidget *button = new QnImageButtonWidget();
    button->setDefaultAction(action(id));
    context()->statisticsModule()->registerButton(statAlias, button);
    return button;
}

bool QnNavigationItem::isTimelineRelevant() const
{
    return this->navigator() && this->navigator()->isTimelineRelevant();
}

void QnNavigationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    /* Time slider and time scrollbar has own backgrounds.
     * Subtract them for proper semi-transparency: */
    QRegion clipRegion(rect().toRect());
    if (m_timeSlider->isVisible())
        clipRegion -= m_timeSlider->geometry().toRect();
    if (m_timeScrollBar->isVisible())
        clipRegion -= m_timeScrollBar->geometry().toRect();

    QnScopedPainterClipRegionRollback clipRollback(painter, clipRegion);
    base_type::paint(painter, option, widget);
}
