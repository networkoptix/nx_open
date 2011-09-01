#include "speedwidget.h"

#include <QtCore/QCoreApplication>

#include <QtGui/QHBoxLayout>
#include <QtGui/QToolButton>

#include "ui/videoitem/timeslider.h" // SliderProxyStyle
#include "ui/widgets/styledslider.h"

SpeedWidget::SpeedWidget(QWidget *parent) :
    QWidget(parent)
{
    installEventFilter(this);

    m_slider = new StyledSlider(this);
    m_slider->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_slider->setStyle(SliderProxyStyle::instance());
    m_slider->setRange(-200, 200);
    m_slider->setValue(10); // 1x
    m_slider->setSingleStep(2);
    m_slider->setPageStep(5);
m_slider->setMinimumWidth(220); // ### otherwise width bounded to 84... wtf?

    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

    m_leftButton = new QToolButton(this);
    m_leftButton->setIcon(QIcon(QLatin1String(":/skin/left-arrow.png")));
    m_leftButton->setIconSize(QSize(14, 14));
    m_leftButton->setAutoRepeat(true);

    m_rightButton = new QToolButton(this);
    m_rightButton->setIcon(QIcon(QLatin1String(":/skin/right-arrow.png")));
    m_rightButton->setIconSize(QSize(14, 14));
    m_rightButton->setAutoRepeat(true);

    connect(m_leftButton, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
    connect(m_rightButton, SIGNAL(clicked()), this, SLOT(onButtonClicked()));

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addWidget(m_leftButton, 0, Qt::AlignCenter);
    layout->addWidget(m_slider, 1, Qt::AlignCenter);
    layout->addWidget(m_rightButton, 0, Qt::AlignCenter);
    setLayout(layout);
}

QSlider *SpeedWidget::slider() const
{
    return m_slider;
}

QSize SpeedWidget::sizeHint() const
{
    return QSize(80, 20);
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
    if (value > 5 && value != 10 && value < 15)
    {
        m_slider->setValue(10); // 1x
        return;
    }

    const float newSpeed = float(value) / 10.0;
    m_slider->setValueText(QString::number(newSpeed) + QLatin1Char('x'));
    emit speedChanged(newSpeed);
}

void SpeedWidget::onButtonClicked()
{
    const int step = m_slider->value() == 10 ? m_slider->pageStep() : m_slider->singleStep();
    if (sender() == m_leftButton)
        m_slider->setValue(m_slider->value() - step);
    else if (sender() == m_rightButton)
        m_slider->setValue(m_slider->value() + step);
}
