#pragma once

#include <algorithm>
#include <chrono>

#include <nx/vms/client/desktop/common/redux/abstract_redux_state.h>

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
    qint64 primaryOsTimeOffset = 0;
    qint64 primaryVmsTimeOffset = 0;

    // Previous selected time server.
    QnUuid lastPrimaryServer;

    milliseconds vmsTime = 0ms;
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
};

} // namespace nx::vms::client::desktop
