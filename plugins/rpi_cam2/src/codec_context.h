#pragma once

#include <string>

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
        int fps, 
        int bitrate);

    void setCodecID(nxcip::CompressionType codecID);
    nxcip::CompressionType codecID() const;

    void setResolution(const nxcip::Resolution& resolution);
    const nxcip::Resolution& resolution() const;
    bool resolutionValid() const;

    void setFps(int fps);
    int fps() const;

    //set the bitrate in bits per second
    void setBitrate(int bitratebps);

    //the bitrate in bits per second
    int bitrate() const;

    std::string toString() const;

private:
    nxcip::CompressionType m_codecID;
    nxcip::Resolution m_resolution;
    int m_fps;
    int m_bitrate;
};

} // namespace webcam_plugin
} // namespace nx

