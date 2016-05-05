#ifndef FFMPEG_AUDIO_DECODER_H
#define FFMPEG_AUDIO_DECODER_H

#ifdef ENABLE_DATA_PROVIDERS

#include "abstract_audio_decoder.h"

/**
 * Client of this class is responsible for encoded data buffer meet ffmpeg 
 * restrictions (see the comments for decoding functions for details).
 */
class QnFfmpegAudioDecoder : public QnAbstractAudioDecoder
{
public:
    QnFfmpegAudioDecoder(QnCompressedAudioDataPtr data);
    bool decode(QnCompressedAudioDataPtr& data, QnByteArray& result);
    ~QnFfmpegAudioDecoder();

    static AVSampleFormat audioFormatQtToFfmpeg(const QnAudioFormat& fmt);

private:
    AVCodec *codec;
    AVCodecContext *c; // TODO: #vasilenko please name these members properly

    static bool m_first_instance;
    CodecID m_codec;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // FFMPEG_AUDIO_DECODER_H
