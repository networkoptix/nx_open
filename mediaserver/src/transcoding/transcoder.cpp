#include "transcoder.h"

QnTranscoder::QnTranscoder():
    m_videoStreamCopy(false),
    m_audioStreamCopy(false),
    m_isVideoPresent(false),
    m_isAudioPresent(false),
    m_initialized(false)
{

}

QnTranscoder::~QnTranscoder()
{

}

bool QnTranscoder::setVideoCodec(CodecID codec, bool directStreamCopy, int width, int height, int bitrate, const Params& params)
{
    m_videoStreamCopy = directStreamCopy;
    m_isVideoPresent = true;
    return true;
}

bool QnTranscoder::setAudioCodec(CodecID codec, bool directStreamCopy, int bitrate, const Params& params)
{
    m_audioStreamCopy = directStreamCopy;
    m_isAudioPresent = true;
    return true;
}

int QnTranscoder::transcodePacket(QnAbstractMediaDataPtr media, QnByteArray& result)
{
    if (media->dataType != QnAbstractMediaData::VIDEO && media->dataType != QnAbstractMediaData::AUDIO)
        return 0; // transcode only audio and video, skip packet

    if (!m_initialized)
    {
        if (media->dataType == QnAbstractMediaData::VIDEO)
            m_delayedVideoQueue << qSharedPointerDynamicCast<QnCompressedVideoData> (media);
        else
            m_delayedAudioQueue << qSharedPointerDynamicCast<QnCompressedAudioData> (media);
        if (m_isVideoPresent && m_videoStreamCopy && m_delayedVideoQueue.isEmpty())
            return 0; // not ready to init
        if (m_isAudioPresent && m_audioStreamCopy && m_delayedAudioQueue.isEmpty())
            return 0; // not ready to init
        int rez = open(m_delayedVideoQueue.isEmpty() ? QnCompressedVideoDataPtr() : m_delayedVideoQueue.first(), 
                       m_delayedAudioQueue.isEmpty() ? QnCompressedAudioDataPtr() : m_delayedAudioQueue.first());
        if (rez != 0)
            return rez;
        m_initialized = true;
    }
    result.clear();
    int errCode = 0;
    while (!m_delayedVideoQueue.isEmpty()) {
        errCode = transcodePacketInternal(m_delayedVideoQueue.dequeue(), result);
        if (errCode != 0)
            return errCode;
    }
    while (!m_delayedAudioQueue.isEmpty()) {
        errCode = transcodePacketInternal(m_delayedAudioQueue.dequeue(), result);
        if (errCode != 0)
            return errCode;
    }
    errCode = transcodePacketInternal(media, result);
    return errCode;
}
