#include "resource_management_ini.h"

ResourceManagementIni& resourceManagementIni()
{
    static ResourceManagementIni ini;
    return ini;
}
