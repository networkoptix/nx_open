#include "system_hosts_model.h"

#include <utils/math/math.h>
#include <utils/common/util.h>
#include <utils/common/connective.h>
#include <finders/systems_finder.h>
#include <nx/utils/disconnect_helper.h>
#include <nx/utils/algorithm/index_of.h>
#include <network/system_description.h>
#include <client_core/client_core_settings.h>

namespace {
using UrlsList = QList<QUrl>;

bool isSamePort(int first, int second)
{
    static const auto isDefaultPort =
        [](int port)
        {
            return ((port == DEFAULT_APPSERVER_PORT) || (port == 0) || (port == -1));
        };

    return ((first == second) || (isDefaultPort(first) == isDefaultPort(second)));
}

} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////
class QnSystemHostsModel::HostsModel: public Connective<QAbstractListModel>
{
    using base_type = Connective<QAbstractListModel>;

public:
    HostsModel(QnSystemHostsModel* parent);

public:
    QString systemId() const;
    void setSystemId(const QString& id);

    QUuid localSystemId() const;
    void setLocalSystemId(const QUuid& id);

    UrlsList recentConnectionUrls() const;

public: // overrides
    int rowCount(const QModelIndex& parent) const override;

    QVariant data(const QModelIndex& index, int role) const override;

    QHash<int, QByteArray> roleNames() const override;

private:
    void reloadHosts();

    void addServer(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId);

    typedef QPair<QnUuid, QUrl> ServerIdHostPair;
    typedef QList<ServerIdHostPair> ServerIdHostList;
    void updateServerHost(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId);
    bool updateServerHostInternal(
        const ServerIdHostList::iterator& it,
        const QUrl& host);

    void removeServer(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId);
    void removeServerInternal(const ServerIdHostList::iterator& it);

    void forceResort();

    ServerIdHostList::iterator getDataIt(const QnUuid& serverId);

private:
    QnSystemHostsModel* m_owner;

    QnDisconnectHelperPtr m_disconnectHelper;
    QString m_systemId;
    QUuid m_localSystemId;
    ServerIdHostList m_hosts;
    UrlsList m_recentConnectionUrls;
};

QnSystemHostsModel::HostsModel::HostsModel(QnSystemHostsModel* parent):
    base_type(parent),
    m_owner(parent)
{
    connect(m_owner, &QnSystemHostsModel::systemIdChanged, this, &HostsModel::reloadHosts);
    connect(m_owner, &QnSystemHostsModel::localSystemIdChanged, this, &HostsModel::forceResort);
}

void QnSystemHostsModel::HostsModel::forceResort()
{
    const auto connections = qnClientCoreSettings->recentLocalConnections();
    const auto it = connections.find(m_localSystemId);
    const auto urls = (it == connections.end() ? UrlsList() : it.value().urls);

    if (urls == m_recentConnectionUrls)
        return;

    m_recentConnectionUrls = urls;
    m_owner->forceUpdate();
}

QString QnSystemHostsModel::HostsModel::systemId() const
{
    return m_systemId;
}

void QnSystemHostsModel::HostsModel::setSystemId(const QString& id)
{
    if (m_systemId == id)
        return;

    m_systemId = id;
    emit m_owner->systemIdChanged();
}

QUuid QnSystemHostsModel::HostsModel::localSystemId() const
{
    return m_localSystemId;
}

void QnSystemHostsModel::HostsModel::setLocalSystemId(const QUuid& id)
{
    if (m_localSystemId == id)
        return;

    m_localSystemId = id;
    emit m_owner->localSystemIdChanged();
}

UrlsList QnSystemHostsModel::HostsModel::recentConnectionUrls() const
{
    return m_recentConnectionUrls;
}

int QnSystemHostsModel::HostsModel::rowCount(const QModelIndex& parent) const
{
    return (parent.isValid() ? 0 : m_hosts.size());
}

QVariant QnSystemHostsModel::HostsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto row = index.row();
    if (!qBetween<int>(0, row, m_hosts.size()))
        return QVariant();

    const auto& data = m_hosts.at(row);
    switch (role)
    {
        case Qt::DisplayRole:
            return (data.second.port() != DEFAULT_APPSERVER_PORT
                ? lit("%1:%2").arg(data.second.host(), QString::number(data.second.port()))
                : data.second.host());
        case UrlRole:
            return data.second.toString();
        default:
            break;
    }
    return QVariant();
}

void QnSystemHostsModel::HostsModel::reloadHosts()
{
    if (!m_hosts.isEmpty())
    {
        beginResetModel();
        m_hosts.clear();
        endResetModel();
    }

    m_disconnectHelper.reset(new QnDisconnectHelper());
    if (m_systemId.isNull())
        return;

    const auto reloadHostsHandler = [this](const QnSystemDescriptionPtr& system)
    {
        if (m_systemId != system->id())
            return;

        for (const auto server: system->servers())
            addServer(system, server.id);

        const auto serverAddedConnection =
            connect(system, &QnBaseSystemDescription::serverAdded, this,
                [this, system](const QnUuid& id) { addServer(system, id); });

        const auto serverRemovedConnection =
            connect(system, &QnBaseSystemDescription::serverRemoved, this,
                [this, system](const QnUuid& id) { removeServer(system, id); });

        const auto changedConnection =
            connect(system, &QnBaseSystemDescription::serverChanged, this,
                [this, system](const QnUuid& id, QnServerFields fields)
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
        const auto reloadHostsConnection =
            connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
                this, reloadHostsHandler);

        m_disconnectHelper->add(reloadHostsConnection);
        return;
    }

    reloadHostsHandler(system);
}

void QnSystemHostsModel::HostsModel::addServer(
    const QnSystemDescriptionPtr& systemDescription,
    const QnUuid& serverId)
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

    beginInsertRows(QModelIndex(), m_hosts.size(), m_hosts.size());
    m_hosts.append(ServerIdHostPair(serverId, host));
    endInsertRows();
}

void QnSystemHostsModel::HostsModel::updateServerHost(
    const QnSystemDescriptionPtr& systemDescription,
    const QnUuid& serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    const auto host = systemDescription->getServerHost(serverId);
    if (!updateServerHostInternal(it, host))
        addServer(systemDescription, serverId);
}

bool QnSystemHostsModel::HostsModel::updateServerHostInternal(
    const ServerIdHostList::iterator& it,
    const QUrl& host)
{
    if (it == m_hosts.end())
        return false;

    if (host.isEmpty())
    {
        removeServerInternal(it);
        return true;
    }

    it->second = host;

    const auto row = std::distance(m_hosts.begin(), it);
    NX_ASSERT(row >= 0);

    const auto modelIndex = index(row);
    NX_ASSERT(modelIndex.isValid());

    emit dataChanged(modelIndex, modelIndex);

    return true;
}

void QnSystemHostsModel::HostsModel::removeServer(
    const QnSystemDescriptionPtr& systemDescription,
    const QnUuid &serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    removeServerInternal(it);
}

void QnSystemHostsModel::HostsModel::removeServerInternal(const ServerIdHostList::iterator& it)
{
    if (it == m_hosts.end())
        return;

    const auto row = std::distance(m_hosts.begin(), it);
    NX_ASSERT(row >= 0);

    beginRemoveRows(QModelIndex(), row, row);
    m_hosts.erase(it);
    endRemoveRows();
}

QnSystemHostsModel::HostsModel::ServerIdHostList::iterator
QnSystemHostsModel::HostsModel::getDataIt(const QnUuid& serverId)
{
    return std::find_if(m_hosts.begin(), m_hosts.end(),
        [serverId](const ServerIdHostPair& value)
        {
            return (value.first == serverId);
        });
}

QHash<int, QByteArray> QnSystemHostsModel::HostsModel::roleNames() const
{
    static const auto kRoles = QHash<int, QByteArray>{{QnSystemHostsModel::UrlRole, "url"}};
    return base_type::roleNames().unite(kRoles);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

QnSystemHostsModel::QnSystemHostsModel(QObject* parent):
    base_type(parent)
{
    setSourceModel(new HostsModel(this));
}

bool QnSystemHostsModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const auto leftUrl = sourceLeft.data(UrlRole).toUrl();
    const auto rightUrl = sourceRight.data(UrlRole).toUrl();
    if (!leftUrl.isValid() || !rightUrl.isValid())
    {
        NX_ASSERT(false, "Urls should be valid");
        return sourceLeft.row() < sourceRight.row();
    }

    const auto recentUrls = hostsModel()->recentConnectionUrls();
    const auto getIndexOfConnection =
        [recentUrls](const QUrl& url) -> int
        {
            return nx::utils::algorithm::index_of(recentUrls,
                [url, recentUrls](const QUrl& recentUrl)
                {
                    return ((recentUrl.host() == url.host())
                        && isSamePort(recentUrl.port(), url.port()));
                });
        };

    const int leftIndex = getIndexOfConnection(leftUrl);
    const int rightIndex = getIndexOfConnection(rightUrl);
    if (leftIndex == rightIndex)
        return (sourceLeft.row() < sourceRight.row());

    if (leftIndex == -1)
        return false;

    if (rightIndex == -1)
        return true;

    return (leftIndex < rightIndex);
}

QString QnSystemHostsModel::systemId() const
{
    return hostsModel()->systemId();
}

void QnSystemHostsModel::setSystemId(const QString& id)
{
    hostsModel()->setSystemId(id);
}

QUuid QnSystemHostsModel::localSystemId() const
{
    return hostsModel()->localSystemId();
}

void QnSystemHostsModel::setLocalSystemId(const QUuid& id)
{
    hostsModel()->setLocalSystemId(id);
}

QnSystemHostsModel::HostsModel* QnSystemHostsModel::hostsModel() const
{
    return static_cast<HostsModel*>(sourceModel());
}
