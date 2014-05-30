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
    }
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
    }

    m_addressesByServer[server->getId()] = newAddresses;
}

void QnDirectModuleFinderHelper::at_resourceRemoved(const QnResourcePtr &resource) {
    if (!resource->hasFlags(QnResource::server))
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    QnHostAddressSet addresses = m_addressesByServer.take(server->getId());
    quint16 port = m_portByServer.take(server->getId());

    if (m_directModuleFinder) {
        foreach (const QHostAddress &address, addresses)
            m_directModuleFinder->removeAddress(address, port);
    }
}
