#pragma once

#include <chrono>

#include <QtCore/QFile>

#include <nx/analytics/frame_info.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/vms/api/types/motion_types.h>
#include <analytics/common/object_metadata.h>

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
        const FrameInfo& frameInfo,
        const QString& additionalFrameInfo = QString());

    void pushObjectMetadata(
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalObjectMetadataInfo = QString());

private:
    void log(QString logLine);

    QString buildFrameLogString(const FrameInfo& frameInfo, const QString& additionalInfo);
    QString buildObjectMetadataLogString(
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalInfo);

private:
    QFile m_outputFile;
    nx::vms::api::StreamIndex m_streamIndex;

    bool m_isLoggingBestShot = false;

    std::chrono::microseconds m_prevFrameTimestamp{0};
    std::chrono::microseconds m_prevObjectMetadataPacketTimestamp{0};
};

} // namespace nx::analytics
