#include "navigationitem.h"

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
#include "ui/graphics/items/standard/graphicslabel.h"
#include "ui/graphics/items/controls/speedslider.h"
#include "ui/graphics/items/controls/volumeslider.h"
#include "ui/graphics/items/image_button_widget.h"
#include "ui/graphics/items/tool_tip_item.h"

#include "timeslider.h"
#include "utils/common/synctime.h"
#include "core/resource/security_cam_resource.h"

static const int SLIDER_NOW_AREA_WIDTH = 30;
static const int TIME_PERIOD_UPDATE_INTERVAL = 1000 * 10;

class SliderToolTipItem : public QnStyledToolTipItem
{
public:
    SliderToolTipItem(AbstractGraphicsSlider *slider, QGraphicsItem *parent = 0)
        : QnStyledToolTipItem(parent), m_slider(slider)
    {
        setAcceptHoverEvents(true);
        setOpacity(0.75);
    }

private:
    AbstractGraphicsSlider *const m_slider;
    QPointF m_pos;
};


class TimeSliderToolTipItem : public QnStyledToolTipItem
{
public:
    TimeSliderToolTipItem(TimeSlider *slider, QGraphicsItem *parent = 0)
        : QnStyledToolTipItem(parent), m_slider(slider)
    {
        setAcceptHoverEvents(true);
        setOpacity(0.75);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        m_pos = mapToItem(m_slider, event->pos());
        m_slider->setMoving(true);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {
        QPointF pos = mapToItem(m_slider, event->pos());
        qint64 shift = qreal(m_slider->sliderRange()) / m_slider->rect().width() * (pos - m_pos).x();
        m_slider->setCurrentValue(m_slider->currentValue() + shift);
        m_pos = pos;
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
    {
        Q_UNUSED(event)
        m_slider->setMoving(false);
    }

private:
    TimeSlider *const m_slider;
    QPointF m_pos;
};


NavigationItem::NavigationItem(QGraphicsItem *parent): 
    base_type(parent),
    m_camera(0), 
    m_forcedCamera(0), 
    m_currentTime(0),
    m_playing(false)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFocusPolicy(Qt::ClickFocus);
    //setCacheMode(QGraphicsItem::ItemCoordinateCache);

    setCursor(Qt::ArrowCursor);

    setAutoFillBackground(true);

    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::black);
        setPalette(pal);
    }


    connect(QnTimePeriodReaderHelper::instance(), SIGNAL(ready(const QnTimePeriodList&, int)), this, SLOT(onTimePeriodLoaded(const QnTimePeriodList&, int)));
    connect(QnTimePeriodReaderHelper::instance(), SIGNAL(failed(int, int)), this, SLOT(onTimePeriodLoadFailed(int, int)));

    m_backwardButton = new QnImageButtonWidget(this);
    m_backwardButton->setIcon(qnSkin->icon("rewind_backward.png"));
    m_backwardButton->setPreferredSize(32, 18);
    m_backwardButton->setFocusProxy(this);

    m_stepBackwardButton = new QnImageButtonWidget(this);
    m_stepBackwardButton->setIcon(qnSkin->icon("step_backward.png"));
    m_stepBackwardButton->setPreferredSize(32, 18);
    m_stepBackwardButton->setFocusProxy(this);

    m_playButton = new QnImageButtonWidget(this);
    m_playButton->setIcon(qnSkin->icon("play.png"));
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

    connect(m_backwardButton, SIGNAL(clicked()), this, SLOT(rewindBackward()));
    connect(m_stepBackwardButton, SIGNAL(clicked()), this, SLOT(stepBackward()));
    connect(m_playButton, SIGNAL(clicked()), this, SLOT(togglePlayPause()));
    connect(m_stepForwardButton, SIGNAL(clicked()), this, SLOT(stepForward()));
    connect(m_forwardButton, SIGNAL(clicked()), this, SLOT(rewindForward()));


    m_speedSlider = new SpeedSlider(Qt::Horizontal, this);
    m_speedSlider->setObjectName("SpeedSlider");
    m_speedSlider->setToolTipItem(new SliderToolTipItem(m_speedSlider));
    m_speedSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_speedSlider->setFocusProxy(this);

    connect(m_speedSlider, SIGNAL(speedChanged(float)), this, SLOT(onSpeedChanged(float)));
    connect(m_speedSlider, SIGNAL(frameBackward()), this, SLOT(stepBackward()));
    connect(m_speedSlider, SIGNAL(frameForward()), this, SLOT(stepForward()));


    m_timeSlider = new TimeSlider(this);
    m_timeSlider->setObjectName("TimeSlider");
    m_timeSlider->toolTipItem()->setVisible(false);
    m_timeSlider->setEndSize(SLIDER_NOW_AREA_WIDTH);
    m_timeSlider->setFlag(QGraphicsItem::ItemIsFocusable, true);
    m_timeSlider->setFocusProxy(this);
    
    QnToolTipItem *timeSliderToolTip = new TimeSliderToolTipItem(m_timeSlider);
    timeSliderToolTip->setFlag(QGraphicsItem::ItemIsFocusable, true);
    m_timeSlider->setToolTipItem(timeSliderToolTip);
    timeSliderToolTip->setFocusProxy(m_timeSlider);

    connect(m_timeSlider, SIGNAL(currentValueChanged(qint64)), this, SLOT(onValueChanged(qint64)));
    connect(m_timeSlider, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));
    connect(m_timeSlider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
    connect(m_timeSlider, SIGNAL(exportRange(qint64,qint64)), this, SLOT(onExportRange(qint64,qint64)));

    m_timerId = startTimer(33);

    m_muteButton = new QnImageButtonWidget(this);
    m_muteButton->setObjectName("MuteButton");
    m_muteButton->setIcon(qnSkin->icon("unmute.png", "mute.png"));
    m_muteButton->setPreferredSize(20, 20);
    m_muteButton->setCheckable(true);
    m_muteButton->setChecked(m_volumeSlider->isMute());
    m_muteButton->setFocusProxy(this);

    m_liveButton = new QnImageButtonWidget(this);
    m_liveButton->setObjectName("LiveButton");
    m_liveButton->setIcon(qnSkin->icon("live.png"));
    m_liveButton->setPreferredSize(48, 24);
    m_liveButton->setCheckable(true);
    m_liveButton->setChecked(m_camera && m_camera->getCamDisplay()->isRealTimeSource());
    m_liveButton->setEnabled(false);
    m_liveButton->setFocusProxy(this);

    connect(m_liveButton, SIGNAL(clicked(bool)), this, SLOT(at_liveButton_clicked(bool)));

    m_mrsButton = new QnImageButtonWidget(this);
    m_mrsButton->setObjectName("MRSButton");
    m_mrsButton->setIcon(qnSkin->icon("mrs.png"));
    m_mrsButton->setPreferredSize(48, 24);
    m_mrsButton->setCheckable(true);
    m_mrsButton->setChecked(true);
    m_mrsButton->hide();
    m_mrsButton->setFocusProxy(this);

    connect(m_mrsButton, SIGNAL(clicked()), this, SIGNAL(clearMotionSelection()));

    m_syncButton = new QnImageButtonWidget(this);
    m_syncButton->setObjectName("SyncButton");
    m_syncButton->setIcon(qnSkin->icon("sync.png"));

    m_syncButton->setPreferredSize(48, 24);
    m_syncButton->setCheckable(true);
    m_syncButton->setChecked(true);
    m_syncButton->setFocusProxy(this);
    m_syncButton->setEnabled(false);


    //connect(m_syncButton, SIGNAL(toggled(bool)), this, SIGNAL(enableItemSync(bool)));
    connect(m_syncButton, SIGNAL(toggled(bool)), this, SLOT(onSyncButtonToggled(bool)));

    m_volumeSlider = new VolumeSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName("VolumeSlider");
    m_volumeSlider->setToolTipItem(new SliderToolTipItem(m_volumeSlider));
    m_volumeSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_volumeSlider->setFocusProxy(this);

    connect(m_muteButton, SIGNAL(clicked(bool)), m_volumeSlider, SLOT(setMute(bool)));
    connect(m_volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(onVolumeLevelChanged(int)));

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

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(5, 0, 5, 0);
    mainLayout->setSpacing(10);
    mainLayout->addItem(leftLayoutV);
    mainLayout->addItem(m_timeSlider);
    mainLayout->addItem(rightLayoutV);
    setLayout(mainLayout);

    QAction *playAction = new QAction(tr("Play / Pause"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    addAction(playAction);

    QAction *speedDownAction = new QAction(tr("Speed Down"), m_speedSlider);
    speedDownAction->setShortcut(tr("Ctrl+-"));
    speedDownAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(speedDownAction, SIGNAL(triggered()), m_speedSlider, SLOT(stepBackward()));
    addAction(speedDownAction);

    QAction *speedUpAction = new QAction(tr("Speed Up"), m_speedSlider);
    speedUpAction->setShortcut(tr("Ctrl++"));
    speedUpAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(speedUpAction, SIGNAL(triggered()), m_speedSlider, SLOT(stepForward()));
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

    setVideoCamera(0);
    m_fullTimePeriodHandle = 0;
    m_timePeriodUpdateTime = 0;
    m_forceTimePeriodLoading = false;
    m_timePeriodLoadErrors = 0;

    updateGeometry();
}

NavigationItem::~NavigationItem()
{
}

void NavigationItem::setVideoCamera(CLVideoCamera *camera)
{
    if (m_forcedCamera == camera)
        return;

    m_forcedCamera = camera;

    updateActualCamera();
}

void NavigationItem::addReserveCamera(CLVideoCamera *camera)
{
    if (camera == NULL)
        return;

    m_reserveCameras.insert(camera);

    updateActualCamera();
    m_forceTimePeriodLoading = !updateRecPeriodList(true);
    //m_liveButton->setEnabled(true);
    //m_syncButton->setEnabled(true);
}

void NavigationItem::removeReserveCamera(CLVideoCamera *camera)
{
    m_reserveCameras.remove(camera);

    updateActualCamera();
    m_forceTimePeriodLoading = !updateRecPeriodList(true);
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(camera->getDevice());
    if (netRes)
        m_motionPeriodLoader.remove(netRes);
    repaintMotionPeriods();
    if(m_reserveCameras.empty()) {
        //m_liveButton->setEnabled(false);
        //m_syncButton->setEnabled(false);
    }
}

void NavigationItem::updateActualCamera()
{
    if (m_forcedCamera != NULL || !m_syncButton->isChecked()) {
        setActualCamera(m_forcedCamera);
        return;
    }

    if (m_reserveCameras.contains(m_camera))
        return;

    if (m_reserveCameras.empty()) {
        setActualCamera(NULL);
        return;
    }

    setActualCamera(*m_reserveCameras.begin());
}

void NavigationItem::setActualCamera(CLVideoCamera *camera)
{
    if (m_camera == camera)
        return;

    m_timeSlider->resetSelectionRange();

    bool zoomResetNeeded = !(m_reserveCameras.contains(camera) && m_reserveCameras.contains(m_camera) && m_syncButton->isChecked());

    if (m_camera) {
        disconnect(m_camera->getCamDisplay(), 0, this, 0);
        
        m_zoomByCamera[m_camera] = m_timeSlider->scalingFactor();
    }

    m_camera = camera;

    if (m_camera)
    {
        // Use QueuedConnection connection because of deadlock. Signal can came from before jump and speed changing can call pauseMedia (because of speed slider animation).
        connect(m_camera->getCamDisplay(), SIGNAL(liveMode(bool)), this, SLOT(onLiveModeChanged(bool)), Qt::QueuedConnection);

        QnAbstractArchiveReader *reader = dynamic_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());


        if (reader) {
            setPlaying(!reader->isMediaPaused());
            m_timeSlider->setLiveMode(reader->isRealTimeSource());
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
        m_timeSlider->setScalingFactor(0);
    }

    if(zoomResetNeeded) {
        updateSlider();

        m_timeSlider->setScalingFactor(m_zoomByCamera.value(m_camera, 0.0));
    }

    m_syncButton->setEnabled(m_reserveCameras.contains(camera));
    m_liveButton->setEnabled(m_reserveCameras.contains(camera));

    emit actualCameraChanged(m_camera);
}

void NavigationItem::onLiveModeChanged(bool value)
{
    m_timeSlider->setLiveMode(value);
    if (value)
        m_speedSlider->resetSpeed();
}

void NavigationItem::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
        updateSlider();

    base_type::timerEvent(event);
}

void NavigationItem::updateSlider()
{
    if (!m_camera)
        return;

    if (m_timeSlider->isMoving())
        return;

    QnAbstractArchiveReader *reader = dynamic_cast<QnAbstractArchiveReader *>(m_camera->getStreamreader());
    if (!reader)
        return;

    qint64 startTime = reader->startTime();
    qint64 endTime = reader->endTime();
    if (startTime != AV_NOPTS_VALUE && endTime != AV_NOPTS_VALUE)
    {
        m_timeSlider->setMinimumValue(startTime != DATETIME_NOW ? startTime / 1000 : qnSyncTime->currentMSecsSinceEpoch() - 10000); // if  nothing is recorded set minvalue to live-10sec
        m_timeSlider->setMaximumValue(endTime != DATETIME_NOW ? endTime / 1000 : DATETIME_NOW);
        if (m_timeSlider->minimumValue() == 0)
            m_timeLabel->setText(formatDuration(m_timeSlider->length() / 1000));
        else
            m_timeLabel->setText(QLatin1String(" ")); //(QDateTime::fromMSecsSinceEpoch(m_timeSlider->maximumValue()).toString(Qt::SystemLocaleShortDate));
        m_timeLabel->setVisible(!m_camera->getCamDisplay()->isRealTimeSource());

        quint64 time = m_camera->getCurrentTime();
        if (time != AV_NOPTS_VALUE)
        {
            m_currentTime = time != DATETIME_NOW ? time/1000 : time;
            m_timeSlider->setCurrentValue(m_currentTime, true);
        }

        m_forceTimePeriodLoading = !updateRecPeriodList(m_forceTimePeriodLoading); // if period does not loaded yet, force loading
    }

    if(!reader->isMediaPaused() && (m_camera->getCamDisplay()->isRealTimeSource() || m_timeSlider->currentValue() == DATETIME_NOW)) {
        m_liveButton->setChecked(true);
        m_forwardButton->setEnabled(false);
        m_stepForwardButton->setEnabled(false);
    } else {
        m_liveButton->setChecked(false);
        m_forwardButton->setEnabled(true);
        m_stepForwardButton->setEnabled(true);
    }
}

void NavigationItem::updateMotionPeriods(const QnTimePeriod& period)
{
    m_motionPeriod = period;
    foreach(CLVideoCamera *camera, m_reserveCameras) {
        QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(camera->getDevice());
        if (netRes)
        {
            MotionPeriods::iterator itr = m_motionPeriodLoader.find(netRes);
            if (itr != m_motionPeriodLoader.end())
            {
                MotionPeriodLoader& loader = itr.value();
                if (!loader.isMotionEmpty())
                    itr.value().loadingHandle = itr.value().loader->load(period, itr.value().regions);
            }
        }
    }
}

NavigationItem::MotionPeriodLoader* NavigationItem::getMotionLoader(QnAbstractArchiveReader* reader)
{
    QnResourcePtr resource = reader->getResource();
    if (!m_motionPeriodLoader.contains(resource))
    {
        MotionPeriodLoader p;
        p.reader = reader;
        p.loader = QnTimePeriodReaderHelper::instance()->createUpdater(resource);
#ifndef DEBUG_MOTION
        if (!p.loader) {
            qWarning() << "Connection to a video server lost. Can't load motion info";
            return 0;
        }
#endif
        connect(p.loader.data(), SIGNAL(ready(const QnTimePeriodList&, int)), this, SLOT(onMotionPeriodLoaded(const QnTimePeriodList&, int)));
        connect(p.loader.data(), SIGNAL(failed(int, int)), this, SLOT(onMotionPeriodLoadFailed(int, int)));
        m_motionPeriodLoader.insert(resource, p);
    }
    return &m_motionPeriodLoader[resource];
}

QnTimePeriodList generateTestPeriods(const QnTimePeriod& interval)
{
    QnTimePeriodList result;
    qint64 curTime = interval.startTimeMs;
    qint64 endTime = interval.startTimeMs + interval.durationMs;
    while (curTime < endTime)
    {
        qint64 dur = ((rand() % 95)+5) * 1000;
        result << QnTimePeriod(curTime, qMin(endTime - curTime, dur));
        curTime += dur;
        curTime += ((rand() % 95)+5) * 1000;
    }

    return result;
}

void NavigationItem::loadMotionPeriods(QnResourcePtr resource, QnAbstractArchiveReader* reader, QList<QRegion> regions)
{
#ifndef DEBUG_MOTION
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!netRes)
        return;
#endif

    MotionPeriodLoader* p = getMotionLoader(reader);
    if (!p)
        return;
    p->regions = regions;

    bool regionsEmpty = true;
    for (int i = 0; i < regions.size(); ++i)
        regionsEmpty &= regions[i].isEmpty();

    if (regionsEmpty)
    {
        repaintMotionPeriods();
        return;
    }

    qint64 w = m_timeSlider->sliderRange();
    qint64 t = m_timeSlider->viewPortPos();
#ifndef DEBUG_MOTION
    if (t <= 0)
        return;  // slider range still not initialized yet
#endif
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    QnTimePeriod loadingPeriod;
    qint64 minTimeMs = reader ? reader->startTime()/1000 : 0;
    loadingPeriod.startTimeMs = qMax(minTimeMs, t - w);
    loadingPeriod.durationMs = qMin(currentTime+1000 - m_timePeriod.startTimeMs, w * 3);

    m_motionPeriod = m_motionPeriod.intersect(loadingPeriod);


#ifdef DEBUG_MOTION
    // for motion periods testing on local files
    p->periods = generateTestPeriods(loadingPeriod);
    repaintMotionPeriods();
    return;
#endif

    p->loadingHandle = p->loader->load(loadingPeriod, regions);
}

bool NavigationItem::updateRecPeriodList(bool force)
{
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    if (!force && currentTime - m_timePeriodUpdateTime < TIME_PERIOD_UPDATE_INTERVAL)
        return true;

    qint64 w = m_timeSlider->sliderRange();
    qint64 t = m_timeSlider->viewPortPos();
    if (t <= 0)
        return false;  // slider range does not initialized now

    m_timePeriodUpdateTime = currentTime;

    //if (!force && m_timePeriod.startTimeMs <= t && t + w <= m_timePeriod.startTimeMs + m_timePeriod.durationMs)
    //    return true;

    if (!m_camera)
        return false;

    QnAbstractArchiveReader* reader = dynamic_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    QnNetworkResourceList resources;
    if (m_syncButton->isChecked())
    {
        foreach(CLVideoCamera *camera, m_reserveCameras) 
        {
            QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(camera->getDevice());
            if (networkResource)
            {
                if (camera->isVisible())
                    resources.push_back(networkResource);
            }
        }
    }
    else {
        QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(reader->getResource());
        if (networkResource)
            resources.push_back(networkResource);
    }

    /* Request interval no shorter than 1 hour. */
    const qint64 oneHour = 60ll * 60ll * 1000ll;
    if (w < oneHour)
        w = oneHour;

    /* It is important to update the stored interval BEFORE sending request to
     * the server. Request is blocking and starts an event loop, so we may get
     * into this method again. */
    if (reader->startTime() == AV_NOPTS_VALUE)
        return false;  // reader is not initialized yet


    qint64 minTimeMs = reader ? reader->startTime()/1000 : 0;
    m_timePeriod.startTimeMs = qMax(minTimeMs, t - w);
    m_timePeriod.durationMs = qMin(currentTime+1000 - m_timePeriod.startTimeMs, w * 3);
    // round time
    qint64 endTimeMs = m_timePeriod.startTimeMs + m_timePeriod.durationMs;
    endTimeMs = roundUp((quint64) endTimeMs, TIME_PERIOD_UPDATE_INTERVAL);
    m_timePeriod.durationMs = endTimeMs - m_timePeriod.startTimeMs;

    if (!resources.isEmpty())
    {
        bool timePeriodAlreadyLoaded = !force && m_timePeriod.startTimeMs <= t && t + w <= m_timePeriod.startTimeMs + m_timePeriod.durationMs;
        if (!timePeriodAlreadyLoaded)
            m_fullTimePeriodHandle = QnTimePeriodReaderHelper::instance()->load(resources, m_timePeriod);
        bool motionPeriodAlreadyLoaded = m_motionPeriod.startTimeMs <= t && t + w <= m_motionPeriod.startTimeMs + m_motionPeriod.durationMs;
        if (!motionPeriodAlreadyLoaded)
            updateMotionPeriods(m_timePeriod);
    }
    return true;
}

void NavigationItem::onMotionPeriodLoadFailed(int status, int handle)
{

}

void NavigationItem::onTimePeriodLoadFailed(int status, int handle)
{
    if (handle == m_fullTimePeriodHandle)
    {
        m_timePeriodLoadErrors++;
        if (m_timePeriodLoadErrors < 2)
            m_timePeriodUpdateTime = 0;
        else if (m_timePeriodLoadErrors == 2)
        {
            QnTimePeriodList periods = m_timeSlider->fullRecTimePeriodList();
            if (!periods.isEmpty() && periods.last().durationMs == -1)
            {
                qint64 diff = qnSyncTime->currentMSecsSinceEpoch() - periods.last().startTimeMs;
                periods.last().durationMs = qMax(0ll, diff);
                m_timeSlider->setRecTimePeriodList(periods);
            }
        }
    }
}

void NavigationItem::onTimePeriodLoaded(const QnTimePeriodList& timePeriods, int handle)
{
    if (handle == m_fullTimePeriodHandle) {
        m_timePeriodLoadErrors = 0;
        m_timeSlider->setRecTimePeriodList(timePeriods);
    }
}

CLVideoCamera* NavigationItem::findCameraByResource(QnResourcePtr resource)
{
    foreach(CLVideoCamera* camera, m_reserveCameras)
    {
        if (camera->getDevice() == resource)
            return camera;
    }
    return 0;
}

void NavigationItem::onDisplayingStateChanged(QnResourcePtr resource, bool visible)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!netRes)
        return;

    CLVideoCamera *camera = findCameraByResource(resource);
    if (camera && camera->isVisible() != visible)
    {
        camera->setVisible(visible);
        updateRecPeriodList(true);
        repaintMotionPeriods();
    }
}

void NavigationItem::repaintMotionPeriods()
{
    QnAbstractArchiveReader *currentReader = 0;
    if (m_camera)
        currentReader = dynamic_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    bool useSync = m_syncButton->isChecked();

    QVector<QnTimePeriodList> allPeriods;
    bool isMotionExist = false;
    bool isMotionFound = false;
    for (MotionPeriods::iterator itr = m_motionPeriodLoader.begin(); itr != m_motionPeriodLoader.end(); ++itr)
    {
        const MotionPeriodLoader& info = itr.value();
        CLVideoCamera *camera = findCameraByResource(info.reader->getResource());
        if (!info.periods.isEmpty() && camera && camera->isVisible() && !info.isMotionEmpty())
            allPeriods << info.periods;
        QnTimePeriodList tp = info.isMotionEmpty() ? QnTimePeriodList() : info.periods;
        isMotionExist |= !tp.isEmpty();
        if (!useSync) 
        {
            info.reader->setPlaybackMask(tp);
            if (info.reader == currentReader)
            {
                isMotionFound = true;
                m_timeSlider->setMotionTimePeriodList(tp);
            }
        }
    }
    if (!useSync && !isMotionFound) 
        m_timeSlider->setMotionTimePeriodList(QnTimePeriodList()); // // not motion for selected item

    if (useSync)
    {
        m_mergedMotionPeriods = QnTimePeriod::mergeTimePeriods(allPeriods);
        foreach(CLVideoCamera *camera, m_reserveCameras)
        {
            QnAbstractArchiveReader *reader = dynamic_cast<QnAbstractArchiveReader*> (camera->getStreamreader());
            if (reader)
                reader->setPlaybackMask(m_mergedMotionPeriods);
        }
        m_timeSlider->setMotionTimePeriodList(m_mergedMotionPeriods);
    }
    //m_mrsButton->setVisible(isMotionExist);
    m_mrsButton->setVisible(false);
}

void NavigationItem::onMotionPeriodLoaded(const QnTimePeriodList& timePeriods, int handle)
{
    bool needUpdate = false;
    for (MotionPeriods::iterator itr = m_motionPeriodLoader.begin(); itr != m_motionPeriodLoader.end(); ++itr)
    {
        MotionPeriodLoader& info = itr.value();
        if (info.loadingHandle == handle)
        {
            info.periods = timePeriods;
            info.loadingHandle = 0;
            needUpdate = true;
        }
    }
    if (needUpdate)
        repaintMotionPeriods();
}


void NavigationItem::onValueChanged(qint64 time)
{
    if (m_camera == 0)
        return;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    if (reader->getSpeed() > 0 &&  m_camera->getCurrentTime() == DATETIME_NOW && !m_camera->getCamDisplay()->isRealTimeSource())
    {
        smartSeek(DATETIME_NOW);
        return;
    }

    if (m_currentTime == time || (!m_timeSlider->isUserInput() && !m_timeSlider->isMoving())) {
        updateRecPeriodList(false);
        return;
    }

    //if (reader->isSkippingFrames())
    //    return;

    //if (reader->isRealTimeSource())
    //    return;

    smartSeek(time);

    updateRecPeriodList(false);
}

void NavigationItem::smartSeek(qint64 timeMSec)
{
    if (m_camera == NULL)
        return;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    if (timeMSec == DATETIME_NOW || m_timeSlider->isAtEnd()) {
        reader->jumpToPreviousFrame(DATETIME_NOW);

        m_liveButton->setChecked(true);
    } else {
        timeMSec *= 1000;
        if (m_timeSlider->isMoving())
            reader->jumpTo(timeMSec, 0);
        else
            reader->jumpToPreviousFrame(timeMSec);

        m_liveButton->setChecked(false);
    }
}

void NavigationItem::pause()
{
    if (m_camera == NULL)
        return;

    setActive(true);

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->pauseMedia();
    m_camera->getCamDisplay()->pauseAudio();
    m_mrsButton->hide();

    reader->setSingleShotMode(true);
    //m_camera->getCamDisplay()->setSingleShotMode(true);
}

void NavigationItem::play()
{
    if (m_camera == NULL)
        return;

    setActive(true);

    QnAbstractArchiveReader *reader = dynamic_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    if (!reader)
        return;

    if (reader->isMediaPaused() && reader->isRealTimeSource()) {
        qint64 time = m_camera->getCurrentTime();
        reader->resumeMedia();
        reader->directJumpToNonKeyFrame(time+1);
    }
    else {
        reader->resumeMedia();
    }

    if (!m_playing)
        m_camera->getCamDisplay()->playAudio(m_playing);
}

void NavigationItem::rewindBackward()
{
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
}

void NavigationItem::rewindForward()
{
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
}

void NavigationItem::stepBackward()
{
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
}

void NavigationItem::stepForward()
{
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
}

void NavigationItem::onSliderPressed()
{
    if (m_camera == NULL)
        return;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->setSingleShotMode(true);
    m_camera->getCamDisplay()->playAudio(false);
    setActive(true);
}

void NavigationItem::onSliderReleased()
{
    if (m_camera == NULL)
        return;

    setActive(true);
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    bool isCamera = dynamic_cast<QnSecurityCamResource*>(reader->getResource().data());
    if (!isCamera) {
        // Disable precise seek via network to reduce network flood
        smartSeek(m_timeSlider->currentValue());
    }
    if (isPlaying())
    {
        reader->setSingleShotMode(false);
        m_camera->getCamDisplay()->playAudio(true);
    }
}

void NavigationItem::onSpeedChanged(float newSpeed)
{
    if (m_camera) {
        QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
        if (qFuzzyIsNull(newSpeed))
            newSpeed = 0.0f;
        if (newSpeed >= 0.0f || reader->isNegativeSpeedSupported())
        {
            reader->setSpeed(newSpeed);
            if (newSpeed != 0.0f)
                play();
            else
                pause();

            //setInfoText(!qFuzzyIsNull(newSpeed) ? tr("[ Speed: %1 ]").arg(tr("%1x").arg(newSpeed)) : tr("Paused"));
        }
        else
        {
            m_speedSlider->resetSpeed();
        }
    }

    if(qFuzzyCompare(newSpeed, 1.0f) || qFuzzyIsNull(newSpeed) || (newSpeed > 0.0f && newSpeed < 1.0f)) {
        m_stepForwardButton->setPressed(false);
        m_stepBackwardButton->setPressed(false);
    } else if(newSpeed > 1.0) {
        m_stepForwardButton->setPressed(true);
        m_stepBackwardButton->setPressed(false);
    } else if(newSpeed < 0.0f) {
        m_stepForwardButton->setPressed(false);
        m_stepBackwardButton->setPressed(true);
    }
}

void NavigationItem::onVolumeLevelChanged(int newVolumeLevel)
{
    setMute(m_volumeSlider->isMute());

    //setInfoText(tr("[ Volume: %1 ]").arg(!m_volumeSlider->isMute() ? tr("%1%").arg(newVolumeLevel) : tr("Muted")));
}

void NavigationItem::setMute(bool mute)
{
    m_muteButton->setChecked(mute);
}

void NavigationItem::setPlaying(bool playing)
{
    setActive(true);

    if (m_playing == playing)
        return;

    m_playing = playing;

    if (m_playing) {
        m_stepBackwardButton->setIcon(qnSkin->icon("backward.png"));

        m_playButton->setIcon(qnSkin->icon("pause.png"));

        m_stepForwardButton->setIcon(qnSkin->icon("forward.png"));

        m_speedSlider->setPrecision(SpeedSlider::LowPrecision);

        play();
    } else {
        m_stepBackwardButton->setIcon(qnSkin->icon("step_backward.png"));

        m_playButton->setIcon(qnSkin->icon("play.png"));

        m_stepForwardButton->setIcon(qnSkin->icon("step_forward.png"));

        m_speedSlider->setPrecision(SpeedSlider::HighPrecision);

        pause();
        m_timeSlider->setLiveMode(false);
    }
}

void NavigationItem::togglePlayPause()
{
    setPlaying(!m_playing);
}

void NavigationItem::at_liveButton_clicked(bool checked)
{
    if (m_camera && checked == m_camera->getCamDisplay()->isRealTimeSource()) {
        qnWarning("Camera live state and live button state are not in sync, investigate.");
        return;
    }

    if(!checked) {
        m_liveButton->toggle(); /* Don't allow jumps back from live. */
        return;
    }

    m_timeSlider->setLiveMode(true);
    m_timeSlider->setCurrentValue(m_timeSlider->maximumValue());
    smartSeek(DATETIME_NOW);
}

void NavigationItem::onSyncButtonToggled(bool value)
{
    if (!m_camera) {
        qnWarning("Actual camera is NULL, expect troubles.");
        return;
    }
        
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    qint64 currentTime = m_camera->getCurrentTime();
    if (reader->isRealTimeSource())
        currentTime = DATETIME_NOW;

    emit enableItemSync(value);
    repaintMotionPeriods();
    updateRecPeriodList(true);
    if (value) {
        reader->jumpTo(currentTime, 0);
        reader->setSpeed(1.0);
    }
    m_speedSlider->resetSpeed();
    if (!value)
        updateActualCamera();
}

void NavigationItem::onExportRange(qint64 startTimeMs,qint64 endTimeMs)
{
    if (forcedVideoCamera() == 0)
    {
        QMessageBox::information(0, tr("No camera selected"), tr("You should select camera to export"));
    }
    else {
        emit exportRange(forcedVideoCamera(), startTimeMs, endTimeMs);
    }
}


bool NavigationItem::MotionPeriodLoader::isMotionEmpty() const
{
    for (int i = 0; i < regions.size(); ++i)
    {
        if (!regions[i].isEmpty())
            return false;
    }
    return true;
}
