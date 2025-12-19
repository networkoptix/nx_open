// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "chunk_bar.h"

#include <cmath>

#include <QtCore/QPointer>
#include <QtGui/QTransform>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGTransformNode>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::core {
namespace timeline {

using namespace std::chrono;

// ------------------------------------------------------------------------------------------------
// ChunkBar::Private

struct ChunkBar::Private
{
    QnTimePeriodList chunks;
    QnTimePeriod window{0ms, 1min};
    QColor backgroundColor = Qt::transparent;
    QColor chunkColor = Qt::green;
    bool mirrored = false;

    Qt::Orientation orientation = Qt::Horizontal; //< Chosen automatically by the aspect ratio.
    int length = 0;
    qreal devicePixelRatio = 1.0;

    bool dirtyTexture = true;
    bool dirtyTransform = true;

    void paintChunks(QRgb* target, int pixelCount);
};

void ChunkBar::Private::paintChunks(QRgb* target, int pixelCount)
{
    const auto startTimeMs = window.startTimeMs;
    const auto endTimeMs = window.endTimeMs();

    const auto begin = chunks.findNearestPeriod(startTimeMs, /*forward*/ true);
    auto end = chunks.findNearestPeriod(endTimeMs, /*forward*/ true);
    if (end != chunks.end() && end->contains(endTimeMs))
        ++end;

    if (begin == end)
        return;

    const double k = static_cast<double>(pixelCount) / (endTimeMs - startTimeMs);

    const auto timeToCoord =
        [startTimeMs, k](qint64 timeMs) { return (int) std::round((timeMs - startTimeMs) * k); };

    const int lastPx = pixelCount - 1;
    const QRgb argbColor = chunkColor.rgba();

    for (auto chunk = begin; chunk != end; ++chunk)
    {
        const int beginPx = std::clamp(timeToCoord(chunk->startTimeMs), 0, lastPx);
        if (chunk->isInfinite())
        {
            std::fill(target + beginPx, target + pixelCount, argbColor);
        }
        else
        {
            int endPx = timeToCoord(chunk->endTimeMs());
            if (endPx == beginPx)
                ++endPx;
            else if (endPx > pixelCount)
                endPx = pixelCount;

            std::fill(target + beginPx, target + endPx, argbColor);
        }
    }
}

// ------------------------------------------------------------------------------------------------
// ChunkBar

ChunkBar::ChunkBar(QQuickItem* parent):
    QQuickItem(parent),
    d(new Private{})
{
    setFlag(QQuickItem::ItemHasContents);
}

ChunkBar::~ChunkBar()
{
    // Required here for forward declared scoped pointer destruction.
}

QnTimePeriodList ChunkBar::chunks() const
{
    return d->chunks;
}

void ChunkBar::setChunks(const QnTimePeriodList& value)
{
    d->chunks = value;
    d->dirtyTexture = true;
    emit chunksChanged();
    update();
}

milliseconds ChunkBar::startTime() const
{
    return d->window.startTime();
}

void ChunkBar::setStartTime(milliseconds value)
{
    if (startTime() == value)
        return;

    d->window.setStartTime(value);
    d->dirtyTexture = true;

    emit startTimeChanged();
    update();
}

qint64 ChunkBar::startTimeMs() const
{
    return startTime().count();
}

void ChunkBar::setStartTimeMs(qint64 value)
{
    setStartTime(milliseconds(value));
}

std::chrono::milliseconds ChunkBar::duration() const
{
    return d->window.duration();
}

void ChunkBar::setDuration(milliseconds value)
{
    if (duration() == value)
        return;

    d->window.setDuration(value);
    d->dirtyTexture = true;

    emit durationChanged();
    update();
}

qint64 ChunkBar::durationMs() const
{
    return duration().count();
}

void ChunkBar::setDurationMs(qint64 value)
{
    setDuration(milliseconds(value));
}

QColor ChunkBar::backgroundColor() const
{
    return d->backgroundColor;
}

void ChunkBar::setBackgroundColor(QColor value)
{
    if (d->backgroundColor == value)
        return;

    d->backgroundColor = value;
    d->dirtyTexture = true;

    emit backgroundColorChanged();
    update();
}

QColor ChunkBar::chunkColor() const
{
    return d->chunkColor;
}

void ChunkBar::setChunkColor(QColor value)
{
    if (d->chunkColor == value)
        return;

    d->chunkColor = value;
    d->dirtyTexture = true;

    emit chunkColorChanged();
    update();
}

bool ChunkBar::mirrored() const
{
    return d->mirrored;
}

void ChunkBar::setMirrored(bool value)
{
    if (d->mirrored == value)
        return;

    d->mirrored = value;

    emit mirroredChanged();
    update();
}

void ChunkBar::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    const auto orientation = newGeometry.height() > newGeometry.width()
        ? Qt::Vertical
        : Qt::Horizontal;

    const auto length = orientation == Qt::Horizontal
        ? newGeometry.width()
        : newGeometry.height();

    // In case of vertical orientation we'll need to rotate a horizontal textured rectangle
    // 90 degrees around `{width, 0}` pivot point, hence the `width` dependency.
    d->dirtyTransform = d->orientation != orientation
        || (orientation == Qt::Vertical && !qFuzzyEquals(oldGeometry.width(), newGeometry.width()));

    d->orientation = orientation;

    if (!qFuzzyEquals(length, d->length))
    {
        d->length = length;
        d->dirtyTexture = true;
    }
}

QSGNode* ChunkBar::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* /*updatePaintNodeData*/)
{
    if (!NX_ASSERT(window()))
        return oldNode;

    const auto devicePixelRatio = window()->effectiveDevicePixelRatio();
    if (!qFuzzyEquals(devicePixelRatio, d->devicePixelRatio))
    {
        d->devicePixelRatio = devicePixelRatio;
        d->dirtyTexture = true;
    }

    QSGTransformNode* transformNode = nullptr;
    QSGImageNode* imageNode = nullptr;

    if (oldNode && NX_ASSERT(
        oldNode->type() == QSGNode::TransformNodeType
        && oldNode->childCount() == 1
        && oldNode->childAtIndex(0)->type() == QSGNode::GeometryNodeType))
    {
        transformNode = static_cast<QSGTransformNode*>(oldNode);
        imageNode = static_cast<QSGImageNode*>(oldNode->childAtIndex(0));
    }

    if (!transformNode)
    {
        transformNode = new QSGTransformNode{};
        d->dirtyTransform = true;
    }

    if (!imageNode)
    {
        imageNode = window()->createImageNode();
        imageNode->setFlag(QSGNode::OwnedByParent);
        imageNode->setOwnsTexture(true);
        transformNode->appendChildNode(imageNode);
        d->dirtyTexture = true;
    }

    if (d->dirtyTransform)
    {
        transformNode->setMatrix(d->orientation == Qt::Horizontal
            ? QMatrix4x4{}
            : QTransform{}.rotate(90) * QTransform{}.translate(width(), 0));

        d->dirtyTransform = false;
    }

    if (d->dirtyTexture)
    {
        const int textureSize = std::ceil(d->length * d->devicePixelRatio);

        QImage image(textureSize, 1, QImage::Format_ARGB32);
        image.fill(d->backgroundColor);

        d->paintChunks(reinterpret_cast<QRgb*>(image.bits()), textureSize);

        const auto texture = window()->createTextureFromImage(image);
        imageNode->setTexture(texture);

        d->dirtyTexture = false;
    }

    if (d->orientation == Qt::Horizontal)
        imageNode->setRect(0, 0, width(), height());
    else
        imageNode->setRect(0, 0, height(), width());

    imageNode->setTextureCoordinatesTransform(d->mirrored
        ? QSGImageNode::MirrorHorizontally
        : QSGImageNode::NoTransform);

    return transformNode;
}

void ChunkBar::registerQmlType()
{
    qmlRegisterType<ChunkBar>("nx.vms.client.core.timeline", 1, 0, "ChunkBar");
}

} // namespace timeline
} // namespace nx::vms::client::core
