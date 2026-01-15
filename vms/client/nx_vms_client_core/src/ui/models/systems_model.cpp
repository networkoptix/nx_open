// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systems_model.h"

#include <QtCore/QCollator>
#include <QtCore/QUrl>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/software_version.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

#include "abstract_systems_controller.h"
#include "systems_controller.h"
#include "system_hosts_model.h"

using namespace nx::vms::common;

using namespace nx::vms::client::core;

namespace {

bool isLocalHost(const QModelIndex& index)
{
    // TODO: #dfisenko Provide this information using special role.
    return index.data(QnSystemsModel::SearchRoleId).toString().contains("localhost");
}

} // namespace

const QHash<int, QByteArray> QnSystemsModel::kRoleNames{
    {QnSystemsModel::SystemNameRoleId, "systemName"},
    {QnSystemsModel::SystemIdRoleId, "systemId"},
    {QnSystemsModel::LocalIdRoleId, "localId"},
    {QnSystemsModel::OwnerDescriptionRoleId, "ownerDescription"},

    {QnSystemsModel::IsFactorySystemRoleId, "isFactorySystem"},

    {QnSystemsModel::IsCloudSystemRoleId, "isCloudSystem"},
    {QnSystemsModel::IsOnlineRoleId, "isOnline"},
    {QnSystemsModel::IsReachableRoleId, "isReachable"},
    {QnSystemsModel::IsCompatibleToMobileClient, "isCompatibleToMobileClient"},
    {QnSystemsModel::IsCompatibleToDesktopClient, "isCompatibleToDesktopClient"},

    {QnSystemsModel::WrongVersionRoleId, "wrongVersion"},
    {QnSystemsModel::WrongCustomizationRoleId, "wrongCustomization"},
    {QnSystemsModel::CompatibleVersionRoleId, "compatibleVersion"},
    {QnSystemsModel::VisibilityScopeRoleId, "visibilityScope"},
    {QnSystemsModel::IsCloudOauthSupportedRoleId, "isOauthSupported"},
    {QnSystemsModel::Is2FaEnabledForSystem, "is2FaEnabledForSystem"},
    {QnSystemsModel::IsSaasSuspended, "isSaasSuspended"},
    {QnSystemsModel::IsSaasShutDown, "isSaasShutDown"},
    {QnSystemsModel::IsSaasUninitialized, "isSaasUninitialized"},
    {QnSystemsModel::IsPending, "isPending"},
    {QnSystemsModel::OrganizationIdRoleId, "organizationId"},
};

class QnSystemsModelPrivate: public QObject
{
    QnSystemsModel* q_ptr;
    Q_DECLARE_PUBLIC(QnSystemsModel)

    using base_type = QObject;

public:
    QnSystemsModelPrivate(QnSystemsModel* parent);

    void updateOwnerDescription();

    void addSystem(const SystemDescriptionPtr& systemDescription);

    void removeSystem(const QString& systemId, const nx::Uuid& localId);

    struct InternalSystemData
    {
        SystemDescriptionPtr system;
        nx::utils::ScopedConnections connections;
    };
    using InternalSystemDataPtr = QSharedPointer<InternalSystemData>;
    using InternalList = QVector<InternalSystemDataPtr>;

    void at_serverChanged(
            const SystemDescriptionPtr& systemDescription,
            const nx::Uuid &serverId,
            QnServerFields fields);

    void emitDataChanged(
        const SystemDescriptionPtr& systemDescription,
        QnSystemsModel::RoleId role);

    void emitDataChanged(const SystemDescriptionPtr& systemDescription,
        QVector<int> roles = QVector<int>());

    void resetModel();

    nx::utils::SoftwareVersion getIncompatibleVersion(
        const SystemDescriptionPtr& systemDescription) const;
    nx::utils::SoftwareVersion getCompatibleVersion(
        const SystemDescriptionPtr& systemDescription) const;
    bool isCompatibleSystem(const SystemDescriptionPtr& sysemDescription) const;
    bool isCompatibleCustomization(const SystemDescriptionPtr& systemDescription) const;

    AbstractSystemsController* controller;
    nx::utils::ScopedConnections connections;
    InternalList internalData;
    QHash<QString, int> systemIdToRow;
    mutable QHash<QString, QString> searchServerNamesHostsCache;
};

QnSystemsModel::QnSystemsModel(AbstractSystemsController* controller, QObject* parent):
    base_type(parent),
    d_ptr(new QnSystemsModelPrivate(this))
{
    Q_D(QnSystemsModel);

    if (!controller)
        controller = new SystemsController(this);

    d->controller = controller;

    d->connections << connect(controller, &AbstractSystemsController::systemDiscovered,
        d, &QnSystemsModelPrivate::addSystem);

    d->connections << connect(controller, &AbstractSystemsController::systemLost,
        d, &QnSystemsModelPrivate::removeSystem);

    d->connections << connect(controller, &AbstractSystemsController::cloudStatusChanged,
        d, &QnSystemsModelPrivate::updateOwnerDescription);

    d->connections << connect(controller, &AbstractSystemsController::cloudLoginChanged,
        d, &QnSystemsModelPrivate::updateOwnerDescription);

    d->resetModel();
}

QnSystemsModel::~QnSystemsModel()
{
}

int QnSystemsModel::getRowIndex(const QString& systemId) const
{
    Q_D(const QnSystemsModel);

    return d->systemIdToRow.value(systemId, -1);
}

int QnSystemsModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const QnSystemsModel);

    if (parent.isValid())
        return 0;

    return d->internalData.count();
}

QVariant QnSystemsModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QnSystemsModel);

    if (!index.isValid() || !qBetween<int>(FirstRoleId, role, RolesCount))
        return QVariant();

    const auto row = index.row();
    if (!qBetween(0, row, rowCount()))
        return QVariant();

    const auto system = d->internalData[row]->system;
    const auto systemId = system->id();

    switch(role)
    {
        case SearchRoleId:
        {
            static constexpr char kDelimiter = ' ';

            if (!d->searchServerNamesHostsCache.contains(systemId))
            {
                QSet<QString> hosts;
                QStringList serverNames;
                bool isLocalhost = false;
                for (const auto& moduleInfo: system->servers())
                {
                    {
                        const auto host = system->getServerHost(moduleInfo.id).host();
                        if (!nx::network::SocketGlobals::addressResolver().isCloudHostname(host))
                        {
                            hosts.insert(host);
                            isLocalhost = isLocalhost || QnSystemHostsModel::isLocalhost(host, true);
                        }
                    }

                    for (const auto& address: system->getServerRemoteAddresses(moduleInfo.id))
                    {
                        const auto host = address.host();
                        if (nx::network::SocketGlobals::addressResolver().isCloudHostname(host))
                            continue;

                        hosts.insert(host);
                    }

                    serverNames.append(moduleInfo.name);
                }

                if (isLocalhost)
                {
                    hosts.insert("localhost");
                    hosts.insert("127.0.0.1");
                }

                d->searchServerNamesHostsCache[systemId] = QStringList({
                    hosts.values().join(kDelimiter),
                    serverNames.join(kDelimiter)}).join(kDelimiter);
            }

            const QString searchText = QStringList({
                data(index, SystemNameRoleId).toString(),
                data(index, OwnerDescriptionRoleId).toString(),
                system->ownerAccountEmail(),
                d->searchServerNamesHostsCache[systemId]}).join(kDelimiter);
            return searchText;
        }
        case SystemNameRoleId:
            return system->isNewSystem() ? tr("New Site") : system->name();
        case SystemIdRoleId:
            return systemId;
        case LocalIdRoleId:
            return system->localId().toQUuid();
        case OwnerDescriptionRoleId:
        {
            if (!system->isCloudSystem())
                return QString(); // No owner for local system

            const bool isLoggedIn =
                (d->controller->cloudStatus() != CloudStatusWatcher::LoggedOut);
            if (isLoggedIn && (d->controller->cloudLogin() == system->ownerAccountEmail()))
                return QString();

            const auto fullName = system->ownerFullName();
            return fullName.isEmpty()
                ? system->ownerAccountEmail()
                : fullName;
        }
        case IsFactorySystemRoleId:
            return system->isNewSystem();
        case IsCloudSystemRoleId:
            return system->isCloudSystem();
        case IsOnlineRoleId:
            return system->isOnline();
        case IsReachableRoleId:
            return system->isReachable();
        case IsCompatibleToMobileClient:
            return d->isCompatibleSystem(system);
        case IsCompatibleToDesktopClient:
            return d->isCompatibleCustomization(system);
        case WrongCustomizationRoleId:
            return !d->isCompatibleCustomization(system);
        case WrongVersionRoleId:
        {
            const auto version = d->getIncompatibleVersion(system);
            return !version.isNull() ? QVariant::fromValue(version) : QVariant();
        }
        case CompatibleVersionRoleId:
        {
            // In desktop client, prefer to use SystemHostsModel::systemVersion.
            const auto version = d->getCompatibleVersion(system);
            return !version.isNull() ? QVariant::fromValue(version) : QVariant();
        }
        case VisibilityScopeRoleId:
            return QVariant::fromValue(d->controller->visibilityScope(system->localId()));
        case IsCloudOauthSupportedRoleId:
            return system->isOauthSupported();
        case Is2FaEnabledForSystem:
            return system->is2FaEnabled();
        case IsSaasSuspended:
            return system->saasState() == nx::vms::api::SaasState::suspended;
        case IsSaasShutDown:
            return system->saasState() == nx::vms::api::SaasState::shutdown
                || system->saasState() == nx::vms::api::SaasState::autoShutdown;
        case IsSaasUninitialized:
            return system->saasState() == nx::vms::api::SaasState::uninitialized;
        case IsPending:
            return system->isPending();
        case OrganizationIdRoleId:
            return system->organizationId().toQUuid();
        default:
            return QVariant();
    }
}

bool QnSystemsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    Q_D(QnSystemsModel);

    if (!index.isValid() || !qBetween<int>(FirstRoleId, role, RolesCount))
        return base_type::setData(index, value, role);

    const auto row = index.row();
    if (!qBetween(0, row, rowCount()))
        return base_type::setData(index, value, role);

    const auto system = d->internalData[row]->system;
    switch (role)
    {
        case SystemNameRoleId:
        {
            d->controller->setScopeInfo(
                system->localId(),
                value.toString(),
                static_cast<welcome_screen::TileVisibilityScope>(
                    data(index, VisibilityScopeRoleId).toInt()));
            break;
        }
        case VisibilityScopeRoleId:
        {
            if (d->controller->setScopeInfo(system->localId(),
                    data(index, SystemNameRoleId).toString(),
                    static_cast<welcome_screen::TileVisibilityScope>(value.toInt())))
            {
                emit dataChanged(index, index, {VisibilityScopeRoleId});
            }
            return true;
        }
    }

    return base_type::setData(index, value, role);
}

bool QnSystemsModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight,
    bool cloudFirstSorting)
{
    using namespace nx::vms::client::core::welcome_screen;

    if (cloudFirstSorting)
    {
        const bool leftIsCloud = sourceLeft.data(QnSystemsModel::IsCloudSystemRoleId).toBool();
        const bool rightIsCloud = sourceRight.data(QnSystemsModel::IsCloudSystemRoleId).toBool();

        if (leftIsCloud != rightIsCloud)
            return leftIsCloud;
    }

    const auto leftIsFactorySystem =
        sourceLeft.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    const auto rightIsFactorySystem =
        sourceRight.data(QnSystemsModel::IsFactorySystemRoleId).toBool();

    const auto leftVisibilityScope =
        sourceLeft.data(QnSystemsModel::VisibilityScopeRoleId).value<TileVisibilityScope>();
    const auto rightVisibilityScope =
        sourceRight.data(QnSystemsModel::VisibilityScopeRoleId).value<TileVisibilityScope>();

    if (leftIsFactorySystem != rightIsFactorySystem)
    {
        if (leftVisibilityScope == rightVisibilityScope)
            return leftIsFactorySystem;

        return leftIsFactorySystem
            ? leftVisibilityScope != TileVisibilityScope::HiddenTileVisibilityScope
            : rightVisibilityScope == TileVisibilityScope::HiddenTileVisibilityScope;
    }

    if (leftVisibilityScope != rightVisibilityScope)
        return leftVisibilityScope > rightVisibilityScope;

    const bool leftIsOnline = sourceLeft.data(QnSystemsModel::IsOnlineRoleId).toBool();
    const bool rightIsOnline = sourceRight.data(QnSystemsModel::IsOnlineRoleId).toBool();

    const bool leftIsConnectable = leftIsOnline
        && sourceLeft.data(QnSystemsModel::IsCompatibleToDesktopClient).toBool();
    const bool rightIsConnectable = rightIsOnline
        && sourceRight.data(QnSystemsModel::IsCompatibleToDesktopClient).toBool();

    if (leftIsConnectable != rightIsConnectable)
        return leftIsConnectable;

    const bool leftIsConnectableMobile = leftIsOnline
        && sourceLeft.data(QnSystemsModel::IsCompatibleToMobileClient).toBool();
    const bool rightIsConnectableMobile = rightIsOnline
        && sourceRight.data(QnSystemsModel::IsCompatibleToMobileClient).toBool();

    if (leftIsConnectableMobile != rightIsConnectableMobile)
        return leftIsConnectableMobile;

    if (cloudFirstSorting)
    {
        if (leftIsOnline != rightIsOnline)
            return leftIsOnline;
    }

    const bool leftIsPending = sourceLeft.data(QnSystemsModel::IsPending).toBool();
    const bool rightIsPending = sourceRight.data(QnSystemsModel::IsPending).toBool();

    if (leftIsPending != rightIsPending)
        return leftIsPending;

    if (!cloudFirstSorting)
    {
        const bool leftIsCloud = sourceLeft.data(QnSystemsModel::IsCloudSystemRoleId).toBool();
        const bool rightIsCloud = sourceRight.data(QnSystemsModel::IsCloudSystemRoleId).toBool();

        if (leftIsCloud != rightIsCloud)
            return leftIsCloud;
    }
    const bool leftIsLocalhost = isLocalHost(sourceLeft);
    const bool rightIsLocalhost = isLocalHost(sourceRight);

    if (leftIsLocalhost != rightIsLocalhost)
        return leftIsLocalhost;

    if (cloudFirstSorting)
    {
        const bool leftIsCloud = sourceLeft.data(QnSystemsModel::IsCloudSystemRoleId).toBool();
        const bool rightIsCloud = sourceRight.data(QnSystemsModel::IsCloudSystemRoleId).toBool();

        if (leftIsCloud != rightIsCloud)
            return leftIsCloud;
    }

    const int namesOrder = nx::utils::naturalStringCompare(
        sourceLeft.data(QnSystemsModel::SystemNameRoleId).toString(),
        sourceRight.data(QnSystemsModel::SystemNameRoleId).toString(),
        Qt::CaseInsensitive);

    if (namesOrder != 0)
        return namesOrder < 0;

    return sourceLeft.data(QnSystemsModel::SystemIdRoleId).toString()
        < sourceRight.data(QnSystemsModel::SystemIdRoleId).toString();
}

QnSystemsModelPrivate::QnSystemsModelPrivate(QnSystemsModel* parent):
    base_type(parent),
    q_ptr(parent)
{
}

void QnSystemsModelPrivate::updateOwnerDescription()
{
    Q_Q(QnSystemsModel);

    const auto count = q->rowCount();
    if (!count)
        return;

    emit q->dataChanged(q->index(0), q->index(count - 1), QVector<int>()
        << QnSystemsModel::OwnerDescriptionRoleId
        << QnSystemsModel::SearchRoleId);
}

void QnSystemsModelPrivate::addSystem(const SystemDescriptionPtr& systemDescription)
{
    Q_Q(QnSystemsModel);

    const auto data = InternalSystemDataPtr(new InternalSystemData({systemDescription}));

    data->connections
        << connect(systemDescription.get(), &SystemDescription::serverChanged, this,
            [this, systemDescription] (const nx::Uuid &serverId, QnServerFields fields)
            {
                searchServerNamesHostsCache.remove(systemDescription->id());

                at_serverChanged(systemDescription, serverId, fields);
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::systemNameChanged, this,
            [this, systemDescription]()
            {
                searchServerNamesHostsCache.remove(systemDescription->id());

                emitDataChanged(systemDescription, QnSystemsModel::SystemNameRoleId);
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::ownerChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QVector<int>()
                    << QnSystemsModel::OwnerDescriptionRoleId
                    << QnSystemsModel::SearchRoleId);
            });

    const auto serverAction =
        [this, systemDescription](const nx::Uuid& id)
        {
            Q_UNUSED(id);

            searchServerNamesHostsCache.remove(systemDescription->id());

            /* A lot of roles depend on server adding/removing. */
            emitDataChanged(systemDescription);
        };

    data->connections
        << connect(systemDescription.get(), &SystemDescription::serverAdded, this, serverAction);

    data->connections
        << connect(systemDescription.get(), &SystemDescription::serverRemoved, this, serverAction);

    data->connections
        << connect(systemDescription.get(), &SystemDescription::serverChanged, this,
            [this, systemDescription]()
            {
                searchServerNamesHostsCache.remove(systemDescription->id());
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::newSystemStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::IsFactorySystemRoleId);
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::reachableStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::IsReachableRoleId);
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::onlineStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::IsOnlineRoleId);
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::system2faEnabledChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::Is2FaEnabledForSystem);
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::oauthSupportedChanged, this,
           [this, systemDescription]()
           {
               emitDataChanged(systemDescription, QnSystemsModel::IsCloudOauthSupportedRoleId);
           });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::isCloudSystemChanged, this,
            [this, systemDescription]()
            {
                QVector<int> roles;
                roles
                    << QnSystemsModel::IsCloudSystemRoleId
                    << QnSystemsModel::OwnerDescriptionRoleId;
                emitDataChanged(systemDescription, roles);
            });

    data->connections
        << connect(systemDescription.get(), &SystemDescription::saasStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(
                    systemDescription,
                    {
                        QnSystemsModel::IsSaasSuspended,
                        QnSystemsModel::IsSaasShutDown,
                        QnSystemsModel::IsSaasUninitialized
                    });
            });

    q->beginInsertRows(QModelIndex(), internalData.size(), internalData.size());
    systemIdToRow[systemDescription->id()] = internalData.size();
    internalData.append(data);
    q->endInsertRows();
}

void QnSystemsModelPrivate::removeSystem(const QString &systemId, const nx::Uuid& localId)
{
    Q_Q(QnSystemsModel);

    const auto removeIt = std::find_if(internalData.begin(), internalData.end(),
        [systemId, localId](const InternalSystemDataPtr &value)
        {
            const auto& system = value->system;
            return system->id() == systemId || system->localId() == localId;
        });

    if (removeIt == internalData.end())
        return;

    const int position = (removeIt - internalData.begin());

    q->beginRemoveRows(QModelIndex(), position, position);
    internalData.erase(removeIt);
    // Adjust row numbers in systemId -> row mapping.
    for (auto it = systemIdToRow.begin(); it != systemIdToRow.end();)
    {
        if (it.value() > position)
            it.value()--;
        if (it.key() == systemId)
            it = systemIdToRow.erase(it);
        else
            ++it;
    }
    q->endRemoveRows();

    searchServerNamesHostsCache.remove(systemId);
}

void QnSystemsModelPrivate::emitDataChanged(
    const SystemDescriptionPtr& systemDescription,
    QnSystemsModel::RoleId role)
{
    emitDataChanged(systemDescription, QVector<int>() << role);
}

void QnSystemsModelPrivate::emitDataChanged(const SystemDescriptionPtr& systemDescription
    , QVector<int> roles)
{
    Q_Q(QnSystemsModel);

    const int row = q->getRowIndex(systemDescription->id());
    if (row == -1)
        return;
    const auto modelIndex = q->index(row);

    q->dataChanged(modelIndex, modelIndex, roles);
}

void QnSystemsModelPrivate::at_serverChanged(
        const SystemDescriptionPtr& systemDescription,
        const nx::Uuid& serverId,
        QnServerFields fields)
{
    Q_UNUSED(serverId)

    Q_Q(QnSystemsModel);

    const int row = q->getRowIndex(systemDescription->id());
    if (row == -1)
        return;

    const auto modelIndex = q->index(row);
    const auto testFlag = [q, modelIndex, fields](QnServerField field, int role)
    {
        if (fields.testFlag(field))
        {
            emit q->dataChanged(modelIndex, modelIndex, QVector<int>() << role
                << QnSystemsModel::SearchRoleId);
        }
    };

    testFlag(QnServerField::Host, QnSystemsModel::IsReachableRoleId);
}

void QnSystemsModelPrivate::resetModel()
{
    Q_Q(QnSystemsModel);

    q->beginResetModel();
    internalData.clear();
    systemIdToRow.clear();
    q->endResetModel();

    for (const auto& system: controller->systemsList())
        addSystem(system);
}

nx::utils::SoftwareVersion QnSystemsModelPrivate::getCompatibleVersion(
    const SystemDescriptionPtr& systemDescription) const
{
    for (const auto& serverInfo: systemDescription->servers())
    {
        if (ServerCompatibilityValidator::isCompatible(serverInfo))
            return serverInfo.version;
    }

    return {};
}

nx::utils::SoftwareVersion QnSystemsModelPrivate::getIncompatibleVersion(
    const SystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();

    if (servers.isEmpty())
        return {};

    const auto incompatibleIt = std::find_if(servers.begin(), servers.end(),
        [systemDescription](const nx::vms::api::ModuleInformation& serverInfo)
        {
            return systemDescription->isReachableServer(serverInfo.id)
                && !ServerCompatibilityValidator::isCompatible(serverInfo);
        });

    return incompatibleIt == servers.end()
        ? nx::utils::SoftwareVersion()
        : incompatibleIt->version;
}

bool QnSystemsModelPrivate::isCompatibleSystem(
    const SystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();
    return std::ranges::all_of(
        servers,
        [systemDescription](const nx::vms::api::ModuleInformation& serverInfo)
        {
            if (!systemDescription->isReachableServer(serverInfo.id))
                return true;

            const auto incompatibilityReason = ServerCompatibilityValidator::check(
                serverInfo);

            return !incompatibilityReason
                || incompatibilityReason == ServerCompatibilityValidator::Reason::cloudHostDiffers;
        });
}

bool QnSystemsModelPrivate::isCompatibleCustomization(
    const SystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();
    return std::ranges::all_of(
        servers,
        [systemDescription](const nx::vms::api::ModuleInformation& serverInfo)
        {
            if (!systemDescription->isReachableServer(serverInfo.id))
                return true;

            return ServerCompatibilityValidator::isCompatibleCustomization(
                serverInfo.customization);
        });
}
