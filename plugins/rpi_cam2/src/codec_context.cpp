#include "codec_context.h"
#include "ffmpeg/utils.h"

namespace nx {
namespace webcam_plugin {

CodecContext::CodecContext():
    m_codecID(nxcip::AV_CODEC_ID_NONE),
    m_fps(0),
    m_bitrate(0)
{
}

CodecContext::CodecContext (
    nxcip::CompressionType codecID, 
    const nxcip::Resolution &resolution,
    int fps,
    int bitrate)
    :
    m_codecID(codecID),
    m_resolution(resolution),
    m_fps(fps),
    m_bitrate(bitrate)
{
}

void CodecContext::setCodecID (nxcip::CompressionType codecID)
{
    m_codecID = codecID;
}

nxcip::CompressionType CodecContext::codecID () const
{
    return m_codecID;
}

void CodecContext::setResolution (const nxcip::Resolution &resolution)
{
    m_resolution = resolution;
}

const nxcip::Resolution & CodecContext::resolution () const
{
    return m_resolution;
}

bool CodecContext::resolutionValid() const
{
    return m_resolution.width > 0
        && m_resolution.height > 0;
}

void CodecContext::setFps (int fps)
{
    m_fps = fps;
}

int CodecContext::fps () const
{
    return m_fps;
}

void CodecContext::setBitrate(int bitrate)
{
    m_bitrate = bitrate;
}

int CodecContext::bitrate () const
{
    return m_bitrate;
}

std::string CodecContext::toString() const
{
    std::string resolutionStr = 
        std::string("width: ") + std::to_string(m_resolution.width) +
        ", height: " + std::to_string(m_resolution.height);

    return std::string("codecID: ") + ffmpeg::utils::avCodecIDStr(ffmpeg::utils::toAVCodecID(m_codecID)) + 
        ", " + resolutionStr + 
        ", fps: " + std::to_string(m_fps) +
        ", bitrate: " + std::to_string(m_bitrate);
}

} // namespace webcam_plugin
} // namespace nx