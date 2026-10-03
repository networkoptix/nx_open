// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_audio_decoder.h"

namespace nx {

AudioFrame::AudioFrame():
    data(/*capacity*/ 0, CL_MEDIA_ALIGNMENT, AV_INPUT_BUFFER_PADDING_SIZE),
    timestampUsec(0)
{
}

}
