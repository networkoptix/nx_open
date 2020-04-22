
#include "workbench_server_port_watcher.h"

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

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

            NX_ASSERT(!currentServer.isNull(), "commonModule()->currentServer() is NULL!");
            if (!currentServer)
                return;

            if (m_currentServer)
                m_currentServer->disconnect(this);

            const auto updateConnectionUrl =
                [this]()
                {
                    if (auto connection = commonModule()->ec2Connection())
                    {
                        const auto oldUrl = connection->connectionInfo().effectiveUrl();
                        auto newUrl = m_currentServer->getApiUrl();
                        newUrl.setUserName(oldUrl.userName());
                        newUrl.setPassword(oldUrl.password());
                        connection->updateConnectionUrl(newUrl);
                    }
                };

            m_currentServer = currentServer;
            updateConnectionUrl();

            connect(currentServer, &QnMediaServerResource::primaryAddressChanged, this,
                [this, updateConnectionUrl](const QnResourcePtr& resource)
                {
                    const auto server = resource.dynamicCast<QnMediaServerResource>();
                    if (NX_ASSERT(server && server == m_currentServer))
                        updateConnectionUrl();
                });
        });
}

QnWorkbenchServerPortWatcher::~QnWorkbenchServerPortWatcher()
{
}
