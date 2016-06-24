
#include "systems_model.h"

#include <utils/math/math.h>
#include <utils/common/app_info.h>
#include <utils/common/software_version.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/disconnect_helper.h>
#include <network/system_description.h>
#include <client_core/client_core_settings.h>
#include <finders/systems_finder.h>
#include <watchers/cloud_status_watcher.h>

namespace
{
    enum { kMaxTilesCount = 8 };

    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1

        , SystemNameRoleId = FirstRoleId
        , SystemIdRoleId

        , OwnerDescriptionRoleId
        , LastPasswordRoleId

        , IsFactorySystemRoleId

        , IsCloudSystemRoleId
        , IsOnlineRoleId
        , IsCompatibleRoleId
        , IsCompatibleVersionRoleId
        , IsCorrectCustomizationRoleId

        , WrongVersionRoleId
        , CompatibleVersionRoleId
        , WrongCustomizationRoleId

        // For local systems
        , LastPasswordsModelRoleId


        , RolesCount
    };

    typedef QHash<int, QByteArray> RoleNames;
    const auto kRoleNames = []() -> RoleNames
    {
        RoleNames result;
        result.insert(SystemNameRoleId, "systemName");
        result.insert(SystemIdRoleId, "systemId");
        result.insert(OwnerDescriptionRoleId, "ownerDescription");
        result.insert(LastPasswordRoleId, "lastPassword");

        result.insert(IsFactorySystemRoleId, "isFactorySystem");

        result.insert(IsCloudSystemRoleId, "isCloudSystem");
        result.insert(IsOnlineRoleId, "isOnline");
        result.insert(IsCompatibleRoleId, "isCompatible");
        result.insert(IsCompatibleVersionRoleId, "isCompatibleVersion");
        result.insert(IsCorrectCustomizationRoleId, "isCorrectCustomization");

        result.insert(WrongVersionRoleId, "wrongVersion");
        result.insert(CompatibleVersionRoleId, "compatibleVersion");
        result.insert(WrongCustomizationRoleId, "wrongCustomization");

        result.insert(LastPasswordsModelRoleId, "lastPasswordsModel");

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

    InternalList::iterator getInternalDataIt(
            const QnSystemDescriptionPtr& systemDescription);

    void at_serverChanged(
            const QnSystemDescriptionPtr& systemDescription,
            const QnUuid &serverId,
            QnServerFields fields);

    QStringListModel* createStringListModel(const QStringList& data) const;

    void resetModel();

    bool systemLess(const InternalSystemDataPtr& firstData,
                    const InternalSystemDataPtr& secondData) const;

    auto getSystemLessPred() const
    {
        return [this](const InternalSystemDataPtr& firstData,
                      const InternalSystemDataPtr& secondData)
        {
            return systemLess(firstData, secondData);
        };
    }

    QString getIncompatibleVersion(const QnSystemDescriptionPtr& systemDescription) const;
    QString getCompatibleVersion(const QnSystemDescriptionPtr& systemDescription) const;
    bool isCompatibleVersion(const QnSystemDescriptionPtr& systemDescription) const;
    bool isCompatibleSystem(const QnSystemDescriptionPtr& sysemDescription) const;
    QString getIncompatibleCustomization(const QnSystemDescriptionPtr& systemDescription) const;
    bool isCorrectCustomization(const QnSystemDescriptionPtr& systemDescription) const;
    bool isFactorySystem(const QnSystemDescriptionPtr& systemDescription) const;

    const int maxCount;
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
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::loginChanged,
            d, &QnSystemsModelPrivate::updateOwnerDescription);

    d->resetModel();
}

QnSystemsModel::~QnSystemsModel()
{}

int QnSystemsModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const QnSystemsModel);

    if (parent.isValid())
        return 0;

    return std::min(d->internalData.count(), d->maxCount);
}

QVariant QnSystemsModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QnSystemsModel);

    if (!index.isValid() || !qBetween<int>(FirstRoleId, role, RolesCount))
        return QVariant();

    const auto row = index.row();
    if (!qBetween(0, row, rowCount()))
        return QVariant();

    const auto systemDescription = d->internalData[row]->system;
    switch(role)
    {
    case SystemNameRoleId:
        return systemDescription->name();
    case SystemIdRoleId:
        return systemDescription->id();
    case OwnerDescriptionRoleId:
    {
        if (!systemDescription->isCloudSystem())
        {
            NX_ASSERT(false, "Non-cloud system can't have cloud owner!");
            return lit("WRONG USER NAME! BUG");
        }

        if ((qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Online)
            && (qnCloudStatusWatcher->cloudLogin() == systemDescription->ownerAccountEmail()))
        {
            return tr("Your system");
        }

        const auto fullName = systemDescription->ownerFullName();
        return (fullName.isEmpty() ? systemDescription->ownerAccountEmail()
            : tr("%1's system", "%1 is a user name").arg(fullName));
    }
    case LastPasswordsModelRoleId:
        return QVariant();  // TODO
    case IsFactorySystemRoleId:
        return d->isFactorySystem(systemDescription);
    case IsCloudSystemRoleId:
        return systemDescription->isCloudSystem();
    case IsOnlineRoleId:
        return !systemDescription->servers().isEmpty();
    case IsCompatibleRoleId:
        return d->isCompatibleSystem(systemDescription);
    case IsCorrectCustomizationRoleId:
        return d->isCorrectCustomization(systemDescription);
    case IsCompatibleVersionRoleId:
        return d->isCompatibleVersion(systemDescription);
    case WrongVersionRoleId:
        return d->getIncompatibleVersion(systemDescription);
    case CompatibleVersionRoleId:
        return d->getCompatibleVersion(systemDescription);
    case WrongCustomizationRoleId:
        return d->getIncompatibleCustomization(systemDescription);

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
    , maxCount(kMaxTilesCount)    // TODO: do we need to change it dynamically or from outside? Think about it.
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

    emit q->dataChanged(q->index(0), q->index(count - 1), QVector<int>{ OwnerDescriptionRoleId });
}

void QnSystemsModelPrivate::addSystem(const QnSystemDescriptionPtr& systemDescription)
{
    Q_Q(QnSystemsModel);

    const auto data = InternalSystemDataPtr(new InternalSystemData(
        { systemDescription, QnDisconnectHelper() }));

    const auto insertPos = std::upper_bound(internalData.begin()
        , internalData.end(), data, getSystemLessPred());

    const int position = (insertPos == internalData.end()
        ? internalData.size() : insertPos - internalData.begin());

    const auto serverChangedHandler = [this, systemDescription]
        (const QnUuid &serverId, QnServerFields fields)
    {
        at_serverChanged(systemDescription, serverId, fields);
    };

    data->connections <<
        connect(systemDescription, &QnSystemDescription::serverChanged,
                this, serverChangedHandler);

    const bool isMaximumNumber = (internalData.size() >= maxCount);
    const bool emitInsertSignal = (position < maxCount);

    {
        const auto beginInsertRowsCallback = [this, position]()
        {
            Q_Q(QnSystemsModel);
            q->beginInsertRows(QModelIndex(), position, position);
        };
        const auto endInserRowsCallback = [this]()
        {
            Q_Q(QnSystemsModel);
            q->endInsertRows();
        };

        const auto insertionGuard = (emitInsertSignal ?
            QnRaiiGuard::create(beginInsertRowsCallback, endInserRowsCallback)
            : QnRaiiGuard::createEmpty());

        internalData.insert(insertPos, data);
    }

    if (emitInsertSignal && isMaximumNumber)
    {
        q->beginRemoveRows(QModelIndex(), maxCount, maxCount);
        q->endRemoveRows();
    }
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
    const bool moreThanMaximum = (internalData.size() > maxCount);
    const bool emitRemoveSignal = (position <= maxCount);

    {
        internalData.erase(removeIt);

        const auto beginRemoveRowsHandler = [q, position]()
        {
            q->beginRemoveRows(QModelIndex(), position, position);
        };
        const auto endRemoveRowsHandler = [q]()
        {
            q->endRemoveRows();
        };

        const auto removeGuard = (emitRemoveSignal
            ? QnRaiiGuard::create(beginRemoveRowsHandler, endRemoveRowsHandler)
            : QnRaiiGuard::createEmpty());
    }

    if (emitRemoveSignal && moreThanMaximum)
    {
        q->beginInsertRows(QModelIndex(), maxCount - 1, maxCount - 1);
        q->endInsertRows();
    }
}

QnSystemsModelPrivate::InternalList::iterator QnSystemsModelPrivate::getInternalDataIt(
    const QnSystemDescriptionPtr& systemDescription)
{
    const auto data = InternalSystemDataPtr(new InternalSystemData(
        { systemDescription, QnDisconnectHelper() }));
    const auto it = std::lower_bound(internalData.begin()
        , internalData.end(), data, getSystemLessPred());

    if (it == internalData.end())
        return internalData.end();

    const auto foundId = (*it)->system->id();
    return (foundId == systemDescription->id() ? it : internalData.end());
}

void QnSystemsModelPrivate::at_serverChanged(
        const QnSystemDescriptionPtr& systemDescription,
        const QnUuid& serverId,
        QnServerFields fields)
{
    Q_UNUSED(serverId)

    Q_Q(QnSystemsModel);

    if (fields.testFlag(QnServerField::FlagsField))
    {
        // If NEW_SYSTEM state flag is changed we have to resort systems.
        // TODO: #ynikitenkov check is exactly NEW_SYSTEM flag is changed
        removeSystem(systemDescription->id());
        addSystem(systemDescription);
    }
    
    const auto dataIt = getInternalDataIt(systemDescription);
    if (dataIt == internalData.end())
        return;

    const int row = (dataIt - internalData.begin());

    const auto modelIndex = q->index(row);
    const auto testFlag = [this, q, modelIndex, fields](QnServerField field, int role)
    {
        if (fields.testFlag(field))
            emit q->dataChanged(modelIndex, modelIndex, QVector<int>(1, role));
    };

    testFlag(QnServerField::HostField, IsOnlineRoleId);
}

void QnSystemsModelPrivate::resetModel()
{
    internalData.clear();

    for (const auto system : qnSystemsFinder->systems())
        addSystem(system);
}

bool QnSystemsModelPrivate::systemLess(
        const QnSystemsModelPrivate::InternalSystemDataPtr& firstData,
        const QnSystemsModelPrivate::InternalSystemDataPtr& secondData) const
{
    const auto first = firstData->system;
    const auto second = secondData->system;

    const bool firstIsFactorySystem = isFactorySystem(first);
    const bool sameFactoryStatus =
        (firstIsFactorySystem == isFactorySystem(second));
    if (!sameFactoryStatus)
        return firstIsFactorySystem;

    const bool firstIsCloudSystem = first->isCloudSystem();
    const bool sameType =
        (firstIsCloudSystem == second->isCloudSystem());
    if (!sameType)
        return firstIsCloudSystem;

    const bool firstCompatible = isCompatibleSystem(first);
    const bool sameCompatible =
        (firstCompatible == isCompatibleSystem(second));
    if (!sameCompatible)
        return firstCompatible;

    const bool firstFullCompatible = getCompatibleVersion(first).isEmpty();
    const bool sameFullCompatible =
        (firstFullCompatible == getCompatibleVersion(second).isEmpty());
    if (!sameFullCompatible)
        return firstFullCompatible;

    return (first->name() < second->name());
}

QString QnSystemsModelPrivate::getCompatibleVersion(
    const QnSystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();
    if (servers.isEmpty())
        return QString();

    const auto predicate = [this](const QnModuleInformation& serverInfo)
    {
        return !serverInfo.hasCompatibleVersion();
    };

    const auto compatibleIt = std::find_if(servers.begin(), servers.end(), predicate);
    return (compatibleIt == servers.end() ? QString() :
        compatibleIt->version.toString(QnSoftwareVersion::BugfixFormat));
}

QString QnSystemsModelPrivate::getIncompatibleVersion(
        const QnSystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();
    if (servers.isEmpty())
        return QString();

    const auto predicate = [this](const QnModuleInformation& serverInfo)
    {
        return serverInfo.version < minimalVersion;
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
        const QnSystemDescriptionPtr& sysemDescription) const
{
    return isCompatibleVersion(sysemDescription)
        && isCorrectCustomization(sysemDescription)
           // TODO: add more checks
        ;
}

QString QnSystemsModelPrivate::getIncompatibleCustomization(
        const QnSystemDescriptionPtr& systemDescription) const
{
    const auto servers = systemDescription->servers();
    if (servers.isEmpty())
        return QString();

    const auto customization = QnAppInfo::customizationName();
    const auto predicate = [customization](const QnModuleInformation& serverInfo)
    {
        // TODO: improve me https://networkoptix.atlassian.net/browse/VMS-2163
        return (customization != serverInfo.customization);
    };

    const auto incompatibleIt =
        std::find_if(servers.begin(), servers.end(), predicate);
    return (incompatibleIt == servers.end() ? QString()
        : incompatibleIt->customization);
}

bool QnSystemsModelPrivate::isCorrectCustomization(
        const QnSystemDescriptionPtr& systemDescription) const
{
    return getIncompatibleCustomization(systemDescription).isEmpty();
}

bool QnSystemsModelPrivate::isFactorySystem(
        const QnSystemDescriptionPtr& systemDescription) const
{
    if (systemDescription->isCloudSystem())
        return false;

    const auto servers = systemDescription->servers();
    if (servers.isEmpty())
        return false;

    const auto predicate = [](const QnModuleInformation &serverInfo)
    {
        return serverInfo.serverFlags.testFlag(Qn::SF_NewSystem);
    };

    const bool isFactory =
        std::any_of(servers.begin(), servers.end(), predicate);
    return isFactory;
}
