#include "timeslider.h"

#include <QtCore/QDateTime>
#include <QtCore/QPair>
#include <QtCore/QPropertyAnimation>

#include <QtGui/QPainterPath>
#include <QtGui/QGradient>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QMenu>
#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneMouseEvent>

#include <utils/common/util.h>

#include "ui/style/skin.h"
#include "ui/style/proxy_style.h"

#include "ui/graphics/items/standard/graphics_frame.h"

#include <qmath.h>
#include "utils/common/synctime.h"
#include "ui/style/globals.h"
#include "tool_tip_slider.h"

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
static const qreal UPDATE_PERIODS_EPS = 0.05;


typedef QHash<unsigned, QPixmap*> TextCache;

}


// -------------------------------------------------------------------------- //
// MySlider
// -------------------------------------------------------------------------- //
class MySlider : public QnToolTipSlider
{
    typedef QnToolTipSlider base_type;

public:
    MySlider(TimeSlider *parent);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    const QRectF &handleRect() const;

    bool isAtEnd() const;

    void setEndSize(int size);

    qreal getMsInPixel() const;

protected:
    void sliderChange(SliderChange change);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

    void resizeEvent(QGraphicsSceneResizeEvent *event);

private:
    void invalidateHandleRect();
    void ensureHandleRect() const;
    void drawTimePeriods(QPainter *painter, const QnTimePeriodList& timePeriods, QColor color);
    void createEndPixmap();
private:
    TimeSlider *m_parent;
    mutable QRectF m_handleRect;
    int m_endSize;
    QPixmap m_pixmap;
    QPixmap m_endPixmap;
    QnTimePeriodList m_recTimePeriodList;
    QnTimePeriodList m_motionTimePeriodList;
};

MySlider::MySlider(TimeSlider *parent)
    : base_type(parent),
      m_parent(parent),
      m_endSize(0)
{
    setAutoHideToolTip(false);
}

void MySlider::createEndPixmap()
{
    QRectF r = contentsRect();
    qreal l = r.left() + QStyle::sliderPositionFromValue(minimum(), maximum(), maximum() - m_endSize, r.width());
    r = QRectF(0, 0, r.right() - l, r.height());

    QLinearGradient linearGrad = QLinearGradient(r.topLeft(), r.topRight());
    linearGrad.setColorAt(0, QColor(0, 255, 0, 0));
    linearGrad.setColorAt(1, QColor(0, 255, 0, 128));

    m_endPixmap = QPixmap(r.size().toSize());
    m_endPixmap.fill(QColor(0,0,0,0));
    QPainter painter(&m_endPixmap);
    painter.fillRect(0,0, m_endPixmap.width(), m_endPixmap.height(), linearGrad);
}

bool MySlider::isAtEnd() const
{
    return m_endSize == 0 || value() > maximum() - m_endSize;
}

void MySlider::setEndSize(int size)
{
    m_endSize = size;
}

qreal MySlider::getMsInPixel() const
{
    qreal w = contentsRect().width() - m_handleRect.width();
    const qint64 range = m_parent->sliderRange();
    return range/w;
}

void MySlider::drawTimePeriods(QPainter *painter, const QnTimePeriodList& timePeriods, QColor color)
{
    if (timePeriods.isEmpty())
        return;

    const qint64 pos = m_parent->viewPortPos() /*+ timezoneOffset*/;

    QRectF contentsRect = this->contentsRect();
    qreal x = contentsRect.left() + m_handleRect.width() / 2;

    qreal msInPixel = getMsInPixel();

    QnTimePeriodList::const_iterator beginItr = timePeriods.findNearestPeriod(pos, false);
    QnTimePeriodList::const_iterator endItr = timePeriods.findNearestPeriod(pos+m_parent->sliderRange(), false);

    int center = m_handleRect.center().x();
    QColor light = color.lighter();
    QColor dark = color;

    //foreach(const QnTimePeriod &period, timePeriods)
    for (QnTimePeriodList::const_iterator itr = beginItr; itr <= endItr; ++itr)
    {
        const QnTimePeriod& period = *itr;
        qreal left = x + static_cast<qreal>(period.startTimeMs - pos) / msInPixel;
        left = qMax(left, contentsRect.left());

        qreal right;
        if (period.durationMs == -1) {
            right = contentsRect.right();
        } else {
            right = x + static_cast<qreal>((period.startTimeMs + period.durationMs) - pos) / msInPixel;
            right = qMin(right, contentsRect.right());
        }

        //if (left > right)
        //    continue;

        if(right < center) {
            painter->fillRect(left, contentsRect.top(), qMax(1.0, right - left), contentsRect.height(), light);
        } else if(left > center) {
            painter->fillRect(left, contentsRect.top(), qMax(1.0, right - left), contentsRect.height(), dark);
        } else {
            painter->fillRect(left, contentsRect.top(), center - left, contentsRect.height(), light);
            painter->fillRect(center, contentsRect.top(), qMax(1.0, right - center), contentsRect.height(), dark);
        }
    }
}

void MySlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //qint64 t = getUsecTimer();

    Q_UNUSED(option)
    Q_UNUSED(widget)

    QRectF r = contentsRect();


    painter->setPen(QPen(Qt::gray));
    
    QPointF lines[8] = {
        QPointF(r.topLeft()),
        QPointF(r.topRight()),
        QPointF(r.topRight()),
        QPointF(r.bottomRight()),
        QPointF(r.bottomRight()),
        QPointF(r.bottomLeft()),
        QPointF(r.bottomLeft()),
        QPointF(r.topLeft())
    };
    painter->drawLines(lines, 4);
    
    //painter->setPen(QPen(Qt::darkGray, 1));
    //painter->drawRect(r);

    r.setRight(m_handleRect.center().x());
    //painter->setPen(QPen(QColor(0, 87, 207), 2));
    //painter->drawRect(r);

    qreal msInPixel = getMsInPixel();
    const QnTimePeriodList& recPeriods = m_parent->recTimePeriodList(msInPixel);
    const QnTimePeriodList& motionPeriods = m_parent->motionTimePeriodList(msInPixel);

    QLinearGradient linearGrad(r.topLeft(), r.bottomRight());
    if (!recPeriods.isEmpty() && m_parent->viewPortPos() + m_parent->sliderRange() >= recPeriods[0].startTimeMs)
    {
        QColor color = QColor(64, 64, 64);
        linearGrad.setColorAt(0, color);
        linearGrad.setColorAt(1, color);
    }
    else {
        linearGrad.setColorAt(0, QColor(0, 43, 130));
        linearGrad.setColorAt(1, QColor(186, 239, 255));
    }

    //painter->setPen(QPen(Qt::green, 0));
    painter->setBrush(linearGrad);
    //painter->drawRect(r);
    painter->fillRect(r, linearGrad);


    // Draw time periods
    drawTimePeriods(painter, recPeriods, QColor(24, 128, 24));
    drawTimePeriods(painter, motionPeriods, QColor(128, 0, 0));

    r = contentsRect();
#if 1
    if (m_endPixmap.width() == 0)
        createEndPixmap();
    painter->drawPixmap(r.topRight() - QPointF(m_endPixmap.width(), 0), m_endPixmap);
#else    
    qreal l = r.left() + QStyle::sliderPositionFromValue(minimum(), maximum(), maximum() - m_endSize, r.width());
    r = QRectF(l, r.top(), r.right() - l, r.height());

    linearGrad = QLinearGradient(r.topLeft(), r.topRight());
    linearGrad.setColorAt(0, QColor(0, 255, 0, 0));
    linearGrad.setColorAt(1, QColor(0, 255, 0, 128));
    painter->fillRect(r, linearGrad);
#endif    

    ensureHandleRect();
    if (m_pixmap.width() == 0)
        m_pixmap = qnSkin->pixmap(QLatin1String("slider-handle.png"), m_handleRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter->drawPixmap(m_handleRect.topLeft(), m_pixmap);

    //qDebug() << "timeFinal=" << (getUsecTimer() - t)/1000.0;
}

void MySlider::sliderChange(SliderChange change)
{
    base_type::sliderChange(change);

    invalidateHandleRect();
}

QVariant MySlider::itemChange(GraphicsItemChange change, const QVariant &value)
{
    return base_type::itemChange(change, value);
}

void MySlider::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    base_type::resizeEvent(event);

    invalidateHandleRect();
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
        setFlag(QGraphicsItem::ItemIsFocusable);

        setFocusPolicy(Qt::ClickFocus);

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
        foreach(QPixmap* image, m_textCache)
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
    QPixmap* createTextPixmap(const QFont& font, const QString& text, bool doRotate);
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

QPixmap* TimeLine::createTextPixmap(const QFont& font, const QString& text, bool doRotate)
{
    // Draw text line to image cache
    QFontMetrics m(font);
    QSize textSize = m.size(Qt::TextSingleLine, text);
    QPixmap* textPixmap = new QPixmap(textSize.width(), textSize.height());
    textPixmap->fill(QColor(0,0,0,0));
    QPainter p(textPixmap);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.setPen(m_parent->palette().color(QPalette::Text));
    p.setFont(font);
    p.drawText(0,  m.ascent(), text);
    p.end();

    if (doRotate) {
        QMatrix rm;
        rm.rotate(90);
        *textPixmap = textPixmap->transformed(rm);
    }
    return textPixmap;
}

void TimeLine::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

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
    qint64 outsideCnt = -xpos / (intervals[level].interval * pixelPerTime);
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
    QVector<QVector<QPointF> > linesList;
    linesList.resize(maxLevel-level+1);
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
        linesList[arrayIndex] << QPointF(xpos, 0);
        linesList[arrayIndex] << QPointF(xpos, (r.height() - m_maxHeight - 3) * lineLen);
        

        if (m_fontWidths[arrayIndex] > 0)
        {
            const int labelNumber = (curTime/interval.interval)%interval.count;
            int fontSize = m_fonts[arrayIndex].pointSizeF()*2;
            unsigned hash;
            if (curLevel < FIRST_DATE_INDEX)
                hash = (128 + (fontSize << 24)) + (curLevel << 16) + interval.value*labelNumber;
            else
                hash = (fontSize << 24) + curTime/1000/900; // curTime to 15 min granularity (we must keep integer number for months and years dates)

            QPixmap* textPixmap = m_textCache.value(hash);
            if (textPixmap == 0)
            {
                QString text;
                if (curLevel < FIRST_DATE_INDEX)
                    text = QString::number(interval.value*labelNumber) + QLatin1String(interval.name);
                else
                    text = QDateTime::fromMSecsSinceEpoch(curTime).toString(interval.name);

                textPixmap= createTextPixmap(m_fonts[arrayIndex], text, curLevel == 0);
                m_textCache.insert(hash, textPixmap);
            }

            QPointF imagePos(xpos - textPixmap->width()/2, (r.height() - m_maxHeight) * lineLen + (curLevel == 0 ? 2 : 0) );
            if (imagePos.x() < 0 && pos == 0)
                imagePos.setX(0);
            qreal o = painter->opacity();
            painter->setOpacity(o * opacity[arrayIndex]);
            painter->drawPixmap(imagePos, *textPixmap);
            painter->setOpacity(o);
        }

        xpos += intervals[level].interval * pixelPerTime;
    }

    for (int arrayIndex = 0; arrayIndex < linesList.size(); ++arrayIndex)
    {
        color.setAlphaF(opacity[arrayIndex]);
        painter->setPen(color);
        painter->drawLines(linesList[arrayIndex]);
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
}

qint64 TimeLine::posToValue(qreal pos) const
{
    qreal d = m_parent->m_slider->handleRect().width();

    return m_parent->viewPortPos() + qRound64((double(m_parent->sliderRange()) / (m_parent->rect().width() - d)) * (pos - d / 2));
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
            m_parent->setCurrentValue(posToValue(event->pos().x()));
            //m_parent->setCurrentValue(qBound(m_parent->viewPortPos(), m_parent->currentValue() + dtime, m_parent->viewPortPos() + m_parent->sliderRange()));
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
    m_endSize(0.0),
    m_aggregatedMsInPixel(-1),
    m_isLiveMode(false)
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
    m_slider->setFocusProxy(this);

    m_timeLine = new TimeLine(this);
    m_timeLine->setLineWidth(0);
    m_timeLine->setFrameShape(GraphicsFrame::Box);
    m_timeLine->setFrameShadow(GraphicsFrame::Sunken);
    m_timeLine->installEventFilter(this);
    m_timeLine->setFocusProxy(this);

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

void TimeSlider::setLiveMode(bool value)
{
    m_isLiveMode = value;
    updateSlider();
}

QnToolTipItem *TimeSlider::toolTipItem() const
{
    return m_slider->toolTipItem();
}

void TimeSlider::setToolTipItem(QnToolTipItem *toolTip)
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
    return m_maximumValue == DATETIME_NOW ? qnSyncTime->currentMSecsSinceEpoch() : m_maximumValue;
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

void TimeSlider::setCurrentValue(qint64 value, bool forceUpdate)
{
/*    if (value == DATETIME_NOW)
        return; // ###*/

    value = qBound(m_minimumValue, value, maximumValue());
    if (m_currentValue == value && !forceUpdate)
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

    if (m_isLiveMode) {
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

const QnTimePeriodList& TimeSlider::fullRecTimePeriodList()
{
    return m_recTimePeriodList;
}

const QnTimePeriodList& TimeSlider::recTimePeriodList(int msInPixel)
{
    if (m_aggregatedMsInPixel != msInPixel)
    {
        m_agregatedRecTimePeriodList = QnTimePeriod::aggregateTimePeriods(m_recTimePeriodList, (int) msInPixel);
        m_agregatedMotionTimePeriodList = QnTimePeriod::aggregateTimePeriods(m_motionTimePeriodList, (int) msInPixel);
        m_aggregatedMsInPixel = msInPixel;
    }
    return m_agregatedRecTimePeriodList;
}

const QnTimePeriodList& TimeSlider::fullMotionTimePeriodList()
{
    return m_motionTimePeriodList;
}

const QnTimePeriodList& TimeSlider::motionTimePeriodList(int msInPixel)
{
    if (m_aggregatedMsInPixel != msInPixel)
    {
        m_agregatedRecTimePeriodList = QnTimePeriod::aggregateTimePeriods(m_recTimePeriodList, (int) msInPixel);
        m_agregatedMotionTimePeriodList = QnTimePeriod::aggregateTimePeriods(m_motionTimePeriodList, (int) msInPixel);
        m_aggregatedMsInPixel = msInPixel;
    }
    return m_agregatedMotionTimePeriodList;
}

void TimeSlider::setRecTimePeriodList(const QnTimePeriodList &timePeriodList) 
{ 
    m_recTimePeriodList = timePeriodList;
    m_aggregatedMsInPixel = -1;
}

void TimeSlider::setMotionTimePeriodList(const QnTimePeriodList &timePeriodList) 
{ 
    m_motionTimePeriodList = timePeriodList; 
    m_aggregatedMsInPixel = -1;
}

qreal TimeSlider::getMsInPixel() const 
{
    return m_slider->getMsInPixel();
}
