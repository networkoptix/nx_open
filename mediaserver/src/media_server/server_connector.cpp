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
        bool used;
        {
            QMutexLocker lock(&m_mutex);
            used = m_usedUrls.contains(trimmed);
        }
        if (used) {
            NX_LOG(lit("QnServerConnector: Module %1 has become incompatible. Url = %2, System name = %3, version = %4")
                   .arg(moduleInformation.id.toString())
                   .arg(url.toString())
                   .arg(moduleInformation.systemName)
                   .arg(moduleInformation.version.toString()),
                   cl_logINFO);
        }
        removeConnection(moduleInformation, url);
        return;
    }

    addConnection(moduleInformation, url);
}

void QnServerConnector::at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url) {
    removeConnection(moduleInformation, url);
}

void QnServerConnector::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation) {
    if (moduleInformation.isCompatibleToCurrentSystem()) {
        for (const QString &address: moduleInformation.remoteAddresses) {
            QUrl url;
            url.setHost(address);
            url.setPort(moduleInformation.port);
            addConnection(moduleInformation, url);
        }
    } else {
        for (const QString &address: moduleInformation.remoteAddresses) {
            QUrl url;
            url.setHost(address);
            url.setPort(moduleInformation.port);
            removeConnection(moduleInformation, url);
        }
    }
}

void QnServerConnector::addConnection(const QnModuleInformation &moduleInformation, const QUrl &url) {
    QUrl trimmed = trimmedUrl(url);

    UrlInfo urlInfo;

    {
        QMutexLocker lock(&m_mutex);

        if (m_usedUrls.contains(trimmed)) {
            NX_LOG(lit("QnServerConnector: Url %1 is already used.").arg(trimmed.toString()), cl_logINFO);
            return;
        }

        QUrl moduleUrl = trimmed;
        moduleUrl.setScheme(moduleInformation.sslAllowed ? lit("https") : lit("http"));
        urlInfo.urlString = moduleUrl.toString();
        urlInfo.peerId = moduleInformation.id;
        m_usedUrls.insert(trimmed, urlInfo);
    }

    NX_LOG(lit("QnServerConnector: Adding connection to module %1. Url = %2").arg(moduleInformation.id.toString()).arg(urlInfo.urlString), cl_logINFO);

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2Connection->addRemotePeer(urlInfo.urlString);
}

void QnServerConnector::removeConnection(const QnModuleInformation &moduleInformation, const QUrl &url) {
    QUrl trimmed = trimmedUrl(url);

    QMutexLocker lock(&m_mutex);
    UrlInfo urlInfo = m_usedUrls.take(trimmed);
    lock.unlock();
    if (urlInfo.peerId.isNull())
        return;

    NX_LOG(lit("QnServerConnector: Removing connection from module %1. Url = %2").arg(moduleInformation.id.toString()).arg(urlInfo.urlString), cl_logINFO);

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2Connection->deleteRemotePeer(urlInfo.urlString);

    QnResourcePtr mServer = qnResPool->getResourceById(moduleInformation.id);
    if (mServer && mServer->getStatus() == Qn::Unauthorized)
        mServer->setStatus(Qn::Offline);
}

void QnServerConnector::start() {
    for (const QnModuleInformation &moduleInformation: m_moduleFinder->foundModules()) {
        if (!moduleInformation.isCompatibleToCurrentSystem())
            continue;

        for (const QString &address: moduleInformation.remoteAddresses) {
            QUrl url;
            url.setHost(address);
            url.setPort(moduleInformation.port);
            addConnection(moduleInformation, url);
        }
    }
}

void QnServerConnector::stop() {
    QHash<QUrl, UrlInfo> usedUrls;
    {
        QMutexLocker lock(&m_mutex);
        usedUrls = m_usedUrls;
    }

    for (auto it = usedUrls.begin(); it != usedUrls.end(); ++it)
        removeConnection(m_moduleFinder->moduleInformation(it->peerId), it.key());

    {
        QMutexLocker lock(&m_mutex);
        m_usedUrls.clear();
    }
}

void QnServerConnector::restart() {
    stop();
    start();
}
