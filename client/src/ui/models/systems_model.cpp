
#include "systems_model.h"

#include <utils/math/math.h>
#include <utils/common/app_info.h>
#include <utils/common/generic_guard.h>
#include <utils/common/software_version.h>
#include <utils/common/connections_holder.h>
#include <network/systems_finder.h>

namespace
{
    typedef QHash<int, QByteArray> RoleNames;

    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1

        , SystemNameRoleId = FirstRoleId
        , UserRoleId

        , IsCloudSystemRoleId

        , IsCompatibleRoleId
        , IsCompatibleVersionRoleId
        , IsCorrectCustomizaionRoleId

        // For local systems 
        , HostRoleId
        
        
        , AfterLastRoleId
    };

    const auto kRoleNames = []() -> RoleNames
    {
        RoleNames result;
        result.insert(SystemNameRoleId, "systemName");
        result.insert(UserRoleId, "userName");

        result.insert(IsCloudSystemRoleId, "isCloudSystem");
        result.insert(IsCompatibleRoleId, "isCompatible");
        result.insert(IsCompatibleVersionRoleId, "isCompatibleVersion");
        result.insert(IsCorrectCustomizaionRoleId, "isCorrectCustomizaion");

        result.insert(HostRoleId, "host");

        return result;
    }();

    bool isCorrectCustomization(const QnSystemDescriptionPtr &sysemDescription)
    {
        const auto servers = sysemDescription->servers();
        if (servers.isEmpty())
            return true;

        const auto customization = QnAppInfo::customizationName();
        const auto predicate = [customization](const QnModuleInformation &serverInfo)
        {
            return (customization == serverInfo.customization);
        };

        const bool hasCorrectCustomization = 
            std::any_of(servers.begin(), servers.end(), predicate);
        return hasCorrectCustomization;
    }

    bool isCompatibleVersion(const QnSystemDescriptionPtr &sysemDescription)
    {
        const auto servers = sysemDescription->servers();
        if (servers.isEmpty())
            return true;

        const QnSoftwareVersion appVersion(QnAppInfo::applicationVersion());
        const auto predicate = [appVersion](const QnModuleInformation &serverInfo)
        {
            return isCompatible(appVersion, serverInfo.version);
        };

        const bool hasCorrectVersion = 
            std::any_of(servers.begin(), servers.end(), predicate);
        return hasCorrectVersion;
    }

    bool isCompatibleSystem(const QnSystemDescriptionPtr &sysemDescription)
    {
        return (isCompatibleVersion(sysemDescription)
            && isCorrectCustomization(sysemDescription));
    }

    QString getSystemHost(const QnSystemDescriptionPtr &sysemDescription)
    {        
        const auto servers = sysemDescription->servers();
        if (servers.isEmpty())
            return QString();

        const auto serverId = servers.first().id;
        const auto primaryAddress =
            sysemDescription->getServerPrimaryAddress(serverId);
        return primaryAddress.toString();
    }

    template<typename DataType, typename ResultType>
    ResultType getLessSystemPred()
    {
        const auto lessPredicate = [](const DataType &firstData
            , const DataType &secondData)
        {
            const auto first = firstData->system;
            const auto second = secondData->system;

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
    QnConnectionsHolder connections;
};

///

QnSystemsModel::QnSystemsModel(int maxCount
    , QObject *parent)
    : base_type(parent)
    , m_maxCount(maxCount)
    , m_connectionsHolder(new QnConnectionsHolder())
    , m_lessPred(getLessSystemPred<InternalSystemDataPtr, LessPred>())
    , m_internalData()
{
    Q_ASSERT_X(qnSystemsFinder, Q_FUNC_INFO, "Systems finder is null!");
    
    const auto discoveredConnection = 
        connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered
        , this, &QnSystemsModel::addSystem);

    const auto lostConnection = 
        connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemLost
        , this, &QnSystemsModel::removeSystem);

    m_connectionsHolder->add(discoveredConnection);
    m_connectionsHolder->add(lostConnection);

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
    if (!index.isValid() || !qBetween<int>(FirstRoleId, role, AfterLastRoleId))
        return QVariant();

    const auto row = index.row();
    if (!qBetween(0, row, rowCount()))
        return QVariant();

    const auto systemDescription = m_internalData[row]->system;
    switch(role)
    {
    case SystemNameRoleId:
        return systemDescription->name();
    case UserRoleId:
        return lit("Some user name <replace me>");
    case IsCloudSystemRoleId:
        return systemDescription->isCloudSystem();

    case IsCompatibleRoleId:
        return isCompatibleSystem(systemDescription);
    case IsCorrectCustomizaionRoleId:
        return isCorrectCustomization(systemDescription);
    case IsCompatibleVersionRoleId:
        return isCompatibleVersion(systemDescription);
    case HostRoleId:
        return getSystemHost(systemDescription);
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
        { systemDescription, QnConnectionsHolder() }));
    
    const auto insertPos = std::upper_bound(m_internalData.begin()
        , m_internalData.end(), data, m_lessPred);

    const int position = (insertPos == m_internalData.end()
        ? m_internalData.size() : insertPos - m_internalData.begin());

    const QPointer<QnSystemsModel> guard(this);
    const auto serverChangedHandler = [this, guard, systemDescription]
        (const QnUuid &serverId, QnServerFields fields)
    {
        if (!guard)
            return;

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
            QnGenericGuard::create(beginInsertRowsCallback, endInserRowsCallback)
            : QnGenericGuard::createEmpty());
        m_internalData.insert(insertPos, data);
    }

    if (isMaximumNumber)
    {
        beginRemoveRows(QModelIndex(), m_maxCount, m_maxCount);
        endRemoveRows();
    }
}

void QnSystemsModel::removeSystem(const QnUuid &systemId)
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
            ? QnGenericGuard::create(beginRemoveRowsHandler, endRemoveRowsHandler)
            : QnGenericGuard::createEmpty());
        m_internalData.erase(removeIt);
    }

    if (moreThanMaximum)
    {
        beginInsertRows(QModelIndex(), m_maxCount - 1, m_maxCount - 1);
        endInsertRows();
    }
}

QnSystemsModel::InternalList::iterator QnSystemsModel::getInternalDataIt(
    const QnSystemDescriptionPtr &systemDescription)
{
    const auto data = InternalSystemDataPtr(new InternalSystemData(
        { systemDescription, QnConnectionsHolder() }));
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

    testFlag(QnServerField::PrimaryAddressField, HostRoleId);
}


