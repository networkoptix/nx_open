#include "timeslider.h"

#include <QtCore/QDateTime>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWheelEvent>
//#include <QDebug>

#include <QProxyStyle>

class ProxyStyle : public QProxyStyle
{
public:
    int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    {
        if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
            return Qt::LeftButton;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

enum PaintMode { PaintMSeconds = 0, PaintSeconds, PaintMinutes, PaintHours, PaintDays };
static const qint64 lengths[] = { 1, 1000, 1000*60, 1000*60*60, 1000*60*60*24, 0x7fffffffffffffffll };
static const int coeffs[] = { 100, 5, 5, 1, 1, 1 };
static const int markCount[] = { 10, 5, 5, 4, 12, 1 };
static const char *const names[] = { "ms", "s", "m", "h", "d" };

class TimeLine : public QFrame
{
    TimeSlider *m_parent;
    QPoint m_previousPos;

public:
    TimeLine(TimeSlider *parent) : QFrame(parent), m_parent(parent) {}

protected:
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void wheelEvent(QWheelEvent *);
};

void TimeLine::wheelEvent(QWheelEvent *event)
{
    m_parent->setScalingFactor(m_parent->scalingFactor() + (double)event->delta()/120);
    update();
}

PaintMode getPaintMode(qint64 msecs)
{
    for (int i = PaintDays; i >= PaintMSeconds; i--) {
        if (msecs > (qint64)lengths[i])
            return (PaintMode)i;
    }
    return PaintMSeconds;
}

qint64 roundTime(qint64 msecs, PaintMode mode)
{
    return msecs - msecs%(coeffs[mode]*lengths[mode]);
}

void TimeLine::paintEvent(QPaintEvent *ev)
{
    QFrame::paintEvent(ev);
    QPainter painter(this);

//    QPalette pal = qApp->style()->standardPalette();
    QPalette pal = m_parent->palette();
    painter.setBrush(pal.brush(QPalette::Base));
    painter.setPen(pal.color(QPalette::Base));
    QRect r(frameWidth(), frameWidth(), width() - 2*frameWidth(), height() - 2*frameWidth());
    painter.drawRect(r);

    int handleThickness = qApp->style()->pixelMetric(QStyle::PM_SliderControlThickness);
    if (handleThickness == 0)
        handleThickness = qApp->style()->pixelMetric(QStyle::PM_SliderThickness);

    int x = handleThickness/2 - 1;
    int y = frameWidth();
    int w = width() - handleThickness + 1;
    int h = height();

    painter.setPen(pal.color(QPalette::Text));
    qint64 sliderRange = m_parent->sliderRange();
    PaintMode paintMode = getPaintMode(sliderRange);

    qint64 viewPortPos = m_parent->viewPortPos();
//    if (m_parent->mode() == TimeSlider::TimeMode) {

        qint64 start(roundTime(viewPortPos, paintMode));
        qint64 end(viewPortPos + sliderRange);

        while (start <= end) {

            int pos = x + w*(start - viewPortPos)/sliderRange;
            painter.drawText(QRect(pos - 20, h - 15, 40, 15),
                             Qt::AlignHCenter,
                             QString("%1%2").
                             arg((start%(lengths[paintMode+1])/(lengths[paintMode]))).
                             arg(tr(names[paintMode])));
            painter.drawLine(QPoint(pos, y), QPoint(pos, h-20));

            qint64 length = (qint64)coeffs[paintMode]*lengths[paintMode];
            for (int i = 0; i < markCount[paintMode]; i++) {
                start += length/markCount[paintMode];
                if (start <= end) {
                    int pos = x + w*(start - viewPortPos)/sliderRange;
                    painter.drawLine(QPoint(pos, y), QPoint(pos, h/4));
                }
            }

        }

//    } else {
        // not implemented yet
//    }

    painter.setPen(QPen(Qt::red, 3));
    int pos = x + m_parent->m_slider->value()*w / m_parent->sliderLength();
    painter.drawLine(QPoint(pos, 0), QPoint(pos, height()));
}

void TimeLine::mouseMoveEvent(QMouseEvent *me)
{
    if (me->buttons() & Qt::LeftButton) {
        // in fact, we need to use (width - slider handle thinkness/2), but it is ok without it
        qint64 length = m_parent->sliderRange();
        int dx = m_previousPos.x() - me->pos().x();
        qint64 dl = length/width()*dx;
        m_parent->setViewPortPos(m_parent->viewPortPos() + dl);
        m_previousPos = me->pos();
    }
}

void TimeLine::mousePressEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton) {
        m_previousPos = me->pos();
    }
}

/*!
    \class TimeSlider
    \brief The TimeSlider class allows to navigate over time line.

    This class consists of slider to set position and time line widget, which shows time depending
    on scalingFactor. Also TimeSlider allows to zoom in/out scale of time line, which allows to
    navigate more accurate.
*/

Q_GLOBAL_STATIC(ProxyStyle, proxyStyle)

TimeSlider::TimeSlider(QWidget *parent) :
    QWidget(parent),
    m_slider(new QSlider(this)),
    m_frame(new TimeLine(this)),
    m_currentValue(0),
    m_maximumValue(1000*10), // 10 sec
    m_viewPortPos(0),
    m_scalingFactor(0),
    m_userInput(true),
    m_delta(0),
//    m_mode(TimeMode),
    m_animation(new QPropertyAnimation(this, "viewPortPos")),
    m_isMoving(false)
{
    m_slider->setOrientation(Qt::Horizontal);
    m_slider->setMaximum(1000);
    m_slider->installEventFilter(this);

    ProxyStyle *s = proxyStyle();
    s->setBaseStyle(qApp->style());
    m_slider->setStyle(s);

    m_frame->setFrameShape(QFrame::WinPanel);
    m_frame->setFrameShadow(QFrame::Sunken);
    m_frame->installEventFilter(this);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(m_slider);
    layout->addWidget(m_frame);
    setLayout(layout);

    connect(m_slider, SIGNAL(valueChanged(int)), SLOT(onSliderValueChanged(int)));
    connect(m_slider, SIGNAL(sliderPressed()), SLOT(onSliderPressed()));
    connect(m_slider, SIGNAL(sliderReleased()), SLOT(onSliderReleased()));
}

/*!
    \property TimeSlider::currentValue
    \brief the sliders current value in milliseconds

    This value is always in range from 0 ms to maximumValue ms.

    \sa TimeSlider::maximumValue
*/
qint64 TimeSlider::currentValue() const
{
    return m_currentValue;
}

void TimeSlider::setCurrentValue(qint64 value)
{
    if (m_currentValue == value)
        return;

    m_currentValue = value < maximumValue() ? value : maximumValue();

    if (value < m_viewPortPos)
        setViewPortPos(value);

    if (value > m_viewPortPos + sliderRange())
        setViewPortPos(value - sliderRange());

    updateSlider();

    update();

    m_animation->stop();
    m_animation->setDuration(1000);
    m_animation->setStartValue(viewPortPos());
    qint64 newViewortPos = m_currentValue - sliderRange()/2; // center
    newViewortPos = newViewortPos < 0 ? 0 : newViewortPos;
    newViewortPos = newViewortPos > maximumValue() - sliderRange() ?
                    maximumValue() - sliderRange() :
                    newViewortPos;
    m_animation->setEndValue(newViewortPos);
//    m_animation->start();

    emit currentValueChanged(m_currentValue);
}

/*!
    \property TimeSlider::maximumValue
    \brief the sliders maximum value in milliseconds

    Sliders minimum value is always 0 and can't be less than zero. Maximum value can't be less
    than 1000ms.

    \sa TimeSlider::currentValue
*/
qint64 TimeSlider::maximumValue() const
{
    return m_maximumValue;
}

void TimeSlider::setMaximumValue(qint64 value)
{
    if (value < 1000)
        value = 1000;
    m_maximumValue = value;
}

/*!
    \property TimeSlider::scalingFactor
    \brief the sliders scaling factr of the time line

    Scaling factor is always great or equal to 0. If factor is 0, slider shows all time line, if
    greater than 0, it exponentially zooms it (visible time is maximumValue/exp(factor/2)).

    \sa TimeSlider::currentValue
*/
double TimeSlider::scalingFactor() const
{
    return m_scalingFactor;
}

void TimeSlider::setScalingFactor(double factor)
{
    double oldfactor = m_scalingFactor;
    m_scalingFactor = factor > 0 ? factor : 0;
    if (sliderRange() < 1000)
        m_scalingFactor = oldfactor;
    setViewPortPos(currentValue() - m_slider->value()*delta());
}

//TimeSlider::Mode TimeSlider::mode() const
//{
//    return m_mode;
//}

QSize TimeSlider::minimumSizeHint() const
{
//    int w = m_slider->minimumSizeHint().width();
//    int h = m_slider->minimumSizeHint().height();
//    return QSize(w, h);
    return QSize(200, 60);
}

QSize TimeSlider::sizeHint() const
{
//    int w = m_slider->sizeHint().width();
//    int h = m_slider->sizeHint().height();
//    return QSize(w, h);
    return QSize(200, 60);
}

/*!
    Increases scale factor by 1.0

    \sa zoomOut(), TimeSlider::scalingFactor
*/
void TimeSlider::zoomIn()
{
    setScalingFactor(scalingFactor() + 1);
}

/*!
    Decreases scale factor by 1.0

    \sa zoomIn(), TimeSlider::scalingFactor
*/
void TimeSlider::zoomOut()
{
    setScalingFactor(scalingFactor() - 1);
}

void TimeSlider::setViewPortPos(double v)
{
    qint64 value = v;
    if (value < 0)
        value = 0;
    else if (value > maximumValue() - sliderRange())
        value = maximumValue() - sliderRange();

    if (m_currentValue < m_viewPortPos)
        setCurrentValue(m_viewPortPos);
    else if (m_currentValue > m_viewPortPos + sliderRange())
        setCurrentValue(m_viewPortPos + sliderRange());

    m_viewPortPos = value;
    updateSlider();

    update();
}

void TimeSlider::onSliderValueChanged(int value)
{
    if (m_userInput)
        setCurrentValue(fromSlider(value));
}

void TimeSlider::onSliderPressed()
{
    m_isMoving = true;
}

void TimeSlider::onSliderReleased()
{
    m_isMoving = false;
}

double TimeSlider::viewPortPos() const
{
    return m_viewPortPos;
}

qint64 TimeSlider::delta() const
{
    return maximumValue()/sliderLength()/exp(scalingFactor()/2);
}

double TimeSlider::fromSlider(int value)
{
    return viewPortPos() + (value)*delta();
}

int TimeSlider::toSlider(double value)
{
    return (value - viewPortPos())/delta();
}

int TimeSlider::sliderLength() const
{
    return m_slider->maximum() - m_slider->minimum();
}

qint64 TimeSlider::sliderRange()
{
    return sliderLength()*delta();
}

void TimeSlider::updateSlider()
{
    m_userInput = false;
    m_slider->setValue(toSlider(m_currentValue));
    m_userInput = true;
}
