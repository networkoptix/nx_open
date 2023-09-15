// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_hosts_model.h"

#include <limits>

#include <finders/systems_finder.h>
#include <network/system_description.h>
#include <network/system_helpers.h>
#include <nx/network/socket_common.h>
#include <nx/utils/algorithm/index_of.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/local_network_interfaces_manager.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

using namespace nx::vms::client::core;

namespace {

using UrlsList = QList<nx::utils::Url>;

constexpr int kLowestServerPriority = std::numeric_limits<int>::max();

QSet<nx::utils::Url> formAdditionalUrlsSet(
    const QnSystemDescriptionPtr& systemDescription,
    const QnUuid& serverId)
{
    const auto mainServerUrl = systemDescription->getServerHost(serverId);

    // Only connection url is valid for localhost determination.
    if (QnSystemHostsModel::isLocalhost(mainServerUrl.host(), true))
    {
        nx::utils::Url localhostUrl;
        localhostUrl.setScheme("https");
        localhostUrl.setHost(nx::network::HostAddress::localhost.toString());
        localhostUrl.setPort(mainServerUrl.port());

        nx::utils::Url localhostIpUrl;
        localhostIpUrl.setScheme("https");
        localhostIpUrl.setHost(QHostAddress(QHostAddress::LocalHost).toString());
        localhostIpUrl.setPort(mainServerUrl.port());

        return systemDescription->getServerRemoteAddresses(serverId)
            + QSet<nx::utils::Url>{localhostUrl, localhostIpUrl};
    }

    return systemDescription->getServerRemoteAddresses(serverId);
}

} // namespace

//-------------------------------------------------------------------------------------------------
// HostsModel

class QnSystemHostsModel::HostsModel: public QAbstractListModel
{
    using base_type = QAbstractListModel;

    struct ServerUrlData
    {
        QnUuid id;
        nx::utils::Url mainUrl;
        QSet<nx::utils::Url> additionalUrls;
        bool compatible = true;
        int priority = kLowestServerPriority;

        bool containsUrl(const nx::utils::Url& url) const
        {
            return additionalUrls.contains(url) || mainUrl == url;
        }
    };
    typedef QList<ServerUrlData> ServerUrlDataList;

public:
    HostsModel(QnSystemHostsModel* parent);

public:
    QString systemId() const;
    void setSystemId(const QString& id);

    QUuid localSystemId() const;
    void setLocalSystemId(const QUuid& id);

    bool getUrlVersionCompatibility(const nx::utils::Url& url) const;
    int getUrlPriority(const nx::utils::Url& url) const;

    bool forcedLocalhostConversion() const;
    void setForcedLocalhostConversion(bool convert);

public: // overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role) const override;

    QHash<int, QByteArray> roleNames() const override;

private:
    void reloadHosts();

    void addServer(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId);
    void updateServerHost(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId);
    bool updateServerHostInternal(
        const ServerUrlDataList::iterator& it,
        const QnSystemDescriptionPtr& systemDescription);

    void removeServer(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId);
    void removeServerInternal(const ServerUrlDataList::iterator& it);

    void forceResort();

    ServerUrlDataList::iterator getDataIt(const QnUuid& serverId);

private:
    QnSystemHostsModel* const m_owner;

    nx::utils::ScopedConnections m_connections;
    QString m_systemId;
    QUuid m_localSystemId;
    ServerUrlDataList m_serversUrlData;
    UrlsList m_recentConnectionUrls;
    bool m_forcedLocalhostConversion = false;
};

QnSystemHostsModel::HostsModel::HostsModel(QnSystemHostsModel* parent):
    base_type(parent),
    m_owner(parent)
{
    connect(m_owner, &QnSystemHostsModel::systemIdChanged, this, &HostsModel::reloadHosts);
    connect(m_owner, &QnSystemHostsModel::localSystemIdChanged, this, &HostsModel::forceResort);
    connect(qnSystemsFinder, &QnSystemsFinder::destroyed, this,
        [this] { m_connections.release(); });
}

void QnSystemHostsModel::HostsModel::forceResort()
{
    const auto connections = appContext()->coreSettings()->recentLocalConnections();
    const auto it = connections.find(m_localSystemId);
    const auto recentUrls = (it == connections.end() ? UrlsList() : it->urls);

    if (recentUrls == m_recentConnectionUrls)
        return;
    m_recentConnectionUrls = recentUrls;

    for (auto& serverData: m_serversUrlData)
        serverData.priority = kLowestServerPriority;

    for (int recentUrlIndex = 0; recentUrlIndex < recentUrls.size(); ++recentUrlIndex)
    {
        const auto& recentUrl = recentUrls[recentUrlIndex];
        auto serverDataIter = std::find_if(m_serversUrlData.begin(), m_serversUrlData.end(),
            [recentUrl](const ServerUrlData& serverData)
            {
                return std::any_of(
                    serverData.additionalUrls.begin(), serverData.additionalUrls.end(),
                    [recentUrl](const nx::utils::Url& url)
                    {
                        return recentUrl.host() == url.host();
                    });
            });

        if (serverDataIter != m_serversUrlData.end()
            && serverDataIter->priority == kLowestServerPriority)
        {
            serverDataIter->priority = recentUrlIndex;
        }
    }

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

bool QnSystemHostsModel::HostsModel::getUrlVersionCompatibility(const nx::utils::Url& url) const
{
    for (const auto& serverHostData: m_serversUrlData)
    {
        if (serverHostData.containsUrl(url))
            return serverHostData.compatible;
    }

    return false;
}

int QnSystemHostsModel::HostsModel::getUrlPriority(const nx::utils::Url& url) const
{
    for (const auto& serverHostData: m_serversUrlData)
    {
        if (serverHostData.containsUrl(url))
            return serverHostData.priority;
    }

    return kLowestServerPriority;
}

bool QnSystemHostsModel::HostsModel::forcedLocalhostConversion() const
{
    return m_forcedLocalhostConversion;
}

void QnSystemHostsModel::HostsModel::setForcedLocalhostConversion(bool convert)
{
    m_forcedLocalhostConversion = convert;
}

int QnSystemHostsModel::HostsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_serversUrlData.size();
}

QVariant QnSystemHostsModel::HostsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto row = index.row();
    if (!qBetween<int>(0, row, m_serversUrlData.size()))
        return QVariant();

    const auto& data = m_serversUrlData.at(row);
    switch (role)
    {
        case Qt::DisplayRole:
        {
            auto host = data.mainUrl.host();
            if (isLocalhost(host, m_forcedLocalhostConversion))
                host = "localhost";
            const auto port = data.mainUrl.port(helpers::kDefaultConnectionPort);
            return port == helpers::kDefaultConnectionPort
                ? host
                : host + ':' + QString::number(port);
        }
        case UrlRole:
            return QVariant::fromValue(data.mainUrl);
        case UrlDisplayRole:
            return data.mainUrl.toDisplayString();
        case ServerIdRole:
            return data.id.toString();
        default:
            break;
    }
    return QVariant();
}

void QnSystemHostsModel::HostsModel::reloadHosts()
{
    if (!m_serversUrlData.isEmpty())
    {
        beginResetModel();
        m_serversUrlData.clear();
        endResetModel();
    }

    m_connections = {};
    if (m_systemId.isNull())
        return;

    const auto reloadHostsHandler =
        [this](const QnSystemDescriptionPtr& system)
        {
            if (m_systemId != system->id())
                return;

            for (const auto& server: system->servers())
                addServer(system, server.id);

            m_connections << connect(system.get(), &QnBaseSystemDescription::serverAdded, this,
                [this, system](const QnUuid& id) { addServer(system, id); });

            m_connections << connect(system.get(), &QnBaseSystemDescription::serverRemoved, this,
                [this, system](const QnUuid& id) { removeServer(system, id); });

            m_connections << connect(system.get(), &QnBaseSystemDescription::serverChanged, this,
                [this, system](const QnUuid& id, QnServerFields fields)
                {
                    if (fields.testFlag(QnServerField::Host))
                        updateServerHost(system, id);
                });
        };

    if (const auto system = qnSystemsFinder->getSystem(m_systemId))
    {
        reloadHostsHandler(system);
    }
    else
    {
        m_connections << connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
            this, reloadHostsHandler);
    }
}

void QnSystemHostsModel::HostsModel::addServer(
    const QnSystemDescriptionPtr& systemDescription,
    const QnUuid& serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    if (it != m_serversUrlData.end())
    {
        updateServerHostInternal(it, systemDescription);
        return;
    }

    const auto host = systemDescription->getServerHost(serverId);
    if (host.isEmpty())
        return;

    const bool isCompatible = nx::vms::common::ServerCompatibilityValidator::isCompatible(
        systemDescription->getServer(serverId));

    beginInsertRows(QModelIndex(), m_serversUrlData.size(), m_serversUrlData.size());
    m_serversUrlData.append(
        {serverId, host, formAdditionalUrlsSet(systemDescription, serverId), isCompatible});
    endInsertRows();
}

void QnSystemHostsModel::HostsModel::updateServerHost(
    const QnSystemDescriptionPtr& systemDescription,
    const QnUuid& serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    if (!updateServerHostInternal(it, systemDescription))
        addServer(systemDescription, serverId);
}

bool QnSystemHostsModel::HostsModel::updateServerHostInternal(
    const ServerUrlDataList::iterator& it,
    const QnSystemDescriptionPtr& systemDescription)
{
    if (it == m_serversUrlData.end())
        return false;

    const auto host = systemDescription->getServerHost(it->id);
    if (host.isEmpty())
    {
        removeServerInternal(it);
        return true;
    }

    it->mainUrl = host;
    it->additionalUrls = formAdditionalUrlsSet(systemDescription, it->id);

    const auto row = std::distance(m_serversUrlData.begin(), it);
    NX_ASSERT(row >= 0);

    const auto modelIndex = index(row);
    NX_ASSERT(modelIndex.isValid());

    emit dataChanged(modelIndex, modelIndex);

    return true;
}

void QnSystemHostsModel::HostsModel::removeServer(
    const QnSystemDescriptionPtr& systemDescription,
    const QnUuid& serverId)
{
    if (systemDescription->id() != m_systemId)
        return;

    const auto it = getDataIt(serverId);
    removeServerInternal(it);
}

void QnSystemHostsModel::HostsModel::removeServerInternal(const ServerUrlDataList::iterator& it)
{
    if (it == m_serversUrlData.end())
        return;

    const auto row = std::distance(m_serversUrlData.begin(), it);
    NX_ASSERT(row >= 0);

    beginRemoveRows(QModelIndex(), row, row);
    m_serversUrlData.erase(it);
    endRemoveRows();
}

QnSystemHostsModel::HostsModel::ServerUrlDataList::iterator
QnSystemHostsModel::HostsModel::getDataIt(const QnUuid& serverId)
{
    return std::find_if(m_serversUrlData.begin(), m_serversUrlData.end(),
        [serverId](const ServerUrlData& value)
        {
            return value.id == serverId;
        });
}

QHash<int, QByteArray> QnSystemHostsModel::HostsModel::roleNames() const
{
    static const auto kRoles = QHash<int, QByteArray>{
        {UrlRole, "url-internal"},
        {UrlDisplayRole, "url"},
        {ServerIdRole, "serverId"},
    };

    auto names = base_type::roleNames();
    names.insert(kRoles);
    return names;
}

//-------------------------------------------------------------------------------------------------
// QnSystemHostsModel

QnSystemHostsModel::QnSystemHostsModel(QObject* parent):
    base_type(parent)
{
    setSourceModel(new HostsModel(this));
    setTriggeringRoles({UrlRole});
}

bool QnSystemHostsModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const auto leftUrl = sourceLeft.data(UrlRole).value<nx::utils::Url>();
    NX_ASSERT(leftUrl.isValid(), nx::format("Invalid url %1").arg(leftUrl.toString()));

    const auto rightUrl = sourceRight.data(UrlRole).value<nx::utils::Url>();
    NX_ASSERT(rightUrl.isValid(), nx::format("Invalid url %1").arg(rightUrl.toString()));

    if (!leftUrl.isValid() || !rightUrl.isValid())
        return sourceLeft.row() < sourceRight.row();

    const bool leftIsCompatible = hostsModel()->getUrlVersionCompatibility(leftUrl);
    const bool rightIsCompatible = hostsModel()->getUrlVersionCompatibility(rightUrl);

    if (leftIsCompatible != rightIsCompatible)
        return leftIsCompatible;

    const int leftPriority = hostsModel()->getUrlPriority(leftUrl);
    const int rightPriority = hostsModel()->getUrlPriority(rightUrl);

    if (leftPriority == rightPriority)
        return sourceLeft.row() < sourceRight.row();

    return leftPriority < rightPriority;
}

bool QnSystemHostsModel::isLocalhost(const QString& host, bool forcedConversion)
{
    if (nx::network::HostAddress(host).isLoopback())
        return true;

    if (forcedConversion && qnLocalNetworkInterfacesManager->containsHost(host))
        return true;

    return false;
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

bool QnSystemHostsModel::forcedLocalhostConversion() const
{
    return hostsModel()->forcedLocalhostConversion();
}

void QnSystemHostsModel::setForcedLocalhostConversion(bool convert)
{
    hostsModel()->setForcedLocalhostConversion(convert);
}

QString QnSystemHostsModel::getRequiredSystemVersion(int hostIndex) const
{
    if (rowCount() > 0 && hostIndex < rowCount())
    {
        const QModelIndex index = QnSystemHostsModel::index(hostIndex, 0);
        const auto url = index.data(UrlRole).value<nx::utils::Url>();
        const bool isCompatible = hostsModel()->getUrlVersionCompatibility(url);
        if (isCompatible)
            return "";

        if (QnSystemDescriptionPtr system = qnSystemsFinder->getSystem(hostsModel()->systemId()))
        {
            auto serverInfo = system->getServer(index.data(ServerIdRole).toUuid());
            if (NX_ASSERT(!serverInfo.version.isNull()))
                return serverInfo.version.toString();

            if (NX_ASSERT(!system->version().isNull()))
                return system->version().toString();
        }
    }

    return "";
}

QnSystemHostsModel::HostsModel* QnSystemHostsModel::hostsModel() const
{
    return static_cast<HostsModel*>(sourceModel());
}
