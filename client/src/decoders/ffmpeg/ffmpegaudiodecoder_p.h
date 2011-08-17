#ifndef FFMPEGAUDIODECODER_P_H
#define FFMPEGAUDIODECODER_P_H

#include "decoders/abstractaudiodecoder.h"

struct AVCodec;
struct AVCodecContext;

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// (see comment to decode functions for details).

class CLFFmpegAudioDecoder : public CLAbstractAudioDecoder
{
public:
    CLFFmpegAudioDecoder(QnCompressedAudioDataPtr data);
    ~CLFFmpegAudioDecoder();

    bool decode(CLAudioData &data);

private:
    AVCodec *m_codec;
    AVCodecContext *m_context;
};

#endif // FFMPEGAUDIODECODER_P_H
