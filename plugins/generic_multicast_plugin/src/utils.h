#pragma once

#include <libavcodec/avcodec.h>
#include <plugins/camera_plugin.h>


extern "C" {

int readFromQIODevice(void* opaque, uint8_t* buf, int size);

} // extern "C"

nxcip::AudioFormat::SampleType fromFfmpegSampleFormatToNx(AVSampleFormat avFormat);
nxcip::CompressionType fromFfmpegCodecIdToNx(AVCodecID codecId);
