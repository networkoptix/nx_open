#include "global_module_finder.h"

#include <common/common_module.h>
#include <api/app_server_connection.h>

QnGlobalModuleFinder::QnGlobalModuleFinder(QObject *parent) :
    QObject(parent)
{
}

void QnGlobalModuleFinder::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    if (m_connection) {
        disconnect(m_connection.get(),      &ec2::AbstractECConnection::remotePeerFound,        this,   &QnGlobalModuleFinder::at_peerFound);
        disconnect(m_connection.get(),      &ec2::AbstractECConnection::remotePeerLost,         this,   &QnGlobalModuleFinder::at_peerLost);
        disconnect(connection.get(),        &ec2::AbstractECConnection::incompatiblePeerFound,  this,   &QnGlobalModuleFinder::at_peerFound);
        disconnect(connection.get(),        &ec2::AbstractECConnection::incompatiblePeerLost,   this,   &QnGlobalModuleFinder::at_peerLost);
    }

    m_connection = connection;

    if (connection) {
        connect(connection.get(),           &ec2::AbstractECConnection::remotePeerFound,        this,   &QnGlobalModuleFinder::at_peerFound);
        connect(connection.get(),           &ec2::AbstractECConnection::remotePeerLost,         this,   &QnGlobalModuleFinder::at_peerLost);
        connect(connection.get(),           &ec2::AbstractECConnection::incompatiblePeerFound,  this,   &QnGlobalModuleFinder::at_peerFound);
        connect(connection.get(),           &ec2::AbstractECConnection::incompatiblePeerLost,   this,   &QnGlobalModuleFinder::at_peerLost);
    }
}

void QnGlobalModuleFinder::at_peerFound(ec2::ApiServerAliveData data) {
    if (data.serverId == qnCommon->moduleGUID())
        return;

    if (data.isClient)
        return;

    bool newPeer = false;

    auto it = m_moduleInformationById.find(data.serverId);
    if (it == m_moduleInformationById.end()) {
        it = m_moduleInformationById.insert(data.serverId, QnModuleInformation());
        it->type = lit("Media Server");
        it->id = data.serverId.toString();
        it->systemInformation = QnSystemInformation(data.systemInformation);

        newPeer = true;
    }
    it->version = QnSoftwareVersion(data.version);
    it->systemName = data.systemName;
    it->parameters[lit("port")] = data.port;

    if (newPeer)
        emit peerFound(*it);
    else
        emit peerChanged(*it);

    qDebug() << "Peer found: " << it->type << it->id << it->version.toString() << it->systemName;
}

void QnGlobalModuleFinder::at_peerLost(ec2::ApiServerAliveData data) {
    if (data.serverId == qnCommon->moduleGUID())
        return;

    if (data.isClient)
        return;

    auto it = m_moduleInformationById.find(data.serverId);
    if (it == m_moduleInformationById.end())
        return;

    emit peerLost(*it);
    m_moduleInformationById.erase(it);
    qDebug() << "Peer lost" << it->id;
}
