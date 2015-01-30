#include "time_slider.h"

#include <cassert>
#include <cmath>
#include <limits>

#include <boost/array.hpp>

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/qmath.h>

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneWheelEvent>

#include <camera/thumbnails_loader.h>

#include <core/resource/camera_bookmark.h>

#include <recording/time_period_list.h>

#include <ui/common/geometry.h>
#include <ui/style/noptix_style.h>
#include <ui/style/globals.h>
#include <ui/graphics/items/controls/time_slider_pixmap_cache.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/processors/kinetic_cutting_processor.h>
#include <ui/processors/drag_processor.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/math/math.h>
#include <utils/math/color_transformations.h>

namespace {

    QTime msecsToTime(qint64 msecs) {
        return QTime(0, 0, 0, 0).addMSecs(msecs); 
    }

    inline qreal adjust(qreal value, qreal target, qreal delta) {
        if(value < target) {
            if(target - value < delta) {
                return target;
            } else {
                return value + delta;
            }
        } else {
            if(value - target < delta) {
                return target;
            } else {
                return value - delta;
            }
        }
    }

    qreal speed(qreal progress, qreal center, qreal starting, qreal relative) {
        /* Speed of tickmark height animation depends on tickmark height.
         * 
         * The goal is for animation of different tickmark groups to start and
         * end at the same moment, even if the height changes in these groups are 
         * different. */
        if(progress > center) {
            return progress * relative;
        } else {
            return (relative - starting / center) * progress + starting;
        }
    }

    /* Note that most numbers below are given relative to time slider size. */



    /* Tickmark bar. */

    /** Minimal distance between tickmarks from the same group for this group to be used as toplevel one. */
    const qreal topLevelTickmarkStep = 0.75;

    /** Minimal distance between tickmarks from the same group for this group to be visible. 
     * Note that because of the fact that tickmarks do not disappear instantly, in some cases
     * step may become smaller that this value. */
    const qreal minTickmarkLineStepPixels = 5.0;

    /** Critical distance between tickmarks from the same group. 
     * Tickmarks that are closer to each other will never be displayed. */
    const qreal criticalTickmarkLineStepPixels = 2.0;

    /** Estimated maximal size of tickmark text label. */
    const qreal maxTickmarkTextSizePixels = 80.0;

    /** Minimal distance between tickmarks from the same group for text labels to be visible for this group. */
    const qreal minTickmarkTextStepPixels = 40.0;

    /** Critical distance between tickmarks from the same group.
     * Text labels will never be displayed for tickmarks that are closer to each other. */
    const qreal criticalTickmarkTextStepPixels = 25.0;
    
    /** Minimal opacity for a tickmark that is visible. */
    const qreal minTickmarkLineOpacity = 0.05;

    /** Minimal opacity for a tickmark text label that is visible. */
    const qreal minTickmarkTextOpacity = 0.5;
    
    /** Ratio between the sizes of consequent tickmark groups. */
    const qreal tickmarkStepScale = 2.0 / 3.0;

    /** Minimal height of a tickmark text. */
    const int minTickmarkTextHeightPixels = 9;
    
    /** Maximal tickmark height, relative to the height of the tickmark bar. */
    const qreal maxTickmarkHeight = 1.0;

    /** Minimal tickmark height, relative to the height of the tickmark bar. */
    const qreal minTickmarkHeight = 0.1;

    /** Height of a tickmark text, relative to tickmark height. */
    const qreal tickmarkTextHeight = 1.0 / 3.0;

    /** Height of a tickmark line, relative to tickmark height. */
    const qreal tickmarkLineHeight = 2.0 / 3.0;

    /* Tickmark height animation parameters. */
    const qreal tickmarkHeightRelativeAnimationSpeed = 1.0;
    const qreal tickmarkHeightCentralAnimationValue = minTickmarkHeight;
    const qreal tickmarkHeightStartingAnimationSpeed = 0.5;

    /* Tickmark opacity animation parameters. */
    const qreal tickmarkOpacityRelativeAnimationSpeed = 1.0;
    const qreal tickmarkOpacityCentralAnimationValue = 0.5;
    const qreal tickmarkOpacityStartingAnimationSpeed = 1.0;



    /* Dates bar. */

    const qreal dateTextTopMargin = 0.0;
    const qreal dateTextBottomMargin = 0.1;

    const qreal minDateSpanFraction = 0.15;
    const qreal minDateSpanPixels = 160;


    
    /* Lines bar. */

    const qreal lineCommentTopMargin = -0.20;
    const qreal lineCommentBottomMargin = 0.05;

    /** Minimal color coefficient for the most noticeable chunk color in range */
    const qreal lineBarMinNoticeableFraction = 0.5;


    /* Thumbnails bar. */

    const qreal thumbnailHeightForDrawing = 10.0;



    /* Global */

    /** Minimal relative change of msecs-per-pixel value of a time slider for animation parameters to be recalculated.
     * This value was introduced so that the parameters are not recalculated constantly when changes are small. */
    const qreal msecsPerPixelChangeThreshold = 1.0e-4;
    
    /** Lower limit on time slider scale. */
    const qreal minMSecsPerPixel = 2.0;

    const QRectF dateBarPosition = QRectF(0.0, 0.0, 1.0, 0.2);

    const QRectF tickmarkBarPosition = QRectF(0.0, 0.2, 1.0, 0.5);

    const QRectF lineBarPosition = QRectF(0.0, 0.7, 1.0, 0.3);

    /** Maximal number of lines in a time slider. */
    const int maxLines = 16;

    const qreal degreesFor2x = 180.0;

    const qreal zoomSideSnapDistance = 0.075;

    const qreal hoverEffectDistance = 5.0;

    const int startDragDistance = 5;



    bool checkLine(int line) {
        if(line < 0 || line >= maxLines) {
            qnWarning("Invalid line number '%1'.", line);
            return false;
        } else {
            return true;
        }
    }

    bool checkLinePeriod(int line, Qn::TimePeriodContent type) {
        if(!checkLine(line))
            return false;

        if(type < 0 || type >= Qn::TimePeriodContentCount) {
            qnWarning("Invalid time period type '%1'.", static_cast<int>(type));
            return false;
        } else {
            return true;
        }
    }

    QRectF positionRect(const QRectF &sourceRect, const QRectF &position) {
        QRectF result = QnGeometry::cwiseMul(position, sourceRect.size());
        result.moveTopLeft(result.topLeft() + sourceRect.topLeft());
        return result;
    }

    void drawData(QPainter *painter, const QRectF &targetRect, const QPixmap &data, const QRectF &sourceRect) {
        painter->drawPixmap(targetRect, data, sourceRect);
    }

    void drawData(QPainter *painter, const QRectF &targetRect, const QImage &data, const QRectF &sourceRect) {
        painter->drawImage(targetRect, data, sourceRect);
    }

    template<class Data>
    void drawCroppedData(QPainter *painter, const QRectF &target, const QRectF &cropTarget, const Data &data, const QRectF &source, QRectF *drawnTarget = NULL) {
        MarginsF targetMargins(
            qMax(0.0, cropTarget.left() - target.left()),
            qMax(0.0, cropTarget.top() - target.top()),
            qMax(0.0, target.right() - cropTarget.right()),
            qMax(0.0, target.bottom() - cropTarget.bottom())
        );

        QRectF erodedTarget;

        if(targetMargins.isNull()) {
            erodedTarget = target;
        } else {
            erodedTarget = QnGeometry::eroded(target, targetMargins);
        } 

        if(!erodedTarget.isValid()) {
            erodedTarget = QRectF();
        } else {
            MarginsF sourceMargins = QnGeometry::cwiseMul(QnGeometry::cwiseDiv(targetMargins, target.size()), source.size());
            QRectF erodedSource = QnGeometry::eroded(source, sourceMargins);

            drawData(painter, erodedTarget, data, erodedSource);
        }

        if(drawnTarget)
            *drawnTarget = erodedTarget;
    }

    void drawCroppedPixmap(QPainter *painter, const QRectF &target, const QRectF &cropTarget, const QPixmap &pixmap, const QRectF &source, QRectF *drawnTarget = NULL) {
        drawCroppedData(painter, target, cropTarget, pixmap, source, drawnTarget);
    }

    void drawCroppedImage(QPainter *painter, const QRectF &target, const QRectF &cropTarget, const QImage &image, const QRectF &source, QRectF *drawnTarget = NULL) {
        drawCroppedData(painter, target, cropTarget, image, source, drawnTarget);
    }

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnTimeSliderChunkPainter
// -------------------------------------------------------------------------- //
// TODO: #Elric
// An even better solution that will remove all blinking 
// (we still have it with recorded chunks trapped between two motion chunks)
// would be to draw it pixel-by-pixel, counting the motion/recording percentage
// in each pixel. This approach can be made to work just as fast as the current one.
class QnTimeSliderChunkPainter {
public:
    QnTimeSliderChunkPainter(QnTimeSlider *slider, QPainter *painter): 
        m_slider(slider), 
        m_painter(painter) 
    {
        assert(m_painter && m_slider);

        const QnTimeSliderColors &colors = slider->colors();

        m_pastColor[Qn::RecordingContent]           = colors.pastRecording;
        m_pastColor[Qn::MotionContent]              = colors.pastMotion;
        m_pastColor[Qn::BookmarksContent]           = colors.pastBookmark;
        m_pastColor[Qn::TimePeriodContentCount]     = colors.pastBackground;

        m_futureColor[Qn::RecordingContent]         = colors.futureRecording;
        m_futureColor[Qn::MotionContent]            = colors.futureMotion;
        m_futureColor[Qn::BookmarksContent]         = colors.futureBookmark;
        m_futureColor[Qn::TimePeriodContentCount]   = colors.futureBackground;

        m_position = m_centralPosition = m_minChunkLength = 0;
    }

    void start(qint64 startPos, qint64 centralPos, qint64 minChunkLength, const QRectF &rect) {
        m_position = startPos;
        m_centralPosition = centralPos;
        m_centralCoordinate = m_slider->quickPositionFromValue(m_centralPosition);
        m_minChunkLength = minChunkLength;
        m_rect = rect;

        qFill(m_weights, 0);
    }

    void paintChunk(qint64 length, Qn::TimePeriodContent content) {
        assert(length >= 0);

        if(m_pendingLength > 0 && m_pendingLength + length > m_minChunkLength) {
            qint64 delta = m_minChunkLength - m_pendingLength;
            length -= delta;

            storeChunk(delta, content);
            flushChunk();
        }

        storeChunk(length, content);
        if(m_pendingLength >= m_minChunkLength)
            flushChunk();
    }

    void stop() {
        if(m_pendingLength > 0)
            flushChunk();
    }

private:
    void storeChunk(qint64 length, Qn::TimePeriodContent content) {
        if(m_pendingLength == 0)
            m_pendingPosition = m_position;

        m_weights[content] += length;
        m_pendingLength += length;
        m_position += length;
    }

    void flushChunk() {
        qint64 leftPosition = m_pendingPosition;
        qint64 rightPosition = m_pendingPosition + m_pendingLength;

        qreal l = m_slider->quickPositionFromValue(leftPosition);
        qreal r = m_slider->quickPositionFromValue(rightPosition);

        if(rightPosition <= m_centralPosition) {
            m_painter->fillRect(QRectF(l, m_rect.top(), r - l, m_rect.height()), currentColor(m_pastColor));
        } else if(leftPosition >= m_centralPosition) {
            m_painter->fillRect(QRectF(l, m_rect.top(), r - l, m_rect.height()), currentColor(m_futureColor));
        } else {
            m_painter->fillRect(QRectF(l, m_rect.top(), m_centralCoordinate - l, m_rect.height()), currentColor(m_pastColor));
            m_painter->fillRect(QRectF(m_centralCoordinate, m_rect.top(), r - m_centralCoordinate, m_rect.height()), currentColor(m_futureColor));
        }

        m_pendingPosition = rightPosition;
        m_pendingLength = 0;

        qFill(m_weights, 0);
    }

    QColor currentColor(const boost::array<QColor, Qn::TimePeriodContentCount + 1> &colors) const {
        qreal rc = m_weights[Qn::RecordingContent];
        qreal mc = m_weights[Qn::MotionContent];
        qreal bc = m_weights[Qn::BookmarksContent];
        qreal nc = m_weights[Qn::TimePeriodContentCount];
        qreal sum = m_pendingLength;

        if (!qFuzzyIsNull(bc) && !(qFuzzyIsNull(mc))) {
            qreal localSum = mc + bc;
            return linearCombine(mc / localSum, colors[Qn::MotionContent], bc/localSum, colors[Qn::BookmarksContent]);
        }

        if (!qFuzzyIsNull(bc)) {
            /* Make sure bookmark is noticeable even if there isn't much of it. 
             * Note that these adjustments don't change sum. */
            rc = rc * (1.0 - lineBarMinNoticeableFraction);
            bc = sum * lineBarMinNoticeableFraction + bc * (1.0 - lineBarMinNoticeableFraction);
            nc = nc * (1.0 - lineBarMinNoticeableFraction);
        } else if (!qFuzzyIsNull(mc)) {
            /* Make sure motion is noticeable even if there isn't much of it. 
             * Note that these adjustments don't change sum. */
            rc = rc * (1.0 - lineBarMinNoticeableFraction);
            mc = sum * lineBarMinNoticeableFraction + mc * (1.0 - lineBarMinNoticeableFraction);
            nc = nc * (1.0 - lineBarMinNoticeableFraction);
        } else if (!qFuzzyIsNull(rc) && rc < sum * lineBarMinNoticeableFraction) {
            /* Make sure recording content is noticeable even if there isn't much of it. 
             * Note that these adjustments don't change sum because mc == 0. */
            rc = sum * lineBarMinNoticeableFraction;// + rc * (1.0 - lineBarMinNoticeableFraction);
            nc = sum * (1.0 - lineBarMinNoticeableFraction);
        }

        return 
            linearCombine(
                1.0,
                linearCombine(rc / sum, colors[Qn::RecordingContent], mc / sum, colors[Qn::MotionContent]),
                1.0, 
                linearCombine(bc / sum, colors[Qn::BookmarksContent], nc / sum, colors[Qn::TimePeriodContentCount])
            );
    }

private:
    QnTimeSlider *m_slider;
    QPainter *m_painter;
    
    qint64 m_centralPosition;
    qreal m_centralCoordinate;
    qint64 m_minChunkLength;
    QRectF m_rect;
    
    qint64 m_position;
    qint64 m_pendingLength;
    qint64 m_pendingPosition;

    boost::array<qint64, Qn::TimePeriodContentCount + 1> m_weights;
    boost::array<QColor, Qn::TimePeriodContentCount + 1> m_pastColor;
    boost::array<QColor, Qn::TimePeriodContentCount + 1> m_futureColor;
};


// -------------------------------------------------------------------------- //
// QnTimeSliderStepStorage
// -------------------------------------------------------------------------- //
// TODO: #Elric move all step construction hell here.
class QnTimeSliderStepStorage {
public:
    QnTimeSliderStepStorage() {
        QnTimeSlider::createSteps(&m_absolute, &m_relative);
    }
    
    const QVector<QnTimeStep> &absolute() const {
        return m_absolute;
    }

    const QVector<QnTimeStep> &relative() const {
        return m_relative;
    }

private:
    QVector<QnTimeStep> m_absolute, m_relative;
};

Q_GLOBAL_STATIC(QnTimeSliderStepStorage, timeSteps);



// -------------------------------------------------------------------------- //
// QnTimeSlider
// -------------------------------------------------------------------------- //
QnTimeSlider::QnTimeSlider(QGraphicsItem *parent):
    base_type(parent),
    m_windowStart(0),
    m_windowEnd(0),
    m_minimalWindow(0),
    m_selectionStart(0),
    m_selectionEnd(0),
    m_selectionValid(false),
    m_oldMinimum(0),
    m_oldMaximum(0),
    m_options(0),
    m_zoomAnchor(0),
    m_animating(false),
    m_kineticsHurried(false),
    m_dragMarker(NoMarker),
    m_dragIsClick(false),
    m_selecting(false),
    m_lineCount(0),
    m_totalLineStretch(0.0),
    m_msecsPerPixel(1.0),
    m_animationUpdateMSecsPerPixel(1.0),
    m_thumbnailsAspectRatio(-1.0),
    m_lastThumbnailsUpdateTime(0),
    m_lastHoverThumbnail(-1),
    m_thumbnailsVisible(false),
    m_rulerHeight(0.0),
    m_prefferedHeight(0.0),
    m_lastMinuteAnimationDelta(0),
    m_pixmapCache(new QnTimeSliderPixmapCache(this)),
    m_localOffset(0)
{
    /* Prepare thumbnail update timer. */
    m_thumbnailsUpdateTimer = new QTimer(this);
    connect(m_thumbnailsUpdateTimer, SIGNAL(timeout()), this, SLOT(updateThumbnailsStepSizeTimer()));

    /* Set default vector sizes. */
    m_lineData.resize(maxLines);
    m_lastMinuteIndicatorVisible.fill(true, maxLines);

    generateProgressPatterns();

    /* Prepare kinetic zoom processor. */
    KineticCuttingProcessor *kineticProcessor = new KineticCuttingProcessor(QMetaType::QReal, this);
    kineticProcessor->setHandler(this);
    registerAnimation(kineticProcessor);
    updateKineticProcessor();

    /* Prepare zoom processor. */
    DragProcessor *dragProcessor = new DragProcessor(this);
    dragProcessor->setHandler(this);
    dragProcessor->setFlags(DragProcessor::DontCompress);
    dragProcessor->setStartDragDistance(startDragDistance);
    dragProcessor->setStartDragTime(-1); /* No drag on timeout. */

    /* Prepare animation timer listener. */
    startListening();
    registerAnimation(this);

    /* Set default property values. */
    setAcceptHoverEvents(true);
    setProperty(Qn::SliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    setOptions(StickToMinimum | StickToMaximum | PreserveWindowSize | UpdateToolTip | SelectionEditable | AdjustWindowToPosition | SnapZoomToSides | UnzoomOnDoubleClick);

    setToolTipFormat(lit("hh:mm:ss"));
    setRulerHeight(60.0);

    /* Run handlers. */
    updateSteps();
    updateMinimalWindow();
    updatePixmapCache();
    sliderChange(SliderRangeChange);
}

QnTimeSlider::~QnTimeSlider() {
    return;
}

void QnTimeSlider::createSteps(QVector<QnTimeStep> *absoluteSteps, QVector<QnTimeStep> *relativeSteps) {
    //: Translate this into 'none' or 'forced' if you want to switch off automatic detection of
    //: AM/PM usage based on user's system locale. Do not translate this string unless you know what you're doing.
    QString ampmUsage = tr("auto"); // TODO: #Elric #tr AM_PM_USAGE as 2nd param
    
    bool ampm;
    if(ampmUsage == lit("forced")) {
        ampm = true;
    } else if(ampmUsage == lit("none")) {
        ampm = false;
    } else {
        ampm = QLocale::system().timeFormat().contains(lit("ap"), Qt::CaseInsensitive);
    }

    //: Suffix for displaying milliseconds on timeline. Do not translate this string unless you know what you're doing.
    QString msSuffix = tr("ms");

    //: Suffix for displaying seconds on timeline. Do not translate this string unless you know what you're doing.
    QString sSuffix = tr("s");

    //: Suffix for displaying minutes on timeline. Do not translate this string unless you know what you're doing.
    QString mSuffix = tr("m");

    //: Suffix for displaying hours on timeline. Do not translate this string unless you know what you're doing.
    QString hSuffix = tr("h");

    //: Suffix for displaying days on timeline. Do not translate this string unless you know what you're doing.
    QString dSuffix = tr("d");

    //: Suffix for displaying months on timeline. Do not translate this string unless you know what you're doing.
    QString moSuffix = tr("M");

    //: Suffix for displaying years on timeline. Do not translate this string unless you know what you're doing.
    QString ySuffix = tr("y");


    //: Format for displaying days on timeline. Do not translate this string unless you know what you're doing.
    QString dFormat = tr("dd MMMM");

    //: Format for displaying months on timeline. Do not translate this string unless you know what you're doing.
    QString moFormat = tr("MMMM");

    //: Format for displaying years on timeline. Do not translate this string unless you know what you're doing.
    QString yFormat = tr("yyyy"); // TODO: #Elric #TR duplicate with dateYearsFormat


    //: Format for displaying minute caption in timeline's header, without am/pm indicator. Do not translate this string unless you know what you're doing.
    QString dateMinsFormat = tr("dd MMMM yyyy hh:mm", "MINUTES");

    //: Format for displaying minute caption in timeline's header, with am/pm indicator. Do not translate this string unless you know what you're doing.
    QString dateMinsApFormat = tr("dd MMMM yyyy hh:mm ap");

    //: Format for displaying hour caption in timeline's header, without am/pm indicator. Do not translate this string unless you know what you're doing.
    QString dateHoursFormat = tr("dd MMMM yyyy hh:mm", "HOURS");

    //: Format for displaying hour caption in timeline's header, with am/pm indicator. Do not translate this string unless you know what you're doing.
    QString dateHoursApFormat = tr("dd MMMM yyyy h ap");

    //: Format for displaying day caption in timeline's header. Do not translate this string unless you know what you're doing.
    QString dateDaysFormat = tr("dd MMMM yyyy");

    //: Format for displaying month caption in timeline's header. Do not translate this string unless you know what you're doing.
    QString dateMonthsFormat = tr("MMMM yyyy");

    //: Format for displaying year caption in timeline's header. Do not translate this string unless you know what you're doing.
    QString dateYearsFormat = tr("yyyy");

    *absoluteSteps <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                10,     1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                50,     1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                100,    1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                500,    1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             1,      60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             5,      60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             10,     60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             30,     60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        1,      60,     mSuffix,        ampm ? dateMinsApFormat : dateMinsFormat, false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        5,      60,     mSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        10,     60,     mSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        30,     60,     mSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   1,      24,     hSuffix,        ampm ? dateHoursApFormat : dateHoursFormat, false) <<
        QnTimeStep(QnTimeStep::Hours,           1000ll * 60 * 60,                   3,      24,     hSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Hours,           1000ll * 60 * 60,                   12,     24,     hSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Days,            1000ll * 60 * 60 * 24,              1,      31,     dFormat,        dateDaysFormat,     false) <<
        QnTimeStep(QnTimeStep::Months,          1000ll * 60 * 60 * 24 * 31,         1,      12,     moFormat,       dateMonthsFormat,   false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        1,      50000,  yFormat,        dateYearsFormat,    false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        5,      50000,  yFormat,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        10,     50000,  yFormat,        dateYearsFormat,    false);
    enumerateSteps(*absoluteSteps);

    *relativeSteps <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                10,     1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                50,     1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                100,    1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                500,    1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             1,      60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             5,      60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             10,     60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             30,     60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        1,      60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        5,      60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        10,     60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        30,     60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   1,      24,     hSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   3,      24,     hSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   12,     24,     hSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24,              1,      31,     dSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30,         1,      12,     moSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    1,      50000,  ySuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    5,      50000,  ySuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    10,     50000,  ySuffix,        QString(),          true);
    enumerateSteps(*relativeSteps);
}

void QnTimeSlider::enumerateSteps(QVector<QnTimeStep> &steps) {
    for(int i = 0; i < steps.size(); i++)
        steps[i].index = i;
}

int QnTimeSlider::lineCount() const {
    return m_lineCount;
}

void QnTimeSlider::setLineCount(int lineCount) {
    if(m_lineCount == lineCount)
        return;

    if(lineCount < 0 || lineCount > maxLines) {
        qnWarning("Invalid line count '%1'.", lineCount);
        return;
    }

    m_lineCount = lineCount;
}

void QnTimeSlider::setLineVisible(int line, bool visible) {
    if(!checkLine(line))
        return;

    if(m_lineData[line].visible == visible)
        return;

    m_lineData[line].visible = visible;

    updateTotalLineStretch();
}

bool QnTimeSlider::isLineVisible(int line) const {
    if(!checkLine(line))
        return false;

    return m_lineData[line].visible;
}

void QnTimeSlider::setLineStretch(int line, qreal stretch) {
    if(!checkLine(line))
        return;
    
    if(qFuzzyCompare(m_lineData[line].stretch, stretch))
        return;

    m_lineData[line].stretch = stretch;

    updateTotalLineStretch();
}

qreal QnTimeSlider::lineStretch(int line) const {
    if(!checkLine(line))
        return 0.0;

    return m_lineData[line].stretch;
}

void QnTimeSlider::setLineComment(int line, const QString &comment) {
    if(!checkLine(line))
        return;

    if(m_lineData[line].comment == comment)
        return;

    m_lineData[line].comment = comment;

    updateLineCommentPixmap(line);
}

QString QnTimeSlider::lineComment(int line) {
    if(!checkLine(line))
        return QString();

    return m_lineData[line].comment;
}

QnTimePeriodList QnTimeSlider::timePeriods(int line, Qn::TimePeriodContent type) const {
    if(!checkLinePeriod(line, type))
        return QnTimePeriodList();
    
    return m_lineData[line].timeStorage.periods(type);
}

void QnTimeSlider::setTimePeriods(int line, Qn::TimePeriodContent type, const QnTimePeriodList &timePeriods) {
    if(!checkLinePeriod(line, type))
        return;

    m_lineData[line].timeStorage.setPeriods(type, timePeriods);
}

QnCameraBookmarkList QnTimeSlider::bookmarks() const {
    return m_bookmarks;
}

void QnTimeSlider::setBookmarks(const QnCameraBookmarkList &bookmarks) {
    m_bookmarks = bookmarks;
}

QnTimeSlider::Options QnTimeSlider::options() const {
    return m_options;
}

void QnTimeSlider::setOptions(Options options) {
    if(m_options == options)
        return;

    Options difference = m_options ^ options;
    m_options = options;

    if(difference & UseUTC) {
        updateSteps();
        updateTickmarkTextSteps();
    }
}

void QnTimeSlider::setOption(Option option, bool value) {
    if(value) {
        setOptions(m_options | option);
    } else {
        setOptions(m_options & ~option);
    }
}

qint64 QnTimeSlider::windowStart() const {
    return m_windowStart;
}

void QnTimeSlider::setWindowStart(qint64 windowStart) {
    setWindow(windowStart, qMax(windowStart + m_minimalWindow, m_windowEnd));
}

qint64 QnTimeSlider::windowEnd() const {
    return m_windowEnd;
}

void QnTimeSlider::setWindowEnd(qint64 windowEnd) {
    setWindow(qMin(m_windowStart, windowEnd - m_minimalWindow), windowEnd);
}

void QnTimeSlider::setWindow(qint64 start, qint64 end, bool animate) {
    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    if(end - start < m_minimalWindow) {
        qint64 delta = (m_minimalWindow - (end - start) + 1) / 2;
        start -= delta;
        end += delta;

        if(start < minimum()) {
            end += minimum() - start;
            start = minimum();
        }
        if(end > maximum()) {
            start += maximum() - end;
            end = maximum();
        }

        start = qBound(minimum(), start, maximum());
        end = qBound(minimum(), end, maximum());
    }

    if (m_windowStart != start || m_windowEnd != end) {
        if(animate) {
            kineticProcessor()->reset(); /* Stop kinetic zoom. */
            m_animating = true;
            setAnimationStart(start);
            setAnimationEnd(end);
        } else {
            m_windowStart = start;
            m_windowEnd = end;

            sliderChange(SliderMappingChange);
            emit windowChanged(m_windowStart, m_windowEnd);

            updateToolTipVisibility();
            updateMSecsPerPixel();
            updateThumbnailsPeriod();
        }
    }
}

bool QnTimeSlider::windowContains(qint64 position) {
    return m_windowStart <= position && position <= m_windowEnd;
}

void QnTimeSlider::ensureWindowContains(qint64 position) {
    qint64 d = 0;

    if(position < m_windowStart) {
        d = position - m_windowStart;
    } else if(m_windowEnd < position) {
        d = position - m_windowEnd;
    } else {
        return;
    }

    setWindow(m_windowStart + d, m_windowEnd + d);
}

void QnTimeSlider::setSliderPosition(qint64 position, bool keepInWindow) {
    if(!keepInWindow) {
        return setSliderPosition(position);
    } else {
        bool inWindow = windowContains(sliderPosition());
        setSliderPosition(position);
        if(inWindow)
            ensureWindowContains(sliderPosition());
    }
}

void QnTimeSlider::setValue(qint64 value, bool keepInWindow) {
    if(!keepInWindow) {
        return setValue(value);
    } else {
        bool inWindow = windowContains(this->value());
        setValue(value);
        if(inWindow)
            ensureWindowContains(this->value());
    }
}

qint64 QnTimeSlider::selectionStart() const {
    return m_selectionStart;
}

void QnTimeSlider::setSelectionStart(qint64 selectionStart) {
    setSelection(selectionStart, qMax(selectionStart, m_selectionEnd));
}

qint64 QnTimeSlider::selectionEnd() const {
    return m_selectionEnd;
}

void QnTimeSlider::setSelectionEnd(qint64 selectionEnd) {
    setSelection(qMin(m_selectionStart, selectionEnd), selectionEnd);
}

void QnTimeSlider::setSelection(qint64 start, qint64 end) {
    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    if (m_selectionStart != start || m_selectionEnd != end) {
        m_selectionStart = start;
        m_selectionEnd = end;

        emit selectionChanged(m_selectionStart, m_selectionEnd);
    }
}

bool QnTimeSlider::isSelectionValid() const {
    return m_selectionValid;
}

void QnTimeSlider::setSelectionValid(bool valid) {
    m_selectionValid = valid;
}

const QString &QnTimeSlider::toolTipFormat() const {
    return m_toolTipFormat;
}

void QnTimeSlider::setToolTipFormat(const QString &format) {
    if(m_toolTipFormat == format)
        return;

    m_toolTipFormat = format;

    updateToolTipText();
}

QnThumbnailsLoader *QnTimeSlider::thumbnailsLoader() const {
    return m_thumbnailsLoader.data();
}

void QnTimeSlider::setThumbnailsLoader(QnThumbnailsLoader *loader, qreal aspectRatio) {
    if(m_thumbnailsLoader.data() == loader)
        return;

    clearThumbnails();
    m_oldThumbnailData.clear();
    m_thumbnailsUpdateTimer->stop();

    if(m_thumbnailsLoader)
        disconnect(m_thumbnailsLoader.data(), NULL, this, NULL);

    m_thumbnailsLoader = loader;
    m_thumbnailsAspectRatio = aspectRatio;

    if(m_thumbnailsLoader) {
        //connect(m_thumbnailsLoader.data(), SIGNAL(sourceSizeChanged()), this, SLOT(updateThumbnailsStepSizeForced())); // TODO: #Elric
        connect(m_thumbnailsLoader.data(), SIGNAL(thumbnailsInvalidated()), this, SLOT(clearThumbnails()));
        connect(m_thumbnailsLoader.data(), SIGNAL(thumbnailLoaded(const QnThumbnail &)), this, SLOT(addThumbnail(const QnThumbnail &)));
    }

    updateThumbnailsPeriod();
    updateThumbnailsStepSize(true); // TODO: #Elric

    if(m_thumbnailsLoader)
        foreach(const QnThumbnail &thumbnail, loader->thumbnails())
            addThumbnail(thumbnail);
}

QPointF QnTimeSlider::positionFromValue(qint64 logicalValue, bool bound) const {
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return QPointF(
        d->pixelPosMin + GraphicsStyle::sliderPositionFromValue(m_windowStart, m_windowEnd, logicalValue, d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound), 
        0.0
    );
}

qint64 QnTimeSlider::valueFromPosition(const QPointF &position, bool bound) const {
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return GraphicsStyle::sliderValueFromPosition(m_windowStart, m_windowEnd, position.x(), d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound);
}

qreal QnTimeSlider::quickPositionFromValue(qint64 logicalValue, bool bound) const {
    if(bound) {
        return m_boundMapper(logicalValue);
    } else {
        return m_unboundMapper(logicalValue);
    }
}

void QnTimeSlider::finishAnimations() {
    animateStepValues(10 * 1000);
    animateThumbnails(10 * 1000);

    if(m_animating) {
        setWindow(animationStart(), animationEnd());
        m_animating = false;
    }

    kineticProcessor()->reset();
}

void QnTimeSlider::hurryKineticAnimations() {
    if(!kineticProcessor()->isRunning())
        return; /* Nothing to hurry up. */

    if(m_kineticsHurried)
        return;

    m_kineticsHurried = true;
    updateKineticProcessor();
}

void QnTimeSlider::setAnimationStart(qint64 start) {
    m_animationStart = start == minimum() ? std::numeric_limits<qint64>::min() : start;
}

void QnTimeSlider::setAnimationEnd(qint64 end) {
    m_animationEnd = end == maximum() ? std::numeric_limits<qint64>::max() : end;
}

qint64 QnTimeSlider::animationStart() {
    return m_animationStart == std::numeric_limits<qint64>::min() ? minimum() : m_animationStart;
}

qint64 QnTimeSlider::animationEnd() {
    return m_animationEnd == std::numeric_limits<qint64>::max() ? maximum() : m_animationEnd;
}

void QnTimeSlider::generateProgressPatterns() {
    const qreal stripeWidth = 8.0;

    QPainterPath path;
    path.moveTo(0.0, 0.0);
    path.lineTo(stripeWidth, 0.0);
    path.lineTo(0.0, stripeWidth * 2);
    path.closeSubpath();

    path.moveTo(stripeWidth * 2, 0.0);
    path.lineTo(stripeWidth * 3, 0.0);
    path.lineTo(stripeWidth, stripeWidth * 4);
    path.lineTo(0.0, stripeWidth * 4);
    path.closeSubpath();

    path.moveTo(stripeWidth * 4, 0.0);
    path.lineTo(stripeWidth * 4, stripeWidth * 2);
    path.lineTo(stripeWidth * 3, stripeWidth * 4);
    path.lineTo(stripeWidth * 2, stripeWidth * 4);
    path.closeSubpath();


    m_progressPastPattern = QPixmap(stripeWidth * 4, stripeWidth * 4);
    m_progressPastPattern.fill(Qt::transparent);

    QPainter pastPainter(&m_progressPastPattern);
    pastPainter.setRenderHint(QPainter::Antialiasing);
    pastPainter.fillPath(path, m_colors.pastLastMinute);


    m_progressFuturePattern = QPixmap(stripeWidth * 4, stripeWidth * 4);
    m_progressFuturePattern.fill(Qt::transparent);

    QPainter futurePainter(&m_progressFuturePattern);
    futurePainter.setRenderHint(QPainter::Antialiasing);
    futurePainter.fillPath(path, m_colors.futureLastMinute);
}

bool QnTimeSlider::isAnimatingWindow() const {
    return m_animating || kineticProcessor()->isRunning();
}

bool QnTimeSlider::scaleWindow(qreal factor, qint64 anchor) {
    qreal targetMSecsPerPixel = m_msecsPerPixel * factor;
    if(targetMSecsPerPixel < minMSecsPerPixel) {
        factor = minMSecsPerPixel / m_msecsPerPixel;
        if(qFuzzyCompare(factor, 1.0))
            return false; /* We've reached the min scale. */
    }

    qint64 start = anchor + (m_windowStart - anchor) * factor;
    qint64 end = anchor + (m_windowEnd - anchor) * factor;

    if(end > maximum()) {
        start = maximum() - (end - start);
        end = maximum();
    }
    if(start < minimum()) {
        end = minimum() + (end - start);
        start = minimum();
    }

    setWindow(start, end);

    /* If after two adjustments desired window end still lies outside the 
     * slider range, then we've reached the max scale. */
    return end <= maximum();
}

QnTimeSlider::Marker QnTimeSlider::markerFromPosition(const QPointF &pos, qreal maxDistance) const {
    if(m_selectionValid) {
        QPointF selectionStart = positionFromValue(m_selectionStart);
        if(qAbs(selectionStart.x() - pos.x()) < maxDistance)
            return SelectionStartMarker;
        
        QPointF selectionEnd = positionFromValue(m_selectionEnd);
        if(qAbs(selectionEnd.x() - pos.x()) < maxDistance)
            return SelectionEndMarker;
    }

    return NoMarker;
}

QPointF QnTimeSlider::positionFromMarker(Marker marker) const {
    switch(marker) {
    case SelectionStartMarker:
        if(!m_selectionValid) {
            return rect().topLeft();
        } else {
            return positionFromValue(m_selectionStart);
        }
    case SelectionEndMarker:
        if(!m_selectionValid) {
            return rect().topLeft();
        } else {
            return positionFromValue(m_selectionEnd);
        }
    default:
        return rect().topLeft();
    }
}

void QnTimeSlider::setMarkerSliderPosition(Marker marker, qint64 position) {
    switch(marker) {
    case SelectionStartMarker:
        setSelectionStart(position);
        break;
    case SelectionEndMarker:
        setSelectionEnd(position);
        break;
    default:
        break;
    }
}

qreal QnTimeSlider::effectiveLineStretch(int line) const {
    return m_lineData[line].visible ? m_lineData[line].stretch : 0.0;
}

QRectF QnTimeSlider::thumbnailsRect() const {
    QRectF rect = this->rect();
    return QRectF(rect.left(), rect.top(), rect.width(), thumbnailsHeight());
}

QRectF QnTimeSlider::rulerRect() const {
    QRectF rect = this->rect();
    return QRectF(rect.left(), rect.top() + thumbnailsHeight(), rect.width(), rulerHeight());
}

bool QnTimeSlider::isThumbnailsVisible() const {
    return m_thumbnailsVisible;
}

qreal QnTimeSlider::thumbnailsHeight() const {
    return size().height() - m_rulerHeight;
}

qreal QnTimeSlider::rulerHeight() const {
    return m_rulerHeight;
}

void QnTimeSlider::setRulerHeight(qreal rulerHeight) {
    if(qFuzzyCompare(m_rulerHeight, rulerHeight))
        return;

    m_prefferedHeight = m_prefferedHeight + (rulerHeight - m_rulerHeight);
    m_rulerHeight = rulerHeight;

    updateGeometry();
    updateThumbnailsStepSize(false);
    updateThumbnailsVisibility();
}

void QnTimeSlider::addThumbnail(const QnThumbnail &thumbnail) {
    if(m_thumbnailsUpdateTimer->isActive())
        return;

    if(m_thumbnailData.contains(thumbnail.time())) 
        return; /* There is no real point in overwriting existing thumbnails. Besides, it would result in flicker. */
 
    m_thumbnailData[thumbnail.time()] = ThumbnailData(thumbnail);
}

void QnTimeSlider::clearThumbnails() {
    m_thumbnailData.clear();
}

void QnTimeSlider::freezeThumbnails() {
    m_oldThumbnailData = m_thumbnailData.values();
    clearThumbnails();

    qreal center = m_thumbnailsPaintRect.center().x();
    qreal height = m_thumbnailsPaintRect.height();

    for(int i = 0; i < m_oldThumbnailData.size(); i++) {
        ThumbnailData &data = m_oldThumbnailData[i];
        data.hiding = true;

        qreal pos = quickPositionFromValue(data.thumbnail.time(), false);
        qreal width = data.thumbnail.size().width() * height / data.thumbnail.size().height();
        data.pos = (pos - center) / width;
    }
}

void QnTimeSlider::animateLastMinute(int deltaMSecs) {
    m_lastMinuteAnimationDelta += deltaMSecs;
}

const QVector<qint64> &QnTimeSlider::indicators() const {
    return m_indicators;
}

void QnTimeSlider::setIndicators(const QVector<qint64> &indicators) {
    m_indicators = indicators;
}

qint64 QnTimeSlider::localOffset() const {
    return m_localOffset;
}

void QnTimeSlider::setLocalOffset(qint64 localOffset) {
    if(m_localOffset == localOffset)
        return;

    m_localOffset = localOffset;

    updateToolTipText();
}

const QnTimeSliderColors &QnTimeSlider::colors() const {
    return m_colors;
}

void QnTimeSlider::setColors(const QnTimeSliderColors &colors) {
    m_colors = colors;
    generateProgressPatterns();
}

void QnTimeSlider::setLastMinuteIndicatorVisible(int line, bool visible) {
    if (line >= maxLines)
        return;

    m_lastMinuteIndicatorVisible[line] = visible;
}

bool QnTimeSlider::isLastMinuteIndicatorVisible(int line) const {
    if (line >= maxLines)
        return false;

    return m_lastMinuteIndicatorVisible[line];
}


// -------------------------------------------------------------------------- //
// Updating
// -------------------------------------------------------------------------- //
void QnTimeSlider::updatePixmapCache() {
    m_pixmapCache->setFont(font());
    m_pixmapCache->setColor(palette().color(QPalette::WindowText));
    m_noThumbnailsPixmap = m_pixmapCache->textPixmap(tr("NO THUMBNAILS\nAVAILABLE"), 16); 

    updateLineCommentPixmaps();
    updateTickmarkTextSteps();
}

void QnTimeSlider::updateKineticProcessor() {
    KineticCuttingProcessor *kineticProcessor = checked_cast<KineticCuttingProcessor *>(this->kineticProcessor());

    if(m_kineticsHurried) {
        kineticProcessor->setMaxShiftInterval(0.4);
        kineticProcessor->setFriction(degreesFor2x * 2.0);
        kineticProcessor->setMaxSpeedMagnitude(degreesFor2x * 8);
        kineticProcessor->setSpeedCuttingThreshold(degreesFor2x / 3);
        kineticProcessor->setFlags(KineticProcessor::IgnoreDeltaTime);
    } else {
        kineticProcessor->setMaxShiftInterval(0.4);
        kineticProcessor->setFriction(degreesFor2x / 2);
        kineticProcessor->setMaxSpeedMagnitude(degreesFor2x * 8);
        kineticProcessor->setSpeedCuttingThreshold(degreesFor2x / 3);
        kineticProcessor->setFlags(KineticProcessor::IgnoreDeltaTime);
    }
}

void QnTimeSlider::updateToolTipVisibility() {
    qint64 pos = sliderPosition();

    toolTipItem()->setVisible(pos >= m_windowStart && pos <= m_windowEnd);
}

void QnTimeSlider::updateToolTipText() {
    if(!(m_options & UpdateToolTip))
        return;

    qint64 pos = sliderPosition();

    QString toolTip;
    if(m_options & UseUTC) {
        toolTip = m_locale.toString(QDateTime::fromMSecsSinceEpoch(pos + m_localOffset), m_toolTipFormat);
    } else {
        toolTip = m_locale.toString(msecsToTime(pos), m_toolTipFormat);
    }

    setToolTip(toolTip);
}

void QnTimeSlider::updateLineCommentPixmap(int line) {
    QRectF lineBarRect = positionRect(rulerRect(), lineBarPosition);

    int height = qFloor(lineBarRect.height() * m_lineData[line].stretch / m_totalLineStretch * (1.0 - lineCommentTopMargin - lineCommentBottomMargin));
    if(height <= 0)
        return;

    m_lineData[line].commentPixmap = m_pixmapCache->textPixmap(
        m_lineData[line].comment,
        height
    );
}

void QnTimeSlider::updateLineCommentPixmaps() {
    for(int i = 0; i < m_lineCount; i++)
        updateLineCommentPixmap(i);
}

void QnTimeSlider::updateSteps() {
    m_steps = (m_options & UseUTC) ? timeSteps()->absolute() : timeSteps()->relative();

    m_nextTickmarkPos.resize(m_steps.size());
    m_tickmarkLines.resize(m_steps.size());
    m_stepData.resize(m_steps.size());
}

void QnTimeSlider::updateTickmarkTextSteps() {

    // TODO: #Elric this one is VERY slow right now.

    for(int i = 0; i < m_steps.size(); i++) {
        QString text = toLongestShortString(m_steps[i]);
        QPixmap pixmap = m_pixmapCache->textPixmap(text, 10);

        /* 2.5 is the AR that our constants are expected to work well with. */
        m_stepData[i].tickmarkTextOversize = qMax(1.0, (1.0 / 2.5) * pixmap.width() / pixmap.height());
    }
}

void QnTimeSlider::updateMSecsPerPixel() {
    qreal msecsPerPixel = (m_windowEnd - m_windowStart) / size().width();
    if(qFuzzyIsNull(msecsPerPixel))
        msecsPerPixel = 1.0; /* Technically, we should never get here, but we want to feel safe. */

    if(qFuzzyCompare(m_msecsPerPixel, msecsPerPixel))
        return;

    m_msecsPerPixel = msecsPerPixel;

    updateThumbnailsStepSize(false);
    updateStepAnimationTargets();
    updateAggregationValue();
}

void QnTimeSlider::updateMinimalWindow() {
    qint64 minimalWindow = size().width() * minMSecsPerPixel;
    if(minimalWindow == m_minimalWindow)
        return;

    m_minimalWindow = minimalWindow;

    /* Re-bound. */
    setWindowStart(m_windowStart);
}

void QnTimeSlider::updateStepAnimationTargets() {
    bool updateNeeded = qAbs(m_msecsPerPixel - m_animationUpdateMSecsPerPixel) / qMin(m_msecsPerPixel, m_animationUpdateMSecsPerPixel) > msecsPerPixelChangeThreshold;
    if(!updateNeeded)
        return;

    int stepCount = m_steps.size();

    /* Find maximal index of the time step to use. */
    int maxStepIndex = stepCount - 1;
    qreal tickmarkSpanPixels = size().width() * topLevelTickmarkStep;
    for(; maxStepIndex >= 0; maxStepIndex--)
        if(m_steps[maxStepIndex].stepMSecs / m_msecsPerPixel <= tickmarkSpanPixels)
            break;
    maxStepIndex = qMax(maxStepIndex, 1); /* Display at least two tickmark levels. */

    /* Adjust target tickmark heights and opacities. */
    for(int i = 0; i < stepCount; i++) {
        TimeStepData &data = m_stepData[i];
        qreal separationPixels = m_steps[i].stepMSecs / m_msecsPerPixel;

        /* Target height & opacity. */
        qreal targetHeight;
        if (separationPixels < minTickmarkLineStepPixels) {
            targetHeight = 0.0;
        } else if(i >= maxStepIndex) {
            targetHeight = maxTickmarkHeight;
        } else  {
            targetHeight = minTickmarkHeight + (maxTickmarkHeight - minTickmarkHeight) * pow(tickmarkStepScale, maxStepIndex - i);
        }
        
        if(!qFuzzyCompare(data.targetHeight, targetHeight)) {
            data.targetHeight = targetHeight;

            if(qFuzzyIsNull(targetHeight)) {
                data.targetLineOpacity = 0.0;
            } else {
                data.targetLineOpacity = minTickmarkLineOpacity + (1.0 - minTickmarkLineOpacity) * (data.targetHeight - minTickmarkHeight) / (maxTickmarkHeight - minTickmarkHeight);
            }
        }

        qreal minTextStepPixels = minTickmarkTextStepPixels * data.tickmarkTextOversize;
        qreal criticalTextStepPixels = criticalTickmarkTextStepPixels * data.tickmarkTextOversize;

        if(separationPixels < minTextStepPixels) {
            data.targetTextOpacity = 0.0;
        } else {
            data.targetTextOpacity = qMax(minTickmarkTextOpacity, data.targetLineOpacity);
        }

        /* Adjust for max height & opacity. */
        qreal maxHeight = qMax(0.0, (separationPixels - criticalTickmarkLineStepPixels) / criticalTickmarkLineStepPixels);
        qreal maxLineOpacity = minTickmarkLineOpacity + (1.0 - minTickmarkLineOpacity) * maxHeight;
        qreal maxTextOpacity = qMin(maxLineOpacity, qMax(0.0, (separationPixels - criticalTextStepPixels) / criticalTextStepPixels));
        data.currentHeight      = qMin(data.currentHeight,      maxHeight);
        data.currentLineOpacity = qMin(data.currentLineOpacity, maxLineOpacity);
        data.currentTextOpacity = qMin(data.currentTextOpacity, maxTextOpacity);
    }

    m_animationUpdateMSecsPerPixel = m_msecsPerPixel;
}

void QnTimeSlider::animateStepValues(int deltaMSecs) {
    int stepCount = m_steps.size();
    qreal dt = deltaMSecs / 1000.0;

    QRectF tickmarkBarRect = positionRect(rulerRect(), tickmarkBarPosition);
    const qreal maxTickmarkTextHeightPixels = tickmarkBarRect.height() * maxTickmarkHeight * tickmarkTextHeight;

    /* Range of valid text height values. */
    const qreal textHeightRangePixels = qMax(0.0, maxTickmarkTextHeightPixels - minTickmarkTextHeightPixels);

    for(int i = 0; i < stepCount; i++) {
        TimeStepData &data = m_stepData[i];

        qreal heightSpeed  = speed(data.currentHeight,      0.5, tickmarkHeightStartingAnimationSpeed,  tickmarkHeightRelativeAnimationSpeed);
        qreal opacitySpeed = speed(data.currentLineOpacity, 0.5, tickmarkOpacityStartingAnimationSpeed, tickmarkOpacityRelativeAnimationSpeed);

        data.currentHeight      = adjust(data.currentHeight,        data.targetHeight,      heightSpeed * dt);
        data.currentLineOpacity = adjust(data.currentLineOpacity,   data.targetLineOpacity, opacitySpeed * dt);
        data.currentTextOpacity = adjust(data.currentTextOpacity,   data.targetTextOpacity, opacitySpeed * dt);

        data.currentTextHeight  = minTickmarkTextHeightPixels + qRound(data.currentHeight * textHeightRangePixels);
        data.currentLineHeight  = qMin(data.currentHeight * tickmarkBarRect.height() * tickmarkLineHeight, tickmarkBarRect.height() - data.currentTextHeight - 1.0);
    }
}

void QnTimeSlider::animateThumbnails(int deltaMSecs) {
    qreal dt = deltaMSecs / 1000.0;

    bool oldShown = false;
    for(QList<ThumbnailData>::iterator pos = m_oldThumbnailData.begin(); pos != m_oldThumbnailData.end(); pos++)
        oldShown |= animateThumbnail(dt, *pos);
    
    if(!oldShown) {
        m_oldThumbnailData.clear();

        for(QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.begin(); pos != m_thumbnailData.end(); pos++)
            animateThumbnail(dt, *pos);
    }
}

bool QnTimeSlider::animateThumbnail(qreal dt, ThumbnailData &data) {
    if(data.selecting && !data.hiding) {
        data.selection = qMin(1.0, data.selection + dt * 4.0);
    } else {
        data.selection = qMax(0.0, data.selection - dt * 4.0);
    }

    if(data.hiding) {
        data.opacity = qMax(0.0, data.opacity - dt * 4.0);

        return !qFuzzyIsNull(data.opacity);
    } else {
        data.opacity = qMin(1.0, data.opacity + dt * 4.0);

        return true;
    }
}

void QnTimeSlider::updateAggregationValue() {
    if (m_lineData.isEmpty())
        return;

    /* Aggregate to 1/16-pixels. */
    qreal aggregationMSecs = qMax(m_msecsPerPixel / 16.0, 1.0);
    
    /* Calculate only once presuming current value is the same on all lines. */
    qreal oldAggregationMSecs = m_lineData[0].timeStorage.aggregationMSecs();
    if(oldAggregationMSecs / 2.0 < aggregationMSecs && aggregationMSecs < oldAggregationMSecs * 2.0)
        return;

    for(int line = 0; line < m_lineCount; line++)
        m_lineData[line].timeStorage.setAggregationMSecs(aggregationMSecs);
}

void QnTimeSlider::updateTotalLineStretch() {
    qreal totalLineStretch = 0.0;
    for(int line = 0; line < m_lineCount; line++)
        totalLineStretch += effectiveLineStretch(line);
    
    if(qFuzzyCompare(m_totalLineStretch, totalLineStretch))
        return;
    m_totalLineStretch = totalLineStretch;

    updateLineCommentPixmaps();
}

void QnTimeSlider::updateThumbnailsPeriod() {
    if (!thumbnailsLoader())
        return;

    if(m_thumbnailsUpdateTimer->isActive() || !m_oldThumbnailData.isEmpty())
        return;

    if(qFuzzyIsNull(thumbnailsHeight()))
        return; // TODO: #Elric may be a wrong place for the check

    thumbnailsLoader()->setTimePeriod(m_windowStart, m_windowEnd);
}

void QnTimeSlider::updateThumbnailsStepSizeLater() {
    m_thumbnailsUpdateTimer->start(250); /* Re-start it if it's active. */
}

void QnTimeSlider::updateThumbnailsStepSizeForced() {
    m_thumbnailsUpdateTimer->stop();
    updateThumbnailsStepSize(true, true);
}

void QnTimeSlider::updateThumbnailsStepSizeTimer() {
    m_thumbnailsUpdateTimer->stop();
    updateThumbnailsStepSize(true);
}

void QnTimeSlider::updateThumbnailsStepSize(bool instant, bool forced) {
    if (!thumbnailsLoader())
        return; /* Nothing to update. */

    if (m_thumbnailsUpdateTimer->isActive()) {
        updateThumbnailsStepSizeLater(); /* Re-start the timer. */
        return;
    }
    
    /* Calculate new bounding size. */
    int boundingHeigth = qRound(thumbnailsHeight());
    if(boundingHeigth < thumbnailHeightForDrawing)
        boundingHeigth = 0;

    QSize boundingSize = QSize(boundingHeigth * 256, boundingHeigth);
    bool boundingSizeChanged = thumbnailsLoader()->boundingSize() != boundingSize;

    /* Calculate actual thumbnail size. */
#if 0
    QSize size = thumbnailsLoader()->thumbnailSize();
    if(size.isEmpty()) {
        size = QSize(boundingHeigth * 4 / 3, boundingHeigth); /* For uninitialized loader, assume 4:3 aspect ratio. */
    } else if(size.height() != boundingHeigth) { // TODO: evil hack, should work on signals.
        size = QSize(boundingHeigth * size.width() / size.height() , boundingHeigth);
    }
#else
    QSize size;
    if(m_thumbnailsAspectRatio > 0)
        size = QSize(boundingHeigth * m_thumbnailsAspectRatio, boundingHeigth);
#endif
    if(size.isEmpty())
        return;

    /* Calculate new time step. */
    qint64 timeStep = m_msecsPerPixel * size.width();
    bool timeStepChanged = qAbs(timeStep / m_msecsPerPixel - thumbnailsLoader()->timeStep() / m_msecsPerPixel) >= 1;

    /* Nothing changed? Leave. */
    if(!timeStepChanged && !boundingSizeChanged && !m_thumbnailData.isEmpty())
        return;

    /* Ok, thumbnails have to be re-generated. So we first freeze our old thumbnails. */
    if(m_oldThumbnailData.isEmpty())
        freezeThumbnails();

    /* If animation is running, we want to wait until it's finished. 
     * We also don't want to update thumbnails too often. */
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if((!instant || isAnimatingWindow() || currentTime - m_lastThumbnailsUpdateTime < 1000) && !forced) {
        updateThumbnailsStepSizeLater();
    } else {
        m_lastThumbnailsUpdateTime = currentTime;
        thumbnailsLoader()->setBoundingSize(boundingSize + QSize(1, 1)); /* Evil hack to force update even if size didn't change. */
        thumbnailsLoader()->setBoundingSize(boundingSize);
        thumbnailsLoader()->setTimeStep(timeStep);
        updateThumbnailsPeriod();
    }
}

void QnTimeSlider::setThumbnailSelecting(qint64 time, bool selecting) {
    if(time < 0)
        return;

    QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.find(time);
    if(pos == m_thumbnailData.end())
        return;

    qint64 actualTime = pos->thumbnail.actualTime();

    QMap<qint64, ThumbnailData>::iterator ipos;
    for(ipos = pos; ipos->thumbnail.actualTime() == actualTime; ipos--) {
        ipos->selecting = selecting;
        if(ipos == m_thumbnailData.begin())
            break;
    }
    for(ipos = pos + 1; ipos != m_thumbnailData.end() && ipos->thumbnail.actualTime() == actualTime; ipos++)
        ipos->selecting = selecting;
}

void QnTimeSlider::updateThumbnailsVisibility() {
    bool visible = thumbnailsHeight() >= thumbnailHeightForDrawing;
    if(m_thumbnailsVisible == visible)
        return;

    m_thumbnailsVisible = visible;

    emit thumbnailsVisibilityChanged();
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    sendPendingMouseMoves(widget);

    m_unboundMapper = QnLinearFunction(m_windowStart, positionFromValue(m_windowStart).x(), m_windowEnd, positionFromValue(m_windowEnd).x());
    m_boundMapper = QnBoundedLinearFunction(m_unboundMapper, rect().left(), rect().right());

    QRectF thumbnailsRect = this->thumbnailsRect();
    QRectF rulerRect = this->rulerRect();
    QRectF dateBarRect = positionRect(rulerRect, dateBarPosition);
    QRectF lineBarRect = positionRect(rulerRect, lineBarPosition);
    QRectF tickmarkBarRect = positionRect(rulerRect, tickmarkBarPosition);
    QRectF bookmarkRect = lineBarRect;
    
    qreal lineTop, lineUnit = qFuzzyIsNull(m_totalLineStretch) ? 0.0 : lineBarRect.height() / m_totalLineStretch;

    /* Draw background. */
    if(qFuzzyIsNull(m_totalLineStretch)) {
        drawSolidBackground(painter, this->rect());
    } else {
        drawSolidBackground(painter, thumbnailsRect);
        drawSolidBackground(painter, tickmarkBarRect);
        drawSolidBackground(painter, dateBarRect);

        /* Draw lines background (that is, time periods). */
        lineTop = lineBarRect.top();
        for(int line = 0; line < m_lineCount; line++) {
            qreal lineHeight = lineUnit * effectiveLineStretch(line);
            if(qFuzzyIsNull(lineHeight))
                continue;

            QRectF lineRect(lineBarRect.left(), lineTop, lineBarRect.width(), lineHeight);

            drawPeriodsBar(
                painter,
                m_lineData[line].timeStorage.aggregated(Qn::RecordingContent),
                m_lineData[line].timeStorage.aggregated(Qn::MotionContent),
                m_lineData[line].timeStorage.aggregated(Qn::BookmarksContent),
                lineRect
            );

            lineTop += lineHeight;
        }

        lineTop = lineBarRect.top();
        for(int line = 0; line < m_lineCount; line++) {
            qreal lineHeight = lineUnit * effectiveLineStretch(line);
            if(qFuzzyIsNull(lineHeight))
                continue;

            QRectF lineRect(lineBarRect.left(), lineTop, lineBarRect.width(), lineHeight);

            if (m_lastMinuteIndicatorVisible[line])
                drawLastMinute(painter, lineRect);
            drawSeparator(painter, lineRect);

            lineTop += lineHeight;
        }
    }

    /* Draw thumbnails. */
    drawThumbnails(painter, QnGeometry::eroded(thumbnailsRect, MarginsF(1.0, 1.0, 1.0, 1.0))); /* Rect is eroded so that thumbnails don't get painted under borders. */

    /* Draw separators. */
    if(!thumbnailsRect.isEmpty())
        drawSeparator(painter, thumbnailsRect);
    drawSeparator(painter, tickmarkBarRect);
    drawSeparator(painter, dateBarRect);

    /* Draw border. */
    {
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        QnScopedPainterPenRollback penRollback(painter, QPen(m_colors.separator, 0));
        QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
        painter->drawRect(rect());
    }

    /* Draw selection. */
    drawSelection(painter);

    /* Draw line comments. */
    lineTop = lineBarRect.top();
    for(int line = 0; line < m_lineCount; line++) {
        qreal lineHeight = lineUnit * effectiveLineStretch(line);
        if(qFuzzyIsNull(lineHeight))
            continue;

        QPixmap pixmap = m_lineData[line].commentPixmap;
        QRectF textRect(
            lineBarRect.left(),
            lineTop + lineHeight * lineCommentTopMargin,
            pixmap.width(),
            pixmap.height()
        );

        painter->drawPixmap(textRect, pixmap, pixmap.rect());

        lineTop += lineHeight;
    }

    /* Draw tickmarks. */
    drawTickmarks(painter, tickmarkBarRect);

    /* Draw dates. */
    drawDates(painter, dateBarRect);

    /* Draw bookmarks. */
    drawBookmarks(painter, bookmarkRect);

    /* Draw position marker. */
    drawMarker(painter, sliderPosition(), m_colors.positionMarker);

    /* Draw indicators. */
    foreach(qint64 position, m_indicators)
        drawMarker(painter, position, m_colors.indicator);
}

void QnTimeSlider::drawSeparator(QPainter *painter, const QRectF &rect) {
    if(qFuzzyCompare(rect.top(), this->rect().top()))
        return; /* Don't draw separator at the top of the widget. */

    QnScopedPainterPenRollback penRollback(painter, QPen(m_colors.separator, 0));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
    painter->drawLine(rect.topLeft(), rect.topRight());
}

void QnTimeSlider::drawLastMinute(QPainter *painter, const QRectF &rect) {
    const qreal moveSpeed = 0.05;

    qint64 startTime = maximum() - 60 * 1000;
    qreal startPos = quickPositionFromValue(startTime);
    if (startPos >= rect.right())
        return;

    qreal shift = qMod(m_lastMinuteAnimationDelta * moveSpeed, static_cast<qreal>(m_progressPastPattern.width()));
    m_lastMinuteAnimationDelta = shift / moveSpeed;
    QTransform brushTransform = QTransform::fromTranslate(-shift, 0.0);

    qreal sliderPos = quickPositionFromValue(sliderPosition());

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    if (sliderPos > startPos && !qFuzzyEquals(startPos, sliderPos)) {
        QBrush brush(m_progressPastPattern);
        brush.setTransform(brushTransform);
        painter->fillRect(QRectF(QPointF(startPos, rect.top()), QPointF(sliderPos, rect.bottom())), brush);
    }

    if (!qFuzzyEquals(rect.right(), sliderPos)) {
        QBrush brush(m_progressFuturePattern);
        brush.setTransform(brushTransform);
        painter->fillRect(QRectF(QPointF(qMax(startPos, sliderPos), rect.top()), rect.bottomRight()), brush);
    }
}

void QnTimeSlider::drawSelection(QPainter *painter) {
    if(!m_selectionValid)
        return;

    if(m_selectionStart == m_selectionEnd) {
        drawMarker(painter, m_selectionStart, m_colors.selectionMarker);
        return;
    }

    qint64 selectionStart = qMax(m_selectionStart, m_windowStart);
    qint64 selectionEnd = qMin(m_selectionEnd, m_windowEnd);
    if(selectionStart < selectionEnd) {
        QRectF rect = this->rect();
        QRectF selectionRect(
            QPointF(quickPositionFromValue(selectionStart), rect.top()),
            QPointF(quickPositionFromValue(selectionEnd), rect.bottom())
        );

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        painter->fillRect(selectionRect, m_colors.selection);
    }

    drawMarker(painter, m_selectionStart, m_colors.selectionMarker);
    drawMarker(painter, m_selectionEnd, m_colors.selectionMarker);
}

void QnTimeSlider::drawMarker(QPainter *painter, qint64 pos, const QColor &color) {
    if(pos < m_windowStart || pos > m_windowEnd) 
        return;
    
    //QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
    QnScopedPainterPenRollback penRollback(painter, QPen(color, 0));

    qreal x = quickPositionFromValue(pos);
    QRectF rect = this->rect();

    painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

void QnTimeSlider::drawPeriodsBar(QPainter *painter, const QnTimePeriodList &recorded, const QnTimePeriodList &motion, const QnTimePeriodList &bookmarks, const QRectF &rect) {
    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location. 
     * It makes transparent time slider look better. */

    /* Note that constness of period lists is important here as requesting
     * iterators from a non-const object will result in detach. */
    const QnTimePeriodList periods[Qn::TimePeriodContentCount] = {recorded, motion, bookmarks};

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodContentCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodContentCount];
    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
         pos[i] = periods[i].findNearestPeriod(minimumValue, true);
         end[i] = periods[i].findNearestPeriod(maximumValue, true);
         if(end[i] != periods[i].end() && end[i]->contains(maximumValue))
             end[i]++;
    }

    qint64 value = minimumValue;
    bool inside[Qn::TimePeriodContentCount];
    for(int i = 0; i < Qn::TimePeriodContentCount; i++)
        inside[i] = pos[i] == end[i] ? false : pos[i]->contains(value);

    QnTimeSliderChunkPainter chunkPainter(this, painter);
    chunkPainter.start(value, this->sliderPosition(), m_msecsPerPixel, rect);

    while(value != maximumValue) {
        qint64 nextValue[Qn::TimePeriodContentCount] = {maximumValue, maximumValue, maximumValue};
        for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
            if(pos[i] == end[i]) 
                continue;
            
            if(!inside[i]) {
                nextValue[i] = qMin(maximumValue, pos[i]->startTimeMs);
                continue;
            }
            
            if(pos[i]->durationMs != -1)
                nextValue[i] = qMin(maximumValue, pos[i]->startTimeMs + pos[i]->durationMs);
        }

        qint64 bestValue = qMin(qMin(nextValue[0], nextValue[1]), nextValue[2]);
        
        Qn::TimePeriodContent content;
        if (inside[Qn::BookmarksContent]) {
            content = Qn::BookmarksContent;
        } else if (inside[Qn::MotionContent]) {
            content = Qn::MotionContent;
        } else if (inside[Qn::RecordingContent]) {
            content = Qn::RecordingContent;
        } else {
            content = Qn::TimePeriodContentCount;
        }

        chunkPainter.paintChunk(bestValue - value, content);

        for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
            if(bestValue != nextValue[i])
                continue;

            if(inside[i])
                pos[i]++;
            inside[i] = !inside[i];
        }

        value = bestValue;
    }

    chunkPainter.stop();
}


void QnTimeSlider::drawSolidBackground(QPainter *painter, const QRectF &rect) {
    qreal leftPos = quickPositionFromValue(windowStart());
    qreal rightPos = quickPositionFromValue(windowEnd());
    qreal centralPos = quickPositionFromValue(sliderPosition());

    if(!qFuzzyCompare(leftPos, centralPos))
        painter->fillRect(QRectF(leftPos, rect.top(), centralPos - leftPos, rect.height()), m_colors.pastBackground);
    if(!qFuzzyCompare(rightPos, centralPos))
        painter->fillRect(QRectF(centralPos, rect.top(), rightPos - centralPos, rect.height()), m_colors.futureBackground);
}

void QnTimeSlider::drawTickmarks(QPainter *painter, const QRectF &rect) {
    int stepCount = m_steps.size();

    /* Find minimal tickmark step index. */
    int minStepIndex = 0;
    for(; minStepIndex < stepCount; minStepIndex++)
        if(!qFuzzyIsNull(m_stepData[minStepIndex].currentHeight))
            break;
    if(minStepIndex >= stepCount)
        minStepIndex = stepCount - 1; /* Tests show that we can actually get here. */

    /* Find initial and maximal positions. */
    QPointF overlap(maxTickmarkTextSizePixels / 2.0, 0.0);
    qint64 startPos = qMax(minimum() + 1, valueFromPosition(positionFromValue(m_windowStart) - overlap, false)) + m_localOffset;
    qint64 endPos = qMin(maximum() - 1, valueFromPosition(positionFromValue(m_windowEnd) + overlap, false)) + m_localOffset;

    /* Initialize next positions for tickmark steps. */
    for(int i = minStepIndex; i < stepCount; i++)
        m_nextTickmarkPos[i] = roundUp(startPos, m_steps[i]);

    /* Draw tickmarks. */
    for(int i = 0; i < m_tickmarkLines.size(); i++)
        m_tickmarkLines[i].clear();

    while(true) {
        qint64 pos = m_nextTickmarkPos[minStepIndex];
        if(pos > endPos)
            break;

        /* Find index of the step to use. */
        int index = minStepIndex;
        for(; index < stepCount; index++) {
            if(m_nextTickmarkPos[index] == pos) {
                m_nextTickmarkPos[index] = add(pos, m_steps[index]);
            } else {
                break;
            }
        }
        index--;

        qreal x = quickPositionFromValue(pos - m_localOffset, false);

        /* Draw label if needed. */
        qreal lineHeight = m_stepData[index].currentLineHeight;
        if(!qFuzzyIsNull(m_stepData[index].currentTextOpacity)) {
            QPixmap pixmap = m_pixmapCache->positionShortPixmap(pos, m_stepData[index].currentTextHeight, m_steps[index]);
            QRectF textRect(x - pixmap.width() / 2.0, rect.top() + lineHeight, pixmap.width(), pixmap.height());

            QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_stepData[index].currentTextOpacity);
            drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());
        }

        /* Calculate line ends. */
        if(pos - m_localOffset >= m_windowStart && pos - m_localOffset <= m_windowEnd) {
            m_tickmarkLines[index] << 
                QPointF(x, rect.top() + 1.0 /* To prevent antialiased lines being drawn outside provided rect. */) <<
                QPointF(x, rect.top() + lineHeight);
        }
    }

    /* Draw tickmarks. */
    {
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
        for(int i = minStepIndex; i < stepCount; i++) {
            painter->setPen(toTransparent(m_colors.tickmark, m_stepData[i].currentLineOpacity));
            painter->drawLines(m_tickmarkLines[i]);
        }
    }
}

void QnTimeSlider::drawDates(QPainter *painter, const QRectF &rect) {
    int stepCount = m_steps.size();

    /* Find index of the highlight time step. */
    int highlightIndex = 0;
    qreal highlightSpanPixels = qMax(size().width() * minDateSpanFraction, minDateSpanPixels);
    for(; highlightIndex < stepCount; highlightIndex++)
        if(!m_steps[highlightIndex].longFormat.isEmpty() && m_steps[highlightIndex].stepMSecs / m_msecsPerPixel >= highlightSpanPixels)
            break;
    highlightIndex = qMin(highlightIndex, stepCount - 1); //TODO: #Elric remove this line.
    const QnTimeStep &highlightStep = m_steps[highlightIndex];

    /* Do some precalculations. */
    const qreal textHeight = rect.height() * (1.0 - dateTextTopMargin - dateTextBottomMargin);
    const qreal textTopMargin = rect.height() * dateTextTopMargin;

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);

    /* Draw highlight. */
    qint64 pos1 = roundUp(m_windowStart + m_localOffset, highlightStep);
    qint64 pos0 = sub(pos1, highlightStep);
    qreal x0 = quickPositionFromValue(m_windowStart);
    qint64 number = absoluteNumber(pos1, highlightStep);
    while(true) {
        qreal x1 = quickPositionFromValue(pos1 - m_localOffset);

        painter->setPen(Qt::NoPen);
        painter->setBrush(number % 2 ? m_colors.dateOverlay : m_colors.dateOverlayAlternate);
        painter->drawRect(QRectF(x0, rect.top(), x1 - x0, rect.height()));

        QPixmap pixmap = m_pixmapCache->positionLongPixmap(pos0, textHeight, highlightStep);

        QRectF textRect((x0 + x1) / 2.0 - pixmap.width() / 2.0, rect.top() + textTopMargin, pixmap.width(), pixmap.height());
        if(textRect.left() < rect.left())
            textRect.moveRight(x1);
        if(textRect.right() > rect.right())
            textRect.moveLeft(x0);

        drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());

        if(pos1 >= m_windowEnd + m_localOffset)
            break;

        painter->setPen(QPen(m_colors.separator, 0));
        painter->drawLine(QPointF(x1, rect.top()), QPointF(x1, rect.bottom()));

        pos0 = pos1;
        pos1 = add(pos1, highlightStep);
        number++;
        x0 = x1;
    }
}

void QnTimeSlider::drawThumbnails(QPainter *painter, const QRectF &rect) {
    if (rect.height() < thumbnailHeightForDrawing)
        return;

    m_thumbnailsPaintRect = rect;

    if (!thumbnailsLoader()) {
        QSizeF labelSizeBound = rect.size();
        labelSizeBound.setHeight(m_noThumbnailsPixmap.height());

        QRectF labelRect = QnGeometry::aligned(QnGeometry::expanded(QnGeometry::aspectRatio(m_noThumbnailsPixmap.size()), labelSizeBound, Qt::KeepAspectRatio), rect, Qt::AlignCenter);
        drawCroppedPixmap(painter, labelRect, rect, m_noThumbnailsPixmap, m_noThumbnailsPixmap.rect());
        return;
    }

    qint64 step = thumbnailsLoader()->timeStep();
    if (thumbnailsLoader()->timeStep() <= 0)
        return;

    qreal aspectRatio = QnGeometry::aspectRatio(thumbnailsLoader()->thumbnailSize());
    qreal thumbnailWidth = rect.height() * aspectRatio;

    if(!m_oldThumbnailData.isEmpty() || m_thumbnailsUpdateTimer->isActive()) {
        QRectF boundingRect = rect; 
        for (int i = 0; i < m_oldThumbnailData.size(); i++) {
            const ThumbnailData &data = m_oldThumbnailData[i];
            if(data.thumbnail.isEmpty())
                continue;

            qreal x = rect.width() / 2 + data.pos * thumbnailWidth;;
            QSizeF targetSize(data.thumbnail.aspectRatio() * rect.height(), rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    } else {
        QnScopedPainterPenRollback penRollback(painter);
        QnScopedPainterBrushRollback brushRollback(painter);

        qint64 startTime = qFloor(m_windowStart, step);
        qint64 endTime = qCeil(m_windowEnd, step);

        QRectF boundingRect = rect; 
        for (qint64 time = startTime; time <= endTime; time += step) {
            QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.find(time);
            if(pos == m_thumbnailData.end())
                continue;

            ThumbnailData &data = *pos;
            if(data.thumbnail.isEmpty())
                continue;

            qreal x = quickPositionFromValue(time, false);
            QSizeF targetSize(data.thumbnail.aspectRatio() * rect.height(), rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    }
}

void QnTimeSlider::drawThumbnail(QPainter *painter, const ThumbnailData &data, const QRectF &targetRect, const QRectF &boundingRect) {
    const QImage &image = data.thumbnail.image();

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * data.opacity);

    QRectF rect;
    drawCroppedImage(painter, targetRect, boundingRect, image, image.rect(), &rect);

    if(!rect.isEmpty()) {
        qreal a = data.selection;
        qreal width = 1.0 + a * 2.0;
        QColor color = linearCombine(1.0 - a, QColor(255, 255, 255, 32), a, m_colors.selectionMarker); // TODO: #Elric customize
        rect = QnGeometry::eroded(rect, width / 2.0);

        painter->setPen(QPen(color, width));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect);

        qreal x = quickPositionFromValue(data.thumbnail.actualTime());
        if(x >= rect.left() && x <= rect.right()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(withAlpha(color, (color.alpha() + 255) / 2));
            painter->drawEllipse(QPointF(x, rect.bottom()), width * 2, width * 2);
        }
    }
    painter->setOpacity(opacity);
}

//TODO: #GDM #Bookmarks check drawBookmarks() against m_localOffset
//TODO: #GDM #Bookmarks check text length to fit right edge
//TODO: #GDM #Bookmarks check text overlapping - paint longest if overlaps
//TODO: #GDM #Bookmarks move text from left edge a bit
void QnTimeSlider::drawBookmarks(QPainter *painter, const QRectF &rect) {
    qint64 windowLength = m_windowEnd - m_windowStart;
    qint64 minBookmarkDuration = windowLength / 16;
    QnCameraBookmarkList displaying;

    QnTimePeriod window(m_windowStart, m_windowEnd - m_windowStart);
    foreach(const QnCameraBookmark &bookmark, m_bookmarks) {
        if (bookmark.name.isEmpty())
            continue;
        if (QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs).intersected(window).durationMs < minBookmarkDuration)
            continue;
        displaying << bookmark;
    }

    if (displaying.isEmpty())
        return;
    
    /* Do some precalculations. */
    const qreal textHeight = rect.height() * 0.7;
    const qreal textTopMargin = rect.height() * 0.15;

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    
    /* Draw highlight. */

    foreach (const QnCameraBookmark &bookmark, displaying) {
        qreal x = quickPositionFromValue(qMax(bookmark.startTimeMs, m_windowStart));
        QPixmap pixmap = m_pixmapCache->textPixmap(bookmark.name, textHeight);

        QRectF textRect(x, rect.top() + textTopMargin, pixmap.width(), pixmap.height());
        drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QSizeF QnTimeSlider::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    switch(which) {
    case Qt::MinimumSize: {
        QSizeF result = base_type::sizeHint(which, constraint);
        result.setHeight(m_rulerHeight);
        return result;
    }
    case Qt::PreferredSize: {
        QSizeF result = base_type::sizeHint(which, constraint);
        result.setHeight(m_prefferedHeight);
        return result;
    }
    default:
        return base_type::sizeHint(which, constraint);
    }
}

void QnTimeSlider::tick(int deltaMSecs) {
    if((m_options & AdjustWindowToPosition) && !m_animating && !isSliderDown()) {
        /* Apply window position corrections if no animation or user interaction is in progress. */
        qint64 position = this->sliderPosition();
        if(position >= m_windowStart && position <= m_windowEnd) {
            qint64 windowRange = m_windowEnd - m_windowStart;

            qreal speed;
            if(position < m_windowStart + windowRange / 3) {
                speed = -1.0 + static_cast<qreal>(position - m_windowStart) / (windowRange / 3);
            } else if(position < m_windowEnd - windowRange / 3) {
                speed = 0.0;
            } else {
                speed = 1.0 - static_cast<qreal>(m_windowEnd - position) / (windowRange / 3);
            }
            speed = speed * qMax(0.5 * windowRange, 32 * 1000.0);

            qint64 delta = speed * deltaMSecs / 1000.0;
            if(m_windowEnd + delta > maximum())
                delta = maximum() - m_windowEnd;
            if(m_windowStart + delta < minimum())
                delta = minimum() - m_windowStart;

            setWindow(m_windowStart + delta, m_windowEnd + delta);
        }
    }

    if(!m_animating) {
        animateStepValues(deltaMSecs);
    } else {
        qint64 animationStart = this->animationStart();
        qint64 animationEnd = this->animationEnd();

        qreal distance = qAbs(animationStart - m_windowStart) + qAbs(animationEnd - m_windowEnd);
        qreal delta = 8.0 * deltaMSecs / 1000.0 * qMax(distance, 2.0 * static_cast<qreal>(m_windowEnd - m_windowStart));

        if(delta > distance) {
            setWindow(animationStart, animationEnd);
        } else {
            qreal startFraction = (animationStart - m_windowStart) / distance;
            qreal endFraction  = (animationEnd - m_windowEnd) / distance;

            setWindow(
                m_windowStart + static_cast<qint64>(delta * startFraction),
                m_windowEnd + static_cast<qint64>(delta * endFraction)
            );
        }

        if(animationStart == m_windowStart && animationEnd == m_windowEnd)
            m_animating = false;

        animateStepValues(8.0 * deltaMSecs);
    }

    animateThumbnails(deltaMSecs);
    animateLastMinute(deltaMSecs);
}

void QnTimeSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    switch(change) {
    case SliderRangeChange: {
        qint64 windowStart = m_windowStart;
        qint64 windowEnd = m_windowEnd;

        if((m_options & StickToMaximum) && windowEnd == m_oldMaximum) {
            if(m_options & PreserveWindowSize)
                windowStart += maximum() - windowEnd;
            windowEnd = maximum();
        }

        if((m_options & StickToMinimum) && windowStart == m_oldMinimum) {
            if(m_options & PreserveWindowSize)
                windowEnd += minimum() - windowStart;
            windowStart = minimum();
        }

        /* Stick zoom anchor. */
        if(m_zoomAnchor == m_oldMinimum)
            m_zoomAnchor = minimum();
        if(m_zoomAnchor == m_oldMaximum)
            m_zoomAnchor = maximum();

        m_oldMinimum = minimum();
        m_oldMaximum = maximum();

        /* Re-bound window. */
        setWindow(windowStart, windowEnd);

        /* Re-bound selection. Invalidate if it's outside the new range. */
        if((m_selectionStart >= minimum() && m_selectionStart <= maximum()) || (m_selectionEnd >= minimum() && m_selectionEnd <= maximum())) {
            setSelection(m_selectionStart, m_selectionEnd);
        } else {
            setSelectionValid(false);
        }
        break;
    }
    case SliderValueChange:
        updateToolTipVisibility();
        updateToolTipText();
        break;
    default:
        break;
    }
}

void QnTimeSlider::wheelEvent(QGraphicsSceneWheelEvent *event) {
    if(m_animating) {
        event->accept();
        return; /* Do nothing if animated unzoom is in progress. */
    }

    /* delta() returns the distance that the wheel is rotated 
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    m_zoomAnchor = valueFromPosition(event->pos());
    
    /* Snap zoom anchor to window sides. */
    if(m_options & SnapZoomToSides) {
        qreal windowRange = m_windowEnd - m_windowStart;
        if((m_zoomAnchor - m_windowStart) / windowRange < zoomSideSnapDistance) {
            m_zoomAnchor = m_windowStart;
        } else if((m_windowEnd - m_zoomAnchor) / windowRange < zoomSideSnapDistance) {
            m_zoomAnchor = m_windowEnd;
        }
    }

    kineticProcessor()->shift(degrees);
    kineticProcessor()->start();

    event->accept();
}

void QnTimeSlider::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);

    updateMSecsPerPixel();
    updateLineCommentPixmaps();
    updateMinimalWindow();
    updateThumbnailsStepSize(false);
    updateThumbnailsVisibility();
}

void QnTimeSlider::kineticMove(const QVariant &degrees) {
    qreal factor = std::pow(2.0, -degrees.toReal() / degreesFor2x);
    
    if(!scaleWindow(factor, m_zoomAnchor))
        kineticProcessor()->reset();
}

void QnTimeSlider::finishKinetic() {
    if(m_kineticsHurried) {
        m_kineticsHurried = false;
        updateKineticProcessor();
    }
}

void QnTimeSlider::keyPressEvent(QKeyEvent *event) {
    switch(event->key()) {
    case Qt::Key_BracketLeft:
    case Qt::Key_BracketRight:
        if(!(m_options & SelectionEditable)) {
            base_type::keyPressEvent(event);
        } else {
            if(!isSelectionValid()) {
                setSelection(sliderPosition(), sliderPosition());
                setSelectionValid(true);
            } else {
                if(event->key() == Qt::Key_BracketLeft) {
                    setSelectionStart(sliderPosition());
                } else {
                    setSelectionEnd(sliderPosition());
                }
            }
        }
        break;
    default:
        base_type::keyPressEvent(event);
        break;
    }
}

void QnTimeSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mouseDoubleClickEvent(event);

    if((m_options & UnzoomOnDoubleClick) && event->button() == Qt::LeftButton)
        setWindow(minimum(), maximum(), true);
}

void QnTimeSlider::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverEnterEvent(event);

    unsetCursor();
}

void QnTimeSlider::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverLeaveEvent(event);

    unsetCursor();

    setThumbnailSelecting(m_lastHoverThumbnail, false);
}

void QnTimeSlider::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverMoveEvent(event);

    switch(markerFromPosition(event->pos(), hoverEffectDistance)) {
    case SelectionStartMarker:
    case SelectionEndMarker:
        setCursor(Qt::SplitHCursor);
        break;
    default:
        unsetCursor();
        break;
    }

    if(thumbnailsRect().contains(event->pos()) && thumbnailsLoader() && thumbnailsLoader()->timeStep() != 0 && m_oldThumbnailData.isEmpty() && !m_thumbnailsUpdateTimer->isActive()) {
        qint64 time = qRound(valueFromPosition(event->pos()), thumbnailsLoader()->timeStep());

        setThumbnailSelecting(m_lastHoverThumbnail, false);
        setThumbnailSelecting(time, true);
        m_lastHoverThumbnail = time;
    } else {
        setThumbnailSelecting(m_lastHoverThumbnail, false);
        m_lastHoverThumbnail = -1;
    }
}

void QnTimeSlider::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        m_dragMarker = markerFromPosition(event->pos(), hoverEffectDistance);
    } else if(event->button() == Qt::RightButton) {
        if(m_options & SelectionEditable) {
            m_dragMarker = CreateSelectionMarker; 
        } else {
            m_dragMarker = NoMarker;
        }
    }

    bool processed = false;
    if(thumbnailsLoader() && thumbnailsLoader()->timeStep() != 0 && event->button() == Qt::LeftButton && thumbnailsRect().contains(event->pos())) {
        qint64 time = qRound(valueFromPosition(event->pos(), false), thumbnailsLoader()->timeStep());
        QMap<qint64, ThumbnailData>::const_iterator pos = m_thumbnailData.find(time);
        if(pos != m_thumbnailData.end()) {
            emit thumbnailClicked();
            setSliderPosition(pos->thumbnail.actualTime(), true);
            processed = true;
        }
    }

    dragProcessor()->mousePressEvent(this, event, m_dragMarker == NoMarker && !processed);

    event->accept();
}

void QnTimeSlider::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    dragProcessor()->mouseMoveEvent(this, event);

    event->accept();
}

void QnTimeSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    dragProcessor()->mouseReleaseEvent(this, event);

    m_dragMarker = NoMarker;

    if(m_dragIsClick && event->button() == Qt::RightButton)
        emit customContextMenuRequested(event->pos(), event->screenPos());

    event->accept();
}

void QnTimeSlider::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    event->ignore();
    return;
}

void QnTimeSlider::changeEvent(QEvent *event) {
    base_type::changeEvent(event);

    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::PaletteChange:
        updatePixmapCache();
        break;
    default:
        break;
    }
}

void QnTimeSlider::startDragProcess(DragInfo *) {
    m_dragIsClick = true;
}

void QnTimeSlider::startDrag(DragInfo *info) {
    setSliderDown(true);

    if(m_dragMarker == CreateSelectionMarker) {
        qint64 pos = valueFromPosition(info->mousePressItemPos());
        setSelectionValid(true);
        setSelection(pos, pos);
        m_dragMarker = SelectionStartMarker;
    }

    if(m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker) {
        m_selecting = true;
        emit selectionPressed();
    }

    m_dragIsClick = false;

    if(m_dragMarker == NoMarker) {
        m_dragDelta = QPointF();
    } else {
        m_dragDelta = positionFromMarker(m_dragMarker) - info->mousePressItemPos();
    }
}

void QnTimeSlider::dragMove(DragInfo *info) {
    QPointF mousePos = info->mouseItemPos() + m_dragDelta;

    if(m_dragMarker == NoMarker || m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker)
        setSliderPosition(valueFromPosition(mousePos));
    
    if(m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker) {
        qint64 selectionStart = m_selectionStart;
        qint64 selectionEnd = m_selectionEnd;
        Marker otherMarker = NoMarker;
        switch(m_dragMarker) {
        case SelectionStartMarker:
            selectionStart = sliderPosition();
            otherMarker = SelectionEndMarker;
            break;
        case SelectionEndMarker:
            selectionEnd = sliderPosition();
            otherMarker = SelectionStartMarker;
            break;
        default:
            break;
        }

        if(selectionStart > selectionEnd) {
            qSwap(selectionStart, selectionEnd);
            m_dragMarker = otherMarker;
        }

        setSelection(selectionStart, selectionEnd);
    }
}

void QnTimeSlider::finishDrag(DragInfo *) {
    if(m_selecting) {
        emit selectionReleased();
        m_selecting = false;
    }

    setSliderDown(false);
}
