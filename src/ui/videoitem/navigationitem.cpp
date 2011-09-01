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
#include "ui/widgets/imagebuttonitem.h"
#include "ui/widgets/speedwidget.h"
#include "ui/widgets/volumewidget.h"

class MyTextItem: public QGraphicsItem
{
public:

    static int const extra_x = 20;
    static int const extra_y = 10;

    MyTextItem(QGraphicsItem* parent) :
      QGraphicsItem(parent),
          m_widget(0)
      {
          setAcceptHoverEvents(true);
      }

    void setNavigationWidget(NavigationWidget *widget) { m_widget = widget; }

    void setText(const QString& text)
    {
        m_text = text;
    }

    QRectF smallRect() const
    {
        return QRectF(0,0,50,30);
    }



    QRectF boundingRect() const
    {
        return smallRect().adjusted(-extra_x,-extra_y,extra_x,extra_y);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        static QPixmap pix(":/skin/time-window.png");
        QRectF rect = smallRect();

        painter->drawPixmap( (smallRect().width() - pix.width())/2, 0, pix);

        painter->setPen(QColor(63, 159, 216));
        painter->setFont(m_font); // default font
        painter->drawText(0, 0, smallRect().width(), smallRect().height(), Qt::AlignCenter, m_text);
        //painter->fillRect(boundingRect(), QColor(255,0,0,128));
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


    void wheelEvent ( QGraphicsSceneWheelEvent * event )
    {

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
    QString m_text;
    QFont m_font;
};

NavigationWidget::NavigationWidget(QWidget *parent) :
    QWidget(parent)
{
    setObjectName("NavigationWidget");

    m_slider = new TimeSlider;
    m_slider->setObjectName("TimeSlider");

    m_label = new MyLable;

    m_volumeWidget = new VolumeWidget(this);
    m_volumeWidget->setObjectName("VolumeWidget");

    m_layout = new QHBoxLayout;
    m_layout->setContentsMargins(10, 0, 10, 0);
    m_layout->setSpacing(10);
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
    CLUnMovedInteractiveOpacityItem(QString("name:)"), 0, 0.5, 0.95),
    m_camera(0), m_currentTime(0)
{
    m_playing = false;

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
    m_stepBackwardButton->setMinimumSize(48, 27);
    m_stepBackwardButton->setMaximumSize(48, 27);

    m_backwardButton = new ImageButtonItem;
    m_backwardButton->addPixmap(QPixmap(":/skin/backward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_backwardButton->addPixmap(QPixmap(":/skin/backward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_backwardButton->setMinimumSize(48, 27);
    m_backwardButton->setMaximumSize(48, 27);

    m_playButton = new ImageButtonItem;
    m_playButton->addPixmap(QPixmap(":/skin/play_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_playButton->addPixmap(QPixmap(":/skin/play_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_playButton->setMinimumSize(48, 45);
    m_playButton->setMaximumSize(48, 45);

    QAction *playAction = new QAction(tr("Play"), m_playButton);
    playAction->setShortcut(tr("Space"));
    playAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(playAction, SIGNAL(triggered()), m_playButton, SLOT(click()));
    m_graphicsWidget->addAction(playAction);

    m_forwardButton = new ImageButtonItem;
    m_forwardButton->addPixmap(QPixmap(":/skin/forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_forwardButton->addPixmap(QPixmap(":/skin/forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_forwardButton->setMinimumSize(48, 27);
    m_forwardButton->setMaximumSize(48, 27);

    m_stepForwardButton = new ImageButtonItem;
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_grey.png"), ImageButtonItem::Active, ImageButtonItem::Background);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_blue.png"), ImageButtonItem::Active, ImageButtonItem::Hovered);
    m_stepForwardButton->addPixmap(QPixmap(":/skin/step_forward_disabled.png"), ImageButtonItem::Disabled, ImageButtonItem::Background);
    m_stepForwardButton->setMinimumSize(48, 27);
    m_stepForwardButton->setMaximumSize(48, 27);

    QGraphicsLinearLayout *buttonsLayout = new QGraphicsLinearLayout;
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->addItem(m_stepBackwardButton);
    buttonsLayout->setAlignment(m_stepBackwardButton, Qt::AlignHCenter | Qt::AlignVCenter);
    buttonsLayout->addItem(m_backwardButton);
    buttonsLayout->setAlignment(m_backwardButton, Qt::AlignHCenter | Qt::AlignVCenter);
    buttonsLayout->addItem(m_playButton);
    buttonsLayout->setAlignment(m_playButton, Qt::AlignHCenter | Qt::AlignVCenter);
    buttonsLayout->addItem(m_forwardButton);
    buttonsLayout->setAlignment(m_forwardButton, Qt::AlignHCenter | Qt::AlignVCenter);
    buttonsLayout->addItem(m_stepForwardButton);
    buttonsLayout->setAlignment(m_stepForwardButton, Qt::AlignHCenter | Qt::AlignVCenter);

    m_speedWidget = new SpeedWidget;
    m_speedWidget->setObjectName("SpeedWidget");
    connect(m_speedWidget, SIGNAL(speedChanged(float)), this, SLOT(onSpeedChanged(float)));

    QGraphicsProxyWidget *speedProxyWidget = new QGraphicsProxyWidget(this);
    speedProxyWidget->setWidget(m_speedWidget);

    QGraphicsLinearLayout *linearLayoutV = new QGraphicsLinearLayout(Qt::Vertical);
    linearLayoutV->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    linearLayoutV->setContentsMargins(0, 0, 0, 0);
    linearLayoutV->setSpacing(0);
    linearLayoutV->addItem(buttonsLayout);
    linearLayoutV->setAlignment(buttonsLayout, Qt::AlignCenter);
    linearLayoutV->addItem(speedProxyWidget);
    linearLayoutV->setAlignment(speedProxyWidget, Qt::AlignTop);

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addItem(linearLayoutV);
    mainLayout->setAlignment(linearLayoutV, Qt::AlignLeft | Qt::AlignVCenter);
    m_graphicsWidget->setLayout(mainLayout);

    connect(m_stepBackwardButton, SIGNAL(clicked()), SLOT(stepBackward()));
    connect(m_backwardButton, SIGNAL(clicked()), SLOT(rewindBackward()));
    connect(m_playButton, SIGNAL(clicked()), SLOT(togglePlayPause()));
    connect(m_forwardButton, SIGNAL(clicked()), SLOT(rewindForward()));
    connect(m_stepForwardButton, SIGNAL(clicked()), SLOT(stepForward()));

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


    connect(m_widget, SIGNAL(pause()), SLOT(pause()));
    connect(m_widget, SIGNAL(play()), SLOT(play()));
    connect(m_widget, SIGNAL(rewindBackward()), SLOT(rewindBackward()));
    connect(m_widget, SIGNAL(rewindForward()), SLOT(rewindForward()));
    connect(m_widget, SIGNAL(stepBackward()), SLOT(stepBackward()));
    connect(m_widget, SIGNAL(stepForward()), SLOT(stepForward()));

    m_textItem = new MyTextItem(m_proxy);
    m_textItem->setOpacity(0.75);
    m_textItem->setVisible(false);
    m_textItem->setNavigationWidget(m_widget);

    setVideoCamera(0);

    m_sliderIsmoving = true;
    m_mouseOver = false;
    m_active = false;
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

    m_camera = camera;

    if (m_camera)
    {
        setVisible(true);
        m_textItem->setVisible(true);

        CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());
        setPlaying(!reader->onPause());
    }
    else
    {
        setVisible(false);
        m_textItem->setVisible(false);
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

    quint64 time;
    if (reader->isSingleShotMode() || reader->isSkippingFrames() || m_camera->currentTime() == 0)
        time = reader->currentTime();
    else
        time = m_camera->currentTime();

    m_currentTime = time/1000;
    m_widget->slider()->setCurrentValue(m_currentTime);

    qreal x = m_widget->slider()->x() + 8 + ((double)(m_widget->slider()->width() - 18)/(m_widget->slider()->sliderRange()))*(m_widget->slider()->currentValue() - m_widget->slider()->viewPortPos()); // fuck you!
    m_textItem->setPos(x - m_textItem->smallRect().width()/2, -40);
    m_textItem->setText(formatDuration(m_currentTime/1000));
    m_widget->label()->setText(formatDuration(length/1000));
}

void NavigationItem::onValueChanged(qint64 time)
{
    if (m_currentTime == time)
        return;

    qreal x = m_widget->slider()->x() + 8 + ((double)(m_widget->slider()->width() - 18)/(m_widget->slider()->sliderRange()))*(m_widget->slider()->currentValue() - m_widget->slider()->viewPortPos()); // fuck you!
    m_textItem->setPos(x - m_textItem->smallRect().width()/2, -40);
    m_textItem->setText(formatDuration(time/1000));

    CLAbstractArchiveReader *reader = static_cast<CLAbstractArchiveReader*>(m_camera->getStreamreader());

    if (reader->isSkippingFrames())
        return;

    time *= 1000;
    if (m_widget->slider()->isMoving())
        reader->jumpTo(time, true);
    else
        reader->jumpToPreviousFrame(time, true);

    m_camera->streamJump(time);

    //reader->setSpeed(-1);
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
    if (isPlaying())
        m_camera->getCamCamDisplay()->setSpeed(newSpeed);
}

void NavigationItem::setPlaying(bool playing)
{
    setActive(true);

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
