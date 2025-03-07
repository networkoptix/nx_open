// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline.h"

#include <array>
#include <chrono>

#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGGeometry>
#include <QtQuick/QSGFlatColorMaterial>
#include <QtQuick/QSGVertexColorMaterial>
#include <QtQuick/QSGTextureMaterial>
#include <QtQuick/QQuickWindow>
#include <QtCore/QtMath>
#include <QtCore/QElapsedTimer>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>

#include "timeline_text_helper.h"
#include "timeline_zoom_level.h"
#include "timeline_chunk_painter.h"

#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/animation/kinetic_helper.h>
#include <utils/math/color_transformations.h>

using KineticHelper = nx::vms::client::core::animation::KineticHelper<qreal>;

namespace {

    const bool drawDebugTicks = false;

    const int zoomLevelsVisible = 3;
    const qreal maxZoomLevelDiff = 1.0;
    const int minTickDps = 8;
    const int extraTickCount = 10;
    const qreal zoomAnimationSpeed = 0.004;
    const qreal textOpacityAnimationSpeed = 0.004;
    const qreal kTextSpacing = 16;
    const qreal zoomMultiplier = 2.0;
    const qreal liveOpacityAnimationSpeed = 0.01;
    const qreal stripesMovingSpeed = 0.002;
    const qreal windowMovingSpeed = 0.04;
    const qint64 correctionThreshold = 5000;
    const auto kDefaultWindowSize = std::chrono::milliseconds(std::chrono::hours(24)).count();
    const qint64 kMinimumAnimationTickMs = 10;

    struct TextMarkInfo
    {
        qint64 tick;
        qreal x;
        int zoomIndex;
    };

    int cFloor(qreal x)
    {
        int result = qFloor(x);
        if (x - result > 0.9)
            ++result;
        return result;
    }

    QImage makeStripesImage(int size, const QColor& color, const QColor& background)
    {
        if ((size - 1) & size) // not power of 2
            size = qNextPowerOfTwo(size);

        int stripeWidth = size / 2;

        QImage image(size, size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        QPainter p(&image);

        QPen pen;
        pen.setWidth(0);
        pen.setColor(Qt::transparent);
        p.setPen(pen);
        p.setBrush(color);

        QPolygon polygon;
        polygon.append(QPoint(-size, size));
        polygon.append(QPoint(0, 0));
        polygon.append(QPoint(stripeWidth, 0));
        polygon.append(QPoint(-stripeWidth, size));

        int colorNum = 0;
        while (polygon[0].x() <= stripeWidth)
        {
            p.setBrush(++colorNum % 2 ? color : background);
            p.drawPolygon(polygon);
            polygon.translate(stripeWidth, 0);
        }

        return image;
    }

} // anonymous namespace

class QnTimelinePrivate
{
public:
    QnTimeline* parent = nullptr;

    QColor textColor = QColor("#727272");
    QColor chunkBarColor = toTransparent(QColor("#2A551E"), 0.4);

    QColor chunkColor = QColor("#3A911E");
    QColor loadingChunkColor = toTransparent(chunkColor, 0.6);
    QColor motionModeChunkColor = toTransparent(QColor("#2A551E"), 0.6);

    QColor activeLiveColorLight = QColor("#358815");
    QColor activeLiveColorDark = QColor("#398f16");
    QColor inactiveLiveColorLight = toTransparent(QColor("#3d6228"), 0.4);
    QColor inactiveLiveColorDark = toTransparent(QColor("#2f4623"), 0.4);
    QColor motionLoadingLiveColorLight = toTransparent(QColor("#358815"), 0.6);
    QColor motionLoadingLiveColorDark = toTransparent(QColor("#398f16"), 0.6);

    QColor motionColor = QColor("#F02C2C");
    QColor motionLoadingColor = toTransparent(QColor("#F02C2C"), 0.6);

    int chunkBarHeight = 48;
    int textY = -1;

    bool motionSearchMode = false;
    bool changingMotionRoi = false;
    bool loadingMotion = false;
    bool loadingState = false;
    bool highlightArchiveChunks = false;

    QSGTexture* stripesDarkTexture = nullptr;
    QSGTexture* stripesLightTexture = nullptr;
    qreal activeLiveOpacity = 0.0;
    qreal liveOpacity = 0.0;
    qreal stripesPosition = 0.0;
    bool stickToEnd = false;
    qreal autoPlaySpeed = 1.0;
    qreal playSpeedCorrection = 1.0;
    qint64 previousCorrectionTime = -1;
    qint64 previousCorrectedPosition = 0;

    qint64 initialWindowStartTime = -1;
    qint64 startBoundTime = -1;
    qint64 endBoundTime = -1;
    qint64 targetPosition = -1;
    bool autoReturnToBounds = true;

    qint64 windowStart = 0;
    qint64 windowEnd = 0;

    QFont textFont;
    QSGTexture* textTexture = nullptr;
    QScopedPointer<QnTimelineTextHelper> textHelper;

    int prevZoomIndex = -1;
    qreal zoomLevel = 1.0;
    qreal targetZoomLevel = 1.0;
    qreal textLevel = 1.0;
    qreal targetTextLevel = 1.0;
    qreal textOpacity = 1.0;

    qint64 stickyTime = -1;
    KineticHelper stickyPointKineticHelper;
    qreal startZoom = 1.0;
    qint64 startWindowSize = 0;
    KineticHelper zoomKineticHelper;
    bool dragWasInterruptedByZoom = false;

    QVector<QnTimelineZoomLevel> zoomLevels;
    QVector<qreal> maxZoomLevelTextLength;

    QnTimePeriodList timePeriods[Qn::TimePeriodContentCount];

    int displayOffset = 0;
    QLocale locale = QLocale::system();

    QElapsedTimer animationTimer;
    qint64 prevAnimationMs = 0;

    nx::vms::client::core::ChunkProvider* chunkProvider = nullptr;
    qint64 rightChunksOffsetMs = 0;

public:
    QnTimelinePrivate(QnTimeline* parent):
        parent(parent),
        windowStart(QDateTime::currentMSecsSinceEpoch() - kDefaultWindowSize / 2),
        windowEnd(windowStart + kDefaultWindowSize)
    {
        const int sec = 1000;
        const int min = 60 * sec;
        const int hour = 60 * min;

        zoomLevels.append({QnTimelineZoomLevel::Milliseconds, 10});
        zoomLevels.append({QnTimelineZoomLevel::Milliseconds, 50});
        zoomLevels.append({QnTimelineZoomLevel::Milliseconds, 100});
        zoomLevels.append({QnTimelineZoomLevel::Milliseconds, 500});
        zoomLevels.append({QnTimelineZoomLevel::Seconds, sec});
        zoomLevels.append({QnTimelineZoomLevel::Seconds, 5 * sec});
        zoomLevels.append({QnTimelineZoomLevel::Seconds, 10 * sec});
        zoomLevels.append({QnTimelineZoomLevel::Seconds, 30 * sec});
        zoomLevels.append({QnTimelineZoomLevel::Minutes, min});
        zoomLevels.append({QnTimelineZoomLevel::Minutes, 5 * min});
        zoomLevels.append({QnTimelineZoomLevel::Minutes, 10 * min});
        zoomLevels.append({QnTimelineZoomLevel::Minutes, 30 * min});
        zoomLevels.append({QnTimelineZoomLevel::Hours, hour});
        zoomLevels.append({QnTimelineZoomLevel::Hours, 3 * hour});
        zoomLevels.append({QnTimelineZoomLevel::Hours, 6 * hour});
        zoomLevels.append({QnTimelineZoomLevel::Hours, 12 * hour});
        zoomLevels.append({QnTimelineZoomLevel::Days, 1});
        zoomLevels.append({QnTimelineZoomLevel::Days, 5});
        zoomLevels.append({QnTimelineZoomLevel::Days, 15});
        zoomLevels.append({QnTimelineZoomLevel::Months, 1});
        zoomLevels.append({QnTimelineZoomLevel::Months, 3});
        zoomLevels.append({QnTimelineZoomLevel::Months, 6});
        zoomLevels.append({QnTimelineZoomLevel::Years,  1});
        zoomLevels.append({QnTimelineZoomLevel::Years, 5});
        zoomLevels.append({QnTimelineZoomLevel::Years, 10});
        targetZoomLevel = zoomLevel = zoomLevels.size() - zoomLevelsVisible;

        static constexpr int kMaxStickyPointSpeed = 15;

        stickyPointKineticHelper.setMaxSpeed(kMaxStickyPointSpeed);
        stickyPointKineticHelper.setMinSpeed(-kMaxStickyPointSpeed);
        zoomKineticHelper.setMinSpeed(-4.0);
        zoomKineticHelper.setMaxSpeed(50);
        zoomKineticHelper.setMinimum(0);

        animationTimer.start();
    }


    void setWindow(qint64 start, qint64 end);
    void updateZoomLevel()
    {
        int index = (prevZoomIndex == -1) ? zoomLevels.size() - 1 : qMax(prevZoomIndex, 0);
        int tickCount = zoomLevels[index].tickCount(windowStart, windowEnd);
        qreal width = parent->width();
        qreal tickSize = width / tickCount;

        if (tickSize < minTickDps)
        {
            while (tickSize < minTickDps && index < zoomLevels.size() - zoomLevelsVisible)
            {
                ++index;
                tickCount = zoomLevels[index].tickCount(windowStart, windowEnd);
                tickSize = width / tickCount;
            }
        }
        else
        {
            while (index > 0)
            {
                tickCount = zoomLevels[index - 1].tickCount(windowStart, windowEnd);
                tickSize = width / tickCount;
                if (tickSize >= minTickDps)
                    --index;
                else
                    break;
            }
        }

        if (prevZoomIndex == -1)
        {
            prevZoomIndex = index;
            targetZoomLevel = zoomLevel = index;
        }

        if (prevZoomIndex != index)
        {
            targetZoomLevel = index;
            parent->update();
        }
    }

    int maximumTickLevel(qint64 tick) const
    {
        for (int i = zoomLevels.size() - 1; i > 0; --i)
        {
            if (zoomLevels[i].testTick(tick))
                return i;
        }
        return 0;
    }

    qint64 pixelPosToTime(qreal x) const
    {
        return windowStart + static_cast<qint64>(x / parent->width() * (windowEnd - windowStart));
    }

    qreal timeToPixelPos(qint64 time) const
    {
        return parent->width() *
            (static_cast<qreal>(time - windowStart) / (windowEnd - windowStart));
    }

    qint64 stickPoint() const
    {
        return windowStart;
    }

    void setLoadingState(bool value)
    {
        if (loadingState == value)
            return;

        loadingState = value;
        updateStripesTextures();
        parent->update();
    }
    void updateLoadingState()
    {
        setLoadingState(motionSearchMode && (loadingMotion || changingMotionRoi));
    }

    int maxSiblingLevel(int zoomLevel) const
    {
        if (zoomLevel <= 3)
            return 3;
        else if (zoomLevel <= 7)
            return 7;
        else if (zoomLevel <= 11)
            return 11;
        else if (zoomLevel <= 14)
            return 14;
        else if (zoomLevel <= 15)
            return 15;
        else if (zoomLevel <= 16)
            return 16;
        else
            return 19;
    }

    qint64 adjustTime(qint64 time) const
    {
        return time + displayOffset;
    }

    void updateTextHelper();
    void updateStripesTextures();

    void tryFitInBounds();

    void animateProperties();
    void zoomWindow(qreal factor);

    qint64 startBoundTimeValue() const;
    qint64 endBoundTimeValue() const;

    void setStickToEnd(bool stickToEnd);

    int calculateTargetTextLevel() const;
    QVector<TextMarkInfo> calculateVisibleTextMarks() const;

    QSize placeSymbol(qreal x, qreal y, QChar symbol, QSGGeometry::TexturedPoint2D* points);
    int placeText(const TextMarkInfo& textMark, int textLevel, QSGGeometry::TexturedPoint2D* points);

    bool hasArchive() const;
};

QnTimeline::QnTimeline(QQuickItem* parent):
    QQuickItem(parent),
    d(new QnTimelinePrivate(this))
{
    setFlag(QQuickItem::ItemHasContents);
    setAcceptedMouseButtons(Qt::LeftButton);

    connect(this, &QnTimeline::widthChanged, this, [this](){ d->updateZoomLevel(); });
    connect(this, &QnTimeline::localeChanged, this, [this]() { d->updateTextHelper(); });
    d->updateTextHelper();
}

QnTimeline::~QnTimeline()
{
}

qint64 QnTimeline::windowStart() const
{
    return d->windowStart;
}

qint64 QnTimeline::windowEnd() const
{
    return d->windowEnd;
}

qint64 QnTimeline::windowSize() const
{
    return d->windowEnd - d->windowStart;
}

void QnTimeline::setWindowSize(qint64 windowSize)
{
    const auto windowStart = position() - windowSize / 2;
    d->initialWindowStartTime = windowStart;
    d->setWindow(windowStart, windowStart + windowSize);
}

qint64 QnTimeline::position() const
{
    return windowStart() + windowSize() / 2;
}

void QnTimeline::setPosition(qint64 position)
{
    if (position == this->position()
        && (d->targetPosition == -1 || d->targetPosition == position))
    {
        return;
    }

    clearCorrection();

    setStickToEnd(position < 0);

    if (!stickToEnd())
    {
        const auto startBound = d->autoReturnToBounds ? d->startBoundTime : -1;
        const auto endBound = (d->autoReturnToBounds && d->endBoundTime != -1)
            ? d->endBoundTime
            : QDateTime::currentMSecsSinceEpoch();

        d->targetPosition = qBound(startBound, position, endBound);
    }

    update();
}

void QnTimeline::setPositionImmediately(qint64 position)
{
    d->targetPosition = -1;
    clearCorrection();
    setStickToEnd(position < 0);

    if (position == this->position())
        return;

    const auto startBound = d->autoReturnToBounds ? d->startBoundTime : -1;
    const auto endBound = (d->autoReturnToBounds && d->endBoundTime != -1)
        ? d->endBoundTime
        : QDateTime::currentMSecsSinceEpoch();

    position = stickToEnd()
        ? endBound
        : qBound(startBound, position, endBound);

    const auto windowSize = this->windowSize();
    const auto newWindowEnd = position + windowSize / 2;
    d->setWindow(newWindowEnd - windowSize, newWindowEnd);
}

bool QnTimeline::motionSearchMode() const
{
    return d->motionSearchMode;
}

void QnTimeline::setMotionSearchMode(bool value)
{
    if (d->motionSearchMode == value)
        return;

    d->motionSearchMode = value;
    emit motionSearchModeChanged();

    if (d->motionSearchMode)
        d->highlightArchiveChunks = true;

    d->updateLoadingState();
}

bool QnTimeline::changingMotionRoi() const
{
    return d->changingMotionRoi;
}

void QnTimeline::setChangingMotionRoi(bool value)
{
    if (d->changingMotionRoi == value)
        return;

    d->changingMotionRoi = value;
    emit changingMotionRoiChanged();

    d->updateLoadingState();
}

bool QnTimeline::stickToEnd() const
{
    return d->stickToEnd;
}

void QnTimeline::setStickToEnd(bool stickToEnd)
{
    if (d->stickToEnd == stickToEnd)
        return;

    if (stickToEnd)
    {
        if (!d->stickyPointKineticHelper.isStopped())
        {
            d->stickyPointKineticHelper.stop();
            emit moveFinished();
        }
    }

    d->setStickToEnd(stickToEnd);
}

void QnTimeline::setStartBound(qint64 startBound)
{
    if (d->startBoundTime == startBound)
        return;

    d->startBoundTime = startBound;
    emit startBoundChanged();

    if (d->autoReturnToBounds && (startBound == -1 || position() < startBound))
        setPositionImmediately(startBound);

    update();
}

bool QnTimeline::isAutoReturnToBoundsEnabled() const
{
    return d->autoReturnToBounds;
}

void QnTimeline::setAutoReturnToBoundsEnabled(bool enabled)
{
    if (d->autoReturnToBounds == enabled)
        return;

    d->autoReturnToBounds = enabled;
    emit autoReturnToBoundsEnabledChanged();
}

int QnTimeline::displayOffset() const
{
    return d->displayOffset;
}

void QnTimeline::setLocale(const QLocale& locale)
{
    if (d->locale == locale)
        return;

    d->locale = locale;
    emit localeChanged();
}

QLocale QnTimeline::locale() const
{
    return d->locale;
}

qint64 QnTimeline::rightChunksOffsetMs() const
{
    return d->rightChunksOffsetMs;
}

void QnTimeline::setRightChunksOffsetMs(qint64 value)
{
    if (d->rightChunksOffsetMs == value)
        return;

    d->rightChunksOffsetMs = value;
    emit rightChunksOffsetChanged();

    update();
}

void QnTimeline::setDisplayOffset(int value)
{
    if (d->displayOffset == value)
        return;

    d->displayOffset = value;
    emit displayOffsetChanged();

    update();
}

qint64 QnTimeline::startBound() const
{
    return d->startBoundTime;
}

void QnTimeline::zoomIn()
{
    d->zoomWindow(zoomMultiplier);
}

void QnTimeline::zoomOut()
{
    d->zoomWindow(1.0 / zoomMultiplier);
}

void QnTimeline::startZoom(qreal scale)
{
    Q_UNUSED(scale)

    if (!d->stickyPointKineticHelper.isStopped())
    {
        d->stickyPointKineticHelper.stop();
        d->dragWasInterruptedByZoom = true;
    }

    d->startWindowSize = windowEnd() - windowStart();
    d->startZoom = width();
    d->zoomKineticHelper.start(d->startZoom);
    update();
}

void QnTimeline::updateZoom(qreal scale)
{
    d->zoomKineticHelper.move(d->startZoom * scale);
    update();
}

void QnTimeline::finishZoom(qreal scale)
{
    d->zoomKineticHelper.finish(d->startZoom * scale);
    d->tryFitInBounds();

    if (d->dragWasInterruptedByZoom)
    {
        d->dragWasInterruptedByZoom = false;
        emit moveFinished();
    }

    update();
}

void QnTimeline::startDrag(int x)
{
    d->targetPosition = -1;
    d->zoomKineticHelper.stop();
    d->stickyTime = d->pixelPosToTime(x);
    d->stickyPointKineticHelper.start(x);
}

void QnTimeline::updateDrag(int x)
{
    d->targetPosition = -1;
    d->stickyPointKineticHelper.move(x);
    update();
}

void QnTimeline::finishDrag(int x)
{
    d->targetPosition = -1;
    d->stickyPointKineticHelper.finish(x);
    if (d->stickyPointKineticHelper.isStopped())
        emit moveFinished();
    update();
}

void QnTimeline::clearCorrection()
{
    d->previousCorrectionTime = -1;
    d->previousCorrectedPosition = -1;
    d->playSpeedCorrection = 1.0;
}

void QnTimeline::correctPosition(qint64 position)
{
    if (position < 0)
    {
        setPosition(position);
        return;
    }

    qint64 time = d->animationTimer.elapsed();
    if (d->previousCorrectionTime >= 0)
    {
        qint64 dt = time - d->previousCorrectionTime;
        qint64 prognosed = d->previousCorrectedPosition
            + static_cast<qint64>(dt * d->autoPlaySpeed * d->playSpeedCorrection);

        qint64 diff = prognosed - position;

        if (qAbs(diff) > correctionThreshold * d->autoPlaySpeed)
        {
            setPosition(position);
            return;
        }

        qint64 dp = d->previousCorrectedPosition
            + (position - d->previousCorrectedPosition) * 2 - prognosed;
        qreal correction = dp / (dt * d->autoPlaySpeed);
        // Take middle value to stabilize correction coefficient.
        d->playSpeedCorrection = (d->playSpeedCorrection + correction) / 2;
    }

    d->previousCorrectionTime = time;
    d->previousCorrectedPosition = position;
}

qint64 QnTimeline::positionAtX(qreal x) const
{
    return d->pixelPosToTime(x);
}

nx::vms::client::core::ChunkProvider* QnTimeline::chunkProvider() const
{
    return d->chunkProvider;
}

void QnTimeline::setChunkProvider(nx::vms::client::core::ChunkProvider* chunkProvider)
{
    if (d->chunkProvider == chunkProvider)
        return;

    if (d->chunkProvider)
        d->chunkProvider->disconnect(this);

    d->chunkProvider = chunkProvider;

    if (d->chunkProvider)
    {
        auto handleTimePeriodsUpdated =
            [this](Qn::TimePeriodContent type)
            {
                auto& data = d->timePeriods[type];
                data = d->chunkProvider->periods(type);
                #ifndef QN_TIME_PERIODS_STD
                    data.detach();
                #endif
                d->highlightArchiveChunks = false;
                update();
            };

        connect(d->chunkProvider, &nx::vms::client::core::ChunkProvider::periodsUpdated,
            this, handleTimePeriodsUpdated);

        const auto handleLoadingMotionChanged =
            [this, chunkProvider]()
            {
                d->loadingMotion = chunkProvider->isLoadingMotion();
                d->updateLoadingState();
            };
        connect(d->chunkProvider, &nx::vms::client::core::ChunkProvider::loadingMotionChanged,
            this, handleLoadingMotionChanged);

        for (const auto type: {Qn::RecordingContent, Qn::MotionContent})
            handleTimePeriodsUpdated(type);
    }

    emit chunkProviderChanged();
}

void QnTimeline::releaseResources()
{
    for (const auto texture: {d->stripesDarkTexture, d->stripesLightTexture, d->textTexture})
    {
        if (texture)
            delete texture;
    }
}

QSGNode* QnTimeline::updatePaintNode(
    QSGNode* node, QQuickItem::UpdatePaintNodeData* updatePaintNodeData)
{
    Q_UNUSED(updatePaintNodeData)

    if (!d->stripesDarkTexture || !d->stripesLightTexture)
        d->updateStripesTextures();

    if (!d->textTexture)
        d->updateTextHelper();

    d->animateProperties();

    if (!node)
    {
        node = new QSGNode();

        node->appendChildNode(updateTextNode(nullptr));
        node->appendChildNode(updateChunksNode(nullptr));
    }
    else
    {
        updateTextNode(node->childAtIndex(0));
        updateChunksNode(static_cast<QSGGeometryNode*>(node->childAtIndex(1)));
    }

    return node;
}

QColor QnTimeline::textColor() const
{
    return d->textColor;
}

void QnTimeline::setTextColor(const QColor& color)
{
    if (d->textColor == color)
        return;

    d->textColor = color;
    d->updateTextHelper();

    emit textColorChanged();

    update();
}

QFont QnTimeline::font() const
{
    return d->textFont;
}

void QnTimeline::setFont(const QFont& font)
{
    if (d->textFont == font)
        return;

    d->textFont = font;
    d->updateTextHelper();
    emit fontChanged();
}

QColor QnTimeline::activeLiveColorLight() const
{
    return d->activeLiveColorLight;
}

void QnTimeline::setActiveLiveColorLight(const QColor& color)
{
    if (d->activeLiveColorLight == color)
        return;

    d->activeLiveColorLight = color;
    emit activeLiveColorLightChanged();

    d->updateStripesTextures();
    update();
}

QColor QnTimeline::activeLiveColorDark() const
{
    return d->activeLiveColorLight;
}

void QnTimeline::setActiveLiveColorDark(const QColor& color)
{
    if (d->activeLiveColorDark == color)
        return;

    d->activeLiveColorDark = color;
    emit activeLiveColorDarkChanged();

    d->updateStripesTextures();
    update();
}

QColor QnTimeline::inactiveLiveColorLight() const
{
    return d->inactiveLiveColorLight;
}

void QnTimeline::setInactiveLiveColorLight(const QColor& color)
{
    if (d->inactiveLiveColorLight == color)
        return;

    d->inactiveLiveColorLight = color;
    emit inactiveLiveColorLightChanged();

    d->updateStripesTextures();
    update();
}

QColor QnTimeline::inactiveLiveColorDark() const
{
    return d->inactiveLiveColorLight;
}

void QnTimeline::setInactiveLiveColorDark(const QColor& color)
{
    if (d->inactiveLiveColorDark == color)
        return;

    d->inactiveLiveColorDark = color;
    emit inactiveLiveColorDarkChanged();

    d->updateStripesTextures();
    update();
}


QColor QnTimeline::chunkColor() const
{
    return d->chunkColor;
}

void QnTimeline::setChunkColor(const QColor& color)
{
    if (d->chunkColor == color)
        return;

    d->chunkColor = color;

    emit chunkColorChanged();
    update();
}

QColor QnTimeline::loadingChunkColor() const
{
    return d->loadingChunkColor;
}

void QnTimeline::setLoadingChunkColor(const QColor& color)
{
    if (d->loadingChunkColor == color)
        return;

    d->loadingChunkColor = color;
    emit loadingChunkColorChanged();
}

QColor QnTimeline::motionModeChunkColor() const
{
    return d->motionModeChunkColor;
}

void QnTimeline::setMotionModeChunkColor(const QColor& color)
{
    if (d->motionModeChunkColor == color)
        return;

    d->motionModeChunkColor = color;
    emit motionModeChunkColorChanged();
}

QColor QnTimeline::motionColor() const
{
    return d->motionColor;
}

void QnTimeline::setMotionColor(const QColor& color)
{
    if (d->motionColor == color)
        return;

    d->motionColor = color;
    emit motionColorChanged();
}

QColor QnTimeline::motionLoadingColor() const
{
    return d->motionLoadingColor;
}

void QnTimeline::setMotionLoadingColor(const QColor& color)
{
    if (d->motionLoadingColor == color)
        return;

    d->motionLoadingColor = color;
    emit motionLoadingColor();
}

QColor QnTimeline::chunkBarColor() const
{
    return d->chunkBarColor;
}

void QnTimeline::setChunkBarColor(const QColor& color)
{
    if (d->chunkBarColor == color)
        return;

    d->chunkBarColor = color;

    emit chunkBarColorChanged();
    update();
}

int QnTimeline::chunkBarHeight() const
{
    return d->chunkBarHeight;
}

void QnTimeline::setChunkBarHeight(int chunkBarHeight)
{
    if (d->chunkBarHeight == chunkBarHeight)
        return;

    d->chunkBarHeight = chunkBarHeight;

    emit chunkBarHeightChanged();

    d->updateStripesTextures();
    update();
}

int QnTimeline::textY() const
{
    return d->textY;
}

void QnTimeline::setTextY(int textY)
{
    if (d->textY == textY)
        return;

    d->textY = textY;

    emit textYChanged();
    update();
}

qint64 QnTimeline::defaultWindowSize() const
{
    return kDefaultWindowSize;
}

QSGNode* QnTimeline::updateTextNode(QSGNode* rootNode)
{
    QSGGeometryNode* textNode;
    QSGGeometry* textGeometry;
    QSGOpacityNode* textOpacityNode;
    QSGGeometryNode* lowerTextNode;
    QSGGeometry* lowerTextGeometry;
    QSGOpacityNode* lowerTextOpacityNode;

    QSGGeometry* ticksGeometry = nullptr;
    QSGGeometryNode* ticksNode = nullptr;

    if (!rootNode)
    {
        rootNode = new QSGNode();

        textOpacityNode = new QSGOpacityNode();

        textNode = new QSGGeometryNode();

        textGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
        textGeometry->setDrawingMode(GL_TRIANGLES);

        auto textMaterial = new QSGTextureMaterial();
        textMaterial->setTexture(d->textTexture);

        textNode->setGeometry(textGeometry);
        textNode->setFlag(QSGNode::OwnsGeometry);
        textNode->setMaterial(textMaterial);
        textNode->setFlag(QSGNode::OwnsMaterial);

        lowerTextOpacityNode = new QSGOpacityNode();

        lowerTextNode = new QSGGeometryNode();

        lowerTextGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
        lowerTextGeometry->setDrawingMode(GL_TRIANGLES);

        lowerTextNode->setGeometry(lowerTextGeometry);
        lowerTextNode->setFlag(QSGNode::OwnsGeometry);
        lowerTextNode->setMaterial(textMaterial);

        textOpacityNode->appendChildNode(textNode);
        lowerTextOpacityNode->appendChildNode(lowerTextNode);
        rootNode->appendChildNode(textOpacityNode);
        rootNode->appendChildNode(lowerTextOpacityNode);

        if (drawDebugTicks)
        {
            ticksNode = new QSGGeometryNode();

            ticksGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
            ticksGeometry->setDrawingMode(GL_LINES);

            auto ticksMaterial = new QSGFlatColorMaterial();
            ticksMaterial->setColor(d->textColor);

            ticksNode->setGeometry(ticksGeometry);
            ticksNode->setFlag(QSGNode::OwnsMaterial);
            ticksNode->setMaterial(ticksMaterial);
            ticksNode->setFlag(QSGNode::OwnsMaterial);
            rootNode->appendChildNode(ticksNode);
        }
    }
    else
    {
        d->prevZoomIndex = cFloor(d->zoomLevel);
        textOpacityNode = static_cast<QSGOpacityNode*>(rootNode->childAtIndex(0));
        lowerTextOpacityNode = static_cast<QSGOpacityNode*>(rootNode->childAtIndex(1));
        textNode = static_cast<QSGGeometryNode*>(textOpacityNode->childAtIndex(0));
        lowerTextNode = static_cast<QSGGeometryNode*>(lowerTextOpacityNode->childAtIndex(0));

        textGeometry = textNode->geometry();
        lowerTextGeometry = lowerTextNode->geometry();

        auto textMaterial = static_cast<QSGTextureMaterial*>(textNode->material());
        if (textMaterial->texture() != d->textTexture)
        {
            delete textMaterial->texture();
            textMaterial->setTexture(d->textTexture);
        }

        if (drawDebugTicks)
        {
            ticksNode = static_cast<QSGGeometryNode*>(rootNode->childAtIndex(2));
            ticksGeometry = ticksNode->geometry();
            auto ticksMaterial = static_cast<QSGFlatColorMaterial*>(ticksNode->material());
            ticksMaterial->setColor(d->textColor);
        }
    }

    d->targetTextLevel = d->calculateTargetTextLevel();
    if (qAbs(d->targetTextLevel - d->textLevel) > maxZoomLevelDiff)
    {
        d->textLevel = d->targetTextLevel
            + (d->textLevel < d->targetTextLevel ? -maxZoomLevelDiff : maxZoomLevelDiff);
    }

    int textMarkLevel = cFloor(d->textLevel);

    textOpacityNode->setOpacity(qMin(d->textLevel - textMarkLevel, d->textOpacity));
    lowerTextOpacityNode->setOpacity(qMin(1.0 - textOpacityNode->opacity(), d->textOpacity));

    const auto textMarks = d->calculateVisibleTextMarks();

    int textCount = 0;
    int lowerTextCount = 0;

    for (const auto& textMark: textMarks)
    {
        const auto& zoomLevel = d->zoomLevels[textMark.zoomIndex];

        const auto valueSize = zoomLevel.value(textMark.tick).size();
        const auto suffixSize = zoomLevel.suffix(textMark.tick).isEmpty() ? 0 : 1;

        lowerTextCount += valueSize;
        lowerTextCount += suffixSize;

        if (textMark.zoomIndex > textMarkLevel)
        {
            textCount += valueSize;
            textCount += suffixSize;
        }
    }

    // Update text.
    textGeometry->allocate(textCount * 6);
    auto textPoints = textGeometry->vertexDataAsTexturedPoint2D();
    lowerTextGeometry->allocate(lowerTextCount * 6);
    auto lowerTextPoints = lowerTextGeometry->vertexDataAsTexturedPoint2D();

    QSGGeometry::Point2D* tickPoints = nullptr;
    if (drawDebugTicks)
    {
        ticksGeometry->allocate(textMarks.size() * 2);
        tickPoints = ticksGeometry->vertexDataAsPoint2D();
        ticksNode->markDirty(QSGNode::DirtyGeometry);
    }

    constexpr int kLineWidth = 1;
    constexpr int kLineHeight = 3;
    constexpr qreal kCorrection = kLineWidth / 2.0;

    for (const auto& textMark: textMarks)
    {
        lowerTextPoints += d->placeText(textMark, textMark.zoomIndex, lowerTextPoints);
        if (textMark.zoomIndex > textMarkLevel)
            textPoints += d->placeText(textMark, textMark.zoomIndex, textPoints);

        if (drawDebugTicks)
        {
            tickPoints[0].set(textMark.x + kCorrection, kCorrection);
            tickPoints[1].set(textMark.x + kCorrection, kLineHeight + kCorrection);
            tickPoints += 2;
        }
    }

    textNode->markDirty(QSGNode::DirtyGeometry);
    lowerTextNode->markDirty(QSGNode::DirtyGeometry);

    return rootNode;
}

QSGGeometryNode* QnTimeline::updateChunksNode(QSGGeometryNode* chunksNode)
{
    QSGGeometry* geometry;
    QSGGeometry* stripesGeometry;

    QSGNode* stripesNode;
    QSGOpacityNode* stripesOpacityNode;

    QSGGeometryNode* lightStripesNode;
    QSGOpacityNode* lightStripesOpacityNode;

    QSGGeometryNode* darkStripesNode;
    QSGOpacityNode* darkStripesOpacityNode;

    const auto createStripesNode =
        [](QSGGeometry* geometry, QSGTexture* texture, bool ownsGeometry = false)
        {
            const auto material = new QSGTextureMaterial();
            material->setTexture(texture);
            material->setHorizontalWrapMode(QSGTexture::Repeat);

            const auto node = new QSGGeometryNode();
            node->setGeometry(geometry);
            if (ownsGeometry)
                node->setFlag(QSGNode::OwnsGeometry);
            node->setMaterial(material);
            node->setFlag(QSGNode::OwnsMaterial);

            return node;
        };

    const auto tryUpdateMaterial =
        [](QSGGeometryNode* node, QSGTexture* texture)
        {
            const auto material = static_cast<QSGTextureMaterial*>(node->material());
            const auto lastTexture = material->texture();
            if (lastTexture == texture)
                return;

            delete lastTexture;
            material->setTexture(texture);
        };

    if (!chunksNode)
    {
        chunksNode = new QSGGeometryNode();

        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(GL_TRIANGLES);
        chunksNode->setGeometry(geometry);
        chunksNode->setFlag(QSGNode::OwnsGeometry);

        const auto material = new QSGVertexColorMaterial();
        chunksNode->setMaterial(material);
        chunksNode->setFlag(QSGNode::OwnsMaterial);

        stripesGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        stripesGeometry->setDrawingMode(GL_TRIANGLE_STRIP);

        stripesOpacityNode = new QSGOpacityNode();

        stripesNode = new QSGNode();

        lightStripesOpacityNode = new QSGOpacityNode();
        lightStripesNode = createStripesNode(stripesGeometry, d->stripesLightTexture, true);
        lightStripesOpacityNode->appendChildNode(lightStripesNode);

        darkStripesOpacityNode = new QSGOpacityNode();
        darkStripesNode = createStripesNode(stripesGeometry, d->stripesDarkTexture);
        darkStripesOpacityNode->appendChildNode(darkStripesNode);

        chunksNode->appendChildNode(stripesOpacityNode);
        stripesOpacityNode->appendChildNode(stripesNode);
        stripesNode->appendChildNode(darkStripesOpacityNode);
        stripesNode->appendChildNode(lightStripesOpacityNode);
    }
    else
    {
        geometry = chunksNode->geometry();

        stripesOpacityNode = static_cast<QSGOpacityNode*>(chunksNode->childAtIndex(0));
        stripesNode = static_cast<QSGGeometryNode*>(stripesOpacityNode->childAtIndex(0));

        darkStripesOpacityNode = static_cast<QSGOpacityNode*>(stripesNode->childAtIndex(0));
        darkStripesNode = static_cast<QSGGeometryNode*>(darkStripesOpacityNode->childAtIndex(0));
        lightStripesOpacityNode = static_cast<QSGOpacityNode*>(stripesNode->childAtIndex(1));
        lightStripesNode = static_cast<QSGGeometryNode*>(lightStripesOpacityNode->childAtIndex(0));

        stripesGeometry = lightStripesNode->geometry();

        tryUpdateMaterial(darkStripesNode, d->stripesDarkTexture);
        tryUpdateMaterial(lightStripesNode, d->stripesLightTexture);
    }

    qint64 liveMs = QDateTime::currentMSecsSinceEpoch();

    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();

    const qint64 kRightChunksBound = qMin(maximumValue, liveMs - d->rightChunksOffsetMs);

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location.
     * It makes transparent time slider look better. */

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodContentCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodContentCount];
    int chunkCount = 0;
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
    {
         pos[i] = d->timePeriods[i].findNearestPeriod(minimumValue, true);
         end[i] = d->timePeriods[i].findNearestPeriod(maximumValue, true);
         if (end[i] != d->timePeriods[i].end() && end[i]->contains(maximumValue))
             end[i]++;

         chunkCount += std::distance(pos[i], end[i]);
    }

    qint64 value = minimumValue;
    bool inside[Qn::TimePeriodContentCount];
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        inside[i] = pos[i] == end[i] ? false : pos[i]->contains(value);

    qreal y = height() - d->chunkBarHeight;

    qreal liveX = qMin(width(), d->timeToPixelPos(liveMs));

    QnTimelineChunkPainter chunkPainter(geometry);
    auto colors = chunkPainter.colors();

    colors[Qn::RecordingContent] =
        [this]()
        {
            if (!d->motionSearchMode)
                return d->chunkColor;

            return d->loadingState && d->highlightArchiveChunks
                ? d->loadingChunkColor
                : d->motionModeChunkColor;
        }();

    colors[Qn::MotionContent] =
        [this]
        {
            if (d->motionSearchMode)
                return d->loadingState ? d->motionLoadingColor : d->motionColor;

            return d->chunkColor;
        }();

    colors[Qn::TimePeriodContentCount] = d->hasArchive() ? d->chunkBarColor : Qt::transparent;

    chunkPainter.setColors(colors);
    chunkPainter.start(
        value, QRectF(0, y, std::max<qreal>(liveX - 1, 0), height() - y),
        chunkCount, minimumValue, std::min(liveMs, maximumValue));

    while (value < kRightChunksBound)
    {
        qint64 nextValue[Qn::TimePeriodContentCount] = { maximumValue, maximumValue };
        for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        {
            if (pos[i] == end[i])
                continue;

            if (pos[i] != d->timePeriods[i].begin())
            {
                const auto prev = pos[i] - 1;
                if (pos[i]->startTimeMs < prev->startTimeMs)
                {
                    // TODO: Check Time periods merging and prevent this warning from appearing.
                    qWarning()
                        << "Invalid period order!"
                        << "Current is:" << pos[i]->startTimeMs << pos[i]->endTimeMs()
                        << "Previous was:" << prev->startTimeMs << prev->endTimeMs();
                    pos[i]++;
                    inside[i] = pos[i] != end[i];
                    continue;
                }
            }

            if (!inside[i])
            {
                nextValue[i] = qMin(maximumValue, pos[i]->startTimeMs);
                continue;
            }

            if (!pos[i]->isInfinite())
                nextValue[i] = qMin(maximumValue, pos[i]->endTimeMs());
        }

        qint64 bestValue = qMin(nextValue[Qn::RecordingContent], nextValue[Qn::MotionContent]);

        Qn::TimePeriodContent content;
        if (inside[Qn::MotionContent])
            content = Qn::MotionContent;
        else if (inside[Qn::RecordingContent])
            content = Qn::RecordingContent;
        else
            content = Qn::TimePeriodContentCount;

        if (bestValue > kRightChunksBound)
            bestValue = kRightChunksBound;

        chunkPainter.paintChunk(bestValue - value, content);

        for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        {
            if (bestValue != nextValue[i])
                continue;

            if (inside[i])
                pos[i]++;
            inside[i] = !inside[i];
        }

        value = bestValue;
    }

    if (value < maximumValue)
        chunkPainter.paintChunk(maximumValue - value, Qn::TimePeriodContentCount);

    chunkPainter.stop();

    chunksNode->markDirty(QSGNode::DirtyGeometry);

    stripesOpacityNode->setOpacity(d->liveOpacity);
    lightStripesOpacityNode->setOpacity(d->activeLiveOpacity);
    darkStripesOpacityNode->setOpacity(d->activeLiveOpacity ? 0 : 1);

    qreal textureX = (width() - liveX) / d->chunkBarHeight;
    const auto stripesPoints = stripesGeometry->vertexDataAsTexturedPoint2D();
    stripesPoints[0].set(liveX, y, d->stripesPosition, 0);
    stripesPoints[1].set(width(), y, textureX + d->stripesPosition, 0.0);
    stripesPoints[2].set(liveX, height(), d->stripesPosition, 1.0);
    stripesPoints[3].set(width(), height(), textureX + d->stripesPosition, 1.0);

    lightStripesNode->markDirty(QSGNode::DirtyGeometry);
    darkStripesNode->markDirty(QSGNode::DirtyGeometry);

    return chunksNode;
}

void QnTimelinePrivate::updateTextHelper()
{
    auto window = parent->window();
    if (!window)
        return;

    QStringList suffixList{"ms", "s", ":", "AM", "PM"};

    static constexpr int kMonthsCount = 12;
    for (int i = 1; i <= kMonthsCount; ++i)
    {
        suffixList.append(locale.standaloneMonthName(i, QLocale::ShortFormat));
        suffixList.append(locale.monthName(i, QLocale::ShortFormat));
    }

    textHelper.reset(new QnTimelineTextHelper(textFont, textColor, suffixList));
    textTexture = window->createTextureFromImage(textHelper->texture());

    // Updates text lengths.
    const QFontMetricsF fm(parent->font());
    maxZoomLevelTextLength.resize(zoomLevels.size());
    for (int i = 0; i < zoomLevels.size(); ++i)
    {
        const auto &level = zoomLevels[i];

        if (level.type == QnTimelineZoomLevel::Days || level.type == QnTimelineZoomLevel::Months)
        {
            maxZoomLevelTextLength[i] = 0;
            for (int month = 1; month <= 12; ++month)
            {
                maxZoomLevelTextLength[i] = qMax(
                    maxZoomLevelTextLength[i],
                    fm.size(0, level.type == QnTimelineZoomLevel::Days
                        ? locale.monthName(month, QLocale::ShortFormat)
                        : locale.standaloneMonthName(month, QLocale::ShortFormat)).width());
            }

            if (level.type == QnTimelineZoomLevel::Days)
                maxZoomLevelTextLength[i] += fm.size(0, " 88").width();
        }
        else
        {
            maxZoomLevelTextLength[i] = fm.size(0, level.longestText()).width();
        }
    }
}

void QnTimelinePrivate::updateStripesTextures()
{
    auto window = parent->window();
    if (!window)
        return;

    const auto stripesDark = makeStripesImage(
        chunkBarHeight, inactiveLiveColorLight, inactiveLiveColorDark);

    const auto activeLightColor = loadingState && highlightArchiveChunks
        ? motionLoadingLiveColorLight
        : activeLiveColorLight;
    const auto activeDarkColor = loadingState && highlightArchiveChunks
        ? motionLoadingLiveColorDark
        : activeLiveColorDark;
    const auto stripesLight = makeStripesImage(chunkBarHeight, activeLightColor, activeDarkColor);

    const auto flags = QQuickWindow::TextureHasAlphaChannel;
    stripesDarkTexture = window->createTextureFromImage(stripesDark, flags);
    stripesLightTexture = window->createTextureFromImage(stripesLight, flags);
}

qint64 QnTimelinePrivate::startBoundTimeValue() const
{
    const auto liveTime = QDateTime::currentMSecsSinceEpoch();
    const auto startBound = startBoundTime == -1
        ? liveTime - kDefaultWindowSize / 2
        : startBoundTime;
    const auto initialWindowStart = initialWindowStartTime > 0
        ? initialWindowStartTime
        : startBound;

    return std::min(startBound, initialWindowStart);
}

qint64 QnTimelinePrivate::endBoundTimeValue() const
{
    const auto liveTime = QDateTime::currentMSecsSinceEpoch();
    return endBoundTime == -1
        ? liveTime
        : endBoundTime;
}

void QnTimelinePrivate::tryFitInBounds()
{
    const auto startBound = startBoundTimeValue();
    const auto endBound = endBoundTimeValue();

    const qint64 position = parent->position();
    const qint64 minimalHalfWindowSize = endBound - startBound;
    const qint64 minimalStartBound = position - minimalHalfWindowSize;
    if (windowStart >= minimalStartBound)
        return;

    const qint64 maximalEndBound = position + minimalHalfWindowSize;
    setWindow(minimalStartBound, maximalEndBound);
}

void QnTimelinePrivate::animateProperties()
{
    qint64 dt = kMinimumAnimationTickMs;

    {
        auto time = animationTimer.elapsed();
        if (prevAnimationMs > 0)
            dt = time - prevAnimationMs;
        prevAnimationMs = time;
    }

    const auto originalWindowStart = windowStart;
    const auto originalWindowEnd = windowEnd;
    const auto originalWindowPosition =
        originalWindowStart + (originalWindowEnd - originalWindowStart) / 2;

    bool updateRequired = false;

    const qint64 liveTime = QDateTime::currentMSecsSinceEpoch();
    const qint64 startBound = startBoundTimeValue();
    const qint64 endBound = endBoundTimeValue();

    const bool zoomWasStarted = !zoomKineticHelper.isStopped();
    zoomKineticHelper.update();
    const bool zoomJustStopped = zoomWasStarted && zoomKineticHelper.isStopped();
    if (zoomJustStopped)
        tryFitInBounds();

    const auto oldWindowSize = windowEnd - windowStart;
    if (!zoomKineticHelper.isStopped())
    {
        qint64 maxSize = (liveTime - startBound) * 16;
        qint64 minSize = startBoundTime == -1 ? maxSize : parent->width();
        qreal factor = startZoom / zoomKineticHelper.value();
        qreal windowSize = qBound<qreal>(minSize, startWindowSize * factor, maxSize);

        factor = windowSize / (windowEnd - windowStart);

        qint64 position = parent->position();
        windowStart = position - (position - windowStart) * factor;
        windowEnd = windowStart + static_cast<qint64>(windowSize);

        updateZoomLevel();
    }

    bool justStopped = stickyPointKineticHelper.isStopped();
    stickyPointKineticHelper.update();
    justStopped = !justStopped && stickyPointKineticHelper.isStopped();

    if (stickyPointKineticHelper.isStopped())
    {
        if (justStopped)
            setStickToEnd(qMax(startBound, parent->position()) >= endBound);

        if (stickToEnd)
            targetPosition = liveTime;

        auto position = parent->position();

        auto calculateNewPosition = [&](qint64 pos, qint64 targetPos)
            {
                const auto windowSize = windowEnd - windowStart;
                auto distance = std::min(windowSize, std::abs(pos - targetPos));

                auto multiplier = std::max(0.003, std::pow(
                    std::min(1.0, static_cast<qreal>(distance) / windowSize + 0.15), 2.0));
                auto delta = static_cast<qint64>(windowSize * windowMovingSpeed * dt * multiplier);
                distance = std::max<qint64>(0, distance - delta);

                if (pos < targetPos)
                    pos = targetPos - distance;
                else
                    pos = targetPos + distance;
                return pos;
            };

        if (targetPosition != -1)
        {
            if (position != targetPosition)
                position = calculateNewPosition(position, targetPosition);
            else
                targetPosition = -1;
        }
        else
        {
            if (autoReturnToBounds)
            {
                if (position < startBound)
                    position = calculateNewPosition(position, startBound);
                else if (position > endBound)
                    position = calculateNewPosition(position, endBound);
            }
            else
            {
                if (endBoundTime == -1 && position > endBound)
                    position = calculateNewPosition(position, endBound);
            }
        }

        const auto delta = position - parent->position();
        if (delta != 0)
        {
            windowStart += delta;
            windowEnd += delta;
        }
    }
    else
    {
        qint64 time = pixelPosToTime(stickyPointKineticHelper.value());
        qint64 delta = stickyTime - time;
        windowStart += delta;
        windowEnd += delta;

        time = parent->position();
        if (time >= endBound && delta > 0)
            setStickToEnd(true);
        else if (time < endBound && delta < 0)
            setStickToEnd(false);

        if (!stickyPointKineticHelper.isMeasuring() && time > endBound + (windowEnd - windowStart))
        {
            stickyPointKineticHelper.stop();
            parent->moveFinished();
        }
    }

    if (!qFuzzyEquals(zoomLevel, targetZoomLevel))
    {
        if (targetZoomLevel > zoomLevel)
        {
            if (targetZoomLevel - zoomLevel > maxZoomLevelDiff)
                zoomLevel = targetZoomLevel - maxZoomLevelDiff;
            else
                zoomLevel = qMin(targetZoomLevel, zoomLevel + zoomAnimationSpeed * dt);
        } else {
            if (zoomLevel - targetZoomLevel > maxZoomLevelDiff)
                zoomLevel = targetZoomLevel + maxZoomLevelDiff;
            else
                zoomLevel = qMax(targetZoomLevel, zoomLevel - zoomAnimationSpeed * dt);
        }

        updateRequired = true;
    }

    if (!qFuzzyEquals(textLevel, targetTextLevel))
    {
        if (targetTextLevel > textLevel) {
            if (targetTextLevel - textLevel > maxZoomLevelDiff)
                textLevel = targetTextLevel - maxZoomLevelDiff;
            else
                textLevel = qMin(targetTextLevel, textLevel + textOpacityAnimationSpeed * dt);
        }
        else
        {
            if (textLevel - targetTextLevel > maxZoomLevelDiff)
                textLevel = targetTextLevel + maxZoomLevelDiff;
            else
                textLevel = qMax(targetTextLevel, textLevel - textOpacityAnimationSpeed * dt);
        }

        updateRequired = true;
    }

    bool windowUpdateRequired = false;
    if (originalWindowStart != windowStart)
    {
        emit parent->windowStartChanged();
        windowUpdateRequired = true;
    }
    if (originalWindowEnd != windowEnd)
    {
        emit parent->windowEndChanged();
        windowUpdateRequired = true;
    }
    if (originalWindowPosition != parent->position())
    {
        emit parent->positionChanged();
        windowUpdateRequired = true;
    }
    if (windowEnd - windowStart != oldWindowSize)
        emit parent->windowSizeChanged();

    windowUpdateRequired = (windowUpdateRequired && startBoundTime >= 0);
    updateRequired = (updateRequired || windowUpdateRequired);

    qreal targetTextOpacity = hasArchive() ? 1.0 : 0.0;
    if (!qFuzzyEquals(textOpacity, targetTextOpacity))
    {
        if (textOpacity < targetTextOpacity)
            textOpacity = qMin(targetTextOpacity, textOpacity + textOpacityAnimationSpeed * dt);
        else
            textOpacity = qMax(targetTextOpacity, textOpacity - textOpacityAnimationSpeed * dt);

        updateRequired = true;
    }

    bool live = parent->position() + 1000 >= QDateTime::currentMSecsSinceEpoch();

    qreal targetLiveOpacity = hasArchive() ? 1.0 : 0.0;
    if (!qFuzzyEquals(liveOpacity, targetLiveOpacity))
    {
        if (liveOpacity < targetLiveOpacity)
            liveOpacity = qMin(targetLiveOpacity, liveOpacity + liveOpacityAnimationSpeed * dt);
        else
            liveOpacity = qMax(targetLiveOpacity, liveOpacity - liveOpacityAnimationSpeed * dt);

        updateRequired = true;
    }

    qreal targetActiveLiveOpacity = live ? 1.0 : 0.0;
    if (!qFuzzyEquals(activeLiveOpacity, targetActiveLiveOpacity))
    {
        if (activeLiveOpacity < targetActiveLiveOpacity)
        {
            activeLiveOpacity = qMin(
                targetActiveLiveOpacity, activeLiveOpacity + liveOpacityAnimationSpeed * dt);
        }
        else
        {
            activeLiveOpacity = qMax(
                targetActiveLiveOpacity, activeLiveOpacity - liveOpacityAnimationSpeed * dt);
        }

        updateRequired = true;
    }

    if (live)
    {
        stripesPosition += dt * stripesMovingSpeed;
        while (stripesPosition > 1.0)
            stripesPosition -= 1.0;

        updateRequired = true;
    }

    if (justStopped)
        parent->moveFinished();

    if (updateRequired)
        parent->update();
    else
        prevAnimationMs = -1;
}

void QnTimelinePrivate::zoomWindow(qreal factor)
{
    qreal speed = qSqrt(
        2 * zoomKineticHelper.deceleration() * parent->width() * qAbs(factor - 1));
    if (factor < 1)
        speed = -speed;

    startZoom = parent->width();
    startWindowSize = windowEnd - windowStart;
    zoomKineticHelper.flick(startZoom, speed);

    parent->update();
}

void QnTimelinePrivate::setStickToEnd(bool stickToEnd)
{
    if (this->stickToEnd == stickToEnd)
        return;

    this->stickToEnd = stickToEnd;

    parent->stickToEndChanged();
    parent->update();
}

int QnTimelinePrivate::calculateTargetTextLevel() const
{
    NX_ASSERT(maxZoomLevelTextLength.size() == zoomLevels.size());

    const int zoomIndex = cFloor(zoomLevel);
    const auto windowSize = windowEnd - windowStart;
    const auto width = parent->width();

    for (int textMarkLevel = zoomIndex; textMarkLevel < zoomLevels.size() - 1; ++textMarkLevel)
    {
        const auto& currentLevel = zoomLevels[textMarkLevel];
        const auto& upperLevel = zoomLevels[textMarkLevel + 1];

        const auto currentLevelMaxTextWidth = maxZoomLevelTextLength[textMarkLevel];
        const auto upperLevelMaxTextWidth = maxZoomLevelTextLength[textMarkLevel + 1];

        const auto currentLevelTickIntervalMs = currentLevel.averageTickLength();
        const auto upperLevelTickIntervalMs = upperLevel.averageTickLength();

        const int ticksBetweenTwoUpperLevelTicks =
            static_cast<int>(upperLevelTickIntervalMs / currentLevelTickIntervalMs - 1);

        const auto upperLevelTickLength = width / windowSize * upperLevelTickIntervalMs;

        const auto freeSpace = upperLevelTickLength - upperLevelMaxTextWidth - kTextSpacing
            - (kTextSpacing + currentLevelMaxTextWidth) * ticksBetweenTwoUpperLevelTicks;

        if (freeSpace > 0)
            return textMarkLevel;
    }

    return zoomLevels.size() - 2;
}

QVector<TextMarkInfo> QnTimelinePrivate::calculateVisibleTextMarks() const
{
    const auto& zoomLevel = zoomLevels[cFloor(this->zoomLevel)];
    const auto textMarkLevel = cFloor(textLevel);

    const auto windowSize = windowEnd - windowStart;
    const auto windowStartAligned = zoomLevel.alignTick(adjustTime(windowStart));
    const auto windowEndAligned = zoomLevel.alignTick(adjustTime(windowEnd));
    const auto windowSizeAligned = windowEndAligned - windowStartAligned;
    const auto width = parent->width();

    const auto fakeWidth = width / windowSize * windowSizeAligned;

    auto tickCount = zoomLevel.tickCount(windowStartAligned, windowEndAligned);
    const auto xStep = fakeWidth / tickCount;
    tickCount = width / xStep + extraTickCount;

    QVector<TextMarkInfo> result;
    auto tick = windowStartAligned;
    auto xStart = timeToPixelPos(tick + (windowStart - adjustTime(windowStart)));

    for (int i = 0; i < tickCount; ++i, tick = zoomLevel.nextTick(tick))
    {
        const auto tickLevel = maximumTickLevel(tick);

        const auto x = zoomLevel.isMonotonic()
            ? xStart + xStep * i
            : xStart + (tick - windowStartAligned) * width / windowSize;

        if (tickLevel >= textMarkLevel)
        {
            const TextMarkInfo textMarkInfo{tick, x, tickLevel};

            if (!result.isEmpty())
            {
                const auto previousTickLevel = result.last().zoomIndex;

                // Remove ticks overlapping with ticks with higher zoom level.
                if (previousTickLevel != tickLevel
                    && result.last().x + maxZoomLevelTextLength[previousTickLevel] / 2
                        > x - maxZoomLevelTextLength[tickLevel] / 2)
                {
                    if (previousTickLevel < tickLevel)
                        result.last() = textMarkInfo;
                    // else skip this tick

                    continue;
                }
            }

            result.append(textMarkInfo);
        }
    }

    return result;
}

QSize QnTimelinePrivate::placeSymbol(
    qreal x,
    qreal y,
    QChar symbol,
    QSGGeometry::TexturedPoint2D* points)
{
    const auto size = textHelper->stringSize(symbol);
    const qreal height = size.height();
    const qreal width = size.width();

    const auto coord = textHelper->symbolCoordinates(symbol);
    const qreal top = coord.top();
    const qreal bottom = coord.bottom();
    const qreal left = coord.left();
    const qreal right = coord.right();

    points[0].set(x, y, left, top);
    points[1].set(x + width, y, right, top);
    points[2].set(x + width, y + height, right, bottom);
    points[3] = points[0];
    points[4] = points[2];
    points[5].set(x, y + height, left, bottom);
    return size;
}

int QnTimelinePrivate::placeText(
    const TextMarkInfo& textMark, int textLevel, QSGGeometry::TexturedPoint2D* points)
{
    const auto value = zoomLevels[textLevel].value(textMark.tick);
    const auto suffix = zoomLevels[textLevel].suffix(textMark.tick);
    const auto suffixSize = textHelper->stringSize(suffix);
    const auto suffixWidth = suffixSize.width();

    qreal textWidth = textHelper->stringSize(value).width();
    if (suffixWidth)
        textWidth += suffixWidth;

    qreal x = qFloor(textMark.x - textWidth / 2);
    qreal y = textY >= 0
        ? textY
        : qFloor((parent->height() - chunkBarHeight - textHelper->lineHeight()) / 2);
    int shift = 0;

    for (QChar symbol: value)
    {
        const auto symbolWidth = placeSymbol(x, y, symbol, points).width();
        points += 6;
        shift += 6;
        x += symbolWidth;
    }

    if (!suffix.isEmpty())
    {
        x += textHelper->spaceWidth();

        const auto coord = textHelper->stringCoordinates(suffix);
        points[0].set(x, y, coord.left(), coord.top());
        points[1].set(x + suffixWidth, y, coord.right(), coord.top());
        points[2].set(x + suffixWidth, y + suffixSize.height(),
            coord.right(), coord.bottom());
        points[3] = points[0];
        points[4] = points[2];
        points[5].set(x, y + suffixSize.height(), coord.left(), coord.bottom());
        points += 6;
        shift += 6;
        x += suffixWidth;
    }

    return shift;
}

bool QnTimelinePrivate::hasArchive() const
{
    return startBoundTime > 0;
}

void QnTimelinePrivate::setWindow(qint64 start, qint64 end)
{
    if (start == windowStart && end == windowEnd)
        return;

    zoomKineticHelper.stop();
    stickyPointKineticHelper.stop();
    targetPosition = -1;

    const auto oldPosition = parent->position();

    windowStart = start;
    windowEnd = end;
    updateZoomLevel();

    emit parent->windowStartChanged();
    emit parent->windowEndChanged();
    emit parent->windowSizeChanged();

    if (oldPosition != parent->position())
        emit parent->positionChanged();

    parent->update();
}
