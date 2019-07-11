#pragma once

#include <chrono>

#include <QtCore/QFile>

#include <nx/streaming/video_data_packet.h>
#include <nx/utils/uuid.h>
#include <utils/media/frame_info.h>
#include <nx/analytics/i_frame_info.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx::analytics {

class MetadataLogger
{
public:
    MetadataLogger(
        const QString& logFilePrefix,
        QnUuid deviceId,
        QnUuid engineId,
        nx::vms::api::StreamIndex = nx::vms::api::StreamIndex::undefined);

    void pushData(
        const QnConstAbstractMediaDataPtr& data,
        const QString& additionalInfo = QString());

    void pushFrameInfo(
        std::unique_ptr<IFrameInfo> frameInfo,
        const QString& additionalFrameInfo = QString());

    void pushObjectMetadata(
        nx::common::metadata::DetectionMetadataPacket metadataPacket,
        const QString& additionalObjectMetadataInfo = QString());

private:
    using PlaceholderMap = std::map</*PlaceholderName*/ QString, /*PlaceholderValue*/ QString>;

    PlaceholderMap placeholderMap() const;
    void doLogging(const QString& logPattern);
    QString makeLogFileName(
        const QString& analyticsLoggingPath,
        const QString& logFilePrefix,
        QnUuid deviceId,
        QnUuid engineId,
        nx::vms::api::StreamIndex streamIndex);

private:
    QFile m_outputFile;
    nx::vms::api::StreamIndex m_streamIndex;

    bool m_isLoggingBestShot = false;

    nx::common::metadata::DetectionMetadataPacket m_currentObjectMetadataPacket;
    nx::common::metadata::DetectionMetadataPacket m_prevObjectMetadataPacket;

    nx::common::metadata::DetectionMetadataPacket m_currentBestShotMetadataPacket;
    QString m_additionalObjectMetadataInfo;

    std::chrono::microseconds m_currentFrameTimestamp{0};
    std::chrono::microseconds m_prevFrameTimestamp{0};
    QString m_additionalFrameInfo;
};

} // namespace nx::analytics
