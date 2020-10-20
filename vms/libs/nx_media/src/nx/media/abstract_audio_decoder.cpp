#include "abstract_audio_decoder.h"

namespace nx {

AudioFrame::AudioFrame():
    data(CL_MEDIA_ALIGNMENT, 0, AV_INPUT_BUFFER_PADDING_SIZE),
    timestampUsec(0)
{
}

}
