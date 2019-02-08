#include "server_connector.h"

#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "nx/vms/discovery/manager.h"
#include <network/connection_validator.h>
#include <nx/utils/log/log.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include <nx/vms/api/types/connection_types.h>

QnServerConnector::QnServerConnector(QnCommonModule* commonModule):
    QObject(),
    QnCommonModuleAware(commonModule)
{
    connect(commonModule, &QnCommonModule::standAloneModeChanged, this,
        [this](bool value)
        {
            if (value)
                stop();
            else
                start();
        }, Qt::DirectConnection);
}

static QString makeModuleUrl(const nx::vms::discovery::ModuleEndpoint& module)
{
    QUrl moduleUrl;
    moduleUrl.setScheme(nx::network::http::urlSheme(module.sslAllowed));
    moduleUrl.setHost(module.endpoint.address.toString());
    moduleUrl.setPort(module.endpoint.port);
    return moduleUrl.toString();
}

void QnServerConnector::addConnection(const nx::vms::discovery::ModuleEndpoint& module)
{
    const auto newUrl = makeModuleUrl(module);
    {
        QnMutexLocker lock(&m_mutex);
        auto& value = m_urls[module.id];
        if (value == newUrl)
        {
            NX_VERBOSE(this, lm("Module %1 change does not affect URL %2").args(module.id, newUrl));
            return;
        }
        value = newUrl;
    }

    NX_DEBUG(this, lm("Adding connection to module %1 by URL %2").args(module.id, newUrl));
    commonModule()->ec2Connection()->addRemotePeer(
        module.id, nx::vms::api::PeerType::server, newUrl);
}

void QnServerConnector::removeConnection(const QnUuid& id)
{
    QString moduleUrl;
    {
        QnMutexLocker lock(&m_mutex);
        moduleUrl = m_urls.take(id);
    }

    if (moduleUrl.isNull())
        return;

    NX_INFO(this, lm("Removing connection to module %1 by URL %2").args(id, (moduleUrl)));
    commonModule()->ec2Connection()->deleteRemotePeer(id);

    const auto server = resourcePool()->getResourceById(id);
    if (server && server->getStatus() == Qn::Unauthorized)
        server->setStatus(Qn::Offline);
}

void QnServerConnector::start()
{
    NX_DEBUG(this, "Started");
    commonModule()->moduleDiscoveryManager()->onSignals(this,
        &QnServerConnector::at_moduleChanged,
        &QnServerConnector::at_moduleChanged,
        &QnServerConnector::at_moduleLost);
}

void QnServerConnector::stop()
{
    NX_DEBUG(this, "Stopped");
    commonModule()->moduleDiscoveryManager()->disconnect(this);

    decltype(m_urls) usedUrls;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(usedUrls, m_urls);
    }

    for (const auto& it: usedUrls.keys())
        removeConnection(it);
}

void QnServerConnector::restart()
{
    stop();
    start();
}

void QnServerConnector::at_moduleFound(nx::vms::discovery::ModuleEndpoint module)
{
    if (QnConnectionValidator::isCompatibleToCurrentSystem(module, commonModule()))
        return addConnection(module);

    NX_VERBOSE(this, lm("Ignore incompatable server %1 %2 %3 %4")
        .args(module.id, module.endpoint, module.systemName, module.version));
}

void QnServerConnector::at_moduleChanged(nx::vms::discovery::ModuleEndpoint module)
{
    if (QnConnectionValidator::isCompatibleToCurrentSystem(module, commonModule()))
        return addConnection(module);
    else
        return removeConnection(module.id);
}

void QnServerConnector::at_moduleLost(QnUuid id)
{
    removeConnection(id);
}
