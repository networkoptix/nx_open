#include "speedwidget.h"

#include <QtCore/QCoreApplication>

#include <QtGui/QHBoxLayout>

#include "ui/videoitem/timeslider.h" // SliderProxyStyle
#include "ui/widgets/styledslider.h"

SpeedWidget::SpeedWidget(QWidget *parent) :
    QWidget(parent)
{
    installEventFilter(this);

    m_slider = new StyledSlider(Qt::Horizontal, this);
    m_slider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_slider->setStyle(SliderProxyStyle::instance());
    m_slider->setRange(-100, 100);
    m_slider->setValue(10);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(20, 0, 20, 0);
    layout->setSpacing(3);
    layout->addWidget(m_slider);
    setLayout(layout);

    connect(m_slider, SIGNAL(valueChanged(int)), SLOT(onValueChanged(int)));
}

QSlider *SpeedWidget::slider() const
{
    return m_slider;
}

QSize SpeedWidget::sizeHint() const
{
    return QSize(80, 16);
}

bool SpeedWidget::eventFilter(QObject *watched, QEvent *event)
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

void SpeedWidget::paintEvent(QPaintEvent *)
{
    static QPixmap pix = QPixmap(":/skin/volume_slider_background.png");

    QPainter p(this);
    p.drawPixmap(contentsRect(), pix);
}

void SpeedWidget::onValueChanged(int value)
{
    m_slider->setValueText(QString::number(float(value) / 10.0) + QLatin1Char('x'));
}
