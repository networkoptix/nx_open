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
        const QString& additionalInfo = QString());

    void pushObjectMetadata(
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalInfo = QString());

private:
    void logLine(QString lineStr);

    QString buildFrameLogString(const FrameInfo& frameInfo, const QString& additionalInfo) const;

    QString buildObjectMetadataLogString(
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalInfo) const;

    void doPushObjectMetadata(
        const char* func,
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalInfo);

public: //< Intended for unit tests.
    MetadataLogger(const QString& filename): m_isAlwaysEnabled(true)

    {
        NX_CRITICAL(!filename.isEmpty());
        m_outputFile.setFileName(filename);
        const bool result = m_outputFile.open(QIODevice::WriteOnly);
        NX_CRITICAL(result);
    }

private:
    bool m_isAlwaysEnabled = false;
    QFile m_outputFile;

    bool m_isLoggingBestShot = false;

    std::chrono::microseconds m_prevFrameTimestamp{0};
    std::chrono::microseconds m_prevObjectMetadataPacketTimestamp{0};
};

} // namespace nx::analytics
