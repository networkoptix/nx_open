#include "timeslider.h"

#include <QtCore/QDateTime>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QApplication>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QPainter>
#include <QtGui/QProxyStyle>
#include <QtGui/QStyle>
#include <QtGui/QStyleFactory>

#include "ui/skin.h"
#include "utils/common/util.h"

#include "ui/widgets2/graphicsframe.h"
#include "ui/widgets2/graphicsslider.h"
#include "ui/widgets/tooltipitem.h"

#include <qmath.h>

namespace {

QVariant qint64Interpolator(const qint64 &start, const qint64 &end, qreal progress)
{
    return start + double(end - start) * progress;
}

static const float MIN_MIN_OPACITY = 0.1f;
static const float MAX_MIN_OPACITY = 0.6f;
static const float MIN_FONT_SIZE = 4.0f;
static const int MAX_LABEL_WIDTH = 64;

}


// -------------------------------------------------------------------------- //
// SliderProxyStyle
// -------------------------------------------------------------------------- //
class SliderProxyStyle : public QProxyStyle
{
public:
    SliderProxyStyle(QStyle *baseStyle = 0) : QProxyStyle(baseStyle)
    {
        if (!baseStyle)
            setBaseStyle(qApp->style());
    }

    int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    {
        if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
            return Qt::LeftButton;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget = 0) const
    {
        QRect r = QProxyStyle::subControlRect(cc, opt, sc, widget);
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

Q_GLOBAL_STATIC_WITH_ARGS(SliderProxyStyle, sliderProxyStyle, (QStyleFactory::create(qApp->style()->objectName())))


// -------------------------------------------------------------------------- //
// MySlider
// -------------------------------------------------------------------------- //
class MySlider : public GraphicsSlider
{
    friend class TimeSlider; // ### for sizeHint()

public:
    MySlider(TimeSlider *parent);

    void setToolTipItem(ToolTipItem *toolTip);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    const QRect &handleRect() const;

    bool isAtEnd() const;

    void setEndSize(int size);

protected:
    void sliderChange(SliderChange change);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    void invalidateHandleRect();
    void ensureHandleRect() const;

private:
    TimeSlider *m_parent;
    ToolTipItem *m_toolTip;
    mutable QRect m_handleRect;
    mutable bool m_handleRectValid;
    int m_endSize;
};

MySlider::MySlider(TimeSlider *parent)
    : GraphicsSlider(parent),
      m_parent(parent),
      m_toolTip(0),
      m_handleRectValid(false),
      m_endSize(0)
{
    setToolTipItem(new StyledToolTipItem);
}

bool MySlider::isAtEnd() const 
{
    if (m_endSize == 0)
        return true;
    else
        return value() > maximum() - m_endSize;
}

void MySlider::setEndSize(int size)
{
    m_endSize = size;
}

void MySlider::setToolTipItem(ToolTipItem *toolTip)
{
    if (m_toolTip == toolTip)
        return;

    delete m_toolTip;

    m_toolTip = toolTip;

    if (m_toolTip)
        m_toolTip->setParentItem(this);
}

void MySlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    ensureHandleRect();

    static const QPixmap pix = Skin::pixmap(QLatin1String("slider-handle.png")).scaled(m_handleRect.width(), m_handleRect.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QRectF r = contentsRect();

#ifdef Q_OS_WIN
    painter->fillRect(rect(), QColor(128,128,128,128));
#endif
    painter->setPen(QPen(Qt::gray, 2));
    painter->drawRect(r);

    painter->setPen(QPen(Qt::darkGray, 1));
    painter->drawRect(r);

    painter->setPen(QPen(QColor(0, 87, 207), 2));
    r.setRight(m_handleRect.center().x());
    painter->drawRect(r);

    QLinearGradient linearGrad(r.topLeft(), r.bottomRight());
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(1, QColor(186, 239, 255));
    painter->fillRect(r, linearGrad);

    /* Draw time periods. */
    if(!m_parent->timePeriodList().empty())
    {
        const qint64 range = m_parent->sliderRange();
        const qint64 pos = m_parent->viewPortPos() /*+ timezoneOffset*/;

        QRectF contentsRect = this->contentsRect();
        qreal x = contentsRect.left() + m_handleRect.width() / 2;
        qreal w = contentsRect.width() - m_handleRect.width();

        // TODO: use lower_bound here and draw only what is actually needed.
        foreach(const QnTimePeriod &period, m_parent->timePeriodList())
        {
            qreal left = x + static_cast<qreal>(period.startTimeUSec / 1000 - pos) / range * w;
            left = qMax(left, contentsRect.left());

            qreal right;
            if(period.durationUSec == -1) {
                right = contentsRect.right();
            } else {
                right = x + static_cast<qreal>((period.startTimeUSec + period.durationUSec) / 1000 - pos) / range * w;
                right = qMin(right, contentsRect.right());
            }

            if(left > right)
                continue;

            painter->fillRect(left, contentsRect.top(), right - left, contentsRect.height(), QColor(255, 0, 0, 96));
        }
    }

    r = contentsRect();
    qreal l = r.left() + QStyle::sliderPositionFromValue(minimum(), maximum(), maximum() - m_endSize, r.width());
    r = QRectF(l, r.top(), r.right() - l, r.height());
    
    linearGrad = QLinearGradient(r.topLeft(), r.topRight());
    linearGrad.setColorAt(0, QColor(0, 255, 0, 0));
    linearGrad.setColorAt(1, QColor(0, 255, 0, 128));
    painter->fillRect(r, linearGrad);

    painter->drawPixmap(m_handleRect, pix, QRectF(pix.rect()));
}

void MySlider::sliderChange(SliderChange change)
{
    GraphicsSlider::sliderChange(change);

    if (change == SliderValueChange && m_toolTip) {
        invalidateHandleRect();
        ensureHandleRect();
        m_toolTip->setPos(m_handleRect.center().x(), m_handleRect.top());
    }
}

QVariant MySlider::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemToolTipHasChanged && m_toolTip)
        m_toolTip->setText(value.toString());

    return GraphicsSlider::itemChange(change, value);
}

void MySlider::invalidateHandleRect()
{
    m_handleRectValid = false;
}

void MySlider::ensureHandleRect() const
{
    if(m_handleRectValid)
        return;

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    m_handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
}

const QRect &MySlider::handleRect() const
{
    ensureHandleRect();

    return m_handleRect;
}


// -------------------------------------------------------------------------- //
// TimeLine
// -------------------------------------------------------------------------- //
class TimeLine : public GraphicsFrame
{
    friend class TimeSlider; // ### for wheelEvent()

public:
    TimeLine(TimeSlider *parent) :
        GraphicsFrame(parent), m_parent(parent),
        m_dragging(false),
        m_scaleSpeed(1.0),
        m_prevWheelDelta(INT_MAX),
        m_cachedXPos(-INT64_MAX)
    {
        setFlag(QGraphicsItem::ItemClipsToShape); // ### paints out of shape, bitch

        setAcceptHoverEvents(true);

        m_lineAnimation = new QPropertyAnimation(m_parent, "currentValue", this);
        m_wheelAnimation = new QPropertyAnimation(m_parent, "scalingFactor", this);

        connect(m_wheelAnimation, SIGNAL(finished()), m_parent, SLOT(onWheelAnimationFinished()));
    }

    inline bool isDragging() const { return m_dragging; }
    void setDragging(bool dragging) { m_dragging = dragging; }

    void wheelAnimationFinished()
    {
        m_scaleSpeed = 1.0;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);

private:
    TimeSlider *m_parent;
    QPointF m_previousPos;
    int m_length;
    QPropertyAnimation *m_lineAnimation;
    QPropertyAnimation *m_wheelAnimation;
    bool m_dragging;
    double m_scaleSpeed;
    int m_prevWheelDelta;
    double m_cachedXPos;
};

void TimeLine::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    static const double SCALE_FACTOR = 2000.0;
    static const int WHEEL_ANIMATION_DURATION = 1000;
    //static const double SCALE_FACTOR = 120.0;
    int delta = event->delta();
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

    if (qAbs(delta) == 120)
    {
        m_scaleSpeed = qMin(m_scaleSpeed * 1.4, (double)10.0);

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

struct IntervalInfo {
    qint64 interval;
    int value;
    const char *name;
    int count;
    const char * maxText;

    bool (*isTimeAccepted) (const IntervalInfo& interval, qint64 time);
};

bool isTimeAcceptedStd(const IntervalInfo& interval, qint64 time)
{
    return time % interval.interval == 0;
}

bool isTimeAcceptedForMonth(const IntervalInfo& interval, qint64 time)
{
    return QDateTime::fromMSecsSinceEpoch(time).date().day() == 1;
}

bool isTimeAcceptedForYear(const IntervalInfo& interval, qint64 time)
{
    return QDateTime::fromMSecsSinceEpoch(time).date().dayOfYear() == 1;
}

IntervalInfo intervals[] = {
    {100ll, 100, "ms", 10, "ms", isTimeAcceptedStd},
    {1000ll, 1, "s", 60, "59s", isTimeAcceptedStd},
    {5ll*1000, 5, "s", 12, "59s", isTimeAcceptedStd},
    {10ll*1000, 10, "s", 6, "59s", isTimeAcceptedStd},
    {30ll*1000, 30, "s", 2, "59s", isTimeAcceptedStd},
    {60ll*1000, 1, "m", 60, "59m", isTimeAcceptedStd},
    {5ll*60*1000, 5, "m", 12, "59m", isTimeAcceptedStd},
    {10ll*60*1000, 10, "m", 6, "59m", isTimeAcceptedStd},
    {30ll*60*1000, 30, "m", 2, "59m", isTimeAcceptedStd},
    {60ll*60*1000, 1, "h", 24, "24h", isTimeAcceptedStd},
    {3ll*60*60*1000, 3, "h", 8, "24h", isTimeAcceptedStd},
    {6ll*60*60*1000, 6, "h", 4, "24h", isTimeAcceptedStd},
    {12ll*60*60*1000, 12, "h", 2, "24h", isTimeAcceptedStd},
    {24ll*60*60*1000, 1, "dd MMM", 99, "dd MMM", isTimeAcceptedStd}, // FIRST_DATE_INDEX here
    {24ll*60*60*1000*30, 1, "MMMM", 99, "September", isTimeAcceptedForMonth},
    {24ll*60*60*1000*30*12, 1, "yyyy", 99, "2011", isTimeAcceptedForYear}
};
static const int FIRST_DATE_INDEX = 13; // use date's labels with this index and above

static inline qint64 roundTime(qint64 msecs, int interval)
{
    return msecs - msecs%(intervals[interval].interval);
}

static inline void drawGradient(QPainter *painter, const QRectF &r, float height)
{
    int MAX_COLOR = 64+16;
    int MIN_COLOR = 0;
    for (int i = r.top(); i < r.bottom(); ++i)
    {
        int k = height - i;
        k *= float(MAX_COLOR-MIN_COLOR) / height + MIN_COLOR;
        painter->setPen(QColor(k,k,k,k));
        painter->drawLine(QLineF(r.left(), i, r.width(), i));
    }
}

void TimeLine::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    GraphicsFrame::paint(painter, option, widget);

    qreal halfHandleThickness = m_parent->m_slider->handleRect().width() / 2.0;

    const QPalette pal = m_parent->palette();
    painter->setBrush(pal.brush(QPalette::Base));
    painter->setPen(pal.color(QPalette::Base));
    const QRectF r(halfHandleThickness, 0, rect().width() - halfHandleThickness * 2, rect().height() - 2*frameWidth());
    painter->drawRect(r);
    drawGradient(painter, QRectF(0, 0, rect().width(), rect().height() - 2*frameWidth()), rect().height());
    painter->setPen(pal.color(QPalette::Text));

    qint64 timezoneOffset = 0;
    if (m_parent->minimumValue() != 0) {
        QDateTime dt1 = QDateTime::currentDateTime();
        QDateTime dt2 = dt1.toUTC();
        dt1.setTimeSpec(Qt::UTC);
        timezoneOffset = dt2.secsTo(dt1) * 1000ll;
    }

    const qint64 range = m_parent->sliderRange();
    const qint64 pos = m_parent->viewPortPos() + timezoneOffset;
    const double pixelPerTime = r.width() / range;

    const int minWidth = 30;

    int level = 0;
    while (minWidth > pixelPerTime*intervals[level].interval && level < FIRST_DATE_INDEX)
        ++level;

    int maxLevel = level;
    while (maxLevel < arraysize(intervals)-1 && intervals[maxLevel].interval <= range)
        ++maxLevel;

    int maxLen = maxLevel - level;

    double xpos = r.left() - qRound64(pixelPerTime * pos);
    int outsideCnt = -xpos / (intervals[level].interval * pixelPerTime);
    xpos += outsideCnt * intervals[level].interval * pixelPerTime;
    if ((pos+range - intervals[level].interval*outsideCnt) / r.width() <= 1) // abnormally long range
        return;

    if (qAbs(m_cachedXPos - xpos) < 1.0)
        xpos = m_cachedXPos;
    else
        m_cachedXPos = xpos;

    QColor color = pal.color(QPalette::Text);

    QVector<float> opacity;
    int maxHeight = 0;
    QVector<int> widths;
    QVector<QFont> fonts;
    for (int i = level; i <= maxLevel; ++i)
    {
        const IntervalInfo &interval = intervals[i];
        const QString text = QLatin1String(interval.maxText);

        QFont font = m_parent->font();
        font.setPointSize(font.pointSize() + (i >= maxLevel-1) - (i <= level+1));
        font.setBold(i == maxLevel);

        QFontMetrics metric(font);
        float textWidth = metric.width(text);
        float sc = textWidth / (pixelPerTime*interval.interval);
        while (sc > 1.0f/1.1f)
        {
            font.setPointSizeF(font.pointSizeF()-0.5);
            if (font.pointSizeF() < MIN_FONT_SIZE)
            {
                textWidth = 0;
                break;
            }

            metric = QFontMetrics(font);
            textWidth = metric.width(text);
            sc = textWidth / (pixelPerTime*interval.interval);
        }

        fonts << font;
        widths << textWidth;
        if (textWidth > 0 && maxHeight < metric.height())
            maxHeight = metric.height();
        opacity << qBound(MAX_MIN_OPACITY, float(pixelPerTime*interval.interval - minWidth)/60.0f, 1.0f);
    }

    // draw grid
    for (qint64 curTime = intervals[level].interval*outsideCnt; curTime <= pos+range; curTime += intervals[level].interval)
    {
        int curLevel = level;
        while (curLevel < maxLevel && intervals[curLevel+1].isTimeAccepted(intervals[curLevel+1], curTime))
            ++curLevel;

        const int arrayIndex = qMin(curLevel-level, opacity.size() - 1);

        color.setAlphaF(opacity[arrayIndex]);
        painter->setPen(color);
        painter->setFont(fonts[arrayIndex]);

        const IntervalInfo &interval = intervals[curLevel];
        const float lineLen = qMin(float(curLevel-level+1) / maxLen, 1.0f);
        painter->drawLine(QPointF(xpos, 0), QPointF(xpos, (r.height() - maxHeight - 3) * lineLen));

        QString text;
        if (curLevel < FIRST_DATE_INDEX)
        {
            const int labelNumber = (curTime/interval.interval)%interval.count;
            text = QString::number(interval.value*labelNumber) + QLatin1String(interval.name);
        }
        else
        {
            text = QDateTime::fromMSecsSinceEpoch(curTime).toString(interval.name);
        }

        if (widths[arrayIndex] > 0)
        {
            if (curLevel == 0 /*|| (curLevel == arraysize(intervals) - 1 && m_parent->minimumValue() != 0) */)
            {
                painter->save();
                painter->translate(xpos-3, (r.height() - maxHeight) * lineLen);
                painter->rotate(90);
                painter->drawText(2, 0, curLevel == 0 ? text : QDateTime::fromMSecsSinceEpoch(curTime).toString(Qt::ISODate));
                painter->restore();
            }
            else
            {
                QRectF textRect(xpos - widths[arrayIndex]/2, (r.height() - maxHeight) * lineLen, widths[arrayIndex], 128);
                if (textRect.left() < 0 && pos == 0)
                    textRect.setLeft(0);
                painter->drawText(textRect, Qt::AlignHCenter, text);
            }
        }
        
        xpos += intervals[level].interval * pixelPerTime;
    }

    // draw cursor vertical line
    painter->setPen(QPen(Qt::red, 3));
    xpos = r.left() + m_parent->m_slider->value() * r.width() / (m_parent->m_slider->maximum() - m_parent->m_slider->minimum());
    painter->drawLine(QLineF(xpos, 0, xpos, rect().height()));
}

void TimeLine::mousePressEvent(QGraphicsSceneMouseEvent *me)
{
    if (me->button() == Qt::LeftButton) {
        me->accept();
        m_lineAnimation->stop();
        if (qFuzzyIsNull(m_parent->scalingFactor()))
            m_parent->setCurrentValue(m_parent->viewPortPos() + qRound64((double(m_parent->sliderRange()) / rect().width()) * me->pos().x()));
        m_parent->setMoving(true);
        m_previousPos = me->pos();
        m_length = 0;
    }
}

void TimeLine::mouseMoveEvent(QGraphicsSceneMouseEvent *me)
{
    if (me->buttons() & Qt::LeftButton) {
        me->accept();

        const bool bUnscaled = qFuzzyIsNull(m_parent->scalingFactor());
        // in fact, we need to use (width - slider handle thinkness/2), but it is ok without it
        const QPointF dpos = bUnscaled ? me->pos() - m_previousPos : m_previousPos - me->pos();
        if (!isDragging() && qAbs(dpos.x()) < QApplication::startDragDistance())
            return;

        setDragging(true);
        m_previousPos = me->pos();
        m_length = dpos.x();

        qint64 dtime = qRound64((double(m_parent->sliderRange()) / rect().width()) * dpos.x());
        if (bUnscaled) {
            m_parent->setCurrentValue(qBound(m_parent->viewPortPos(), m_parent->currentValue() + dtime, m_parent->viewPortPos() + m_parent->sliderRange()));
        } else if (m_parent->centralise()) {
            dtime = dtime < 0 ? qMax(dtime, -m_parent->viewPortPos())
                              : qMin(dtime, m_parent->maximumValue() - (m_parent->viewPortPos() + m_parent->sliderRange()));
            m_parent->setCurrentValue(m_parent->currentValue() + dtime);
        } else {
            m_parent->setViewPortPos(m_parent->viewPortPos() + dtime);
        }
    }
}

void TimeLine::mouseReleaseEvent(QGraphicsSceneMouseEvent *me)
{
    if (me->button() == Qt::LeftButton) {
        me->accept();
        if (isDragging()) {
            setDragging(false);

            if (qAbs(m_length) > 5) {
                int dx = m_length*2/**(35.0/t.elapsed())*/;

                m_lineAnimation->setStartValue(m_parent->viewPortPos());
                m_lineAnimation->setEasingCurve(QEasingCurve::OutQuad);
                m_lineAnimation->setEndValue(m_parent->viewPortPos() + qRound64((double(m_parent->sliderRange()) / rect().width()) * dx));
                m_lineAnimation->setDuration(1000);
                //m_lineAnimation->start();
            }
        } else {
            m_parent->setCurrentValue(m_parent->viewPortPos() + qRound64((double(m_parent->sliderRange()) / rect().width()) * me->pos().x()));
        }
        m_parent->setMoving(false);
    }
}

void TimeLine::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *)
{
    QPropertyAnimation *scaleAnimation = new QPropertyAnimation(m_parent, "scalingFactor", this);
    scaleAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    scaleAnimation->setStartValue(m_parent->scalingFactor());
    scaleAnimation->setEndValue(0);
    scaleAnimation->setDuration(1000);
    scaleAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}


// -------------------------------------------------------------------------- //
// TimeSlider
// -------------------------------------------------------------------------- //
/*!
    \class TimeSlider
    \brief The TimeSlider class allows to navigate over time line.

    This class consists of slider to set position and time line widget, which shows time depending
    on scalingFactor. Also TimeSlider allows to zoom in/out scale of time line, which allows to
    navigate more accurate.
*/
TimeSlider::TimeSlider(QGraphicsItem *parent) :
    GraphicsWidget(parent),
    m_minimumValue(0),
    m_maximumValue(10 * 1000), // 10 sec
    m_currentValue(0),
    m_viewPortPos(0),
    m_minimumRange(5 * 1000), // 5 sec
    m_scalingFactor(0.0),
    m_delta(0),
    m_isUserInput(false),
    m_centralise(true),
    m_endSize(0.0)
{
    setAcceptHoverEvents(true);

    qRegisterAnimationInterpolator<qint64>(qint64Interpolator);

    m_animation = new QPropertyAnimation(this, "viewPortPos", this);

    m_slider = new MySlider(this);
    m_slider->setOrientation(Qt::Horizontal);
    m_slider->setContentsMargins(0, 4, 0, 4);
    m_slider->setMaximum(10000);
    m_slider->installEventFilter(this);

    m_slider->setStyle(sliderProxyStyle());

    m_timeLine = new TimeLine(this);
    m_timeLine->setLineWidth(0);
    m_timeLine->setFrameShape(GraphicsFrame::Box);
    m_timeLine->setFrameShadow(GraphicsFrame::Sunken);
    m_timeLine->installEventFilter(this);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addItem(m_slider);
    layout->addItem(m_timeLine);
    setLayout(layout);

    connect(m_slider, SIGNAL(sliderPressed()), this, SIGNAL(sliderPressed()));
    connect(m_slider, SIGNAL(sliderReleased()), this, SIGNAL(sliderReleased()));
    connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(centraliseSlider()), Qt::QueuedConnection);
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onSliderValueChanged(int)));
}

TimeSlider::~TimeSlider()
{
    m_animation->stop();
}

void TimeSlider::setToolTipItem(ToolTipItem *toolTip)
{
    m_slider->setToolTipItem(toolTip);
}

/*!
    Returns the difference between maximumValue and minimumValue.
*/
qint64 TimeSlider::length() const
{
    return maximumValue() - m_minimumValue;
}

/*!
    \property TimeSlider::minimumValue
    \brief the sliders minimum value in milliseconds

    Minimum value can't be greater than maximum value.

    \sa currentValue
*/
qint64 TimeSlider::minimumValue() const
{
    return m_minimumValue;
}

void TimeSlider::setMinimumValue(qint64 value)
{
    if (m_minimumValue == value)
        return;

    //m_minimumValue = qMin(value, m_maximumValue);
    m_minimumValue = value;

    update();

    Q_EMIT minimumValueChanged(m_minimumValue);
}

/*!
    \property TimeSlider::maximumValue
    \brief the sliders maximum value in milliseconds

    \sa currentValue
*/
qint64 TimeSlider::maximumValue() const
{
    return m_maximumValue == DATETIME_NOW ? QDateTime::currentDateTime().toMSecsSinceEpoch() : m_maximumValue;
}

void TimeSlider::setMaximumValue(qint64 value)
{
    if (m_maximumValue == value)
        return;

    //m_maximumValue = qMax(value, m_minimumValue + 1000);
    m_maximumValue = value;

    update();

    Q_EMIT maximumValueChanged(m_maximumValue);
}

/*!
    \property TimeSlider::currentValue
    \brief the sliders current value in milliseconds

    This value is always in range from minimumValue ms to maximumValue ms.

    \sa TimeSlider::maximumValue
*/
qint64 TimeSlider::currentValue() const
{
    return m_currentValue;
}

void TimeSlider::setCurrentValue(qint64 value)
{
/*    if (value == DATETIME_NOW)
        return; // ###*/

    value = qBound(m_minimumValue, value, maximumValue());
    if (m_currentValue == value)
        return;

    m_currentValue = value;

    updateSlider();
    update();

    if (!m_isUserInput/* && !isMoving()*/)
        centraliseSlider();

    Q_EMIT currentValueChanged(m_currentValue);
}

/*!
    \property TimeSlider::scalingFactor
    \brief the scaling factor of the time line

    Scaling factor is always greater or equal to 0. If factor is 0, slider shows all time line, if
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
    m_scalingFactor = qMax(factor, double(0.0));
    if (sliderRange() < m_minimumRange)
        m_scalingFactor = oldfactor;
    //setViewPortPos(currentValue() - m_slider->value()*delta());
    setViewPortPos(currentValue() - sliderRange()/2);
}

QSizeF TimeSlider::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    return m_slider->sizeHint(which, constraint) + QSizeF(0, 40);
}

bool TimeSlider::isMoving() const
{
    return m_slider->isSliderDown();
}

void TimeSlider::setMoving(bool b)
{
    m_slider->setSliderDown(b);
}

bool TimeSlider::isAtEnd() 
{
    return m_slider->isAtEnd();
}

void TimeSlider::setEndSize(qreal size)
{
    m_endSize = size;
}

void TimeSlider::onSliderValueChanged(int value)
{
    if (!m_isUserInput && !isMoving()) {
        m_isUserInput = true;
        setCurrentValue(fromSlider(value));
        m_isUserInput = false;
    }
}

qint64 TimeSlider::viewPortPos() const
{
    return m_viewPortPos;
}

void TimeSlider::setViewPortPos(qint64 value)
{
    m_viewPortPos = qBound(m_minimumValue, value, maximumValue() - sliderRange());

    if (m_currentValue < m_viewPortPos) {
        setCurrentValue(m_viewPortPos);
    } else if (m_currentValue > m_viewPortPos + sliderRange()) {
        setCurrentValue(m_viewPortPos + sliderRange());
    } else {
        updateSlider();
        update();
    }
}

/*!
    \returns Milliseconds per one unit of the underlying slider.
*/
double TimeSlider::delta() const
{
    return ((1.0f/(m_slider->maximum() - m_slider->minimum()))*length())/qExp(scalingFactor()/2);
}

qint64 TimeSlider::fromSlider(int value)
{
    return m_viewPortPos + value * delta();
}

int TimeSlider::toSlider(qint64 value)
{
    return (value - m_viewPortPos) / delta();
}

/*!
    \returns Slider range in milliseconds.
*/
qint64 TimeSlider::sliderRange() const
{
    return (m_slider->maximum() - m_slider->minimum())*delta();
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

    if(isAtEnd()) {
        m_slider->setToolTip("Live");
    } else {
        if (m_minimumValue == 0)
            m_slider->setToolTip(formatDuration(m_currentValue / 1000));
        else
            m_slider->setToolTip(QDateTime::fromMSecsSinceEpoch(m_currentValue).toString(Qt::SystemLocaleShortDate));
    }
}

void TimeSlider::centraliseSlider()
{
    if (!m_centralise)
        return;

    if (m_timeLine->isDragging() || (!isMoving() && m_animation->state() != QAbstractAnimation::Running /*&& !m_userInput*/))
    {
        setViewPortPos(m_currentValue - sliderRange()/2);
    }
    else if (m_animation->state() != QPropertyAnimation::Running /*&& !isMoving()*/)
    {
        qint64 newViewortPos = m_currentValue - sliderRange()/2; // center
        newViewortPos = qBound(m_minimumValue, newViewortPos, maximumValue() - sliderRange());
        if (qAbs(newViewortPos - m_viewPortPos) < 2*delta()) {
            setViewPortPos(m_currentValue - sliderRange()/2);
            return;
        }
        m_animation->stop();
        m_animation->setDuration(500);
        m_animation->setEasingCurve(QEasingCurve::InOutQuad);
        m_animation->setStartValue(m_viewPortPos);
        m_animation->setEndValue(newViewortPos);
        m_animation->start();
    }
}

void TimeSlider::onWheelAnimationFinished()
{
    m_timeLine->wheelAnimationFinished();
}

bool TimeSlider::eventFilter(QObject *target, QEvent *event)
{
    if (target == m_slider)
    {
        switch (event->type())
        {
        case QEvent::GraphicsSceneWheel:
            //QApplication::sendEvent(m_timeLine, event);
            m_timeLine->wheelEvent(static_cast<QGraphicsSceneWheelEvent *>(event)); // ### direct call?
            return true;
        case QEvent::GraphicsSceneMouseDoubleClick:
            return true;
        default:
            break;
        }
    }

    return GraphicsWidget::eventFilter(target, event);
}

void TimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) 
{
    m_slider->setEndSize(m_endSize * (m_slider->maximum() - m_slider->minimum()) / (m_slider->size().width())); // Hack hack hack

    GraphicsWidget::paint(painter, option, widget);
}



