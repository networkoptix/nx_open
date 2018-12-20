#pragma once

#include <deque>

#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/sdp.h>
#include <core/resource/resource_media_layout.h>

namespace nx::streaming::rtp {

class QnRtspAudioLayout: public QnResourceAudioLayout
{

public:
    virtual int channelCount() const override;
    virtual AudioTrack getAudioTrackInfo(int index) const override;
    void setAudioTrackInfo(const AudioTrack& info);

private:
    AudioTrack m_audioTrack;
};

class StreamParser: public QObject
{
    Q_OBJECT

public:
    virtual ~StreamParser() {};

    virtual void setSdpInfo(const Sdp::Media& sdp) = 0;
    virtual QnAbstractMediaDataPtr nextData() = 0;
    virtual bool processData(
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) = 0;

    int getFrequency() { return m_frequency; };

signals:
    void packetLostDetected(quint32 prev, quint32 next);
protected:
    void setFrequency(int frequency) { m_frequency = frequency; }

private:
    int m_frequency = 0;
};

class VideoStreamParser: public StreamParser
{

public:
    VideoStreamParser();
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

class AudioStreamParser: public StreamParser
{

public:
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() = 0;
    virtual QnAbstractMediaDataPtr nextData() override;

protected:
    std::deque<QnAbstractMediaDataPtr> m_audioData;
};

} // namespace nx::streaming::rtp
