#pragma once

#include <nx/kit/ini_config.h>

namespace system_log {

struct RaidEventIniConfig: public nx::kit::IniConfig
{
    RaidEventIniConfig(): IniConfig("server_raid_event.ini") { reload(); }

    NX_INI_STRING(
        "Application",
        logName,
        "The name of the system event log to receive notifications from.");

    NX_INI_STRING(
        "MR_MONITOR",
        providerName,
        "The name of the event provider to receive notifications from.");

    NX_INI_INT(
        3,
        maxLevel,
        "All events with the level higher then maxLevel are ignored.");

    QString getLogName() const;
    QString getProviderName() const;
    int getMaxLevel() const;
};

inline RaidEventIniConfig& ini()
{
    static RaidEventIniConfig ini;
    return ini;
}

} // namespace system_log