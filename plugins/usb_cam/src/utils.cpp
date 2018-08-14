#include "utils.h"

#include <nx/utils/url.h>

#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {
namespace utils {

namespace {

const std::string WEBCAM_URL_PREFIX = "webcam://";

const std::vector<nxcip::CompressionType> VIDEO_CODEC_PRIORITY_LIST =
{
    nxcip::AV_CODEC_ID_H264,
    nxcip::AV_CODEC_ID_H263,
    nxcip::AV_CODEC_ID_MJPEG
};

int getPriorityCodec(
    const std::vector<std::shared_ptr<device::AbstractCompressionTypeDescriptor>>& codecDescriptorList)
{
    for (const auto codecID : VIDEO_CODEC_PRIORITY_LIST)
    {
        int index = 0;
        for(const auto& descriptor : codecDescriptorList)
        {
            if (codecID == descriptor->toNxCompressionType())
                return index;
            ++index;
        }
                
    }
    return codecDescriptorList.size();
}

} // namespace 

std::string decodeCameraInfoUrl(const char * url)
{
    std::string urlStr(url);
    return urlStr.length() > WEBCAM_URL_PREFIX.length() 
        ? urlStr.substr(WEBCAM_URL_PREFIX.length())
        : urlStr;
    //QString encodeUrl = QString(url).mid(WEBCAM_URL_PREFIX.length()); /* webcam:// == 9 chars */
    //return nx::utils::Url::fromPercentEncoding(encodeUrl.toLatin1()).toStdString();
}

std::string encodeCameraInfoUrl(const char * url)
{
    return WEBCAM_URL_PREFIX + url;
    //return QByteArray(WEBCAM_URL_PREFIX.c_str()).append(nx::utils::Url::toPercentEncoding(url)).data();
}

std::vector<AVCodecID> ffmpegCodecPriorityList()
{
    std::vector<AVCodecID> ffmpegCodecList;
    for(const auto & nxCodecID : VIDEO_CODEC_PRIORITY_LIST)
        ffmpegCodecList.push_back(ffmpeg::utils::toAVCodecID(nxCodecID));
    return ffmpegCodecList;
}

std::shared_ptr<device::AbstractCompressionTypeDescriptor> getPriorityDescriptor(
    const std::vector<std::shared_ptr<device::AbstractCompressionTypeDescriptor>>& codecDescriptorList)
{
    int index = getPriorityCodec(codecDescriptorList);
    std::shared_ptr<device::AbstractCompressionTypeDescriptor>descriptor = nullptr;
    if (index < codecDescriptorList.size())
        descriptor = codecDescriptorList[index];
    else if (codecDescriptorList.size() > 0)
        descriptor = codecDescriptorList[0];
    return descriptor;
}

} // namespace utils
} // namespace usb_cam
} // namespace nx