#include "navigationitem.h"

#include <QtCore/QEvent>

#include <QtGui/QAction>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QGraphicsProxyWidget>
#include <QtGui/QLabel>

#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/video_server.h>
#include "timeslider.h"
#include "camera/camera.h"
#include "ui/videoitem/video_wnd_item.h"
#include "ui/widgets/imagebutton.h"
#include "ui/widgets/speedslider.h"
#include "ui/widgets/volumeslider.h"
#include "ui/widgets/tooltipitem.h"
#include "ui/skin.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "utils/common/util.h"
#include "utils/common/warnings.h"

void detail::QnTimePeriodUpdater::update(const QnVideoServerConnectionPtr &connection, const QnNetworkResourceList &networkResources, const QnTimePeriod &timePeriod)
{
    m_connection = connection;
    m_networkResources = networkResources;
    m_timePeriod = timePeriod;

    if(m_updatePending) {
        m_updateNeeded = true;
        return;
    }

    sendRequest();
}

void detail::QnTimePeriodUpdater::at_replyReceived(const QnTimePeriodList &timePeriods)
{
    if(m_updateNeeded) {
        /* The data we got has already expired... resend the request. */
        sendRequest();
        return;
    }

    m_updatePending = false;
    emit ready(timePeriods);
}

void detail::QnTimePeriodUpdater::sendRequest()
{
    m_updateNeeded = false;
    m_updatePending = true;
    m_connection->asyncRecordedTimePeriods(m_networkResources, m_timePeriod.startTimeUSec, m_timePeriod.startTimeUSec + m_timePeriod.durationUSec, 1, this, SLOT(at_replyReceived(const QnTimePeriodList &)));
}


static const int SLIDER_NOW_AREA_WIDTH = 30;

// ### hack to avoid scene move up and down
class QLabelKillsWheelEvent : public QLabel
{
protected:
    void wheelEvent(QWheelEvent *) {}
};

class QGraphicsWidgetKillsWheelEvent : public QGraphicsWidget
{
public:
    QGraphicsWidgetKillsWheelEvent(QGraphicsItem *parent = 0) : QGraphicsWidget(parent) {}

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *) {}
};
// ###


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


NavigationItem::NavigationItem(QGraphicsItem * /*parent*/) :
    CLUnMovedInteractiveOpacityItem(QString("name:)"), 0, 0.5, 0.95),
    m_camera(0), m_forcedCamera(0), m_currentTime(0),
    m_playing(false),
    m_mouseOver(false),
    restoreInfoTextData(0)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, false);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    setFlag(QGraphicsItem::ItemHasNoContents, true);
    setFlag(QGraphicsItem::ItemIsPanel, true);

    setAcceptHoverEvents(true);

    setCursor(Qt::ArrowCursor);

    m_timePeriodUpdater = new detail::QnTimePeriodUpdater(this);
    connect(m_timePeriodUpdater, SIGNAL(ready(const QnTimePeriodList &)), this, SLOT(onTimePeriodUpdaterReady(const QnTimePeriodList &)));

    m_graphicsWidget = new QGraphicsWidgetKillsWheelEvent(this);
    m_graphicsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_graphicsWidget->setCursor(Qt::ArrowCursor);
    m_graphicsWidget->setAutoFillBackground(true);

    {
        QPalette palette = m_graphicsWidget->palette();
        palette.setColor(QPalette::Window, Qt::black);
        m_graphicsWidget->setPalette(palette);
    }

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

    m_speedSlider = new SpeedSlider(Qt::Horizontal, this);
    m_speedSlider->setObjectName("SpeedSlider");
    m_speedSlider->setToolTipItem(new SliderToolTipItem(m_speedSlider));
    m_speedSlider->setCursor(Qt::ArrowCursor);

    connect(m_speedSlider, SIGNAL(speedChanged(float)), this, SLOT(onSpeedChanged(float)));
    connect(m_speedSlider, SIGNAL(frameBackward()), this, SLOT(stepBackward()));
    connect(m_speedSlider, SIGNAL(frameForward()), this, SLOT(stepForward()));

    QGraphicsLinearLayout *leftLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    leftLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    leftLayoutV->setContentsMargins(0, 0, 0, 0);
    leftLayoutV->setSpacing(0);
    leftLayoutV->addItem(m_speedSlider);
    leftLayoutV->setAlignment(m_speedSlider, Qt::AlignTop);
    leftLayoutV->addItem(buttonsLayout);
    leftLayoutV->setAlignment(buttonsLayout, Qt::AlignBottom);


    m_timeSlider = new TimeSlider(this);
    m_timeSlider->setObjectName("TimeSlider");
    m_timeSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_timeSlider->setToolTipItem(new TimeSliderToolTipItem(m_timeSlider));
    m_timeSlider->setCursor(Qt::ArrowCursor);
    m_graphicsWidget->resize(m_timeSlider->size());

    m_timeSlider->setEndSize(SLIDER_NOW_AREA_WIDTH);

    {
        QPalette palette = m_timeSlider->palette();
        palette.setColor(QPalette::Window, QColor(15, 15, 15));
        palette.setColor(QPalette::WindowText, QColor(63, 159, 216));
        m_timeSlider->setPalette(palette);
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
    m_muteButton->setCursor(Qt::ArrowCursor);

    m_liveButton = new ImageButton(this);
    //m_liveButton->addPixmap(Skin::pixmap(QLatin1String("live.png")), ImageButton::Active, ImageButton::Background);
    m_liveButton->addPixmap(Skin::pixmap(QLatin1String("live.png")), ImageButton::Disabled, ImageButton::Background);
    m_liveButton->setPreferredSize(20, 20);
    m_liveButton->setMaximumSize(m_liveButton->preferredSize());
    m_liveButton->setEnabled(false);
    m_liveButton->setCursor(Qt::ArrowCursor);
    m_liveButton->hide();

    m_volumeSlider = new VolumeSlider(Qt::Horizontal);
    m_volumeSlider->setObjectName("VolumeSlider");
    m_volumeSlider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_volumeSlider->setToolTipItem(new SliderToolTipItem(m_volumeSlider));
    m_volumeSlider->setCursor(Qt::ArrowCursor);

    connect(m_muteButton, SIGNAL(clicked(bool)), m_volumeSlider, SLOT(setMute(bool)));
    connect(m_volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(onVolumeLevelChanged(int)));

    m_timeLabel = new QLabelKillsWheelEvent;
    m_timeLabel->setObjectName("TimeLabel");
    m_timeLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_timeLabel->setStyleSheet("QLabel { background: transparent; color: rgb(63, 159, 216); }");

    QGraphicsProxyWidget *timeLabelProxyWidget = new QGraphicsProxyWidget(this);
    timeLabelProxyWidget->setWidget(m_timeLabel);

    QGraphicsLinearLayout *rightLayoutH = new QGraphicsLinearLayout(Qt::Horizontal);
    rightLayoutH->setContentsMargins(0, 0, 0, 0);
    rightLayoutH->setSpacing(3);
    rightLayoutH->addItem(timeLabelProxyWidget);
    rightLayoutH->setAlignment(timeLabelProxyWidget, Qt::AlignLeft | Qt::AlignVCenter);
    rightLayoutH->addItem(m_liveButton);
    rightLayoutH->setAlignment(m_liveButton, Qt::AlignCenter);
    rightLayoutH->addItem(m_muteButton);
    rightLayoutH->setAlignment(m_muteButton, Qt::AlignRight | Qt::AlignVCenter);

    QGraphicsLinearLayout *rightLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    rightLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    rightLayoutV->setContentsMargins(0, 3, 0, 0);
    rightLayoutV->setSpacing(0);
    rightLayoutV->addItem(m_volumeSlider);
    rightLayoutV->setAlignment(m_volumeSlider, Qt::AlignCenter);
    rightLayoutV->addStretch();
    rightLayoutV->addItem(rightLayoutH);
    rightLayoutV->setAlignment(rightLayoutH, Qt::AlignBottom);
    rightLayoutV->addStretch();


    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(5, 0, 5, 0);
    mainLayout->setSpacing(10);
    mainLayout->addItem(leftLayoutV);
    mainLayout->setAlignment(leftLayoutV, Qt::AlignLeft | Qt::AlignVCenter);
    mainLayout->addItem(m_timeSlider);
    mainLayout->setAlignment(m_timeSlider, Qt::AlignCenter);
    mainLayout->addItem(rightLayoutV);
    mainLayout->setAlignment(rightLayoutV, Qt::AlignRight | Qt::AlignVCenter);
    m_graphicsWidget->setLayout(mainLayout);


    QAction *playAction = new QAction(tr("Play / Pause"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    m_graphicsWidget->addAction(playAction);

    setVideoCamera(0);
}

NavigationItem::~NavigationItem()
{
    delete m_timeSlider;
    delete m_timeLabel;
}

QRectF NavigationItem::boundingRect() const
{
    return m_graphicsWidget->boundingRect();
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
    updatePeriodList(true);
}

void NavigationItem::removeReserveCamera(CLVideoCamera *camera)
{
    m_reserveCameras.remove(camera);

    updateActualCamera();
    updatePeriodList(true);
}

void NavigationItem::updateActualCamera()
{
    if (m_forcedCamera != NULL) {
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
    if(m_camera == camera)
        return;

    m_speedSlider->resetSpeed();
    m_timeSlider->resetSelectionRange();
    restoreInfoText();

    if (m_camera)
        disconnect(m_camera->getCamCamDisplay(), 0, this, 0);

    m_camera = camera;

    if (m_camera)
    {
        setVisible(true);

        connect(m_camera->getCamCamDisplay(), SIGNAL(liveMode(bool)), this, SLOT(onLiveModeChanged(bool)));

        QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
        setPlaying(!reader->onPause());
    }
    else
    {
        setVisible(false);
        m_timeSlider->setScalingFactor(0);
    }
}

void NavigationItem::onLiveModeChanged(bool value)
{
    if (value)
        m_speedSlider->resetSpeed();
}

void NavigationItem::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
        updateSlider();

    CLUnMovedInteractiveOpacityItem::timerEvent(event);
}

void NavigationItem::updateSlider()
{
    if (!m_camera)
        return;

    if (m_timeSlider->isMoving())
        return;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader *>(m_camera->getStreamreader());
    qint64 startTime = reader->startTime();
    qint64 endTime = reader->endTime();
    if (startTime != AV_NOPTS_VALUE && endTime != AV_NOPTS_VALUE)
    {
        m_timeSlider->setMinimumValue(startTime / 1000);
        m_timeSlider->setMaximumValue(endTime != DATETIME_NOW ? endTime / 1000 : DATETIME_NOW);
        if (m_timeSlider->minimumValue() == 0)
            m_timeLabel->setText(formatDuration(m_timeSlider->length() / 1000));
        else
            m_timeLabel->setText(QString());//###QDateTime::fromMSecsSinceEpoch(m_timeSlider->maximumValue()).toString(Qt::SystemLocaleShortDate));

        quint64 time = m_camera->getCurrentTime();
        if (time != AV_NOPTS_VALUE)
        {
            m_currentTime = time/1000;
            m_timeSlider->setCurrentValue(m_currentTime);
        }
        m_liveButton->setVisible(!reader->onPause() && m_camera->getCamCamDisplay()->isRealTimeSource());

        updatePeriodList(false);
    }
}

void NavigationItem::updatePeriodList(bool force)
{
    qint64 w = m_timeSlider->sliderRange() * 1000;
    qint64 t = m_timeSlider->viewPortPos() * 1000;
    if(t < 0)
        return; /* TODO: why? */

    if(!force && m_timePeriod.startTimeUSec <= t && t + w <= m_timePeriod.startTimeUSec + m_timePeriod.durationUSec)
        return;

    QnNetworkResourceList resources;
    QnVideoServerConnectionPtr connection;
    foreach(CLVideoCamera *camera, m_reserveCameras) {
        QnNetworkResourcePtr networkResource = qSharedPointerDynamicCast<QnNetworkResource>(camera->getDevice());
        if(networkResource.isNull())
            continue;

        resources.push_back(networkResource);

        if(!connection.isNull())
            continue;

        QnVideoServerPtr serverResource = qSharedPointerDynamicCast<QnVideoServer>(qnResPool->getResourceById(networkResource->getParentId()));
        if(serverResource.isNull())
            continue;

        QnVideoServerConnectionPtr serverConnection = serverResource->apiConnection();
        if(serverConnection.isNull())
            continue;

        connection = serverConnection;
    }

    /* Request interval no shorter than 1 hour. */
    const qint64 oneHour = 60ll * 60ll * 1000ll * 1000ll;
    if(w < oneHour)
        w = oneHour;

    /* It is important to update the stored interval BEFORE sending request to
     * the server. Request is blocking and starts an event loop, so we may get
     * into this method again. */
    m_timePeriod.startTimeUSec = t - w;
    m_timePeriod.durationUSec = w * 3;

    QnTimePeriodList timePeriods;
    if(!connection.isNull())
        m_timePeriodUpdater->update(connection, resources, m_timePeriod);
}

void NavigationItem::onTimePeriodUpdaterReady(const QnTimePeriodList &timePeriods)
{
    m_timeSlider->setTimePeriodList(timePeriods);
}

void NavigationItem::onValueChanged(qint64 time)
{
    if (m_currentTime == time)
        return;

    if (m_camera == 0)
        return;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    if (reader->isSkippingFrames())
        return;

    smartSeek(time);

    updatePeriodList(false);
}

void NavigationItem::smartSeek(qint64 timeMSec)
{
    //qDebug() << timeMSec;

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    if(m_timeSlider->isAtEnd()) {
        reader->jumpToPreviousFrame(DATETIME_NOW);

        m_liveButton->show();
    } else {
        timeMSec *= 1000;
        if (m_timeSlider->isMoving())
            reader->jumpTo(timeMSec, 0);
        else
            reader->jumpToPreviousFrame(timeMSec);

        m_liveButton->hide();
    }
}

void NavigationItem::pause()
{
    setActive(true);

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->pauseMedia();
    m_camera->getCamCamDisplay()->pauseAudio();
    m_liveButton->hide();

    reader->setSingleShotMode(true);
    //m_camera->getCamCamDisplay()->setSingleShotMode(true);
}

void NavigationItem::play()
{
    setActive(true);

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    if (reader->onPause() && reader->isRealTimeSource()) {
        reader->resumeMedia();
        reader->jumpToPreviousFrame(m_camera->getCurrentTime());
    }
    else {
        reader->resumeMedia();
    }

    if (!m_playing)
        m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void NavigationItem::rewindBackward()
{
    setActive(true);
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->jumpTo(0, true);
    //m_camera->streamJump(0);
    reader->resumeDataProcessors();
}

void NavigationItem::rewindForward()
{
    setActive(true);
    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());

    bool stopped = reader->onPause();
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

void NavigationItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    m_mouseOver = true;
    CLUnMovedInteractiveOpacityItem::hoverEnterEvent(e);
}

void NavigationItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    m_mouseOver = false;
    CLUnMovedInteractiveOpacityItem::hoverLeaveEvent(e);
}

void NavigationItem::onSliderPressed()
{
    Q_ASSERT(m_camera);

    QnAbstractArchiveReader *reader = static_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->setSingleShotMode(true);
    m_camera->getCamCamDisplay()->playAudio(false);
    setActive(true);
}

void NavigationItem::onSliderReleased()
{
    Q_ASSERT(m_camera);

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

            if (CLVideoWindowItem *vwi = m_camera->getVideoWindow())
            {
                if (restoreInfoTextData)
                {
                    restoreInfoTextData->timer.stop();
                }
                else
                {
                    restoreInfoTextData = new RestoreInfoTextData;
                    restoreInfoTextData->extraInfoText = vwi->extraInfoText();
                    restoreInfoTextData->timer.setSingleShot(true);
                    connect(&restoreInfoTextData->timer, SIGNAL(timeout()), this, SLOT(restoreInfoText()));
                }

                vwi->setExtraInfoText(!qFuzzyIsNull(newSpeed) ? tr("[ Speed: %1 ]").arg(tr("%1x").arg(newSpeed)) : tr("Paused"));

                restoreInfoTextData->timer.start(3000);
            }
        }
        else
        {
            m_speedSlider->resetSpeed();
        }
    }
}

void NavigationItem::onVolumeLevelChanged(int newVolumeLevel)
{
    m_muteButton->setChecked(m_volumeSlider->isMute());

    if (m_camera && m_camera->getVideoWindow())
    {
        CLVideoWindowItem *vwi = m_camera->getVideoWindow();

        if (restoreInfoTextData)
        {
            restoreInfoTextData->timer.stop();
        }
        else
        {
            restoreInfoTextData = new RestoreInfoTextData;
            restoreInfoTextData->extraInfoText = vwi->extraInfoText();
            restoreInfoTextData->timer.setSingleShot(true);
            connect(&restoreInfoTextData->timer, SIGNAL(timeout()), this, SLOT(restoreInfoText()));
        }

        vwi->setExtraInfoText(tr("[ Volume: %1 ]").arg(!m_volumeSlider->isMute() ? tr("%1%").arg(newVolumeLevel) : tr("Muted")));

        restoreInfoTextData->timer.start(3000);
    }
}

void NavigationItem::restoreInfoText()
{
    if (!restoreInfoTextData)
        return;

    if (m_camera && m_camera->getVideoWindow())
    {
        CLVideoWindowItem *vwi = m_camera->getVideoWindow();

        vwi->setExtraInfoText(restoreInfoTextData->extraInfoText);
    }

    delete restoreInfoTextData;
    restoreInfoTextData = 0;
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
    }
}

void NavigationItem::togglePlayPause()
{
    setPlaying(!m_playing);
}
