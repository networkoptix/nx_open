#include "server_connector.h"

#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "network/module_finder.h"
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

void QnServerConnector::at_moduleFinder_moduleAddressFound(const QnModuleInformation &moduleInformation, const SocketAddress &address)
{
    if (!QnConnectionValidator::isCompatibleToCurrentSystem(
        moduleInformation,
        commonModule()))
    {
        bool used;
        {
            QnMutexLocker lock( &m_mutex );
            used = m_usedAddresses.contains(address.toString());
        }
        if (used)
        {
            NX_LOG(lit("QnServerConnector: Module %1 has become incompatible. Url = %2, System name = %3, version = %4")
                   .arg(moduleInformation.id.toString())
                   .arg(address.toString())
                   .arg(moduleInformation.systemName)
                   .arg(moduleInformation.version.toString()),
                   cl_logINFO);
        }
        NX_LOG(lit("QnServerConnector. Removing address %1 from module %2 since incompatible")
            .arg(address.toString()).arg(moduleInformation.id.toString()),
            cl_logDEBUG1);
        removeConnection(moduleInformation, address);
        return;
    }

    addConnection(moduleInformation, address);
}

void QnServerConnector::at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const SocketAddress &address) {
    NX_LOG(lit("QnServerConnector. Removing address %1 from module %2 since address has been lost")
        .arg(address.toString()).arg(moduleInformation.id.toString()),
        cl_logDEBUG1);
    removeConnection(moduleInformation, address);
}

void QnServerConnector::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation)
{
    const auto& moduleFinder = commonModule()->moduleFinder();
    if (QnConnectionValidator::isCompatibleToCurrentSystem(moduleInformation, commonModule()))
    {
        for (const SocketAddress &address: moduleFinder->moduleAddresses(moduleInformation.id))
            addConnection(moduleInformation, address);
    }
    else
    {
        for (const SocketAddress &address: moduleFinder->moduleAddresses(moduleInformation.id))
        {
            NX_LOG(lit("QnServerConnector. Removing address %1 from module %2 since module has been changed")
                .arg(address.toString()).arg(moduleInformation.id.toString()),
                cl_logDEBUG1);
            removeConnection(moduleInformation, address);
        }
    }
}

void QnServerConnector::addConnection(
    const QnModuleInformation &moduleInformation,
    const SocketAddress &address)
{
    AddressInfo urlInfo;

    {
        QnMutexLocker lock( &m_mutex );

        QString addressString = address.toString();

        if (m_usedAddresses.contains(addressString)) {
            NX_LOG(lit("QnServerConnector: Address %1 is already used.").arg(address.toString()), cl_logINFO);
            return;
        }

        QUrl moduleUrl;
        moduleUrl.setScheme(moduleInformation.sslAllowed ? lit("https") : lit("http"));
        moduleUrl.setHost(address.address.toString());
        moduleUrl.setPort(address.port);
        urlInfo.urlString = moduleUrl.toString();
        urlInfo.peerId = moduleInformation.id;
        m_usedAddresses.insert(addressString, urlInfo);
    }

    NX_LOG(lit("QnServerConnector: Adding connection to module %1. Url = %2").arg(moduleInformation.id.toString()).arg(urlInfo.urlString), cl_logINFO);

    ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();
    ec2Connection->addRemotePeer(moduleInformation.id, urlInfo.urlString);
}

void QnServerConnector::removeConnection(const QnModuleInformation &moduleInformation, const SocketAddress &address)
{
    QnMutexLocker lock( &m_mutex );
    AddressInfo urlInfo = m_usedAddresses.take(address.toString());
    lock.unlock();
    if (urlInfo.peerId.isNull())
        return;

    NX_LOG(lit("QnServerConnector: Removing connection from module %1. Url = %2").arg(moduleInformation.id.toString()).arg(urlInfo.urlString), cl_logINFO);

    ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();
    ec2Connection->deleteRemotePeer(moduleInformation.id);

    QnResourcePtr mServer = resourcePool()->getResourceById(moduleInformation.id);
    if (mServer && mServer->getStatus() == Qn::Unauthorized)
        mServer->setStatus(Qn::Offline);
}

void QnServerConnector::start()
{
    const auto& moduleFinder = commonModule()->moduleFinder();
    connect(moduleFinder,     &QnModuleFinder::moduleAddressFound,    this,   &QnServerConnector::at_moduleFinder_moduleAddressFound);
    connect(moduleFinder,     &QnModuleFinder::moduleAddressLost,     this,   &QnServerConnector::at_moduleFinder_moduleAddressLost);
    connect(moduleFinder,     &QnModuleFinder::moduleChanged,     this,   &QnServerConnector::at_moduleFinder_moduleChanged);

    for (const QnModuleInformation &moduleInformation: moduleFinder->foundModules())
    {
        if (!QnConnectionValidator::isCompatibleToCurrentSystem(moduleInformation, commonModule()))
            continue;

        for (const SocketAddress &address: moduleFinder->moduleAddresses(moduleInformation.id))
            addConnection(moduleInformation, address);
    }
}

void QnServerConnector::stop()
{
    const auto& moduleFinder = commonModule()->moduleFinder();
    moduleFinder->disconnect(this);

    QHash<QString, AddressInfo> usedUrls;
    {
        QnMutexLocker lock( &m_mutex );
        usedUrls = m_usedAddresses;
    }

    for (auto it = usedUrls.begin(); it != usedUrls.end(); ++it)
    {
        NX_LOG(lit("QnServerConnector. Removing address %1 from module %2 since stopping...")
            .arg(it.key()).arg(it->peerId.toString()),
            cl_logDEBUG1);
        removeConnection(moduleFinder->moduleInformation(it->peerId), it.key());
    }

    {
        QnMutexLocker lock( &m_mutex );
        m_usedAddresses.clear();
    }
}

void QnServerConnector::restart() {
    stop();
    start();
}
