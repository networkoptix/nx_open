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
#include "base/log.h"
#include "util.h"

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
Q_GLOBAL_STATIC(ProxyStyle, proxyStyle)

class TimeLine : public QFrame
{
    TimeSlider *m_parent;
    QPoint m_previousPos;
    QPropertyAnimation *m_wheelAnimation;

public:
    TimeLine(TimeSlider *parent) : QFrame(parent), m_parent(parent), m_wheelAnimation(new QPropertyAnimation(m_parent, "scalingFactor")) {}

protected:
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void wheelEvent(QWheelEvent *);
    void drawGradient(QPainter& painter, const QRectF& r);
};

void TimeLine::wheelEvent(QWheelEvent *event)
{
    int delta = event->delta();
    if (abs(delta) == 120) {
        if (m_wheelAnimation->state() != QPropertyAnimation::Running) {
            m_wheelAnimation->setStartValue(m_parent->scalingFactor());
            m_wheelAnimation->setEndValue(m_parent->scalingFactor() + (double)delta/120);
            m_wheelAnimation->setDuration(100);
            m_wheelAnimation->start();
        } else {
            m_wheelAnimation->setEndValue(m_parent->scalingFactor() + (double)delta/120);
            m_wheelAnimation->setDuration(m_wheelAnimation->currentTime() + 100);
        }
    } else {
        m_parent->setScalingFactor(m_parent->scalingFactor() + (double)delta/120);
    }
    update();
}

struct IntervalInfo { qint64 interval; int value; const char * name; int count; const char * maxText; };
static const IntervalInfo intervalNames[] = { {100, 100, "ms", 10, "999ms"}, {1000, 1, "s", 60, "59s"},
                                              {5*1000, 5, "s", 12, "59s"}, {10*1000, 10, "s", 6, "59s"}, {30*1000, 30, "s", 2, "59s"}, {60*1000, 1, "m", 60, "59m"},
                                              {5*60*1000, 5, "m", 12, "59m"}, {10*60*1000, 10, "m", 6, "59m"}, {30*60*1000, 30, "m", 2, "59m"}, {60*60*1000, 1, "h", 24, "59h"},
                                              {3*60*60*1000, 3, "h", 8, "59h"}, {6*60*60*1000, 6, "h", 4, "59h"}, {12*60*60*1000, 12, "h", 2, "59h"},
                                              {24*60*60*1000, 1, "d", 1, "99d"}
                                            };

qint64 roundTime(qint64 msecs, int interval)
{
    return msecs - msecs%(intervalNames[interval].interval);
}

double round(double r) {
    return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}

/*
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

    int MAX_COLOR = 128+32;
    int MIN_COLOR = 0;
    for (int i = 0; i < height(); ++i)
    {
        //int k = qAbs(height()/2 - i);
        int k = height() - i;
        k *= (float) (MAX_COLOR-MIN_COLOR) / (float) height();
        k += MIN_COLOR;
        //quint32 color = k + (k << 8) + (k << 16) + (k << 24);
        QColor color(k,k,k,k);
        //color.setAlphaF(k / (float) MAX_COLOR);
        painter.setPen(color);
        painter.drawLine(0, i, width(), i);
    }
    painter.setPen(pal.color(QPalette::Base));


    int handleThickness = qApp->style()->pixelMetric(QStyle::PM_SliderControlThickness);
    if (handleThickness == 0)
        handleThickness = qApp->style()->pixelMetric(QStyle::PM_SliderThickness);

    int x = handleThickness/2 - 1;
    int y = frameWidth();
    int w = width() - handleThickness + 1;
    int h = height();

    painter.setPen(pal.color(QPalette::Text));

    qint64 range = m_parent->sliderRange();
    qint64 pos = m_parent->viewPortPos();

    double pixelPerTime = (double)w/range;
    const int minWidth = 5;
    int i = 0;
    while (minWidth > pixelPerTime*intervalNames[i].interval) {
        i++;
    }
    const int minVisibleWidth = 20;

    int ie = i;
    for ( ; intervalNames[ie].interval < m_parent->maximumValue() && ie < 13; ie++) {
    }

    int len = 0;
    int maxLen = 1;
    for (int j = i ; intervalNames[j].interval < range && j < 13; j++, maxLen++) {
    }

    int xposBase = x + w*(roundTime(pos, i) - pos)/(float)range + 0.5;
    int xDeltaBase = w*intervalNames[i].interval / (double)range + 0.5;

    int xDelta = xDeltaBase;

    qint64 end(pos + range);
    for ( ; i < ie; i++)
    {
        QColor color = pal.color(QPalette::Text);
        float opacity = (pixelPerTime*intervalNames[i].interval - minWidth)/minVisibleWidth;
        color.setAlphaF(opacity > 1 ? 1 : opacity);
        painter.setPen(color);
        painter.setFont(m_parent->font());

        QFontMetrics metric(m_parent->font());
        int textWidth = metric.width(intervalNames[i].maxText);

        qint64 start(roundTime(pos, i));
        int labelNumber = (start/(intervalNames[i].interval))%intervalNames[i].count;
        bool firstTime = true;
        int xpos = x + w*(start - pos)/ (float) range + 0.5;
        double dd = (xpos-xposBase) / (double) xDeltaBase;
        int di = round(dd);
        di*= xDeltaBase;
        di += xposBase;
        //xpos = ((xpos-xposBase) /xDeltaBase*xDeltaBase) + xposBase;
        xpos = di;


        while (start < end) {
            //int xpos = x + w*(start - pos)/ (float) range + 0.5;
            if (xpos >= 0) 
            {
                int deltaInterval = intervalNames[i + 1].interval/intervalNames[i].interval;
                bool skipText = (i < ie - 1) && labelNumber % deltaInterval == 0;
                if (!skipText) {
                    if (firstTime) {
                        firstTime = false;
                        len++;
                    }
                    painter.drawLine(QPoint(xpos, 0), QPoint(xpos, (h - 20)*(double)len/maxLen));
                    QString text = QString("%1%2").
                            arg(intervalNames[i].value*labelNumber).
                            arg(intervalNames[i].name);
                    if (textWidth*1.1 < pixelPerTime*intervalNames[i].interval) {
                        painter.drawText(QRect(xpos - textWidth/2, (h - 15)*(double)len/maxLen, textWidth, 15),
                                         Qt::AlignHCenter,
                                         text
                                         );
                    }
                }
            }
            start += intervalNames[i].interval;
            labelNumber = (labelNumber + 1) % intervalNames[i].count;
            xpos += xDelta;
        }
        if ( i < ie-1)
            xDelta *= intervalNames[i+1].interval / intervalNames[i].interval;
    }

    painter.setPen(QPen(Qt::red, 3));
    int xpos = x + m_parent->m_slider->value()*w / m_parent->sliderLength();
    painter.drawLine(QPoint(xpos, 0), QPoint(xpos, height()));
}
*/

void TimeLine::drawGradient(QPainter& painter, const QRectF& r)
{
    int MAX_COLOR = 128+32;
    int MIN_COLOR = 0;
    for (int i = r.top(); i < r.bottom(); ++i)
    {
        //int k = qAbs(height()/2 - i);
        int k = height() - i;
        k *= (float) (MAX_COLOR-MIN_COLOR) / (float) height();
        k += MIN_COLOR;
        //quint32 color = k + (k << 8) + (k << 16) + (k << 24);
        QColor color(k,k,k,k);
        //color.setAlphaF(k / (float) MAX_COLOR);
        painter.setPen(color);
        painter.drawLine(r.left(), i, r.width(), i);
    }
}

void TimeLine::paintEvent(QPaintEvent *ev)
{
    QFrame::paintEvent(ev);
    QPainter painter(this);

    int handleThickness = round(qApp->style()->pixelMetric(QStyle::PM_SliderControlThickness)/4.0);
    if (handleThickness == 0)
        handleThickness = round(qApp->style()->pixelMetric(QStyle::PM_SliderThickness)/4.0);

    QPalette pal = m_parent->palette();
    painter.setBrush(pal.brush(QPalette::Base));
    painter.setPen(pal.color(QPalette::Base));
    const QRect r(handleThickness, frameWidth(), width() - handleThickness, height() - 2*frameWidth());
    painter.drawRect(r);
    drawGradient(painter, r);
    painter.setPen(pal.color(QPalette::Text));

    const qint64 range = m_parent->sliderRange();
    const qint64 pos = m_parent->viewPortPos();
    const double pixelPerTime = (double) r.width() /range;

    const int minWidth = 5;
    const int minVisibleWidth = 20;

    int level = 0;
    for (;minWidth > pixelPerTime*intervalNames[level].interval; level++);

    int maxLevel = level;
    for (; intervalNames[maxLevel].interval < range && maxLevel < arraysize(intervalNames); maxLevel++);
    int maxLen = maxLevel - level;
    QFontMetrics metric(m_parent->font());

    QColor color = pal.color(QPalette::Text);
    //float opacity = (pixelPerTime*intervalNames[level].interval - minWidth)/minVisibleWidth;
    //color.setAlphaF(opacity > 1 ? 1 : opacity);
    painter.setPen(color);
    painter.setFont(m_parent->font());

    double xpos = r.left() - round(pixelPerTime * pos);
    int ticksOutside = -xpos / (intervalNames[level].interval * pixelPerTime);
    xpos += ticksOutside * intervalNames[level].interval * pixelPerTime;
    qint64 currentTime = intervalNames[level].interval * ticksOutside;
    while (currentTime <= pos + range) 
    {
        int currentLevel = level;
        for (;currentLevel < maxLevel && currentLevel < arraysize(intervalNames)-1; currentLevel++)
        {
            if (currentTime % intervalNames[currentLevel+1].interval)
                break;
        }
        const IntervalInfo& interval = intervalNames[currentLevel];
        int labelNumber = (currentTime/(interval.interval))%interval.count;
        double len = qMin(1.0, (currentLevel-level+1) / (double) maxLen);
        painter.drawLine(QPoint(xpos, 0), QPoint(xpos, (r.height() - 20)*len));
        QString text = QString("%1%2").arg(interval.value*labelNumber).arg(interval.name);
        int textWidth = metric.width(interval.maxText);
        QRectF textRect(xpos - textWidth/2, (r.height() - 15) * len, textWidth, 15);
        if (textWidth * 1.1 < pixelPerTime*interval.interval)
            painter.drawText(textRect, Qt::AlignHCenter, text);
        currentTime += intervalNames[level].interval;
        xpos += intervalNames[level].interval * pixelPerTime;
    }
    // draw cursor vertical line
    painter.setPen(QPen(Qt::red, 3));
    xpos = r.left() + m_parent->m_slider->value() * (r.width()-handleThickness-1) / m_parent->sliderLength();
    painter.drawLine(QPoint(xpos, 0), QPoint(xpos, height()));
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
    m_sliderPressed(false),
//    m_mode(TimeMode),
    m_animation(new QPropertyAnimation(this, "viewPortPos"))
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
    connect(m_slider, SIGNAL(sliderPressed()), SIGNAL(sliderPressed()));
    connect(m_slider, SIGNAL(sliderReleased()), SIGNAL(sliderReleased()));

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

    /*
    if (value < m_viewPortPos)
        setViewPortPos(value);
    else if (value > m_viewPortPos + sliderRange())
        setViewPortPos(value - sliderRange());
    */
    m_currentValue = qMin(value, maximumValue());
    if (!m_sliderPressed && m_animation->state() != QAbstractAnimation::Running)
        setViewPortPos(value - sliderRange()/2);

    updateSlider();

    update();

    emit currentValueChanged(m_currentValue);
}

void TimeSlider::onSliderPressed()
{
    m_sliderPressed = true;
}

void TimeSlider::onSliderReleased()
{
    m_sliderPressed = false;
    m_animation->stop();
    m_animation->setDuration(500);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    qint64 newViewortPos = m_currentValue - sliderRange()/2; // center
    newViewortPos = newViewortPos < 0 ? 0 : newViewortPos;
    newViewortPos = newViewortPos > maximumValue() - sliderRange() ? maximumValue() - sliderRange() : newViewortPos;

    double start = viewPortPos();
    double end = newViewortPos;
    m_animation->setStartValue(start);
    m_animation->setEndValue(end);
    m_animation->start();

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

double TimeSlider::viewPortPos() const
{
    return m_viewPortPos;
}

double TimeSlider::delta() const
{
    return ((1.0/sliderLength())*maximumValue())/exp(scalingFactor()/2);
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
