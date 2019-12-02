#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::server::nvr::hanwha {

struct NvrIni: public nx::kit::IniConfig
{
    NvrIni(): IniConfig("hanwha_nvr.ini") { reload(); }

    NX_INI_INT(0, upperPowerConsumptionLimitWatts,
        "If more than zero, the NVR will go to the 'PoE over budget' mode when power consumption\n"
        "exceeds this value. Otherwise the value of this limit is calculated automatically\n"
        "depending on the NVR model");

    NX_INI_INT(0, lowerPowerConsumptionLimitWatts,
        "If more than zero, the NVR will return to the 'normal' mode when power consumption\n"
        "becomes lower than this value. Otherwise the value of this limit is calculated\n"
        "automatically depending on the NVR model");
};

NvrIni& nvrIni();

} // namespace nx::vms::server::nvr::hanwha
