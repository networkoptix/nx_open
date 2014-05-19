#include "incompatible_server_adder.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/global_module_finder.h>

namespace {

QnMediaServerResourcePtr makeResource(const QnModuleInformation &moduleInformation) {
    QnMediaServerResourcePtr server(new QnMediaServerResource(qnResTypePool));

    server->setId(moduleInformation.id);
    server->setStatus(QnResource::Incompatible, true);

    QList<QHostAddress> addressList;
    foreach (const QString &address, moduleInformation.remoteAddresses)
        addressList.append(QHostAddress(address));
    server->setNetAddrList(addressList);

    if (!addressList.isEmpty()) {
        QString url = QString(lit("http://%1:%2")).arg(addressList.first().toString()).arg(moduleInformation.parameters.value(lit("port")).toInt());
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
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerFound,   this,   &QnIncompatibleServerAdder::at_peerFound);
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerLost,    this,   &QnIncompatibleServerAdder::at_peerLost);
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerChanged, this,   &QnIncompatibleServerAdder::at_peerChanged);

    // fill resource pool with already found modules
    foreach (const QnModuleInformation &moduleInformation, QnGlobalModuleFinder::instance()->foundModules())
        at_peerFound(moduleInformation);
}


void QnIncompatibleServerAdder::at_peerFound(const QnModuleInformation &moduleInformation) {
    at_peerChanged(moduleInformation);
}

void QnIncompatibleServerAdder::at_peerChanged(const QnModuleInformation &moduleInformation) {
    QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleInformation.id).dynamicCast<QnMediaServerResource>();

    // don't touch normal servers
    if (server && server->getStatus() != QnResource::Incompatible)
        return;

    // don't add servers having version < 2.3 because they cannot be connected to the system anyway
    if (!isSuitable(moduleInformation))
        return;

    QnMediaServerResourcePtr newServer = makeResource(moduleInformation);

    if (server)
        server->update(newServer);
    else
        qnResPool->addResource(newServer);
}

void QnIncompatibleServerAdder::at_peerLost(const QnModuleInformation &moduleInformation) {
    QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleInformation.id).dynamicCast<QnMediaServerResource>();

    // don't touch normal servers
    if (server && server->getStatus() != QnResource::Incompatible)
        return;

    if (server)
        qnResPool->removeResource(server);
}
