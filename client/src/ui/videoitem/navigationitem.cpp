#include "navigationitem.h"

#include <QtCore/QEvent>

#include <QtGui/QAction>
#include <QtGui/QGraphicsLinearLayout>

#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/video_server.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>

#include "camera/camera.h"

#include "ui/skin/skin.h"
#include "ui/videoitem/video_wnd_item.h"
#include "ui/widgets/imagebutton.h"
#include "ui/widgets/speedslider.h"
#include "ui/widgets/volumeslider.h"
#include "ui/widgets/tooltipitem.h"
#include "ui/graphics/items/image_button_widget.h"

#include "ui/widgets2/graphicslabel.h"

#include "timeslider.h"

static const int SLIDER_NOW_AREA_WIDTH = 30;
static const int TIME_PERIOD_UPDATE_INTERVAL = 1000 * 10;

class SliderToolTipItem : public StyledToolTipItem
{
public:
    SliderToolTipItem(AbstractGraphicsSlider *slider, QGraphicsItem *parent = 0)
        : StyledToolTipItem(parent), m_slider(slider)
    {
        setAcceptHoverEvents(true);
        setOpacity(0.75);
    }
#if 0
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        m_pos = mapToScene(event->pos());
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {
        QPointF pos = mapToScene(event->pos());
        qreal shift = qreal(m_slider->maximum() - m_slider->minimum()) / m_slider->rect().width() * (pos - m_pos).x();
        m_slider->setValue(m_slider->value() + shift);
        m_pos = pos;
    }
#endif
private:
    AbstractGraphicsSlider *const m_slider;
    QPointF m_pos;
};


class TimeSliderToolTipItem : public StyledToolTipItem
{
public:
    TimeSliderToolTipItem(TimeSlider *slider, QGraphicsItem *parent = 0)
        : StyledToolTipItem(parent), m_slider(slider)
    {
        setAcceptHoverEvents(true);
        setOpacity(0.75);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        m_pos = mapToScene(event->pos());
        m_slider->setMoving(true);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {
        QPointF pos = mapToScene(event->pos());
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


NavigationItem::NavigationItem(QGraphicsItem *parent)
    : QGraphicsWidget(parent),
    m_camera(0), m_forcedCamera(0), m_currentTime(0),
    m_playing(false),
    restoreInfoTextData(0)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, false);
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

    m_backwardButton = new ImageButton(this);
    m_backwardButton->addPixmap(Skin::pixmap(QLatin1String("rewind_backward_grey.png")), ImageButton::Active, ImageButton::Background);
    m_backwardButton->addPixmap(Skin::pixmap(QLatin1String("rewind_backward_blue.png")), ImageButton::Active, ImageButton::Hovered);
    m_backwardButton->setPreferredSize(32, 18);
    m_backwardButton->setMaximumSize(m_backwardButton->preferredSize());

    m_stepBackwardButton = new ImageButton(this);
    m_stepBackwardButton->addPixmap(Skin::pixmap(QLatin1String("step_backward_grey.png")), ImageButton::Active, ImageButton::Background);
    m_stepBackwardButton->addPixmap(Skin::pixmap(QLatin1String("step_backward_blue.png")), ImageButton::Active, ImageButton::Hovered);
    m_stepBackwardButton->setPreferredSize(32, 18);
    m_stepBackwardButton->setMaximumSize(m_stepBackwardButton->preferredSize());

    m_playButton = new ImageButton(this);
    m_playButton->addPixmap(Skin::pixmap(QLatin1String("play_grey.png")), ImageButton::Active, ImageButton::Background);
    m_playButton->addPixmap(Skin::pixmap(QLatin1String("play_blue.png")), ImageButton::Active, ImageButton::Hovered);
    m_playButton->setPreferredSize(32, 30);
    m_playButton->setMaximumSize(m_playButton->preferredSize());

    m_stepForwardButton = new ImageButton(this);
    m_stepForwardButton->addPixmap(Skin::pixmap(QLatin1String("step_forward_grey.png")), ImageButton::Active, ImageButton::Background);
    m_stepForwardButton->addPixmap(Skin::pixmap(QLatin1String("step_forward_blue.png")), ImageButton::Active, ImageButton::Hovered);
    m_stepForwardButton->setPreferredSize(32, 18);
    m_stepForwardButton->setMaximumSize(m_stepForwardButton->preferredSize());

    m_forwardButton = new ImageButton(this);
    m_forwardButton->addPixmap(Skin::pixmap(QLatin1String("rewind_forward_grey.png")), ImageButton::Active, ImageButton::Background);
    m_forwardButton->addPixmap(Skin::pixmap(QLatin1String("rewind_forward_blue.png")), ImageButton::Active, ImageButton::Hovered);
    m_forwardButton->setPreferredSize(32, 18);
    m_forwardButton->setMaximumSize(m_forwardButton->preferredSize());

    connect(m_backwardButton, SIGNAL(clicked()), this, SLOT(rewindBackward()));
    connect(m_stepBackwardButton, SIGNAL(clicked()), this, SLOT(stepBackward()));
    connect(m_playButton, SIGNAL(clicked()), this, SLOT(togglePlayPause()));
    connect(m_stepForwardButton, SIGNAL(clicked()), this, SLOT(stepForward()));
    connect(m_forwardButton, SIGNAL(clicked()), this, SLOT(rewindForward()));


    m_speedSlider = new SpeedSlider(Qt::Horizontal, this);
    m_speedSlider->setObjectName("SpeedSlider");
    m_speedSlider->setToolTipItem(new SliderToolTipItem(m_speedSlider));
    m_speedSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);

    connect(m_speedSlider, SIGNAL(speedChanged(float)), this, SLOT(onSpeedChanged(float)));
    connect(m_speedSlider, SIGNAL(frameBackward()), this, SLOT(stepBackward()));
    connect(m_speedSlider, SIGNAL(frameForward()), this, SLOT(stepForward()));


    m_timeSlider = new TimeSlider(this);
    m_timeSlider->setObjectName("TimeSlider");
    m_timeSlider->setToolTipItem(new TimeSliderToolTipItem(m_timeSlider));
    m_timeSlider->toolTipItem()->setVisible(false);
    m_timeSlider->setEndSize(SLIDER_NOW_AREA_WIDTH);

    m_timeSlider->setAutoFillBackground(true);
    {
        QPalette pal = m_timeSlider->palette();
        pal.setColor(QPalette::Window, QColor(15, 15, 15, 128));
        pal.setColor(QPalette::WindowText, QColor(63, 159, 216));
        m_timeSlider->setPalette(pal);
    }

    connect(m_timeSlider, SIGNAL(currentValueChanged(qint64)), this, SLOT(onValueChanged(qint64)));
    connect(m_timeSlider, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));
    connect(m_timeSlider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
    connect(m_timeSlider, SIGNAL(exportRange(qint64,qint64)), this, SIGNAL(exportRange(qint64,qint64)));

    m_timerId = startTimer(33);


    m_muteButton = new ImageButton(this);
    m_muteButton->setObjectName("MuteButton");
    m_muteButton->addPixmap(Skin::pixmap(QLatin1String("unmute.png")), ImageButton::Active, ImageButton::Background);
    m_muteButton->addPixmap(Skin::pixmap(QLatin1String("mute.png")), ImageButton::Active, ImageButton::Checked);
    m_muteButton->setPreferredSize(20, 20);
    m_muteButton->setMaximumSize(m_muteButton->preferredSize());
    m_muteButton->setCheckable(true);
    m_muteButton->setChecked(m_volumeSlider->isMute());

    m_liveButton = new QnImageButtonWidget(this);
    m_liveButton->setObjectName("LiveButton");
    m_liveButton->setPixmap(0,                              Skin::pixmap(QLatin1String("live.png")));
    m_liveButton->setPixmap(QnImageButtonWidget::HOVERED,   Skin::pixmap(QLatin1String("live_hovered.png")));
    m_liveButton->setPixmap(QnImageButtonWidget::CHECKED,   Skin::pixmap(QLatin1String("live_checked.png")));
    m_liveButton->setPixmap(QnImageButtonWidget::DISABLED,  Skin::pixmap(QLatin1String("live_disabled.png")));
    m_liveButton->setPixmap(QnImageButtonWidget::CHECKED | QnImageButtonWidget::DISABLED, Skin::pixmap(QLatin1String("live_disabled.png")));
    m_liveButton->setPreferredSize(48, 24);
    m_liveButton->setMaximumSize(m_liveButton->preferredSize());
    m_liveButton->setMinimumSize(m_liveButton->preferredSize());
    m_liveButton->setCheckable(true);
    m_liveButton->setChecked(m_camera && m_camera->getCamCamDisplay()->isRealTimeSource());
    m_liveButton->setAnimationSpeed(4.0);
    m_liveButton->setEnabled(false);

    connect(m_liveButton, SIGNAL(clicked(bool)), this, SLOT(at_liveButton_clicked(bool)));

    m_mrsButton = new ImageButton(this);
    m_mrsButton->setObjectName("MRSButton");
    m_mrsButton->addPixmap(Skin::pixmap(QLatin1String("mrs.png")), ImageButton::Active, ImageButton::Background);
    m_mrsButton->addPixmap(Skin::pixmap(QLatin1String("mrs_checked.png")), ImageButton::Active, ImageButton::Checked);
    m_mrsButton->setPreferredSize(48, 24);
    m_mrsButton->setMaximumSize(m_mrsButton->preferredSize());
    m_mrsButton->setCheckable(true);
    m_mrsButton->setChecked(true);
    m_mrsButton->hide();

    connect(m_mrsButton, SIGNAL(clicked()), this, SIGNAL(clearMotionSelection()));

    m_syncButton = new ImageButton(this);
    m_syncButton->setObjectName("SyncButton");
    m_syncButton->addPixmap(Skin::pixmap(QLatin1String("sync.png")), ImageButton::Active, ImageButton::Background);
    m_syncButton->addPixmap(Skin::pixmap(QLatin1String("sync_checked.png")), ImageButton::Active, ImageButton::Checked);
    m_syncButton->setPreferredSize(48, 24);
    m_syncButton->setMaximumSize(m_syncButton->preferredSize());
    m_syncButton->setCheckable(true);
    m_syncButton->setChecked(true);

    //connect(m_syncButton, SIGNAL(toggled(bool)), this, SIGNAL(enableItemSync(bool)));
    connect(m_syncButton, SIGNAL(toggled(bool)), this, SLOT(onSyncButtonToggled(bool)));


    m_volumeSlider = new VolumeSlider(Qt::Horizontal);
    m_volumeSlider->setObjectName("VolumeSlider");
    m_volumeSlider->setToolTipItem(new SliderToolTipItem(m_volumeSlider));
    m_volumeSlider->setCacheMode(QGraphicsItem::ItemCoordinateCache);

    connect(m_muteButton, SIGNAL(clicked(bool)), m_volumeSlider, SLOT(setMute(bool)));
    connect(m_volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(onVolumeLevelChanged(int)));

    m_timeLabel = new GraphicsLabel(this);
    m_timeLabel->setObjectName("TimeLabel");
    m_timeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::Label);
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
    leftLayoutV->setAlignment(m_speedSlider, Qt::AlignTop);
    leftLayoutV->addItem(buttonsLayout);
    leftLayoutV->setAlignment(buttonsLayout, Qt::AlignBottom);

    QGraphicsLinearLayout *rightSublayoutVL = new QGraphicsLinearLayout(Qt::Vertical);
    rightSublayoutVL->setContentsMargins(0, 0, 0, 0);
    rightSublayoutVL->setSpacing(0);
    rightSublayoutVL->addItem(m_timeLabel);
    rightSublayoutVL->setAlignment(m_timeLabel, Qt::AlignLeft | Qt::AlignVCenter);
    rightSublayoutVL->addItem(m_mrsButton);
    rightSublayoutVL->setAlignment(m_mrsButton, Qt::AlignCenter);

    QGraphicsLinearLayout *rightSublayoutVR = new QGraphicsLinearLayout(Qt::Vertical);
    rightSublayoutVR->setContentsMargins(0, 0, 0, 0);
    rightSublayoutVR->setSpacing(0);
    rightSublayoutVR->addItem(m_syncButton);
    rightSublayoutVR->setAlignment(m_syncButton, Qt::AlignCenter);
    rightSublayoutVR->addItem(m_liveButton);
    rightSublayoutVR->setAlignment(m_liveButton, Qt::AlignCenter);

    QGraphicsLinearLayout *rightLayoutH = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutH->setContentsMargins(0, 0, 0, 0);
    rightLayoutH->setSpacing(3);
    rightLayoutH->addItem(rightSublayoutVL);
    rightLayoutH->setAlignment(rightSublayoutVL, Qt::AlignLeft | Qt::AlignVCenter);
    rightLayoutH->addItem(rightSublayoutVR);
    rightLayoutH->setAlignment(rightSublayoutVR, Qt::AlignCenter);
    rightLayoutH->addItem(m_muteButton);
    rightLayoutH->setAlignment(m_muteButton, Qt::AlignRight | Qt::AlignTop);

    QGraphicsLinearLayout *rightLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    rightLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    rightLayoutV->setContentsMargins(0, 3, 0, 0);
    rightLayoutV->setSpacing(0);
    rightLayoutV->addItem(m_volumeSlider);
    rightLayoutV->setAlignment(m_volumeSlider, Qt::AlignCenter);
    rightLayoutV->addStretch();
    rightLayoutV->addItem(rightLayoutH);
    rightLayoutV->setAlignment(rightLayoutH, Qt::AlignBottom);

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(5, 0, 5, 0);
    mainLayout->setSpacing(10);
    mainLayout->addItem(leftLayoutV);
    mainLayout->setAlignment(leftLayoutV, Qt::AlignLeft | Qt::AlignVCenter);
    mainLayout->addItem(m_timeSlider);
    mainLayout->setAlignment(m_timeSlider, Qt::AlignCenter);
    mainLayout->addItem(rightLayoutV);
    mainLayout->setAlignment(rightLayoutV, Qt::AlignRight | Qt::AlignVCenter);
    setLayout(mainLayout);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);


    QAction *playAction = new QAction(tr("Play / Pause"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    addAction(playAction);


    setVideoCamera(0);
    m_fullTimePeriodHandle = 0;
    m_timePeriodUpdateTime = 0;
    m_forceTimePeriodLoading = false;
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
    m_liveButton->setEnabled(true);
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
    if(m_reserveCameras.empty())
        m_liveButton->setEnabled(false);
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

    m_speedSlider->resetSpeed();
    m_timeSlider->resetSelectionRange();
    restoreInfoText();

    if (m_camera)
        disconnect(m_camera->getCamCamDisplay(), 0, this, 0);

    m_camera = camera;

    if (m_camera)
    {
        connect(m_camera->getCamCamDisplay(), SIGNAL(liveMode(bool)), this, SLOT(onLiveModeChanged(bool)));

        QnAbstractArchiveReader *reader = dynamic_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
        if (reader)
            setPlaying(!reader->isMediaPaused());
        else
            setPlaying(true);
        if (!m_syncButton->isChecked()) {
            updateRecPeriodList(true);
            repaintMotionPeriods();
        }
        m_timeSlider->setLiveMode(reader->isRealTimeSource());
    }
    else
    {
        m_timeSlider->setScalingFactor(0);
    }

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

    QGraphicsWidget::timerEvent(event);
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
        m_timeSlider->setMinimumValue(startTime != DATETIME_NOW ? startTime / 1000 : QDateTime::currentMSecsSinceEpoch() - 10000); // if  nothing is recorded set minvalue to live-10sec
        m_timeSlider->setMaximumValue(endTime != DATETIME_NOW ? endTime / 1000 : DATETIME_NOW);
        if (m_timeSlider->minimumValue() == 0)
            m_timeLabel->setText(formatDuration(m_timeSlider->length() / 1000));
        else
            m_timeLabel->setText(QString()); //(QDateTime::fromMSecsSinceEpoch(m_timeSlider->maximumValue()).toString(Qt::SystemLocaleShortDate));
        m_timeLabel->setVisible(!m_camera->getCamCamDisplay()->isRealTimeSource());

        quint64 time = m_camera->getCurrentTime();
        if (time != AV_NOPTS_VALUE)
        {
            m_currentTime = time != DATETIME_NOW ? time/1000 : time;
            m_timeSlider->setCurrentValue(m_currentTime, true);
        }

        m_forceTimePeriodLoading = !updateRecPeriodList(m_forceTimePeriodLoading); // if period does not loaded yet, force loading
    }

    m_liveButton->setChecked(!reader->isMediaPaused() && m_camera->getCamCamDisplay()->isRealTimeSource());
}

void NavigationItem::updateMotionPeriods(const QnTimePeriod& period)
{
    m_motionPeriod = period;
    foreach(CLVideoCamera *camera, m_reserveCameras) {
        QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(camera->getDevice());
        if (netRes)
        {
            MotionPeriods::iterator itr = m_motionPeriodLoader.find(netRes);
            if (itr != m_motionPeriodLoader.end() && !itr.value().region.isEmpty())
                itr.value().loadingHandle = itr.value().loader->load(period, itr.value().region);
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

void NavigationItem::loadMotionPeriods(QnResourcePtr resource, QnAbstractArchiveReader* reader, QRegion region)
{
#ifndef DEBUG_MOTION
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!netRes)
        return;
#endif

    MotionPeriodLoader* p = getMotionLoader(reader);
    if (!p)
        return;
    p->region = region;

    if (region.isEmpty())
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
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
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

    p->loadingHandle = p->loader->load(loadingPeriod, region);
}

bool NavigationItem::updateRecPeriodList(bool force)
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
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
    qint64 minTimeMs = reader ? reader->startTime()/1000 : 0;
    m_timePeriod.startTimeMs = qMax(minTimeMs, t - w);
    m_timePeriod.durationMs = qMin(currentTime+1000 - m_timePeriod.startTimeMs, w * 3);
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

}

void NavigationItem::onTimePeriodLoaded(const QnTimePeriodList& timePeriods, int handle)
{
    if (handle == m_fullTimePeriodHandle)
        m_timeSlider->setRecTimePeriodList(timePeriods);
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

    CLVideoCamera* camera = findCameraByResource(resource);
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
        CLVideoCamera* camera = findCameraByResource(info.reader->getResource());
        if (!info.periods.isEmpty() && camera && camera->isVisible() && !info.region.isEmpty())
            allPeriods << info.periods;
        QnTimePeriodList tp = info.region.isEmpty() ? QnTimePeriodList() : info.periods;
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
        foreach(CLVideoCamera* camera, m_reserveCameras)
        {
            QnAbstractArchiveReader* reader = dynamic_cast<QnAbstractArchiveReader*> (camera->getStreamreader());
            if (reader)
                reader->setPlaybackMask(m_mergedMotionPeriods);
        }
        m_timeSlider->setMotionTimePeriodList(m_mergedMotionPeriods);
    }
    m_mrsButton->setVisible(isMotionExist);
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

    if (m_camera->getCurrentTime() == DATETIME_NOW && !m_camera->getCamCamDisplay()->isRealTimeSource())
    {
        smartSeek(DATETIME_NOW);
        return;
    }

    if (m_currentTime == time || !m_timeSlider->isUserInput()) {
        updateRecPeriodList(false);
        return;
    }

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    //if (reader->isSkippingFrames())
    //    return;

    if (reader->isRealTimeSource())
        return;

    smartSeek(time);

    updateRecPeriodList(false);
}

void NavigationItem::smartSeek(qint64 timeMSec)
{
    if (m_camera == NULL)
        return;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    if (m_timeSlider->isAtEnd()) {
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
    m_camera->getCamCamDisplay()->pauseAudio();
    m_mrsButton->hide();

    reader->setSingleShotMode(true);
    //m_camera->getCamCamDisplay()->setSingleShotMode(true);
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
        reader->resumeMedia();
        reader->directJumpToNonKeyFrame(m_camera->getCurrentTime()+1);
    }
    else {
        reader->resumeMedia();
    }

    if (!m_playing)
        m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void NavigationItem::rewindBackward()
{
    if (m_camera == NULL)
        return;

    setActive(true);
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->jumpTo(0, true);
    //m_camera->streamJump(0);
    reader->resumeDataProcessors();
}

void NavigationItem::rewindForward()
{
    if (m_camera == NULL)
        return;

    setActive(true);
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    bool stopped = reader->isMediaPaused();
    if (stopped)
        play();

    reader->jumpTo(reader->lengthMksec(), false);
    //m_camera->streamJump(reader->lengthMksec());
    reader->resumeDataProcessors();
    if (stopped)
    {
        qApp->processEvents();
        pause();
    }
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
            m_camera->getCamCamDisplay()->playAudio(false);

        reader->previousFrame(curr_time);
        //m_camera->streamJump(curr_time);
        //m_camera->getCamCamDisplay()->setSingleShotMode(false);
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
    //m_camera->getCamCamDisplay()->setSingleShotMode(true);
}

void NavigationItem::onSliderPressed()
{
    if (m_camera == NULL)
        return;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->setSingleShotMode(true);
    m_camera->getCamCamDisplay()->playAudio(false);
    setActive(true);
}

void NavigationItem::onSliderReleased()
{
    if (m_camera == NULL)
        return;

    setActive(true);
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    smartSeek(m_timeSlider->currentValue());
    if (isPlaying())
    {
        reader->setSingleShotMode(false);
        m_camera->getCamCamDisplay()->playAudio(true);
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

            setInfoText(!qFuzzyIsNull(newSpeed) ? tr("[ Speed: %1 ]").arg(tr("%1x").arg(newSpeed)) : tr("Paused"));
        }
        else
        {
            m_speedSlider->resetSpeed();
        }
    }
}

void NavigationItem::onVolumeLevelChanged(int newVolumeLevel)
{
    setMute(m_volumeSlider->isMute());

    setInfoText(tr("[ Volume: %1 ]").arg(!m_volumeSlider->isMute() ? tr("%1%").arg(newVolumeLevel) : tr("Muted")));
}

void NavigationItem::setInfoText(const QString &infoText)
{
    if (CLVideoWindowItem *vwi = m_camera ? m_camera->getVideoWindow() : 0) {
        if (restoreInfoTextData) {
            restoreInfoTextData->timer.stop();
        } else {
            restoreInfoTextData = new RestoreInfoTextData;
            restoreInfoTextData->extraInfoText = vwi->extraInfoText();
            restoreInfoTextData->timer.setSingleShot(true);
            connect(&restoreInfoTextData->timer, SIGNAL(timeout()), this, SLOT(restoreInfoText()));
        }
        restoreInfoTextData->timer.start(3000);

        vwi->setExtraInfoText(infoText);
    }
}

void NavigationItem::restoreInfoText()
{
    if (!restoreInfoTextData)
        return;

    if (CLVideoWindowItem *vwi = m_camera ? m_camera->getVideoWindow() : 0) {
        restoreInfoTextData->timer.stop();
        vwi->setExtraInfoText(restoreInfoTextData->extraInfoText);
    }

    delete restoreInfoTextData;
    restoreInfoTextData = 0;
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
        m_stepBackwardButton->addPixmap(Skin::pixmap(QLatin1String("backward_grey.png")), ImageButton::Active, ImageButton::Background);
        m_stepBackwardButton->addPixmap(Skin::pixmap(QLatin1String("backward_blue.png")), ImageButton::Active, ImageButton::Hovered);

        m_playButton->addPixmap(Skin::pixmap(QLatin1String("pause_grey.png")), ImageButton::Active, ImageButton::Background);
        m_playButton->addPixmap(Skin::pixmap(QLatin1String("pause_blue.png")), ImageButton::Active, ImageButton::Hovered);

        m_stepForwardButton->addPixmap(Skin::pixmap(QLatin1String("forward_grey.png")), ImageButton::Active, ImageButton::Background);
        m_stepForwardButton->addPixmap(Skin::pixmap(QLatin1String("forward_blue.png")), ImageButton::Active, ImageButton::Hovered);

        m_speedSlider->setPrecision(SpeedSlider::LowPrecision);

        play();
    } else {
        m_stepBackwardButton->addPixmap(Skin::pixmap(QLatin1String("step_backward_grey.png")), ImageButton::Active, ImageButton::Background);
        m_stepBackwardButton->addPixmap(Skin::pixmap(QLatin1String("step_backward_blue.png")), ImageButton::Active, ImageButton::Hovered);

        m_playButton->addPixmap(Skin::pixmap(QLatin1String("play_grey.png")), ImageButton::Active, ImageButton::Background);
        m_playButton->addPixmap(Skin::pixmap(QLatin1String("play_blue.png")), ImageButton::Active, ImageButton::Hovered);

        m_stepForwardButton->addPixmap(Skin::pixmap(QLatin1String("step_forward_grey.png")), ImageButton::Active, ImageButton::Background);
        m_stepForwardButton->addPixmap(Skin::pixmap(QLatin1String("step_forward_blue.png")), ImageButton::Active, ImageButton::Hovered);

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
    if (m_camera && checked == m_camera->getCamCamDisplay()->isRealTimeSource()) {
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
    //m_timeSlider->setCurrentValue(checked ? DATETIME_NOW : m_timeSlider->minimumValue());
}

void NavigationItem::onSyncButtonToggled(bool value)
{
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
