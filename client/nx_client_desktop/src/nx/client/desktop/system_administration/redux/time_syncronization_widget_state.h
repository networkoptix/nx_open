#pragma once

#include <algorithm>
#include <chrono>

#include <nx/client/desktop/common/redux/abstract_redux_state.h>

#include <nx/utils/uuid.h>

namespace nx::client::desktop {

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

    std::chrono::milliseconds vmsTime = std::chrono::milliseconds(0);
    Status status = Status::synchronizedWithInternet;

    struct ServerInfo
    {
        QnUuid id;
        QString name;
        QString ipAddress;
        bool online = true;
        bool hasInternet = true;
        std::chrono::milliseconds osTime = std::chrono::milliseconds(0);
        std::chrono::milliseconds vmsTime = std::chrono::milliseconds(0);
        // QString timeZoneId;
        qint64 timeZoneOffsetMs = 0;
    };
    QList<ServerInfo> servers;

    bool syncTimeWithInternet() const
    {
        return enabled && primaryServer.isNull();
    }

    QnUuid actualPrimaryServer() const
    {
        if (!enabled)
            return QnUuid();

        if (std::any_of(servers.cbegin(), servers.cend(),
            [this](const ServerInfo& info) { return info.id == primaryServer; }))
        {
            return primaryServer;
        }

        return QnUuid();
    }
};

} // namespace nx::client::desktop
