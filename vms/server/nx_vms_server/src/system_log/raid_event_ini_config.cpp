#include "raid_event_ini_config.h"

#include <nx/fusion/model_functions.h>

namespace system_log {

QString RaidEventIniConfig::getLogName() const
{
    return logName;
}

QString RaidEventIniConfig::getProviderName() const
{
    return providerName;
}

int RaidEventIniConfig::getMaxLevel() const
{
    return maxLevel;
}

}
