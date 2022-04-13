// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <algorithm>
#include <vector>
#include <fstream>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;

bool toBool(std::string str)
{
    std::transform(str.begin(), str.begin(), str.end(), ::tolower);
    return str == "true" || str == "1";
}

bool startsWith(const std::string& str, const std::string& prefix)
{
    return str.rfind(prefix, 0) == 0;
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

std::string join(
    const std::vector<std::string>& strings,
    const std::string& delimiter,
    const std::string& itemPrefix,
    const std::string& itemPostfix)
{
    std::string result;
    for (int i = 0; i < strings.size(); ++i)
    {
        result += itemPrefix + strings[i] + itemPostfix;
        if (i != strings.size() - 1)
            result += delimiter;
    }

    return result;
}

std::map<std::string, std::string> toStdMap(const Ptr<const IStringMap>& sdkMap)
{
    std::map<std::string, std::string> result;
    if (!sdkMap)
        return result;

    for (int i = 0; i < sdkMap->count(); ++i)
        result[sdkMap->key(i)] = sdkMap->value(i);

    return result;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
