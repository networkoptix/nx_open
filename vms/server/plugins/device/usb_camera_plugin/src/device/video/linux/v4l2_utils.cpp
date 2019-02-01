#ifdef __linux__

#include "v4l2_utils.h"

#include <map>
#include <cstring>
#include <string>
#include <algorithm>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>

#include "device/video/rpi/rpi_utils.h"
#include "udev_utils.h"

#define NX_V4L2_LOG(...) \
NX_DEBUG(nx::utils::log::Tag(std::string("nx::usb_cam::v4l2_utils::")+__FUNCTION__), __VA_ARGS__)

namespace nx {
namespace usb_cam {
namespace device {
namespace video {    
namespace detail {

namespace {

static const std::string kV4l2DevicePath = "/dev";

// Convenience class for opening and closing devices represented by devicePath
struct DeviceInitializer
{
    DeviceInitializer(const std::string& devicePath, int oFlag = O_RDWR):
        fileDescriptor(open(devicePath.c_str(), oFlag))
    {
    }

    ~DeviceInitializer()
    {
        if (fileDescriptor > 0)
            close(fileDescriptor);
    }

    int fileDescriptor = -1;
};

static unsigned int toV4L2PixelFormat(nxcip::CompressionType nxCodecID)
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
    
    //force a null terminator on the end of the string with c_str()
    return std::string(
        deviceCapability.card,
        deviceCapability.card + sizeof(deviceCapability.card)).c_str();
};

std::vector<std::string> getDevicePaths()
{
    const auto isDeviceFile =
        [](const std::string& devicePath)
        {
            struct stat buffer;
            stat(devicePath.c_str(), &buffer);
            return S_ISCHR(buffer.st_mode);
        };

    std::vector<std::string> devicePaths;

    DIR *directory = opendir(kV4l2DevicePath.c_str());
    if (!directory)
    {
        NX_V4L2_LOG("opendir( %1 ) failed, returning empty list", kV4l2DevicePath);
        return devicePaths;
    }

    struct dirent *directoryEntry;
    while ((directoryEntry = readdir(directory)) != NULL)
    {
        if (!strstr(directoryEntry->d_name, "video"))
            continue;

        std::string devVideo = kV4l2DevicePath + "/" + directoryEntry->d_name;
        if (isDeviceFile(devVideo))
            devicePaths.push_back(devVideo);
    }

    closedir(directory);
    return devicePaths;
}

float toFrameRate(const v4l2_fract& frameInterval)
{
    return frameInterval.numerator
        ? (float)((float)frameInterval.denominator / (float)frameInterval.numerator)
        : 0;
};

} // namespace


V4L2CompressionTypeDescriptor::V4L2CompressionTypeDescriptor(
    const struct v4l2_fmtdesc& formatEnum)
    :
    m_descriptor(new struct v4l2_fmtdesc({0}))
{
    memcpy(m_descriptor, &formatEnum, sizeof(formatEnum));
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
    switch (m_descriptor->pixelformat)
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

//--------------------------------------------------------------------------------------------------
// public api

std::string getDeviceName(const std::string& devicePath)
{
    DeviceInitializer initializer(devicePath);
    if (initializer.fileDescriptor == -1)
    {
        int error = errno;
        NX_V4L2_LOG("failed to open %1, reason: %2", devicePath, strerror(error));
        return {};
    }
    return getDeviceName(initializer.fileDescriptor);
}

std::vector<DeviceData> getDeviceList()
{
    auto devicePaths = getDevicePaths();
    NX_V4L2_LOG("found %1 devices", devicePaths.size());

    if (devicePaths.empty())
        return {};

    std::vector<DeviceData> deviceList;
    std::map<std::string, int> duplicateCameras;


    for (const auto& devicePath : devicePaths)
    {
        DeviceInitializer initializer(devicePath);
        int fileDescriptor = initializer.fileDescriptor;
        if (fileDescriptor == -1)
        {
            int error = errno;
            NX_V4L2_LOG("failed to open %1: %2", devicePath, strerror(error));
            continue;
        }

        std::string name = getDeviceName(fileDescriptor);

        int nameIndex = 0;
        auto it = duplicateCameras.find(name);
        if (it == duplicateCameras.end())
            duplicateCameras.emplace(name, 0);
        else
            nameIndex = ++it->second;

        std::string uniqueId;
        if (nx::utils::AppInfo::isRaspberryPi() && rpi::isMmalCamera(name))
            uniqueId = rpi::getMmalUniqueId();
        else
            uniqueId = getDeviceUniqueId(devicePath);

        if (uniqueId.empty())
            uniqueId = std::to_string(nameIndex) + "-" + name;

        deviceList.push_back(DeviceData(name, devicePath, uniqueId));
    }

    return deviceList;
}

std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> getSupportedCodecs(
    const std::string& devicePath)
{
    DeviceInitializer initializer(devicePath);

    std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> codecDescriptorList;
    struct v4l2_fmtdesc formatEnum = {0};
    formatEnum.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(initializer.fileDescriptor, VIDIOC_ENUM_FMT, &formatEnum) == 0)
    {
        auto descriptor = std::make_shared<V4L2CompressionTypeDescriptor>(formatEnum);
        codecDescriptorList.push_back(std::move(descriptor));
        ++formatEnum.index;
    }

    return codecDescriptorList;
}

std::vector<ResolutionData> getResolutionList(
    const std::string& devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if (rpi::isMmalCamera(getDeviceName(devicePath)))
        return rpi::getMmalResolutionList();

    const auto getResolution =
        [](const v4l2_frmsizeenum& enumerator, int * width, int * height)
        {
            if (enumerator.type == V4L2_FRMSIZE_TYPE_DISCRETE)
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
    if (initializer.fileDescriptor == -1)
    {
        int error = errno;
        NX_V4L2_LOG("failed to open %1, %2:", devicePath, strerror(error));
        return {};
    }

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
            struct v4l2_fract v4l2FrameInterval = frameRateEnum.type == V4L2_FRMIVAL_TYPE_DISCRETE
                ? frameRateEnum.discrete
                : frameRateEnum.stepwise.min;

            resolutionList.push_back(ResolutionData(width, height, toFrameRate(v4l2FrameInterval)));

            if (frameRateEnum.type != V4L2_FRMIVAL_TYPE_DISCRETE)
                break;

            ++frameRateEnum.index;
        }

        // There is only one resolution reported if this is true
        if (frameSizeEnum.type != V4L2_FRMSIZE_TYPE_DISCRETE)
            break;

        ++frameSizeEnum.index;
    }

    return resolutionList;
}

void setBitrate(
    const std::string& devicePath,
    int bitrate,
    const device::CompressionTypeDescriptorPtr& /*targetCodecID*/)
{
    if (!rpi::isMmalCamera(getDeviceName(devicePath)))
        return;

    DeviceInitializer initializer(devicePath);
    if (initializer.fileDescriptor == -1)
    {
        int error = errno;
        NX_V4L2_LOG("failed to open %1: %2", devicePath, strerror(error));
        return;
    }

    struct v4l2_ext_controls ecs = {0};
    struct v4l2_ext_control ec {0};
    ec.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ec.value = bitrate;
    ec.size = 0;
    ecs.controls = &ec;
    ecs.count = 1;
    ecs.ctrl_class = V4L2_CTRL_CLASS_MPEG;

    if (ioctl(initializer.fileDescriptor, VIDIOC_S_EXT_CTRLS, &ecs) != 0)
    {
        int error = errno;
        NX_V4L2_LOG("ioctl() failed: %1", strerror(error));
    }
}

int getMaxBitrate(const std::string& devicePath, const device::CompressionTypeDescriptorPtr& /*tagetCodecID*/)
{
    if (rpi::isMmalCamera(getDeviceName(devicePath)))
        return rpi::getMmalMaxBitrate();

    DeviceInitializer initializer(devicePath);
    if (initializer.fileDescriptor == -1)
    {
        int error = errno;
        NX_V4L2_LOG("failed to open %1: %2", devicePath, strerror(error));
        return 0;
    }

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

    NX_V4L2_LOG("ioctl() failed: %1", strerror(error));

    if (error == ENOSPC)
    {
        if (ioctl(initializer.fileDescriptor, VIDIOC_G_EXT_CTRLS, &ecs))
            return ec.value;

        error = errno;

        NX_V4L2_LOG("ioctl() failed again: %1", strerror(error));
    }

    return 0;
}

} // namespace detail
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif
