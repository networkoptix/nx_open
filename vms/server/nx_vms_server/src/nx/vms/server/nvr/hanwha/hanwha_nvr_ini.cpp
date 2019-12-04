#include "hanwha_nvr_ini.h"

namespace nx::vms::server::nvr::hanwha {

NvrIni& nvrIni()
{
    static NvrIni ini;
    return ini;
}

} // namespace nx::vms::server::nvr::hanwha
