#ifdef __linux__

#include "v4l2_utils.h"

#include <string>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#include <nx/utils/app_info.h>

namespace nx {
namespace device {
namespace impl {

namespace {

const std::vector<ResolutionData> RPI_RESOLUTION_LIST = {
    {1920, 1080, 30},
    {1920, 1080, 25},
    {1920, 1080, 20},
    {1920, 1080, 15},
    {1920, 1080, 10},
    {1920, 1080, 7},
    {1280, 720, 30},
    {1280, 720, 25},
    {1280, 720, 20},
    {1280, 720, 15},
    {1280, 720, 10},
    {1280, 720, 7},
    {800, 600, 30},
    {800, 600, 25},
    {800, 600, 20},
    {800, 600, 15},
    {800, 600, 10},
    {800, 600, 7},
    {640, 480, 30},
    {640, 480, 25},
    {640, 480, 20},
    {640, 480, 15},
    {640, 480, 10},
    {640, 480, 7},
    {640, 360, 30},
    {640, 360, 25},
    {640, 360, 20},
    {640, 360, 15},
    {640, 360, 10},
    {640, 360, 7},
    {480, 270, 30},
    {480, 270, 25},
    {480, 270, 20},
    {480, 270, 15},
    {480, 270, 10},
    {480, 270, 7} 
};

/*!
 * convenience class for opening and closing devices represented by devicePath
 */
struct DeviceInitializer
{
    DeviceInitializer(const char * devicePath):
        fileDescriptor(open(devicePath, O_RDWR))
    {
    }

    ~DeviceInitializer()
    {
        if (fileDescriptor != -1)
            close(fileDescriptor);
    }

    int fileDescriptor;
};

bool isRpiMmal(const char * deviceName);
std::vector<std::string> getDevicePaths();
unsigned int toV4L2PixelFormat(nxcip::CompressionType nxCodecID);
std::string getDeviceName(int fileDescriptor);
float toFrameRate(const v4l2_fract& frameInterval);


bool isRpiMmal(const char * deviceName)
{
    std::string lower(deviceName);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return nx::utils::AppInfo::isRaspberryPi() && lower.find("mmal") != lower.npos;
}

unsigned int toV4L2PixelFormat(nxcip::CompressionType nxCodecID)
{
    switch(nxCodecID)
    {
        case nxcip::AV_CODEC_ID_MPEG2VIDEO: return V4L2_PIX_FMT_MPEG2;
        case nxcip::AV_CODEC_ID_H263:       return V4L2_PIX_FMT_H263;
        case nxcip::AV_CODEC_ID_MJPEG:      return V4L2_PIX_FMT_MJPEG;
        case nxcip::AV_CODEC_ID_MPEG4:      return V4L2_PIX_FMT_MPEG4;
        case nxcip::AV_CODEC_ID_H264:       return V4L2_PIX_FMT_H264;
        default:                            return 0;
    }
}

std::string getDeviceName(int fileDescriptor)
{
    struct v4l2_capability deviceCapability;
    if (ioctl(fileDescriptor, VIDIOC_QUERYCAP, &deviceCapability) == -1)
        return std::string();
    //return std::string(reinterpret_cast<char*> (deviceCapability.card));
    return std::string(deviceCapability.card, deviceCapability.card + sizeof(deviceCapability.card));
};

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

float toFrameRate(const v4l2_fract& frameInterval)
{
    return frameInterval.numerator
        ? (float)((float)frameInterval.denominator / (float)frameInterval.numerator)
        : 0;
};

} // namespace


V4L2CompressionTypeDescriptor::V4L2CompressionTypeDescriptor():
    m_descriptor(new struct v4l2_fmtdesc({0}))
{
}

V4L2CompressionTypeDescriptor::~V4L2CompressionTypeDescriptor()
{
    delete m_descriptor;
}

struct v4l2_fmtdesc * V4L2CompressionTypeDescriptor::descriptor()
{
    return m_descriptor;
}

__u32 V4L2CompressionTypeDescriptor::pixelFormat() const
{
    return m_descriptor->pixelformat;
}

nxcip::CompressionType V4L2CompressionTypeDescriptor::toNxCompressionType() const
{
    switch(m_descriptor->pixelformat)
    {
        case V4L2_PIX_FMT_MPEG2:      return nxcip::AV_CODEC_ID_MPEG2VIDEO;
        case V4L2_PIX_FMT_H263:       return nxcip::AV_CODEC_ID_H263;
        case V4L2_PIX_FMT_MJPEG:      return nxcip::AV_CODEC_ID_MJPEG;
        case V4L2_PIX_FMT_MPEG4:      return nxcip::AV_CODEC_ID_MPEG4;
        case V4L2_PIX_FMT_H264:
        case V4L2_PIX_FMT_H264_NO_SC:
#ifdef V4L2_PIX_FMT_H264_MVC
        case V4L2_PIX_FMT_H264_MVC:   
#endif
                                      return nxcip::AV_CODEC_ID_H264;
        default:                      return nxcip::AV_CODEC_ID_NONE;
    }
}



//////////////////////////////////////////// Public API ////////////////////////////////////////////

std::string getDeviceName(const char * devicePath)
{
    DeviceInitializer initializer(devicePath);
    return getDeviceName(initializer.fileDescriptor);
}

std::vector<DeviceData> getDeviceList()
{
    std::vector<std::string> devicePaths = getDevicePaths();
    std::vector<DeviceData> deviceList;
    if (devicePaths.empty())
        return deviceList;

    for (const auto& devicePath : devicePaths)
    {
        DeviceInitializer initializer(devicePath.c_str());
        int fileDescriptor = initializer.fileDescriptor;
        if (fileDescriptor == -1)
            continue;

        deviceList.push_back(device::DeviceData(getDeviceName(fileDescriptor), devicePath));
    }
    return deviceList;
}

std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> getSupportedCodecs(const char * devicePath)
{
    DeviceInitializer initializer(devicePath);

    std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> codecDescriptorList;
    struct v4l2_fmtdesc formatEnum = {0};
    formatEnum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(initializer.fileDescriptor, VIDIOC_ENUM_FMT, &formatEnum) == 0)
    {
        auto descriptor = std::make_shared<V4L2CompressionTypeDescriptor>();
        memcpy(descriptor->descriptor(), &formatEnum, sizeof(formatEnum));
        codecDescriptorList.push_back(std::move(descriptor));
        ++formatEnum.index;
    }

    return codecDescriptorList;
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    const std::shared_ptr<AbstractCompressionTypeDescriptor>& targetCodecID)
{
    const auto getResolution =
        [](const v4l2_frmsizeenum& enumerator, int * width, int * height)
        {
            if(enumerator.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                *width = enumerator.discrete.width;
                *height = enumerator.discrete.height;
            }
            else
            {
                *width = enumerator.stepwise.max_width;
                *height = enumerator.stepwise.max_height;
            }
        };

    DeviceInitializer initializer(devicePath);

    if(isRpiMmal(getDeviceName(initializer.fileDescriptor).c_str()))
        return RPI_RESOLUTION_LIST;


    auto descriptor = 
        std::dynamic_pointer_cast<const V4L2CompressionTypeDescriptor>(targetCodecID);

    std::vector<ResolutionData> resolutionList;

    __u32 pixelFormat = descriptor->pixelFormat();

    struct v4l2_frmsizeenum frameSizeEnum = {0};
    frameSizeEnum.index = 0;
    frameSizeEnum.pixel_format = pixelFormat;

    while (ioctl(initializer.fileDescriptor, VIDIOC_ENUM_FRAMESIZES, &frameSizeEnum) == 0)
    {
        int width = 0;
        int height = 0;
        getResolution(frameSizeEnum, &width, &height);

        struct v4l2_frmivalenum frameRateEnum = {0};
        frameRateEnum.pixel_format = pixelFormat;
        frameRateEnum.width = width;
        frameRateEnum.height = height;

        while(ioctl(initializer.fileDescriptor, VIDIOC_ENUM_FRAMEINTERVALS, &frameRateEnum) == 0)
        {
            struct v4l2_fract v4l2FrameRate = frameRateEnum.type == V4L2_FRMIVAL_TYPE_DISCRETE
                ? frameRateEnum.discrete
                : frameRateEnum.stepwise.min;

            resolutionList.push_back(ResolutionData(width, height, toFrameRate(v4l2FrameRate)));

            if(frameRateEnum.type != V4L2_FRMIVAL_TYPE_DISCRETE)
                break;

            ++frameRateEnum.index;
        }

        // there is only one resolution reported if this is true
        if(frameSizeEnum.type != V4L2_FRMSIZE_TYPE_DISCRETE)
            break;

        ++frameSizeEnum.index;
    }

    return resolutionList;
}

void setBitrate(const char * devicePath, int bitrate, nxcip::CompressionType /*targetCodecID*/)
{
    if(!isRpiMmal(getDeviceName(devicePath).c_str()))
        return;

    DeviceInitializer initializer(devicePath);
    struct v4l2_ext_controls ecs = {0};
    struct v4l2_ext_control ec {0};
    ec.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ec.value = bitrate;
    ec.size = 0;
    ecs.controls = &ec;
    ecs.count = 1;
    ecs.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ioctl(initializer.fileDescriptor, VIDIOC_S_EXT_CTRLS, &ecs);
}

int getMaxBitrate(const char * devicePath, nxcip::CompressionType tagetCodecID)
{
    DeviceInitializer initializer(devicePath);
    struct v4l2_ext_controls ecs = {0};
    struct v4l2_ext_control ec = {0};
    ec.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ec.size = sizeof(ec.value);
    ecs.controls = &ec;
    ecs.count = 1;
    ecs.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    if (ioctl(initializer.fileDescriptor, VIDIOC_G_EXT_CTRLS, &ecs) == 0)
        return ec.value;

    int error = errno;
    if (error == ENOSPC)
    {
        if (ioctl(initializer.fileDescriptor, VIDIOC_G_EXT_CTRLS, &ecs))
            return ec.value;
    }
    
    return 10000000; // not sure what else to do here
}

} // namespace impl
} // namespace device
} // namespace nx

#endif