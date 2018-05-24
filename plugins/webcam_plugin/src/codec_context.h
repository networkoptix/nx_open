#pragma once

#include <camera/camera_plugin.h>

namespace nx {
namespace webcam_plugin {

class CodecContext    
{
public:
    CodecContext();
    CodecContext(
        nxcip::CompressionType codecID, 
        const nxcip::Resolution &resolution,
        float fps, 
        int64_t bitrate);

    void setCodecID(nxcip::CompressionType codecID);
    nxcip::CompressionType codecID() const;

    void setResolution(const nxcip::Resolution& resolution);
    const nxcip::Resolution& resolution() const;
    bool resolutionValid() const;

    void setFps(float fps);
    float fps() const;

    //set the bitrate in bits per second
    void setBitrate(int64_t bitratebps);

    //the bitrate in bits per second
    int64_t bitrate() const;

private:
    nxcip::CompressionType m_codecID;
    nxcip::Resolution m_resolution;
    float m_fps;
    int64_t m_bitrate;
};

} // namespace webcam_plugin
} // namespace nx

