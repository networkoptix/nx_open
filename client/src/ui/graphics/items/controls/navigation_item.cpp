#include "navigation_item.h"

#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QAction>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QGraphicsProxyWidget>

#include <nx/streaming/abstract_archive_stream_reader.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/synctime.h>

#include <ui/common/palette.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench.h>
#include <ui/graphics/items/controls/speed_slider.h>
#include <ui/graphics/items/controls/volume_slider.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

#include <core/resource/resource.h>

#include <utils/common/model_functions.h>

#include "time_slider.h"
#include "time_scroll_bar.h"
#include "clock_label.h"

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

    setAutoFillBackground(true);

    setPaletteColor(this, QPalette::Window, Qt::red);

    /* Create buttons. */
    m_jumpBackwardButton = newActionButton(QnActions::JumpToStartAction);
    m_jumpBackwardButton->setIcon(qnSkin->icon("slider/navigation/rewind_backward.png"));
    m_jumpBackwardButton->setPreferredSize(32, 32);

    m_stepBackwardButton = newActionButton(QnActions::PreviousFrameAction);
    m_stepBackwardButton->setIcon(qnSkin->icon("slider/navigation/step_backward.png"));
    m_stepBackwardButton->setPreferredSize(32, 32);

    m_playButton = newActionButton(QnActions::PlayPauseAction);
    m_playButton->setIcon(qnSkin->icon("slider/navigation/play.png", "slider/navigation/pause.png"));
    m_playButton->setPreferredSize(32, 32);

    m_stepForwardButton = newActionButton(QnActions::NextFrameAction);
    m_stepForwardButton->setIcon(qnSkin->icon("slider/navigation/step_forward.png"));
    m_stepForwardButton->setPreferredSize(32, 32);

    m_jumpForwardButton = newActionButton(QnActions::JumpToEndAction);
    m_jumpForwardButton->setIcon(qnSkin->icon("slider/navigation/rewind_forward.png"));
    m_jumpForwardButton->setPreferredSize(32, 32);

    m_muteButton = newActionButton(QnActions::ToggleMuteAction);
    m_muteButton->setIcon(qnSkin->icon("slider/buttons/unmute.png", "slider/buttons/mute.png"));
    m_muteButton->setPreferredSize(20, 20);

    m_liveButton = newActionButton(QnActions::JumpToLiveAction);
    m_liveButton->setIcon(qnSkin->icon("slider/buttons/live.png"));
    m_liveButton->setPreferredSize(48, 24);

    m_syncButton = newActionButton(QnActions::ToggleSyncAction);
    m_syncButton->setIcon(qnSkin->icon("slider/buttons/sync.png"));
    m_syncButton->setPreferredSize(48, 24);

    m_bookmarksModeButton = newActionButton(QnActions::BookmarksModeAction);
    m_bookmarksModeButton->setIcon(qnSkin->icon("slider/buttons/bookmarks.png"));
    m_bookmarksModeButton->setPreferredSize(48, 24);

    m_calendarButton = newActionButton(QnActions::ToggleCalendarAction);
    m_calendarButton->setIcon(qnSkin->icon("slider/buttons/calendar.png"));
    m_calendarButton->setPreferredSize(48, 24);


    /* Create sliders. */
    m_speedSlider = new QnSpeedSlider(this);
    m_speedSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_speedSlider->installEventFilter(this);

    m_volumeSlider = new QnVolumeSlider(this);
    m_volumeSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_volumeSlider->toolTipItem()->setParentItem(parent);

    m_timeSlider = new QnTimeSlider(this, parent);
    m_timeSlider->setOption(QnTimeSlider::UnzoomOnDoubleClick, false);
    m_timeSlider->setRulerHeight(70.0);

    m_timeScrollBar = new QnTimeScrollBar(this);

    /* Initialize navigator. */
    navigator()->setTimeSlider(m_timeSlider);
    navigator()->setTimeScrollBar(m_timeScrollBar);

    /* Put it all into layouts. */
    QGraphicsLinearLayout *buttonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    buttonsLayout->setSpacing(2);
    buttonsLayout->addItem(m_jumpBackwardButton);
    buttonsLayout->setAlignment(m_jumpBackwardButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_stepBackwardButton);
    buttonsLayout->setAlignment(m_stepBackwardButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_playButton);
    buttonsLayout->setAlignment(m_playButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_stepForwardButton);
    buttonsLayout->setAlignment(m_stepForwardButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_jumpForwardButton);
    buttonsLayout->setAlignment(m_jumpForwardButton, Qt::AlignCenter);

    QnClockLabel *clockLabel = new QnClockLabel(this);
    clockLabel->setAlignment(Qt::AlignCenter);

    QGraphicsLinearLayout *leftLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    leftLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    leftLayoutV->setContentsMargins(0, 3, 0, 0);
    leftLayoutV->setSpacing(0);
    leftLayoutV->setMinimumHeight(87.0);
    leftLayoutV->addItem(m_speedSlider);
    leftLayoutV->addItem(buttonsLayout);
    leftLayoutV->addItem(clockLabel);

    GraphicsWidget *leftWidget = new GraphicsWidget();
    leftWidget->setLayout(leftLayoutV);

    QGraphicsLinearLayout *rightLayoutHU = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutHU->setContentsMargins(0, 0, 0, 0);
    rightLayoutHU->setSpacing(3);
    rightLayoutHU->addItem(m_muteButton);
    rightLayoutHU->addItem(m_volumeSlider);

    QGraphicsLinearLayout *rightLayoutHM = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutHM->setContentsMargins(0, 0, 0, 0);
    rightLayoutHM->setSpacing(3);
    rightLayoutHM->addItem(m_liveButton);
    rightLayoutHM->addItem(m_syncButton);

    QGraphicsLinearLayout *rightLayoutHL = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutHL->setContentsMargins(0, 0, 0, 0);
    rightLayoutHL->setSpacing(3);
    rightLayoutHL->addItem(m_bookmarksModeButton);
    rightLayoutHL->addItem(m_calendarButton);

    QGraphicsLinearLayout *rightLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    rightLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    rightLayoutV->setContentsMargins(0, 3, 0, 3);
    rightLayoutV->setSpacing(0);
    rightLayoutV->setMinimumHeight(87.0);
    rightLayoutV->addItem(rightLayoutHU);
    rightLayoutV->addItem(rightLayoutHM);
    rightLayoutV->addItem(rightLayoutHL);

    QGraphicsLinearLayout *sliderLayout = new QGraphicsLinearLayout(Qt::Vertical);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    sliderLayout->setSpacing(0);
    sliderLayout->addItem(m_timeSlider);
    sliderLayout->setStretchFactor(m_timeSlider, 0x1000);
    sliderLayout->addItem(m_timeScrollBar);

    QGraphicsLinearLayout *leftLayoutVV = new QGraphicsLinearLayout(Qt::Vertical);
    leftLayoutVV->setContentsMargins(0, 0, 0, 0);
    leftLayoutVV->setSpacing(0);
    leftLayoutVV->addStretch(1);
    leftLayoutVV->addItem(leftWidget);

    QGraphicsLinearLayout *rightLayoutVV = new QGraphicsLinearLayout(Qt::Vertical);
    rightLayoutVV->setContentsMargins(0, 0, 0, 0);
    rightLayoutVV->setSpacing(0);
    rightLayoutVV->addStretch(1);
    rightLayoutVV->addItem(rightLayoutV);

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(5, 0, 5, 0);
    mainLayout->setSpacing(10);
    mainLayout->addItem(leftLayoutVV);
    mainLayout->addItem(sliderLayout);
    mainLayout->setStretchFactor(sliderLayout, 0x1000);
    mainLayout->addItem(rightLayoutVV);
    setLayout(mainLayout);


    /* Set up handlers. */
    QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
    connect(streamSynchronizer,             SIGNAL(runningChanged()),                   this,           SLOT(updateSyncButtonChecked()));
    connect(streamSynchronizer,             SIGNAL(effectiveChanged()),                 this,           SLOT(updateSyncButtonChecked()));
    connect(streamSynchronizer,             SIGNAL(effectiveChanged()),                 this,           SLOT(updateSyncButtonEnabled()));

    connect(m_speedSlider,                  SIGNAL(roundedSpeedChanged(qreal)),         this,           SLOT(updateNavigatorSpeedFromSpeedSlider()));
    connect(m_volumeSlider,                 SIGNAL(valueChanged(qint64)),               this,           SLOT(updateMuteButtonChecked()));

    connect(action(QnActions::PreviousFrameAction), SIGNAL(triggered()),                       this,           SLOT(at_stepBackwardButton_clicked()));
    connect(action(QnActions::NextFrameAction),    SIGNAL(triggered()),                        this,           SLOT(at_stepForwardButton_clicked()));
    connect(action(QnActions::JumpToStartAction),  SIGNAL(triggered()),                        navigator(),    SLOT(jumpBackward()));
    connect(action(QnActions::JumpToEndAction),    SIGNAL(triggered()),                        navigator(),    SLOT(jumpForward()));

    connect(action(QnActions::PlayPauseAction),    SIGNAL(toggled(bool)),                      navigator(),    SLOT(setPlaying(bool)));
    connect(action(QnActions::PlayPauseAction),    SIGNAL(toggled(bool)),                      this,           SLOT(updateSpeedSliderParametersFromNavigator()));
    connect(action(QnActions::PlayPauseAction),    SIGNAL(toggled(bool)),                      this,           SLOT(updatePlaybackButtonsIcons()));

    connect(action(QnActions::JumpToLiveAction),   SIGNAL(triggered()),                        this,           SLOT(at_liveButton_clicked()));
    connect(action(QnActions::ToggleSyncAction),   SIGNAL(triggered()),                        this,           SLOT(at_syncButton_clicked()));
    connect(action(QnActions::ToggleMuteAction),   SIGNAL(toggled(bool)),                      m_volumeSlider, SLOT(setMute(bool)));

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged,    this,  [this]()
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

    connect(navigator(),                    SIGNAL(currentWidgetAboutToBeChanged()),    m_speedSlider,  SLOT(finishAnimations()));
    connect(navigator(),                    SIGNAL(currentWidgetChanged()),             this,           SLOT(updateSyncButtonEnabled()));
    connect(navigator(),                    SIGNAL(currentWidgetChanged()),             this,           SLOT(updateJumpButtonsTooltips()));
    connect(navigator(),                    SIGNAL(currentWidgetChanged()),             this,           SLOT(updateBookButtonEnabled()));
    connect(navigator(),                    SIGNAL(speedRangeChanged()),                this,           SLOT(updateSpeedSliderParametersFromNavigator()));
    connect(navigator(),                    SIGNAL(liveChanged()),                      this,           SLOT(updateLiveButtonChecked()));
    connect(navigator(),                    SIGNAL(liveSupportedChanged()),             this,           SLOT(updateLiveButtonEnabled()));
    connect(navigator(),                    SIGNAL(liveChanged()),                      this,           SLOT(updatePlaybackButtonsEnabled()));
    connect(navigator(),                    SIGNAL(playingSupportedChanged()),          this,           SLOT(updatePlaybackButtonsEnabled()));
    connect(navigator(),                    SIGNAL(speedChanged()),                     this,           SLOT(updateSpeedSliderSpeedFromNavigator()));
    connect(navigator(),                    SIGNAL(speedRangeChanged()),                this,           SLOT(updateSpeedSliderParametersFromNavigator()));

    connect(navigator(), &QnWorkbenchNavigator::hasArchiveChanged, this, &QnNavigationItem::updatePlaybackButtonsEnabled);

    /* Play button is not synced with the actual playing state, so we update it only when current widget changes. */
    connect(navigator(),                    SIGNAL(currentWidgetChanged()),             this,           SLOT(updatePlayButtonChecked()), Qt::QueuedConnection);


    /* Register actions. */
    addAction(action(QnActions::PreviousFrameAction));
    addAction(action(QnActions::NextFrameAction));
    addAction(action(QnActions::PlayPauseAction));
    addAction(action(QnActions::JumpToStartAction));
    addAction(action(QnActions::JumpToEndAction));

    addAction(action(QnActions::VolumeDownAction));
    addAction(action(QnActions::VolumeUpAction));

    addAction(action(QnActions::JumpToLiveAction));
    addAction(action(QnActions::ToggleMuteAction));
    addAction(action(QnActions::ToggleSyncAction));
    addAction(action(QnActions::ToggleCalendarAction));

    connect(action(QnActions::VolumeUpAction),         SIGNAL(triggered()), m_volumeSlider,        SLOT(stepForward()));
    connect(action(QnActions::VolumeDownAction),       SIGNAL(triggered()), m_volumeSlider,        SLOT(stepBackward()));

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
    updateSyncButtonChecked();
    updateLiveButtonChecked();
    updateLiveButtonEnabled();
    updatePlaybackButtonsEnabled();

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
void QnNavigationItem::updateSpeedSliderParametersFromNavigator() {
    qreal minimalSpeed = navigator()->minimalSpeed();
    qreal maximalSpeed = navigator()->maximalSpeed();

    qreal speedStep, defaultSpeed;
    if(m_playButton->isChecked()) {
        speedStep = 1.0;
        defaultSpeed = 1.0;
    } else {
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

void QnNavigationItem::updateSpeedSliderSpeedFromNavigator() {
    if(m_updatingNavigatorFromSpeedSlider)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updatingSpeedSliderFromNavigator, true);
    m_speedSlider->setSpeed(navigator()->speed());
    updatePlaybackButtonsPressed();
    updatePlayButtonChecked();
}

void QnNavigationItem::updateNavigatorSpeedFromSpeedSlider() {
    if(m_updatingSpeedSliderFromNavigator)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updatingNavigatorFromSpeedSlider, true);
    navigator()->setSpeed(m_speedSlider->roundedSpeed());
    updatePlaybackButtonsPressed();
}

void QnNavigationItem::updatePlaybackButtonsPressed() {
    qreal speed = navigator()->speed();

    if(qFuzzyCompare(speed, 1.0) || qFuzzyIsNull(speed) || (speed > 0.0 && speed < 1.0)) {
        m_stepForwardButton->setPressed(false);
        m_stepBackwardButton->setPressed(false);
    } else if(speed > 1.0) {
        m_stepForwardButton->setPressed(true);
        m_stepBackwardButton->setPressed(false);
    } else if(speed < 0.0) {
        m_stepForwardButton->setPressed(false);
        m_stepBackwardButton->setPressed(true);
    }
}

void QnNavigationItem::updatePlaybackButtonsIcons() {
    bool playing = m_playButton->isChecked();

    // TODO: #Elric this is cheating!
    action(QnActions::PreviousFrameAction)->setText(playing ? tr("Speed Down") : tr("Previous Frame"));
    action(QnActions::NextFrameAction)->setText(playing ? tr("Speed Up") : tr("Next Frame"));

    m_stepBackwardButton->setIcon(playing
        ? qnSkin->icon("slider/navigation/backward.png")
        : qnSkin->icon("slider/navigation/step_backward.png"));
    m_stepForwardButton->setIcon(playing
        ? qnSkin->icon("slider/navigation/forward.png")
        : qnSkin->icon("slider/navigation/step_forward.png"));

    updatePlaybackButtonsEnabled(); // TODO: #Elric remove this once buttonwidget <-> action enabled sync is implemented. OR when we disable actions and not buttons.
}

void QnNavigationItem::updateJumpButtonsTooltips() {
    bool hasPeriods = navigator()->currentWidgetFlags() & QnWorkbenchNavigator::WidgetSupportsPeriods;

    // TODO: #Elric this is cheating!
    action(QnActions::JumpToStartAction)->setText(hasPeriods ? tr("Previuos Chunk") : tr("To Start"));
    action(QnActions::JumpToEndAction)->setText(hasPeriods ? tr("Next Chunk") : tr("To End"));

    updatePlaybackButtonsEnabled(); // TODO: #Elric remove this once buttonwidget <-> action enabled sync is implemented. OR when we disable actions and not buttons.
}

void QnNavigationItem::updateBookButtonEnabled()
{
    const auto currentWidget = navigator()->currentWidget();

    const bool motionSearchMode = (currentWidget
        && currentWidget->options().testFlag(QnResourceWidget::DisplayMotion));

    const bool bookmarksEnabled = (currentWidget
        && currentWidget->resource()->flags().testFlag(Qn::live));

    const auto layout = workbench()->currentLayout();
    const bool bookmarkMode = (bookmarksEnabled && !motionSearchMode
        && layout->data(Qn::LayoutBookmarksModeRole).toBool());

    const auto modeAction = action(QnActions::BookmarksModeAction);
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
    m_speedSlider->setEnabled(playable);
}

void QnNavigationItem::updateMuteButtonChecked() {
    m_muteButton->setChecked(m_volumeSlider->isMute());
}

void QnNavigationItem::updateLiveButtonChecked() {
    m_liveButton->setChecked(navigator()->isLive());

    /* This is needed as button's enabled state will be updated from its action. */ // TODO
    updateLiveButtonEnabled();
}

void QnNavigationItem::updateLiveButtonEnabled() {
    bool enabled = navigator()->isLiveSupported();

    m_liveButton->setEnabled(enabled);
}

void QnNavigationItem::updateSyncButtonChecked() {
    QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();

    m_syncButton->setChecked(streamSynchronizer->isEffective() && streamSynchronizer->isRunning());
}

void QnNavigationItem::updateSyncButtonEnabled() {
    QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();

    m_syncButton->setEnabled(streamSynchronizer->isEffective() && (navigator()->currentWidgetFlags() & QnWorkbenchNavigator::WidgetSupportsSync));
}

void QnNavigationItem::updatePlayButtonChecked() {
    m_playButton->setChecked(navigator()->isPlaying());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnNavigationItem::eventFilter(QObject *watched, QEvent *event) {
    if(watched == m_speedSlider && event->type() == QEvent::GraphicsSceneWheel)
        return at_speedSlider_wheelEvent(static_cast<QGraphicsSceneWheelEvent *>(event));

    return base_type::eventFilter(watched, event);
}

bool QnNavigationItem::at_speedSlider_wheelEvent(QGraphicsSceneWheelEvent *event) {
    if(event->delta() > 0) {
        m_stepForwardButton->click();
    } else if(event->delta() < 0) {
        m_stepBackwardButton->click();
    } else {
        return false;
    }

    return true;
}

void QnNavigationItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    base_type::wheelEvent(event);

    event->accept(); /* Don't let wheel events escape into the scene. */
}

void QnNavigationItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mousePressEvent(event);

    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnNavigationItem::at_liveButton_clicked() {
    /* Reset speed. It MUST be done before setLive(true) is called. */
    navigator()->setSpeed(1.0);

    navigator()->setLive(true);

    /* Move time scrollbar so that maximum is visible. */
    m_timeSlider->finishAnimations();
    m_timeScrollBar->setValue(m_timeScrollBar->maximum());

    /* Reset button's checked state. */
    if(!m_liveButton->isChecked())
        m_liveButton->setChecked(true); /* Cannot go out of live mode by pressing 'live' button. */
}

void QnNavigationItem::at_syncButton_clicked() {
    QnWorkbenchStreamSynchronizer *streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();

    if (m_syncButton->isChecked()) {
        streamSynchronizer->setState(navigator()->currentWidget());
    } else {
        streamSynchronizer->setState(NULL);
    }
}

void QnNavigationItem::at_stepBackwardButton_clicked() {
    if (!m_stepBackwardButton->isEnabled())
        return;

    if(m_playButton->isChecked()) {
        m_speedSlider->speedDown();
        if(qFuzzyIsNull(m_speedSlider->speed()))
            m_speedSlider->speedDown(); /* Skip 'pause'. */
    } else {
        navigator()->stepBackward();
    }
}

void QnNavigationItem::at_stepForwardButton_clicked() {
    if (!m_stepForwardButton->isEnabled())
        return;

    if(m_playButton->isChecked()) {
        m_speedSlider->speedUp();
        if(qFuzzyIsNull(m_speedSlider->speed()))
            m_speedSlider->speedUp(); /* Skip 'pause'. */
    } else {
        navigator()->stepForward();
    }
}

QnImageButtonWidget *QnNavigationItem::newActionButton(QnActions::IDType id)
{
    const auto statAlias = lit("%1_%2").arg(lit("navigation_item")
        , QnLexical::serialized(id));
    QnImageButtonWidget *button = new QnImageButtonWidget(statAlias);
    button->setDefaultAction(action(id));
    button->setCached(true);
    return button;
}
