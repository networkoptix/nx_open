#include "navigationitem.h"

#include <QtCore/QString>
#include <QtCore/QTimerEvent>
#include <QtGui/QAction>
#include <QtGui/QHBoxLayout>
#include <QtGui/QPushButton>
#include <QtGui/QLabel>
#include <QtGui/QGraphicsProxyWidget>
#include <QtGui/QGraphicsLinearLayout>

#include "timeslider.h"
#include "camera/camera.h"
#include "device_plugins/archive/abstract_archive_stream_reader.h"
#include "util.h"
#include "../widgets/imagebuttonitem.h"

class MyTextItem: public QGraphicsTextItem
{
public:
    MyTextItem(QGraphicsItem * parent = 0) : QGraphicsTextItem(parent), m_widget(0) {}

    void setNavigationWidget(NavigationWidget *widget) { m_widget = widget; }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        static QPixmap pix(":/skin/time-window.png");
        QRectF rect = boundingRect();
        painter->drawPixmap((rect.width() - pix.width())/2, rect.y() + (rect.height() - (pix.height() - 10))/2, pix);
        QGraphicsTextItem::paint(painter, option, widget);
    }
    void mouseMoveEvent ( QGraphicsSceneMouseEvent * event )
    {
        if (!m_widget)
            return;

        QPointF pos = mapToScene(event->pos());
        qint64 value = m_widget->slider()->currentValue() + (double)m_widget->slider()->sliderRange()/m_widget->slider()->width()*(pos - m_pos).x();
        m_widget->slider()->setCurrentValue(value);
        m_pos = pos;
    }

    void mousePressEvent ( QGraphicsSceneMouseEvent * event )
    {
        m_pos = mapToScene(event->pos());
        m_widget->slider()->setMoving(true);
    }

    void mouseReleaseEvent ( QGraphicsSceneMouseEvent * /*event*/ )
    {
        m_widget->slider()->setMoving(false);
    }

private:
    NavigationWidget *m_widget;
    QPointF m_pos;
};

NavigationWidget::NavigationWidget(QWidget *parent) :
    QWidget(parent)
{
    setObjectName("NavigationWidget");
    m_layout = new QHBoxLayout;
    m_layout->setContentsMargins(10, 0, 10, 0);

    m_layout->setSpacing(10);

    m_slider = new TimeSlider;
    m_slider->setObjectName("TimeSlider");

    m_label = new QLabel;

    m_volumeWidget = new VolumeWidget;
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

NavigationItem::NavigationItem(QGraphicsItem */*parent*/) :
    CLUnMovedInteractiveOpacityItem(QString("name:)"), 0, 0.5, 0.95)
{
    m_playing = false;
    m_ignoreWheel = false;

    m_graphicsWidget = new QGraphicsWidget(this);
    m_graphicsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_graphicsWidget->setAutoFillBackground(true);
    QPalette pal = m_graphicsWidget->palette();
    pal.setColor(QPalette::Window, Qt::black);
    m_graphicsWidget->setPalette(pal);

    m_stepBackwardButton = new ImageButtonItem;
    m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_stepBackwardButton->addPixmap(QPixmap(":/skin/step_backward_disabled.png"), ImageButtonItem::Disabled, ImageButtonItem::Background);
    m_stepBackwardButton->setMinimumSize(54, 30);
    m_stepBackwardButton->setMaximumSize(54, 30);

    m_backwardButton = new ImageButtonItem;
    m_backwardButton->addPixmap(QPixmap(":/skin/backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_backwardButton->addPixmap(QPixmap(":/skin/backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_backwardButton->setMinimumSize(54, 30);
    m_backwardButton->setMaximumSize(54, 30);

    m_playButton = new ImageButtonItem;
    m_playButton->addPixmap(QPixmap(":/skin/play_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_playButton->addPixmap(QPixmap(":/skin/play_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_playButton->setMinimumSize(54, 51);
    m_playButton->setMaximumSize(54, 51);

    QAction *playAction = new QAction(tr("Play"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    m_graphicsWidget->addAction(playAction);

    m_forwardButton = new ImageButtonItem;
    m_forwardButton->addPixmap(QPixmap(":/skin/forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_forwardButton->addPixmap(QPixmap(":/skin/forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_forwardButton->setMinimumSize(54, 30);
    m_forwardButton->setMaximumSize(54, 30);

    m_stepForwardButton = new ImageButtonItem;
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_disabled.png"), ImageButtonItem::Disabled, ImageButtonItem::Background);
    m_stepForwardButton->setMinimumSize(54, 30);
    m_stepForwardButton->setMaximumSize(54, 30);

    QGraphicsLinearLayout *linearLayout = new QGraphicsLinearLayout;
    linearLayout->setContentsMargins(0,0,0,0);
    linearLayout->addItem(m_stepBackwardButton);
    linearLayout->setAlignment(m_stepBackwardButton, Qt::AlignHCenter | Qt::AlignVCenter);
    linearLayout->addItem(m_backwardButton);
    linearLayout->setAlignment(m_backwardButton, Qt::AlignHCenter | Qt::AlignVCenter);
    linearLayout->addItem(m_playButton);
    linearLayout->setAlignment(m_playButton, Qt::AlignHCenter | Qt::AlignVCenter);
    linearLayout->addItem(m_forwardButton);
    linearLayout->setAlignment(m_forwardButton, Qt::AlignHCenter | Qt::AlignVCenter);
    linearLayout->addItem(m_stepForwardButton);
    linearLayout->setAlignment(m_stepForwardButton, Qt::AlignHCenter | Qt::AlignVCenter);
    m_graphicsWidget->setLayout(linearLayout);

    connect(m_stepBackwardButton, SIGNAL(clicked()), SLOT(stepBackward()));
    connect(m_backwardButton, SIGNAL(clicked()), SLOT(rewindBackward()));
    connect(m_playButton, SIGNAL(clicked()), SLOT(togglePlayPause()));
    connect(m_forwardButton, SIGNAL(clicked()), SLOT(rewindForward()));
    connect(m_stepForwardButton, SIGNAL(clicked()), SLOT(stepForward()));

    m_proxy = new QGraphicsProxyWidget();
    linearLayout->addItem(m_proxy);
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
    textItem->setNavigationWidget(m_widget);

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
        setPlaying(!reader->onPause());
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
    tim.stop();
    CLUnMovedInteractiveOpacityItem::hoverEnterEvent(e);
}

void NavigationItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    m_ignoreWheel = true;
    tim.singleShot(500, this, SLOT(resetHover()));
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
    if (isPlaying())
    {
        reader->setSingleShotMode(false);
        m_camera->getCamCamDisplay()->playAudio(true);
    }
}

void NavigationItem::resetHover()
{
    m_mouseOver = false;
    m_ignoreWheel = false;
}

void NavigationItem::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    if (m_playing) {

        m_playButton->addPixmap(QPixmap(":/skin/pause_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_playButton->addPixmap(QPixmap(":/skin/pause_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
        m_stepBackwardButton->setEnabled(false);
        m_stepForwardButton->setEnabled(false);

        play();
    } else {

        m_playButton->addPixmap(QPixmap(":/skin/play_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
        m_playButton->addPixmap(QPixmap(":/skin/play_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
        m_stepBackwardButton->setEnabled(true);
        m_stepForwardButton->setEnabled(true);

        pause();
    }
}

void NavigationItem::togglePlayPause()
{
    setPlaying(!m_playing);
}
