#include "navigation_item.h"

#include <QEvent>
#include <QCoreApplication>
#include <QAction>
#include <QGraphicsLinearLayout>
#include <QMessageBox>

#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/video_server.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>

#include "camera/camera.h"

#include "ui/style/skin.h"
#include "ui/graphics/items/standard/graphics_label.h"
#include "ui/graphics/items/controls/speed_slider.h"
#include "ui/graphics/items/controls/volume_slider.h"
#include "ui/graphics/items/controls/tool_tip_item.h"
#include "ui/graphics/items/image_button_widget.h"

#include "utils/common/synctime.h"
#include "core/resource/security_cam_resource.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_navigator.h"
#include "ui/workbench/workbench_context.h"
#include "time_slider.h"
#include "time_scroll_bar.h"
#include "utils/common/scoped_value_rollback.h"

QnNavigationItem::QnNavigationItem(QGraphicsItem *parent, QnWorkbenchContext *context): 
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent->toGraphicsObject()),
    m_updatingSpeedSliderFromNavigator(false),
    m_updatingNavigatorFromSpeedSlider(false)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFocusPolicy(Qt::ClickFocus);

    setCursor(Qt::ArrowCursor);

    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::black);
        setPalette(pal);
    }


    /* Create buttons. */
    m_backwardButton = new QnImageButtonWidget(this);
    m_backwardButton->setIcon(qnSkin->icon("rewind_backward.png"));
    m_backwardButton->setPreferredSize(32, 18);
    m_backwardButton->setFocusProxy(this);

    m_stepBackwardButton = new QnImageButtonWidget(this);
    m_stepBackwardButton->setIcon(qnSkin->icon("step_backward.png"));
    m_stepBackwardButton->setPreferredSize(32, 18);
    m_stepBackwardButton->setFocusProxy(this);

    m_playButton = new QnImageButtonWidget(this);
    m_playButton->setCheckable(true);
    m_playButton->setIcon(qnSkin->icon("play.png", "pause.png"));
    m_playButton->setPreferredSize(32, 30);
    m_playButton->setFocusProxy(this);

    m_stepForwardButton = new QnImageButtonWidget(this);
    m_stepForwardButton->setIcon(qnSkin->icon("step_forward.png"));
    m_stepForwardButton->setPreferredSize(32, 18);
    m_stepForwardButton->setFocusProxy(this);

    m_forwardButton = new QnImageButtonWidget(this);
    m_forwardButton->setIcon(qnSkin->icon("rewind_forward.png"));
    m_forwardButton->setPreferredSize(32, 18);
    m_forwardButton->setFocusProxy(this);

    m_muteButton = new QnImageButtonWidget(this);
    m_muteButton->setIcon(qnSkin->icon("unmute.png", "mute.png"));
    m_muteButton->setPreferredSize(20, 20);
    m_muteButton->setCheckable(true);
    m_muteButton->setFocusProxy(this);

    m_liveButton = new QnImageButtonWidget(this);
    m_liveButton->setIcon(qnSkin->icon("live.png"));
    m_liveButton->setPreferredSize(48, 24);
    m_liveButton->setCheckable(true);
    m_liveButton->setFocusProxy(this);

    m_syncButton = new QnImageButtonWidget(this);
    m_syncButton->setIcon(qnSkin->icon("sync.png"));
    m_syncButton->setPreferredSize(48, 24);
    m_syncButton->setCheckable(true);
    m_syncButton->setFocusProxy(this);


    /* Time label. */
    m_timeLabel = new GraphicsLabel(this);
    m_timeLabel->setObjectName("TimeLabel");
    m_timeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::Label);
    m_timeLabel->setText(QLatin1String(" ")); /* So that it takes some space right away. */
    {
        QPalette pal = m_timeLabel->palette();
        pal.setColor(QPalette::Window, QColor(0, 0, 0, 0));
        pal.setColor(QPalette::WindowText, QColor(63, 159, 216));
        m_timeLabel->setPalette(pal);
    }


    /* Create sliders. */
    m_speedSlider = new QnSpeedSlider(this);
    m_speedSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_speedSlider->setFocusProxy(this);

    m_volumeSlider = new QnVolumeSlider(this);
    m_volumeSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_volumeSlider->setFocusProxy(this);

    m_timeSlider = new QnTimeSlider(this);
    m_timeSlider->setMinimumHeight(60.0);

    m_timeScrollBar = new QnTimeScrollBar(this);
    

    /* Create navigator. */
    m_navigator = new QnWorkbenchNavigator(this);
    m_navigator->setTimeSlider(m_timeSlider);
    m_navigator->setTimeScrollBar(m_timeScrollBar);


    /* Put it all into layouts. */
    QGraphicsLinearLayout *buttonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    buttonsLayout->setSpacing(2);
    buttonsLayout->addItem(m_backwardButton);
    buttonsLayout->setAlignment(m_backwardButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_stepBackwardButton);
    buttonsLayout->setAlignment(m_stepBackwardButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_playButton);
    buttonsLayout->setAlignment(m_playButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_stepForwardButton);
    buttonsLayout->setAlignment(m_stepForwardButton, Qt::AlignCenter);
    buttonsLayout->addItem(m_forwardButton);
    buttonsLayout->setAlignment(m_forwardButton, Qt::AlignCenter);

    QGraphicsLinearLayout *leftLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    leftLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    leftLayoutV->setContentsMargins(0, 0, 0, 0);
    leftLayoutV->setSpacing(0);
    leftLayoutV->addItem(m_speedSlider);
    leftLayoutV->addItem(buttonsLayout);

    QGraphicsLinearLayout *rightLayoutHU = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutHU->setContentsMargins(0, 0, 0, 0);
    rightLayoutHU->setSpacing(3);
    rightLayoutHU->addItem(m_liveButton);
    rightLayoutHU->addItem(m_syncButton);

    QGraphicsLinearLayout *rightLayoutHL = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutHL->setContentsMargins(0, 0, 0, 0);
    rightLayoutHL->setSpacing(3);
    rightLayoutHL->addItem(m_volumeSlider);
    rightLayoutHL->addItem(m_muteButton);

    QGraphicsLinearLayout *rightLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    rightLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    rightLayoutV->setContentsMargins(0, 3, 0, 0);
    rightLayoutV->setSpacing(0);
    rightLayoutV->addItem(rightLayoutHL);
    rightLayoutV->addItem(rightLayoutHU);
    rightLayoutV->setAlignment(rightLayoutHU, Qt::AlignRight | Qt::AlignVCenter);
    rightLayoutV->addItem(m_timeLabel);

    QGraphicsLinearLayout *sliderLayout = new QGraphicsLinearLayout(Qt::Vertical);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    sliderLayout->setSpacing(0);
    sliderLayout->addItem(m_timeSlider);
    sliderLayout->setStretchFactor(m_timeSlider, 0x1000);
    sliderLayout->addItem(m_timeScrollBar);

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(5, 0, 5, 0);
    mainLayout->setSpacing(10);
    mainLayout->addItem(leftLayoutV);
    mainLayout->addItem(sliderLayout);
    mainLayout->addItem(rightLayoutV);
    setLayout(mainLayout);


    /* Set up handlers. */
    connect(display(),              SIGNAL(streamsSynchronizedChanged()),       this,           SLOT(updateButtonsSyncState()));
    connect(display(),              SIGNAL(streamsSynchronizationEffectiveChanged()), this,      SLOT(updateButtonsSyncEffectiveState()));

    connect(m_speedSlider,          SIGNAL(roundedSpeedChanged(qreal)),         this,           SLOT(updateNavigatorSpeedFromSpeedSlider()));
    connect(m_volumeSlider,         SIGNAL(valueChanged(qint64)),               this,           SLOT(updateButtonsMuteState()));

    connect(m_stepBackwardButton,   SIGNAL(clicked()),                          this,           SLOT(stepBackward()));
    connect(m_stepForwardButton,    SIGNAL(clicked()),                          this,           SLOT(stepForward()));
    connect(m_backwardButton,       SIGNAL(clicked()),                          this,           SLOT(rewindBackward()));
    connect(m_forwardButton,        SIGNAL(clicked()),                          this,           SLOT(rewindForward()));
    
    connect(m_playButton,           SIGNAL(toggled(bool)),                      m_navigator,    SLOT(setPlaying(bool)));
    connect(m_playButton,           SIGNAL(toggled(bool)),                      this,           SLOT(updateSpeedSliderParametersFromNavigator()));
    connect(m_playButton,           SIGNAL(toggled(bool)),                      this,           SLOT(updateButtonsPlayingState()));

    connect(m_liveButton,           SIGNAL(clicked()),                          this,           SLOT(at_liveButton_clicked()));
    connect(m_syncButton,           SIGNAL(clicked()),                          this,           SLOT(at_syncButton_clicked()));
    connect(m_muteButton,           SIGNAL(clicked(bool)),                      m_volumeSlider, SLOT(setMute(bool)));
    
    connect(m_navigator,            SIGNAL(currentWidgetAboutToBeChanged()),    this,           SLOT(at_navigator_currentWidgetAboutToBeChanged()));
    connect(m_navigator,            SIGNAL(currentWidgetChanged()),             this,           SLOT(at_navigator_currentWidgetChanged()));
    connect(m_navigator,            SIGNAL(speedRangeChanged()),                this,           SLOT(updateSpeedSliderParametersFromNavigator()));
    connect(m_navigator,            SIGNAL(liveChanged()),                      this,           SLOT(updateButtonsLiveState()));
    connect(m_navigator,            SIGNAL(liveSupportedChanged()),             this,           SLOT(updateButtonsLiveSupportedState()));
    connect(m_navigator,            SIGNAL(playingSupportedChanged()),          this,           SLOT(updateButtonsPlayingSupportedState()));
    connect(m_navigator,            SIGNAL(speedChanged()),                     this,           SLOT(updateSpeedSliderSpeedFromNavigator()));
    connect(m_navigator,            SIGNAL(speedRangeChanged()),                this,           SLOT(updateSpeedSliderParametersFromNavigator()));


    /* Create actions. */
    QAction *playAction = new QAction(tr("Play / Pause"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    addAction(playAction);

    QAction *speedDownAction = new QAction(tr("Speed Down"), m_speedSlider);
    speedDownAction->setShortcut(tr("Ctrl+-"));
    speedDownAction->setShortcutContext(Qt::ApplicationShortcut);
    //connect(speedDownAction, SIGNAL(triggered()), m_speedSlider, SLOT(stepBackward())); // TODO
    addAction(speedDownAction);

    QAction *speedUpAction = new QAction(tr("Speed Up"), m_speedSlider);
    speedUpAction->setShortcut(tr("Ctrl++"));
    speedUpAction->setShortcutContext(Qt::ApplicationShortcut);
    //connect(speedUpAction, SIGNAL(triggered()), m_speedSlider, SLOT(stepForward())); // TODO
    addAction(speedUpAction);

    QAction *prevFrameAction = new QAction(tr("Previous Frame"), m_stepBackwardButton);
    prevFrameAction->setShortcut(tr("Ctrl+Left"));
    prevFrameAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(prevFrameAction, SIGNAL(triggered()), m_stepBackwardButton, SLOT(click()));
    addAction(prevFrameAction);

    QAction *nextFrameAction = new QAction(tr("Next Frame"), m_stepForwardButton);
    nextFrameAction->setShortcut(tr("Ctrl+Right"));
    nextFrameAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(nextFrameAction, SIGNAL(triggered()), m_stepForwardButton, SLOT(click()));
    addAction(nextFrameAction);

    QAction *toStartAction = new QAction(tr("To Start"), m_backwardButton);
    toStartAction->setShortcut(tr("Z"));
    toStartAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(toStartAction, SIGNAL(triggered()), m_backwardButton, SLOT(click()));
    addAction(toStartAction);

    QAction *toEndAction = new QAction(tr("To End"), m_forwardButton);
    toEndAction->setShortcut(tr("X"));
    toEndAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(toEndAction, SIGNAL(triggered()), m_forwardButton, SLOT(click()));
    addAction(toEndAction);

    QAction *volumeDownAction = new QAction(tr("Volume Down"), m_volumeSlider);
    volumeDownAction->setShortcut(tr("Ctrl+Down"));
    volumeDownAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(volumeDownAction, SIGNAL(triggered()), m_volumeSlider, SLOT(stepBackward()));
    addAction(volumeDownAction);

    QAction *volumeUpAction = new QAction(tr("Volume Up"), m_volumeSlider);
    volumeUpAction->setShortcut(tr("Ctrl+Up"));
    volumeUpAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(volumeUpAction, SIGNAL(triggered()), m_volumeSlider, SLOT(stepForward()));
    addAction(volumeUpAction);

    QAction *toggleMuteAction = new QAction(tr("Toggle Mute"), m_muteButton);
    toggleMuteAction->setShortcut(tr("M"));
    toggleMuteAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(toggleMuteAction, SIGNAL(triggered()), m_muteButton, SLOT(click()));
    addAction(toggleMuteAction);

    QAction *liveAction = new QAction(tr("Go To Live"), m_liveButton);
    liveAction->setShortcut(tr("L"));
    liveAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(liveAction, SIGNAL(triggered()), m_liveButton, SLOT(click()));
    addAction(liveAction);

    QAction *toggleSyncAction = new QAction(tr("Toggle Sync"), m_syncButton);
    toggleSyncAction->setShortcut(tr("S"));
    toggleSyncAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(toggleSyncAction, SIGNAL(triggered()), m_syncButton, SLOT(click()));
    addAction(toggleSyncAction);


    /* Run handlers */
    updateButtonsMuteState();
    updateButtonsSyncState();
    updateButtonsLiveState();
    updateButtonsLiveSupportedState();
    updateButtonsPlayingSupportedState();
    
    updateButtonsPlayingState();
    updateSpeedSliderParametersFromNavigator();

    updateGeometry();
}

QnNavigationItem::~QnNavigationItem()
{
}

#if 0
void NavigationItem::setActualCamera(CLVideoCamera *camera)
{
    if (m_camera == camera)
        return;

    //m_timeSlider->resetSelectionRange();

    bool zoomResetNeeded = !(m_reserveCameras.contains(camera) && m_reserveCameras.contains(m_camera) && m_syncButton->isChecked());

    if (m_camera) {
        disconnect(m_camera->getCamDisplay(), 0, this, 0);
        
        //m_zoomByCamera[m_camera] = m_timeSlider->scalingFactor();
    }

    m_camera = camera;

    if (m_camera)
    {
        // Use QueuedConnection connection because of deadlock. Signal can came from before jump and speed changing can call pauseMedia (because of speed slider animation).
        connect(m_camera->getCamDisplay(), SIGNAL(liveMode(bool)), this, SLOT(onLiveModeChanged(bool)), Qt::QueuedConnection);

        QnAbstractArchiveReader *reader = dynamic_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());


        if (reader) {
            setPlaying(!reader->isMediaPaused());
            //m_timeSlider->setLiveMode(reader->isRealTimeSource());
            m_speedSlider->setSpeed(reader->getSpeed());
        }
        else
            setPlaying(true);
        if (!m_syncButton->isChecked()) {
            updateRecPeriodList(true);
            repaintMotionPeriods();
        }

    }
    else
    {
        //m_timeSlider->setScalingFactor(0);
    }

    if(zoomResetNeeded) {
        updateSlider();

        //m_timeSlider->setScalingFactor(m_zoomByCamera.value(m_camera, 0.0));
    }

    m_syncButton->setEnabled(m_reserveCameras.contains(camera));
    m_liveButton->setEnabled(m_reserveCameras.contains(camera));

    emit actualCameraChanged(m_camera);
}
#endif

void QnNavigationItem::rewindBackward()
{
#if 0
    if (m_camera == NULL)
        return;

    setActive(true);

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    qint64 pos = 0;
    if(m_reserveCameras.contains(m_camera))
    {
        const QnTimePeriodList &fullPeriods = m_timeSlider->fullMotionTimePeriodList().empty() ? m_timeSlider->fullRecTimePeriodList() : m_timeSlider->fullMotionTimePeriodList();
        const QnTimePeriodList periods = QnTimePeriod::aggregateTimePeriods(fullPeriods, MAX_FRAME_DURATION);
        if (!periods.isEmpty())
        {
            if (m_camera->getCurrentTime() == DATETIME_NOW) {
                pos = periods.last().startTimeMs*1000;
            }
            else {
                QnTimePeriodList::const_iterator itr = qUpperBound(periods.begin(), periods.end(), m_camera->getCurrentTime()/1000);
                itr = qMax(itr-2, periods.begin());
                pos = itr->startTimeMs*1000ll;
                if (reader->isReverseMode() && itr->durationMs != -1)
                    pos += itr->durationMs*1000ll;
            }
        }
    }
    reader->jumpTo(pos, 0);
#endif
}

void QnNavigationItem::rewindForward()
{
#if 0
    if (m_camera == NULL)
        return;

    setActive(true);
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    qint64 pos = reader->endTime();
    if(m_reserveCameras.contains(m_camera)) 
    {
        const QnTimePeriodList &fullPeriods = m_timeSlider->fullMotionTimePeriodList().empty() ? m_timeSlider->fullRecTimePeriodList() : m_timeSlider->fullMotionTimePeriodList();
        const QnTimePeriodList periods = QnTimePeriod::aggregateTimePeriods(fullPeriods, MAX_FRAME_DURATION);
        QnTimePeriodList::const_iterator itr = qUpperBound(periods.begin(), periods.end(), m_camera->getCurrentTime()/1000);
        if (itr == periods.end() || reader->isReverseMode() && itr->durationMs == -1)
            pos = DATETIME_NOW;
        else
            pos = (itr->startTimeMs + (reader->isReverseMode() ? itr->durationMs : 0)) * 1000ll;
    }
    reader->jumpTo(pos, 0);
#endif
}

void QnNavigationItem::stepBackward()
{
#if 0
    if (m_camera == NULL)
        return;

    setActive(true);

    if (m_playing)
    {
        m_speedSlider->stepBackward();
        return;
    }

    m_speedSlider->resetSpeed();

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    if (!reader->isSkippingFrames() && m_camera->getCurrentTime() != 0)
    {
        quint64 curr_time = m_camera->getCurrentTime();

        if (reader->isSingleShotMode())
            m_camera->getCamDisplay()->playAudio(false);

        reader->previousFrame(curr_time);
        //m_camera->streamJump(curr_time);
        //m_camera->getCamDisplay()->setSingleShotMode(false);
    }
#endif
}

void QnNavigationItem::stepForward()
{
#if 0
    if (m_camera == NULL)
        return;

    setActive(true);

    if (m_playing)
    {
        m_speedSlider->stepForward();
        return;
    }

    m_speedSlider->resetSpeed();

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->nextFrame();
    //m_camera->getCamDisplay()->setSingleShotMode(true);
#endif
}


// -------------------------------------------------------------------------- //
// Updaters
// -------------------------------------------------------------------------- //
void QnNavigationItem::updateButtonsSpeedState() {
    qreal speed = m_navigator->speed();

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

void QnNavigationItem::updateSpeedSliderParametersFromNavigator() {
    qreal minimalSpeed = m_navigator->minimalSpeed();
    qreal maximalSpeed = m_navigator->maximalSpeed();
    
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

    m_speedSlider->setSpeedRange(minimalSpeed, maximalSpeed);
    m_speedSlider->setMinimalSpeedStep(speedStep);
    m_speedSlider->setDefaultSpeed(defaultSpeed);
}

void QnNavigationItem::updateSpeedSliderSpeedFromNavigator() {
    if(m_updatingNavigatorFromSpeedSlider)
        return;

    QnScopedValueRollback<bool> guard(&m_updatingSpeedSliderFromNavigator, true);
    m_speedSlider->setSpeed(m_navigator->speed());
    updateButtonsSpeedState();
}

void QnNavigationItem::updateNavigatorSpeedFromSpeedSlider() {
    if(m_updatingSpeedSliderFromNavigator)
        return;

    QnScopedValueRollback<bool> guard(&m_updatingNavigatorFromSpeedSlider, true);
    m_navigator->setSpeed(m_speedSlider->roundedSpeed());
    updateButtonsSpeedState();
}

void QnNavigationItem::updateButtonsPlayingState() {
    bool playing = m_playButton->isChecked();

    m_stepBackwardButton->setIcon(qnSkin->icon(playing ? "backward.png" : "step_backward.png"));
    m_stepForwardButton->setIcon(qnSkin->icon(playing ? "forward.png" : "step_forward.png"));
}

void QnNavigationItem::updateButtonsPlayingSupportedState() {
    bool enabled = m_navigator->isPlayingSupported();

    m_playButton->setEnabled(enabled);
    m_stepBackwardButton->setEnabled(enabled);
    m_stepForwardButton->setEnabled(enabled);
    m_speedSlider->setEnabled(enabled);
}

void QnNavigationItem::updateButtonsMuteState() {
    m_muteButton->setChecked(m_volumeSlider->isMute());
}

void QnNavigationItem::updateButtonsLiveState() {
    m_liveButton->setChecked(m_navigator->isLive());
}

void QnNavigationItem::updateButtonsLiveSupportedState() {
    bool enabled = m_navigator->isLiveSupported();

    m_liveButton->setEnabled(enabled);
}

void QnNavigationItem::updateButtonsSyncState() {
    m_syncButton->setChecked(display()->isStreamsSynchronized());
}

void QnNavigationItem::updateButtonsSyncEffectiveState() {
    m_syncButton->setEnabled(display()->isStreamsSynchronizationEffective());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnNavigationItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    base_type::wheelEvent(event);
    
    event->accept(); /* Don't let wheel events escape into the scene. */
}

void QnNavigationItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mousePressEvent(event);

    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnNavigationItem::at_navigator_currentWidgetAboutToBeChanged() {
    m_speedSlider->finishAnimations();
}

void QnNavigationItem::at_navigator_currentWidgetChanged() {
    m_playButton->setChecked(m_navigator->isPlaying());
}

void QnNavigationItem::at_liveButton_clicked() {
    if(m_liveButton->isChecked()) {
        m_navigator->setLive(true);
    } else {
        m_liveButton->setChecked(true); /* Cannot go out of live mode by pressing 'live' button. */
    }
}

void QnNavigationItem::at_syncButton_clicked() {
    display()->setStreamsSynchronized(m_syncButton->isChecked());
}
