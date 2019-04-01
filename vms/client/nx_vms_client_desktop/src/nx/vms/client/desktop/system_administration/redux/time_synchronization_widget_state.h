#pragma once

#include <algorithm>
#include <chrono>

#include <nx/vms/client/desktop/common/redux/abstract_redux_state.h>

#include <common/common_globals.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

struct TimeSynchronizationWidgetState: AbstractReduxState
{
    enum class Status
    {
        synchronizedWithInternet,
        synchronizedWithSelectedServer,
        notSynchronized,
        singleServerLocalTime,
        noInternetConnection,
        selectedServerIsOffline
    };

    bool hasChanges = false;
    bool readOnly = false;

    bool enabled = true;
    QnUuid primaryServer;

    // Previous selected time server.
    QnUuid lastPrimaryServer;

    milliseconds baseTime = 0ms;
    milliseconds elapsedTime = 0ms;

    milliseconds commonTimezoneOffset = 0ms;
    Status status = Status::synchronizedWithInternet;

    struct ServerInfo
    {
        QnUuid id;
        QString name;
        QString ipAddress;
        bool online = true;
        bool hasInternet = true;
        milliseconds osTimeOffset = 0ms;
        milliseconds vmsTimeOffset = 0ms;
        // QString timeZoneId;
        milliseconds timeZoneOffset = 0ms;
    };
    QList<ServerInfo> servers;

    bool syncTimeWithInternet() const
    {
        return status == Status::synchronizedWithInternet
            || status == Status::noInternetConnection;
    }

    milliseconds calcCommonTimezoneOffset() const
    {
        milliseconds commonUtcOffset = 0ms;
        bool initialized = false;

        for (const auto& server: servers)
        {
            if (!server.online)
                continue;

            const auto utcOffset = server.timeZoneOffset;
            if (utcOffset == milliseconds(Qn::InvalidUtcOffset))
                continue;

            if (!initialized)
            {
                commonUtcOffset = utcOffset;
                initialized = true;
            }
            else if (commonUtcOffset != utcOffset)
                return 0ms; //< Show time in UTC when time zones are different
        }

        return commonUtcOffset;
    }
};

} // namespace nx::vms::client::desktop
