#include "transcoder.h"

// ---------------------------- QnCodecTranscoder ------------------

void QnCodecTranscoder::setParams(const Params& params)
{
    m_params = params;
}

void QnCodecTranscoder::setBitrate(int value)
{
    m_bitrate = value;
}

AVCodecContext* QnCodecTranscoder::getCodecContext()
{
    return 0;
}

// --------------------------- QnVideoTranscoder -----------------

void QnVideoTranscoder::setSize(const QSize& size)
{
    m_size = size;
}

QSize QnVideoTranscoder::getSize() const
{
    return m_size;
}

// ---------------------- QnTranscoder -------------------------

QnTranscoder::QnTranscoder():
    m_initialized(false),
    m_videoCodec(CODEC_ID_NONE),
    m_audioCodec(CODEC_ID_NONE)
{

}

QnTranscoder::~QnTranscoder()
{

}

bool QnTranscoder::setVideoCodec(CodecID codec, QnVideoTranscoderPtr vTranscoder)
{
    m_videoCodec = codec;
    m_vTranscoder = vTranscoder;
    return true;
}

bool QnTranscoder::setAudioCodec(CodecID codec, QnAudioTranscoderPtr aTranscoder)
{
    m_aTranscoder = aTranscoder;
    m_audioCodec = codec;
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
        if (m_videoCodec != CODEC_ID_NONE && m_delayedVideoQueue.isEmpty())
            return 0; // not ready to init
        if (m_audioCodec != CODEC_ID_NONE && m_delayedAudioQueue.isEmpty())
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
