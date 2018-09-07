#include "utils.h"

#include <nx/utils/url.h>

namespace nx {
namespace usb_cam {
namespace utils {

namespace {

const std::string kWebcamUrlPrefix = "webcam://";

} // namespace 

std::string decodeCameraInfoUrl(const char * url)
{
    std::string urlStr(url);
    return urlStr.length() > kWebcamUrlPrefix.length() 
        ? urlStr.substr(kWebcamUrlPrefix.length())
        : urlStr;
    //QString encodeUrl = QString(url).mid(kWebcamUrlPrefix.length()); /* webcam:// == 9 chars */
    //return nx::utils::Url::fromPercentEncoding(encodeUrl.toLatin1()).toStdString();
}

std::string encodeCameraInfoUrl(const char * url)
{
    return kWebcamUrlPrefix + url;
    //return QByteArray(kWebcamUrlPrefix.c_str()).append(nx::utils::Url::toPercentEncoding(url)).data();
}

} // namespace utils
} // namespace usb_cam
} // namespace nx