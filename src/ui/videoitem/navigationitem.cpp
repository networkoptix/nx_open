#include "navigationitem.h"

#include <QtCore/QString>
#include <QtCore/QTimerEvent>
#include <QtGui/QAction>
#include <QtGui/QHBoxLayout>
#include <QtGui/QPushButton>
#include <QtGui/QLabel>
#include <QtGui/QGraphicsProxyWidget>

#include "timeslider.h"
#include "camera/camera.h"
#include "device_plugins/archive/abstract_archive_stream_reader.h"
#include "util.h"

class MyTextItem: public QGraphicsTextItem
{
public:
    MyTextItem(QGraphicsItem * parent = 0) : QGraphicsTextItem(parent) {}
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        static QPixmap pix(":/skin/time-window.png");
        QRectF rect = boundingRect();
        painter->drawPixmap((rect.width() - pix.width())/2, rect.y() + (rect.height() - (pix.height() - 10))/2, pix);
        QGraphicsTextItem::paint(painter, option, widget);
    }
};

NavigationWidget::NavigationWidget(QWidget *parent) :
    QWidget(parent)
{
    setObjectName("NavigationWidget");
    m_playing = false;
    m_layout = new QHBoxLayout;
    m_layout->setContentsMargins(10, 0, 10, 0);

    m_layout->setSpacing(10);

    m_backwardButton = new MyButton();
    m_backwardButton->setText("button");
    m_backwardButton->setFixedSize(54, 30);
    m_backwardButton->setPixmap(QPixmap(":/skin/backward_grey.png"));
    m_backwardButton->setPressedPixmap(QPixmap(":/skin/backward_blue.png"));

    m_playButton = new MyButton();
    m_playButton->setFixedSize(54, 51);
    m_playButton->setPixmap(QPixmap(":/skin/play_grey.png"));
    m_playButton->setPressedPixmap(QPixmap(":/skin/play_blue.png"));

    QAction *playAction = new QAction(tr("Play"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    this->addAction(playAction);

    m_forwardButton = new MyButton();
    m_forwardButton->setFixedSize(54, 30);
    m_forwardButton->setPixmap(QPixmap(":/skin/forward_grey.png"));
    m_forwardButton->setPressedPixmap(QPixmap(":/skin/forward_blue.png"));

    m_stepBackwardButton = new MyButton();
    m_stepBackwardButton->setFixedSize(54, 30);
    m_stepBackwardButton->setPixmap(QPixmap(":/skin/step_backward_grey.png"));
    m_stepBackwardButton->setPressedPixmap(QPixmap(":/skin/step_backward_blue.png"));
    m_stepBackwardButton->setEnabled(false);

    m_stepForwardButton = new MyButton();
    m_stepForwardButton->setFixedSize(54, 30);
    m_stepForwardButton->setPixmap(QPixmap(":/skin/step_forward_grey.png"));
    m_stepForwardButton->setPressedPixmap(QPixmap(":/skin/step_forward_blue.png"));
    m_stepForwardButton->setEnabled(false);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_stepBackwardButton);
    buttonLayout->addWidget(m_backwardButton);
    buttonLayout->addWidget(m_playButton);
    buttonLayout->addWidget(m_forwardButton);
    buttonLayout->addWidget(m_stepForwardButton);
    buttonLayout->setSpacing(2);

    connect(m_playButton, SIGNAL(clicked()), SLOT(togglePlayPause()));

    connect(m_backwardButton, SIGNAL(clicked()), SIGNAL(rewindBackward()));
    connect(m_forwardButton, SIGNAL(clicked()), SIGNAL(rewindForward()));

    connect(m_stepBackwardButton, SIGNAL(clicked()), SIGNAL(stepBackward()));
    connect(m_stepForwardButton, SIGNAL(clicked()), SIGNAL(stepForward()));

    m_slider = new TimeSlider;
    m_slider->setObjectName("TimeSlider");

    connect(m_playButton, SIGNAL(toggled()), SLOT(slot()));

    m_label = new QLabel;

    m_volumeWidget = new VolumeWidget;
    m_layout->addLayout(buttonLayout);
    m_layout->addWidget(m_slider);
    m_layout->addWidget(m_label);
    m_layout->addWidget(m_volumeWidget);

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

void NavigationWidget::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    if (m_playing) {

        m_playButton->setPixmap(QPixmap(":/skin/pause_grey.png"));
        m_playButton->setPressedPixmap(QPixmap(":/skin/pause_blue.png"));
        m_stepBackwardButton->setEnabled(false);
        m_stepForwardButton->setEnabled(false);

        emit play();
    } else {

        m_playButton->setPixmap(QPixmap(":/skin/play_grey.png"));
        m_playButton->setPressedPixmap(QPixmap(":/skin/play_blue.png"));
        m_stepBackwardButton->setEnabled(true);
        m_stepForwardButton->setEnabled(true);

        emit pause();
    }
}

void NavigationWidget::togglePlayPause()
{
    setPlaying(!m_playing);
}

NavigationItem::NavigationItem(QGraphicsItem */*parent*/) :
    CLUnMovedInteractiveOpacityItem(QString("name:)"), 0, 0.5, 0.95)
{
    m_proxy = new QGraphicsProxyWidget(this);
    m_widget = new NavigationWidget();
    m_widget->slider()->setStyleSheet("QWidget { background: rgb(15, 15, 15); color:rgb(63, 159, 216); }");
    m_widget->label()->setStyleSheet("QLabel { color: rgb(63, 159, 216); }");
    m_widget->setStyleSheet("QWidget { background: black; }");
    m_proxy->setWidget(m_widget);
    m_timerId = startTimer(33);
    connect(m_widget->slider(), SIGNAL(currentValueChanged(qint64)), SLOT(onValueChanged(qint64)));
    connect(m_widget->slider(), SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));
    connect(m_widget->slider(), SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));


    connect(m_widget, SIGNAL(pause()), SLOT(pause()));
    connect(m_widget, SIGNAL(play()), SLOT(play()));
    connect(m_widget, SIGNAL(rewindBackward()), SLOT(rewindBackward()));
    connect(m_widget, SIGNAL(rewindForward()), SLOT(rewindForward()));
    connect(m_widget, SIGNAL(stepBackward()), SLOT(stepBackward()));
    connect(m_widget, SIGNAL(stepForward()), SLOT(stepForward()));

    textItem = new MyTextItem(m_proxy);
    textItem->setOpacity(0.75);
    textItem->setDefaultTextColor(QColor(63, 159, 216));
    textItem->setVisible(false);

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
    setAcceptHoverEvents(true);
    m_proxy->setAcceptHoverEvents(true);
    m_widget->setMouseTracking(true);

    m_camera = camera;
    if (!camera) {
        setVisible(false);
        if (textItem)
            textItem->setVisible(false);
        m_widget->slider()->setScalingFactor(0);
    } else {
        setVisible(true);
        CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
        m_widget->setPlaying(!reader->onPause());
        textItem->setVisible(true);
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

    quint64 time;
    if (reader->isSingleShotMode() || reader->isSkippingFrames() || m_camera->currentTime() == 0)
        time = reader->currentTime();
    else
        time = m_camera->currentTime();

    m_currentTime = time/1000;
    m_widget->slider()->setCurrentValue(m_currentTime);

    qreal x = m_widget->slider()->x() + 8 + ((double)(m_widget->slider()->width() - 18)/(m_widget->slider()->sliderRange()))*(m_widget->slider()->currentValue() - m_widget->slider()->viewPortPos()); // fuck you!
    textItem->setPos(x - textItem->boundingRect().width()/2, -40);
    textItem->setPlainText(formatDuration(m_currentTime/1000));
    m_widget->label()->setText(formatDuration(length/1000));
}

void NavigationItem::onValueChanged(qint64 time)
{
    if (m_currentTime == time)
        return;

    qreal x = m_widget->slider()->x() + 8 + ((double)(m_widget->slider()->width() - 18)/(m_widget->slider()->sliderRange()))*(m_widget->slider()->currentValue() - m_widget->slider()->viewPortPos()); // fuck you!
    textItem->setPos(x - textItem->boundingRect().width()/2, -40);
    textItem->setPlainText(formatDuration(time/1000));

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
}

void NavigationItem::onSliderReleased()
{
    m_sliderIsmoving = false;
    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
    quint64 time = m_widget->slider()->currentValue();

    reader->previousFrame(time*1000);
    if (m_widget->isPlaying())
    {
        reader->setSingleShotMode(false);
        m_camera->getCamCamDisplay()->playAudio(true);
    }
}
