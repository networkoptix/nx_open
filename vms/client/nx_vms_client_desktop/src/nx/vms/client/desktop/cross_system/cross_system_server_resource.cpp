// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_server_resource.h"

#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/client/core/network/remote_connection.h>

namespace nx::vms::client::desktop {

CrossSystemServerResource::CrossSystemServerResource(core::RemoteConnectionPtr connection)
    :
    QnMediaServerResource(),
    m_endpoint(connection->address()),
    m_moduleInformation(connection->moduleInformation())
{
    setIdUnsafe(m_moduleInformation.id);
    m_restConnection = connection->serverApi();
}

nx::vms::api::ModuleInformation CrossSystemServerResource::getModuleInformation() const
{
    return m_moduleInformation;
}

QString CrossSystemServerResource::rtspUrl() const
{
    return nx::network::url::Builder()
        .setEndpoint(m_endpoint)
        .setScheme(nx::network::rtsp::kSecureUrlSchemeName)
        .toUrl().toString();
}

} // namespace nx::vms::client::desktop
