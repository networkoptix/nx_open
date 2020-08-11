// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <algorithm>
#include <vector>
#include <fstream>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

bool toBool(std::string str)
{
    std::transform(str.begin(), str.begin(), str.end(), ::tolower);
    return str == "true" || str == "1";
}

std::vector<char> loadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return {};

    return std::vector<char>(std::istreambuf_iterator<char>(file), {});
}

std::string imageFormatFromPath(const std::string& path)
{
    auto endsWith =
        [](const std::string& str, const std::string& suffix)
        {
            if (suffix.size() > str.size())
                return false;

            return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
        };

    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg"))
        return "image/jpeg";

    if (endsWith(path, ".png"))
        return "image/png";

    if (endsWith(path, ".tiff"))
        return "image/tiff";

    return "";
}

bool isHttpOrHttpsUrl(const std::string& path)
{
    auto startsWith =
        [](const std::string& str, const std::string& prefix)
        {
            if (prefix.size() > str.size())
                return false;

            return std::equal(prefix.begin(), prefix.end(), str.begin());
        };

    return startsWith(path, "http://") || startsWith(path, "https://");
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
