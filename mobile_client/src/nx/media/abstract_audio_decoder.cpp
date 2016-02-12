#include "abstract_audio_decoder.h"

namespace nx {
namespace media {


AudioFrame::AudioFrame():
    data(kMediaAlignment, 0),
    timestampUsec(0)
{
}

}
}
