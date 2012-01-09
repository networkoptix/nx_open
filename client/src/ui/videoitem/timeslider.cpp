#include "timeslider.h"

#include <QPainterPath>
#include <QGradient>
#include <QtCore/QDateTime>
#include <QtCore/QPair>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QMenu>
#include <QtGui/QPainter>

#include <utils/common/util.h>

#include "ui/skin/skin.h"
#include "ui/proxystyle.h"

#include "ui/widgets2/graphicsframe.h"
#include "ui/widgets2/graphicsslider.h"
#include "ui/widgets/tooltipitem.h"

#include "ui/context_menu/menu_wrapper.h"

#include <qmath.h>

//#define TIMESLIDER_ANIMATED_DRAG

namespace {

QVariant qint64Interpolator(const qint64 &start, const qint64 &end, qreal progress)
{
    return start + double(end - start) * progress;
}

static const float MIN_MIN_OPACITY = 0.1f;
static const float MAX_MIN_OPACITY = 0.6f;
static const float MIN_FONT_SIZE = 4.0f;
static const float FONT_SIZE_CACHE_EPS = 0.1;
static const int MAX_LABEL_WIDTH = 64;


typedef QHash<unsigned, QImage*> TextCache;

}


// -------------------------------------------------------------------------- //
// SliderProxyStyle
// -------------------------------------------------------------------------- //
class SliderProxyStyle : public ProxyStyle
{
public:
    SliderProxyStyle(QObject *parent = 0) : ProxyStyle(0, parent) {}

    int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    {
        if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
            return Qt::LeftButton;
        return ProxyStyle::styleHint(hint, option, widget, returnData);
    }

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget = 0) const
    {
        QRect r = ProxyStyle::subControlRect(cc, opt, sc, widget);
        if (cc == CC_Slider && sc == SC_SliderHandle) {
            int side = qMin(r.width(), r.height());
            if (qstyleoption_cast<const QStyleOptionSlider *>(opt)->orientation == Qt::Horizontal)
                r.setWidth(side);
            else
                r.setHeight(side);
        }
        return r;
    }
};


// -------------------------------------------------------------------------- //
// MySlider
// -------------------------------------------------------------------------- //
class MySlider : public GraphicsSlider
{
public:
    MySlider(TimeSlider *parent);

    ToolTipItem *toolTipItem() const;
    void setToolTipItem(ToolTipItem *toolTip);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    const QRectF &handleRect() const;

    bool isAtEnd() const;

    void setEndSize(int size);

protected:
    void sliderChange(SliderChange change);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    void invalidateHandleRect();
    void ensureHandleRect() const;
    void drawTimePeriods(QPainter *painter, const QnTimePeriodList& timePeriods, QColor color);
private:
    TimeSlider *m_parent;
    ToolTipItem *m_toolTip;
    mutable QRectF m_handleRect;
    int m_endSize;
    QPixmap m_pixmap;
};

MySlider::MySlider(TimeSlider *parent)
    : GraphicsSlider(parent),
      m_parent(parent),
      m_toolTip(0),
      m_endSize(0)
{
    setToolTipItem(new StyledToolTipItem);
}

bool MySlider::isAtEnd() const
{
    return m_endSize == 0 || value() > maximum() - m_endSize;
}

void MySlider::setEndSize(int size)
{
    m_endSize = size;
}

ToolTipItem *MySlider::toolTipItem() const
{
    return m_toolTip;
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

void MySlider::drawTimePeriods(QPainter *painter, const QnTimePeriodList& timePeriods, QColor color)
{
    const qint64 range = m_parent->sliderRange();
    const qint64 pos = m_parent->viewPortPos() /*+ timezoneOffset*/;

    QRectF contentsRect = this->contentsRect();
    qreal x = contentsRect.left() + m_handleRect.width() / 2;
    qreal w = contentsRect.width() - m_handleRect.width();

    // TODO: use lower_bound here and draw only what is actually needed.
    foreach(const QnTimePeriod &period, timePeriods)
    {
        qreal left = x + static_cast<qreal>(period.startTimeMs - pos) / range * w;
        left = qMax(left, contentsRect.left());

        qreal right;
        if (period.durationMs == -1) {
            right = contentsRect.right();
        } else {
            right = x + static_cast<qreal>((period.startTimeMs + period.durationMs) - pos) / range * w;
            right = qMin(right, contentsRect.right());
        }

        if (left > right)
            continue;

        painter->fillRect(left, contentsRect.top(), qMax(1.0,right - left), contentsRect.height(), color);
    }
}

void MySlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //qint64 t = getUsecTimer();

    Q_UNUSED(option)
    Q_UNUSED(widget)

    QRectF r = contentsRect();

    painter->setPen(QPen(Qt::gray, 2));
    painter->drawRect(r);

    //painter->setPen(QPen(Qt::darkGray, 1));
    //painter->drawRect(r);

    r.setRight(m_handleRect.center().x());
    //painter->setPen(QPen(QColor(0, 87, 207), 2));
    //painter->drawRect(r);

    
    QLinearGradient linearGrad(r.topLeft(), r.bottomRight());
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(1, QColor(186, 239, 255));
    //painter->setPen(QPen(Qt::green, 0));
    painter->setBrush(linearGrad);
    //painter->drawRect(r);
    painter->fillRect(r, linearGrad);
    

    // Draw time periods
    if (!m_parent->recTimePeriodList().empty())
        drawTimePeriods(painter, m_parent->recTimePeriodList(), QColor(255, 0, 0));
    if (!m_parent->motionTimePeriodList().empty())
        drawTimePeriods(painter, m_parent->motionTimePeriodList(), QColor(0, 255, 0));

    r = contentsRect();
    qreal l = r.left() + QStyle::sliderPositionFromValue(minimum(), maximum(), maximum() - m_endSize, r.width());
    r = QRectF(l, r.top(), r.right() - l, r.height());

    linearGrad = QLinearGradient(r.topLeft(), r.topRight());
    linearGrad.setColorAt(0, QColor(0, 255, 0, 0));
    linearGrad.setColorAt(1, QColor(0, 255, 0, 128));
    painter->fillRect(r, linearGrad);


    ensureHandleRect();
    if (m_pixmap.width() == 0)
        m_pixmap = Skin::pixmap(QLatin1String("slider-handle.png"), m_handleRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter->drawPixmap(m_handleRect.topLeft(), m_pixmap);

    //qDebug() << "timeFinal=" << (getUsecTimer() - t)/1000.0;
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
    if (change == ItemToolTipHasChanged && m_toolTip) {
        m_toolTip->setText(value.toString());
        m_toolTip->setVisible(!m_toolTip->text().isEmpty());
    }

    return GraphicsSlider::itemChange(change, value);
}

void MySlider::invalidateHandleRect()
{
    m_handleRect = QRectF();
}

void MySlider::ensureHandleRect() const
{
    if (!m_handleRect.isValid()) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        m_handleRect = QRectF(style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle));
    }
}

const QRectF &MySlider::handleRect() const
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
        m_rangeSelectionState(NoRangeSelected),
        m_dragging(false),
        m_scaleSpeed(1.0),
        m_prevWheelDelta(INT_MAX),
        m_cachedXPos(INT64_MIN),
        m_cachedOutsideCnt(INT64_MIN),
        m_gradientReady(false),
        m_maxHeight(0),
        m_cachedFontLevel(-1),
        m_cachedFontMaxLevel(-1),
        m_cachedFontPixelPerTime(-1)
    {
        setFlag(QGraphicsItem::ItemClipsToShape); // ### paints out of shape, bitch

        setAcceptHoverEvents(true);

#ifdef TIMESLIDER_ANIMATED_DRAG
        m_length = 0;
        m_lineAnimation = new QPropertyAnimation(m_parent, "currentValue", this);
#endif
        m_wheelAnimation = new QPropertyAnimation(m_parent, "scalingFactor", this);

        connect(m_wheelAnimation, SIGNAL(finished()), m_parent, SLOT(onWheelAnimationFinished()));

        QDateTime dt1 = QDateTime::currentDateTime();
        QDateTime dt2 = dt1.toUTC();
        dt1.setTimeSpec(Qt::UTC);
        m_timezoneOffset = dt2.secsTo(dt1) * 1000ll;
    }
    ~TimeLine()
    {
        foreach(QImage* image, m_textCache)
            delete image;
    }

    QPair<qint64, qint64> selectionRange() const;
    void setSelectionRange(const QPair<qint64, qint64> &range);
    inline void setSelectionRange(qint64 begin, qint64 end)
    { setSelectionRange(QPair<qint64, qint64>(begin, end)); }
    inline void resetSelectionRange()
    { setSelectionRange(0, 0); }

    inline bool isDragging() const { return m_dragging; }
    void setDragging(bool dragging) { m_dragging = dragging; }

    void wheelAnimationFinished()
    {
        m_scaleSpeed = 1.0;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    void updateFontCache(int level, int maxLevel, double pixelPerTime);
    QImage* createTextImage(const QFont& font, const QString& text, bool doRotate);
private:
    int m_maxHeight;
    QVector<int> m_fontWidths;
    QVector<QFont> m_fonts;
    double m_cachedFontPixelPerTime;
    int m_cachedFontLevel;
    int m_cachedFontMaxLevel;

    qint64 posToValue(qreal pos) const;
    qreal valueToPos(qint64 value) const;

private:
    TimeSlider *m_parent;
    QPointF m_previousPos;
#ifdef TIMESLIDER_ANIMATED_DRAG
    int m_length;
    QPropertyAnimation *m_lineAnimation;
#endif
    QPropertyAnimation *m_wheelAnimation;

    enum {
        NoRangeSelected = 0,
        SelectingRangeBegin,
        SelectingRangeEnd,
        RangeSelected
    } m_rangeSelectionState;

    bool m_dragging;
    double m_scaleSpeed;
    int m_prevWheelDelta;
    double m_cachedXPos;
    double m_cachedOutsideCnt;
    QBrush m_gradient;
    bool m_gradientReady;
    QPair<qint64, qint64> m_selectedRange;
    TextCache m_textCache;
    qint64 m_timezoneOffset;
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

bool isTimeAcceptedForMonth(const IntervalInfo& /*interval*/, qint64 time)
{
    return QDateTime::fromMSecsSinceEpoch(time).date().day() == 1;
}

bool isTimeAcceptedForYear(const IntervalInfo& /*interval*/, qint64 time)
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

void TimeLine::updateFontCache(int level, int maxLevel, double pixelPerTime)
{
    m_fonts.clear();
    m_fontWidths.clear();
    m_cachedFontPixelPerTime = pixelPerTime;
    m_maxHeight = 0;
    for (int i = level; i <= maxLevel; ++i)
    {
        const IntervalInfo &interval = intervals[i];
        const QString& text = interval.maxText;

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

        m_fonts << font;
        m_fontWidths << textWidth;
        if (textWidth > 0 && m_maxHeight < metric.height())
            m_maxHeight = metric.height();
    }
    m_cachedFontLevel = level;
    m_cachedFontMaxLevel = maxLevel;
}

QImage* TimeLine::createTextImage(const QFont& font, const QString& text, bool doRotate)
{
    // Draw text line to image cache
    QFontMetrics m(font);
    QSize textSize = m.size(Qt::TextSingleLine, text);
    QImage* textImage = new QImage(textSize.width(), textSize.height(), QImage::Format_ARGB32_Premultiplied);
    textImage->fill(0);
    QPainter p(textImage);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.setPen(m_parent->palette().color(QPalette::Text));
    p.setFont(font);
    p.drawText(0,  m.ascent(), text);
    p.end();

    if (doRotate) {
        QMatrix rm;
        rm.rotate(90);
        *textImage = textImage->transformed(rm);
    }
    return textImage;
}

void TimeLine::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //qint64 t = getUsecTimer();

    //GraphicsFrame::paint(painter, option, widget);

    const qreal halfHandleThickness = m_parent->m_slider->handleRect().width() / 2.0;
    const QRectF r(halfHandleThickness, 0, rect().width() - halfHandleThickness * 2, rect().height() - frameWidth() * 2);
    if (qFuzzyIsNull(r.width()))
        return;

    if (!m_gradientReady)
    {
        QLinearGradient gradient(QPointF(0,0), QPointF(0, r.height()));
        gradient.setColorAt(0.0, QColor(64,64,64,64));
        gradient.setColorAt(1.0, QColor(144,144,144,144));
        m_gradient = QBrush(gradient);
        m_gradientReady = true;
    }
    painter->fillRect(r, m_gradient);

    const QPalette pal = m_parent->palette();
    painter->setPen(pal.color(QPalette::Text));

    const qint64 range = m_parent->sliderRange();
    qint64 pos = m_parent->viewPortPos();
    if (m_parent->minimumValue() != 0)
        pos += m_timezoneOffset;
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
    if (outsideCnt - m_cachedOutsideCnt == 1)
        outsideCnt = m_cachedOutsideCnt;
    else
        m_cachedOutsideCnt = outsideCnt;

    xpos += outsideCnt * intervals[level].interval * pixelPerTime;
    if ((pos+range - intervals[level].interval*outsideCnt) / r.width() <= 1) // abnormally long range
        return;

    if (qAbs(m_cachedXPos - xpos) < 1.0)
        xpos = m_cachedXPos;
    else
        m_cachedXPos = xpos;

    QColor color = pal.color(QPalette::Text);

    QVector<float> opacity;
    QVector<QPainterPath> paths;
    paths.resize(maxLevel-level+1);
    opacity.resize(maxLevel-level+1);

    if (level != m_cachedFontLevel || maxLevel != m_cachedFontMaxLevel || qAbs( 1.0 - pixelPerTime/m_cachedFontPixelPerTime) > FONT_SIZE_CACHE_EPS)
        updateFontCache(level, maxLevel, pixelPerTime);
    for (int i = level; i <= maxLevel; ++i)
        opacity[i-level] = qBound(MAX_MIN_OPACITY, float(pixelPerTime*intervals[i].interval - minWidth)/60.0f, 1.0f);

    // draw grid
    for (qint64 curTime = intervals[level].interval*outsideCnt; curTime <= pos+range; curTime += intervals[level].interval)
    {
        int curLevel = level;
        while (curLevel < maxLevel && intervals[curLevel+1].isTimeAccepted(intervals[curLevel+1], curTime))
            ++curLevel;

        const int arrayIndex = qMin(curLevel-level, opacity.size() - 1);

        const IntervalInfo &interval = intervals[curLevel];
        const float lineLen = qMin(float(curLevel-level+1) / maxLen, 1.0f);
        paths[arrayIndex].moveTo(QPointF(xpos, 0));
        paths[arrayIndex].lineTo(QPointF(xpos, (r.height() - m_maxHeight - 3) * lineLen));

        if (m_fontWidths[arrayIndex] > 0)
        {
            const int labelNumber = (curTime/interval.interval)%interval.count;
            int fontSize = m_fonts[arrayIndex].pointSizeF()*2;
            unsigned hash;
            if (curLevel < FIRST_DATE_INDEX)
                hash = (128 + (fontSize << 24)) + (curLevel << 16) + interval.value*labelNumber;
            else 
                hash = (fontSize << 24) + curTime/1000/900; // curTime to 15 min granularity (we must keep integer number for months and years dates)
            
            QImage* textImage = m_textCache.value(hash);
            if (textImage == 0)
            {
                QString text;
                if (curLevel < FIRST_DATE_INDEX)
                    text = QString::number(interval.value*labelNumber) + QLatin1String(interval.name);
                else
                    text = QDateTime::fromMSecsSinceEpoch(curTime).toString(interval.name);

                textImage= createTextImage(m_fonts[arrayIndex], text, curLevel == 0);
                m_textCache.insert(hash, textImage);
            }

            QPointF imagePos(xpos - textImage->width()/2, (r.height() - m_maxHeight) * lineLen + (curLevel == 0 ? 2 : 0) );
            if (imagePos.x() < 0 && pos == 0)
                imagePos.setX(0);
            painter->setOpacity(opacity[arrayIndex]);
            painter->drawImage(imagePos, *textImage);
        }

        xpos += intervals[level].interval * pixelPerTime;
    }

    painter->setOpacity(1.0);
    for (int arrayIndex = 0; arrayIndex < paths.size(); ++arrayIndex)
    {
        color.setAlphaF(opacity[arrayIndex]);
        painter->setPen(color);
        painter->drawPath(paths[arrayIndex]);
    }

    // draw selection range
    if (m_rangeSelectionState != NoRangeSelected)
    {
        const qreal rangeBegin = m_rangeSelectionState >= SelectingRangeBegin ? valueToPos(m_selectedRange.first) : 0.0;
        const qreal rangeEnd = m_rangeSelectionState >= SelectingRangeEnd ? valueToPos(m_selectedRange.second) : 0.0;

        if (m_rangeSelectionState >= SelectingRangeEnd)
        {
            const QColor selectionColor(120, 150, 180, m_rangeSelectionState == RangeSelected ? 150 : 50);
            painter->fillRect(QRectF(QPointF(rangeBegin, 0), QPointF(rangeEnd, rect().height())), selectionColor);
        }
        painter->setPen(QPen(Qt::red, 0));
        if (m_rangeSelectionState >= SelectingRangeBegin)
            painter->drawLine(QLineF(rangeBegin, 0, rangeBegin, rect().height()));
        if (m_rangeSelectionState >= SelectingRangeEnd)
            painter->drawLine(QLineF(rangeEnd, 0, rangeEnd, rect().height()));
    }

    // draw cursor vertical line
    painter->setPen(QPen(Qt::red, 3));
    xpos = r.left() + m_parent->m_slider->value() * r.width() / (m_parent->m_slider->maximum() - m_parent->m_slider->minimum());
    painter->drawLine(QLineF(xpos, 0, xpos, rect().height()));

    //qDebug() << "time=" << (getUsecTimer() - t)/1000.0;
}

qint64 TimeLine::posToValue(qreal pos) const
{
    return m_parent->viewPortPos() + qRound64((double(m_parent->sliderRange()) / rect().width()) * pos);
}

qreal TimeLine::valueToPos(qint64 value) const
{
    return (double(value - m_parent->viewPortPos()) / m_parent->sliderRange()) * rect().width();
}

QPair<qint64, qint64> TimeLine::selectionRange() const
{
    return m_selectedRange;
}

void TimeLine::setSelectionRange(const QPair<qint64, qint64> &range)
{
    if (range.first <= range.second || range.second == 0) {
        m_selectedRange.first = range.first;
        m_selectedRange.second = range.second;
    } else {
        m_selectedRange.first = range.second;
        m_selectedRange.second = range.first;
    }

    m_rangeSelectionState = range.first == range.second || range.second == 0
                            ? range.first == 0 ? NoRangeSelected
                                               : SelectingRangeEnd
                            : RangeSelected;

    update();
}

void TimeLine::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QScopedPointer<QMenu> menu(new QMenu(event->widget()));
    QAction *selectRangeAction = menu->addAction(tr("Select Range"));
    menu->addSeparator();
    QAction *clearRangeAction = menu->addAction(tr("Clear Range"));
    clearRangeAction->setEnabled(m_rangeSelectionState == RangeSelected);
    QAction *exportRangeAction = menu->addAction(tr("Export Selected Range..."));
    exportRangeAction->setEnabled(m_rangeSelectionState == RangeSelected);

    QAction *selectedAction = menu->exec(event->screenPos());

    if (selectedAction == selectRangeAction) {
        resetSelectionRange();
        m_rangeSelectionState = SelectingRangeBegin;
    } else if (selectedAction == clearRangeAction) {
        resetSelectionRange();
    } else if (selectedAction == exportRangeAction) {
        Q_EMIT m_parent->exportRange(m_selectedRange.first, m_selectedRange.second);
    }
}

void TimeLine::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept();
    if (m_rangeSelectionState == SelectingRangeBegin || m_rangeSelectionState == SelectingRangeEnd) {
        const qint64 value = posToValue(event->pos().x());
        if (m_rangeSelectionState == SelectingRangeBegin)
            m_selectedRange.first = value;
        else
            m_selectedRange.second = value;
        update();
    }
}

void TimeLine::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();

#ifdef TIMESLIDER_ANIMATED_DRAG
        m_length = 0;
        m_lineAnimation->stop();
#endif
        if (qFuzzyIsNull(m_parent->scalingFactor()))
            m_parent->setCurrentValue(posToValue(event->pos().x()));
        m_parent->setMoving(true);
        m_previousPos = event->pos();
    }
}

void TimeLine::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        event->accept();

        const bool bUnscaled = qFuzzyIsNull(m_parent->scalingFactor());
        const QPointF dpos = bUnscaled ? event->pos() - m_previousPos : m_previousPos - event->pos();
        if (!isDragging() && qAbs(dpos.x()) < QApplication::startDragDistance())
            return;

        setDragging(true);
        m_previousPos = event->pos();
#ifdef TIMESLIDER_ANIMATED_DRAG
        m_length += dpos.x();
#endif

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

void TimeLine::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();

        const bool wasDragging = isDragging();
        if (wasDragging) {
            setDragging(false);

#ifdef TIMESLIDER_ANIMATED_DRAG
            if (qAbs(m_length) > 5) {
                const int dx = m_length*2/**(35.0/t.elapsed())*/;

                m_lineAnimation->setStartValue(m_parent->viewPortPos());
                m_lineAnimation->setEasingCurve(QEasingCurve::OutQuad);
                m_lineAnimation->setEndValue(m_parent->viewPortPos() + qRound64((double(m_parent->sliderRange()) / rect().width()) * dx));
                m_lineAnimation->setDuration(1000);
                m_lineAnimation->start();
            }
#endif
        } else {
            m_parent->setCurrentValue(posToValue(event->pos().x()));
        }
        m_parent->setMoving(false);

        if (!wasDragging) {
            if (event->modifiers() == Qt::ShiftModifier && (m_rangeSelectionState == NoRangeSelected || m_rangeSelectionState == RangeSelected)) {
                resetSelectionRange();
                m_rangeSelectionState = SelectingRangeBegin;
            }
            if (m_rangeSelectionState == SelectingRangeBegin || m_rangeSelectionState == SelectingRangeEnd) {
                const qint64 value = posToValue(event->pos().x());
                if (m_rangeSelectionState == SelectingRangeBegin) {
                    m_selectedRange.first = value;
                    m_rangeSelectionState = SelectingRangeEnd;
                } else {
                    m_selectedRange.second = value;
                    m_rangeSelectionState = RangeSelected;
                }
                update();
            }
        }
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
    qRegisterAnimationInterpolator<qint64>(qint64Interpolator);

    setAcceptHoverEvents(true);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred, QSizePolicy::Slider);

    m_animation = new QPropertyAnimation(this, "viewPortPos", this);

    m_slider = new MySlider(this);
    m_slider->setOrientation(Qt::Horizontal);
    m_slider->setContentsMargins(0, 4, 0, 4);
    m_slider->setMaximum(10000);
    m_slider->installEventFilter(this);

    m_slider->setStyle(new SliderProxyStyle(m_slider));

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

ToolTipItem *TimeSlider::toolTipItem() const
{
    return m_slider->toolTipItem();
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

    if (!m_isUserInput)
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
    if (!m_isUserInput) {
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

qint64 TimeSlider::fromSlider(int value) const
{
    return m_viewPortPos + value * delta();
}

int TimeSlider::toSlider(qint64 value) const
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

QPair<qint64, qint64> TimeSlider::selectionRange() const
{
    return m_timeLine->selectionRange();
}

void TimeSlider::setSelectionRange(const QPair<qint64, qint64> &range)
{
    m_timeLine->setSelectionRange(range);
}

void TimeSlider::updateSlider()
{
    if (!m_isUserInput) {
        m_isUserInput = true;
        m_slider->setValue(toSlider(m_currentValue));
        m_isUserInput = false;
    }

    if (isAtEnd()) {
        m_slider->setToolTip("Live");
    } else {
        if (m_minimumValue == 0)
            m_slider->setToolTip(formatDuration(m_currentValue / 1000));
        else
            m_slider->setToolTip(QDateTime::fromMSecsSinceEpoch(m_currentValue).toString(QLatin1String("yyyy MMM dd\nhh:mm:ss")));
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
