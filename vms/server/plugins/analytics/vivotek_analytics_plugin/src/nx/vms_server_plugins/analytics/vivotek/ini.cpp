#include "ini.h"

namespace nx::vms_server_plugins::analytics::vivotek {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
