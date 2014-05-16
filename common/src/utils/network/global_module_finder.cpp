#include "global_module_finder.h"

#include <utils/network/modulefinder.h>
#include <common/common_module.h>
#include <api/app_server_connection.h>
#include <nx_ec/data/api_module_data.h>

QnGlobalModuleFinder::QnGlobalModuleFinder(QObject *parent) :
    QObject(parent)
{
}

void QnGlobalModuleFinder::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    if (m_connection)
        disconnect(m_connection->getModuleInformationManager().get(),   &ec2::AbstractModuleInformationManager::moduleChanged,  this,   &QnGlobalModuleFinder::at_moduleChanged);

    m_connection = connection;

    if (connection)
        connect(connection->getModuleInformationManager().get(),        &ec2::AbstractModuleInformationManager::moduleChanged,  this,   &QnGlobalModuleFinder::at_moduleChanged);
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
    data->port = moduleInformation.parameters.value(lit("port")).toInt();
    data->isAlive = true;
}

void QnGlobalModuleFinder::fillFromApiModuleData(const ec2::ApiModuleData &data, QnModuleInformation *moduleInformation) {
    moduleInformation->type = data.type;
    moduleInformation->id = data.id;
    moduleInformation->systemName = data.systemName;
    moduleInformation->version = QnSoftwareVersion(data.version);
    moduleInformation->systemInformation = QnSystemInformation(data.systemInformation);
    moduleInformation->parameters.insert(lit("port"), QString::number(data.port));
}

QList<QnModuleInformation> QnGlobalModuleFinder::foundModules() const {
    return m_moduleInformationById.values();
}

void QnGlobalModuleFinder::at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive) {
    if (isAlive)
        at_moduleFinder_moduleFound(moduleInformation);
    else
        at_moduleFinder_moduleLost(moduleInformation);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    auto it = m_moduleInformationById.find(moduleInformation.id);

    if (it == m_moduleInformationById.end()) {
        m_moduleInformationById[moduleInformation.id] = moduleInformation;
        emit peerFound(moduleInformation);
    } else {
        *it = moduleInformation;
        emit peerChanged(moduleInformation);
    }
}

void QnGlobalModuleFinder::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    auto it = m_moduleInformationById.find(moduleInformation.id);

    if (it != m_moduleInformationById.end()) {
        m_moduleInformationById.erase(it);
        emit peerLost(moduleInformation);
    }
}
