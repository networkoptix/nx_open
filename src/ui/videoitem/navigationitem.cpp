#include "navigationitem.h"

#include <QtCore/QString>
#include <QtCore/QTimerEvent>
#include <QtGui/QAction>
#include <QtGui/QHBoxLayout>
#include <QtGui/QPushButton>
#include <QtGui/QLabel>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QGraphicsProxyWidget>

#include "timeslider.h"
#include "camera/camera.h"
#include "device_plugins/archive/abstract_archive_stream_reader.h"
#include "util.h"
#include "ui/videoitem/video_wnd_item.h"
#include "ui/widgets/imagebuttonitem.h"
#include "ui/widgets/speedslider.h"
#include "ui/widgets/volumeslider.h"
#include "ui/widgets/tooltipitem.h"

class TimeSliderToolTipItem: public StyledToolTipItem
{
public:
    TimeSliderToolTipItem(NavigationWidget *widget, QGraphicsItem *parent = 0)
        : StyledToolTipItem(parent), m_widget(widget)
    {
        Q_ASSERT(m_widget);
        setAcceptHoverEvents(true);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        m_pos = mapToScene(event->pos());
        m_widget->slider()->setMoving(true);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {
        QPointF pos = mapToScene(event->pos());
        qint64 value = m_widget->slider()->currentValue() + (double)m_widget->slider()->sliderRange()/m_widget->slider()->width()*(pos - m_pos).x();
        m_widget->slider()->setCurrentValue(value);
        m_pos = pos;
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
    {
        Q_UNUSED(event)
        m_widget->slider()->setMoving(false);
    }

private:
    NavigationWidget *const m_widget;
    QPointF m_pos;
};


NavigationWidget::NavigationWidget(QWidget *parent) :
    QWidget(parent)
{
    setObjectName("NavigationWidget");

    m_slider = new TimeSlider;
    m_slider->setObjectName("TimeSlider");

    m_label = new MyLable;

    m_layout = new QHBoxLayout;
    m_layout->setContentsMargins(10, 0, 10, 0);
    m_layout->setSpacing(10);
    m_layout->addWidget(m_slider);
    m_layout->addWidget(m_label);
    setLayout(m_layout);
}

TimeSlider * NavigationWidget::slider() const
{
    return m_slider;
}

QLabel * NavigationWidget::label() const
{
    return m_label;
}

NavigationItem::NavigationItem(QGraphicsItem */*parent*/) :
    CLUnMovedInteractiveOpacityItem(QString("name:)"), 0, 0.5, 0.95),
    m_camera(0), m_currentTime(0),
    restoreInfoTextData(0)
{
    m_playing = false;

    m_graphicsWidget = new QGraphicsWidget(this);
    m_graphicsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_graphicsWidget->setAutoFillBackground(true);
    QPalette pal = m_graphicsWidget->palette();
    pal.setColor(QPalette::Window, Qt::black);
    m_graphicsWidget->setPalette(pal);

    m_backwardButton = new ImageButtonItem(this);
    m_backwardButton->addPixmap(QPixmap(":/skin/rewind_backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_backwardButton->addPixmap(QPixmap(":/skin/rewind_backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_backwardButton->setPreferredSize(32, 18);

    m_stepBackwardButton = new ImageButtonItem(this);
    m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_stepBackwardButton->setPreferredSize(32, 18);

    m_playButton = new ImageButtonItem(this);
    m_playButton->addPixmap(QPixmap(":/skin/play_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_playButton->addPixmap(QPixmap(":/skin/play_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_playButton->setPreferredSize(32, 30);

    m_stepForwardButton = new ImageButtonItem(this);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_stepForwardButton->setPreferredSize(32, 18);

    m_forwardButton = new ImageButtonItem(this);
    m_forwardButton->addPixmap(QPixmap(":/skin/rewind_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_forwardButton->addPixmap(QPixmap(":/skin/rewind_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_forwardButton->setPreferredSize(32, 18);

    connect(m_backwardButton, SIGNAL(clicked()), this, SLOT(rewindBackward()));
    connect(m_stepBackwardButton, SIGNAL(clicked()), this, SLOT(stepBackward()));
    connect(m_playButton, SIGNAL(clicked()), this, SLOT(togglePlayPause()));
    connect(m_stepForwardButton, SIGNAL(clicked(), this), SLOT(stepForward()));
    connect(m_forwardButton, SIGNAL(clicked()), this, SLOT(rewindForward()));

    QAction *playAction = new QAction(tr("Play"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    m_graphicsWidget->addAction(playAction);

    QGraphicsLinearLayout *buttonsLayout = new QGraphicsLinearLayout;
    buttonsLayout->setSpacing(2);
    buttonsLayout->addItem(m_backwardButton);
    buttonsLayout->setAlignment(m_backwardButton, Qt::AlignHCenter | Qt::AlignBottom);
    buttonsLayout->addItem(m_stepBackwardButton);
    buttonsLayout->setAlignment(m_stepBackwardButton, Qt::AlignHCenter | Qt::AlignBottom);
    buttonsLayout->addItem(m_playButton);
    buttonsLayout->setAlignment(m_playButton, Qt::AlignHCenter | Qt::AlignBottom);
    buttonsLayout->addItem(m_stepForwardButton);
    buttonsLayout->setAlignment(m_stepForwardButton, Qt::AlignHCenter | Qt::AlignBottom);
    buttonsLayout->addItem(m_forwardButton);
    buttonsLayout->setAlignment(m_forwardButton, Qt::AlignHCenter | Qt::AlignBottom);

    m_speedSlider = new SpeedSlider(Qt::Horizontal, this);
    m_speedSlider->setObjectName("SpeedSlider");
    m_speedSlider->setCursor(Qt::ArrowCursor);

    connect(m_speedSlider, SIGNAL(speedChanged(float)), this, SLOT(onSpeedChanged(float)));

    QGraphicsLinearLayout *linearLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    linearLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    linearLayoutV->setContentsMargins(0, 0, 0, 0);
    linearLayoutV->setSpacing(0);
    linearLayoutV->addItem(m_speedSlider);
    linearLayoutV->setAlignment(m_speedSlider, Qt::AlignTop);
    linearLayoutV->addItem(buttonsLayout);
    linearLayoutV->setAlignment(buttonsLayout, Qt::AlignCenter);

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout;
    mainLayout->setContentsMargins(5, 0, 5, 0);
    mainLayout->addItem(linearLayoutV);
    mainLayout->setAlignment(linearLayoutV, Qt::AlignLeft | Qt::AlignVCenter);
    m_graphicsWidget->setLayout(mainLayout);

    m_proxy = new QGraphicsProxyWidget(this);
    mainLayout->addItem(m_proxy);
    m_widget = new NavigationWidget();
    m_graphicsWidget->resize(m_widget->size());
    m_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_widget->slider()->setStyleSheet("QWidget { background: rgb(15, 15, 15); color:rgb(63, 159, 216); }");
    m_widget->label()->setStyleSheet("QLabel { color: rgb(63, 159, 216); }");
    m_widget->setStyleSheet("QWidget { background: black; }");
    m_proxy->setWidget(m_widget);
    m_timerId = startTimer(33);

    connect(m_widget->slider(), SIGNAL(currentValueChanged(qint64)), SLOT(onValueChanged(qint64)));
    connect(m_widget->slider(), SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));
    connect(m_widget->slider(), SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));

    m_muteButton = new ImageButtonItem(this);
    m_muteButton->setObjectName("MuteButton");
    m_muteButton->addPixmap(QPixmap(":/skin/unmute.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_muteButton->addPixmap(QPixmap(":/skin/mute.png"), ImageButtonItem::Active, ImageButtonItem::Checked);
    m_muteButton->setPreferredSize(20, 20);
    m_muteButton->setCheckable(true);
    m_muteButton->setChecked(m_volumeSlider->isMute());
    m_muteButton->setCursor(Qt::ArrowCursor);

    m_volumeSlider = new VolumeSlider(Qt::Vertical);
    m_volumeSlider->setObjectName("VolumeSlider");
    m_volumeSlider->setCursor(Qt::ArrowCursor);

    connect(m_muteButton, SIGNAL(clicked(bool)), m_volumeSlider, SLOT(setMute(bool)));
    connect(m_volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(onVolumeLevelChanged(int)));

    mainLayout->addItem(m_muteButton);
    mainLayout->setAlignment(m_muteButton, Qt::AlignRight | Qt::AlignBottom);
    mainLayout->addItem(m_volumeSlider);
    mainLayout->setAlignment(m_volumeSlider, Qt::AlignCenter);

    connect(m_widget, SIGNAL(pause()), SLOT(pause()));
    connect(m_widget, SIGNAL(play()), SLOT(play()));
    connect(m_widget, SIGNAL(rewindBackward()), SLOT(rewindBackward()));
    connect(m_widget, SIGNAL(rewindForward()), SLOT(rewindForward()));
    connect(m_widget, SIGNAL(stepBackward()), SLOT(stepBackward()));
    connect(m_widget, SIGNAL(stepForward()), SLOT(stepForward()));

    m_timesliderToolTip = new TimeSliderToolTipItem(m_widget, m_proxy);
    m_timesliderToolTip->setOpacity(0.75);
    m_timesliderToolTip->setVisible(false);

    setVideoCamera(0);

    m_sliderIsmoving = true;
    m_mouseOver = false;
    m_active = false;

    setCursor(Qt::ArrowCursor);
}

NavigationItem::~NavigationItem()
{
    delete m_widget;
}

QWidget *NavigationItem::navigationWidget()
{
    return m_widget;
}

QRectF NavigationItem::boundingRect() const
{
    return QRectF(0, 0, m_widget->width(), m_widget->height());
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

    setAcceptHoverEvents(true);
    m_proxy->setAcceptHoverEvents(true);
    m_widget->setMouseTracking(true);

    m_speedSlider->resetSpeed();
    restoreInfoText();

    m_camera = camera;

    if (m_camera)
    {
        setVisible(true);
        m_timesliderToolTip->setVisible(true);

        CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
        setPlaying(!reader->onPause());
    }
    else
    {
        setVisible(false);
        m_timesliderToolTip->setVisible(false);
        m_widget->slider()->setScalingFactor(0);
    }
}

void NavigationItem::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
        updateSlider();
}

void NavigationItem::updateSlider()
{
    if (!m_camera)
        return;

    if (m_widget->slider()->isMoving())
        return;

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    qint64 length = reader->lengthMksec()/1000;
    m_widget->slider()->setMaximumValue(length);


    quint64 time = m_camera->currentTime();
    if (time != AV_NOPTS_VALUE)
    {
        m_currentTime = time/1000;
        m_widget->slider()->setCurrentValue(m_currentTime);

        qreal x = m_widget->slider()->x() + 8 + ((double)(m_widget->slider()->width() - 18)/(m_widget->slider()->sliderRange()))*(m_widget->slider()->currentValue() - m_widget->slider()->viewPortPos()); // fuck you!
        m_timesliderToolTip->setPos(x, -40);
        m_timesliderToolTip->setText(formatDuration(m_currentTime / 1000));
        m_widget->label()->setText(formatDuration(length/1000));
    }
}

void NavigationItem::onValueChanged(qint64 time)
{
    if (m_currentTime == time)
        return;

    qreal x = m_widget->slider()->x() + 8 + ((double)(m_widget->slider()->width() - 18)/(m_widget->slider()->sliderRange()))*(m_widget->slider()->currentValue() - m_widget->slider()->viewPortPos()); // fuck you!
    m_timesliderToolTip->setPos(x, -40);
    m_timesliderToolTip->setText(formatDuration(time / 1000));

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    if (reader->isSkippingFrames())
        return;

    time *= 1000;
    if (m_widget->slider()->isMoving())
        reader->jumpTo(time, true);
    else
        reader->jumpToPreviousFrame(time, true);

    m_camera->streamJump(time);
}

void NavigationItem::pause()
{
    setActive(true);

    m_speedSlider->resetSpeed();

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->pause();
    m_camera->getCamCamDisplay()->playAudio(false);

    reader->setSingleShotMode(true);
    m_camera->getCamCamDisplay()->setSingleShotMode(true);
}

void NavigationItem::play()
{
    setActive(true);

    m_speedSlider->resetSpeed();

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    reader->setSingleShotMode(false);
    reader->resume();
    reader->resumeDataProcessors();

    m_camera->getCamCamDisplay()->playAudio(true);
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


void NavigationItem::wheelEvent(QGraphicsSceneWheelEvent *)
{
    cl_log.log("wheelEvent", cl_logALWAYS);
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
    quint64 time = m_widget->slider()->currentValue();

    reader->previousFrame(time*1000);
    if (isPlaying())
    {
        reader->setSingleShotMode(false);
        m_camera->getCamCamDisplay()->playAudio(true);
    }
}

void NavigationItem::onSpeedChanged(float newSpeed)
{
    if (isPlaying() && m_camera) {
        CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
        if (reader->isSpeedSupported(newSpeed))
        {
            reader->setSpeed(newSpeed);

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

                vwi->setExtraInfoText(tr("[ Speed: %1x ]").arg(newSpeed));

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

        vwi->setExtraInfoText(tr("[ Volume: %1 ]").arg(!m_volumeSlider->isMute() ? QString::number(newVolumeLevel) + QLatin1Char('%') : tr("Muted")));

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

        play();
    } else {
        m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_playButton->addPixmap(QPixmap(":/skin/play_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_playButton->addPixmap(QPixmap(":/skin/play_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);

        pause();
    }
}

void NavigationItem::togglePlayPause()
{
    setPlaying(!m_playing);
}
