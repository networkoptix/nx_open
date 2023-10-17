// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_server_port_watcher.h"

#include <client/client_message_processor.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>

using namespace nx::vms::client::desktop;

QnWorkbenchServerPortWatcher::QnWorkbenchServerPortWatcher(QObject *parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_currentServer()
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr &resource)
        {
            if (resource != m_currentServer)
                return;

            disconnect(m_currentServer.get(), nullptr, this, nullptr);
            m_currentServer.clear();
        });

    connect(qnClientMessageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this,
        [this]()
        {
            auto server = currentServer();

            NX_ASSERT(!server.isNull(), "currentServer is nullptr!");
            if (!server)
                return;

            if (m_currentServer)
                m_currentServer->disconnect(this);

            const auto updateConnectionAddress =
                [this]()
                {
                    if (auto connection = this->connection())
                        connection->updateAddress(m_currentServer->getPrimaryAddress());
                };

            m_currentServer = server;
            updateConnectionAddress();

            connect(server.get(),
                &QnMediaServerResource::primaryAddressChanged,
                this,
                [this, updateConnectionAddress](const QnResourcePtr& resource)
                {
                    const auto server = resource.dynamicCast<QnMediaServerResource>();
                    if (NX_ASSERT(server && server == m_currentServer))
                        updateConnectionAddress();
                });
        });
}

QnWorkbenchServerPortWatcher::~QnWorkbenchServerPortWatcher()
{
}
