
#include "servers_selection_model.h"

#include <cassert>
#include <functional>

#include <QUuid>

#include <base/constants.h>
#include <helpers/server_info_manager.h>
#include <helpers/model_change_helper.h>

namespace api = nx::mediaserver::api;

namespace
{
    enum ServerLoggedState
    {
        kServerLoggedInState
        , kServerUnauthorized
        , kDifferentNetwork
    };

    struct ServerModelInfo
    {
        qint64 updateTimestamp;
        int removeRequestsCounter;
        bool locked;
        QString lockReason;
        Qt::CheckState selectedState;
        ServerLoggedState loginState;
        api::ServerInfo serverInfo;

        ServerModelInfo();

        ServerModelInfo(qint64 initUpdateTimestamp
            , Qt::CheckState initSelectedState
            , ServerLoggedState loginState
            , bool initLocked
            , const QString &initLockReason
            , int initRemoveRequestsCounter
            , const api::ServerInfo initServerInfo);
    };

    ServerModelInfo::ServerModelInfo()
        : updateTimestamp(0)
        , removeRequestsCounter(0)
        , locked(false)
        , selectedState(Qt::Unchecked)
        , loginState(kServerUnauthorized)
        , serverInfo()
    {}

    ServerModelInfo::ServerModelInfo(qint64 initUpdateTimestamp
        , Qt::CheckState initSelectedState
        , ServerLoggedState initLoginState
        , bool initLocked
        , const QString &initLockReason
        , int initRemoveRequestsCounter
        , const api::ServerInfo initServerInfo)
        : updateTimestamp(initUpdateTimestamp)
        , removeRequestsCounter(initRemoveRequestsCounter)
        , locked(initLocked)
        , lockReason(initLockReason)
        , selectedState(initSelectedState)
        , loginState(initLoginState)
        , serverInfo(initServerInfo)
    {}

    typedef QVector<ServerModelInfo> ServerModelInfosVector;

    struct SystemModelInfo
    {
        QString name;

        int selectedServers;
        int loggedServers;
        int unauthorizedServers;
        ServerModelInfosVector servers;

        SystemModelInfo();

        explicit SystemModelInfo(const QString &initName);
    };

    SystemModelInfo::SystemModelInfo()
        : name()
        , selectedServers(0)
        , loggedServers(0)
        , unauthorizedServers(0)
        , servers()
    {}

    SystemModelInfo::SystemModelInfo(const QString &initName)
        : name(initName)
        , selectedServers(0)
        , loggedServers(0)
        , unauthorizedServers(0)
        , servers()
    {}


    typedef QVector<SystemModelInfo> SystemModelInfosVector;

    template<typename SystemIterator
        , typename ServerIterator>
    struct ItemSearchInfoBase
    {
        typedef SystemIterator SystemItType;

        SystemIterator systemInfoIterator;
        ServerIterator serverInfoIterator;
        int systemRowIndex;
        int serverRowIndex;

        ItemSearchInfoBase()
            : systemInfoIterator()
            , serverInfoIterator()
            , systemRowIndex()
            , serverRowIndex()
        {}

        ItemSearchInfoBase(SystemIterator sysInfoIterator
            , ServerIterator srvInfoIterator
            , int sysRowIndex
            , int serverRowIndex)
            : systemInfoIterator(sysInfoIterator)
            , serverInfoIterator(srvInfoIterator)
            , systemRowIndex(sysRowIndex)
            , serverRowIndex(serverRowIndex)
        {}
    };

    typedef ItemSearchInfoBase<SystemModelInfosVector::iterator
        , ServerModelInfosVector::iterator> ItemSearchInfo;
    typedef ItemSearchInfoBase<SystemModelInfosVector::const_iterator
        , ServerModelInfosVector::const_iterator> ItemSearchInfoConst;

    class SelectionUpdaterGuard
    {
    public:
        SelectionUpdaterGuard(rtu::ServerInfoManager *serverInfoManager
            , const rtu::ServerInfoManager::SuccessfulCallback &success
            , const rtu::ServerInfoManager::FailedCallback &failed);

        ~SelectionUpdaterGuard();

        void addServerToUpdate(const api::ServerInfo *info);

    private:
        rtu::ServerInfoManager * const m_serverInfoManager;
        const rtu::ServerInfoManager::SuccessfulCallback m_success;
        const rtu::ServerInfoManager::FailedCallback m_failed;
        api::ServerInfoContainer m_selectedServers;
    };

    SelectionUpdaterGuard::SelectionUpdaterGuard(rtu::ServerInfoManager *serverInfoManager
        , const rtu::ServerInfoManager::SuccessfulCallback &success
        , const rtu::ServerInfoManager::FailedCallback &failed)
        : m_serverInfoManager(serverInfoManager)
        , m_success(success)
        , m_failed(failed)
        , m_selectedServers()
    {
    }

    SelectionUpdaterGuard::~SelectionUpdaterGuard()
    {
        if (!m_selectedServers.empty())
            m_serverInfoManager->updateServerInfos(
                m_selectedServers, m_success, m_failed);
    }

    void SelectionUpdaterGuard::addServerToUpdate(const api::ServerInfo *info)
    {
        if (info && info->hasExtraInfo())
            m_selectedServers.push_back(*info);
    }

    class ActiveDestructable
    {
    public:
        typedef std::function<void ()> Callback;
        ActiveDestructable(const Callback &callback);

        ~ActiveDestructable();

        void cancel();

    private:
        Callback m_callback;
    };

    ActiveDestructable::ActiveDestructable(const Callback &callback)
        : m_callback(callback)
    {
    }

    ActiveDestructable::~ActiveDestructable()
    {
        if (m_callback)
            m_callback();
    }

    void ActiveDestructable::cancel()
    {
        m_callback = Callback();
    }

    typedef QSharedPointer<ActiveDestructable> ActiveDestructablePtr;
}

namespace
{
    enum { kSystemItemCapacity = 1 };
    enum { kUnknownGroupItemCapacity = 1 };

    const QString factorySystemName;    /// empty system name shows if it is MAC

    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1

        /// Common role ids
        , kItemType = kFirstCustomRoleId
        , kNameRoleId
        , kSelectedStateRoleId

        /// System's specific roles
        , kSelectedServersCountRoleId
        , kServersCountRoleId
        , kLoggedState
        , kEnabledRoleId

        /// Server's specific roles
        , kIdRoleId
        , kSystemNameRoleId
        , kMacAddressRoleId
        , kIpAddressRoleId
        , kPortRoleId
        , kLoggedIn
        , kDefaultPassword
        , kBusyStateRoleId
        , kSafeModeRoleId
        , kHasHddRoleId
        , kVersionRoleId
        , kOsRoleId
        , kOperationRoleId
        , kAvailableByHttp

        , kLastCustomRoleId
    };

    const rtu::Roles kRolesDescription = []() -> rtu::Roles
    {
        rtu::Roles result;
        result.insert(kItemType, "itemType");
        result.insert(kNameRoleId, "name");
        result.insert(kSelectedStateRoleId, "selectedState");

        result.insert(kSelectedServersCountRoleId, "selectedServersCount");
        result.insert(kServersCountRoleId, "serversCount");
        result.insert(kLoggedState, "loggedState");
        result.insert(kEnabledRoleId, "enabled");

        result.insert(kIdRoleId, "id");
        result.insert(kSystemNameRoleId, "systemName");
        result.insert(kMacAddressRoleId, "macAddress");
        result.insert(kIpAddressRoleId, "ipAddress");
        result.insert(kPortRoleId, "port");
        result.insert(kLoggedIn, "loggedIn");
        result.insert(kDefaultPassword, "defaultPassword");
        result.insert(kBusyStateRoleId, "isBusy");
        result.insert(kSafeModeRoleId, "safeMode");
        result.insert(kHasHddRoleId, "hasHdd");
        result.insert(kVersionRoleId, "version");
        result.insert(kOsRoleId, "os");
        result.insert(kOperationRoleId, "operation");
        result.insert(kAvailableByHttp, "availableByHttp");

        return result;
    }();

    typedef std::function<void (int startIndex, int finishIndex)> MultiIndexActionType;
    typedef std::function<void ()> UnindexedActionType;

    typedef std::function<void ()> SelectionChangedActionType;
    typedef std::function<void (const api::ServerInfo &info)> ServerInfoActionType;
}

namespace
{
    template<typename SystemContainerType
        , typename SearchInfoType>
    bool findItem(int rowIndex
        , SystemContainerType &systems
        , SearchInfoType &searchInfo)
    {
        int index = 0;
        int systemRowIndex = 0;
        const int systemsCount = systems.size();
        for (int systemIndex = 0; systemIndex != systemsCount; ++systemIndex)
        {
            typename SearchInfoType::SystemItType systemIt = systems.begin() + systemIndex;
            if (index == rowIndex)
            {
                enum { kInvalidServerRowIndex = -1 };
                searchInfo.systemInfoIterator = systemIt;
                searchInfo.systemRowIndex = rowIndex;
                searchInfo.serverInfoIterator = systemIt->servers.end();
                searchInfo.serverRowIndex = kInvalidServerRowIndex;
                return true;
            }

            enum { kSystemItemOffset = 1 };
            index += kSystemItemOffset;

            const int serversCount = systemIt->servers.size();
            if ((index + serversCount) > rowIndex)
            {
                searchInfo.systemInfoIterator = systemIt;
                searchInfo.systemRowIndex = systemRowIndex;
                searchInfo.serverInfoIterator = systemIt->servers.begin() + (rowIndex - index);
                searchInfo.serverRowIndex = rowIndex;
                return true;
            }

            index += serversCount;
            systemRowIndex += serversCount + kSystemItemOffset;
        }
        return false;
    }
}
class rtu::ServersSelectionModel::Impl : public QObject
{
public:
    Impl(rtu::ServersSelectionModel *owner
         , rtu::ModelChangeHelper *changeHelper);

    virtual ~Impl();

    int rowCount() const;

    int knownEntitiesCount() const;

    int unknownEntitiesCount() const;

    int serversCount() const;

    QVariant data(const QModelIndex &index
        , int role) const;

    QVariant knownEntitiesData(int row
        , int role) const;

    QVariant unknownEntitiesData(int unknownItemIndex
        , int role) const;

    void changeServerSelectedState(SystemModelInfo &systemModelInfo
        , ServerModelInfo &serverModelInfo
        , bool selected
        , SelectionUpdaterGuard *guard = nullptr);

    void changeItemSelectedState(int rowIndex
        , bool explicitSelection
        , bool updateSelection = true
        , SelectionUpdaterGuard *guard = nullptr);

    void setSelectedItems(const IDsVector &ids);

    void setAllItemSelected(bool selected);

    int selectedCount() const;

    api::ServerInfoPtrContainer selectedServers();

    void tryLoginWith(const QString &primarySystem
        , const QString &password
        , const Callback &callback);

    ///

    api::ExtraServerInfo& getExtraInfo(ItemSearchInfo &searchInfo);

    void updateTimeDateInfo(ItemSearchInfo &searchInfo
        , qint64 utcDateTimeMs
        , const QByteArray &timeZoneId
        , qint64 timestampMs);

    void updateActionsInfo(ItemSearchInfo &searchInfo
        , api::Constants::SystemCommands sysCommands);

    void updateInterfacesInfo(ItemSearchInfo &searchInfo
        , const InterfaceInfoList &interfaces
        , const QString &host);

    void updatePasswordInfo(ItemSearchInfo &searchInfo
        , const QString &password);

    void updateSystemNameInfo(const QUuid &id
        , const QString &systemName);

    void updatePortInfo(const QUuid &id
        , int port);

    void serverDiscovered(const BaseServerInfo &baseInfo);

    void blinkForItem(int row);

    void addServer(const api::ServerInfo &info
        , Qt::CheckState selected = Qt::Unchecked
        , bool locked = false
        , const QString &lockReason = QString()
        , int removeRequestsCounter = 0);

    void changeServer(const api::BaseServerInfo &info
        , bool outdate = false);

    void removeServers(const rtu::IDsVector &removed);

    void unknownAdded(const QString &adderss);

    void unknownRemoved(const QString &address);

    void updateExtraInfo(const QUuid &id
        , const ExtraServerInfo &extraServerInfo
        , const QString &host);

    void updateExtraInfoFailed(const QUuid &id
        , api::Client::ResultCode errorCode);

    bool findServer(const QUuid &id
        , ItemSearchInfo *searchInfo);

    void setLockedState(const IDsVector &ids
        , bool locked
        , const QString &reason);

    void changeAccessMethod(
        const QUuid& id, HttpAccessMethod newHttpAccessMethod);

private:
    SystemModelInfo *findSystemModelInfo(const QString &systemName
        , int &row);

    void setSystemSelectedStateImpl(SystemModelInfo &systemInfo
        , bool selected);


private:
    /// return true if no selected servers
    void removeServerImpl(const QUuid &id
        , bool removeEmptySystem
        , bool forceRemove = false);

private:
    rtu::ServersSelectionModel * const m_owner;
    rtu::ModelChangeHelper * const m_changeHelper;

    SystemModelInfosVector m_systems;
    rtu::ServerInfoManager *m_serverInfoManager;

    const ServerInfoManager::SuccessfulCallback m_updateExtra;
    const ServerInfoManager::FailedCallback m_updateExtraFailed;

    QStringList m_unknownEntities;

};

rtu::ServersSelectionModel::Impl::Impl(rtu::ServersSelectionModel *owner
    , rtu::ModelChangeHelper *changeHelper)
    : QObject(owner)
    , m_owner(owner)
    , m_changeHelper(changeHelper)
    , m_systems()

    , m_serverInfoManager(new ServerInfoManager(this))

    , m_updateExtra([this](const QUuid &id, const ExtraServerInfo &extra
        , const QString &host) { updateExtraInfo(id, extra, host); })
    , m_updateExtraFailed([this](const QUuid &id, api::Client::ResultCode errorCode) { updateExtraInfoFailed(id, errorCode); })
    , m_unknownEntities()

{
}

rtu::ServersSelectionModel::Impl::~Impl() {}

int rtu::ServersSelectionModel::Impl::rowCount() const
{
    return knownEntitiesCount() + unknownEntitiesCount();
}

int rtu::ServersSelectionModel::Impl::knownEntitiesCount() const
{
    return serversCount() + m_systems.count();
}

int rtu::ServersSelectionModel::Impl::unknownEntitiesCount() const
{
    return (m_unknownEntities.isEmpty() ? 0
        : kUnknownGroupItemCapacity + m_unknownEntities.size());
}

int rtu::ServersSelectionModel::Impl::serversCount() const
{
    int count = 0;
    std::for_each(m_systems.begin(), m_systems.end()
        , [&count](const SystemModelInfo& systemInfo)
    {
        count += systemInfo.servers.size();
    });
    return count;
}


QVariant rtu::ServersSelectionModel::Impl::knownEntitiesData(int row
    , int role) const
{
    ItemSearchInfoConst searchInfo;
    if (!findItem(row, m_systems, searchInfo))
        return QVariant();

    const SystemModelInfo &systemInfo = *searchInfo.systemInfoIterator;
    if (searchInfo.serverInfoIterator != searchInfo.systemInfoIterator->servers.end())
    {
        /// Returns values for server model item

        const ServerModelInfo &serverInfo = *searchInfo.serverInfoIterator;
        const ServerInfo &info = searchInfo.serverInfoIterator->serverInfo;
        switch(role)
        {
        case kItemType:
            return Constants::ServerItemType;
        case kSelectedStateRoleId:
            return serverInfo.selectedState;
        case kSystemNameRoleId:
            return systemInfo.name;
        case kNameRoleId:
            return info.baseInfo().name;
        case kIpAddressRoleId:
            return info.baseInfo().displayAddress;
        case kVersionRoleId:
            return info.baseInfo().version.toString();
        case kOsRoleId:
            return info.baseInfo().os;
        case kIdRoleId:
            return info.baseInfo().id;
        case kOperationRoleId:
        {
            if (searchInfo.serverInfoIterator->locked)
                return searchInfo.serverInfoIterator->lockReason;
            else if (searchInfo.serverInfoIterator->loginState == kDifferentNetwork)
                return tr("Server is unavailable");

            return QString();
        }
        case kAvailableByHttp:
            return info.baseInfo().httpAccessMethod == HttpAccessMethod::kTcp;
        case kMacAddressRoleId:
        {
            const bool hasMacAddress = info.hasExtraInfo() && !info.extraInfo().interfaces.empty();
            return (hasMacAddress ? info.extraInfo().interfaces.front().macAddress : QString());
        }
        case kLoggedIn:
            return info.hasExtraInfo();
        case kBusyStateRoleId:
            return searchInfo.serverInfoIterator->locked;
        case kSafeModeRoleId:
            return searchInfo.serverInfoIterator->serverInfo.baseInfo().safeMode;
        case kHasHddRoleId:
            return (searchInfo.serverInfoIterator->serverInfo.baseInfo().flags.testFlag(api::Constants::HasHdd));
        case kPortRoleId:
            return info.baseInfo().port;
        case kDefaultPassword:
            return (info.hasExtraInfo() && api::Client::defaultAdminPasswords().contains(info.extraInfo().password));
        }
    }
    else
    {
        /// Returns values for system model item
        switch(role)
        {
        case kItemType:
            return Constants::SystemItemType;
        case kNameRoleId:
        case kSystemNameRoleId:
            return systemInfo.name;
        case kSelectedStateRoleId:
            return (systemInfo.servers.empty() || !systemInfo.selectedServers ? Qt::Unchecked :
                (systemInfo.selectedServers == systemInfo.servers.size() ? Qt::Checked : Qt::PartiallyChecked));
        case kLoggedState:
            return (systemInfo.unauthorizedServers == systemInfo.servers.size() ? Constants::NotLogged
                : (systemInfo.unauthorizedServers ? Constants::PartiallyLogged : Constants::LoggedToAllServers));

        case kEnabledRoleId:
        {
            /// TODO: #ynikitenkov refactor this. Add calcaulation "on fly", not here
            for (const auto &serverModelInfo: systemInfo.servers)
            {
                if (!serverModelInfo.locked && (serverModelInfo.loginState == kServerLoggedInState))
                    return true;
            }
            return false;
        }
        case kSelectedServersCountRoleId:
            return systemInfo.selectedServers;
        case kServersCountRoleId:
            return systemInfo.servers.size();
        }
    }

    return QVariant();
}

QVariant rtu::ServersSelectionModel::Impl::unknownEntitiesData(int unknownItemIndex
    , int role) const
{
    static const QString kGroupName = "Other discovered servers";

    enum { kGroupIndex = 0 };
    if ((unknownItemIndex == kGroupIndex))
    {
        switch(role)
        {
        case kItemType:
            return Constants::UnknownGroupType;
        case kNameRoleId:
            return kGroupName;
        }
    }
    else
    {
        switch(role)
        {
        case kItemType:
            return Constants::UnknownEntityType;
        case kIpAddressRoleId:
            return m_unknownEntities.at(unknownItemIndex - kUnknownGroupItemCapacity);
        }
    }
    return QVariant();
}

QVariant rtu::ServersSelectionModel::Impl::data(const QModelIndex &index
    , int role) const
{
    const int row = index.row();

    if ((role < kFirstCustomRoleId) || (role > kLastCustomRoleId)
        || (row < 0) || (row >= rowCount()))
        return QVariant();

    const int knownCount = knownEntitiesCount();

    if (row < knownCount)
    {
        return knownEntitiesData(row, role);
    }
    return unknownEntitiesData(row - knownCount, role);
}

SystemModelInfo *rtu::ServersSelectionModel::Impl::findSystemModelInfo(const QString &systemName
    , int &row)
{
    int systemRowIndex = 0;
    const auto it = std::find_if(m_systems.begin(), m_systems.end()
        , [&systemName, &systemRowIndex](const SystemModelInfo &info)
    {
        if (info.name == systemName)
            return true;

        systemRowIndex += info.servers.size() + kSystemItemCapacity;
        return false;
    });

    if (it == m_systems.end())
        return nullptr;

    row = systemRowIndex;
    return &*it;
}

void rtu::ServersSelectionModel::Impl::setSystemSelectedStateImpl(SystemModelInfo &systemInfo
    , bool selected)
{
    Qt::CheckState newState = (selected ? Qt::Checked : Qt::Unchecked);
    int selectedCount = 0;
    for(ServerModelInfo &server: systemInfo.servers)
    {
        if (!server.serverInfo.hasExtraInfo() || server.locked)
            continue;

        server.selectedState = newState;
        ++selectedCount;
    }
    systemInfo.selectedServers = (selected ? selectedCount : 0);
}

void rtu::ServersSelectionModel::Impl::setSelectedItems(const IDsVector &ids)
{
    SelectionUpdaterGuard guard(m_serverInfoManager, m_updateExtra, m_updateExtraFailed);

    for (auto &systemModelInfo: m_systems)
    {
        for (auto &serverModelInfo: systemModelInfo.servers)
        {
            const bool shouldBeSelected = ids.contains(serverModelInfo.serverInfo.baseInfo().id);
            const bool selected = (serverModelInfo.selectedState == Qt::Checked);
            const bool shouldBeUpdated = (shouldBeSelected ^ selected);
            if (!shouldBeUpdated || serverModelInfo.locked
                || (serverModelInfo.loginState != kServerLoggedInState))
           {
                continue;
            }

            changeServerSelectedState(systemModelInfo, serverModelInfo, !selected, &guard);
        }
    }
    m_changeHelper->dataChanged(0, knownEntitiesCount() - 1);

    emit m_owner->selectionChanged();
}

void rtu::ServersSelectionModel::Impl::changeServerSelectedState(SystemModelInfo &systemModelInfo
    , ServerModelInfo &serverModelInfo
    , bool selected
    , SelectionUpdaterGuard *guard)
{
    const bool currentSelected = (serverModelInfo.selectedState == Qt::Checked);
    if (currentSelected == selected)
        return;

    serverModelInfo.selectedState = (selected ? Qt::Checked : Qt::Unchecked);

    if (selected)
    {
        ++systemModelInfo.selectedServers;
        guard->addServerToUpdate(&serverModelInfo.serverInfo);
    }
    else
        --systemModelInfo.selectedServers;
}

void rtu::ServersSelectionModel::Impl::changeItemSelectedState(int rowIndex
    , bool explicitSelection
    , bool updateSelection
    , SelectionUpdaterGuard *guard)
{
    ItemSearchInfo searchInfo;
    if (!findItem(rowIndex, m_systems, searchInfo))
        return;

    typedef QScopedPointer<SelectionUpdaterGuard> GuardHolder;
    GuardHolder localGuard;
    if (!guard)
    {
        localGuard.reset(new SelectionUpdaterGuard(
            m_serverInfoManager, m_updateExtra, m_updateExtraFailed));
        guard = localGuard.data();
    }

    if (searchInfo.serverInfoIterator != searchInfo.systemInfoIterator->servers.end())
    {
        if (!searchInfo.serverInfoIterator->serverInfo.hasExtraInfo()
            || searchInfo.serverInfoIterator->locked)  /// Not logged in or applying changes - do nothing
        {
            return;
        }

        const ServerModelInfo &serverInfo = *searchInfo.serverInfoIterator;
        const bool wasSelected = (serverInfo.selectedState != Qt::Unchecked);

        if (explicitSelection)
        {
            for(auto &system: m_systems)
            {
                setSystemSelectedStateImpl(system, false);
            }
            m_changeHelper->dataChanged(0, knownEntitiesCount() - 1);
        }

        const bool selected = (!wasSelected || explicitSelection);

        changeServerSelectedState(*searchInfo.systemInfoIterator, *searchInfo.serverInfoIterator
            , selected, guard);

        if (!selected && explicitSelection) /// TODO: #ynikitenkov: refactor this pornography! Make selection change code more clear
            ++searchInfo.systemInfoIterator->selectedServers;


        m_changeHelper->dataChanged(rowIndex, rowIndex);
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
    }
    else
    {
        SystemModelInfo &systemInfo = *searchInfo.systemInfoIterator;
        const Qt::CheckState systemSelection = (!systemInfo.selectedServers ? Qt::Unchecked :
            (systemInfo.selectedServers < systemInfo.servers.size() ? Qt::PartiallyChecked : Qt::Checked));

        const bool newSelectedState = (systemSelection == Qt::Unchecked);
        setSystemSelectedStateImpl(systemInfo, newSelectedState);
        if (newSelectedState)
        {
            for(const ServerModelInfo& info: systemInfo.servers)
            {
                guard->addServerToUpdate(&info.serverInfo);
            }
        }
        m_changeHelper->dataChanged(searchInfo.systemRowIndex
            , searchInfo.systemRowIndex + searchInfo.systemInfoIterator->servers.size());
    }

    if (updateSelection)
        emit m_owner->selectionChanged();
}

void rtu::ServersSelectionModel::Impl::setAllItemSelected(bool selected)
{
    bool selectionChanged = false;
    int index = 0;

    SelectionUpdaterGuard guard(m_serverInfoManager, m_updateExtra, m_updateExtraFailed);
    for (int systemIndex = 0; systemIndex != m_systems.count(); ++systemIndex)
    {
        index += kSystemItemCapacity;

        const SystemModelInfo &systemInfo = m_systems.at(systemIndex);
        if ((selected && (systemInfo.selectedServers != systemInfo.servers.size()))
            || (!selected && systemInfo.selectedServers))
        {
            for (int serverIndex = 0; serverIndex != systemInfo.servers.size(); ++serverIndex)
            {
                const ServerModelInfo &serverInfo = systemInfo.servers.at(serverIndex);
                const bool wasSelected = (serverInfo.selectedState == Qt::Checked);
                if ((wasSelected != selected) && serverInfo.serverInfo.hasExtraInfo())
                {
                    changeItemSelectedState(index + serverIndex, false, false, &guard);
                    selectionChanged = true;
                }
            }
        }
        index += systemInfo.servers.size();
    }

    if (selectionChanged)
        emit m_owner->selectionChanged();
}

int rtu::ServersSelectionModel::Impl::selectedCount() const
{
    int count = 0;
    for(const SystemModelInfo &info: m_systems)
    {
        count += info.selectedServers;
    }
    return count;
}

api::ServerInfoPtrContainer rtu::ServersSelectionModel::Impl::selectedServers()
{
    ServerInfoPtrContainer result;
    for(SystemModelInfo &info: m_systems)
    {
        if (info.selectedServers)
        {
            for (ServerModelInfo &serverInfo: info.servers)
            {
                if (serverInfo.selectedState != Qt::Unchecked)
                    result.push_back(&serverInfo.serverInfo);
            }
        }
    }
    return result;
}

void rtu::ServersSelectionModel::Impl::tryLoginWith(const QString &primarySystem
    , const QString &password
    , const Callback &callback)
{
    const auto &failedHandler = ActiveDestructablePtr(new ActiveDestructable(callback));

    for (const SystemModelInfo &systemInfo: m_systems)
    {
        if (!systemInfo.unauthorizedServers == systemInfo.servers.size())
            continue;

        for(const ServerModelInfo &serverInfo: systemInfo.servers)
        {
            if (serverInfo.serverInfo.hasExtraInfo())
                continue;

            auto handler = (serverInfo.serverInfo.baseInfo().systemName == primarySystem
                ? failedHandler : ActiveDestructablePtr());

            const auto &success =
                [this, handler](const QUuid &id, const ExtraServerInfo &extra, const QString &host)
            {
                updateExtraInfo(id, extra, host);
                if (handler)
                    handler->cancel();
            };

            m_serverInfoManager->loginToServer(serverInfo.serverInfo.baseInfo(), password
                , success, m_updateExtraFailed);
        }
    }
}

api::ExtraServerInfo& rtu::ServersSelectionModel::Impl::getExtraInfo(ItemSearchInfo &searchInfo)
{
    ServerInfo &info = searchInfo.serverInfoIterator->serverInfo;
    if (!info.hasExtraInfo())
    {
        if (searchInfo.serverInfoIterator->loginState == kServerUnauthorized)
            --searchInfo.systemInfoIterator->unauthorizedServers;

        searchInfo.serverInfoIterator->loginState = kServerLoggedInState;
        info.setExtraInfo(ExtraServerInfo());
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);

        ++searchInfo.systemInfoIterator->loggedServers;
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
    }

    return info.writableExtraInfo();
}

void rtu::ServersSelectionModel::Impl::updateExtraInfo(const QUuid &id
    , const ExtraServerInfo &extraInfo
    , const QString &hostName)
{
    /// server is discovered and logged in

    ItemSearchInfo searchInfo;
    if (!findServer(id, &searchInfo))
        return;

    updatePasswordInfo(searchInfo, extraInfo.password);
    updateTimeDateInfo(searchInfo, extraInfo.utcDateTimeMs
        , extraInfo.timeZoneId, extraInfo.timestampMs);
    updateInterfacesInfo(searchInfo, extraInfo.interfaces, hostName);
    updateActionsInfo(searchInfo, extraInfo.sysCommands);

    m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
}

void rtu::ServersSelectionModel::Impl::updateExtraInfoFailed(const QUuid &id
    , api::Client::ResultCode errorCode)
{
    /// Server is not available or can't be logged in
    ItemSearchInfo searchInfo;
    if (!findServer(id, &searchInfo))
        return;

    const ServerLoggedState newLoggedState = (errorCode == api::Client::ResultCode::kUnauthorized
        ? kServerUnauthorized : kDifferentNetwork);

    ServerLoggedState &currentLoggedState = searchInfo.serverInfoIterator->loginState;

    bool updateSystemRow = false;
    if (currentLoggedState == kServerLoggedInState)
    {
        --searchInfo.systemInfoIterator->loggedServers;
        updateSystemRow = true;
    }

    if ((currentLoggedState != kServerUnauthorized) && (newLoggedState == kServerUnauthorized))
    {
        ++searchInfo.systemInfoIterator->unauthorizedServers;
        updateSystemRow = true;
    }

    if ((currentLoggedState == kServerUnauthorized) && (newLoggedState == kDifferentNetwork))
    {
        --searchInfo.systemInfoIterator->unauthorizedServers;
        updateSystemRow = true;
    }

    if (updateSystemRow)
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);

    currentLoggedState = newLoggedState;

    searchInfo.serverInfoIterator->serverInfo.resetExtraInfo();
    m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);

    if (searchInfo.serverInfoIterator->selectedState == Qt::Checked)
    {
        searchInfo.serverInfoIterator->selectedState = Qt::Unchecked;
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);

        --searchInfo.systemInfoIterator->selectedServers;
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);

        if (!selectedCount())
            emit m_owner->selectionChanged();
    }
}

void rtu::ServersSelectionModel::Impl::updateTimeDateInfo(
    ItemSearchInfo &searchInfo
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , qint64 timestampMs)
{
    ExtraServerInfo &extra = getExtraInfo(searchInfo);

    enum { kMaxMsecDiff = 5000 };
    const auto timeDiffMs = llabs(utcDateTimeMs - (extra.utcDateTimeMs + (timestampMs - extra.timestampMs)));
    const bool diffTime = (timeDiffMs > kMaxMsecDiff);
    const bool diffTimeZone = (extra.timeZoneId != timeZoneId);

    if (diffTimeZone)
        extra.timeZoneId = timeZoneId;

    if (diffTime)
    {
        extra.utcDateTimeMs = utcDateTimeMs;
        extra.timestampMs = timestampMs;
    }

    if ((searchInfo.serverInfoIterator->selectedState == Qt::Checked) && (diffTime || diffTimeZone))
        emit m_owner->updateSelectionData();
}

void rtu::ServersSelectionModel::Impl::updateActionsInfo(ItemSearchInfo &searchInfo
    , api::Constants::SystemCommands sysCommands)
{
    ExtraServerInfo &extra = getExtraInfo(searchInfo);
    if (extra.sysCommands == sysCommands)
        return;

    extra.sysCommands = sysCommands;
    if (searchInfo.serverInfoIterator->selectedState == Qt::Checked)
        emit m_owner->updateSelectionData();
}

void rtu::ServersSelectionModel::Impl::updateInterfacesInfo(
    ItemSearchInfo &searchInfo
    , const InterfaceInfoList &interfaces
    , const QString &host)
{
    searchInfo.serverInfoIterator->serverInfo.writableBaseInfo().hostAddress = host;
    m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);

    ExtraServerInfo &extra = getExtraInfo(searchInfo);

    bool hasChanges = (extra.interfaces.size() != interfaces.size());
    for (const api::InterfaceInfo &itf: interfaces)
    {
        InterfaceInfoList &oldItf = extra.interfaces;
        const auto &it = std::find_if(oldItf.begin(), oldItf.end()
            , [itf](const api::InterfaceInfo &param)
        {
            return (param.name == itf.name);
        });

        if (it == oldItf.end())
        {
            oldItf.push_back(itf);
            unknownRemoved(itf.ip);
            continue;
        }

        if (*it != itf)
        {
            unknownRemoved(itf.ip);
            hasChanges = true;
        }

        *it = itf;
    }

    if ((searchInfo.serverInfoIterator->selectedState == Qt::Checked) && hasChanges)
        emit m_owner->updateSelectionData();
}

void rtu::ServersSelectionModel::Impl::updatePasswordInfo(
    ItemSearchInfo &searchInfo
    , const QString &password)
{
    ExtraServerInfo &extra = getExtraInfo(searchInfo);
    const QString oldPassword = extra.password;
    if (oldPassword == password)
        return;

    extra.password = password;

    const BaseServerInfo &info = searchInfo.serverInfoIterator->serverInfo.baseInfo();
    for (ServerModelInfo &otherInfo: searchInfo.systemInfoIterator->servers)
    {
        ServerInfo &otherServerInfo = otherInfo.serverInfo;
        if (otherServerInfo.hasExtraInfo()
            && (oldPassword == otherServerInfo.extraInfo().password)
            && (info.systemName == otherServerInfo.baseInfo().systemName))
        {
            otherServerInfo.writableExtraInfo().password = password;
        }
    }

    m_changeHelper->dataChanged(searchInfo.systemRowIndex
        , searchInfo.systemRowIndex + searchInfo.systemInfoIterator->servers.size());

    if (searchInfo.serverInfoIterator->selectedState == Qt::Checked)
        emit m_owner->updateSelectionData();
}

void rtu::ServersSelectionModel::Impl::updateSystemNameInfo(const QUuid &id
    , const QString &systemName)
{
    ItemSearchInfo searchInfo;
    if (!findServer(id, &searchInfo))
        return;

    BaseServerInfo base =
        searchInfo.serverInfoIterator->serverInfo.baseInfo();
    base.systemName = systemName;
    base.flags &= ~api::Constants::IsFactoryFlag;
    changeServer(base);
}

void rtu::ServersSelectionModel::Impl::updatePortInfo(const QUuid &id
    , int port)
{
    ItemSearchInfo searchInfo;
    if (!findServer(id, &searchInfo))
        return;

    BaseServerInfo base =
        searchInfo.serverInfoIterator->serverInfo.baseInfo();
    base.port = port;
    changeServer(base);
}

void rtu::ServersSelectionModel::Impl::blinkForItem(int row)
{
    ItemSearchInfoConst searchInfo;
    if (!findItem(row, m_systems, searchInfo))
        return;

    emit m_owner->blinkAtSystem(searchInfo.systemRowIndex);
}

void rtu::ServersSelectionModel::Impl::serverDiscovered(const BaseServerInfo &baseInfo)
{
    ItemSearchInfo searchInfo;
    if (!findServer(baseInfo.id, &searchInfo))
        return;

    // At least x3 because http and multicast timeouts can occur.
    enum { kUpdatePeriod = api::Client::kDefaultTimeoutMs * 3 };

    /// TODO: #ynikitenkov change for QElapsedTimer implementation
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 msSinceLastUpdate = (now - searchInfo.serverInfoIterator->updateTimestamp);
    if (msSinceLastUpdate < kUpdatePeriod)
        return;
    searchInfo.serverInfoIterator->updateTimestamp = now;

    if (searchInfo.serverInfoIterator->loginState == kServerUnauthorized)
        return;

    const ServerInfo &foundInfo = searchInfo.serverInfoIterator->serverInfo;
    ServerInfo tmp = (!foundInfo.hasExtraInfo() ? ServerInfo(baseInfo)
        : ServerInfo(baseInfo, foundInfo.extraInfo()));

    m_serverInfoManager->updateServerInfos(api::ServerInfoContainer(1, tmp)
        , m_updateExtra, m_updateExtraFailed);
}

void rtu::ServersSelectionModel::Impl::addServer(const ServerInfo &info
    , Qt::CheckState selected
    , bool locked
    , const QString &lockReason
    , int removeRequestsCounter)
{
    const QString systemName = (info.baseInfo().flags.testFlag(api::Constants::IsFactoryFlag)
        ? QString() : info.baseInfo().systemName);

    int row = 0;
    SystemModelInfo *systemModelInfo = findSystemModelInfo(systemName, row);
    if (!systemModelInfo)
    {

        SystemModelInfo systemInfo(systemName);
        SystemModelInfosVector::iterator place = std::lower_bound(m_systems.begin(), m_systems.end(), systemInfo,
            [](const SystemModelInfo &left, const SystemModelInfo &right) {
                return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0; });

        if (place == m_systems.end())
        {
            row = knownEntitiesCount();
        }
        else
        {
            findSystemModelInfo(place->name, row);
        }

        const auto &guard = m_changeHelper->insertRowsGuard(row, row);
        Q_UNUSED(guard);
        systemModelInfo = m_systems.insert(place, systemInfo);
    }

    const ServerLoggedState loginState = (info.hasExtraInfo() ? kServerLoggedInState : kDifferentNetwork);

    {
        const int serverRow = (row + systemModelInfo->servers.size() + 1);
        const auto &guard = m_changeHelper->insertRowsGuard(serverRow, serverRow);
        Q_UNUSED(guard);

        /// If removeRequestsCounter is 0 then it means it is first-time addition operation
        const int finalRemoveRequestsCount = (removeRequestsCounter > 0 ? removeRequestsCounter - 1 : 0);

        /// TODO: #ynikitenkov change for QElapsedTimer implementation
        systemModelInfo->servers.push_back(ServerModelInfo(QDateTime::currentMSecsSinceEpoch(), selected
            , loginState, locked, lockReason, finalRemoveRequestsCount, info));
    }

    systemModelInfo->loggedServers += (loginState == kServerLoggedInState ? 1 : 0);

    if (selected != Qt::Unchecked)
        ++systemModelInfo->selectedServers;
    m_changeHelper->dataChanged(row, row);

    if (!info.hasExtraInfo())
        m_serverInfoManager->loginToServer(info.baseInfo()
            , m_updateExtra, m_updateExtraFailed);


    emit m_owner->serversCountChanged();
}

void rtu::ServersSelectionModel::Impl::changeServer(const BaseServerInfo &baseInfo
    , bool outdate)
{
    ItemSearchInfo searchInfo;
    if (!findServer(baseInfo.id, &searchInfo))
        return;

    unknownRemoved(baseInfo.hostAddress);
    ServerInfo foundServer = searchInfo.serverInfoIterator->serverInfo;
    const bool selectionOutdated = (searchInfo.serverInfoIterator->selectedState == Qt::Checked)
        && outdate && (foundServer.baseInfo() != baseInfo);

    const bool newIsFactory = (baseInfo.flags.testFlag(api::Constants::IsFactoryFlag));
    const bool currentIsFactory = (foundServer.baseInfo().flags.testFlag(api::Constants::IsFactoryFlag));
    const bool diffSystemName = ((foundServer.baseInfo().systemName != baseInfo.systemName)
        || (newIsFactory != currentIsFactory));

    if (diffSystemName)
    {
        const Qt::CheckState selected = searchInfo.serverInfoIterator->selectedState;

        const QString newSystemName = (newIsFactory ? QString() : baseInfo.systemName);

        int targetSystemRow = 0;
        const bool targetSystemExists =
            (findSystemModelInfo(newSystemName, targetSystemRow) != nullptr);

        SystemModelInfo &system = *searchInfo.systemInfoIterator;
        const bool inplaceRename = !targetSystemExists && (system.servers.size() == 1);
        if (inplaceRename)
        {
            system.name = newSystemName;
            m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
        }

        const auto locked = searchInfo.serverInfoIterator->locked;
        const auto lockReason = searchInfo.serverInfoIterator->lockReason;

        /// Remove operation with "force" flag never increases counter, thus
        /// increase it manualy
        const auto counter = searchInfo.serverInfoIterator->removeRequestsCounter + 1;

        removeServerImpl(baseInfo.id, targetSystemExists, true);
        foundServer.setBaseInfo(baseInfo);
        addServer(foundServer, selected, locked, lockReason, counter);
    }
    else
    {
        /// just updates information
        searchInfo.serverInfoIterator->serverInfo.setBaseInfo(baseInfo);
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
    }

    if (selectionOutdated)
        emit m_owner->updateSelectionData();
}

bool rtu::ServersSelectionModel::Impl::findServer(const QUuid &id
    , ItemSearchInfo *searchInfo)
{
    Q_ASSERT(searchInfo != nullptr);

    int rowIndex = 0;
    for(auto itSystem = m_systems.begin(); itSystem != m_systems.end(); ++itSystem)
    {
        for(auto itServer = itSystem->servers.begin(); itServer != itSystem->servers.end(); ++itServer)
        {
            if (itServer->serverInfo.baseInfo().id == id)
            {
                *searchInfo = ItemSearchInfo(itSystem, itServer, rowIndex
                    , rowIndex + kSystemItemCapacity + (itServer - itSystem->servers.begin()));
                return true;
            }
        }
        rowIndex += kSystemItemCapacity + itSystem->servers.count();
    }
    return false;
}

void rtu::ServersSelectionModel::Impl::setLockedState(const IDsVector &ids
    , bool locked
    , const QString &reason)
{
    bool selectionChanged = false;
    for (const QUuid &id: ids)
    {
        ItemSearchInfo searchInfo;
        if (!findServer(id, &searchInfo))
            continue;

        enum { kItemNotFoundIndex = -1 };
        ServerModelInfo &modelInfo = *searchInfo.serverInfoIterator;
        if (locked == modelInfo.locked)
        {
            Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid lock state");
            continue;
        }

        modelInfo.locked = locked;
        modelInfo.lockReason = reason;

        if (!locked && modelInfo.removeRequestsCounter > 0)
        {
            Q_ASSERT_X(modelInfo.removeRequestsCounter == 1, Q_FUNC_INFO    /// removeRequestsCounter is int type. It is to find out if any
                , "wrong removeRequestsCounter is greater than 1");         /// logic error about addition and deletion servers presences.
                                                                            /// Appropriate values are 0 or 1 only!

            removeServerImpl(id, true);
            continue;
        }

        if (locked && (modelInfo.selectedState == Qt::Checked))
        {
            --searchInfo.systemInfoIterator->selectedServers;
            modelInfo.selectedState = Qt::Unchecked;

            selectionChanged = true;
        }

        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);  /// to update enabled flag state
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
    }

    if (selectionChanged)
        emit m_owner->selectionChanged();

}

void rtu::ServersSelectionModel::Impl::changeAccessMethod(
    const QUuid& id, HttpAccessMethod newHttpAccessMethod)
{
    ItemSearchInfo searchInfo;
    if (!findServer(id, &searchInfo))
        return;

    searchInfo.serverInfoIterator->serverInfo.writableBaseInfo().
        httpAccessMethod = newHttpAccessMethod;

    m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);  /// TODO: remove after testing
}

void rtu::ServersSelectionModel::Impl::removeServers(const IDsVector &removed)
{
    const int wasSelected = selectedCount();
    for(auto &id: removed)
    {
        removeServerImpl(id, true);
    }
    const int selectedNow = selectedCount();

    if (wasSelected && !selectedNow)
        emit m_owner->selectionChanged();

    emit m_owner->serversCountChanged();
}

void rtu::ServersSelectionModel::Impl::unknownAdded(const QString &address)
{
    /// If we found address in existing servers
    /// it means this server is known
    for (const SystemModelInfo &systemItem: m_systems)
    {
        for (const ServerModelInfo &serverItem: systemItem.servers)
        {
            const ServerInfo &serverInfo = serverItem.serverInfo;
            if (serverInfo.baseInfo().hostAddress == address)
            {
                return;
            }

            if (!serverInfo.hasExtraInfo())
                continue;

            for (const api::InterfaceInfo &itf: serverInfo.extraInfo().interfaces)
            {
                if (itf.ip == address)
                {
                    return;
                }
            }
        }
    }

    const auto &itInsert = std::lower_bound(m_unknownEntities.begin()
        , m_unknownEntities.end(), address);

    const int groupIndex = knownEntitiesCount();
    const int index = (itInsert - m_unknownEntities.begin());
    const int insertEndIndex = index + groupIndex + kUnknownGroupItemCapacity;
    const int insertStartIndex = (m_unknownEntities.empty()
        ? groupIndex : insertEndIndex);

    const auto insertGuard = m_changeHelper->insertRowsGuard(
        insertStartIndex, insertEndIndex);
    Q_UNUSED(insertGuard);

    m_unknownEntities.insert(itInsert, address);
}

void rtu::ServersSelectionModel::Impl::unknownRemoved(const QString &address)
{
    enum { kNotFoundIndex = -1 };
    const int index = m_unknownEntities.indexOf(address);

    if (index == kNotFoundIndex)
        return;

    const int unknownEntitiesStartIndex = knownEntitiesCount();
    const bool isLastItem = (m_unknownEntities.size() == 1);

    const int removeEndIndex = index + unknownEntitiesStartIndex + kUnknownGroupItemCapacity;
    const int removeStartIndex = (isLastItem
        ? unknownEntitiesStartIndex : removeEndIndex);

    const auto removeGuard = m_changeHelper->removeRowsGuard(
        removeStartIndex, removeEndIndex);
    Q_UNUSED(removeGuard);

    m_unknownEntities.removeAt(index);
}

void rtu::ServersSelectionModel::Impl::removeServerImpl(const QUuid &id
    , bool removeEmptySystem
    , bool forceRemove)
{
    ItemSearchInfo searchInfo;

    if (!findServer(id, &searchInfo))
        return;

    if (searchInfo.serverInfoIterator->locked && !forceRemove)
    {
        ++searchInfo.serverInfoIterator->removeRequestsCounter; /// if locked - remove it later, on unlocking operation
        return;
    }

    {
        const auto &modelActionGuard = m_changeHelper->removeRowsGuard(
            searchInfo.serverRowIndex, searchInfo.serverRowIndex);
        Q_UNUSED(modelActionGuard);

        if (searchInfo.serverInfoIterator->selectedState == Qt::Checked)
            --searchInfo.systemInfoIterator->selectedServers;
        if (searchInfo.serverInfoIterator->loginState == kServerLoggedInState)
        {
            --searchInfo.systemInfoIterator->loggedServers;
        }
        else if (searchInfo.serverInfoIterator->loginState == kServerUnauthorized)
        {
            --searchInfo.systemInfoIterator->unauthorizedServers;
        }

        searchInfo.systemInfoIterator->servers.erase(searchInfo.serverInfoIterator);
    }

    if (!searchInfo.systemInfoIterator->servers.empty())
    {
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
    }
    else if (removeEmptySystem)
    {
        const auto &modelActionGuard = m_changeHelper->removeRowsGuard(
            searchInfo.systemRowIndex, searchInfo.systemRowIndex);
        Q_UNUSED(modelActionGuard);

        m_systems.erase(searchInfo.systemInfoIterator);
    }
}

///

rtu::ServersSelectionModel::ServersSelectionModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_impl(new Impl(this, CREATE_MODEL_CHANGE_HELPER(this)))
{}

rtu::ServersSelectionModel::~ServersSelectionModel() {}

void rtu::ServersSelectionModel::setSelectedItems(const IDsVector &ids)
{
    m_impl->setSelectedItems(ids);
}

void rtu::ServersSelectionModel::changeItemSelectedState(int rowIndex)
{
    m_impl->changeItemSelectedState(rowIndex, false, true);
    emit dataChanged(index(0), index(m_impl->knownEntitiesCount() - 1));
}

void rtu::ServersSelectionModel::setItemSelected(int rowIndex)
{
    m_impl->changeItemSelectedState(rowIndex, true, true);
    emit dataChanged(index(0), index(m_impl->knownEntitiesCount() - 1));
}

void rtu::ServersSelectionModel::setAllItemSelected(bool selected)
{
    m_impl->setAllItemSelected(selected);
    emit dataChanged(index(0), index(m_impl->knownEntitiesCount() - 1));
}

void rtu::ServersSelectionModel::tryLoginWith(
    const QString &primarySystem
    , const QString &password
    , const rtu::Callback &callback)
{
    m_impl->tryLoginWith(primarySystem, password, callback);
}

void rtu::ServersSelectionModel::blinkForItem(int row)
{
    m_impl->blinkForItem(row);
}

void rtu::ServersSelectionModel::serverDiscovered(const BaseServerInfo &baseInfo)
{
    m_impl->serverDiscovered(baseInfo);
}

void rtu::ServersSelectionModel::addServer(const BaseServerInfo &baseInfo)
{
    ItemSearchInfo searchInfo;
    if (m_impl->findServer(baseInfo.id, &searchInfo))
    {
        Q_ASSERT_X(searchInfo.serverInfoIterator->locked
            , Q_FUNC_INFO, "Server should be locked in this situation");
        if (searchInfo.serverInfoIterator->locked)
            --searchInfo.serverInfoIterator->removeRequestsCounter;

        m_impl->changeServer(baseInfo);
        return;
    }

    m_impl->addServer(ServerInfo(baseInfo));
}

void rtu::ServersSelectionModel::changeServer(const BaseServerInfo &baseInfo)
{
    m_impl->changeServer(baseInfo, true);
}

void rtu::ServersSelectionModel::removeServers(const IDsVector &removed)
{
    m_impl->removeServers(removed);
}

void rtu::ServersSelectionModel::unknownAdded(const QString &address)
{
    m_impl->unknownAdded(address);
}

void rtu::ServersSelectionModel::unknownRemoved(const QString &address)
{
    m_impl->unknownRemoved(address);
}

void rtu::ServersSelectionModel::updateExtraInfo(const QUuid &id
    , const ExtraServerInfo &extraInfo
    , const QString &hostName)
{
    m_impl->updateExtraInfo(id, extraInfo, hostName);
}

void rtu::ServersSelectionModel::updateTimeDateInfo(const QUuid &id
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , qint64 timestampMs)
{
    ItemSearchInfo searchInfo;
    if (!m_impl->findServer(id, &searchInfo))
        return;

    m_impl->updateTimeDateInfo(searchInfo, utcDateTimeMs, timeZoneId, timestampMs);
}

void rtu::ServersSelectionModel::updateInterfacesInfo(const QUuid &id
    , const QString &host
    , const InterfaceInfoList &interfaces)
{
    ItemSearchInfo searchInfo;
    if (!m_impl->findServer(id, &searchInfo))
        return;

    m_impl->updateInterfacesInfo(searchInfo, interfaces, host);
}

void rtu::ServersSelectionModel::updateSystemNameInfo(const QUuid &id
    , const QString &systemName)
{
    m_impl->updateSystemNameInfo(id, systemName);
}

void rtu::ServersSelectionModel::updatePortInfo(const QUuid &id
    , int port)
{
    m_impl->updatePortInfo(id, port);
}

void rtu::ServersSelectionModel::updatePasswordInfo(const QUuid &id
    , const QString &password)
{
    ItemSearchInfo searchInfo;
    if (!m_impl->findServer(id, &searchInfo))
        return;

    m_impl->updatePasswordInfo(searchInfo, password);
}

void rtu::ServersSelectionModel::lockItems(const IDsVector &ids
    , const QString &reason)
{
    m_impl->setLockedState(ids, true, reason);
}

void rtu::ServersSelectionModel::unlockItems(const IDsVector &ids)
{
    m_impl->setLockedState(ids, false, QString());
}

void rtu::ServersSelectionModel::changeAccessMethod(
    const QUuid& id, HttpAccessMethod newHttpAccessMethod)
{
    m_impl->changeAccessMethod(id, newHttpAccessMethod);
}

int rtu::ServersSelectionModel::selectedCount() const
{
    return m_impl->selectedCount();
}

api::ServerInfoPtrContainer rtu::ServersSelectionModel::selectedServers()
{
    return m_impl->selectedServers();
}

int rtu::ServersSelectionModel::serversCount() const
{
    return m_impl->serversCount();
}

int rtu::ServersSelectionModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_impl->rowCount();
    return 0;
}

QVariant rtu::ServersSelectionModel::data(const QModelIndex &index
    , int role) const
{
    if (!index.isValid())
        return QVariant();

    return m_impl->data(index, role);
}

rtu::Roles rtu::ServersSelectionModel::roleNames() const
{
    return kRolesDescription;
}
