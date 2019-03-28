#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QFile>

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>
#include <nx/debugging/abstract_visual_metadata_debugger.h>
#include <nx/analytics/metadata_logger.h>

namespace nx::streaming::rtp {

class QnNxRtpParser: public VideoStreamParser
{
public:
    /** @param debugSourceId Human-readable stream source id for logging. */
    QnNxRtpParser(QnUuid devivceId);

    virtual void setSdpInfo(const Sdp::Media& /*sdp*/) override {};
    virtual bool processData(quint8* rtpBufferBase, int bufferOffset, int readed, bool& gotData) override;

    qint64 position() const { return m_position; }
    QnConstMediaContextPtr mediaContext() const { return m_context; }

    /**
     * Don't parse incoming audio packets if value is false.
     */
    void setAudioEnabled(bool value);

private:
    void logMediaData(const QnAbstractMediaDataPtr& metadata);

private:
    const QnUuid m_deviceId;
    QnConstMediaContextPtr m_context;
    QnAbstractMediaDataPtr m_nextDataPacket;
    QnByteArray* m_nextDataPacketBuffer;
    qint64 m_position;
    bool m_isAudioEnabled;
    qint64 m_lastFramePtsUs; //< Intended for debug.
    nx::debugging::VisualMetadataDebuggerPtr m_visualDebugger;
    nx::analytics::MetadataLogger m_logger;
};

using QnNxRtpParserPtr = QSharedPointer<QnNxRtpParser>;

} // namespace nx::streaming::rtp
