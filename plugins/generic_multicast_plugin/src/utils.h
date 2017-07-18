#pragma once

#include <libavcodec/avcodec.h>
#include <plugins/camera_plugin.h>


extern "C" {

int readFromIoDevice(void* opaque, uint8_t* buf, int size);

int checkIoDevice(void* opaque);

} // extern "C"

nxcip::AudioFormat::SampleType fromFfmpegSampleFormatToNx(AVSampleFormat avFormat);
nxcip::CompressionType fromFfmpegCodecIdToNx(AVCodecID codecId);
