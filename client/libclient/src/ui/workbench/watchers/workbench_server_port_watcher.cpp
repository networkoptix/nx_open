
#include "workbench_server_port_watcher.h"

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

QnWorkbenchServerPortWatcher::QnWorkbenchServerPortWatcher(QObject *parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_currentServer()
{
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr &resource)
        {
            if (m_currentServer.isNull() || (resource != m_currentServer))
                return;

            disconnect(m_currentServer, nullptr, this, nullptr);
            m_currentServer.clear();
        });

    connect(qnClientMessageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this,
        [this]()
        {
            const auto currentServer = qnCommon->currentServer();

            NX_ASSERT(!currentServer.isNull(), Q_FUNC_INFO, "qnCommon->currentServer() is NULL!");
            if (!currentServer)
                return;

            m_currentServer = currentServer;
            connect(currentServer, &QnMediaServerResource::portChanged, this
                , [this](const QnResourcePtr &resource)
            {
                const auto currentServer = resource.dynamicCast<QnMediaServerResource>();
                if (!currentServer)
                    return;

                QUrl url = QnAppServerConnectionFactory::url();
                if (url.isEmpty() || (url.port() == currentServer->getPort()))
                    return;

                url.setPort(currentServer->getPort());
                QnAppServerConnectionFactory::setUrl(url);

                context()->menu()->trigger(QnActions::ReconnectAction);
            });
        });
}

QnWorkbenchServerPortWatcher::~QnWorkbenchServerPortWatcher()
{
}
