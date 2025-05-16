// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_port_watcher.h"

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

ServerPortWatcher::ServerPortWatcher(SystemContext* context, QObject *parent)
    :
    base_type(parent),
    SystemContextAware(context)
{
    connect(messageProcessor(), &QnCommonMessageProcessor::initialResourcesReceived, this,
        [this]()
        {
            m_serverConnection.reset();

            const auto server = currentServer();
            if (!server)
                return;

            const auto updateConnectionAddress =
                [this](const QnMediaServerResourcePtr& server)
                {
                    if (auto connection = this->connection())
                        connection->updateAddress(server->getPrimaryAddress());
                };

            updateConnectionAddress(server);

            m_serverConnection.reset(connect(
                server.get(),
                &QnMediaServerResource::primaryAddressChanged,
                this,
                [this, updateConnectionAddress](const QnResourcePtr& resource)
                {
                    const auto server = resource.dynamicCast<QnMediaServerResource>();
                    if (NX_ASSERT(server && server->getId() == currentServerId()))
                        updateConnectionAddress(server);
                }));
        });
}

ServerPortWatcher::~ServerPortWatcher()
{
}

} // namespace nx::vms::client::desktop
