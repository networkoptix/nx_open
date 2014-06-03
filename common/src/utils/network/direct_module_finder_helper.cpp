#include "direct_module_finder_helper.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/network/direct_module_finder.h>

QnDirectModuleFinderHelper::QnDirectModuleFinderHelper(QObject *parent) :
    QObject(parent),
    m_directModuleFinder(0)
{
    connect(qnResPool,      &QnResourcePool::resourceAdded,         this,       &QnDirectModuleFinderHelper::at_resourceAdded);
    connect(qnResPool,      &QnResourcePool::resourceChanged,       this,       &QnDirectModuleFinderHelper::at_resourceChanged);
    connect(qnResPool,      &QnResourcePool::resourceRemoved,       this,       &QnDirectModuleFinderHelper::at_resourceRemoved);
}

void QnDirectModuleFinderHelper::setDirectModuleFinder(QnDirectModuleFinder *directModuleFinder) {
    if (m_directModuleFinder == directModuleFinder)
        return;

    if (m_directModuleFinder) {
        for (auto it = m_addressesByServer.begin(); it != m_addressesByServer.end(); ++it) {
            quint16 port = m_portByServer[it.key()];
            foreach (const QHostAddress &address, it.value())
                m_directModuleFinder->removeAddress(address, port);
        }
    }

    m_directModuleFinder = directModuleFinder;

    if (m_directModuleFinder) {
        for (auto it = m_addressesByServer.begin(); it != m_addressesByServer.end(); ++it) {
            quint16 port = m_portByServer[it.key()];
            foreach (const QHostAddress &address, it.value())
                m_directModuleFinder->addAddress(address, port);
        }
    }
}

void QnDirectModuleFinderHelper::at_resourceAdded(const QnResourcePtr &resource) {
    if (!resource->hasFlags(QnResource::server))
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    QnHostAddressSet addresses = QnHostAddressSet::fromList(server->getNetAddrList());
    quint16 port = QUrl(server->getApiUrl()).port();

    if (addresses.isEmpty() || port == 0)
        return;

    m_addressesByServer.insert(server->getId(), addresses);
    m_portByServer.insert(server->getId(), port);

    if (m_directModuleFinder) {
        foreach (const QHostAddress &address, addresses)
            m_directModuleFinder->addAddress(address, port);

        foreach (const QUrl &url, server->getAdditionalUrls())
            m_directModuleFinder->addManualAddress(QHostAddress(url.host()), url.port());

        foreach (const QUrl &url, server->getIgnoredUrls())
            m_directModuleFinder->addIgnoredModule(server->getId(), QHostAddress(url.host()), url.port());
    }

    connect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged);
}

void QnDirectModuleFinderHelper::at_resourceChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(QnResource::server))
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

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
            m_directModuleFinder->removeAddress(address, oldPort);

        foreach (const QHostAddress &address, newAddresses - commonAddresses)
            m_directModuleFinder->addAddress(address, newPort);

        if (oldPort != newPort) {
            foreach (const QUrl &url, server->getAdditionalUrls())
                m_directModuleFinder->removeAddress(QHostAddress(url.host()), oldPort);

            foreach (const QUrl &url, server->getAdditionalUrls())
                m_directModuleFinder->addAddress(QHostAddress(url.host()), newPort);

            QnId serverId = server->getId();

            foreach (const QUrl &url, m_directModuleFinder->ignoredModules().values(serverId))
                m_directModuleFinder->removeIgnoredModule(serverId, QHostAddress(url.host()), oldPort);

            foreach (const QUrl &url, m_directModuleFinder->ignoredModules().values(serverId))
                m_directModuleFinder->addIgnoredModule(serverId, QHostAddress(url.host()), newPort);
        }
    }

    m_addressesByServer[server->getId()] = newAddresses;
}

void QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(QnResource::server))
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    QnId serverId = server->getId();

    QnUrlSet oldIgnoredUrls = QnUrlSet::fromList(m_directModuleFinder->ignoredModules().values(server->getId()));
    QnUrlSet newIgnoredUrls = QnUrlSet::fromList(server->getIgnoredUrls());

    if (oldIgnoredUrls != newIgnoredUrls) {
        QnUrlSet commonIgnnoredUrls = oldIgnoredUrls & newIgnoredUrls;

        foreach (const QUrl &url, oldIgnoredUrls - commonIgnnoredUrls)
            m_directModuleFinder->removeIgnoredModule(serverId, QHostAddress(url.host()), url.port());

        foreach (const QUrl &url, newIgnoredUrls - commonIgnnoredUrls)
            m_directModuleFinder->addIgnoredModule(serverId, QHostAddress(url.host()), url.port());
    }

    QnUrlSet oldAdditionalUrls = m_manualAddressesByServer.value(serverId);
    QnUrlSet newAdditionalUrls = QnUrlSet::fromList(server->getAdditionalUrls());

    if (oldAdditionalUrls != newAdditionalUrls) {
        QnUrlSet commonAdditionalUrls = oldAdditionalUrls & newAdditionalUrls;

        foreach (const QUrl &url, oldAdditionalUrls - commonAdditionalUrls)
            m_directModuleFinder->removeManualAddress(QHostAddress(url.host()), url.port());

        foreach (const QUrl &url, newAdditionalUrls - commonAdditionalUrls)
            m_directModuleFinder->addManualAddress(QHostAddress(url.host()), url.port());
    }
}

void QnDirectModuleFinderHelper::at_resourceRemoved(const QnResourcePtr &resource) {
    if (!resource->hasFlags(QnResource::server))
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    disconnect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnDirectModuleFinderHelper::at_resourceAuxUrlsChanged);

    QnHostAddressSet addresses = m_addressesByServer.take(server->getId());
    quint16 port = m_portByServer.take(server->getId());

    if (m_directModuleFinder) {
        foreach (const QHostAddress &address, addresses)
            m_directModuleFinder->removeAddress(address, port);

        foreach (const QUrl &url, server->getAdditionalUrls())
            m_directModuleFinder->removeManualAddress(QHostAddress(url.host()), url.port());

        foreach (const QUrl &url, server->getIgnoredUrls())
            m_directModuleFinder->removeIgnoredModule(server->getId(), QHostAddress(url.host()), url.port());
    }
}
