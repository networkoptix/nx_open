#include "abstract_audio_decoder.h"

namespace nx {

AudioFrame::AudioFrame():
    data(nx::media::kMediaAlignment, 0),
    timestampUsec(0)
{
}

}
