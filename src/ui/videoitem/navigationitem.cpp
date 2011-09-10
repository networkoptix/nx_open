#include "navigationitem.h"

#include <QtCore/QEvent>

#include <QtGui/QAction>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QGraphicsProxyWidget>
#include <QtGui/QLabel>

#include "timeslider.h"
#include "util.h"
#include "camera/camera.h"
#include "device_plugins/archive/abstract_archive_stream_reader.h"
#include "ui/videoitem/video_wnd_item.h"
#include "ui/widgets/imagebuttonitem.h"
#include "ui/widgets/speedslider.h"
#include "ui/widgets/volumeslider.h"
#include "ui/widgets/tooltipitem.h"

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
        Q_ASSERT(m_slider);
        setAcceptHoverEvents(true);
        setOpacity(0.75);
    }
#if 0
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        m_pos = event->pos();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {
        QPointF pos = event->pos();
        qreal shift = qreal(m_slider->maximum() - m_slider->minimum()) / m_slider->geometry().width() * (pos - m_pos).x();
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
        Q_ASSERT(m_slider);
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
        qint64 value = m_slider->currentValue() + (double)m_slider->sliderRange()/m_slider->width()*(pos - m_pos).x();
        m_slider->setCurrentValue(value);
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


NavigationItem::NavigationItem(QGraphicsItem */*parent*/) :
    CLUnMovedInteractiveOpacityItem(QString("name:)"), 0, 0.5, 0.95),
    m_camera(0), m_currentTime(0),
    m_playing(false),
    m_sliderIsmoving(true),
    m_mouseOver(false),
    m_active(false),
    restoreInfoTextData(0)
{
    m_graphicsWidget = new QGraphicsWidgetKillsWheelEvent(this);
    m_graphicsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_graphicsWidget->setCursor(Qt::ArrowCursor);
    m_graphicsWidget->setAutoFillBackground(true);

    QPalette palette = m_graphicsWidget->palette();
    palette.setColor(QPalette::Window, Qt::black);
    m_graphicsWidget->setPalette(palette);

    m_backwardButton = new ImageButtonItem(this);
    m_backwardButton->addPixmap(QPixmap(":/skin/rewind_backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_backwardButton->addPixmap(QPixmap(":/skin/rewind_backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_backwardButton->setPreferredSize(32, 18);
    m_backwardButton->setMaximumSize(m_backwardButton->preferredSize());

    m_stepBackwardButton = new ImageButtonItem(this);
    m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_stepBackwardButton->setPreferredSize(32, 18);
    m_stepBackwardButton->setMaximumSize(m_stepBackwardButton->preferredSize());

    m_playButton = new ImageButtonItem(this);
    m_playButton->addPixmap(QPixmap(":/skin/play_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_playButton->addPixmap(QPixmap(":/skin/play_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_playButton->setPreferredSize(32, 30);
    m_playButton->setMaximumSize(m_playButton->preferredSize());

    m_stepForwardButton = new ImageButtonItem(this);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_stepForwardButton->setPreferredSize(32, 18);
    m_stepForwardButton->setMaximumSize(m_stepForwardButton->preferredSize());

    m_forwardButton = new ImageButtonItem(this);
    m_forwardButton->addPixmap(QPixmap(":/skin/rewind_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_forwardButton->addPixmap(QPixmap(":/skin/rewind_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
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


    m_timeSlider = new TimeSlider;
    m_timeSlider->setObjectName("TimeSlider");
    m_timeSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_graphicsWidget->resize(m_timeSlider->size());
    m_timeSlider->setStyleSheet("QWidget { background: rgb(15, 15, 15); color:rgb(63, 159, 216); }");

    connect(m_timeSlider, SIGNAL(currentValueChanged(qint64)), this, SLOT(onValueChanged(qint64)));
    connect(m_timeSlider, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));
    connect(m_timeSlider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));

    QGraphicsProxyWidget *timeSliderProxyWidget = new QGraphicsProxyWidget(this);
    timeSliderProxyWidget->setAcceptHoverEvents(true);
    timeSliderProxyWidget->setWidget(m_timeSlider);
    m_timerId = startTimer(33);

    m_timeSliderToolTip = new TimeSliderToolTipItem(m_timeSlider, timeSliderProxyWidget);
    m_timeSliderToolTip->setVisible(false);


    m_muteButton = new ImageButtonItem(this);
    m_muteButton->setObjectName("MuteButton");
    m_muteButton->addPixmap(QPixmap(":/skin/unmute.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_muteButton->addPixmap(QPixmap(":/skin/mute.png"), ImageButtonItem::Active, ImageButtonItem::Checked);
    m_muteButton->setPreferredSize(20, 20);
    m_muteButton->setMaximumSize(m_muteButton->preferredSize());
    m_muteButton->setCheckable(true);
    m_muteButton->setChecked(m_volumeSlider->isMute());
    m_muteButton->setCursor(Qt::ArrowCursor);

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
    mainLayout->addItem(timeSliderProxyWidget);
    mainLayout->setAlignment(timeSliderProxyWidget, Qt::AlignCenter);
    mainLayout->addItem(rightLayoutV);
    mainLayout->setAlignment(rightLayoutV, Qt::AlignRight | Qt::AlignVCenter);
    m_graphicsWidget->setLayout(mainLayout);


    QAction *playAction = new QAction(tr("Play / Pause"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    m_graphicsWidget->addAction(playAction);

    setVideoCamera(0);

    setAcceptHoverEvents(true);
    setCursor(Qt::ArrowCursor);
}

NavigationItem::~NavigationItem()
{
    delete m_timeSlider;
    delete m_timeLabel;
}

QRectF NavigationItem::boundingRect() const
{
    return QRectF(0, 0, m_timeSlider->width(), m_timeSlider->height());
}

bool NavigationItem::isActive() const
{
    return m_active;
}

void NavigationItem::setActive(bool active)
{
    m_active = active;
}

void NavigationItem::setVideoCamera(CLVideoCamera *camera)
{
    if (m_camera == camera)
        return;

    m_speedSlider->resetSpeed();
    restoreInfoText();

    m_camera = camera;

    if (m_camera)
    {
        setVisible(true);
        m_timeSliderToolTip->setVisible(true);

        CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
        setPlaying(!reader->onPause());
    }
    else
    {
        setVisible(false);
        m_timeSliderToolTip->setVisible(false);
        m_timeSlider->setScalingFactor(0);
    }
}

void NavigationItem::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
        updateSlider();

    CLUnMovedInteractiveOpacityItem::timerEvent(event);
}

void NavigationItem::updateSliderToolTip(qint64 time)
{
    qreal x = 8 + (qreal(m_timeSlider->width() - 18)/m_timeSlider->sliderRange())*(m_timeSlider->currentValue() - m_timeSlider->viewPortPos());
    m_timeSliderToolTip->setPos(x, -m_timeSliderToolTip->boundingRect().height());
    m_timeSliderToolTip->setText(formatDuration(time / 1000));
}

void NavigationItem::updateSlider()
{
    if (!m_camera)
        return;

    if (m_timeSlider->isMoving())
        return;

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    qint64 length = reader->lengthMksec()/1000;
    m_timeSlider->setMaximumValue(length);


    quint64 time = m_camera->currentTime();
    if (time != AV_NOPTS_VALUE)
    {
        m_currentTime = time/1000;
        m_timeSlider->setCurrentValue(m_currentTime);

        updateSliderToolTip(m_currentTime);
        m_timeLabel->setText(formatDuration(length / 1000));
    }
}

void NavigationItem::onValueChanged(qint64 time)
{
    if (m_currentTime == time)
        return;

    updateSliderToolTip(time);

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    if (reader->isSkippingFrames())
        return;

    time *= 1000;
    if (m_timeSlider->isMoving())
        reader->jumpTo(time, true);
    else
        reader->jumpToPreviousFrame(time, true);

    m_camera->streamJump(time);
}

void NavigationItem::pause()
{
    setActive(true);

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->pause();
    m_camera->getCamCamDisplay()->playAudio(false);

    reader->setSingleShotMode(true);
    m_camera->getCamCamDisplay()->setSingleShotMode(true);
}

void NavigationItem::play()
{
    setActive(true);

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->setSingleShotMode(false);
    reader->resume();
    reader->resumeDataProcessors();

    m_camera->getCamCamDisplay()->playAudio(m_playing);
}

void NavigationItem::rewindBackward()
{
    setActive(true);
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->jumpTo(0, true);
    m_camera->streamJump(0);
    reader->resumeDataProcessors();
}

void NavigationItem::rewindForward()
{
    setActive(true);
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    bool stopped = reader->onPause();
    if (stopped)
        play();

    reader->jumpTo(reader->lengthMksec(), false);
    m_camera->streamJump(reader->lengthMksec());
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

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    if (!reader->isSkippingFrames() && m_camera->currentTime() != 0)
    {
        quint64 curr_time = m_camera->currentTime();

        if (reader->isSingleShotMode())
            m_camera->getCamCamDisplay()->playAudio(false);

        reader->previousFrame(curr_time);
        m_camera->streamJump(curr_time);
        m_camera->getCamCamDisplay()->setSingleShotMode(false);
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

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->nextFrame();
    reader->resume();
    m_camera->getCamCamDisplay()->setSingleShotMode(true);
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
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->setSingleShotMode(true);
    m_camera->getCamCamDisplay()->playAudio(false);
    m_sliderIsmoving = true;
    setActive(true);
}

void NavigationItem::onSliderReleased()
{
    setActive(true);
    m_sliderIsmoving = false;
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    quint64 time = m_timeSlider->currentValue();

    reader->previousFrame(time*1000);
    if (isPlaying())
    {
        reader->setSingleShotMode(false);
        m_camera->getCamCamDisplay()->playAudio(true);
    }
}

void NavigationItem::onSpeedChanged(float newSpeed)
{
    if (m_camera) {
        CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
        if (reader->isSpeedSupported(newSpeed))
        {
            reader->setSpeed(newSpeed);
            if (!qFuzzyIsNull(newSpeed))
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
        m_stepBackwardButton->addPixmap(QPixmap(":/skin/backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_stepBackwardButton->addPixmap(QPixmap(":/skin/backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_playButton->addPixmap(QPixmap(":/skin/pause_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_playButton->addPixmap(QPixmap(":/skin/pause_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_stepForwardButton->addPixmap(QPixmap(":/skin/forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_stepForwardButton->addPixmap(QPixmap(":/skin/forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_speedSlider->setPrecision(SpeedSlider::LowPrecision);

        play();
    } else {
        m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_playButton->addPixmap(QPixmap(":/skin/play_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_playButton->addPixmap(QPixmap(":/skin/play_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_speedSlider->setPrecision(SpeedSlider::HighPrecision);

        pause();
    }
}

void NavigationItem::togglePlayPause()
{
    setPlaying(!m_playing);
}
