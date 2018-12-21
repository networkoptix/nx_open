#include "to_string.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

namespace nx {
namespace sdk {
namespace common {

static const int kLevelIndent = 4;

static std::string indent(int overallIndent, int level)
{
    return std::string(overallIndent, ' ') + std::string(kLevelIndent * level, ' ');
}

std::string toJsonString(const IStringMap* map, int overallIndent)
{
    if (!map)
    {
        NX_KIT_ASSERT(map);
        return "[]\n";
    }

    std::string result = indent(overallIndent, 0) + "[\n";

    const int count = map->count();
    for (int i = 0; i < count; ++i)
    {
        result += indent(overallIndent, 1) + "{ "
            + "\"name\": " + nx::kit::utils::toString(map->key(i)) + ", "
            + "\"value\": " + nx::kit::utils::toString(map->value(i))
            + " }" + ((i < count - 1) ? "," : "") + "\n";
    }

    result += indent(overallIndent, 0) + "]";
    return result;
}

std::string toJsonString(const DeviceInfo* deviceInfo, int overallIndent)
{
    using nx::kit::utils::toString;

    std::string result = indent(overallIndent, 0) + "{\n";

    const std::string innerIndent = indent(overallIndent, 1);

    result += innerIndent + "\"vendor\": " + toString(deviceInfo->vendor) + ",\n";
    result += innerIndent + "\"model\": " + toString(deviceInfo->model) + ",\n";
    result += innerIndent + "\"firmware\": " + toString(deviceInfo->firmware) + ",\n";
    result += innerIndent + "\"uid\": " + toString(deviceInfo->uid) + ",\n";
    result += innerIndent + "\"sharedId\": " + toString(deviceInfo->sharedId) + ",\n";
    result += innerIndent + "\"url\": " + toString(deviceInfo->url) + ",\n";
    result += innerIndent + "\"login\": " + toString(deviceInfo->login) + ",\n";
    result += innerIndent + "\"password\": " + toString(deviceInfo->password) + ",\n";
    result += innerIndent + "\"channel\": " + toString(deviceInfo->channel) + ",\n";
    result += innerIndent + "\"logicalId\": " + toString(deviceInfo->logicalId) + "\n";

    result += indent(overallIndent, 0) + "}";
    return result;
}

} // namespace common
} // namespace sdk
} // namespace nx
