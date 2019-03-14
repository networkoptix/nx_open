#pragma once

#include <memory>

#include "ffmpeg/codec.h"

namespace nx {
namespace usb_cam {

/**
 * Allocates a default audio encoder (AAC) used by the plugin, setting the relevant fields, and
 * calls ffmpeg::Codec::initializeEncoder() on it. Using the encoder requires a call to
 * ffmpeg::Codec::open().
 *
 * @params[out] outFFmpegError - the return value from the call to initializeEncoder():
 *    0 on success, negative value on failure.
 * @return - the initialized encoder
 */
std::unique_ptr<ffmpeg::Codec> getDefaultAudioEncoder(int * outFfmpegError);

} // namespace usb_cam
} // namespace nx
