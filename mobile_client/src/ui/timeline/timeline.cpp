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

namespace {

    const int zoomLevelsVisible = 3;
    const qreal maxZoomLevelDiff = 1.0;
    const int minTickDps = 8;
    const int extraTickCount = 10;
    const qreal zoomAnimationSpeed = 0.004;
    const qreal textOpacityAnimationSpeed = 0.004;
    const qreal textMarginDp = 35;
    const qreal zoomMultiplier = 2.0;

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

} // anonymous namespace

class QnTimelinePrivate {
public:
    QnTimeline *parent;

    QColor tickColor;
    QColor textColor;
    QColor cursorColor;
    QColor chunkColor;

    qint64 startTime;
    qint64 endTime;

    qint64 windowStart;
    qint64 windowEnd;

    qint64 cursorPosition;

    QImage textTexture;

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

    QScopedPointer<QnTimelineTextHelper> textHelper;

    QElapsedTimer animationTimer;
    qint64 prevAnimationMs;

    QnTimelinePrivate(QnTimeline *parent):
        parent(parent),
        tickColor("#727272"),
        textColor("#727272"),
        cursorColor("#727272"),
        chunkColor("#2196F3"),
        textLevel(1.0),
        targetTextLevel(1.0),
        timeZoneShift(0)
    {
        dpMultiplier = 2;

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

    bool animateProperties(qint64 dt);
    void zoomWindow(qreal factor, int x);
};

QnTimeline::QnTimeline(QQuickItem *parent) :
    QQuickItem(parent),
    d(new QnTimelinePrivate(this))
{
    setFlag(QQuickItem::ItemHasContents);
    setAcceptedMouseButtons(Qt::LeftButton);
    connect(this, &QnTimeline::widthChanged, this, [this](){ d->updateZoomLevel(); });

    QnTimelineZoomLevel::monthsNames << tr("Jan") << tr("Feb") << tr("Mar")
                                     << tr("Apr") << tr("May") << tr("Jun")
                                     << tr("Jul") << tr("Aug") << tr("Sep")
                                     << tr("Oct") << tr("Nov") << tr("Dec");

    QStringList suffixList;
    suffixList << lit("ms") << lit("s") << lit("m") << lit("h") << lit(":00");
    for (const QString &month : QnTimelineZoomLevel::monthsNames)
        suffixList.append(month);

    d->textHelper.reset(new QnTimelineTextHelper(QFont(), d->textColor, suffixList));
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
    emit windowStartDateChanged();
    emit positionChanged();
    emit positionDateChanged();
}

QDateTime QnTimeline::windowStartDate() const {
    return QDateTime::fromMSecsSinceEpoch(windowStart(), Qt::UTC);
}

void QnTimeline::setWindowStartDate(const QDateTime &dateTime) {
    setWindowStart(dateTime.toMSecsSinceEpoch());
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
    emit windowEndDateChanged();
    emit positionChanged();
    emit positionDateChanged();
}

QDateTime QnTimeline::windowEndDate() const {
    return QDateTime::fromMSecsSinceEpoch(windowEnd(), Qt::UTC);
}

void QnTimeline::setWindowEndDate(const QDateTime &dateTime) {
    setWindowEnd(dateTime.toMSecsSinceEpoch());
}

void QnTimeline::setWindow(qint64 windowStart, qint64 windowEnd) {
    if (d->windowStart == windowStart && d->windowEnd == windowEnd)
        return;

    d->windowStart = windowStart;
    d->windowEnd = windowEnd;
    d->updateZoomLevel();
    update();
    emit windowStartChanged();
    emit windowStartDateChanged();
    emit windowEndChanged();
    emit windowEndDateChanged();
    emit positionChanged();
    emit positionDateChanged();
}

qint64 QnTimeline::position() const {
    return (d->windowStart + d->windowEnd) / 2;
}

void QnTimeline::setPosition(qint64 position) {
    if (position == this->position())
        return;

    qint64 size = windowEnd() - windowStart();
    qint64 start = position - size / 2;
    setWindow(start, start + size);
}

QDateTime QnTimeline::positionDate() const {
    return QDateTime::fromMSecsSinceEpoch(position(), Qt::UTC);
}

void QnTimeline::setPositionDate(const QDateTime &dateTime) {
    setPosition(dateTime.toMSecsSinceEpoch());
}

void QnTimeline::zoomIn(int x) {
    d->zoomWindow(zoomMultiplier, x >= 0 ? x : width() / 2);
}

void QnTimeline::zoomOut(int x) {
    d->zoomWindow(1.0 / zoomMultiplier, x >= 0 ? x : width() / 2);
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
    d->stickyPointKineticHelper.finish(x);
    d->zoomKineticHelper.finish(d->startZoom * scale);
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

void QnTimeline::wheelEvent(QWheelEvent *event) {
#if 0
    if (event->angleDelta().x() != 0) {
        int zoomIndex = qFloor(d->zoomLevel);
        const QnTimelineZoomLevel &zoomLevel = d->zoomLevels[zoomIndex];
        qreal zoomMultiplier = 1.0 - (d->zoomLevel - zoomIndex);
        qint64 timeDelta = zoomLevel.interval * zoomMultiplier * event->angleDelta().x() * 0.01;
        setWindow(windowStart() + timeDelta, windowEnd() + timeDelta);
    }
#endif

    if (event->angleDelta().y() != 0) {
        qreal factor = qAbs(event->angleDelta().y()) / 120.0 * zoomMultiplier;
        if (event->angleDelta().y() < 0)
            factor = 1.0 / factor;
        d->zoomWindow(factor, event->x());
    }
}

QSGNode *QnTimeline::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *updatePaintNodeData) {
    Q_UNUSED(updatePaintNodeData)

    qint64 time = d->animationTimer.elapsed();

    bool animated = d->animateProperties(time - d->prevAnimationMs);
    d->prevAnimationMs = time;

    if (!node) {
        node = new QSGNode();

        node->appendChildNode(updateTicksNode(nullptr));
        node->appendChildNode(updateChunksNode(nullptr));
    } else {
        updateTicksNode(static_cast<QSGGeometryNode*>(node->childAtIndex(0)));
        updateChunksNode(static_cast<QSGGeometryNode*>(node->childAtIndex(1)));
    }

    if (animated)
        update();

    return node;
}

QSGGeometryNode *QnTimeline::updateTicksNode(QSGGeometryNode *ticksNode) {
    int zoomIndex = cFloor(d->zoomLevel);
    const QnTimelineZoomLevel &zoomLevel = d->zoomLevels[zoomIndex];

    int lineWidth = d->dp(1);
    int lineHeight = d->dp(8);
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
    qreal visibilityMultipler = 1.0 - (d->zoomLevel - zoomIndex);

    QSGGeometry *geometry;
    QSGGeometryNode *textNode;
    QSGGeometry *textGeometry;
    QSGGeometryNode *lowerTextNode;
    QSGGeometry *lowerTextGeometry;
    QSGOpacityNode *lowerTextOpacityNode;

    if (!ticksNode) {
        ticksNode = new QSGGeometryNode();

        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), (tickCount + 1) * 2);
        geometry->setDrawingMode(GL_LINES);
        geometry->setLineWidth(lineWidth);

        QSGFlatColorMaterial *material = new QSGFlatColorMaterial();
        material->setColor(d->tickColor);

        ticksNode->setGeometry(geometry);
        ticksNode->setFlag(QSGNode::OwnsGeometry);
        ticksNode->setMaterial(material);
        ticksNode->setFlag(QSGNode::OwnsMaterial);

        textNode = new QSGGeometryNode();

        textGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
        textGeometry->setDrawingMode(GL_TRIANGLES);

        QSGTextureMaterial *textMaterial = new QSGTextureMaterial();
        textMaterial->setTexture(window()->createTextureFromImage(d->textHelper->texture()));

        textNode->setGeometry(textGeometry);
        textNode->setFlag(QSGNode::OwnsGeometry);
        textNode->setMaterial(textMaterial);
        textNode->setFlag(QSGNode::OwnsMaterial);
        ticksNode->appendChildNode(textNode);

        lowerTextOpacityNode = new QSGOpacityNode();

        lowerTextNode = new QSGGeometryNode();

        lowerTextGeometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
        lowerTextGeometry->setDrawingMode(GL_TRIANGLES);

        lowerTextNode->setGeometry(lowerTextGeometry);
        lowerTextNode->setFlag(QSGNode::OwnsGeometry);
        lowerTextNode->setMaterial(textMaterial);
        lowerTextOpacityNode->appendChildNode(lowerTextNode);
        ticksNode->appendChildNode(lowerTextOpacityNode);
    } else {
        geometry = ticksNode->geometry();
        geometry->allocate((tickCount + 1) * 2);
        d->prevZoomIndex = zoomIndex;

        textNode = static_cast<QSGGeometryNode*>(ticksNode->childAtIndex(0));
        textGeometry = textNode->geometry();
        lowerTextOpacityNode = static_cast<QSGOpacityNode*>(ticksNode->childAtIndex(1));
        lowerTextNode = static_cast<QSGGeometryNode*>(lowerTextOpacityNode->childAtIndex(0));
        lowerTextGeometry = lowerTextNode->geometry();
    }

    QSGGeometry::Point2D *points = geometry->vertexDataAsPoint2D();
    points[0].set(0, correction);
    points[1].set(width(), correction);
    points += 2;

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

    qreal lowerTextOpacity = 1.0 - (d->textLevel - textMarkLevel);
    lowerTextOpacityNode->setOpacity(lowerTextOpacity);

    int textCount = 0;
    int lowerTextCount = 0;

    for (int i = 1; i < geometry->vertexCount() / 2; ++i) {
        int tickLevel = d->tickLevel(tick);
        int *count = textMarkLevel == tickLevel ? &lowerTextCount : &textCount;

        qreal tickHeight = qFloor(lineHeight * qMin<qreal>(tickLevel - zoomIndex + visibilityMultipler, zoomLevelsVisible));
        qreal x;
        if (zoomLevel.isMonotonic())
            x = xStart + xStep * (i - 1);
        else
            x = xStart + (tick - windowStartAligned) * width() / windowSize;

        if (tickLevel >= textMarkLevel) {
            MarkInfo info;
            info.tick = tick;
            info.x = x;
            info.zoomIndex = tickLevel;
            markedTicks.append(info);
            QString baseValue = d->zoomLevels[tickLevel].baseValue(tick);
            *count += (!baseValue.isEmpty() && !baseValue[0].isDigit()) ? 1 : baseValue.size();
            if (!d->zoomLevels[tickLevel].suffix().isEmpty())
                ++(*count);
        }

        points[0].set(x + correction, correction);
        points[1].set(x + correction, correction + tickHeight);

        points += 2;
        tick = zoomLevel.nextTick(tick);
    }
    ticksNode->markDirty(QSGNode::DirtyGeometry);

    /* update text */
    textGeometry->allocate(textCount * 6);
    QSGGeometry::TexturedPoint2D *textPoints = textGeometry->vertexDataAsTexturedPoint2D();
    lowerTextGeometry->allocate(lowerTextCount * 6);
    QSGGeometry::TexturedPoint2D *lowerTextPoints = lowerTextGeometry->vertexDataAsTexturedPoint2D();

    QSize digitSize = d->textHelper->digitSize();
    for (const MarkInfo &info: markedTicks) {
        QSGGeometry::TexturedPoint2D *points = (info.zoomIndex == textMarkLevel) ? lowerTextPoints : textPoints;

        QString baseValue = d->zoomLevels[info.zoomIndex].baseValue(info.tick);
        QString suffix = d->zoomLevels[info.zoomIndex].suffix();
        QSize suffixSize = d->textHelper->stringSize(suffix);
        QSize valueSize;

        bool isNum = baseValue.isEmpty() || baseValue[0].isDigit();

        qreal tw = suffixSize.width();
        if (isNum) {
            valueSize = d->textHelper->digitSize();
            tw += valueSize.width() * baseValue.size();
        } else {
            valueSize = d->textHelper->stringSize(baseValue);
            tw += valueSize.width();
        }

        qreal x = qFloor(info.x - tw / 2);
        qreal y = qFloor(lineHeight * qMin<qreal>(d->maxSiblingLevel(info.zoomIndex) - zoomIndex + visibilityMultipler, zoomLevelsVisible)) + d->dp(4);
        int shift = 0;

        if (isNum) {
            for (int i = 0; i < baseValue.size(); ++i) {
                int digit = baseValue.mid(i, 1).toInt();
                QRectF texCoord = d->textHelper->digitCoordinates(digit);
                points[0].set(x, y, texCoord.left(), texCoord.top());
                points[1].set(x + digitSize.width(), y, texCoord.right(), texCoord.top());
                points[2].set(x + digitSize.width(), y + digitSize.height(), texCoord.right(), texCoord.bottom());
                points[3] = points[0];
                points[4] = points[2];
                points[5].set(x, y + digitSize.height(), texCoord.left(), texCoord.bottom());
                points += 6;
                shift += 6;
                x += digitSize.width();
            }
        } else {
            QRectF texCoord = d->textHelper->stringCoordinates(baseValue);
            points[0].set(x, y, texCoord.left(), texCoord.top());
            points[1].set(x + valueSize.width(), y, texCoord.right(), texCoord.top());
            points[2].set(x + valueSize.width(), y + valueSize.height(), texCoord.right(), texCoord.bottom());
            points[3] = points[0];
            points[4] = points[2];
            points[5].set(x, y + valueSize.height(), texCoord.left(), texCoord.bottom());
            points += 6;
            shift += 6;
        }

        if (!suffix.isEmpty()) {
            QRectF texCoord = d->textHelper->stringCoordinates(suffix);
            points[0].set(x, y, texCoord.left(), texCoord.top());
            points[1].set(x + suffixSize.width(), y, texCoord.right(), texCoord.top());
            points[2].set(x + suffixSize.width(), y + suffixSize.height(), texCoord.right(), texCoord.bottom());
            points[3] = points[0];
            points[4] = points[2];
            points[5].set(x, y + suffixSize.height(), texCoord.left(), texCoord.bottom());
            shift += 6;
        }

        if (info.zoomIndex == textMarkLevel)
            lowerTextPoints += shift;
        else
            textPoints += shift;
    }

    textNode->markDirty(QSGNode::DirtyGeometry);
    lowerTextNode->markDirty(QSGNode::DirtyGeometry);

    return ticksNode;
}

QSGGeometryNode *QnTimeline::updateChunksNode(QSGGeometryNode *chunksNode) {
    QSGGeometry *geometry;

    if (!chunksNode) {
        chunksNode = new QSGGeometryNode();

        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(GL_TRIANGLES);
        chunksNode->setGeometry(geometry);
        chunksNode->setFlag(QSGNode::OwnsGeometry);

        QSGVertexColorMaterial *material = new QSGVertexColorMaterial();
        chunksNode->setMaterial(material);
        chunksNode->setFlag(QSGNode::OwnsMaterial);
    } else {
        geometry = chunksNode->geometry();
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

    qreal y = height() * 2 / 3;

    QnTimelineChunkPainter chunkPainter(geometry);
    std::array<QColor, Qn::TimePeriodContentCount + 1> colors = chunkPainter.colors();
    colors[Qn::RecordingContent] = d->chunkColor;
    chunkPainter.setColors(colors);
    chunkPainter.start(value, position(), QRectF(0, y, width(), height() - y),
                       d->timePeriods[Qn::RecordingContent].size(),
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

//            if(!pos[i]->isInfinite())
            if(!pos[i]->durationMs == -1)
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

    return chunksNode;
}

bool QnTimelinePrivate::animateProperties(qint64 dt) {
    bool changed = false;

    bool windowChanged = false;
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

    stickyPointKineticHelper.update();
    if (!stickyPointKineticHelper.isStopped() || windowChanged) {
        windowChanged = true;

        qint64 time = pixelPosToTime(stickyPointKineticHelper.value());
        qint64 delta = stickyTime - time;
        windowStart += delta;
        windowEnd += delta;
    }

    changed |= windowChanged;

    if (!qFuzzyCompare(zoomLevel, targetZoomLevel)) {
        changed = true;

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
        changed = true;

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
        parent->windowStartDateChanged();
        parent->windowEndDateChanged();
        parent->positionDateChanged();
    }

    return changed;
}

void QnTimelinePrivate::zoomWindow(qreal factor, int x) {
    qreal speed = qSqrt(2 * zoomKineticHelper.deceleration() * parent->width() * qAbs(factor - 1));
    if (factor < 1)
        speed = -speed;

    stickyPointKineticHelper.start(x);
    stickyPointKineticHelper.stop();
    stickyTime = pixelPosToTime(x);
    startZoom = parent->width();
    startWindowSize = windowEnd - windowStart;
    zoomKineticHelper.flick(startZoom, speed);

    parent->update();
}
