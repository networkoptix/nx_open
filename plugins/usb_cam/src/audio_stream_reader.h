#pragma once

#include <memory>
#include <string>

#include "ffmpeg/input_format.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx { namespace ffmpeg { class InputFormat; }}
namespace nx { namespace ffmpeg { class Codec; }}
namespace nx { namespace ffmpeg { class Packet; }}

namespace nxpl { class TimeProvider; }

namespace nx {
namespace usb_cam {

class AudioStreamReader
{
public:
    AudioStreamReader(const std::string& url, nxpl::TimeProvider * timeProvider);
    ~AudioStreamReader();

    int getNextData(ffmpeg::Packet * packet);

private:    
    std::string m_url;
    nxpl::TimeProvider * m_timeProvider;
    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
    std::unique_ptr<ffmpeg::Codec> m_decoder;
    std::unique_ptr<ffmpeg::Codec> m_encoder;
    
    bool m_initialized;
    int m_initCode;
    int m_retries;

private:
    int initialize();
    void uninitialize();
    bool ensureInitialized();
    int initializeInputFormat();
    int initializeDecoder();
    int initializeEncoder();
    int decodeNextFrame(AVFrame * outFrame) const;
    int encode(AVPacket * outPacket, const AVFrame * frame) const;
};

} //namespace usb_cam
} //namespace nx