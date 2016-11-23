#include "system_hosts_model.h"

#include <utils/math/math.h>
#include <utils/common/util.h>
#include <finders/systems_finder.h>
#include <nx/utils/disconnect_helper.h>

namespace {

static const int kUrlRole = Qt::UserRole + 1;

typedef QHash<int, QByteArray> RolesHash;
static const RolesHash kRoles =
{
    {Qt::DisplayRole, "display"},
    {kUrlRole, "url"}
};

}

QnSystemHostsModel::QnSystemHostsModel(QObject* parent):
    base_type(parent),
    m_disconnectHelper(),
    m_systemId(),
    m_hosts()
{
    connect(this, &QnSystemHostsModel::systemIdChanged, this, &QnSystemHostsModel::reloadHosts);

    connect(this, &QnSystemHostsModel::modelReset, this, &QnSystemHostsModel::countChanged);
    connect(this, &QnSystemHostsModel::rowsInserted, this, &QnSystemHostsModel::countChanged);
    connect(this, &QnSystemHostsModel::rowsRemoved, this, &QnSystemHostsModel::countChanged);
}

QnSystemHostsModel::~QnSystemHostsModel()
{}

QString QnSystemHostsModel::systemId() const
{
    return m_systemId;
}

void QnSystemHostsModel::setSystemId(const QString &id)
{
    if (m_systemId == id)
        return;

    m_systemId = id;
    emit systemIdChanged();
}

QUrl QnSystemHostsModel::firstHost() const
{
    return (m_hosts.isEmpty() ? QUrl() : m_hosts.front().second);
}

bool QnSystemHostsModel::isEmpty() const
{
    return m_hosts.empty();
}

int QnSystemHostsModel::count() const
{
    return m_hosts.size();
}

int QnSystemHostsModel::rowCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? 0 : m_hosts.size());
}

QVariant QnSystemHostsModel::getData(const QString& dataRole, int row)
{
    const auto role = kRoles.key(dataRole.toLatin1(), Qt::DisplayRole);
    return data(index(row), role);
}

QVariant QnSystemHostsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto row = index.row();
    if (!qBetween<int>(0, row, m_hosts.size()))
        return QVariant();

    const auto& hostData = m_hosts.at(row);
    switch (role)
    {
        case Qt::DisplayRole:
        {
            if (hostData.second.port() != DEFAULT_APPSERVER_PORT)
                return lit("%1:%2").arg(hostData.second.host(), QString::number(hostData.second.port()));

            return hostData.second.host();
        }
        case kUrlRole:
            return hostData.second.toString();
        default:
            break;
    }
    return QVariant();
}

void QnSystemHostsModel::reloadHosts()
{
    if (!m_hosts.isEmpty())
    {
        beginResetModel();
        m_hosts.clear();
        endResetModel();

        emit firstHostChanged();
        emit isEmptyChanged();
        emit countChanged();
    }

    m_disconnectHelper.reset(new QnDisconnectHelper());
    if (m_systemId.isNull())
        return;

    const auto reloadHostsHandler = [this](const QnSystemDescriptionPtr &system)
    {
        if (m_systemId != system->id())
            return;

        for (const auto server : system->servers())
            addServer(system, server.id);

        const auto serverAddedConnection =
            connect(system, &QnBaseSystemDescription::serverAdded, this
                , [this, system](const QnUuid &id)
        {
            addServer(system, id);
        });

        const auto serverRemovedConnection =
            connect(system, &QnBaseSystemDescription::serverRemoved, this
                , [this, system](const QnUuid &id)
        {
            removeServer(system, id);
        });

        const auto changedConnection =
            connect(system, &QnBaseSystemDescription::serverChanged, this
                , [this, system](const QnUuid &id, QnServerFields fields)
        {
            if (fields.testFlag(QnServerField::Host))
                updateServerHost(system, id);
        });

        m_disconnectHelper->add(serverAddedConnection);
        m_disconnectHelper->add(serverRemovedConnection);
        m_disconnectHelper->add(changedConnection);
    };

    const auto system = qnSystemsFinder->getSystem(m_systemId);
    if (!system)
    {
        const auto reloadHostsConnection = connect(qnSystemsFinder
            , &QnAbstractSystemsFinder::systemDiscovered
            , this, reloadHostsHandler);

        m_disconnectHelper->add(reloadHostsConnection);
        return;
    }

    reloadHostsHandler(system);
}

void QnSystemHostsModel::addServer(const QnSystemDescriptionPtr &systemDescription
    , const QnUuid &serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    const auto host = systemDescription->getServerHost(serverId);
    if (it != m_hosts.end())
    {
        updateServerHostInternal(it, host);
        return;
    }

    if (host.isEmpty())
        return;

    bool onlineStatusChanged = (m_hosts.isEmpty());
    beginInsertRows(QModelIndex(), m_hosts.size(), m_hosts.size());
    m_hosts.append(ServerIdHostPair(serverId, host));
    endInsertRows();

    if (onlineStatusChanged)
    {
        emit firstHostChanged();
        emit isEmptyChanged();
    }
}

void QnSystemHostsModel::updateServerHost(const QnSystemDescriptionPtr &systemDescription
    , const QnUuid &serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    const auto host = systemDescription->getServerHost(serverId);
    if (!updateServerHostInternal(it, host))
        addServer(systemDescription, serverId);
}

bool QnSystemHostsModel::updateServerHostInternal(const ServerIdHostList::iterator &it
    , const QUrl& host)
{
    if (it == m_hosts.end())
        return false;

    if (host.isEmpty())
    {
        removeServerInternal(it);
        return true;
    }

    it->second = host;
    const auto row = (it - m_hosts.begin());
    const auto modelIndex = index(row);
    emit dataChanged(modelIndex, modelIndex);


    if (row == 0)
        emit firstHostChanged();

    return true;
}

void QnSystemHostsModel::removeServer(const QnSystemDescriptionPtr &systemDescription
    , const QnUuid &serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    removeServerInternal(it);
}

void QnSystemHostsModel::removeServerInternal(const ServerIdHostList::iterator &it)
{
    if (it == m_hosts.end())
        return;

    const auto row = it - m_hosts.begin();
    beginRemoveRows(QModelIndex(), row, row);
    m_hosts.erase(it);
    endRemoveRows();

    if (row == 0)
    {
        emit firstHostChanged();
        if (m_hosts.isEmpty())
            emit isEmptyChanged();
    }
}

QnSystemHostsModel::ServerIdHostList::iterator
QnSystemHostsModel::getDataIt(const QnUuid &serverId)
{
    return std::find_if(m_hosts.begin(), m_hosts.end()
        , [serverId](const ServerIdHostPair &value)
    {
        return (value.first == serverId);
    });
}

RolesHash QnSystemHostsModel::roleNames() const
{
    return kRoles;
}
