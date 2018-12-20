#include "nx_utils_ini.h"

namespace nx::utils {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::utils
