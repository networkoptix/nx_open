#ifndef FFMPEG_AUDIO_DECODER_H
#define FFMPEG_AUDIO_DECODER_H

#ifdef ENABLE_DATA_PROVIDERS

#include "abstract_audio_decoder.h"
#include <utils/media/ffmpeg_helper.h>

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
    AVCodec* codec;
    AVCodecContext* m_audioDecoderCtx;

    static bool m_first_instance;
    AVCodecID m_codec;
    AVFrame* m_outFrame;
    std::unique_ptr<QnFfmpegAudioHelper> m_audioHelper;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // FFMPEG_AUDIO_DECODER_H
