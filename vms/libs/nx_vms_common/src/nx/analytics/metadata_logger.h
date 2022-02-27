// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QFile>

#include <nx/utils/thread/mutex.h>
#include <nx/analytics/frame_info.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/vms/api/types/motion_types.h>
#include <analytics/common/object_metadata.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>

namespace nx::analytics {

class NX_VMS_COMMON_API MetadataLogger
{
public:
    MetadataLogger(
        const QString& logFilePrefix,
        QnUuid deviceId,
        QnUuid engineId,
        nx::vms::api::StreamIndex = nx::vms::api::StreamIndex::undefined);

    ~MetadataLogger();

    void pushData(
        const QnConstAbstractMediaDataPtr& abstractMediaData,
        const QString& additionalInfo = QString());

    void pushFrameInfo(
        const FrameInfo& frameInfo,
        const QString& additionalInfo = QString());

    void pushObjectMetadata(
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalInfo = QString());

    void pushCustomMetadata(
        const nx::sdk::Ptr<nx::sdk::analytics::ICustomMetadataPacket>& customMetadata);

private:
    void logLine(QString lineStr);

    QString buildFrameLogString(const FrameInfo& frameInfo, const QString& additionalInfo) const;

    QString buildCustomMetadataLogString(
        const nx::sdk::Ptr<nx::sdk::analytics::ICustomMetadataPacket>& customMetadata);

    QString buildObjectMetadataLogString(
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalInfo) const;

    void doPushObjectMetadata(
        const char* func,
        const nx::common::metadata::ObjectMetadataPacket& metadataPacket,
        const QString& additionalInfo);

public: //< Intended for unit tests.
    MetadataLogger(const QString& filename);

private:
    mutable nx::Mutex m_mutex;

    bool m_isAlwaysEnabled = false;
    QFile m_outputFile;

    bool m_isLoggingBestShot = false;

    std::chrono::microseconds m_prevFrameTimestamp{0};
    std::chrono::microseconds m_prevObjectMetadataPacketTimestamp{0};
    std::chrono::microseconds m_prevCustomMetadataTimestamp{0};
};

} // namespace nx::analytics
