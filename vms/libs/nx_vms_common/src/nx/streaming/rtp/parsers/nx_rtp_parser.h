// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QFile>

#include <nx/streaming/rtp/result.h>
#include <nx/analytics/metadata_logger.h>

namespace nx::streaming::rtp {

class NX_VMS_COMMON_API QnNxRtpParser
{
public:
    /** @param tag Human-readable tag for logging. */
    QnNxRtpParser(QnUuid deviceId, const QString& tag);

    Result processData(const uint8_t* data, int size, bool& gotData);
    QnAbstractMediaDataPtr nextData();

    qint64 position() const { return m_position; }
    CodecParametersConstPtr mediaContext() const { return m_context; }

    /**
     * Don't parse incoming audio packets if value is false.
     */
    void setAudioEnabled(bool value);

private:
    void logMediaData(const QnAbstractMediaDataPtr& metadata);

private:
    const QnUuid m_deviceId;
    CodecParametersConstPtr m_context;
    QnAbstractMediaDataPtr m_mediaData;
    QnAbstractMediaDataPtr m_nextDataPacket;
    QnByteArray* m_nextDataPacketBuffer;
    qint64 m_position;
    bool m_isAudioEnabled;
    qint64 m_lastFramePtsUs; //< Intended for debug.
    nx::analytics::MetadataLogger m_primaryLogger;
    nx::analytics::MetadataLogger m_secondaryLogger;
};

using QnNxRtpParserPtr = QSharedPointer<QnNxRtpParser>;

} // namespace nx::streaming::rtp
