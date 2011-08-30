#include "volumewidget.h"

#include <QtCore/QCoreApplication>

#include <QtGui/QHBoxLayout>
#include <QtGui/QPushButton>

#include "openal/qtvaudiodevice.h"

#include "ui/videoitem/navigationitem.h"
#include "ui/videoitem/timeslider.h" // SliderProxyStyle
#include "ui/widgets/styledslider.h"

VolumeWidget::VolumeWidget(QWidget *parent) :
    QWidget(parent)
{
    installEventFilter(this);

    m_slider = new StyledSlider(Qt::Horizontal, this);
    m_slider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_slider->setStyle(SliderProxyStyle::instance());
    m_slider->setRange(0, 100);

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

QSlider *VolumeWidget::slider() const
{
    return m_slider;
}

QSize VolumeWidget::sizeHint() const
{
    return QSize(80, 20);
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
    const bool isMute = QtvAudioDevice::instance().isMute();
    m_button->setChecked(isMute);
    m_slider->setValueText(!isMute ? QString::number(value) + QLatin1Char('%') : tr("Muted"));
}

void VolumeWidget::onButtonChecked()
{
    QtvAudioDevice::instance().setMute(m_button->isChecked());
    float volume = QtvAudioDevice::instance().volume();
    m_slider->setValue(volume * 100);
}
