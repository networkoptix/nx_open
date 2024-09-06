// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QMap>

#include <nx/analytics/metadata_logger.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/rtp/result.h>
#include <nx/utils/software_version.h>

namespace nx::rtp {

class NX_VMS_COMMON_API QnNxRtpParser: public VideoStreamParser
{
    using base_type = VideoStreamParser;
public:
    /** @param tag Human-readable tag for logging. */
    QnNxRtpParser(nx::Uuid deviceId, const QString& tag);
    QnNxRtpParser();

    virtual void setSdpInfo(const Sdp::Media& /*sdp*/) override {}

    virtual bool isUtcTime() const override { return true; }

    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;
    virtual void clear() override {}

    Result processData(const quint8* data, int size, bool& gotData);

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
    Result processData(
        const RtpHeader& rtpHeader, const uint8_t* payload,
        int dataSize, bool& gotData);


private:
    nx::utils::SoftwareVersion m_serverVersion;
    CodecParametersConstPtr m_context;
    QnAbstractMediaDataPtr m_mediaData;
    QnAbstractMediaDataPtr m_nextDataPacket;
    nx::utils::ByteArray* m_nextDataPacketBuffer = nullptr;
    qint64 m_position = AV_NOPTS_VALUE;
    bool m_isAudioEnabled = true;
    qint64 m_lastFramePtsUs; //< Intended for debug.
    std::unique_ptr<nx::analytics::MetadataLogger> m_primaryLogger;
    std::unique_ptr<nx::analytics::MetadataLogger> m_secondaryLogger;
};

using QnNxRtpParserPtr = QSharedPointer<QnNxRtpParser>;

} // namespace nx::rtp
