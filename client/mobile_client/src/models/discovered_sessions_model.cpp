#include "discovered_sessions_model.h"

#include <algorithm>

#include <network/module_finder.h>

namespace {
    const QnSoftwareVersion minimalSupportedVersion(2, 5, 0, 0);
}

QnDiscoveredSessionsModel::QnDiscoveredSessionsModel(QObject *parent)
    : QnLoginSessionsModel(parent),
      m_moduleFinder(QnModuleFinder::instance())
{
    connect(m_moduleFinder, &QnModuleFinder::moduleAddressFound, this, &QnDiscoveredSessionsModel::at_moduleFinder_moduleAddressFound);
    connect(m_moduleFinder, &QnModuleFinder::moduleAddressLost,  this, &QnDiscoveredSessionsModel::at_moduleFinder_moduleAddressLost);
    connect(m_moduleFinder, &QnModuleFinder::moduleChanged,      this, &QnDiscoveredSessionsModel::at_moduleFinder_moduleChanged);

    for (const ec2::ApiDiscoveredServerData &serverData: m_moduleFinder->discoveredServers())
        at_moduleFinder_moduleChanged(serverData);
}

QnDiscoveredSessionsModel::~QnDiscoveredSessionsModel()
{
}

int QnDiscoveredSessionsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_discoveredSessions.size();
}

QVariant QnDiscoveredSessionsModel::data(const QModelIndex &index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const DiscoveredSession *session = static_cast<const DiscoveredSession*>(this->session(index.row()));

    switch (role) {
    case IsCompatibleRole:
        return session->serverVersion >= minimalSupportedVersion;
    case ServerVersionRole:
        return session->serverVersion.toString(QnSoftwareVersion::BugfixFormat);
    default:
        break;
    }

    return QnLoginSessionsModel::data(index, role);
}

QHash<int, QByteArray> QnDiscoveredSessionsModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = QnLoginSessionsModel::roleNames();
    roleNames[IsCompatibleRole] = "isCompatible";
    roleNames[ServerVersionRole] = "serverVersion";
    return roleNames;
}

const QnLoginSession *QnDiscoveredSessionsModel::session(int row) const
{
    return &m_discoveredSessions[row];
}

void QnDiscoveredSessionsModel::at_moduleFinder_moduleAddressFound(
        const QnModuleInformation &moduleInformation,
        const SocketAddress &address)
{
    auto it = std::find_if(m_discoveredSessions.begin(), m_discoveredSessions.end(),
                           [&moduleInformation, &address](const DiscoveredSession &session)
    {
        return session.moduleId == moduleInformation.id &&
               session.address == address.address.toString() &&
               session.port == address.port;
    });

    if (it != m_discoveredSessions.end()) {
        it->address = address.address.toString();
        it->port = address.port;
        it->systemName = moduleInformation.systemName;
        it->serverVersion = moduleInformation.version;
        it->moduleId = moduleInformation.id;

        const QModelIndex idx = index(std::distance(m_discoveredSessions.begin(), it));
        emit dataChanged(idx, idx);
    } else {
        DiscoveredSession session;
        session.address = address.address.toString();
        session.port = address.port;
        session.systemName = moduleInformation.systemName;
        session.serverVersion = moduleInformation.version;
        session.moduleId = moduleInformation.id;

        int row = m_discoveredSessions.size();
        beginInsertRows(QModelIndex(), row, row);
        m_discoveredSessions.append(session);
        endInsertRows();
    }
}

void QnDiscoveredSessionsModel::at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const SocketAddress &address)
{
    auto it = std::find_if(m_discoveredSessions.begin(), m_discoveredSessions.end(),
                           [&moduleInformation, &address](const DiscoveredSession &session)
    {
        return session.moduleId == moduleInformation.id &&
               session.address == address.address.toString() &&
               session.port == address.port;
    });

    if (it == m_discoveredSessions.end())
        return;

    int row = std::distance(m_discoveredSessions.begin(), it);
    beginRemoveRows(QModelIndex(), row, row);
    m_discoveredSessions.erase(it);
    endRemoveRows();
}

void QnDiscoveredSessionsModel::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation)
{
    for (const SocketAddress &address: m_moduleFinder->moduleAddresses(moduleInformation.id))
        at_moduleFinder_moduleAddressFound(moduleInformation, address);
}
