
#include "system_hosts_model.h"

#include <utils/math/math.h>
#include <finders/systems_finder.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/disconnect_helper.h>

QnSystemHostsModel::QnSystemHostsModel(QObject *parent)
    : base_type(parent)
    , m_disconnectHelper()
    , m_systemId()
    , m_hosts()
{
    connect(this, &QnSystemHostsModel::systemIdChanged
        , this, &QnSystemHostsModel::reloadHosts);
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

QString QnSystemHostsModel::firstHost() const
{
    return (m_hosts.isEmpty() ? QString() : m_hosts.front().second);
}

bool QnSystemHostsModel::isEmpty() const
{
    return m_hosts.empty();
}

int QnSystemHostsModel::rowCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? 0 : m_hosts.size());
}

QVariant QnSystemHostsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto row = index.row();
    if (!qBetween<int>(0, row, m_hosts.size()) || (role != Qt::DisplayRole))
        return QVariant();

    const auto hostData = m_hosts.at(row);
    return hostData.second;
}

void QnSystemHostsModel::reloadHosts()
{
    if (!m_hosts.isEmpty())
    {
        const auto removeGuard = QnRaiiGuard::create(
            [this]() { beginRemoveRows(QModelIndex(), 0, m_hosts.size() - 1); }
            , [this]() { endRemoveRows(); });

        m_hosts.clear();
    }

    m_disconnectHelper.reset();
    if (m_systemId.isNull())
        return;

    const auto reloadHostsHandler = [this](const QnSystemDescriptionPtr &system)
    {
        if (m_systemId != system->id())
            return;

        for (const auto server : system->servers())
            addServer(system, server.id, false);

        m_disconnectHelper.reset(new QnDisconnectHelper());

        const auto serverAddedConnection =
            connect(system, &QnSystemDescription::serverAdded, this
                , [this, system](const QnUuid &id)
        {
            addServer(system, id, true);
        });

        const auto serverRemovedConnection =
            connect(system, &QnSystemDescription::serverRemoved, this
                , [this, system](const QnUuid &id)
        {
            removeServer(system, id);
        });

        const auto changedConnection =
            connect(system, &QnSystemDescription::serverChanged, this
                , [this, system](const QnUuid &id, QnServerFields fields)
        {
            if (fields.testFlag(QnServerField::HostField))
                updateServerHost(system, id);
        });

        m_disconnectHelper->add(serverAddedConnection);
        m_disconnectHelper->add(serverRemovedConnection);
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
    , const QnUuid &serverId
    , bool tryUpdate)
{
    if (systemDescription->id() != m_systemId)
        return;

    if (tryUpdate && updateServerHost(systemDescription, serverId))
        return;

    const auto host = systemDescription->getServerHost(serverId);
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

bool QnSystemHostsModel::updateServerHost(const QnSystemDescriptionPtr &systemDescription
    , const QnUuid &serverId)
{
    if (systemDescription->id() != m_systemId)
        return false;

    const auto it = getDataIt(serverId);
    if (it == m_hosts.end())
    {
        addServer(systemDescription, serverId, false);
        return false;
    }

    const auto newHost = systemDescription->getServerHost(serverId);
    if (newHost.isEmpty())
    {
        removeServer(systemDescription, serverId);
        return true;
    }

    it->second = newHost;
    const auto row = (it - m_hosts.begin());
    const auto modelIndex = index(row);
    dataChanged(modelIndex, modelIndex);

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
