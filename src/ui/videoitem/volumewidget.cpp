#include "volumewidget.h"

#include <QtCore/QCoreApplication>

#include <QtGui/QHBoxLayout>
#include <QtGui/QPushButton>

#include "../../openal/qtvaudiodevice.h"
#include "timeslider.h"

StyledSlider::StyledSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent),
      m_timerId(0)
{
}

StyledSlider::StyledSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent),
      m_timerId(0)
{
}

StyledSlider::~StyledSlider()
{
    if (m_timerId)
    {
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

void StyledSlider::paintEvent(QPaintEvent *)
{
    static const int gradHeigth = 10;

    QPainter p(this);

    const double handlePos = (double)(width() - 1) * value() / qAbs(maximum() - minimum());
    /*QStyleOptionSlider slider;
    initStyleOption(&slider);
    int available = style()->pixelMetric(QStyle::PM_SliderSpaceAvailable, &slider, this);
    const double handlePos = (double)QStyle::sliderPositionFromValue(slider.minimum, slider.maximum, slider.sliderValue, available);*/

    QRect r = contentsRect();
    p.fillRect(rect(), QColor(0,0,0,0));

    p.setPen(QPen(Qt::darkGray, 1));
    p.drawRect(QRect(r.x(), (height() - gradHeigth)/2 - 1, r.width()-1, gradHeigth + 2));

    QLinearGradient linearGrad(QPointF(0, 0), QPointF(handlePos, height()));
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(1, QColor(186, 239, 255));
    p.fillRect(1, (height() - gradHeigth)/2, handlePos+1, gradHeigth, linearGrad);

    if (m_timerId)
    {
        p.setPen(QPen(palette().color(QPalette::Active, QPalette::HighlightedText), 1));
        p.drawText(r, Qt::AlignCenter, value() ? QString::number(value()) + QLatin1Char('%') : tr("Muted"));
    }
}

void StyledSlider::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
    {
        killTimer(m_timerId);
        m_timerId = 0;
        update();
    }

    QSlider::timerEvent(event);
}

void StyledSlider::sliderChange(SliderChange change)
{
    if (m_timerId)
        killTimer(m_timerId);
    m_timerId = startTimer(2500);

    QSlider::sliderChange(change);
}


VolumeWidget::VolumeWidget(QWidget *parent) :
    QWidget(parent)
{
    installEventFilter(this);

    m_slider = new StyledSlider(Qt::Horizontal, this);
    m_slider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_slider->setRange(0, 100);
    m_slider->setValue(50);
    m_slider->setStyle(SliderProxyStyle::instance());

    m_button = new MyButton(this);
    m_button->setPixmap(QPixmap(":/skin/unmute.png"));
    m_button->setCheckedPixmap(QPixmap(":/skin/mute.png"));
    m_button->setMinimumSize(20, 20);
    m_button->setCheckable(true);
    m_button->installEventFilter(this);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(20, 0, 20, 0);
    layout->setSpacing(3);
    layout->addWidget(m_button);
    layout->addWidget(m_slider);
    setLayout(layout);

    const float volume = QtvAudioDevice::instance().volume();
    m_slider->setValue(volume * 100);
    m_button->setChecked(QtvAudioDevice::instance().isMute());

    connect(m_slider, SIGNAL(valueChanged(int)), SLOT(onValueChanged(int)));
    connect(m_button, SIGNAL(toggled(bool)), SLOT(onButtonChecked()));

    setFixedSize(144, 36);
}

bool VolumeWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (/*watched != m_slider && */event->type() == QEvent::Wheel)
    {
        // adjust and redirect to slider
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
        const QPoint pos = m_slider->geometry().center();
        QWheelEvent *newEvent = new QWheelEvent(m_slider->mapFromGlobal(pos), pos,
                                                wheelEvent->delta(), wheelEvent->buttons(),
                                                wheelEvent->modifiers(), wheelEvent->orientation());
        QCoreApplication::postEvent(m_slider, newEvent);
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void VolumeWidget::paintEvent(QPaintEvent *)
{
    static QPixmap pix = QPixmap(":/skin/volume_slider_background.png");

    QPainter p(this);
    p.drawPixmap(contentsRect(), pix);
}

void VolumeWidget::onValueChanged(int value)
{
    QtvAudioDevice::instance().setVolume(value / 100.0);
    m_button->setChecked(QtvAudioDevice::instance().isMute());
}

void VolumeWidget::onButtonChecked()
{
    QtvAudioDevice::instance().setMute(m_button->isChecked());
    float volume = QtvAudioDevice::instance().volume();
    m_slider->setValue(volume * 100);
}
