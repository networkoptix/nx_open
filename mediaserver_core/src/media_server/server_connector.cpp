#include "server_connector.h"

#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "nx/vms/discovery/manager.h"
#include "network/module_information.h"
#include <network/connection_validator.h>
#include <nx/utils/log/log.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

#if 0
namespace {
    QUrl trimmedUrl(const QUrl &url) {
        QUrl newUrl;
        newUrl.setScheme(lit("http"));
        newUrl.setHost(url.host());
        newUrl.setPort(url.port());
        return newUrl;
    }
}
#endif

QnServerConnector::QnServerConnector(QnCommonModule* commonModule):
    QObject(),
    QnCommonModuleAware(commonModule)
{
}

static QString makeModuleUrl(const nx::vms::discovery::ModuleEndpoint& module)
{
    QUrl moduleUrl;
    moduleUrl.setScheme(module.sslAllowed ? lit("https") : lit("http"));
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
    commonModule()->ec2Connection()->addRemotePeer(module.id, newUrl);
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

    NX_LOGX(lm("Removing connection to module %1 by URL %2").args(id, (moduleUrl)), cl_logINFO);
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
        usedUrls = m_urls;
    }

    for (const auto& it: m_urls.keys())
        removeConnection(it);

    {
        QnMutexLocker lock(&m_mutex);
        m_urls.clear();
    }
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
