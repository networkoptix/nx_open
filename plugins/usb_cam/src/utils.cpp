#include "utils.h"

#include <string>

namespace nx {
namespace usb_cam {
namespace utils {

namespace {

} // namespace 

std::string decodeCameraInfoUrl(const char * url)
{
    /**
     * Expected to be of the form: <protocol>://<ip>:<port>/<camera-resource>, 
     * where <camera-resource> is /dev/video* on Linux or the device-instance-id on Windows.
     */
    std::string hostWithResource(url);
    
    // find the second occurence of ":" in the url
    int colon = hostWithResource.find(":", hostWithResource.find(":") + 1);
    if(colon == std::string::npos)
        return std::string();

    hostWithResource = hostWithResource.substr(colon);
    
    return hostWithResource.substr(hostWithResource.find("/") + 1);
}

std::string encodeCameraInfoUrl(const char * host, const char * cameraResource)
{
    return std::string(host) + "/" + cameraResource;
}

float msecPerFrame(float fps)
{
    return 1.0 / fps * 1000;
}

} // namespace utils
} // namespace usb_cam
} // namespace nx