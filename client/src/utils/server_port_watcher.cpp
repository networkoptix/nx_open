
#include "server_port_watcher.h"

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

QnServerPortWatcher::QnServerPortWatcher(QnWorkbenchContext *context
    , QObject *parent)
    : QObject(parent)
    , m_currentServerResource()
{
    Q_ASSERT_X(context, Q_FUNC_INFO, "Context should be created up to now!");
    if (!context)
        return;

    QObject::connect(qnResPool, &QnResourcePool::resourceRemoved, this
        , [this, context](const QnResourcePtr &resource)
    {
        if (!m_currentServerResource || !resource)
            return;

        const auto removedServer = resource.dynamicCast<QnMediaServerResource>();
        if (!removedServer || (removedServer->getId() != m_currentServerResource->getId()))
            return;

        QObject::disconnect(m_currentServerResource.data(), nullptr, this, nullptr);

    });

    QObject::connect(qnClientMessageProcessor, &QnCommonMessageProcessor::initialResourcesReceived
        , this, [this, context]()
    {
        m_currentServerResource = qnCommon->currentServer();
        if (!m_currentServerResource)
            return;

        QObject::connect(m_currentServerResource.data(), &QnMediaServerResource::apiPortChanged, this
            , [context, this](const QnResourcePtr &)
        {
            if (!m_currentServerResource)
                return;

            QUrl url = QnAppServerConnectionFactory::url();
            if (url.isEmpty() || (url.port() == m_currentServerResource->getPort()))
                return;

            url.setPort(m_currentServerResource->getPort());
            QnAppServerConnectionFactory::setUrl(url);

            context->menu()->trigger(Qn::ReconnectAction);
        });
    });
}

QnServerPortWatcher::~QnServerPortWatcher()
{
}
