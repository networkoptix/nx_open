#pragma once 

#include <memory>

#include "ffmpeg/codec.h"

namespace nx {
namespace usb_cam {

std::unique_ptr<ffmpeg::Codec> getDefaultMp3Encoder(int * outFfmpegError);

}
}