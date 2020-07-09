#include "stub_analytics_plugin_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
