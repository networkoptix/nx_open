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
#include <QDebug>

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
    const qreal textMarginDp = 35;
    const qreal zoomMultiplier = 2.0;
    const qreal liveOpacityAnimationSpeed = 0.01;
    const qreal stripesMovingSpeed = 0.002;
    const qreal windowMovingSpeed = 0.04;
    const qint64 correctionThreshold = 5000;

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
    QColor chunkColor;

    int chunkBarHeight;
    int textY;

    bool showLive;
    QSGTexture *stripesDarkTexture;
    QSGTexture *stripesLightTexture;
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

    qreal dpMultiplier;

    int prevZoomIndex;
    qreal zoomLevel;
    qreal targetZoomLevel;
    qreal textLevel;
    qreal targetTextLevel;

    qint64 stickyTime;
    QnKineticHelper<qreal> stickyPointKineticHelper;
    qreal startZoom;
    qint64 startWindowSize;
    QnKineticHelper<qreal> zoomKineticHelper;

    QVector<QnTimelineZoomLevel> zoomLevels;

    QnTimePeriodList timePeriods[Qn::TimePeriodContentCount];

    qint64 timeZoneShift;

    QElapsedTimer animationTimer;
    qint64 prevAnimationMs;

    QStringList suffixList;

    QnCameraChunkProvider *chunkProvider;

    QnTimelinePrivate(QnTimeline *parent):
        parent(parent),
        textColor("#727272"),
        chunkColor("#2196F3"),
        showLive(true),
        stripesDarkTexture(0),
        stripesLightTexture(0),
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
        textTexture(0),
        textLevel(1.0),
        targetTextLevel(1.0),
        timeZoneShift(0),
        chunkProvider(nullptr)
    {
        dpMultiplier = 2;
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
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Hours,           12 * hour));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Days,            24 * hour));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Months,          1));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Years,           1));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Years,           5));
        zoomLevels.append(QnTimelineZoomLevel(QnTimelineZoomLevel::Years,           10));
        targetZoomLevel = zoomLevel = zoomLevels.size() - zoomLevelsVisible;

        prevZoomIndex = -1;

        stickyPointKineticHelper.setMaxSpeed(50);
        stickyPointKineticHelper.setMinSpeed(-50);
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

    int dp(qreal dpix) {
        return qRound(dpMultiplier * dpix);
    }

    void updateZoomLevel() {
        int index = (prevZoomIndex == -1) ? zoomLevels.size() - 1 : qMax(prevZoomIndex, 0);
        int tickCount = zoomLevels[index].tickCount(windowStart, windowEnd);
        qreal width = parent->width() / dp(1.0);
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
};

QnTimeline::QnTimeline(QQuickItem *parent) :
    QQuickItem(parent),
    d(new QnTimelinePrivate(this))
{
    setFlag(QQuickItem::ItemHasContents);
    setAcceptedMouseButtons(Qt::LeftButton);

    connect(this, &QnTimeline::positionChanged, this, &QnTimeline::positionDateChanged);

    connect(this, &QnTimeline::widthChanged, this, [this](){ d->updateZoomLevel(); });

    QnTimelineZoomLevel::monthsNames << tr("January")   << tr("February")   << tr("March")
                                     << tr("April")     << tr("May")        << tr("June")
                                     << tr("July")      << tr("August")     << tr("September")
                                     << tr("October")   << tr("November")   << tr("December");

    QnTimelineZoomLevel::maxMonthLength = 0;
    for (const QString &month : QnTimelineZoomLevel::monthsNames)
        QnTimelineZoomLevel::maxMonthLength = qMax(QnTimelineZoomLevel::maxMonthLength, month.size());

    d->suffixList << lit("ms") << lit("s") << lit(":");
    for (const QString &month : QnTimelineZoomLevel::monthsNames)
        d->suffixList.append(month);

    QDate date(2015, 1, 1);
    for (int i = 0; i < 12; ++i) {
        d->suffixList.append(date.toString(lit("MMMM")));
        date = date.addMonths(1);
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
    if (position == this->position())
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

    if (stickToEnd)
        d->stickyPointKineticHelper.stop();

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

qint64 QnTimeline::timeZoneShift() const {
    return d->timeZoneShift;
}

void QnTimeline::setTimeZoneShift(qint64 timeZoneShift) {
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
    d->stickyTime = position();
    d->zoomWindow(zoomMultiplier);
}

void QnTimeline::zoomOut() {
    d->stickyTime = position();
    d->zoomWindow(1.0 / zoomMultiplier);
}

void QnTimeline::startPinch(int x, qreal scale) {
    Q_UNUSED(scale)

    d->stickyTime = d->pixelPosToTime(x);
    d->startWindowSize = windowEnd() - windowStart();
    d->startZoom = width();
    d->stickyPointKineticHelper.start(x);
    d->zoomKineticHelper.start(d->startZoom);
    update();
}

void QnTimeline::updatePinch(int x, qreal scale) {
    d->stickyPointKineticHelper.move(x);
    d->zoomKineticHelper.move(d->startZoom * scale);
    update();
}

void QnTimeline::finishPinch(int x, qreal scale) {
    d->zoomKineticHelper.finish(d->startZoom * scale);
    d->stickyPointKineticHelper.finish(x);
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

    int lineWidth = d->dp(1);
    int lineHeight = d->dp(3);
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
        if (pix > d->dp(textMarginDp)) {
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

    textOpacityNode->setOpacity(d->textLevel - textMarkLevel);
    lowerTextOpacityNode->setOpacity(1.0 - textOpacityNode->opacity());

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

            lowerTextCount += d->zoomLevels[textMarkLevel].baseValue(tick).size();
            lowerTextCount += d->zoomLevels[textMarkLevel].subValue(tick).size();
            lowerTextCount += d->zoomLevels[textMarkLevel].suffix(tick).isEmpty() ? 0 : 1;

            if (tickLevel > textMarkLevel) {
                textCount += d->zoomLevels[textMarkLevel + 1].baseValue(tick).size();
                textCount += d->zoomLevels[textMarkLevel + 1].subValue(tick).size();
                textCount += d->zoomLevels[textMarkLevel + 1].suffix(tick).isEmpty() ? 0 : 1;
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
        lowerTextPoints += d->placeText(info, textMarkLevel, lowerTextPoints);
        if (info.zoomIndex > textMarkLevel)
            textPoints += d->placeText(info, textMarkLevel + 1, textPoints);
    }

    textNode->markDirty(QSGNode::DirtyGeometry);
    lowerTextNode->markDirty(QSGNode::DirtyGeometry);

    return rootNode;
}

QSGGeometryNode *QnTimeline::updateChunksNode(QSGGeometryNode *chunksNode) {
    QSGGeometry *geometry;
    QSGGeometry *stripesGeometry;
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

        chunksNode->appendChildNode(stripesNode);
        stripesNode->appendChildNode(lightStripesOpacityNode);
        lightStripesOpacityNode->appendChildNode(lightStripesNode);
    } else {
        geometry = chunksNode->geometry();

        stripesNode = static_cast<QSGGeometryNode*>(chunksNode->childAtIndex(0));
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

    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location.
     * It makes transparent time slider look better. */

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodContentCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodContentCount];
    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
         pos[i] = d->timePeriods[i].findNearestPeriod(minimumValue, true);
         end[i] = d->timePeriods[i].findNearestPeriod(maximumValue, true);
         if(end[i] != d->timePeriods[i].end() && end[i]->contains(maximumValue))
             end[i]++;
    }

    qint64 value = minimumValue;
    bool inside[Qn::TimePeriodContentCount];
    for(int i = 0; i < Qn::TimePeriodContentCount; i++)
        inside[i] = pos[i] == end[i] ? false : pos[i]->contains(value);

    qreal y = height() - d->chunkBarHeight;

    QnTimelineChunkPainter chunkPainter(geometry);
    std::array<QColor, Qn::TimePeriodContentCount + 1> colors = chunkPainter.colors();
    colors[Qn::RecordingContent] = d->chunkColor;
    colors[Qn::TimePeriodContentCount] = d->chunkColor.darker();
    chunkPainter.setColors(colors);
    chunkPainter.start(value, position(), QRectF(0, y, width(), height() - y),
                       qMax(1, d->timePeriods[Qn::RecordingContent].size()),
            minimumValue, maximumValue);

    while (value != maximumValue) {
        qint64 nextValue[Qn::TimePeriodContentCount] = {maximumValue, maximumValue, maximumValue};
        for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
            if(pos[i] == end[i])
                continue;

            if(!inside[i]) {
                nextValue[i] = qMin(maximumValue, pos[i]->startTimeMs);
                continue;
            }

            if(!pos[i]->isInfinite())
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

    chunksNode->markDirty(QSGNode::DirtyGeometry);

    lightStripesOpacityNode->setOpacity(d->liveOpacity);

    qreal liveX = qMin(width(), d->timeToPixelPos(QDateTime::currentMSecsSinceEpoch()));
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

    QImage stripesDark = makeStripesImage(chunkBarHeight, chunkColor.darker(180), chunkColor.darker());
    QImage stripesLight = makeStripesImage(chunkBarHeight, chunkColor, chunkColor.darker(105));

    stripesDarkTexture = parent->window()->createTextureFromImage(stripesDark);
    stripesLightTexture = parent->window()->createTextureFromImage(stripesLight);
}

void QnTimelinePrivate::animateProperties(qint64 dt) {
    bool windowChanged = false;

    if (!stickToEnd && autoPlay) {
        windowChanged = true;
        qint64 shift = dt * autoPlaySpeed * playSpeedCorrection;
        windowStart += shift;
        windowEnd += shift;
    }

    zoomKineticHelper.update();
    if (!zoomKineticHelper.isStopped()) {
        windowChanged = true;

        qint64 maxSize = parent->width() / dp(minTickDps) * (365 / 4 * 24 * 60 * 60 * 1000ll);
        qint64 minSize = parent->width() / dp(1);
        qreal factor = startZoom / zoomKineticHelper.value();
        qreal windowSize = qBound<qreal>(minSize, startWindowSize * factor, maxSize);

        factor = windowSize / (windowEnd - windowStart);

        windowStart = stickyTime - (stickyTime - windowStart) * factor;
        windowEnd = windowStart + static_cast<qint64>(windowSize);

        updateZoomLevel();
    }

    qint64 liveTime = QDateTime::currentMSecsSinceEpoch();

    qint64 startBound = startBoundTime == -1 ? liveTime - 1000 * 60 * 60 * 4 : startBoundTime;
    qint64 endBound = endBoundTime == -1 ? liveTime : endBoundTime;

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
            windowChanged = true;
            windowStart += delta;
            windowEnd += delta;
        }
    } else {
        windowChanged = true;
        qint64 time = pixelPosToTime(stickyPointKineticHelper.value());
        qint64 delta = stickyTime - time;
        windowStart += delta;
        windowEnd += delta;

        time = parent->position();
        if (time >= endBound && delta > 0)
            setStickToEnd(true);
        else if (time < endBound && delta < 0)
            setStickToEnd(false);
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

    if (windowChanged) {
        parent->windowStartChanged();
        parent->windowEndChanged();
        parent->positionChanged();
    }

    bool live = parent->position() + 1000 >= QDateTime::currentMSecsSinceEpoch();

    qreal targetLiveOpacity = live ? 1.0 : 0.0;
    if (!qFuzzyCompare(liveOpacity, targetLiveOpacity)) {
        if (liveOpacity < targetLiveOpacity)
            liveOpacity = qMin(targetLiveOpacity, liveOpacity + liveOpacityAnimationSpeed * dt);
        else
            liveOpacity = qMax(targetLiveOpacity, liveOpacity - liveOpacityAnimationSpeed * dt);
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
