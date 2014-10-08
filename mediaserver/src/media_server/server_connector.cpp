#include "server_connector.h"

#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "utils/network/module_finder.h"
#include "utils/network/module_information.h"
#include "utils/common/log.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

namespace {
    QUrl trimmedUrl(const QUrl &url) {
        QUrl newUrl;
        newUrl.setScheme(lit("http"));
        newUrl.setHost(url.host());
        newUrl.setPort(url.port());
        return newUrl;
    }
}

QnServerConnector::QnServerConnector(QnModuleFinder *moduleFinder, QObject *parent) :
    QObject(parent),
    m_moduleFinder(moduleFinder)
{
    connect(m_moduleFinder,     &QnModuleFinder::moduleUrlFound,    this,   &QnServerConnector::at_moduleFinder_moduleUrlFound);
    connect(m_moduleFinder,     &QnModuleFinder::moduleUrlLost,     this,   &QnServerConnector::at_moduleFinder_moduleUrlLost);
    connect(m_moduleFinder,     &QnModuleFinder::moduleChanged,     this,   &QnServerConnector::at_moduleFinder_moduleChanged);
}

void QnServerConnector::at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url) {
    QUrl trimmed = trimmedUrl(url);

    if (!moduleInformation.isCompatibleToCurrentSystem()) {
        if (m_usedUrls.contains(trimmed)) {
            NX_LOG(lit("QnServerConnector: Module %1 has become incompatible. Url = %2, System name = %3, version = %4")
                   .arg(moduleInformation.id.toString())
                   .arg(url.toString())
                   .arg(moduleInformation.systemName)
                   .arg(moduleInformation.version.toString()),
                   cl_logINFO);
        }
        at_moduleFinder_moduleUrlLost(moduleInformation, url);
        return;
    }

    addConnection(moduleInformation, url);
}

void QnServerConnector::at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url) {
    removeConnection(moduleInformation, url);
}

void QnServerConnector::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation) {
    if (moduleInformation.isCompatibleToCurrentSystem())
        return;

    foreach (const QString &address, moduleInformation.remoteAddresses) {
        QUrl url;
        url.setHost(address);
        url.setPort(moduleInformation.port);
        removeConnection(moduleInformation, url);
    }
}

void QnServerConnector::addConnection(const QnModuleInformation &moduleInformation, const QUrl &url) {
    QUrl trimmed = trimmedUrl(url);

    if (m_usedUrls.contains(trimmed)) {
        NX_LOG(lit("QnServerConnector: Url %1 is already used.").arg(trimmed.toString()), cl_logINFO);
        return;
    }

    QUrl moduleUrl = trimmed;
    moduleUrl.setScheme(moduleInformation.sslAllowed ? lit("https") : lit("http"));
    QString urlStr = moduleUrl.toString();
    m_usedUrls.insert(trimmed, urlStr);

    NX_LOG(lit("QnServerConnector: Adding connection to module %1. Url = %2").arg(moduleInformation.id.toString()).arg(urlStr), cl_logINFO);

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2Connection->addRemotePeer(urlStr);
}

void QnServerConnector::removeConnection(const QnModuleInformation &moduleInformation, const QUrl &url) {
    QUrl trimmed = trimmedUrl(url);
    QString urlStr = m_usedUrls.take(trimmed);
    if (urlStr.isEmpty())
        return;

    NX_LOG(lit("QnServerConnector: Removing connection from module %1. Url = %2").arg(moduleInformation.id.toString()).arg(urlStr), cl_logINFO);

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2Connection->deleteRemotePeer(urlStr);

    QHostAddress host(url.host());
    int port = moduleInformation.port;

    QnResourcePtr mServer = qnResPool->getResourceById(moduleInformation.id);
    if (mServer && mServer->getStatus() == Qn::Unauthorized)
        mServer->setStatus(Qn::Offline);
}

void QnServerConnector::reconnect() {
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

    foreach (const QString &urlStr, m_usedUrls)
        ec2Connection->deleteRemotePeer(urlStr);
    m_usedUrls.clear();

    foreach (const QnModuleInformation &moduleInformation, m_moduleFinder->foundModules()) {
        if (!moduleInformation.isCompatibleToCurrentSystem())
            continue;

        foreach (const QString &address, moduleInformation.remoteAddresses) {
            QUrl url;
            url.setHost(address);
            url.setPort(moduleInformation.port);
            addConnection(moduleInformation, url);
        }
    }
}
