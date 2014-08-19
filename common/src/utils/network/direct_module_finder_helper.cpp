#include "direct_module_finder_helper.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <utils/network/direct_module_finder.h>

namespace {
    const int checkInterval = 30 * 1000;
}

QnDirectModuleFinderHelper::QnDirectModuleFinderHelper(QObject *parent) :
    QObject(parent),
    m_directModuleFinder(0)
{
    QTimer *timer = new QTimer(this);
    timer->setInterval(checkInterval);

    connect(qnResPool,      &QnResourcePool::resourceAdded,         this,       &QnDirectModuleFinderHelper::at_resourceAdded);
    connect(qnResPool,      &QnResourcePool::resourceChanged,       this,       &QnDirectModuleFinderHelper::at_resourceChanged);
    connect(qnResPool,      &QnResourcePool::resourceRemoved,       this,       &QnDirectModuleFinderHelper::at_resourceRemoved);

    connect(timer,          &QTimer::timeout,                       this,       &QnDirectModuleFinderHelper::at_timer_timeout);

    timer->start();
}

void QnDirectModuleFinderHelper::setDirectModuleFinder(QnDirectModuleFinder *directModuleFinder) {
    if (m_directModuleFinder == directModuleFinder)
        return;

    if (m_directModuleFinder) {
        for (auto it = m_addressesByServer.begin(); it != m_addressesByServer.end(); ++it) {
            quint16 port = m_portByServer[it.key()];
            foreach (const QHostAddress &address, it.value())
                m_directModuleFinder->removeAddress(address, port, it.key());
        }
    }

    m_directModuleFinder = directModuleFinder;

    if (m_directModuleFinder) {
        for (auto it = m_addressesByServer.begin(); it != m_addressesByServer.end(); ++it) {
            quint16 port = m_portByServer[it.key()];
            foreach (const QHostAddress &address, it.value())
                m_directModuleFinder->addAddress(address, port, it.key());
        }
    }
}

QnUrlSet QnDirectModuleFinderHelper::urlsForPeriodicalCheck() const {
    return m_urlsForPeriodicalCheck;
}

void QnDirectModuleFinderHelper::setUrlsForPeriodicalCheck(const QnUrlSet &urls) {
    m_urlsForPeriodicalCheck = urls;
}

void QnDirectModuleFinderHelper::at_resourceAdded(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QUuid serverId = server->getId();

    QnHostAddressSet addresses = QnHostAddressSet::fromList(server->getNetAddrList());
    quint16 port = QUrl(server->getApiUrl()).port();

    if (addresses.isEmpty() || port == 0)
        return;

    m_addressesByServer.insert(serverId, addresses);
    m_portByServer.insert(serverId, port);

    if (m_directModuleFinder) {
        foreach (const QHostAddress &address, addresses)
            m_directModuleFinder->addAddress(address, port, serverId);

        foreach (const QUrl &url, server->getAdditionalUrls())
            m_directModuleFinder->addUrl(url, serverId);

        foreach (const QUrl &url, server->getIgnoredUrls())
            m_directModuleFinder->addIgnoredModule(url, serverId);
    }

    connect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged);
}

void QnDirectModuleFinderHelper::at_resourceChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QUuid serverId = server->getId();

    QnHostAddressSet oldAddresses = m_addressesByServer.value(server->getId());
    QnHostAddressSet newAddresses = QnHostAddressSet::fromList(server->getNetAddrList());

    quint16 oldPort = m_portByServer.value(server->getId());
    quint16 newPort = QUrl(server->getApiUrl()).port();

    if (oldAddresses == newAddresses && oldPort == newPort)
        return;

    if (m_directModuleFinder) {
        QnHostAddressSet commonAddresses;
        if (oldPort == newPort)
            commonAddresses = oldAddresses & newAddresses;

        foreach (const QHostAddress &address, oldAddresses - commonAddresses)
            m_directModuleFinder->removeAddress(address, oldPort, serverId);

        foreach (const QHostAddress &address, newAddresses - commonAddresses)
            m_directModuleFinder->addAddress(address, newPort, serverId);

        if (oldPort != newPort) {
            foreach (const QUrl &url, server->getAdditionalUrls())
                m_directModuleFinder->removeUrl(url, serverId);

            foreach (const QUrl &url, server->getAdditionalUrls())
                m_directModuleFinder->addUrl(url, serverId);

            QUuid serverId = server->getId();

            QList<QUrl> ignoredUrls = m_directModuleFinder->ignoredModules().keys(serverId);

            foreach (const QUrl &url, ignoredUrls)
                m_directModuleFinder->removeIgnoredModule(url, serverId);

            foreach (const QUrl &url, ignoredUrls) {
                QUrl newUrl = url;
                newUrl.setPort(newPort);
                m_directModuleFinder->addIgnoredModule(newUrl, serverId);
            }
        }
    }

    m_addressesByServer[server->getId()] = newAddresses;
}

void QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QUuid serverId = server->getId();

    QnUrlSet oldIgnoredUrls = QnUrlSet::fromList(m_directModuleFinder->ignoredModules().keys(serverId));
    QnUrlSet newIgnoredUrls = QnUrlSet::fromList(server->getIgnoredUrls());

    if (oldIgnoredUrls != newIgnoredUrls) {
        QnUrlSet commonIgnnoredUrls = oldIgnoredUrls & newIgnoredUrls;

        foreach (const QUrl &url, oldIgnoredUrls - commonIgnnoredUrls)
            m_directModuleFinder->removeIgnoredModule(url, serverId);

        foreach (const QUrl &url, newIgnoredUrls - commonIgnnoredUrls)
            m_directModuleFinder->addIgnoredModule(url, serverId);
    }

    QnUrlSet oldAdditionalUrls = m_manualAddressesByServer.value(serverId);
    QnUrlSet newAdditionalUrls = QnUrlSet::fromList(server->getAdditionalUrls());

    if (oldAdditionalUrls != newAdditionalUrls) {
        QnUrlSet commonAdditionalUrls = oldAdditionalUrls & newAdditionalUrls;

        foreach (const QUrl &url, oldAdditionalUrls - commonAdditionalUrls)
            m_directModuleFinder->removeUrl(url, serverId);

        foreach (const QUrl &url, newAdditionalUrls - commonAdditionalUrls)
            m_directModuleFinder->addUrl(url, serverId);
    }
}

void QnDirectModuleFinderHelper::at_resourceRemoved(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QUuid serverId = server->getId();

    disconnect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged);

    QnHostAddressSet addresses = m_addressesByServer.take(serverId);
    quint16 port = m_portByServer.take(serverId);

    if (m_directModuleFinder) {
        foreach (const QHostAddress &address, addresses)
            m_directModuleFinder->removeAddress(address, port, serverId);

        foreach (const QUrl &url, server->getAdditionalUrls())
            m_directModuleFinder->removeUrl(url, serverId);

        foreach (const QUrl &url, server->getIgnoredUrls())
            m_directModuleFinder->removeIgnoredModule(url, serverId);
    }
}

void QnDirectModuleFinderHelper::at_timer_timeout() {
    if (!m_directModuleFinder)
        return;

    foreach (const QUrl &url, m_urlsForPeriodicalCheck)
        m_directModuleFinder->checkUrl(url);
}
