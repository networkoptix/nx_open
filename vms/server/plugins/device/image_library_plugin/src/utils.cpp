// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <nx/kit/debug.h>

using nx::kit::utils::toString;
using namespace std::string_literals;

void logCameraInfo(const nxcip::CameraInfo& info, const std::string& label)
{
    // NOTE: Empty fields are not logged.
    #define CAMERA_INFO_FIELD_LOG(FIELD) \
        ((info.FIELD[0] == '\0') ? "" : ("    "s + #FIELD + ": " + toString(info.FIELD) + ",\n"))

    NX_PRINT << "CameraInfo: " << label << "\n"
        << "{\n"
        << CAMERA_INFO_FIELD_LOG(modelName)
        << CAMERA_INFO_FIELD_LOG(firmware)
        << CAMERA_INFO_FIELD_LOG(uid)
        << CAMERA_INFO_FIELD_LOG(url)
        << CAMERA_INFO_FIELD_LOG(auxiliaryData)
        << CAMERA_INFO_FIELD_LOG(defaultLogin)
        << CAMERA_INFO_FIELD_LOG(defaultPassword)
        << "}";

    #undef CAMERA_INFO_FIELD_LOG
}

std::string fileUrlToPath(const std::string& url)
{
    static const std::string kFilePrefix = "file://";

    std::string path = url;

    if (nx::kit::utils::stringStartsWith(path, kFilePrefix))
    {
        path = url.substr(kFilePrefix.size());
        // Skip the third slash, if any.
        if (path[0] == '/')
            path = path.substr(1);
    }

    // Remove the trailing slash or backslash, if any.
    const char lastChar = path[path.size() - 1];
    if (lastChar == '/' || lastChar == '\\')
        path = path.substr(0, path.size() - 1);

    return path;
}
