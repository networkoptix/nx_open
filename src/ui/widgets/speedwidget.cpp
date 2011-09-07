#include "speedwidget.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QGradient>
#include <QtGui/QHBoxLayout>
#include <QtGui/QPainter>
#include <QtGui/QToolButton>

#include "ui/videoitem/timeslider.h" // SliderProxyStyle

class SpeedSliderProxyStyle : public SliderProxyStyle
{
public:
    SpeedSliderProxyStyle(QStyle *baseStyle = 0) : SliderProxyStyle(baseStyle) {}

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget = 0) const
    {
        QRect r = SliderProxyStyle::subControlRect(cc, opt, sc, widget);
        if (cc == CC_Slider && sc == SC_SliderHandle)
        {
            int side = qMin(r.width(), r.height());
            if (qstyleoption_cast<const QStyleOptionSlider *>(opt)->orientation == Qt::Horizontal)
                r.setWidth(side);
            else
                r.setHeight(side);
        }
        return r;
    }
};


static const int fixedSpeedSet[] = { -160, -80, -40, -20, -15, -10, -5, 5, 10, 15, 20, 40, 80, 160 }; // x10
static const int fixedSpeedSetSize = sizeof(fixedSpeedSet) / sizeof(fixedSpeedSet[0]);
static const int fsIndex1x = 8;

//static const int sliderHeigth = 12;

SpeedSlider::SpeedSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent),
      m_animation(0)
{
//    setMinimumHeight(sliderHeigth + 2);

    setStyle(new SpeedSliderProxyStyle);
    setRange(0, fixedSpeedSetSize * 10 - 1);
    setSingleStep(10);
    setPageStep(10);
    setTickInterval(10);

    resetSpeed();
}

SpeedSlider::~SpeedSlider()
{
    if (m_animation)
    {
        m_animation->stop();
        delete m_animation;
    }
}

void SpeedSlider::resetSpeed()
{
    setValue(fsIndex1x * 10); // 1x
}

void SpeedSlider::paintEvent(QPaintEvent *e)
{
    QSlider::paintEvent(e); return;/*
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 0));

    QRect r = contentsRect();
    if (r.height() > sliderHeigth)
    {
        r.moveTop((r.height() - sliderHeigth) / 2);
        r.setHeight(sliderHeigth);
    }
    r.adjust(0, 0, -1, -1);

    const float handlePos = float(value() - minimum()) / (maximum() - minimum());

    QLinearGradient linearGrad(QPointF(0, 0), QPointF(r.width(), 0));
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(handlePos, QColor(186, 239, 255));
    if (!qFuzzyCompare(handlePos, 1.0f))
    {
        linearGrad.setColorAt(handlePos + 0.001, QColor(0, 0, 0, 0));
        linearGrad.setColorAt(1, QColor(0, 0, 0, 0));
    }

    p.setPen(QPen(Qt::darkGray, 1));
    p.setBrush(linearGrad);
    p.drawRect(r);*/
}

void SpeedSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    QSlider::mouseReleaseEvent(ev);

    if (ev->isAccepted() && value() != 10)
    {
        if (m_animation)
            delete m_animation;

        m_animation = new QPropertyAnimation(this, "value", this);
        m_animation->setEasingCurve(QEasingCurve::OutQuad);
        m_animation->setDuration(500);
        m_animation->setEndValue(fsIndex1x * 10);
        m_animation->start();
    }
}

void SpeedSlider::sliderChange(SliderChange change)
{
    QSlider::sliderChange(change);

    int idx = value() / 10; // truncation!
    Q_ASSERT(idx >= 0 && idx < fixedSpeedSetSize);
    int value = fixedSpeedSet[idx]; // map to speed * 10
    if (value > 5 && value < 15)
        value = 10; // make a gap around 1x

    const float newSpeed = float(value) / 10.0;
    emit speedChanged(newSpeed);
}


SpeedWidget::SpeedWidget(QWidget *parent) :
    QWidget(parent)
{
    installEventFilter(this);

    m_slider = new SpeedSlider(this);
    m_slider->setTickPosition(QSlider::TicksAbove);
m_slider->setMinimumWidth(220); // ### otherwise width bounded to 84... wtf?

    connect(m_slider, SIGNAL(speedChanged(float)), this, SIGNAL(speedChanged(float)));
/*
    m_leftButton = new QToolButton(this);
    m_leftButton->setIcon(QIcon(QLatin1String(":/skin/left-arrow.png")));
    m_leftButton->setIconSize(QSize(12, 12));
    m_leftButton->setAutoRepeat(true);

    m_rightButton = new QToolButton(this);
    m_rightButton->setIcon(QIcon(QLatin1String(":/skin/right-arrow.png")));
    m_rightButton->setIconSize(QSize(12, 12));
    m_rightButton->setAutoRepeat(true);

    connect(m_leftButton, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
    connect(m_rightButton, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
*/
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
//    layout->addWidget(m_leftButton, 0, Qt::AlignCenter);
    layout->addWidget(m_slider, 1, Qt::AlignCenter);
//    layout->addWidget(m_rightButton, 0, Qt::AlignCenter);
    setLayout(layout);
}

QSize SpeedWidget::sizeHint() const
{
    return QSize(80, 20);
}

void SpeedWidget::resetSpeed()
{
    m_slider->resetSpeed();
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
    p.drawPixmap(rect(), pix);
}

void SpeedWidget::onButtonClicked()
{
    /*const int step = m_slider->value() == 10 ? m_slider->pageStep() : m_slider->singleStep();
    if (sender() == m_leftButton)
        m_slider->setValue(m_slider->value() - step);
    else if (sender() == m_rightButton)
        m_slider->setValue(m_slider->value() + step);*/
}
