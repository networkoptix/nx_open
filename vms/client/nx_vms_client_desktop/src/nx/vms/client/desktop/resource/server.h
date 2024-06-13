// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <nx/vms/api/data/port_forwarding_configuration.h>
#include <nx/vms/client/core/resource/server.h>

#include "resource_fwd.h"

namespace nx::vms::client::desktop {

struct ForwardedPortConfiguration: public nx::vms::api::PortForwardingConfiguration
{
    uint16_t forwardedPort = 0;

    ForwardedPortConfiguration(const nx::vms::api::PortForwardingConfiguration& configuration):
        nx::vms::api::PortForwardingConfiguration(configuration)
    {
    }
};

struct RemoteAccessData
{
    bool enabledForCurrentUser = true;
    std::vector<ForwardedPortConfiguration> forwardedPortConfigurations;

    bool operator==(const RemoteAccessData&) const = default;
};

class NX_VMS_CLIENT_DESKTOP_API ServerResource: public nx::vms::client::core::ServerResource
{
    Q_OBJECT
    using base_type = nx::vms::client::core::ServerResource;

public:
    void setCompatible(bool value);
    bool isCompatible() const;

    void setDetached(bool value);
    bool isDetached() const;

    void setRemoteAccessData(const RemoteAccessData& value);
    RemoteAccessData remoteAccessData() const;

signals:
    void compatibilityChanged(const QnResourcePtr& resource);
    void isDetachedChanged();
    void remoteAccessDataChanged();

private:
    bool m_isCompatible = true;
    bool m_isDetached = false;
    RemoteAccessData m_remoteAccessData;
};

using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;

} // namespace nx::vms::client::desktop
