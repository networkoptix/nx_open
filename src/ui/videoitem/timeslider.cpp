#include "timeslider.h"

#include <QtCore/QDateTime>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWheelEvent>
//#include <QDebug>

#include "base/log.h"
#include "util.h"

static const double MAX_MIN_OPACITY = 0.6;
static const double MIN_MIN_OPACITY = 0.1;

Q_GLOBAL_STATIC(SliderProxyStyle, proxyStyle)

SliderProxyStyle *SliderProxyStyle::instance()
{
    return proxyStyle();
}

void MySlider::paintEvent(QPaintEvent *)
{
    static const int handleSize = 18;
    static const int gradHeigth = 10;
    static const int margins = 5;
    static const QPixmap pix = QPixmap(":/skin/slider-handle.png").scaled(handleSize, handleSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter p(this);

    int length = abs(maximum() - minimum());
    double handlePos = (double)(width()-handleSize)*value()/length;

    QRect r = contentsRect();
#ifdef Q_OS_WIN
    p.fillRect(rect(), QColor(128,128,128,128));
#endif
    p.setPen(QPen(Qt::gray, 2));
    p.drawRect(QRect(0, margins, r.width(), r.height() - 2*margins));

    p.setPen(QPen(Qt::darkGray, 1));
    p.drawRect(QRect(0, margins, r.width(), r.height() - 2*margins));

    p.setPen(QPen(QColor(0, 87, 207), 2));
    p.drawRect(QRect(0, margins, handlePos + handleSize/2, r.height() - 2*margins));

    QLinearGradient linearGrad(QPointF(0, 0), QPointF(handlePos, height()));
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(1, QColor(186, 239, 255));
    p.fillRect(0, (height() - gradHeigth)/2, handlePos + handleSize/2, gradHeigth, linearGrad);

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QRect handleRect(handlePos/* - handleSize/2*/, 0, handleSize, handleSize);
    p.drawPixmap(handleRect, pix);
}

QVariant qint64Interpolator(const qint64 &start, const qint64 &end, qreal progress)
{
    return start + (double)(end - start)*progress;
}

class TimeLine : public QFrame
{
    TimeSlider *m_parent;
    QPoint m_previousPos;
    int length;
    QPropertyAnimation *m_lineAnimation;
    QPropertyAnimation *m_opacityAnimation;
    QPropertyAnimation *m_wheelAnimation;
    bool m_dragging;
    float m_minOpacity;
    float m_scaleSpeed;
    int m_prevWheelDelta;

public:
    TimeLine(TimeSlider *parent) :
        QFrame(parent),
        m_parent(parent),
        m_lineAnimation(new QPropertyAnimation(m_parent, "currentValue")),
        m_opacityAnimation(new QPropertyAnimation(m_parent, "minOpacity")),
        m_wheelAnimation(new QPropertyAnimation(m_parent, "scalingFactor")),
        m_dragging(false),
        m_minOpacity(MAX_MIN_OPACITY),
        m_scaleSpeed(1.0),
        m_prevWheelDelta(INT_MAX)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        connect(m_wheelAnimation, SIGNAL(finished()), m_parent, SLOT(onWheelAnimationFinished()));
    }

    bool isDragging() const { return m_dragging; }
    void setMinOpacity(double value)
    {
        if (m_minOpacity != value)
        {
            m_minOpacity = value;
            update();
        }
    }
    double minOpacity() const { return m_minOpacity; }
    void wheelAnimationFinished();
    void wheelEvent(QWheelEvent *);
protected:
    void paintEvent(QPaintEvent *);
    void mouseDoubleClickEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void drawGradient(QPainter& painter, const QRectF& r);
};

void TimeLine::wheelAnimationFinished()
{
    m_opacityAnimation->stop();
    m_opacityAnimation->setStartValue(minOpacity());
    m_opacityAnimation->setEndValue(MAX_MIN_OPACITY);
    m_opacityAnimation->setDuration(500);
    m_opacityAnimation->start();
    m_scaleSpeed = 1.0;
}

void TimeLine::wheelEvent(QWheelEvent *event)
{
    static const float SCALE_FACTOR = 2000.0;
    static const int WHEEL_ANIMATION_DURATION = 1000;
    //static const float SCALE_FACTOR = 120.0;
    int delta = event->delta();
    m_opacityAnimation->stop();
    //setMinOpacity(0.1);
    if (m_opacityAnimation->endValue().toFloat() != MIN_MIN_OPACITY)
    {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(minOpacity());
        m_opacityAnimation->setEndValue(MIN_MIN_OPACITY);
        m_opacityAnimation->setDuration(WHEEL_ANIMATION_DURATION);
        m_opacityAnimation->start();
    }

    if (delta != m_prevWheelDelta)
        m_scaleSpeed = 1.0;

    if (delta*m_prevWheelDelta<0 && m_prevWheelDelta!= INT_MAX)
    {
        // different directions
        m_scaleSpeed = 1;
        m_wheelAnimation->stop();
        m_prevWheelDelta = delta;
        return;
    }

    if (abs(delta) == 120)
    {
        m_scaleSpeed *= 1.40;
        m_scaleSpeed  = qMin(m_scaleSpeed, (float)10.0);

        m_wheelAnimation->setEndValue(m_parent->scalingFactor() + delta/SCALE_FACTOR*m_scaleSpeed);
        if (m_wheelAnimation->state() != QPropertyAnimation::Running)
        {
            m_wheelAnimation->setStartValue(m_parent->scalingFactor());
            m_wheelAnimation->setDuration(WHEEL_ANIMATION_DURATION);
            m_wheelAnimation->setEasingCurve(QEasingCurve::InOutQuad);
            m_wheelAnimation->start();
        }
        else
        {
            m_wheelAnimation->setDuration(m_wheelAnimation->currentTime() + WHEEL_ANIMATION_DURATION);
        }
    }
    else
    {
        m_scaleSpeed = 1;
        m_wheelAnimation->stop();
        m_parent->setScalingFactor(m_parent->scalingFactor() + delta/120.0);
    }
    update();
    m_parent->centraliseSlider(); // we need to trigger animation
    m_prevWheelDelta = delta;
}
static const int MAX_LABEL_WIDTH = 64;
struct IntervalInfo { qint64 interval; int value; const char * name; int count; const char * maxText; };
static const IntervalInfo intervals[] =
{
    {100, 100, "ms", 10, "ms"},
    {1000, 1, "s", 60, "59s"},
    {5*1000, 5, "s", 12, "59s"},
    {10*1000, 10, "s", 6, "59s"},
    {30*1000, 30, "s", 2, "59s"},
    {60*1000, 1, "m", 60, "59m"},
    {5*60*1000, 5, "m", 12, "59m"},
    {10*60*1000, 10, "m", 6, "59m"},
    {30*60*1000, 30, "m", 2, "59m"},
    {60*60*1000, 1, "h", 24, "59h"},
    {3*60*60*1000, 3, "h", 8, "59h"},
    {6*60*60*1000, 6, "h", 4, "59h"},
    {12*60*60*1000, 12, "h", 2, "59h"},
    {24*60*60*1000, 1, "d", 1, "99d"}
};

qint64 roundTime(qint64 msecs, int interval)
{
    return msecs - msecs%(intervals[interval].interval);
}

double round(double r) {
    return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}

void TimeLine::drawGradient(QPainter& painter, const QRectF& r)
{
    int MAX_COLOR = 64+16;
    int MIN_COLOR = 0;
    for (int i = r.top(); i < r.bottom(); ++i)
    {
        int k = height() - i;
        k *= (float) (MAX_COLOR-MIN_COLOR) / (float) height();
        k += MIN_COLOR;
        QColor color(k,k,k,k);
        painter.setPen(color);
        painter.drawLine(r.left(), i, r.width(), i);
    }
}

void TimeLine::paintEvent(QPaintEvent *ev)
{
    static const float MIN_FONT_SIZE = 4.0;
    QFrame::paintEvent(ev);
    QPainter painter(this);

    int handleThickness = round(qApp->style()->pixelMetric(QStyle::PM_SliderControlThickness)/4.0);
    if (handleThickness == 0)
        handleThickness = round(qApp->style()->pixelMetric(QStyle::PM_SliderThickness)/4.0);
    handleThickness = 16/2;

    QPalette pal = m_parent->palette();
    painter.setBrush(pal.brush(QPalette::Base));
    painter.setPen(pal.color(QPalette::Base));
    const QRect r(handleThickness, frameWidth(), width() - handleThickness, height() - 2*frameWidth());
    painter.drawRect(r);
    drawGradient(painter, QRect(0, 0, width(), height() - 2*frameWidth()));
    painter.setPen(pal.color(QPalette::Text));

    const qint64 range = m_parent->sliderRange();
    const qint64 pos = m_parent->viewPortPos();
    const double pixelPerTime = (double) r.width() /range;

    const int minWidth = 30;

    int level = 0;
    for ( ; minWidth > pixelPerTime*intervals[level].interval && level < arraysize(intervals)-1; level++) {}

    unsigned maxLevel = level;
    for ( ; maxLevel < arraysize(intervals)-1 && intervals[maxLevel].interval <= range; maxLevel++) {}
    int maxLen = maxLevel - level;

    QColor color = pal.color(QPalette::Text);

    QVector<float> opacity;
    int maxHeight = 0;
    QVector<int> widths;
    QVector<QFont> fonts;
    for (unsigned i = level; i <= maxLevel; ++i)
    {
        float k = (pixelPerTime*intervals[i].interval - minWidth)/60.0;
        opacity << qMax(m_minOpacity, qMin(k, 1.0f));
        QFont font = m_parent->font();
        font.setPointSize(font.pointSize() + (i >= maxLevel-1) - (i <= level+1));
        font.setBold(i == maxLevel);

        QFontMetrics metric(font);
        double sc = (float) metric.width(intervals[i].maxText) / (pixelPerTime*intervals[i].interval);
        while (sc > 1.0/1.1)
        {
            font.setPointSizeF(font.pointSizeF()-0.5);
            if (font.pointSizeF() < MIN_FONT_SIZE)
                break;
            metric = QFontMetrics(font);
            sc = (float) metric.width(intervals[i].maxText) / (pixelPerTime*intervals[i].interval);
        }
        fonts << font;
        if (font.pointSizeF() < MIN_FONT_SIZE)
        {
            widths << 0;
        }
        else
        {
            widths << metric.width(intervals[i].maxText);
            maxHeight = maxHeight < metric.height() ? metric.height() : maxHeight;
        }
    }
    // draw grid
    double xpos = r.left() - round(pixelPerTime * pos);
    int outsideCnt = -xpos / (intervals[level].interval * pixelPerTime);
    xpos += outsideCnt * intervals[level].interval * pixelPerTime;
    for (qint64 curTime = intervals[level].interval*outsideCnt; curTime <= pos+range; curTime += intervals[level].interval)
    {
        int curLevel = level;
        for ( ; curLevel < arraysize(intervals)-2 && curTime % intervals[curLevel+1].interval == 0; curLevel++) {}
        int arrayIndex = qMin(curLevel-level, opacity.size()-1);

        color.setAlphaF(opacity[arrayIndex]);
        painter.setPen(color);
        painter.setFont(fonts[arrayIndex]);

        const IntervalInfo& interval = intervals[curLevel];
        double lineLen = qMin(1.0, (curLevel-level+1) / (double) maxLen);
        painter.drawLine(QPoint(xpos, 0), QPoint(xpos, (r.height() - maxHeight - 3)*lineLen));
        int labelNumber = (curTime/(interval.interval))%interval.count;
        QString text = QString("%1%2").arg(interval.value*labelNumber).arg(interval.name);

        if (widths[arrayIndex])
        {
            if (curLevel == 0)
            {
                painter.save();
                painter.translate(xpos-3, (r.height() - maxHeight) * lineLen);
                painter.rotate(90);
                painter.drawText(2, 0, text);
                painter.restore();
            }
            else
            {
                QRectF textRect(xpos - widths[arrayIndex]/2, (r.height() - maxHeight) * lineLen, widths[arrayIndex], 128);
                if (textRect.left() < 0 && pos == 0)
                    textRect.setLeft(0);
                painter.drawText(textRect, Qt::AlignHCenter, text);
            }
        }
        xpos += intervals[level].interval * pixelPerTime;
    }
    // draw cursor vertical line
    painter.setPen(QPen(Qt::red, 3));
    xpos = r.left() + m_parent->m_slider->value() * (r.width()-handleThickness-1) / m_parent->sliderLength();
    painter.drawLine(QPoint(xpos, 0), QPoint(xpos, height()));
}

void TimeLine::mouseDoubleClickEvent(QMouseEvent *)
{
    QPropertyAnimation *scaleAnimation = new QPropertyAnimation(m_parent, "scalingFactor");
    scaleAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    scaleAnimation->setStartValue(m_parent->scalingFactor());
    scaleAnimation->setEndValue(0);
    scaleAnimation->setDuration(1000);
    scaleAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

void TimeLine::mouseMoveEvent(QMouseEvent *me)
{
    if (me->buttons() & Qt::LeftButton) {
        // in fact, we need to use (width - slider handle thinkness/2), but it is ok without it
        QPoint dpos = m_previousPos - me->pos();
        if (!m_dragging && dpos.manhattanLength() < QApplication::startDragDistance())
            return;

        m_dragging = true;
        m_previousPos = me->pos();
        length = dpos.x();

        qint64 dtime = qRound((double) m_parent->sliderRange()/width()*dpos.x());
        if (m_parent->centralise()) {
            if (dtime < 0)
                dtime = qMax(dtime, -m_parent->viewPortPos());
            else
                dtime = qMin(dtime, m_parent->maximumValue() - (m_parent->viewPortPos() + m_parent->sliderRange()));
            qint64 time = m_parent->currentValue() + dtime;
            m_parent->setCurrentValue(time);
        }
        else
            m_parent->setViewPortPos(m_parent->viewPortPos() + dtime);
    }
}

void TimeLine::mousePressEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton) {
        m_parent->m_slider->setSliderDown(true);
        m_previousPos = me->pos();
        m_lineAnimation->stop();
        length = 0;
    }
}

void TimeLine::mouseReleaseEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton) {
        if (m_dragging) {
            m_dragging = false;

            if (abs(length) > 5) {
                int dx = length*2/**(35.0/t.elapsed())*/;

                qint64 range = m_parent->sliderRange();
                qint64 dl = range/width()*dx;
                m_lineAnimation->setStartValue(m_parent->viewPortPos());
                m_lineAnimation->setEasingCurve(QEasingCurve::OutQuad);
                m_lineAnimation->setEndValue(m_parent->viewPortPos() + dl);
                m_lineAnimation->setDuration(1000/* + abs(length)*/);
                //            m_lineAnimation->start();
            }
        } else {
            qint64 time = m_parent->viewPortPos() + round((double) m_parent->sliderRange()/width()*me->pos().x());
            m_parent->setCurrentValue(time);
        }
        m_parent->m_slider->setSliderDown(false);
    }
}

/*!
    \class TimeSlider
    \brief The TimeSlider class allows to navigate over time line.

    This class consists of slider to set position and time line widget, which shows time depending
    on scalingFactor. Also TimeSlider allows to zoom in/out scale of time line, which allows to
    navigate more accurate.
*/

bool TimeSlider::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::Wheel && target == m_slider)
    {
        m_frame->wheelEvent((QWheelEvent*) event);
        return true;
    }
    if (event->type() == QEvent::MouseButtonDblClick && target == m_slider)
    {
        return true;
    }
    return false;
}

TimeSlider::TimeSlider(QWidget *parent) :
    QWidget(parent),
    m_slider(new MySlider(this)),
    m_frame(0),
    m_currentValue(0),
    m_maximumValue(1000*10), // 10 sec
    m_viewPortPos(0),
    m_scalingFactor(0),
    m_isUserInput(false),
    m_sliderPressed(false),
    m_delta(0),
//    m_mode(TimeMode),
    m_animation(new QPropertyAnimation(this, "viewPortPos")),
    m_centralise(true),
    m_minimumRange(5*1000) // 5 seconds
{
    qRegisterAnimationInterpolator<qint64>(qint64Interpolator);

    m_frame = new TimeLine(this);
    setAttribute(Qt::WA_TranslucentBackground);
    m_slider->setOrientation(Qt::Horizontal);
    m_slider->setMaximum(1000);
    m_slider->installEventFilter(this);

    SliderProxyStyle *s = proxyStyle();
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

    if (m_currentValue < 0)
        m_currentValue  = 0;


    updateSlider();

    update();

    if (!m_isUserInput && !isMoving())
        centraliseSlider();

    emit currentValueChanged(m_currentValue);
}

void TimeSlider::onSliderPressed()
{
    m_sliderPressed = true;
}

void TimeSlider::onSliderReleased()
{
    centraliseSlider();

    m_sliderPressed = false;
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
    if (sliderRange() < m_minimumRange)
        m_scalingFactor = oldfactor;
    //setViewPortPos(currentValue() - m_slider->value()*delta());
    setViewPortPos(currentValue() - sliderRange()/2);
}

double TimeSlider::minOpacity() const
{
    return m_frame ? m_frame->minOpacity() : 0.0;
}

void TimeSlider::setMinOpacity(double value)
{
    m_frame->setMinOpacity(value);
}

//TimeSlider::Mode TimeSlider::mode() const
//{
//    return m_mode;
//}

QSize TimeSlider::minimumSizeHint() const
{
    int w = m_slider->minimumSizeHint().width();
    int h = m_slider->minimumSizeHint().height();
    return QSize(w, h + 40);
}

QSize TimeSlider::sizeHint() const
{
    int w = m_slider->sizeHint().width();
    int h = m_slider->sizeHint().height();
    return QSize(w, h + 40);
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

void TimeSlider::setViewPortPos(qint64 value)
{
    m_viewPortPos = qBound(qint64(0), value, maximumValue() - sliderRange());

    if (m_currentValue < m_viewPortPos)
        setCurrentValue(m_viewPortPos);
    else if (m_currentValue > m_viewPortPos + sliderRange())
        setCurrentValue(m_viewPortPos + sliderRange());

    updateSlider();
    update();
}

void TimeSlider::onSliderValueChanged(int value)
{
    if (!m_isUserInput) {
        m_isUserInput = true;
        qDebug() << m_isUserInput;
        setCurrentValue(fromSlider(value));
        m_isUserInput = false;
    }
}

qint64 TimeSlider::viewPortPos() const
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

qint64 TimeSlider::minimumRange() const
{
    return m_minimumRange;
}

void TimeSlider::setMinimumRange(qint64 r)
{
    m_minimumRange = r;
}

void TimeSlider::updateSlider()
{
    if (!m_isUserInput) {
        m_isUserInput = true;
        m_slider->setValue(toSlider(m_currentValue));
        m_isUserInput = false;
    }
}

void TimeSlider::centraliseSlider()
{
    // todo: maybe use isSliderDown() instead if m_sliderPressed?
    if (m_centralise) {
        if (m_frame->isDragging() || (!m_sliderPressed && m_animation->state() != QAbstractAnimation::Running /*&& !m_userInput*/)) {
            setViewPortPos(m_currentValue - sliderRange()/2);
        }
        else if (m_animation->state() != QPropertyAnimation::Running /*&& !m_sliderPressed*/) {
            qint64 newViewortPos = m_currentValue - sliderRange()/2; // center
            newViewortPos = newViewortPos < 0 ? 0 : newViewortPos;
            newViewortPos = newViewortPos > maximumValue() - sliderRange() ? maximumValue() - sliderRange() : newViewortPos;
            if (qAbs(newViewortPos - viewPortPos()) < 2*delta()) {
                setViewPortPos(m_currentValue - sliderRange()/2);
                return;
            }
            m_animation->stop();
            m_animation->setDuration(500);
            m_animation->setEasingCurve(QEasingCurve::InOutQuad);

            double start = viewPortPos();
            double end = newViewortPos;
            m_animation->setStartValue(start);
            m_animation->setEndValue(end);
            m_animation->start();
        }
    }
}

void TimeSlider::onWheelAnimationFinished()
{
    m_frame->wheelAnimationFinished();
}
