// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_audio_decoder.h"
#include "ffmpeg_audio_decoder.h"

QnAbstractAudioDecoder* QnAudioDecoderFactory::createDecoder(QnCompressedAudioDataPtr data)
{
    auto result = new QnFfmpegAudioDecoder(data);
    if (!result->isInitialized())
    {
        delete result;
        return nullptr;
    }
    return result;
}
