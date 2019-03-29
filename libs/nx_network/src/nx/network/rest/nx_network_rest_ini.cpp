#include "nx_network_rest_ini.h"

namespace nx::network::rest {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::network::rest
