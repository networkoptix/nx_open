#ifdef __linux__

#include "v4l2_utils.h"

#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace v4l2 {

std::vector<std::string> getDevicePaths();
nxcip::CompressionType toNxCompressionTypeVideo(unsigned int v4l2PixelFormat);
unsigned int toV4L2PixelFormat(nxcip::CompressionType nxCodecID);

/*!
 * convenience class for opening and closing devices represented by devicePath
 */
class DeviceInitializer
{
public:
    DeviceInitializer(const std::string& devicePath):
        m_fileDescriptor(open(devicePath.c_str(), O_RDWR))
    {
    }

    ~DeviceInitializer()
    {
        if(m_fileDescriptor != -1)
            close(m_fileDescriptor);
    }

    int fileDescriptor()
    {
        return m_fileDescriptor;
    }

private:
    int m_fileDescriptor;
};

nxcip::CompressionType toNxCompressionTypeVideo(unsigned int v4l2PixelFormat)
{
    switch(v4l2PixelFormat)
    {
        case V4L2_PIX_FMT_MPEG2:
            return nxcip::AV_CODEC_ID_MPEG2VIDEO;
        case V4L2_PIX_FMT_H263:
            return nxcip::AV_CODEC_ID_H263;
        case V4L2_PIX_FMT_MJPEG:
            return nxcip::AV_CODEC_ID_MJPEG;
        case V4L2_PIX_FMT_MPEG4:
            return nxcip::AV_CODEC_ID_MPEG4;
        case V4L2_PIX_FMT_H264:
        case V4L2_PIX_FMT_H264_NO_SC:
        case V4L2_PIX_FMT_H264_MVC:
            return nxcip::AV_CODEC_ID_H264;
        default:
            return nxcip::AV_CODEC_ID_NONE;
    }
}

unsigned int toV4L2PixelFormat(nxcip::CompressionType nxCodecID)
{
    switch(nxCodecID)
    {
        case nxcip::AV_CODEC_ID_MPEG2VIDEO:
            return V4L2_PIX_FMT_MPEG2;
        case nxcip::AV_CODEC_ID_H263:
            return V4L2_PIX_FMT_H263;
        case nxcip::AV_CODEC_ID_MJPEG:
            return V4L2_PIX_FMT_MJPEG;
        case nxcip::AV_CODEC_ID_MPEG4:
            return V4L2_PIX_FMT_MPEG4;
        case nxcip::AV_CODEC_ID_H264:
            return V4L2_PIX_FMT_H264;
        default:
            return 0;
    }
}

std::vector<std::string> getDevicePaths()
{
    const auto isDeviceFile =
        [](const char *path)
        {
            struct stat buffer;
            stat(path, &buffer);
            return S_ISCHR(buffer.st_mode);
        };

    std::vector<std::string> deviceList;

    std::string dev("/dev");
    DIR *directory = opendir(dev.c_str());
    if (!directory)
        return deviceList;

    struct dirent *directoryEntry;
    while ((directoryEntry = readdir(directory)) != NULL)
    {
        if (!strstr(directoryEntry->d_name, "video"))
            continue;

        std::string devVideo = dev + "/" + directoryEntry->d_name;
        if (isDeviceFile(devVideo.c_str()))
            deviceList.push_back(devVideo);
    }

    closedir(directory);
    return deviceList;
}

std::vector<DeviceData> getDeviceList()
{
    const auto getDeviceName =
        [](int fileDescriptor)
        {
            struct v4l2_capability deviceCapability;
            if (ioctl(fileDescriptor, VIDIOC_QUERYCAP, &deviceCapability) == -1)
                return std::string();
            return std::string(reinterpret_cast<char*> (deviceCapability.card));
        };


    std::vector<std::string> devicePaths = getDevicePaths();
    std::vector<DeviceData> deviceList;
    if (devicePaths.empty())
        return deviceList;

    for (const auto& devicePath : devicePaths)
    {
        DeviceInitializer initializer(devicePath);
        int fileDescriptor = initializer.fileDescriptor();
        if (fileDescriptor == -1)
            continue;

        DeviceData d;
        d.setDeviceName(getDeviceName(fileDescriptor));
        d.setDevicePath(devicePath);
        deviceList.push_back(d);
    }

    return deviceList;
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath)
{
    DeviceInitializer initializer(devicePath);
    if (initializer.fileDescriptor() == -1)
        return std::vector<nxcip::CompressionType>();

    std::vector<nxcip::CompressionType> codecList;
    struct v4l2_fmtdesc formatEnum;
    memset(&formatEnum, 0, sizeof(formatEnum));
    formatEnum.index = 0;
    formatEnum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int fileDescriptor = initializer.fileDescriptor();
    while (ioctl(fileDescriptor, VIDIOC_ENUM_FMT, &formatEnum) == 0)
    {
        nxcip::CompressionType nxCodecID = toNxCompressionTypeVideo(formatEnum.pixelformat);
        if (std::find(codecList.begin(), codecList.end(), nxCodecID) == codecList.end())
            codecList.push_back(nxCodecID);
        ++formatEnum.index;
    }

    return codecList;
}

float getHighestFrameRate(
    int fileDescriptor,
    unsigned int v4l2PixelFormat,
    const nxcip::Resolution& resolution)
{
    const auto toFrameRate =
        [] (const v4l2_fract& frameInterval) -> float
        {
            return frameInterval.numerator == 0
            ? 0
            : (frameInterval.denominator / frameInterval.numerator);
        };

    struct v4l2_frmivalenum frameRateEnum;
    memset(&frameRateEnum, 0, sizeof(frameRateEnum));
    frameRateEnum.index = 0;
    frameRateEnum.pixel_format = v4l2PixelFormat;
    frameRateEnum.width = resolution.width;
    frameRateEnum.height = resolution.height;

    float highestFrameRate = 0;
    while(ioctl(fileDescriptor, VIDIOC_ENUM_FRAMEINTERVALS, &frameRateEnum) == 0)
    {
        struct v4l2_fract v4l2FrameRate = frameRateEnum.type == V4L2_FRMIVAL_TYPE_DISCRETE
            ? frameRateEnum.discrete
            : frameRateEnum.stepwise.min;

        float frameRate = toFrameRate(v4l2FrameRate);
        if(highestFrameRate < frameRate)
            highestFrameRate = frameRate;

        if(frameRateEnum.type != V4L2_FRMIVAL_TYPE_DISCRETE)
            break;

        ++frameRateEnum.index;
    }
    return highestFrameRate;
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID)
{
    const auto getResolution =
        [](const v4l2_frmsizeenum& enumerator) -> nxcip::Resolution
        {
            return enumerator.type == V4L2_FRMSIZE_TYPE_DISCRETE
                ? nxcip::Resolution(enumerator.discrete.width, enumerator.discrete.height)
                : nxcip::Resolution(enumerator.stepwise.max_width, enumerator.stepwise.max_height);
        };

    DeviceInitializer initializer(devicePath);
    if (initializer.fileDescriptor() == -1)
        return {};

    std::vector<ResolutionData> resolutionList;

    int pixelFormat = toV4L2PixelFormat(targetCodecID);

    if (pixelFormat == 0)
        return {};

    struct v4l2_frmsizeenum frameSizeEnum;
    memset(&frameSizeEnum, 0, sizeof(frameSizeEnum));
    frameSizeEnum.index = 0;
    frameSizeEnum.pixel_format = pixelFormat;

    int fileDescriptor = initializer.fileDescriptor();
    while (ioctl(fileDescriptor, VIDIOC_ENUM_FRAMESIZES, &frameSizeEnum) == 0)
    {
        ResolutionData resolutionData;
        resolutionData.resolution = getResolution(frameSizeEnum);
        resolutionData.maxFps =
            getHighestFrameRate(fileDescriptor, pixelFormat, resolutionData.resolution);

        //todo get bitrate somehow
        resolutionData.bitrate = 200000;

        resolutionList.push_back(resolutionData);

        if(frameSizeEnum.type != V4L2_FRMSIZE_TYPE_DISCRETE)
            break;

        ++frameSizeEnum.index;
    }

    return resolutionList;
}

} // namespace v4l2
} // namespace utils
} // namespace webcam_plugin
} // namespace nx

#endif