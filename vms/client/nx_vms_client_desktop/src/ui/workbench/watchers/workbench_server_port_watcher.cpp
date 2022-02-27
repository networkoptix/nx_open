// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_server_port_watcher.h"

#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>

using namespace nx::vms::client::desktop::ui;

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

            disconnect(m_currentServer, nullptr, this, nullptr);
            m_currentServer.clear();
        });

    connect(qnClientMessageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this,
        [this]()
        {
            const auto currentServer = commonModule()->currentServer();

            NX_ASSERT(!currentServer.isNull(), "commonModule()->currentServer() is nullptr!");
            if (!currentServer)
                return;

            if (m_currentServer)
                m_currentServer->disconnect(this);

            const auto updateConnectionAddress =
                [this]()
                {
                    if (auto connection = this->connection())
                        connection->updateAddress(m_currentServer->getPrimaryAddress());
                };

            m_currentServer = currentServer;
            updateConnectionAddress();

            connect(currentServer, &QnMediaServerResource::primaryAddressChanged, this,
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
