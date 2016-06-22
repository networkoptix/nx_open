#include "timeline.h"

#include <array>

#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGGeometry>
#include <QtQuick/QSGFlatColorMaterial>
#include <QtQuick/QSGVertexColorMaterial>
#include <QtQuick/QSGTextureMaterial>
#include <QtQuick/QQuickWindow>
#include <QtCore/qmath.h>
#include <QtCore/QElapsedTimer>

#include "timeline_text_helper.h"
#include "timeline_zoom_level.h"
#include "timeline_chunk_painter.h"
#include "kinetic_helper.h"
#include "camera/camera_chunk_provider.h"

namespace {

    const bool drawDebugTicks = false;

    const int zoomLevelsVisible = 3;
    const qreal maxZoomLevelDiff = 1.0;
    const int minTickDps = 8;
    const int extraTickCount = 10;
    const qreal zoomAnimationSpeed = 0.004;
    const qreal textOpacityAnimationSpeed = 0.004;
    const qreal textMargin = 35;
    const qreal zoomMultiplier = 2.0;
    const qreal liveOpacityAnimationSpeed = 0.01;
    const qreal stripesMovingSpeed = 0.002;
    const qreal windowMovingSpeed = 0.04;
    const qint64 correctionThreshold = 5000;
    const qint64 defaultWindowSize = 90 * 60 * 1000;
    const qint64 kMSecsInMinute = 60 * 1000;

    struct MarkInfo {
        qint64 tick;
        qreal x;
        int zoomIndex;
    };

    int cFloor(qreal x) {
        int result = qFloor(x);
        if (x - result > 0.9)
            ++result;
        return result;
    }

    QImage makeStripesImage(int size, const QColor &color, const QColor &background) {
        if ((size - 1) & size) // not power of 2
            size = qNextPowerOfTwo(size);

        int stripeWidth = size / 2;

        QImage image(size, size, QImage::Format_RGB888);
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);

        QPen pen;
        pen.setWidth(0);
        pen.setColor(Qt::transparent);
        p.setPen(pen);
        p.setBrush(color);

        p.fillRect(image.rect(), background);

        QPolygon polygon;
        polygon.append(QPoint(-stripeWidth, size));
        polygon.append(QPoint(-stripeWidth + size, 0));
        polygon.append(QPoint(size, 0));
        polygon.append(QPoint(0, size));

        while (polygon[0].x() < size) {
            p.drawPolygon(polygon);
            polygon.translate(stripeWidth * 2, 0);
        }

        return image;
    }

} // anonymous namespace

class QnTimelinePrivate {
public:
    QnTimeline *parent;

    QColor textColor;
    QColor chunkBarColor;
    QColor chunkColor;

    int chunkBarHeight;
    int textY;

    bool showLive;
    QSGTexture *stripesDarkTexture;
    QSGTexture *stripesLightTexture;
    qreal activeLiveOpacity;
    qreal liveOpacity;
    qreal stripesPosition;
    bool stickToEnd;
    bool autoPlay;
    qreal autoPlaySpeed;
    qreal playSpeedCorrection;
    qint64 previousCorrectionTime;
    qint64 previousCorrectedPosition;

    qint64 startBoundTime;
    qint64 endBoundTime;
    qint64 targetPosition;

    qint64 windowStart;
    qint64 windowEnd;

    QFont textFont;
    QSGTexture *textTexture;
    QScopedPointer<QnTimelineTextHelper> textHelper;

    int prevZoomIndex;
    qreal zoomLevel;
    qreal targetZoomLevel;
    qreal textLevel;
    qreal targetTextLevel;
    qreal textOpacity;

    qint64 stickyTime;
    QnKineticHelper<qreal> stickyPointKineticHelper;
    qreal startZoom;
    qint64 startWindowSize;
    QnKineticHelper<qreal> zoomKineticHelper;
    bool dragWasInterruptedByZoom;

    QVector<QnTimelineZoomLevel> zoomLevels;

    QnTimePeriodList timePeriods[Qn::TimePeriodContentCount];

    int timeZoneShift;

    QElapsedTimer animationTimer;
    qint64 prevAnimationMs;

    QStringList suffixList;

    QnCameraChunkProvider *chunkProvider;

    QnTimelinePrivate(QnTimeline *parent):
        parent(parent),
        textColor("#727272"),
        chunkBarColor("#223925"),
        chunkColor("#3a911e"),
        showLive(true),
        stripesDarkTexture(0),
        stripesLightTexture(0),
        activeLiveOpacity(0.0),
        liveOpacity(0.0),
        stripesPosition(0.0),
        stickToEnd(false),
        autoPlay(false),
        autoPlaySpeed(1.0),
        playSpeedCorrection(1.0),
        previousCorrectionTime(-1),
        startBoundTime(-1),
        endBoundTime(-1),
        targetPosition(-1),
        windowStart(QDateTime::currentMSecsSinceEpoch() - defaultWindowSize),
        windowEnd(windowStart + defaultWindowSize * 2),
        textTexture(0),
        textLevel(1.0),
        targetTextLevel(1.0),
        textOpacity(1.0),
        dragWasInterruptedByZoom(false),
        timeZoneShift(0),
        chunkProvider(nullptr)
    {
        chunkBarHeight = 48;
        textY = -1;

        const int sec = 1000;
        const int min = 60 * sec;
        const int hour = 60 * min;

        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Milliseconds,    10));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Milliseconds,    50));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Milliseconds,    100));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Milliseconds,    500));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Seconds,         sec));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Seconds,         5 * sec));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Seconds,         10 * sec));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Seconds,         30 * sec));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Minutes,         min));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Minutes,         5 * min));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Minutes,         10 * min));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Minutes,         30 * min));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Hours,           hour));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Hours,           3 * hour));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Hours,           6 * hour));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Hours,           12 * hour));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Days,            1));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Days,            5));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Days,            15));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Months,          1));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Months,          3));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Months,          6));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Years,           1));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Years,           5));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Years,           10));
        targetZoomLevel = zoomLevel = zoomLevels.size() - zoomLevelsVisible;

        prevZoomIndex = -1;

        enum { maxStickyPointSpeed = 15 };

        stickyPointKineticHelper.setMaxSpeed(maxStickyPointSpeed);
        stickyPointKineticHelper.setMinSpeed(-maxStickyPointSpeed);
        zoomKineticHelper.setMinSpeed(-4.0);
        zoomKineticHelper.setMaxSpeed(50);
        zoomKineticHelper.setMinimum(0);

        animationTimer.start();
        prevAnimationMs = 0;
    }

    ~QnTimelinePrivate() {
        if (stripesDarkTexture)
            delete stripesDarkTexture;
        if (stripesLightTexture)
            delete stripesLightTexture;
        if (textTexture)
            delete textTexture;
    }

    void updateZoomLevel() {
        int index = (prevZoomIndex == -1) ? zoomLevels.size() - 1 : qMax(prevZoomIndex, 0);
        int tickCount = zoomLevels[index].tickCount(windowStart, windowEnd);
        qreal width = parent->width();
        qreal tickSize = width / tickCount;

        if (tickSize < minTickDps) {
            while (tickSize < minTickDps && index < zoomLevels.size() - zoomLevelsVisible) {
                ++index;
                tickCount = zoomLevels[index].tickCount(windowStart, windowEnd);
                tickSize = width / tickCount;
            }
        } else {
            while (index > 0) {
                tickCount = zoomLevels[index - 1].tickCount(windowStart, windowEnd);
                tickSize = width / tickCount;
                if (tickSize >= minTickDps)
                    --index;
                else
                    break;
            }
        }

        if (prevZoomIndex == -1) {
            prevZoomIndex = index;
            targetZoomLevel = zoomLevel = index;
        }

        if (prevZoomIndex != index) {
            targetZoomLevel = index;
            parent->update();
        }
    }

    int tickLevel(qint64 tick) const {
        for (int i = zoomLevels.size() - 1; i > 0; --i) {
            if (zoomLevels[i].testTick(tick))
                return i;
        }
        return 0;
    }

    qint64 pixelPosToTime(qreal x) const {
        return windowStart + x / parent->width() * (windowEnd - windowStart);
    }

    qreal timeToPixelPos(qint64 time) const {
        return parent->width() * (static_cast<qreal>(time - windowStart) / (windowEnd - windowStart));
    }

    qint64 stickPoint() const {
        return windowStart;
    }

    int maxSiblingLevel(int zoomLevel) const {
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

    qint64 adjustTime(qint64 time) const {
        return time + timeZoneShift;
    }

    void updateTextHelper() {
        QQuickWindow *window = parent->window();
        if (!window)
            return;

        textHelper.reset(new QnTimelineTextHelper(textFont, textColor, suffixList));
        textTexture = window->createTextureFromImage(textHelper->texture());
    }

    void updateStripesTextures();

    void animateProperties(qint64 dt);
    void zoomWindow(qreal factor);

    void setStickToEnd(bool stickToEnd);

    void placeDigit(qreal x, qreal y, int digit, QSGGeometry::TexturedPoint2D *points);
    int placeText(const MarkInfo &textMark, int textLevel, QSGGeometry::TexturedPoint2D *points);

    bool hasArchive() const;
};

QnTimeline::QnTimeline(QQuickItem *parent) :
    QQuickItem(parent),
    d(new QnTimelinePrivate(this))
{
    setFlag(QQuickItem::ItemHasContents);
    setAcceptedMouseButtons(Qt::LeftButton);

    connect(this, &QnTimeline::positionChanged, this, &QnTimeline::positionDateChanged);

    connect(this, &QnTimeline::widthChanged, this, [this](){ d->updateZoomLevel(); });

    d->suffixList << lit("ms") << lit("s") << lit(":");

    QnTimelineZoomLevel::maxMonthLength = 0;
    QLocale locale;
    for (int i = 1; i <= 12; ++i) {
        QString standaloneMonthName = locale.standaloneMonthName(i);
        QString monthName = locale.monthName(i);
        d->suffixList.append(standaloneMonthName);
        d->suffixList.append(monthName);
        QnTimelineZoomLevel::maxMonthLength = qMax(QnTimelineZoomLevel::maxMonthLength,
                                                   qMax(standaloneMonthName.length(), monthName.length()));
    }

    d->updateTextHelper();
}

QnTimeline::~QnTimeline() {

}

qint64 QnTimeline::windowStart() const {
    return d->windowStart;
}

void QnTimeline::setWindowStart(qint64 windowStart) {
    if (d->windowStart == windowStart)
        return;

    d->windowStart = windowStart;
    d->updateZoomLevel();
    update();
    emit windowStartChanged();
    emit positionChanged();
}

qint64 QnTimeline::windowEnd() const {
    return d->windowEnd;
}

void QnTimeline::setWindowEnd(qint64 windowEnd) {
    if (d->windowEnd == windowEnd)
        return;

    d->windowEnd = windowEnd;
    d->updateZoomLevel();
    update();
    emit windowEndChanged();
    emit positionChanged();
}

void QnTimeline::setWindow(qint64 windowStart, qint64 windowEnd) {
    if (d->windowStart == windowStart && d->windowEnd == windowEnd)
        return;

    d->windowStart = windowStart;
    d->windowEnd = windowEnd;
    d->updateZoomLevel();
    update();
    emit windowStartChanged();
    emit windowEndChanged();
    emit positionChanged();
}

qint64 QnTimeline::position() const {
    return windowStart() + (windowEnd() - windowStart()) / 2;
}

void QnTimeline::setPosition(qint64 position) {
    if (position == this->position() && (d->targetPosition == -1 || d->targetPosition == position))
        return;

    clearCorrection();

    setStickToEnd(position < 0);

    if (!stickToEnd())
        d->targetPosition = qBound(d->startBoundTime, position, d->endBoundTime != -1 ? d->endBoundTime : QDateTime::currentMSecsSinceEpoch());

    update();
}

QDateTime QnTimeline::positionDate() const {
    return QDateTime::fromMSecsSinceEpoch(position(), Qt::UTC);
}

void QnTimeline::setPositionDate(const QDateTime &dateTime) {
    setPosition(dateTime.isValid() ? dateTime.toMSecsSinceEpoch() : -1);
}

bool QnTimeline::stickToEnd() const {
    return d->stickToEnd;
}

void QnTimeline::setStickToEnd(bool stickToEnd) {
    if (d->stickToEnd == stickToEnd)
        return;

    if (stickToEnd) {
        if (!d->stickyPointKineticHelper.isStopped()) {
            d->stickyPointKineticHelper.stop();
            emit moveFinished();
        }
    }

    d->setStickToEnd(stickToEnd);
}

void QnTimeline::setStartBound(qint64 startBound) {
    if (d->startBoundTime == startBound)
        return;

    d->startBoundTime = startBound;

    emit startBoundChanged();

    update();
}

bool QnTimeline::autoPlay() const {
    return d->autoPlay;
}

void QnTimeline::setAutoPlay(bool autoPlay) {
    if (d->autoPlay == autoPlay)
        return;

    d->autoPlay = autoPlay;

    if (d->autoPlay)
        update();
}

int QnTimeline::timeZoneShift() const {
    return d->timeZoneShift;
}

void QnTimeline::setTimeZoneShift(int timeZoneShift) {
    if (d->timeZoneShift == timeZoneShift)
        return;

    d->timeZoneShift = timeZoneShift;

    emit timeZoneShiftChanged();

    update();
}

qint64 QnTimeline::startBound() const {
    return d->startBoundTime;
}

void QnTimeline::zoomIn() {
    d->zoomWindow(zoomMultiplier);
}

void QnTimeline::zoomOut() {
    d->zoomWindow(1.0 / zoomMultiplier);
}

void QnTimeline::startZoom(qreal scale) {
    Q_UNUSED(scale)

    if (!d->stickyPointKineticHelper.isStopped()) {
        d->stickyPointKineticHelper.stop();
        d->dragWasInterruptedByZoom = true;
    }

    d->startWindowSize = windowEnd() - windowStart();
    d->startZoom = width();
    d->zoomKineticHelper.start(d->startZoom);
    update();
}

void QnTimeline::updateZoom(qreal scale) {
    d->zoomKineticHelper.move(d->startZoom * scale);
    update();
}

void QnTimeline::finishZoom(qreal scale) {
    d->zoomKineticHelper.finish(d->startZoom * scale);

    if (d->dragWasInterruptedByZoom) {
        d->dragWasInterruptedByZoom = false;
        emit moveFinished();
    }

    update();
}

void QnTimeline::startDrag(int x) {
    d->zoomKineticHelper.stop();
    d->stickyTime = d->pixelPosToTime(x);
    d->stickyPointKineticHelper.start(x);
}

void QnTimeline::updateDrag(int x) {
    d->stickyPointKineticHelper.move(x);
    update();
}

void QnTimeline::finishDrag(int x) {
    d->stickyPointKineticHelper.finish(x);
    if (d->stickyPointKineticHelper.isStopped())
        emit moveFinished();
    update();
}

void QnTimeline::clearCorrection() {
    d->previousCorrectionTime = -1;
    d->previousCorrectedPosition = -1;
    d->playSpeedCorrection = 1.0;
}

void QnTimeline::correctPosition(qint64 position) {
    if (position < 0) {
        setPosition(position);
        return;
    }

    qint64 time = d->animationTimer.elapsed();
    if (d->previousCorrectionTime >= 0) {
        qint64 dt = time - d->previousCorrectionTime;
        qint64 prognosed = d->previousCorrectedPosition + dt * d->autoPlaySpeed * d->playSpeedCorrection;

        qint64 diff = prognosed - position;

        if (qAbs(diff) > correctionThreshold * d->autoPlaySpeed) {
            setPosition(position);
            return;
        }

        qint64 dp = d->previousCorrectedPosition + (position - d->previousCorrectedPosition) * 2 - prognosed;
        qreal correction = dp / (dt * d->autoPlaySpeed);
        d->playSpeedCorrection = (d->playSpeedCorrection + correction) / 2; /* take middle value to stabilize correction coefficient */
    }

    d->previousCorrectionTime = time;
    d->previousCorrectedPosition = position;
}

qint64 QnTimeline::positionAtX(qreal x) const {
    return d->pixelPosToTime(x);
}

QnCameraChunkProvider *QnTimeline::chunkProvider() const {
    return d->chunkProvider;
}

void QnTimeline::setChunkProvider(QnCameraChunkProvider *chunkProvider) {
    if (d->chunkProvider == chunkProvider)
        return;

    if (d->chunkProvider)
        disconnect(d->chunkProvider, nullptr, this, nullptr);

    d->chunkProvider = chunkProvider;

    if (d->chunkProvider) {
        connect(d->chunkProvider, &QnCameraChunkProvider::timePeriodsUpdated, this, [this]() {
            d->timePeriods[Qn::RecordingContent] = d->chunkProvider->timePeriods();
            update();
        });
    }

    emit chunkProviderChanged();
}

QSGNode *QnTimeline::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *updatePaintNodeData) {
    Q_UNUSED(updatePaintNodeData)

    if (!d->stripesDarkTexture || !d->stripesLightTexture)
        d->updateStripesTextures();

    if (!d->textTexture)
        d->updateTextHelper();

    qint64 time = d->animationTimer.elapsed();

    d->animateProperties(time - d->prevAnimationMs);
    d->prevAnimationMs = time;

    if (!node) {
        node = new QSGNode();

        node->appendChildNode(updateTextNode(nullptr));
        node->appendChildNode(updateChunksNode(nullptr));
    } else {
        updateTextNode(node->childAtIndex(0));
        updateChunksNode(static_cast<QSGGeometryNode*>(node->childAtIndex(1)));
    }

    update();

    return node;
}

QColor QnTimeline::textColor() const {
    return d->textColor;
}

void QnTimeline::setTextColor(const QColor &color) {
    if (d->textColor == color)
        return;

    d->textColor = color;
    d->updateTextHelper();

    emit textColorChanged();

    update();
}

QFont QnTimeline::font() const {
    return d->textFont;
}

void QnTimeline::setFont(const QFont &font) {
    if (d->textFont == font)
        return;

    d->textFont = font;
    d->updateTextHelper();
    emit fontChanged();
}

QColor QnTimeline::chunkColor() const {
    return d->chunkColor;
}

void QnTimeline::setChunkColor(const QColor &color) {
    if (d->chunkColor == color)
        return;

    d->chunkColor = color;

    emit chunkColorChanged();

    d->updateStripesTextures();
    update();
}

QColor QnTimeline::chunkBarColor() const {
    return d->chunkBarColor;
}

void QnTimeline::setChunkBarColor(const QColor &color) {
    if (d->chunkBarColor == color)
        return;

    d->chunkBarColor = color;

    emit chunkBarColorChanged();

    d->updateStripesTextures();
    update();
}

int QnTimeline::chunkBarHeight() const {
    return d->chunkBarHeight;
}

void QnTimeline::setChunkBarHeight(int chunkBarHeight) {
    if (d->chunkBarHeight == chunkBarHeight)
        return;

    d->chunkBarHeight = chunkBarHeight;

    emit chunkBarHeightChanged();

    d->updateStripesTextures();
    update();
}

int QnTimeline::textY() const {
    return d->textY;
}

void QnTimeline::setTextY(int textY) {
    if (d->textY == textY)
        return;

    d->textY = textY;

    emit textYChanged();
    update();
}

QSGNode *QnTimeline::updateTextNode(QSGNode *rootNode) {
    int zoomIndex = cFloor(d->zoomLevel);
    const QnTimelineZoomLevel &zoomLevel = d->zoomLevels[zoomIndex];

    int lineWidth = 1;
    int lineHeight = 3;
    qreal correction = lineWidth / 2.0;

    qint64 windowSize = windowEnd() - windowStart();
    qint64 windowStartAligned = zoomLevel.alignTick(d->adjustTime(windowStart()));
    qint64 windowEndAligned = zoomLevel.alignTick(d->adjustTime(windowEnd()));
    qint64 windowSizeAligned = windowEndAligned - windowStartAligned;

    qreal fakeWidth = width() / windowSize * windowSizeAligned;

    int tickCount = zoomLevel.tickCount(windowStartAligned, windowEndAligned);
    qreal xStep = fakeWidth / tickCount;
    tickCount = width() / xStep + extraTickCount;
    qint64 tick = windowStartAligned;
    qreal xStart = d->timeToPixelPos(tick + (windowStart() - d->adjustTime(windowStart())));

    QSGGeometryNode *textNode;
    QSGGeometry *textGeometry;
    QSGOpacityNode *textOpacityNode;
    QSGGeometryNode *lowerTextNode;
    QSGGeometry *lowerTextGeometry;
    QSGOpacityNode *lowerTextOpacityNode;

    QSGGeometry *ticksGeometry = nullptr;
    QSGGeometryNode *ticksNode = nullptr;

    if (!rootNode) {
        rootNode = new QSGNode();

        textOpacityNode = new QSGOpacityNode();

        textNode = new QSGGeometryNode();

        textGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
        textGeometry->setDrawingMode(GL_TRIANGLES);

        QSGTextureMaterial *textMaterial = new QSGTextureMaterial();
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

        if (drawDebugTicks) {
            ticksNode = new QSGGeometryNode();

            ticksGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), tickCount * 2);
            ticksGeometry->setDrawingMode(GL_LINES);

            QSGFlatColorMaterial *ticksMaterial = new QSGFlatColorMaterial();
            ticksMaterial->setColor(d->textColor);

            ticksNode->setGeometry(ticksGeometry);
            ticksNode->setFlag(QSGNode::OwnsMaterial);
            ticksNode->setMaterial(ticksMaterial);
            ticksNode->setFlag(QSGNode::OwnsMaterial);
            rootNode->appendChildNode(ticksNode);
        }
    } else {
        d->prevZoomIndex = zoomIndex;
        textOpacityNode = static_cast<QSGOpacityNode*>(rootNode->childAtIndex(0));
        lowerTextOpacityNode = static_cast<QSGOpacityNode*>(rootNode->childAtIndex(1));
        textNode = static_cast<QSGGeometryNode*>(textOpacityNode->childAtIndex(0));
        lowerTextNode = static_cast<QSGGeometryNode*>(lowerTextOpacityNode->childAtIndex(0));

        textGeometry = textNode->geometry();
        lowerTextGeometry = lowerTextNode->geometry();

        QSGTextureMaterial *textMaterial = static_cast<QSGTextureMaterial*>(textNode->material());
        if (textMaterial->texture() != d->textTexture) {
            delete textMaterial->texture();
            textMaterial->setTexture(d->textTexture);
        }

        if (drawDebugTicks) {
            ticksNode = static_cast<QSGGeometryNode*>(rootNode->childAtIndex(2));
            ticksGeometry = ticksNode->geometry();
            ticksGeometry->allocate(tickCount * 2);
            QSGFlatColorMaterial *ticksMaterial = static_cast<QSGFlatColorMaterial*>(ticksNode->material());
            ticksMaterial->setColor(d->textColor);
        }
    }

    QVector<MarkInfo> markedTicks;

    int textMarkLevel = zoomIndex;

    while (textMarkLevel < d->zoomLevels.size() - 1) {
        int lowerTextWidth = d->zoomLevels[textMarkLevel].maxTextWidth() * d->textHelper->maxCharWidth();
        int preLowerTextWidth = d->zoomLevels[textMarkLevel + 1].maxTextWidth() * d->textHelper->maxCharWidth();
        qint64 preLowerTickInterval = d->zoomLevels[textMarkLevel + 1].nextTick(0);
        int tickCount = preLowerTickInterval / d->zoomLevels[textMarkLevel].nextTick(0);
        qreal preLowerTickWidth = width() * preLowerTickInterval / windowSize;

        qreal pix = preLowerTickWidth - lowerTextWidth * tickCount - preLowerTextWidth;
        if (pix > textMargin) {
            d->targetTextLevel = textMarkLevel;
            if (qAbs(d->targetTextLevel - d->textLevel) > maxZoomLevelDiff)
                d->textLevel = d->targetTextLevel + (d->textLevel < d->targetTextLevel ? -maxZoomLevelDiff : maxZoomLevelDiff);
            break;
        }

        ++textMarkLevel;
    }
    if (textMarkLevel == d->zoomLevels.size() - 1)
        d->targetTextLevel = textMarkLevel - 1;

    textMarkLevel = cFloor(d->textLevel);

    textOpacityNode->setOpacity(qMin(d->textLevel - textMarkLevel, d->textOpacity));
    lowerTextOpacityNode->setOpacity(qMin(1.0 - textOpacityNode->opacity(), d->textOpacity));

    int textCount = 0;
    int lowerTextCount = 0;

    QSGGeometry::Point2D *tickPoints = nullptr;
    if (drawDebugTicks) {
        tickPoints = ticksGeometry->vertexDataAsPoint2D();
        ticksNode->markDirty(QSGNode::DirtyGeometry);
    }

    for (int i = 0; i < tickCount; ++i) {
        int tickLevel = d->tickLevel(tick);

        qreal x;
        if (zoomLevel.isMonotonic())
            x = xStart + xStep * i;
        else
            x = xStart + (tick - windowStartAligned) * width() / windowSize;

        if (tickLevel >= textMarkLevel) {
            MarkInfo info;
            info.tick = tick;
            info.x = x;
            info.zoomIndex = tickLevel;
            markedTicks.append(info);

            lowerTextCount += d->zoomLevels[tickLevel].baseValue(tick).size();
            lowerTextCount += d->zoomLevels[tickLevel].subValue(tick).size();
            lowerTextCount += d->zoomLevels[tickLevel].suffix(tick).isEmpty() ? 0 : 1;

            if (tickLevel > textMarkLevel) {
                textCount += d->zoomLevels[tickLevel].baseValue(tick).size();
                textCount += d->zoomLevels[tickLevel].subValue(tick).size();
                textCount += d->zoomLevels[tickLevel].suffix(tick).isEmpty() ? 0 : 1;
            }
        }

        if (drawDebugTicks) {
            tickPoints[0].set(x + correction, correction);
            tickPoints[1].set(x + correction, lineHeight + correction);
            tickPoints += 2;
        }

        tick = zoomLevel.nextTick(tick);
    }

    /* update text */
    textGeometry->allocate(textCount * 6);
    QSGGeometry::TexturedPoint2D *textPoints = textGeometry->vertexDataAsTexturedPoint2D();
    lowerTextGeometry->allocate(lowerTextCount * 6);
    QSGGeometry::TexturedPoint2D *lowerTextPoints = lowerTextGeometry->vertexDataAsTexturedPoint2D();

    for (const MarkInfo &info: markedTicks) {
        lowerTextPoints += d->placeText(info, info.zoomIndex, lowerTextPoints);
        if (info.zoomIndex > textMarkLevel)
            textPoints += d->placeText(info, info.zoomIndex, textPoints);
    }

    textNode->markDirty(QSGNode::DirtyGeometry);
    lowerTextNode->markDirty(QSGNode::DirtyGeometry);

    return rootNode;
}

QSGGeometryNode *QnTimeline::updateChunksNode(QSGGeometryNode *chunksNode) {
    QSGGeometry *geometry;
    QSGGeometry *stripesGeometry;
    QSGOpacityNode *stripesOpacityNode;
    QSGOpacityNode *lightStripesOpacityNode;
    QSGGeometryNode *stripesNode;
    QSGGeometryNode *lightStripesNode;

    if (!chunksNode) {
        chunksNode = new QSGGeometryNode();

        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(GL_TRIANGLES);
        chunksNode->setGeometry(geometry);
        chunksNode->setFlag(QSGNode::OwnsGeometry);

        QSGVertexColorMaterial *material = new QSGVertexColorMaterial();
        chunksNode->setMaterial(material);
        chunksNode->setFlag(QSGNode::OwnsMaterial);

        stripesGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        stripesGeometry->setDrawingMode(GL_TRIANGLE_STRIP);

        QSGTextureMaterial *stripesDarkMaterial = new QSGTextureMaterial();
        stripesDarkMaterial->setTexture(d->stripesDarkTexture);
        stripesDarkMaterial->setHorizontalWrapMode(QSGTexture::Repeat);

        QSGTextureMaterial *stripesLightMaterial = new QSGTextureMaterial();
        stripesLightMaterial->setTexture(d->stripesLightTexture);
        stripesLightMaterial->setHorizontalWrapMode(QSGTexture::Repeat);

        stripesOpacityNode = new QSGOpacityNode();

        stripesNode = new QSGGeometryNode();
        stripesNode->setGeometry(stripesGeometry);
        stripesNode->setFlag(QSGNode::OwnsGeometry);
        stripesNode->setMaterial(stripesDarkMaterial);
        stripesNode->setFlag(QSGNode::OwnsMaterial);

        lightStripesNode = new QSGGeometryNode();
        lightStripesNode->setGeometry(stripesGeometry);
        lightStripesNode->setMaterial(stripesLightMaterial);
        lightStripesNode->setFlag(QSGNode::OwnsMaterial);

        lightStripesOpacityNode = new QSGOpacityNode();

        chunksNode->appendChildNode(stripesOpacityNode);
        stripesOpacityNode->appendChildNode(stripesNode);
        stripesNode->appendChildNode(lightStripesOpacityNode);
        lightStripesOpacityNode->appendChildNode(lightStripesNode);
    } else {
        geometry = chunksNode->geometry();

        stripesOpacityNode = static_cast<QSGOpacityNode*>(chunksNode->childAtIndex(0));
        stripesNode = static_cast<QSGGeometryNode*>(stripesOpacityNode->childAtIndex(0));
        lightStripesOpacityNode = static_cast<QSGOpacityNode*>(stripesNode->childAtIndex(0));
        lightStripesNode = static_cast<QSGGeometryNode*>(lightStripesOpacityNode->childAtIndex(0));
        stripesGeometry = stripesNode->geometry();

        QSGTextureMaterial *material = static_cast<QSGTextureMaterial*>(stripesNode->material());
        if (material->texture() != d->stripesDarkTexture) {
            delete material->texture();
            material->setTexture(d->stripesDarkTexture);
            material = static_cast<QSGTextureMaterial*>(lightStripesNode->material());
            delete material->texture();
            material->setTexture(d->stripesLightTexture);
        }
    }

    qint64 liveMs = QDateTime::currentMSecsSinceEpoch();

    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();
    qint64 kRightChunksBound = qMin(maximumValue, liveMs - kMSecsInMinute);

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location.
     * It makes transparent time slider look better. */

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodContentCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodContentCount];
    int chunkCount = 0;
    for (int i = 0; i < Qn::TimePeriodContentCount; i++) {
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

    QnTimelineChunkPainter chunkPainter(geometry);
    std::array<QColor, Qn::TimePeriodContentCount + 1> colors = chunkPainter.colors();
    colors[Qn::RecordingContent] = d->chunkColor;
    colors[Qn::TimePeriodContentCount] = d->chunkBarColor;
    chunkPainter.setColors(colors);
    chunkPainter.start(value, QRectF(0, y, width(), height() - y),
                       chunkCount, minimumValue, maximumValue);

    while (value < kRightChunksBound) {
        qint64 nextValue[Qn::TimePeriodContentCount] = { maximumValue, maximumValue };
        for (int i = 0; i < Qn::TimePeriodContentCount; i++) {
            if (pos[i] == end[i])
                continue;

            if (pos[i] != d->timePeriods[i].begin()) {
                const auto prev = pos[i] - 1;
                if (pos[i]->startTimeMs < prev->startTimeMs) {
                    // TODO: Check Time periods merging and prevent this warning from appearing.
                    qWarning() << "Invalid period order!"
                               << "Current is:" << pos[i]->startTimeMs << pos[i]->endTimeMs()
                               << "Previous was:" << prev->startTimeMs << prev->endTimeMs();
                    pos[i]++;
                    inside[i] = pos[i] != end[i];
                    continue;
                }
            }

            if (!inside[i]) {
                nextValue[i] = qMin(maximumValue, pos[i]->startTimeMs);
                continue;
            }

            if (!pos[i]->isInfinite())
                nextValue[i] = qMin(maximumValue, pos[i]->endTimeMs());
        }

        qint64 bestValue = qMin(nextValue[Qn::RecordingContent], nextValue[Qn::MotionContent]);

        Qn::TimePeriodContent content;
        if (inside[Qn::MotionContent]) {
            content = Qn::MotionContent;
        } else if (inside[Qn::RecordingContent]) {
            content = Qn::RecordingContent;
        } else {
            content = Qn::TimePeriodContentCount;
        }

        if (bestValue > kRightChunksBound)
            bestValue = kRightChunksBound;

        chunkPainter.paintChunk(bestValue - value, content);

        for (int i = 0; i < Qn::TimePeriodContentCount; i++) {
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

    qreal liveX = qMin(width(), d->timeToPixelPos(liveMs));
    qreal textureX = (width() - liveX) / d->chunkBarHeight;
    QSGGeometry::TexturedPoint2D *stripesPoints = stripesGeometry->vertexDataAsTexturedPoint2D();
    stripesPoints[0].set(liveX, y, d->stripesPosition, 0);
    stripesPoints[1].set(width(), y, textureX + d->stripesPosition, 0.0);
    stripesPoints[2].set(liveX, height(), d->stripesPosition, 1.0);
    stripesPoints[3].set(width(), height(), textureX + d->stripesPosition, 1.0);

    stripesNode->markDirty(QSGNode::DirtyGeometry);
    lightStripesNode->markDirty(QSGNode::DirtyGeometry);

    return chunksNode;
}

void QnTimelinePrivate::updateStripesTextures() {
    QQuickWindow *window = parent->window();
    if (!window)
        return;

    enum { tintAmount = 106 };
    QImage stripesDark = makeStripesImage(chunkBarHeight, chunkBarColor, chunkBarColor.lighter(tintAmount));
    QImage stripesLight = makeStripesImage(chunkBarHeight, chunkColor, chunkColor.darker(tintAmount));

    stripesDarkTexture = parent->window()->createTextureFromImage(stripesDark);
    stripesLightTexture = parent->window()->createTextureFromImage(stripesLight);
}

void QnTimelinePrivate::animateProperties(qint64 dt) {
    const auto originalWindowStart = windowStart;
    const auto originalWindowEnd = windowEnd;
    const auto originalWindowPosition = originalWindowStart + (originalWindowEnd - originalWindowStart) / 2;

    if (!stickToEnd && autoPlay) {
        qint64 shift = dt * autoPlaySpeed * playSpeedCorrection;
        windowStart += shift;
        windowEnd += shift;
    }

    qint64 liveTime = QDateTime::currentMSecsSinceEpoch();

    qint64 startBound = startBoundTime == -1 ? liveTime - defaultWindowSize : startBoundTime;
    qint64 endBound = endBoundTime == -1 ? liveTime : endBoundTime;

    zoomKineticHelper.update();
    if (!zoomKineticHelper.isStopped()) {
        qint64 maxSize = (liveTime - startBound) * 2;
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

    if (stickyPointKineticHelper.isStopped()) {
        qint64 time = parent->position();
        qint64 windowSize = windowEnd - windowStart;
        qint64 delta = windowSize * windowMovingSpeed;

        if (justStopped)
            setStickToEnd(qMax(startBound, parent->position()) >= endBound);

        if (stickToEnd)
            targetPosition = liveTime;

        if (targetPosition != -1) {
            if (time < targetPosition)
                time = qMin(targetPosition, qMax(time, targetPosition - windowSize) + delta);
            else if (time > targetPosition)
                time = qMax(targetPosition, qMin(time, targetPosition + windowSize) - delta);

            if (time == targetPosition)
                targetPosition = -1;
        } else if (time < startBound)
            time = qMin(startBound, qMax(time, startBound - windowSize) + delta);
        else if (time > endBound)
            time = qMax(endBound, qMin(time, endBound + windowSize) - delta);

        delta = time - parent->position();
        if (delta != 0) {
            windowStart += delta;
            windowEnd += delta;
        }
    } else {
        qint64 time = pixelPosToTime(stickyPointKineticHelper.value());
        qint64 delta = stickyTime - time;
        windowStart += delta;
        windowEnd += delta;

        time = parent->position();
        if (time >= endBound && delta > 0)
            setStickToEnd(true);
        else if (time < endBound && delta < 0)
            setStickToEnd(false);

        if (!stickyPointKineticHelper.isMeasuring() && time > endBound + (windowEnd - windowStart)) {
            stickyPointKineticHelper.stop();
            parent->moveFinished();
        }
    }

    if (!qFuzzyCompare(zoomLevel, targetZoomLevel)) {
        if (targetZoomLevel > zoomLevel) {
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
    }

    if (!qFuzzyCompare(textLevel, targetTextLevel)) {
        if (targetTextLevel > textLevel) {
            if (targetTextLevel - textLevel > maxZoomLevelDiff)
                textLevel = targetTextLevel - maxZoomLevelDiff;
            else
                textLevel = qMin(targetTextLevel, textLevel + textOpacityAnimationSpeed * dt);
        } else {
            if (textLevel - targetTextLevel > maxZoomLevelDiff)
                textLevel = targetTextLevel + maxZoomLevelDiff;
            else
                textLevel = qMax(targetTextLevel, textLevel - textOpacityAnimationSpeed * dt);
        }
    }

    if (originalWindowStart != windowStart)
        emit parent->windowStartChanged();
    if (originalWindowEnd != windowEnd)
        emit parent->windowEndChanged();
    if (originalWindowPosition != parent->position())
        emit parent->positionChanged();

    qreal targetTextOpacity = hasArchive() ? 1.0 : 0.0;
    if (!qFuzzyCompare(textOpacity, targetTextOpacity)) {
        if (textOpacity < targetTextOpacity)
            textOpacity = qMin(targetTextOpacity, textOpacity + textOpacityAnimationSpeed * dt);
        else
            textOpacity = qMax(targetTextOpacity, textOpacity - textOpacityAnimationSpeed * dt);
    }

    bool live = parent->position() + 1000 >= QDateTime::currentMSecsSinceEpoch();

    qreal targetLiveOpacity = hasArchive() ? 1.0 : 0.0;
    if (!qFuzzyCompare(liveOpacity, targetLiveOpacity)) {
        if (liveOpacity < targetLiveOpacity)
            liveOpacity = qMin(targetLiveOpacity, liveOpacity + liveOpacityAnimationSpeed * dt);
        else
            liveOpacity = qMax(targetLiveOpacity, liveOpacity - liveOpacityAnimationSpeed * dt);
    }

    qreal targetActiveLiveOpacity = live ? 1.0 : 0.0;
    if (!qFuzzyCompare(activeLiveOpacity, targetActiveLiveOpacity)) {
        if (activeLiveOpacity < targetActiveLiveOpacity)
            activeLiveOpacity = qMin(targetActiveLiveOpacity, activeLiveOpacity + liveOpacityAnimationSpeed * dt);
        else
            activeLiveOpacity = qMax(targetActiveLiveOpacity, activeLiveOpacity - liveOpacityAnimationSpeed * dt);
    }

    if (live) {
        stripesPosition += dt * stripesMovingSpeed;
        while (stripesPosition > 1.0)
            stripesPosition -= 1.0;
    }

    if (justStopped)
        parent->moveFinished();
}

void QnTimelinePrivate::zoomWindow(qreal factor) {
    qreal speed = qSqrt(2 * zoomKineticHelper.deceleration() * parent->width() * qAbs(factor - 1));
    if (factor < 1)
        speed = -speed;

    startZoom = parent->width();
    startWindowSize = windowEnd - windowStart;
    zoomKineticHelper.flick(startZoom, speed);

    parent->update();
}

void QnTimelinePrivate::setStickToEnd(bool stickToEnd) {
    if (this->stickToEnd == stickToEnd)
        return;

    this->stickToEnd = stickToEnd;

    parent->stickToEndChanged();
    parent->update();
}

void QnTimelinePrivate::placeDigit(qreal x, qreal y, int digit, QSGGeometry::TexturedPoint2D *points) {
    QSize digitSize = textHelper->digitSize();

    QRectF texCoord = textHelper->digitCoordinates(digit);
    points[0].set(x, y, texCoord.left(), texCoord.top());
    points[1].set(x + digitSize.width(), y, texCoord.right(), texCoord.top());
    points[2].set(x + digitSize.width(), y + digitSize.height(), texCoord.right(), texCoord.bottom());
    points[3] = points[0];
    points[4] = points[2];
    points[5].set(x, y + digitSize.height(), texCoord.left(), texCoord.bottom());
}

int QnTimelinePrivate::placeText(const MarkInfo &textMark, int textLevel, QSGGeometry::TexturedPoint2D *points) {
    QString baseValue = zoomLevels[textLevel].baseValue(textMark.tick);
    QString subValue = zoomLevels[textLevel].subValue(textMark.tick);
    QString suffix = zoomLevels[textLevel].suffix(textMark.tick);
    QSize suffixSize = textHelper->stringSize(suffix);

    QSize digitSize = textHelper->digitSize();

    qreal tw = digitSize.width() * (baseValue.size() + subValue.size()) + suffixSize.width();
    if (baseValue.isEmpty())
        tw += textHelper->spaceWidth() * 2;

    qreal x = qFloor(textMark.x - tw / 2);
    qreal y = textY >= 0 ? textY : qFloor((parent->height() - chunkBarHeight - textHelper->lineHeight()) / 2);
    int shift = 0;

    for (int i = 0; i < baseValue.size(); ++i) {
        placeDigit(x, y, baseValue.mid(i, 1).toInt(), points);
        points += 6;
        shift += 6;
        x += digitSize.width();
    }

    if (subValue.isEmpty())
        x += textHelper->spaceWidth();

    if (!suffix.isEmpty()) {
        QRectF texCoord = textHelper->stringCoordinates(suffix);
        points[0].set(x, y, texCoord.left(), texCoord.top());
        points[1].set(x + suffixSize.width(), y, texCoord.right(), texCoord.top());
        points[2].set(x + suffixSize.width(), y + suffixSize.height(), texCoord.right(), texCoord.bottom());
        points[3] = points[0];
        points[4] = points[2];
        points[5].set(x, y + suffixSize.height(), texCoord.left(), texCoord.bottom());
        points += 6;
        shift += 6;
        x += suffixSize.width();
    }

    for (int i = 0; i < subValue.size(); ++i) {
        placeDigit(x, y, subValue.mid(i, 1).toInt(), points);
        points += 6;
        shift += 6;
        x += digitSize.width();
    }

    return shift;
}

bool QnTimelinePrivate::hasArchive() const {
    return startBoundTime > 0;
}
