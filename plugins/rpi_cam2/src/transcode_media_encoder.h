#pragma once

#include "media_encoder.h"

#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "native_media_encoder.h"
#include "transcode_stream_reader.h"
#include "device_data.h"
#include "codec_context.h"

namespace nx {
namespace webcam_plugin {

namespace ffmpeg { class StreamReader; }

class CameraManager;

class TranscodeMediaEncoder
:
    public MediaEncoder
{
public:
    TranscodeMediaEncoder(
        CameraManager* const cameraManager, 
        nxpl::TimeProvider *const timeProvider,
        const CodecContext& codecContext,
        const std::weak_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);

    virtual ~TranscodeMediaEncoder();

    virtual nxcip::StreamReader* getLiveStreamReader() override;

private:
    std::weak_ptr<NativeMediaEncoder> m_nativeMediaEncoder;
    std::weak_ptr<NativeStreamReader> m_nativeStreamReader;
};

} // namespace nx 
} // namespace webcam_plugin 
