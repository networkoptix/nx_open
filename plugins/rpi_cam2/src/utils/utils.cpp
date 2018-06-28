#include "v4l2_utils.h"


namespace nx {
namespace rpi_cam2 {
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
    auto addResolutionData =
        [](std::vector<ResolutionData>* list, const nxcip::Resolution& resolution, int fps)
        {
            ResolutionData r;
            r.resolution = resolution;
            r.maxFps = fps;
            r.bitrate = 16000000;
            list->push_back(r);
        };

        std::vector<ResolutionData> list;

        addResolutionData(&list, {1920, 1080}, 30);
        addResolutionData(&list, {1280, 720}, 30); // locks up the h264_omx encoder
        addResolutionData(&list, {800, 600}, 30);
        addResolutionData(&list, {640, 480}, 30); // locks up the h264_omx encoder
        addResolutionData(&list, {640, 360}, 30);
        addResolutionData(&list, {480, 270}, 30);
        addResolutionData(&list, {320, 180}, 30);

        return list;

     //return v4l2::getResolutionList(devicePath, targetCodecID);
}

} // namespace utils
} // namespace rpi_cam2
} // namespace nx