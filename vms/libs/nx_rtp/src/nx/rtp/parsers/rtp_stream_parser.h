// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <optional>

#include <nx/media/media_data_packet.h>
#include <nx/rtp/result.h>
#include <nx/rtp/rtp.h>
#include <nx/rtp/sdp.h>

namespace nx::rtp {

class NX_RTP_API StreamParser
{
public:
    virtual ~StreamParser() = default;

    virtual void setSdpInfo(const Sdp::Media& sdp) = 0;
    virtual QnAbstractMediaDataPtr nextData() = 0;
    virtual Result processRtpExtension(
        const RtpHeaderExtensionHeader& /*extensionHeader*/,
        quint8* /*data*/,
        int /*size*/) { return true; }
    virtual Result processData(
        const RtpHeader& header,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) = 0;

    virtual void clear() = 0;

    virtual bool forceProcessEmptyData() const { return false; }

    int getFrequency() const { return m_frequency; };

protected:
    void setFrequency(int frequency) { m_frequency = frequency; }

private:
    int m_frequency = 0;
};

using StreamParserPtr = std::unique_ptr<nx::rtp::StreamParser>;

class NX_RTP_API VideoStreamParser: public StreamParser
{

public:
    VideoStreamParser();
    virtual ~VideoStreamParser() = default;
    virtual QnAbstractMediaDataPtr nextData() override;

protected:
    struct Chunk
    {
        Chunk() = default;
        Chunk(int bufferOffset, quint16 len, quint8 nalStart = false):
            bufferStart(nullptr),
            bufferOffset(bufferOffset),
            len(len),
            nalStart(nalStart)
        {
        }

        quint8* bufferStart = nullptr;
        int bufferOffset = 0;
        quint16 len = 0;
        bool nalStart = false;
    };

protected:
    void backupCurrentData(const quint8* currentBufferBase);

protected:
    QnAbstractMediaDataPtr m_mediaData;
    std::vector<Chunk> m_chunks;
    std::vector<quint8> m_nextFrameChunksBuffer;
};

class NX_RTP_API AudioStreamParser: public StreamParser
{

public:
    virtual CodecParametersConstPtr getCodecParameters() = 0;
    virtual QnAbstractMediaDataPtr nextData() override;

protected:
    std::deque<QnAbstractMediaDataPtr> m_audioData;
};

} // namespace nx::rtp
