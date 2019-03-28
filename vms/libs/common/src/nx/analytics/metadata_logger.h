#pragma once

#include <chrono>

#include <QtCore/QFile>

#include <nx/streaming/video_data_packet.h>
#include <nx/utils/uuid.h>
#include <utils/media/frame_info.h>
#include <nx/analytics/i_frame_info.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx::analytics {

using PlaceholderName = QString;
using PlaceholderValue = QString;

using PlaceholderMap = std::map<PlaceholderName, PlaceholderValue>;

class MetadataLogger
{
public:
    MetadataLogger(
        const QString& logFilePrefix,
        const QString& frameLogPattern,
        const QString& metadataLogPattern,
        QnUuid deviceId,
        QnUuid engineId);

    void pushFrameInfo(std::unique_ptr<IFrameInfo> frameInfo);

    void pushObjectMetadata(const nx::common::metadata::DetectionMetadataPacket& metadataPacket);

private:
    PlaceholderMap placeholderMap() const;
    void doLogging(const QString& logPattern);

private:
    QFile m_outputFile;
    QString m_frameLogPattern;
    QString m_metadataLogPattern;

    nx::common::metadata::DetectionMetadataPacket m_lastObjectMetadataPacket;
    nx::common::metadata::DetectionMetadataPacket m_previousObjectMetadataPacket;

    std::chrono::microseconds m_lastFrameTimestamp{0};
    std::chrono::microseconds m_previousFrameTimestamp{0};
};

} // namespace nx::analytics
