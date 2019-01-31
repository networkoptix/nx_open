#pragma once

#include <map>
#include <queue>

#include <QtGui/QImage>

#include <nx/debugging/abstract_visual_metadata_debugger.h>

#include <nx/utils/thread/long_runnable.h>
#include <decoders/video/ffmpeg_video_decoder.h>

class QFile;

namespace nx {
namespace debugging {

using FrameTimestamp = int64_t;
using MetadataTimestamp = int64_t;

class VisualMetadataDebugger:
    public AbstractVisualMetadataDebugger,
    public QnLongRunnable
{

public:
    VisualMetadataDebugger(
        const QString& debuggerRootDir,
        int maxFrameCacheSize,
        int maxMetadataCacheSize);

    virtual ~VisualMetadataDebugger() override;

public:
    virtual void push(const QnConstCompressedVideoDataPtr& video) override;
    virtual void push(
        const nx::common::metadata::DetectionMetadataPacketPtr& detectionMetadata) override;
    virtual void push(const CLConstVideoDecoderOutputPtr& frame) override;
    virtual void push(const QnConstCompressedMetadataPtr& metadata) override;

protected:
    virtual void run() override;

private:
    CLVideoDecoderOutputPtr decode(const QnConstCompressedVideoDataPtr& video);

    void addFrameToCache(const CLConstVideoDecoderOutputPtr& frame);
    void addMetadataToCache(const nx::common::metadata::DetectionMetadataPacketPtr& metadata);
    std::pair<FrameTimestamp, MetadataTimestamp> makeOverlayedImages();
    void updateCache(
        const std::pair<FrameTimestamp, MetadataTimestamp>& lastProcessedTimestamps);

    static QByteArray makeMetadataString(
        const nx::common::metadata::DetectionMetadataPacketPtr& detectionMetadata);

    static void drawMetadata(
        QImage* inOutImage,
        const nx::common::metadata::DetectionMetadataPacketPtr& detectionMetadata);

private:
    mutable QnMutex m_mutex;
    std::queue<CLConstVideoDecoderOutputPtr> m_frameQueue;
    std::queue<nx::common::metadata::DetectionMetadataPacketPtr> m_metadataQueue;

    std::map<FrameTimestamp, CLConstVideoDecoderOutputPtr> m_frameCache;
    std::map<MetadataTimestamp, nx::common::metadata::DetectionMetadataPacketPtr> m_metadataCache;

    const int m_maxFrameCacheSize;
    const int m_maxMetadataCacheSize;

    const QString m_debuggerRootDir;
    const QString m_rawImagesDir;
    const QString m_overlayedImagesDir;
    const QString m_metadataDumpFileName;
    const QString m_errorFileName;

    std::unique_ptr<QFile> m_metadataDumpFile;
    std::unique_ptr<QFile> m_errorFile;

    std::unique_ptr<QnFfmpegVideoDecoder> m_decoder;
};

} // namespace debugging
} // namespace nx
