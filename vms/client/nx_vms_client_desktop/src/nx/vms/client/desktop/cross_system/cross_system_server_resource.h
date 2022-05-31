// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>

namespace nx::vms::client::desktop {

class CrossSystemServerResource: public QnMediaServerResource
{
public:
    CrossSystemServerResource(core::RemoteConnectionPtr connection);

    virtual nx::vms::api::ModuleInformation getModuleInformation() const override;

    virtual QString rtspUrl() const override;

private:
    nx::network::SocketAddress m_endpoint;
    const nx::vms::api::ModuleInformation m_moduleInformation;
};

using CrossSystemServerResourcePtr = QnSharedResourcePointer<CrossSystemServerResource>;

} // namespace nx::vms::client::desktop
