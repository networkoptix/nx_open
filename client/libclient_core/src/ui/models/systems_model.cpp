#include "systems_model.h"

#include <QtCore/QUrl>

#include <utils/math/math.h>
#include <utils/common/app_info.h>
#include <utils/common/software_version.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/disconnect_helper.h>
#include <network/system_description.h>
#include <network/connection_validator.h>
#include <finders/systems_finder.h>
#include <watchers/cloud_status_watcher.h>

namespace
{
    typedef QHash<int, QByteArray> RoleNames;
    const auto kRoleNames = []() -> RoleNames
    {
        RoleNames result;
        result.insert(QnSystemsModel::SystemNameRoleId, "systemName");
        result.insert(QnSystemsModel::SystemIdRoleId, "systemId");
        result.insert(QnSystemsModel::LocalIdRoleId, "localId");
        result.insert(QnSystemsModel::OwnerDescriptionRoleId, "ownerDescription");

        result.insert(QnSystemsModel::IsFactorySystemRoleId, "isFactorySystem");
        result.insert(QnSystemsModel::SafeModeRoleId, "safeMode");

        result.insert(QnSystemsModel::IsCloudSystemRoleId, "isCloudSystem");
        result.insert(QnSystemsModel::IsRunningRoleId, "isRunning");
        result.insert(QnSystemsModel::IsReachableRoleId, "isReachable");
        result.insert(QnSystemsModel::IsConnectableRoleId, "isConnectable");
        result.insert(QnSystemsModel::IsCompatibleRoleId, "isCompatible");
        result.insert(QnSystemsModel::IsCompatibleVersionRoleId, "isCompatibleVersion");
        result.insert(QnSystemsModel::IsCompatibleInternalRoleId, "isCompatibleInternal");

        result.insert(QnSystemsModel::WrongVersionRoleId, "wrongVersion");
        result.insert(QnSystemsModel::CompatibleVersionRoleId, "compatibleVersion");

        return result;
    }();
}

class QnSystemsModelPrivate: public Connective<QObject>
{
    QnSystemsModel* q_ptr;
    Q_DECLARE_PUBLIC(QnSystemsModel)

    using base_type = Connective<QObject>;

public:
    QnSystemsModelPrivate(QnSystemsModel* parent);

    void updateOwnerDescription();

    void addSystem(const QnSystemDescriptionPtr& systemDescription);

    void removeSystem(const QString& systemId);

    struct InternalSystemData
    {
        QnSystemDescriptionPtr system;
        QnDisconnectHelper connections;
    };
    using InternalSystemDataPtr = QSharedPointer<InternalSystemData>;
    using InternalList = QVector<InternalSystemDataPtr>;

    void at_serverChanged(
            const QnSystemDescriptionPtr& systemDescription,
            const QnUuid &serverId,
            QnServerFields fields);

    void emitDataChanged(
        const QnSystemDescriptionPtr& systemDescription,
        QnSystemsModel::RoleId role);

    void emitDataChanged(const QnSystemDescriptionPtr& systemDescription
        , QVector<int> roles = QVector<int>());

    void resetModel();

    QString getIncompatibleVersion(const QnSystemDescriptionPtr& systemDescription) const;
    QString getCompatibleVersion(const QnSystemDescriptionPtr& systemDescription) const;
    bool isCompatibleVersion(const QnSystemDescriptionPtr& systemDescription) const;
    bool isCompatibleSystem(const QnSystemDescriptionPtr& sysemDescription) const;
    bool isCompatibleInternal(const QnSystemDescriptionPtr& systemDescription) const;

    QnDisconnectHelper disconnectHelper;
    InternalList internalData;
    QnSoftwareVersion minimalVersion;
};

QnSystemsModel::QnSystemsModel(QObject *parent)
    : base_type(parent)
    , d_ptr(new QnSystemsModelPrivate(this))
{
    NX_ASSERT(qnSystemsFinder, Q_FUNC_INFO, "Systems finder is null!");

    Q_D(QnSystemsModel);

    d->disconnectHelper <<
        connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
                d, &QnSystemsModelPrivate::addSystem);

    d->disconnectHelper <<
        connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemLost,
                d, &QnSystemsModelPrivate::removeSystem);

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
            d, &QnSystemsModelPrivate::updateOwnerDescription);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::effectiveUserNameChanged,
        d, &QnSystemsModelPrivate::updateOwnerDescription);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::loginChanged,
            d, &QnSystemsModelPrivate::updateOwnerDescription);

    d->resetModel();
}

QnSystemsModel::~QnSystemsModel()
{}

int QnSystemsModel::getRowIndex(const QString& systemId) const
{
    Q_D(const QnSystemsModel);

    const auto it = std::find_if(d->internalData.begin(), d->internalData.end(),
        [systemId](const QnSystemsModelPrivate::InternalSystemDataPtr& data)
        {
            return systemId == data->system->id();
        });

    return (it == d->internalData.end() ? -1 : it - d->internalData.begin());
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

    switch(role)
    {
        case SearchRoleId:
        {
            QString hosts;
            for (const auto& moduleInfo : system->servers())
            {
                hosts.append(system->getServerHost(moduleInfo.id).host());
                hosts.append(lit(" "));
            }

            return QStringList({data(index, SystemNameRoleId).toString(),
                data(index, OwnerDescriptionRoleId).toString(),
                system->ownerAccountEmail(), hosts}
                ).join(lit(" "));
        }
        case SystemNameRoleId:
            return system->name();
        case SystemIdRoleId:
            return system->id();
        case LocalIdRoleId:
            return system->localId().toQUuid();
        case OwnerDescriptionRoleId:
        {
            if (!system->isCloudSystem())
                return QString(); // No owner for local system

            const bool isLoggedIn =
                (qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut);
            if (isLoggedIn && (qnCloudStatusWatcher->effectiveUserName() == system->ownerAccountEmail()))
            {
                return tr("Your system");
            }

            const auto fullName = system->ownerFullName();
            return (fullName.isEmpty() ? system->ownerAccountEmail()
                : tr("Owner: %1", "%1 is a user name").arg(fullName));
        }
        case IsFactorySystemRoleId:
            return system->isNewSystem();
        case SafeModeRoleId:
            return system->safeMode();
        case IsCloudSystemRoleId:
            return system->isCloudSystem();
        case IsRunningRoleId:
            return system->isRunning();
        case IsReachableRoleId:
            return system->isReachable();
        case IsConnectableRoleId:
            return system->isConnectable();
        case IsCompatibleRoleId:
            return d->isCompatibleSystem(system);
        case IsCompatibleInternalRoleId:
            return d->isCompatibleInternal(system);
        case IsCompatibleVersionRoleId:
            return d->isCompatibleVersion(system);
        case WrongVersionRoleId:
            return d->getIncompatibleVersion(system);
        case CompatibleVersionRoleId:
            return d->getCompatibleVersion(system);

        default:
            return QVariant();
    }
}

QString QnSystemsModel::minimalVersion() const
{
    Q_D(const QnSystemsModel);
    return d->minimalVersion.toString();
}

void QnSystemsModel::setMinimalVersion(const QString& minimalVersion)
{
    Q_D(QnSystemsModel);

    const auto version = QnSoftwareVersion(minimalVersion);

    if (d->minimalVersion == version)
        return;

    d->minimalVersion = version;

    d->resetModel();

    emit minimalVersionChanged();
}

RoleNames QnSystemsModel::roleNames() const
{
    return kRoleNames;
}

QnSystemsModelPrivate::QnSystemsModelPrivate(QnSystemsModel* parent)
    : base_type(parent)
    , q_ptr(parent)
    , disconnectHelper()
    , internalData()
    , minimalVersion(1, 0)
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

void QnSystemsModelPrivate::addSystem(const QnSystemDescriptionPtr& systemDescription)
{
    Q_Q(QnSystemsModel);

    const auto data = InternalSystemDataPtr(new InternalSystemData(
        { systemDescription, QnDisconnectHelper() }));

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::serverChanged, this,
            [this, systemDescription] (const QnUuid &serverId, QnServerFields fields)
            {
                at_serverChanged(systemDescription, serverId, fields);
            });

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::systemNameChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::SystemNameRoleId);
            });

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::isCloudSystemChanged, this,
            [this, systemDescription]()
            {
                /**
                 * Move system to right place. No data will not be preserved in case
                 * of cloud-to-system (and vice versa) state change
                 */
                removeSystem(systemDescription->id());
                addSystem(systemDescription);
            });

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::ownerChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QVector<int>()
                    << QnSystemsModel::OwnerDescriptionRoleId
                    << QnSystemsModel::SearchRoleId);
            });

    const auto serverAction =
        [this, systemDescription](const QnUuid& id)
        {
            Q_UNUSED(id);
            /* A lot of roles depend on server adding/removing. */
            emitDataChanged(systemDescription);
        };

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::serverAdded, this, serverAction);
    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::serverRemoved, this, serverAction);

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::newSystemStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::IsFactorySystemRoleId);
            });

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::safeModeStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::SafeModeRoleId);
            });

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::reachableStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::IsReachableRoleId);
            });

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::runningStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::IsRunningRoleId);
            });

    data->connections
        << connect(systemDescription, &QnBaseSystemDescription::connectableStateChanged, this,
            [this, systemDescription]()
            {
                emitDataChanged(systemDescription, QnSystemsModel::IsConnectableRoleId);
            });

    q->beginInsertRows(QModelIndex(), internalData.size(), internalData.size());
    internalData.append(data);
    q->endInsertRows();
}

void QnSystemsModelPrivate::removeSystem(const QString &systemId)
{
    Q_Q(QnSystemsModel);

    const auto removeIt = std::find_if(internalData.begin()
        , internalData.end(), [systemId](const InternalSystemDataPtr &value)
    {
        return (value->system->id() == systemId);
    });

    if (removeIt == internalData.end())
        return;

    const int position = (removeIt - internalData.begin());

    q->beginRemoveRows(QModelIndex(), position, position);
    internalData.erase(removeIt);
    q->endRemoveRows();
}

void QnSystemsModelPrivate::emitDataChanged(
    const QnSystemDescriptionPtr& systemDescription,
    QnSystemsModel::RoleId role)
{
    emitDataChanged(systemDescription, QVector<int>() << role);
}

void QnSystemsModelPrivate::emitDataChanged(const QnSystemDescriptionPtr& systemDescription
    , QVector<int> roles)
{
    const auto dataIt = std::find_if(internalData.begin(), internalData.end(),
        [id = systemDescription->id()](const InternalSystemDataPtr &value)
    {
        return (value->system->id() == id);
    });

    if (dataIt == internalData.end())
        return;

    Q_Q(QnSystemsModel);

    const int row = (dataIt - internalData.begin());
    const auto modelIndex = q->index(row);

    q->dataChanged(modelIndex, modelIndex, roles);
}

void QnSystemsModelPrivate::at_serverChanged(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId,
        QnServerFields fields)
{
    Q_UNUSED(serverId)

    Q_Q(QnSystemsModel);

    const auto dataIt = std::find_if(internalData.begin(), internalData.end(),
        [id = systemDescription->id()](const InternalSystemDataPtr& data)
        {
            return id == data->system->id();
        });

    if (dataIt == internalData.end())
        return;

    const int row = (dataIt - internalData.begin());

    const auto modelIndex = q->index(row);
    const auto testFlag = [this, q, modelIndex, fields](QnServerField field, int role)
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

    for (const auto system : qnSystemsFinder->systems())
        addSystem(system);

    q->endResetModel();
}

QString QnSystemsModelPrivate::getCompatibleVersion(
    const QnSystemDescriptionPtr& systemDescription) const
{
    for (const auto& serverInfo: systemDescription->servers())
    {
        const auto result = QnConnectionValidator::validateConnection(serverInfo);
        switch (result)
        {
            case Qn::IncompatibleProtocolConnectionResult:
                return serverInfo.version.toString(QnSoftwareVersion::BugfixFormat);
            case Qn::IncompatibleCloudHostConnectionResult:
                return serverInfo.version.toString();
            default:
                break;
        }
    }

    return QString();
}

QString QnSystemsModelPrivate::getIncompatibleVersion(
        const QnSystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();

    if (servers.isEmpty())
        return QString();

    const auto predicate =
        [this, systemDescription](const QnModuleInformation& serverInfo)
        {
            if (!systemDescription->isReachableServer(serverInfo.id))
                return false;
            auto connectionResult = QnConnectionValidator::validateConnection(serverInfo);
            return connectionResult == Qn::IncompatibleVersionConnectionResult;
        };

    const auto incompatibleIt =
        std::find_if(servers.begin(), servers.end(), predicate);
    return (incompatibleIt == servers.end() ? QString() :
        incompatibleIt->version.toString(QnSoftwareVersion::BugfixFormat));
}

bool QnSystemsModelPrivate::isCompatibleVersion(
        const QnSystemDescriptionPtr& systemDescription) const
{
    return getIncompatibleVersion(systemDescription).isEmpty();
}

bool QnSystemsModelPrivate::isCompatibleSystem(
    const QnSystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();
    return std::all_of(servers.cbegin(), servers.cend(),
        [systemDescription](const QnModuleInformation& serverInfo)
        {
            if (!systemDescription->isReachableServer(serverInfo.id))
                return true;

            auto connectionResult = QnConnectionValidator::validateConnection(serverInfo);
            return connectionResult == Qn::SuccessConnectionResult;
        });
}

bool QnSystemsModelPrivate::isCompatibleInternal(
    const QnSystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();
    return std::all_of(servers.cbegin(), servers.cend(),
        [systemDescription](const QnModuleInformation& serverInfo)
        {
            if (!systemDescription->isReachableServer(serverInfo.id))
                return true;

            auto connectionResult = QnConnectionValidator::validateConnection(serverInfo);
            return connectionResult != Qn::IncompatibleInternalConnectionResult;
        });
}
