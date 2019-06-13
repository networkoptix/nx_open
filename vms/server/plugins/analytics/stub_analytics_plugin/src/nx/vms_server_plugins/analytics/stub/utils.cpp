#include "utils.h"

#include <algorithm>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

bool toBool(std::string str)
{
    std::transform(str.begin(), str.begin(), str.end(), ::tolower);
    return str == "true" || str == "1";
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx