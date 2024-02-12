// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QMap>

#include <nx/analytics/metadata_logger.h>
#include <nx/rtp/result.h>
#include <nx/utils/software_version.h>

namespace nx::rtp {

class NX_VMS_COMMON_API QnNxRtpParser
{
public:
    /** @param tag Human-readable tag for logging. */
    QnNxRtpParser(nx::Uuid deviceId, const QString& tag);

    Result processData(const uint8_t* data, int size, bool& gotData);
    QnAbstractMediaDataPtr nextData();

    qint64 position() const { return m_position; }
    CodecParametersConstPtr mediaContext() const { return m_context; }

    /**
     * Don't parse incoming audio packets if value is false.
     */
    void setAudioEnabled(bool value);
    void setServerVersion(const nx::utils::SoftwareVersion& serverVersion);

private:
    void logMediaData(const QnAbstractMediaDataPtr& metadata);

private:
    nx::utils::SoftwareVersion m_serverVersion;
    const nx::Uuid m_deviceId;
    CodecParametersConstPtr m_context;
    QnAbstractMediaDataPtr m_mediaData;
    QnAbstractMediaDataPtr m_nextDataPacket;
    nx::utils::ByteArray* m_nextDataPacketBuffer;
    qint64 m_position;
    bool m_isAudioEnabled;
    qint64 m_lastFramePtsUs; //< Intended for debug.
    nx::analytics::MetadataLogger m_primaryLogger;
    nx::analytics::MetadataLogger m_secondaryLogger;
};

using QnNxRtpParserPtr = QSharedPointer<QnNxRtpParser>;

} // namespace nx::rtp
