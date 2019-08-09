#include "visual_metadata_debugger.h"

#include <algorithm>

#include <QtCore/QDir>
#include <QtGui/QPainter>

#include <utils/common/sleep.h>
#include <nx/streaming/abstract_stream_data_provider.h>

namespace nx {
namespace debugging {

namespace {

static const QString kRawImagesDirectory = lit("raw");
static const QString kOverlayedImagesDirectory = lit("overlayed");
static const QString kMetadataDumpFile = lit("metadata.txt");
static const QString kErrorFile = lit("error.txt");

} // namespace

VisualMetadataDebugger::VisualMetadataDebugger(
    const QString& debuggerRootDir,
    int maxFrameCacheSize,
    int maxMetadataCacheSize)
    :
    m_maxFrameCacheSize(maxFrameCacheSize),
    m_maxMetadataCacheSize(maxMetadataCacheSize),
    m_debuggerRootDir(debuggerRootDir),
    m_rawImagesDir(
        QDir::cleanPath(m_debuggerRootDir + QDir::separator() + kRawImagesDirectory)),
    m_overlayedImagesDir(
        QDir::cleanPath(m_debuggerRootDir + QDir::separator() + kOverlayedImagesDirectory)),
    m_metadataDumpFileName(QDir(m_debuggerRootDir).filePath(kMetadataDumpFile)),
    m_errorFileName(QDir(m_debuggerRootDir).filePath(kErrorFile))
{
    start();
}

VisualMetadataDebugger::~VisualMetadataDebugger()
{
    stop();
}

void VisualMetadataDebugger::push(const CLConstVideoDecoderOutputPtr& frame)
{
    QnMutexLocker lock(&m_mutex);
    m_frameQueue.push(frame);
}

void VisualMetadataDebugger::push(const QnConstCompressedMetadataPtr& compressedMetadata)
{
    using namespace nx::common::metadata;
    if (compressedMetadata->metadataType != MetadataType::ObjectDetection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (const auto metadataPacket = fromCompressedMetadataPacket(compressedMetadata))
        m_metadataQueue.push(metadataPacket);
}

void VisualMetadataDebugger::push(const QnConstCompressedVideoDataPtr& video)
{
    auto decoded = decode(video);
    if (decoded)
    {
        QnMutexLocker lock(&m_mutex);
        m_frameQueue.push(decoded);
    }
}

void VisualMetadataDebugger::push(
    const nx::common::metadata::ObjectMetadataPacketPtr& metadataPacket)
{
    QnMutexLocker lock(&m_mutex);
    m_metadataQueue.push(metadataPacket);
}

void VisualMetadataDebugger::run()
{
    while (!needToStop())
    {
        CLConstVideoDecoderOutputPtr frame;
        nx::common::metadata::ObjectMetadataPacketPtr metadataPacket;

        {
            QnMutexLocker lock(&m_mutex);
            if (!m_frameQueue.empty())
            {
                frame = m_frameQueue.front();
                m_frameQueue.pop();
            }

            if (!m_metadataQueue.empty())
            {
                metadataPacket = m_metadataQueue.front();
                m_metadataQueue.pop();
            }
        }

        if (!frame && !metadataPacket)
        {
            QnSleep::msleep(10);
            continue;
        }

        if (frame)
        {
            qDebug() << "=======> Frame: " << frame->pkt_dts << frame->pkt_pts << frame->pts;
            auto image = frame->toImage();
            image.save(QDir(m_rawImagesDir).filePath(QString::number(frame->pkt_dts) + lit(".jpg")));
            addFrameToCache(frame);
        }

        if (metadataPacket)
        {
            if (!m_metadataDumpFile)
            {
                m_metadataDumpFile = std::make_unique<QFile>(m_metadataDumpFileName);
                m_metadataDumpFile->open(QIODevice::WriteOnly);
            }

            m_metadataDumpFile->write(makeMetadataString(metadataPacket));
            m_metadataDumpFile->flush();
            addMetadataToCache(metadataPacket);
        }

        const auto lastProcesedTimestamps = makeOverlayedImages();
        updateCache(lastProcesedTimestamps);
    }
}

CLVideoDecoderOutputPtr VisualMetadataDebugger::decode(const QnConstCompressedVideoDataPtr& video)
{
    if (!m_decoder && !(video->flags & AV_PKT_FLAG_KEY))
        return CLVideoDecoderOutputPtr{nullptr};

    if (!m_decoder || video->compressionType != m_decoder->getContext()->codec_id)
    {
        DecoderConfig config;

        m_decoder = std::make_unique<QnFfmpegVideoDecoder>(
            config,
            video->compressionType,
            video);

        m_decoder->getContext()->flags &= ~CODEC_FLAG_GRAY; //< Turn off Y-only mode.
    }

    CLVideoDecoderOutputPtr decoded(new CLVideoDecoderOutput());
    if (!m_decoder->decode(video, &decoded))
        return CLVideoDecoderOutputPtr{nullptr};

    return decoded;
}

void VisualMetadataDebugger::addFrameToCache(const CLConstVideoDecoderOutputPtr& frame)
{
    while (m_frameCache.size() > m_maxFrameCacheSize)
        m_frameCache.erase(m_frameCache.begin());

    m_frameCache[frame->pkt_dts] = frame;
}

void VisualMetadataDebugger::addMetadataToCache(
    const nx::common::metadata::ObjectMetadataPacketPtr& metadata)
{
    while (m_metadataCache.size() > m_maxMetadataCacheSize)
        m_metadataCache.erase(m_metadataCache.begin());

    m_metadataCache[metadata->timestampUs] = metadata;
}

std::pair<FrameTimestamp, MetadataTimestamp> VisualMetadataDebugger::makeOverlayedImages()
{
    auto lastProcessedMetadataTimestamp = AV_NOPTS_VALUE;
    auto lastProcessedFrameTimestamp = AV_NOPTS_VALUE;

    for (const auto& metadata : m_metadataCache)
    {
        auto frameAfterMetadata = m_frameCache.lower_bound(
            /*metadata timestamp*/ metadata.first);

        if (frameAfterMetadata == m_frameCache.cend())
            break; //< There are no frames that arrived after metadata.

        CLConstVideoDecoderOutputPtr bestFrame = frameAfterMetadata->second;
        auto bestFrameTimestamp = frameAfterMetadata->first;

        if (frameAfterMetadata != m_frameCache.cbegin())
        {
            const auto frameBeforeMetadata = --frameAfterMetadata;
            const auto prevFrameDifference =
                frameBeforeMetadata->first - metadata.first;
            const auto nextFrameDifference =
                frameAfterMetadata->first - metadata.first;

            if (prevFrameDifference < nextFrameDifference)
            {
                bestFrame = frameBeforeMetadata->second;
                bestFrameTimestamp = frameBeforeMetadata->first;
            }
        }

        auto image = bestFrame->toImage();
        drawMetadata(&image, metadata.second);

        // File name format is: frameTimestamp_metadataTimestqamp.jpg
        // e.g. 1000000_1000001.jpg
        image.save(
            QDir(m_overlayedImagesDir).filePath(
                QString::number(bestFrameTimestamp)
                + lit("_")
                + QString::number(metadata.first)
                + lit(".jpg")));

        lastProcessedFrameTimestamp = bestFrameTimestamp;
        lastProcessedMetadataTimestamp = metadata.first;
    }

    return std::make_pair(lastProcessedFrameTimestamp, lastProcessedMetadataTimestamp);
}

void VisualMetadataDebugger::updateCache(
    const std::pair<FrameTimestamp, MetadataTimestamp>& lastProcessedTimestamps)
{
    if (lastProcessedTimestamps.first != AV_NOPTS_VALUE)
    {
        const auto lastFrameItr = m_frameCache.lower_bound(
            lastProcessedTimestamps.first);

        NX_ASSERT(lastProcessedTimestamps.first == lastFrameItr->first);
        m_frameCache.erase(m_frameCache.cbegin(), lastFrameItr);
    }

    if (lastProcessedTimestamps.second != AV_NOPTS_VALUE)
    {
        const auto lastMetadataItr = m_metadataCache.lower_bound(
            lastProcessedTimestamps.second);

        NX_ASSERT(lastProcessedTimestamps.second == lastMetadataItr->first);
        m_metadataCache.erase(m_metadataCache.cbegin(), lastMetadataItr);
    }
}

QByteArray VisualMetadataDebugger::makeMetadataString(
    const nx::common::metadata::ObjectMetadataPacketPtr& metadataPacket)
{
    QString str = lit("Packet:%1,%2\n")
        .arg(metadataPacket->timestampUs)
        .arg(metadataPacket->durationUs);

    for (const auto& objectMetadata : metadataPacket->objectMetadataList)
    {
        str += lit("\tObject:%1,%2,%3,%4,%5,%6\n")
            .arg(objectMetadata.typeId)
            .arg(objectMetadata.trackId.toString())
            .arg(objectMetadata.boundingBox.x())
            .arg(objectMetadata.boundingBox.y())
            .arg(objectMetadata.boundingBox.width())
            .arg(objectMetadata.boundingBox.height());

        for (const auto& attribute: objectMetadata.attributes)
        {
            str += lit("\t\tLabel:%1,%2\n")
                .arg(attribute.name)
                .arg(attribute.value);
        }
    }

    return str.toUtf8();
}

void VisualMetadataDebugger::drawMetadata(
    QImage* inOutImage,
    const nx::common::metadata::ObjectMetadataPacketPtr& metadataPacket)
{
    const auto imageWidth = inOutImage->width();
    const auto imageHeight = inOutImage->height();

    QPainter painter(inOutImage);
    painter.setBrush(Qt::NoBrush);
    QPen pen(Qt::red);
    pen.setWidth(7);
    painter.setPen(pen);

    for (const auto& objectMetadata : metadataPacket->objectMetadataList)
    {
        painter.drawRect(
            objectMetadata.boundingBox.x() * imageWidth,
            objectMetadata.boundingBox.y() * imageHeight,
            objectMetadata.boundingBox.width() * imageWidth,
            objectMetadata.boundingBox.height() * imageHeight);
    }

    painter.end();
}

} // namespace debugging
} // namespace nx
