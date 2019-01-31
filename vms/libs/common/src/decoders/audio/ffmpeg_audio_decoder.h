#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include "abstract_audio_decoder.h"
#include <utils/media/ffmpeg_helper.h>

/**
 * Client of this class is responsible for encoded data buffer meet ffmpeg restrictions (see the
 * comments for decoding functions for details).
 */
class QnFfmpegAudioDecoder: public QnAbstractAudioDecoder
{
public:
    QnFfmpegAudioDecoder(QnCompressedAudioDataPtr data);
    virtual ~QnFfmpegAudioDecoder() override;

    virtual bool decode(QnCompressedAudioDataPtr& data, QnByteArray& result) override;

    bool isInitialized() const;

private:
    AVCodec* codec = nullptr;
    AVCodecContext* m_audioDecoderCtx = nullptr;

    static bool m_first_instance;
    bool m_initialized = false;
    AVCodecID m_codec = AV_CODEC_ID_NONE;
    AVFrame* m_outFrame = nullptr;
    std::unique_ptr<QnFfmpegAudioHelper> m_audioHelper;
};

#endif // ENABLE_DATA_PROVIDERS
