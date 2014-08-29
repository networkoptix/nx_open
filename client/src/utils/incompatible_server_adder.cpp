#include "incompatible_server_adder.h"

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/global_module_finder.h>

namespace {

QnMediaServerResourcePtr makeResource(const QnModuleInformation &moduleInformation) {
    QnMediaServerResourcePtr server(new QnMediaServerResource(qnResTypePool));

    server->setId(moduleInformation.id);
    server->setStatus(Qn::Incompatible, true);

    QList<QHostAddress> addressList;
    foreach (const QString &address, moduleInformation.remoteAddresses)
        addressList.append(QHostAddress(address));
    server->setNetAddrList(addressList);

    if (!addressList.isEmpty()) {
        QString url = QString(lit("http://%1:%2")).arg(addressList.first().toString()).arg(moduleInformation.port);
        server->setApiUrl(url);
        server->setUrl(url);
    }
    server->setVersion(moduleInformation.version);
    server->setSystemInfo(moduleInformation.systemInformation);
    server->setSystemName(moduleInformation.systemName);

    return server;
}

bool isSuitable(const QnModuleInformation &moduleInformation) {
    return moduleInformation.version >= QnSoftwareVersion(2, 3, 0, 0);
}

} // anonymous namespace

QnIncompatibleServerAdder::QnIncompatibleServerAdder(QObject *parent) :
    QObject(parent)
{
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerChanged, this,   &QnIncompatibleServerAdder::at_peerChanged);
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerLost,    this,   &QnIncompatibleServerAdder::at_peerLost);

    // fill resource pool with already found modules
    foreach (const QnModuleInformation &moduleInformation, QnGlobalModuleFinder::instance()->foundModules())
        at_peerChanged(moduleInformation);
}

void QnIncompatibleServerAdder::at_peerChanged(const QnModuleInformation &moduleInformation) {
    QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(moduleInformation.id, true).dynamicCast<QnMediaServerResource>();

    // don't add servers having version < 2.3 because they cannot be connected to the system anyway
    if (!isSuitable(moduleInformation))
        return;

    // don't touch normal servers
    if (server) {
        if (server->getStatus() == Qn::Offline || server->getStatus() == Qn::Incompatible) {
            server->setVersion(moduleInformation.version);
            server->setSystemInfo(moduleInformation.systemInformation);
            server->setSystemName(moduleInformation.systemName);
            server->setStatus(moduleInformation.isCompatibleToCurrentSystem() ? Qn::Online : Qn::Incompatible);

            QList<QHostAddress> addresses;
            foreach (const QString &address, moduleInformation.remoteAddresses) {
                QHostAddress hostAddress(address);
                if (hostAddress.isNull())
                    continue;
                addresses.append(hostAddress);
            }
            if (!addresses.isEmpty()) {
                server->setNetAddrList(addresses);
                QString url = QString(lit("http://%1:%2")).arg(addresses.first().toString()).arg(moduleInformation.port);
                server->setApiUrl(url);
                server->setUrl(url);
            }

            qnResPool->updateIncompatibility(server);
        }
        return;
    }

    qnResPool->addResource(makeResource(moduleInformation));
}

void QnIncompatibleServerAdder::at_peerLost(const QnModuleInformation &moduleInformation) {
    QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleInformation.id).dynamicCast<QnMediaServerResource>();

    // don't touch normal servers
    if (server && server->getStatus() != Qn::Incompatible)
        return;

    QnResourcePtr incompatibleResource = qnResPool->getIncompatibleResourceById(moduleInformation.id);

    if (incompatibleResource)
        qnResPool->removeResource(incompatibleResource);
    else if (server)
        server->setStatus(Qn::Offline);
}
