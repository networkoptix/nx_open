
#include "systems_model.h"

#include <utils/math/math.h>
#include <utils/common/app_info.h>
#include <utils/common/software_version.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/disconnect_helper.h>
#include <finders/systems_finder.h>
#include <core/core_settings.h>

namespace
{
    enum { kMaxTilesCount = 8 };

    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1

        , SystemNameRoleId = FirstRoleId
        , SystemIdRoleId

        , OwnerEmailRoleId
        , LastPasswordRoleId

        , IsFactorySystemRoleId

        , IsCloudSystemRoleId
        , IsOnlineRoleId
        , IsCompatibleRoleId
        , IsCompatibleVersionRoleId
        , IsCorrectCustomizationRoleId

        , WrongVersionRoleId
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
        result.insert(OwnerEmailRoleId, "ownerEmail");
        result.insert(LastPasswordRoleId, "lastPassword");

        result.insert(IsFactorySystemRoleId, "isFactorySystem");

        result.insert(IsCloudSystemRoleId, "isCloudSystem");
        result.insert(IsOnlineRoleId, "isOnline");
        result.insert(IsCompatibleRoleId, "isCompatible");
        result.insert(IsCompatibleVersionRoleId, "isCompatibleVersion");
        result.insert(IsCorrectCustomizationRoleId, "isCorrectCustomization");

        result.insert(WrongVersionRoleId, "wrongVersion");
        result.insert(WrongCustomizationRoleId, "wrongCustomization");

        result.insert(LastPasswordsModelRoleId, "lastPasswordsModel");

        return result;
    }();

    QString getIncompatibleCustomization(const QnSystemDescriptionPtr &systemDescription)
    {
        const auto servers = systemDescription->servers();
        if (servers.isEmpty())
            return QString();

        const auto customization = QnAppInfo::customizationName();
        const auto predicate = [customization](const QnModuleInformation &serverInfo)
        {
            // TODO: improve me https://networkoptix.atlassian.net/browse/VMS-2163
            return (customization != serverInfo.customization);
        };

        const auto incompatibleIt =
            std::find_if(servers.begin(), servers.end(), predicate);
        return (incompatibleIt == servers.end() ? QString()
            : incompatibleIt->customization);
    }

    bool isCorrectCustomization(const QnSystemDescriptionPtr &systemDescription)
    {
        return getIncompatibleCustomization(systemDescription).isEmpty();
    }

    QString getIncompatibleVersion(const QnSystemDescriptionPtr &systemDescription)
    {
        const auto servers = systemDescription->servers();
        if (servers.isEmpty())
            return QString();

        const QnSoftwareVersion appVersion(QnAppInfo::applicationVersion());
        const auto predicate = [appVersion](const QnModuleInformation &serverInfo)
        {
            // TODO: improve me https://networkoptix.atlassian.net/browse/VMS-2166
            return !serverInfo.hasCompatibleVersion();
        };

        const auto incompatibleIt =
            std::find_if(servers.begin(), servers.end(), predicate);
        return (incompatibleIt == servers.end() ? QString()
            : incompatibleIt->version.toString(QnSoftwareVersion::BugfixFormat));
    }

    bool isCompatibleVersion(const QnSystemDescriptionPtr &systemDescription)
    {
        return getIncompatibleVersion(systemDescription).isEmpty();
    }

    bool isCompatibleSystem(const QnSystemDescriptionPtr &sysemDescription)
    {
        return (isCompatibleVersion(sysemDescription)
            && isCorrectCustomization(sysemDescription)
            // TODO: add more checks
            );
    }

    bool isFactorySystem(const QnSystemDescriptionPtr &systemDescription)
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

    template<typename DataType, typename ResultType>
    ResultType getLessSystemPred()
    {
        const auto lessPredicate = [](const DataType &firstData
            , const DataType &secondData)
        {
            const auto first = firstData->system;
            const auto second = secondData->system;

            const bool firstIsFactorySystem = isFactorySystem(first);
            const bool sameFactoryStatus = (firstIsFactorySystem
                == isFactorySystem(second));
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

            return (first->name() < second->name());
        };
        return lessPredicate;
    }
}

///
struct QnSystemsModel::InternalSystemData
{
    QnSystemDescriptionPtr system;
    QnDisconnectHelper connections;
};

///

QnSystemsModel::QnSystemsModel(QObject *parent)
    : base_type(parent)
    , m_maxCount(kMaxTilesCount)    // TODO: do we need to change it dynamically or from outside? Think about it.
    , m_lessPred(getLessSystemPred<InternalSystemDataPtr, LessPred>())
    , m_connections()
    , m_internalData()
{
    NX_ASSERT(qnSystemsFinder, Q_FUNC_INFO, "Systems finder is null!");

    const auto discoveredConnection =
        connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered
        , this, &QnSystemsModel::addSystem);

    const auto lostConnection =
        connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemLost
        , this, &QnSystemsModel::removeSystem);

    m_connections << discoveredConnection << lostConnection;

    for (const auto system : qnSystemsFinder->systems())
        addSystem(system);
}

QnSystemsModel::~QnSystemsModel()
{}

int QnSystemsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return std::min(m_internalData.count(), m_maxCount);
}

QVariant QnSystemsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !qBetween<int>(FirstRoleId, role, RolesCount))
        return QVariant();

    const auto row = index.row();
    if (!qBetween(0, row, rowCount()))
        return QVariant();

    const auto systemDescription = m_internalData[row]->system;
    switch(role)
    {
    case SystemNameRoleId:
        return systemDescription->name();
    case SystemIdRoleId:
        return systemDescription->id();
    case OwnerEmailRoleId:
        return (systemDescription->isCloudSystem() 
            ?  systemDescription->ownerAccountEmail()
            : lit("WRONG USER NAME! BUG"));
    case LastPasswordsModelRoleId:
        return QVariant();  // TODO
    case IsFactorySystemRoleId:
        return isFactorySystem(systemDescription);
    case IsCloudSystemRoleId:
        return systemDescription->isCloudSystem();
    case IsOnlineRoleId:
        return !systemDescription->servers().isEmpty();
    case IsCompatibleRoleId:
        return isCompatibleSystem(systemDescription);
    case IsCorrectCustomizationRoleId:
        return isCorrectCustomization(systemDescription);
    case IsCompatibleVersionRoleId:
        return isCompatibleVersion(systemDescription);
    case WrongVersionRoleId:
        return getIncompatibleVersion(systemDescription);
    case WrongCustomizationRoleId:
        return getIncompatibleCustomization(systemDescription);

    default:
        return QVariant();
    }
}

RoleNames QnSystemsModel::roleNames() const
{
    return kRoleNames;
}

void QnSystemsModel::addSystem(const QnSystemDescriptionPtr &systemDescription)
{
    const auto data = InternalSystemDataPtr(new InternalSystemData(
        { systemDescription, QnDisconnectHelper() }));

    const auto insertPos = std::upper_bound(m_internalData.begin()
        , m_internalData.end(), data, m_lessPred);

    const int position = (insertPos == m_internalData.end()
        ? m_internalData.size() : insertPos - m_internalData.begin());

    const auto serverChangedHandler = [this, systemDescription]
        (const QnUuid &serverId, QnServerFields fields)
    {
        serverChanged(systemDescription, serverId, fields);
    };

    const auto serverChangedConnection =
        connect(systemDescription, &QnSystemDescription::serverChanged
            , this, serverChangedHandler);

    data->connections.add(serverChangedConnection);

    const bool isMaximumNumber = (m_internalData.size() >= m_maxCount);
    const bool emitInsertSignal = (position < m_maxCount);

    {
        const auto beginInsertRowsCallback = [this, position]()
        {
            beginInsertRows(QModelIndex(), position, position);
        };
        const auto endInserRowsCallback = [this]()
            { endInsertRows(); };

        const auto insertionGuard = (emitInsertSignal ?
            QnRaiiGuard::create(beginInsertRowsCallback, endInserRowsCallback)
            : QnRaiiGuard::createEmpty());

        m_internalData.insert(insertPos, data);
    }

    if (emitInsertSignal && isMaximumNumber)
    {
        beginRemoveRows(QModelIndex(), m_maxCount, m_maxCount);
        endRemoveRows();
    }
}

void QnSystemsModel::removeSystem(const QString &systemId)
{
    const auto removeIt = std::find_if(m_internalData.begin()
        , m_internalData.end(), [systemId](const InternalSystemDataPtr &value)
    {
        return (value->system->id() == systemId);
    });

    if (removeIt == m_internalData.end())
        return;

    const int position = (removeIt - m_internalData.begin());
    const bool moreThanMaximum = (m_internalData.size() > m_maxCount);
    const bool emitRemoveSignal = (position <= m_maxCount);

    {
        const auto beginRemoveRowsHandler = [this, position]()
            { beginRemoveRows(QModelIndex(), position, position); };
        const auto endRemoveRowsHandler = [this]()
            { endRemoveRows(); };

        const auto removeGuard = (emitRemoveSignal
            ? QnRaiiGuard::create(beginRemoveRowsHandler, endRemoveRowsHandler)
            : QnRaiiGuard::createEmpty());
        m_internalData.erase(removeIt);
    }

    if (emitRemoveSignal && moreThanMaximum)
    {
        beginInsertRows(QModelIndex(), m_maxCount - 1, m_maxCount - 1);
        endInsertRows();
    }
}

QnSystemsModel::InternalList::iterator QnSystemsModel::getInternalDataIt(
    const QnSystemDescriptionPtr &systemDescription)
{
    const auto data = InternalSystemDataPtr(new InternalSystemData(
        { systemDescription, QnDisconnectHelper() }));
    const auto it = std::lower_bound(m_internalData.begin()
        , m_internalData.end(), data, m_lessPred);

    if (it == m_internalData.end())
        return m_internalData.end();

    const auto foundId = (*it)->system->id();
    return (foundId == systemDescription->id() ? it : m_internalData.end());
}

void QnSystemsModel::serverChanged(const QnSystemDescriptionPtr &systemDescription
    , const QnUuid &serverId
    , QnServerFields fields)
{
    if (fields.testFlag(QnServerField::FlagsField))
    {
        // If NEW_SYSTEM state flag is changed we have to resort systems.
        // TODO: #ynikitenkov check is exactly NEW_SYSTEM flag is changed
        removeSystem(systemDescription->id());
        addSystem(systemDescription);
    }
    
    const auto dataIt = getInternalDataIt(systemDescription);
    if (dataIt == m_internalData.end())
        return;

    const int row = (dataIt - m_internalData.begin());

    const auto modelIndex = index(row);
    const auto testFlag = [this, modelIndex, fields](QnServerField field, int role)
    {
        if (fields.testFlag(field))
            emit dataChanged(modelIndex, modelIndex, QVector<int>(1, role));
    };

    testFlag(QnServerField::HostField, IsOnlineRoleId);
}
