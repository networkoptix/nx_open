// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/settings.h>

namespace nx::vms::client::core {

struct DeviceAgentId
{
    QnUuid device;
    QnUuid engine;

    inline bool operator==(const DeviceAgentId& other) const
    {
        return device == other.device && engine == other.engine;
    }
};

inline size_t qHash(const DeviceAgentId& key)
{
    return qHash(key.device) + qHash(key.engine);
}

struct NX_VMS_CLIENT_CORE_API DeviceAgentData
{
    enum class Status
    {
        initial,
        loading,
        applying,
        ok,
        failure,
    };
    nx::vms::api::analytics::SettingsModel model;
    nx::vms::api::analytics::SettingsValues values;
    QJsonObject errors;
    QnUuid modelId;
    Status status = Status::initial;
};

} // namespace nx::vms::client::core
