#include "system_commands_ini.h"

namespace nx {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx
