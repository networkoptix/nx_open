#include "v4l2_utils.h"
#include "logging.h"


namespace nx {
namespace webcam_plugin {
namespace utils {

std::vector<DeviceData> getDeviceList()
{
    return v4l2::getDeviceList();
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char *devicePath)
{
    return v4l2::getSupportedCodecs(devicePath);
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID)
{
    debugln(__FUNCTION__);
    debugln("    HARDCODING RESOLUTION AND FPS");
    auto addResolutionData =
        [](std::vector<ResolutionData>* list, const nxcip::Resolution& resolution, int fps)
        {
            ResolutionData r;
            r.resolution = resolution;
            r.maxFps = fps;
            r.bitrate = 16000000;
            list->push_back(r);
            debug("    w:%d, h:%d, fps:%d\n", resolution.width, resolution.height, fps);
        };

        std::vector<ResolutionData> list;

        addResolutionData(&list, {1920, 1080}, 30);
        addResolutionData(&list, {1280, 720}, 30); // locks up the h264_omx encoder
        addResolutionData(&list, {800, 600}, 30);
        addResolutionData(&list, {640, 480}, 30); // locks up the h264_omx encoder
        addResolutionData(&list, {320, 240}, 30);

        return list;

     //return v4l2::getResolutionList(devicePath, targetCodecID);
}

} // namespace utils
} // namespace webcam_plugin
} // namespace nx