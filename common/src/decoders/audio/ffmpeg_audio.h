#ifndef cl_ffmpeg_h
#define cl_ffmpeg_h

#ifdef ENABLE_DATA_PROVIDERS

#include "abstractaudiodecoder.h"

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// ( see comment to decode functions for details ).

class CLFFmpegAudioDecoder : public CLAbstractAudioDecoder
{
public:
    CLFFmpegAudioDecoder(QnCompressedAudioDataPtr data);
    bool decode(QnCompressedAudioDataPtr& data, QnByteArray& result);
    ~CLFFmpegAudioDecoder();

    static AVSampleFormat audioFormatQtToFfmpeg(const QnAudioFormat& fmt);

private:
    AVCodec *codec;
    AVCodecContext *c; // TODO: #vasilenko please name these members properly

    static bool m_first_instance;
    CodecID m_codec;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //cl_ffmpeg_h

