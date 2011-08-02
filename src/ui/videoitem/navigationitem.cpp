#include "navigationitem.h"

#include <QtCore/QString>
#include <QtCore/QTimerEvent>
#include <QtGui/QHBoxLayout>
#include <QtGui/QPushButton>
#include <QtGui/QLabel>
#include <QtGui/QGraphicsProxyWidget>

#include "timeslider.h"
#include "camera/camera.h"
#include "device_plugins/archive/abstract_archive_stream_reader.h"
#include "util.h"

void TimeLabel::setCurrentValue(qint64 value)
{
    m_currentValue = value;
    setText(formatDuration(m_currentValue/1000, m_maximumValue/1000));
}

void TimeLabel::setMaximumValue(qint64 value)
{
    m_maximumValue = value;
}

NavigationWidget::NavigationWidget(QWidget *parent) :
    QWidget(parent)
{
    setObjectName("NavigationWidget");
    m_playing = false;
    m_layout = new QHBoxLayout;
    m_layout->setMargin(0);
    m_layout->setSpacing(0);

    m_backwardButton = new QPushButton();
    m_backwardButton->setFixedSize(75, 50);
    m_backwardButton->setIcon(QIcon(":/skin/player/to_very_beginning.png"));
    m_backwardButton->setIconSize(QSize(50, 50));

    m_playButton = new QPushButton();
    m_playButton->setFixedSize(75, 50);
    m_playButton->setIcon(QIcon(":/skin/player/play.png"));
    m_playButton->setIconSize(QSize(50, 50));

    m_forwardButton = new QPushButton();
    m_forwardButton->setFixedSize(75, 50);
    m_forwardButton->setIcon(QIcon(":/skin/player/to_very_end.png"));
    m_forwardButton->setIconSize(QSize(50, 50));

    connect(m_playButton, SIGNAL(clicked()), SLOT(togglePlayPause()));

    connect(m_backwardButton, SIGNAL(clicked()), SLOT(backwardPressed()));
    connect(m_forwardButton, SIGNAL(clicked()), SLOT(forwardPressed()));

    m_slider = new TimeSlider;
    m_slider->setObjectName("TimeSlider");
    connect(m_playButton, SIGNAL(toggled()), SLOT(slot()));

    m_label = new TimeLabel;

    m_layout->addSpacerItem(new QSpacerItem(100, 10));
    m_layout->addWidget(m_backwardButton);
    m_layout->addWidget(m_playButton);
    m_layout->addWidget(m_forwardButton);
    m_layout->addWidget(m_slider);
    m_layout->addWidget(m_label);
    m_layout->addSpacerItem(new QSpacerItem(100, 10));
    setLayout(m_layout);
}

TimeSlider * NavigationWidget::slider() const
{
    return m_slider;
}

TimeLabel * NavigationWidget::label() const
{
    return m_label;
}

void NavigationWidget::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    if (m_playing) {

        m_playButton->setIcon(QIcon(":/skin/player/pause.png"));
        m_backwardButton->setIcon(QIcon(":/skin/player/to_very_beginning.png"));
        m_forwardButton->setIcon(QIcon(":/skin/player/to_very_end.png"));

        emit play();
    } else {

        m_playButton->setIcon(QIcon(":/skin/player/play.png"));
        m_backwardButton->setIcon(QIcon(":/skin/player/frame_backward.png"));
        m_forwardButton->setIcon(QIcon(":/skin/player/frame_forward.png"));

        emit pause();
    }
}

void NavigationWidget::togglePlayPause()
{
    setPlaying(!m_playing);
}

void NavigationWidget::backwardPressed()
{
    if (playing())
        emit rewindBackward();
    else
        emit stepBackward();
}

void NavigationWidget::forwardPressed()
{
    if (playing())
        emit rewindForward();
    else
        emit stepForward();
}

NavigationItem::NavigationItem(QGraphicsItem */*parent*/) :
    CLUnMovedInteractiveOpacityItem(QString("name:)"), 0, 0.5, 0.5)
{
    m_proxy = new QGraphicsProxyWidget(this);
    m_widget = new NavigationWidget();
    m_proxy->installEventFilter(this);
    m_widget->slider()->setStyleSheet("QWidget { background: rgb(15, 15, 15); color:rgb(63, 159, 216); }");
    m_widget->label()->setStyleSheet("QLabel { font-size: 30px; color: rgb(63, 159, 216); }");
    m_widget->setStyleSheet("QWidget { background: black; }");
    m_proxy->setWidget(m_widget);
    m_timerId = startTimer(33);
    connect(m_widget->slider(), SIGNAL(currentValueChanged(qint64)), SLOT(onValueChanged(qint64)));

    connect(m_widget, SIGNAL(pause()), SLOT(pause()));
    connect(m_widget, SIGNAL(play()), SLOT(play()));
    connect(m_widget, SIGNAL(rewindBackward()), SLOT(rewindBackward()));
    connect(m_widget, SIGNAL(rewindForward()), SLOT(rewindForward()));
    connect(m_widget, SIGNAL(stepBackward()), SLOT(stepBackward()));
    connect(m_widget, SIGNAL(stepForward()), SLOT(stepForward()));

    setVideoCamera(0);
    m_sliderIsmoving = true;
    m_mouseOver = false;
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

void NavigationItem::setVideoCamera(CLVideoCamera *camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;
    if (!camera) {
        setVisible(false);
    } else {
        setVisible(true);
        CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
        m_widget->setPlaying(!reader->onPause());
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

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    qint64 length = reader->lengthMksec()/1000;
    m_widget->slider()->setMaximumValue(length);
    m_widget->label()->setMaximumValue(length);

    quint64 time;
    if (reader->isSingleShotMode() || reader->isSkippingFrames() || m_camera->currentTime() == 0)
        time = reader->currentTime();
    else
        time = m_camera->currentTime();

    m_currentTime = time/1000;
    m_widget->slider()->setCurrentValue(time/1000);
    m_widget->label()->setCurrentValue(time/1000);
}

void NavigationItem::onValueChanged(qint64 time)
{
    if (m_currentTime == time)
        return;

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    if (reader->isSkippingFrames())
        return;

    time *= 1000;
    if (m_sliderIsmoving)
        reader->jumpTo(time, true);
    else
        reader->jumpToPreviousFrame(time, true);

    m_camera->streamJump(time);
}

void NavigationItem::pause()
{
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->pause();
    m_camera->getCamCamDisplay()->playAudio(false);

    reader->setSingleShotMode(true);
    m_camera->getCamCamDisplay()->setSingleShotMode(true);
}

void NavigationItem::play()
{
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->setSingleShotMode(false);
    reader->resume();
    reader->resumeDataProcessors();

    m_camera->getCamCamDisplay()->playAudio(true);
}

void NavigationItem::rewindBackward()
{
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->jumpTo(0, true);
    m_camera->streamJump(0);
    reader->resumeDataProcessors();
}

void NavigationItem::rewindForward()
{
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->jumpTo(reader->lengthMksec(), true);
    m_camera->streamJump(reader->lengthMksec());
    reader->resumeDataProcessors();
}

void NavigationItem::stepBackward()
{
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
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    reader->nextFrame();
    reader->resume();
    m_camera->getCamCamDisplay()->setSingleShotMode(true);
}

void NavigationItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_mouseOver = true;
}

void NavigationItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_mouseOver = false;
}

