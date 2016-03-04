
#include "systems_model.h"

#include <utils/math/math.h>
#include <utils/common/app_info.h>
#include <utils/common/software_version.h>
#include <network/systems_finder.h>

namespace
{
    typedef QHash<int, QByteArray> RoleNames;

    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1

        , NameRoleId = FirstRoleId
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
        result.insert(NameRoleId, "systemName");
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
}

QnSystemsModel::QnSystemsModel(QObject *parent)
    : base_type(parent)
    , m_systems()
{
    Q_ASSERT_X(qnSystemsFinder, Q_FUNC_INFO, "Systems finder is null!");
    
    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered
        , this, &QnSystemsModel::addSystem);
    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemLost
        , this, &QnSystemsModel::removeSystem);

    for (const auto system : qnSystemsFinder->systems())
        addSystem(system);
}

QnSystemsModel::~QnSystemsModel()
{}

int QnSystemsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_systems.count();
}

QVariant QnSystemsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !qBetween<int>(FirstRoleId, role, AfterLastRoleId))
        return QVariant();

    const auto row = index.row();
    if (!qBetween(0, row, rowCount()))
        return QVariant();

    const auto systemDescription = m_systems[row];
    switch(role)
    {
    case NameRoleId:
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
    static const auto predicate = [](const QnSystemDescriptionPtr &first
        , const QnSystemDescriptionPtr &second)
    {
        const bool isSameType =
            (first->isCloudSystem() == second->isCloudSystem());

        return (isSameType ? (first->name() < second->name())
            : first->isCloudSystem());
    };

    const auto insertPos = std::upper_bound(m_systems.begin()
        , m_systems.end(), systemDescription, predicate);

    const int position = (insertPos == m_systems.end()
        ? m_systems.size() : insertPos - m_systems.begin());

    beginInsertRows(QModelIndex(), position, position);
    m_systems.insert(insertPos, systemDescription);
    endInsertRows();
}

void QnSystemsModel::removeSystem(const QnUuid &systemId)
{
    const auto removeIt = std::find_if(m_systems.begin()
        , m_systems.end(), [systemId](const QnSystemDescriptionPtr &value)
    {
        return (value->id() == systemId);
    });

    if (removeIt == m_systems.end())
        return;

    const int pos = (removeIt - m_systems.begin());
    beginRemoveRows(QModelIndex(), pos, pos);
    m_systems.erase(removeIt);
    endRemoveRows();
}
