// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_server_resource.h"

#include <api/server_rest_connection.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

CrossSystemServerResource::CrossSystemServerResource(core::RemoteConnectionPtr connection)
{
    addFlags(Qn::cross_system);
    setPrimaryAddress(connection->address());
    m_restConnection = connection->serverApi();
}

CrossSystemServerResource::CrossSystemServerResource(
    const nx::Uuid& id,
    nx::network::SocketAddress endpoint,
    core::RemoteConnectionPtr connection)
{
    addFlags(Qn::cross_system);
    setIdUnsafe(id);
    setPrimaryAddress(std::move(endpoint));
    m_restConnection = rest::ServerConnectionPtr(new rest::ServerConnection(
        systemContext()->httpClientPool(),
        id,
        /*auditId*/ nx::Uuid::createUuid(),
        connection->certificateCache().get(),
        getPrimaryAddress(),
        connection->credentials()));
}

QString CrossSystemServerResource::rtspUrl() const
{
    return nx::network::url::Builder()
        .setEndpoint(getPrimaryAddress())
        .setScheme(nx::network::rtsp::kSecureUrlSchemeName)
        .toUrl().toString();
}

} // namespace nx::vms::client::desktop
