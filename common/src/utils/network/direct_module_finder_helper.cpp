#include "direct_module_finder_helper.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <utils/network/module_finder.h>
#include <utils/network/multicast_module_finder.h>
#include <utils/network/direct_module_finder.h>

namespace {
    const int checkInterval = 30 * 1000;
}

QnModuleFinderHelper::QnModuleFinderHelper(QnModuleFinder *moduleFinder) :
    QObject(moduleFinder)
{
    Q_ASSERT(moduleFinder);

    m_multicastModuleFinder = moduleFinder->multicastModuleFinder();
    m_directModuleFinder = moduleFinder->directModuleFinder();

    for (const QnMediaServerResourcePtr &server: qnResPool->getAllServers())
        at_resourceAdded(server);

    QTimer *timer = new QTimer(this);
    timer->setInterval(checkInterval);

    connect(qnResPool,      &QnResourcePool::resourceAdded,         this,       &QnModuleFinderHelper::at_resourceAdded);
    connect(qnResPool,      &QnResourcePool::resourceChanged,       this,       &QnModuleFinderHelper::at_resourceChanged);
    connect(qnResPool,      &QnResourcePool::resourceRemoved,       this,       &QnModuleFinderHelper::at_resourceRemoved);

    connect(timer,          &QTimer::timeout,                       this,       &QnModuleFinderHelper::at_timer_timeout);

    timer->start();
}

QnUrlSet QnModuleFinderHelper::urlsForPeriodicalCheck() const {
    return m_urlsForPeriodicalCheck;
}

void QnModuleFinderHelper::setUrlsForPeriodicalCheck(const QnUrlSet &urls, bool check) {
    m_urlsForPeriodicalCheck = urls;
    if (check)
        at_timer_timeout();
}

void QnModuleFinderHelper::at_resourceAdded(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QnUuid serverId = server->getId();

    QnHostAddressSet addresses = QnHostAddressSet::fromList(server->getNetAddrList());
    quint16 port = server->getPort();

    if (addresses.isEmpty() || port == 0)
        return;

    m_addressesByServer.insert(serverId, addresses);
    m_portByServer.insert(serverId, port);

    if (m_multicastModuleFinder) {
        for (const QUrl &url: server->getIgnoredUrls()) {
            QHostAddress address(url.host());
            if (!address.isNull())
                continue;

            m_multicastModuleFinder->addIgnoredModule(QnNetworkAddress(address, url.port()), serverId);
        }
    }

    if (m_directModuleFinder) {
        for (const QHostAddress &address: addresses)
            m_directModuleFinder->addUrl(QnNetworkAddress(address, port).toUrl(), serverId);

        for (const QUrl &url: server->getAdditionalUrls())
            m_directModuleFinder->addUrl(url, serverId);

        for (const QUrl &url: server->getIgnoredUrls())
            m_directModuleFinder->addIgnoredModule(url, serverId);
    }

    connect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnModuleFinderHelper::at_resourceAuxUrlsChanged);
}

void QnModuleFinderHelper::at_resourceChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QnUuid serverId = server->getId();

    QnHostAddressSet oldAddresses = m_addressesByServer.value(server->getId());
    QnHostAddressSet newAddresses = QnHostAddressSet::fromList(server->getNetAddrList());

    quint16 oldPort = m_portByServer.value(server->getId());
    quint16 newPort = server->getPort();

    if (oldAddresses == newAddresses && oldPort == newPort)
        return;

    QnHostAddressSet commonAddresses;
    if (oldPort == newPort)
        commonAddresses = oldAddresses & newAddresses;

    if (m_multicastModuleFinder) {
        if (oldPort != newPort) {
            QList<QnNetworkAddress> ignoredAddresses = m_multicastModuleFinder->ignoredModules().keys(serverId);

            for (const QnNetworkAddress &address: ignoredAddresses)
                m_multicastModuleFinder->removeIgnoredModule(address, serverId);

            for (const QnNetworkAddress &address: ignoredAddresses)
                m_multicastModuleFinder->addIgnoredModule(QnNetworkAddress(address.host(), newPort), serverId);
        }
    }

    if (m_directModuleFinder) {
        QnHostAddressSet addressesToRemove = (newPort == oldPort) ? oldAddresses - commonAddresses : oldAddresses;
        for (const QHostAddress &address: addressesToRemove)
            m_directModuleFinder->removeUrl(QnNetworkAddress(address, oldPort).toUrl(), serverId);

        QnHostAddressSet addressesToAdd = (newPort == oldPort) ? newAddresses - commonAddresses : newAddresses;
        for (const QHostAddress &address: addressesToAdd)
            m_directModuleFinder->addUrl(QnNetworkAddress(address, newPort).toUrl(), serverId);

        if (oldPort != newPort) {
            for (const QUrl &url: server->getAdditionalUrls())
                m_directModuleFinder->removeUrl(url, serverId);

            for (const QUrl &url: server->getAdditionalUrls())
                m_directModuleFinder->addUrl(url, serverId);

            QList<QUrl> ignoredUrls = m_directModuleFinder->ignoredModules().keys(serverId);

            for (const QUrl &url: ignoredUrls)
                m_directModuleFinder->removeIgnoredModule(url, serverId);

            for (const QUrl &url: ignoredUrls) {
                QUrl newUrl = url;
                newUrl.setPort(newPort);
                m_directModuleFinder->addIgnoredModule(newUrl, serverId);
            }
        }
    }

    m_addressesByServer[server->getId()] = newAddresses;
}

void QnModuleFinderHelper::at_resourceAuxUrlsChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QnUuid serverId = server->getId();

    QnUrlSet newAdditionalUrls = QnUrlSet::fromList(server->getAdditionalUrls());
    QnUrlSet newIgnoredUrls = QnUrlSet::fromList(server->getIgnoredUrls());

    if (m_multicastModuleFinder) {
        QnUrlSet oldIgnoredUrls;
        for (const QnNetworkAddress &address: m_multicastModuleFinder->ignoredModules().keys(serverId))
            oldIgnoredUrls.insert(address.toUrl());

        if (oldIgnoredUrls != newIgnoredUrls) {
            QnUrlSet commonIgnnoredUrls = oldIgnoredUrls & newIgnoredUrls;

            for (const QUrl &url: oldIgnoredUrls - commonIgnnoredUrls)
                m_multicastModuleFinder->removeIgnoredModule(QnNetworkAddress(url), serverId);

            for (const QUrl &url: newIgnoredUrls - commonIgnnoredUrls)
                m_multicastModuleFinder->addIgnoredModule(QnNetworkAddress(url), serverId);
        }
    }

    if (m_directModuleFinder) {
        QnUrlSet oldAdditionalUrls = m_manualAddressesByServer.value(serverId);
        QnUrlSet oldIgnoredUrls = QnUrlSet::fromList(m_directModuleFinder->ignoredModules().keys(serverId));

        if (oldIgnoredUrls != newIgnoredUrls) {
            QnUrlSet commonIgnnoredUrls = oldIgnoredUrls & newIgnoredUrls;

            for (const QUrl &url: oldIgnoredUrls - commonIgnnoredUrls)
                m_directModuleFinder->removeIgnoredModule(url, serverId);

            for (const QUrl &url: newIgnoredUrls - commonIgnnoredUrls)
                m_directModuleFinder->addIgnoredModule(url, serverId);
        }

        if (oldAdditionalUrls != newAdditionalUrls) {
            QnUrlSet commonAdditionalUrls = oldAdditionalUrls & newAdditionalUrls;

            for (const QUrl &url: oldAdditionalUrls - commonAdditionalUrls)
                m_directModuleFinder->removeUrl(url, serverId);

            for (const QUrl &url: newAdditionalUrls - commonAdditionalUrls)
                m_directModuleFinder->addUrl(url, serverId);
        }
    }
}

void QnModuleFinderHelper::at_resourceRemoved(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server) || resource->getId() == qnCommon->moduleGUID())
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    QnUuid serverId = server->getId();

    disconnect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnModuleFinderHelper::at_resourceAuxUrlsChanged);

    QnHostAddressSet addresses = m_addressesByServer.take(serverId);
    quint16 port = m_portByServer.take(serverId);

    if (m_multicastModuleFinder) {
        for (const QUrl &url: server->getIgnoredUrls()) {
            QnNetworkAddress address(url);
            if (!address.isValid())
                continue;

            m_multicastModuleFinder->removeIgnoredModule(address, serverId);
        }
    }

    if (m_directModuleFinder) {
        for (const QHostAddress &address: addresses)
            m_directModuleFinder->removeUrl(QnNetworkAddress(address, port).toUrl(), serverId);

        for (const QUrl &url: server->getAdditionalUrls())
            m_directModuleFinder->removeUrl(url, serverId);

        for (const QUrl &url: server->getIgnoredUrls())
            m_directModuleFinder->removeIgnoredModule(url, serverId);
    }
}

void QnModuleFinderHelper::at_timer_timeout() {
    if (!m_directModuleFinder)
        return;

    for (const QUrl &url: m_urlsForPeriodicalCheck)
        m_directModuleFinder->checkUrl(url);
}
