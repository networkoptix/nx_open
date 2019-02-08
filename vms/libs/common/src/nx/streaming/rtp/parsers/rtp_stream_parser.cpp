#include "rtp_stream_parser.h"

namespace nx::streaming::rtp {

namespace {

static const int kDefaultChunkContainerSize = 1024;
static const int kDefaultChannelCount = 1;

} // namespace

VideoStreamParser::VideoStreamParser()
{
    m_chunks.reserve(kDefaultChunkContainerSize);
}

QnAbstractMediaDataPtr VideoStreamParser::nextData()
{
    if (m_mediaData)
    {
        QnAbstractMediaDataPtr result;
        result.swap( m_mediaData );
        return result;
    }
    else
    {
        return QnAbstractMediaDataPtr();
    }
}

void VideoStreamParser::backupCurrentData(const quint8* currentBufferBase)
{
    size_t chunksLength = 0;
    for (const auto& chunk: m_chunks)
        chunksLength += chunk.len;

    m_nextFrameChunksBuffer.resize(chunksLength);

    size_t offset = 0;
    quint8* nextFrameBufRaw = m_nextFrameChunksBuffer.data();
    for (auto& chunk: m_chunks)
    {
        memcpy(nextFrameBufRaw + offset, currentBufferBase + chunk.bufferOffset, chunk.len);
        chunk.bufferStart = nextFrameBufRaw;
        chunk.bufferOffset = (int)offset;
        offset += chunk.len;
    }
}

QnAbstractMediaDataPtr AudioStreamParser::nextData()
{
    if (m_audioData.empty())
    {
        return QnAbstractMediaDataPtr();
    }
    else
    {
        auto result = m_audioData.front();
        m_audioData.pop_front();
        return result;
    }
}

int QnRtspAudioLayout::channelCount() const
{
    return kDefaultChannelCount;
}

QnResourceAudioLayout::AudioTrack QnRtspAudioLayout::getAudioTrackInfo(int /*index*/) const
{
    return m_audioTrack;
}

void QnRtspAudioLayout::setAudioTrackInfo(const AudioTrack& info)
{
    m_audioTrack = info;
}

} // namespace nx::streaming::rtp
