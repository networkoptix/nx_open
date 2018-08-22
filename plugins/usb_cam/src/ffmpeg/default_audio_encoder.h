#pragma once 

#include <memory>

#include "ffmpeg/codec.h"

namespace nx {
namespace ffmpeg {

std::unique_ptr<Codec> getDefaultMp3Encoder(int * outFfmpegError);

} // namespace ffmpeg
} // namespace nx