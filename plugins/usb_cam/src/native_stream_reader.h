#pragma once

#include "stream_reader.h"

namespace nx {
namespace usb_cam {

//!Transfers or transcodes packets from USB webcameras and streams them
class NativeStreamReader : public InternalStreamReader
{
public:
    NativeStreamReader(
        int encoderIndex,
        nxpl::TimeProvider *const timeProvider,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~NativeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
};

} // namespace usb_cam
} // namespace nx
