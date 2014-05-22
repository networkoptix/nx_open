#include "global_module_finder.h"

#include <utils/network/module_finder.h>
#include <common/common_module.h>
#include <api/app_server_connection.h>
#include <nx_ec/data/api_module_data.h>

QnGlobalModuleFinder::QnGlobalModuleFinder(QObject *parent) :
    QObject(parent)
{
}

void QnGlobalModuleFinder::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    if (m_connection)
        disconnect(m_connection->getMiscManager().get(),   &ec2::AbstractMiscManager::moduleChanged,  this,   &QnGlobalModuleFinder::at_moduleChanged);

    m_connection = connection;

    if (connection)
        connect(connection->getMiscManager().get(),        &ec2::AbstractMiscManager::moduleChanged,  this,   &QnGlobalModuleFinder::at_moduleChanged);
}

void QnGlobalModuleFinder::setModuleFinder(QnModuleFinder *moduleFinder) {
    if (m_moduleFinder){
        disconnect(m_moduleFinder.data(),   &QnModuleFinder::moduleFound,   this,   &QnGlobalModuleFinder::at_moduleFinder_moduleFound);
        disconnect(m_moduleFinder.data(),   &QnModuleFinder::moduleLost,    this,   &QnGlobalModuleFinder::at_moduleFinder_moduleLost);
    }

    m_moduleFinder = moduleFinder;

    if (moduleFinder) {
        foreach (const QnModuleInformation &moduleInformation, moduleFinder->revealedModules())
            at_moduleFinder_moduleFound(moduleInformation);

        connect(moduleFinder,               &QnModuleFinder::moduleFound,   this,   &QnGlobalModuleFinder::at_moduleFinder_moduleFound);
        connect(moduleFinder,               &QnModuleFinder::moduleLost,    this,   &QnGlobalModuleFinder::at_moduleFinder_moduleLost);
    }
}

void QnGlobalModuleFinder::fillApiModuleData(const QnModuleInformation &moduleInformation, ec2::ApiModuleData *data) {
    data->type = moduleInformation.type;
    data->id = moduleInformation.id;
    data->systemName = moduleInformation.systemName;
    data->version = moduleInformation.version.toString();
    data->systemInformation = moduleInformation.systemInformation.toString();
    data->addresses = moduleInformation.remoteAddresses.toList();
    data->port = moduleInformation.parameters.value(lit("port")).toInt();
    data->isAlive = true;
}

void QnGlobalModuleFinder::fillFromApiModuleData(const ec2::ApiModuleData &data, QnModuleInformation *moduleInformation) {
    moduleInformation->type = data.type;
    moduleInformation->id = data.id;
    moduleInformation->systemName = data.systemName;
    moduleInformation->version = QnSoftwareVersion(data.version);
    moduleInformation->systemInformation = QnSystemInformation(data.systemInformation);
    moduleInformation->remoteAddresses = QSet<QString>::fromList(data.addresses);
    moduleInformation->parameters.insert(lit("port"), QString::number(data.port));
}

QList<QnModuleInformation> QnGlobalModuleFinder::foundModules() const {
    return m_moduleInformationById.values();
}

QList<QnId> QnGlobalModuleFinder::discoverers(const QnId &moduleId) {
    return m_discovererIdByServerId.values(moduleId);
}

QnModuleInformation QnGlobalModuleFinder::moduleInformation(const QnId &id) const {
    return m_moduleInformationById[id];
}

void QnGlobalModuleFinder::at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive, const QnId &discoverer) {
    if (isAlive)
        addModule(moduleInformation, discoverer);
    else
        removeModule(moduleInformation, discoverer);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation) {
    addModule(moduleInformation, QnId(qnCommon->moduleGUID()));
}

void QnGlobalModuleFinder::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    removeModule(moduleInformation, QnId(qnCommon->moduleGUID()));
}

void QnGlobalModuleFinder::addModule(const QnModuleInformation &moduleInformation, const QnId &discoverer) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    auto it = m_moduleInformationById.find(moduleInformation.id);

    m_discovererIdByServerId.insert(moduleInformation.id, discoverer);

    if (it == m_moduleInformationById.end()) {
        m_moduleInformationById[moduleInformation.id] = moduleInformation;
        emit peerFound(moduleInformation);
    } else {
        *it = moduleInformation;
        emit peerChanged(moduleInformation);
    }
}

void QnGlobalModuleFinder::removeModule(const QnModuleInformation &moduleInformation, const QnId &discoverer) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    auto it = m_moduleInformationById.find(moduleInformation.id);

    if (it != m_moduleInformationById.end()) {
        m_moduleInformationById.erase(it);
        m_discovererIdByServerId.remove(moduleInformation.id, discoverer);
        emit peerLost(moduleInformation);
    }
}
