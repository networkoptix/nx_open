#pragma once

#include <vector>

#include "camera/camera_plugin_types.h"

namespace nx {
namespace webcam_plugin {

class VideoCodecPriority
{
public:
    VideoCodecPriority();

    /*!
     * get the first codec in the given list matching an internal list of codecs
     */
    nxcip::CompressionType getPriorityCodec(
        const std::vector<nxcip::CompressionType>& codecList);

private:
    std::vector<nxcip::CompressionType> m_codecPriorityList;
};

} // namespace webcam_plugin
} // namespace nx

