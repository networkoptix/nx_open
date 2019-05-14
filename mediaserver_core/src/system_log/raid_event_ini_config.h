#pragma once

#include <nx/kit/ini_config.h>

namespace system_log {

struct RaidEventIniConfig: public nx::kit::IniConfig
{
    RaidEventIniConfig(): IniConfig("server_raid_event.ini") { reload(); }

    NX_INI_STRING(
        "Security", //< TODO #szaitsev: This value is for tests only, should be changed.
        logName,
        "The name of the system event log to receive notifications from.");

    QString getLogName() const;
};

inline RaidEventIniConfig& ini()
{
    static RaidEventIniConfig ini;
    return ini;
}

} // namespace system_log