#pragma once

#include <map>
#include <queue>

#include <QtGui/QImage>

#include <nx/utils/thread/long_runnable.h>
#include <nx/streaming/video_data_packet.h>

#include <utils/media/frame_info.h>
#include <analytics/common/object_detection_metadata.h>
#include <decoders/video/ffmpeg_video_decoder.h>

namespace nx {
namespace debugging {

using FrameTimestamp = int64_t;
using MetadataTimestamp = int64_t;

class VisualMetadataDebugger: public QnLongRunnable
{
    Q_OBJECT;

public:
    VisualMetadataDebugger(
        const QString& debuggerRootDir,
        int maxFrameCacheSize,
        int maxMetadataCacheSize);

public:
    void push(const QnCompressedVideoDataPtr& video);
    void push(const nx::common::metadata::DetectionMetadataPacketPtr& detectionMetadata);
    void push(const CLVideoDecoderOutputPtr& frame);
    void push(const QnCompressedMetadataPtr& metadata);

protected:
    virtual void run() override;

private:
    CLVideoDecoderOutputPtr decode(const QnCompressedVideoDataPtr& video);

    void addFrameToCache(const CLVideoDecoderOutputPtr& frame);
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
    std::queue<CLVideoDecoderOutputPtr> m_frameQueue;
    std::queue<nx::common::metadata::DetectionMetadataPacketPtr> m_metadataQueue;

    std::map<FrameTimestamp, CLVideoDecoderOutputPtr> m_frameCache;
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