#pragma once

#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct DeviceAgentId
{
    QnUuid device;
    QnUuid engine;

    inline bool operator==(const DeviceAgentId& other) const
    {
        return device == other.device && engine == other.engine;
    }
};

inline uint qHash(const DeviceAgentId& key)
{
    return qHash(key.device) + qHash(key.engine);
}

struct NX_VMS_CLIENT_DESKTOP_API DeviceAgentData
{
    enum class Status
    {
        initial,
        loading,
        applying,
        ok,
        failure,
    };
    QJsonObject model;
    QJsonObject values;
    Status status = Status::initial;
};

} // namespace nx::vms::client::desktop
