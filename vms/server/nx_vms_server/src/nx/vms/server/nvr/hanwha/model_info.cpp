#include "model_info.h"

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/hanwha_nvr_ini.h>

namespace nx::vms::server::nvr::hanwha {

static const std::map</*boardId*/ int, /*model*/ QString> kModelByBoardId = {
    {kWrn810BoardId, kWrn810},
    {kWrn1610BoardId, kWrn1610}
};

static const std::map</*model*/ QString, PowerConsumptionLimits> kDefaultPowerConsumptionLimits = {
    {kWrn410, {40.0, 50.0}},
    {kWrn810, {80.0, 100.0}},
    {kWrn1610, {180.0, 200.0}}
};

PowerConsumptionLimits getPowerConsumptionLimit(const DeviceInfo& deviceInfo)
{
    PowerConsumptionLimits result;

    if (const auto it = kDefaultPowerConsumptionLimits.find(deviceInfo.model);
        it != kDefaultPowerConsumptionLimits.cend())
    {
        result = it->second;
    }

    NX_DEBUG(NX_SCOPE_TAG,
        "Got the following power consumption limits for the model '%1': "
        "lower limit: %2 watts, upper limit: %3 watts",
        deviceInfo.model,
        result.lowerLimitWatts,
        result.upperLimitWatts);

    if (const double iniLowerLimit = nvrIni().lowerPowerConsumptionLimitWatts; iniLowerLimit > 0)
    {
        NX_DEBUG(NX_SCOPE_TAG,
            "Taking the lower power consumption limit from %1, lower limit: %2 watts",
            nvrIni().iniFile(),
            iniLowerLimit);

        result.lowerLimitWatts = iniLowerLimit;
    }

    if (const double iniUpperLimit = nvrIni().upperPowerConsumptionLimitWatts; iniUpperLimit > 0)
    {
        NX_DEBUG(NX_SCOPE_TAG,
            "Taking the upper power consumption limit from %1, upper limit: %2 watts",
            nvrIni().iniFile(),
            iniUpperLimit);

        result.upperLimitWatts = iniUpperLimit;
    }

    if (result.upperLimitWatts < result.lowerLimitWatts)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Upper power consumption limit is less than the lower one: %1 > %2, "
            "using the following value for both limits: %3",
            result.lowerLimitWatts,
            result.upperLimitWatts,
            result.lowerLimitWatts);

        result.upperLimitWatts = result.lowerLimitWatts;
    }

    return result;
}

QString getModelByBoardId(int boardId)
{
    if (const auto it = kModelByBoardId.find(boardId); it != kModelByBoardId.cend())
        return it->second;

    NX_WARNING(NX_SCOPE_TAG, "Unable to find model name by board id '%1'", boardId);
    return QString();
}

} // namespace nx::vms::server::nvr::hanwha

