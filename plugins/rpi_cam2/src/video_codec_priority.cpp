#include "video_codec_priority.h"
#include <algorithm>

namespace nx {
namespace webcam_plugin {

VideoCodecPriority::VideoCodecPriority():
    m_codecPriorityList({
        //nxcip::AV_CODEC_ID_H264
        //,nxcip::AV_CODEC_ID_MJPEG
    })
{
}

nxcip::CompressionType VideoCodecPriority::getPriorityCodec(
    const std::vector<nxcip::CompressionType>& codecList)
{
    for (const auto & codecID : m_codecPriorityList)
    {
        if (std::find(codecList.begin(), codecList.end(), codecID) != codecList.end())
            return codecID;
    }
    return nxcip::AV_CODEC_ID_NONE;
}

} // namespace webcam_plugin
} // namespace nx

