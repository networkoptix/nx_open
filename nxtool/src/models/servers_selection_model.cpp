
#include "servers_selection_model.h"

#include <functional>

#include <QUuid>

#include <base/server_info.h>
#include <base/requests.h>
#include <helpers/server_info_manager.h>
#include <helpers/model_change_helper.h>

namespace
{
    struct ServerModelInfo
    {
        Qt::CheckState selectedState;
        rtu::ServerInfo serverInfo;
        
        ServerModelInfo();
        explicit ServerModelInfo(Qt::CheckState initSelectedState
            , const rtu::ServerInfo initServerInfo);
    };

    ServerModelInfo::ServerModelInfo()
        : selectedState()
        , serverInfo()
    {}

    ServerModelInfo::ServerModelInfo(Qt::CheckState initSelectedState
        , const rtu::ServerInfo initServerInfo)
        : selectedState(initSelectedState)
        , serverInfo(initServerInfo)
    {}
    
    typedef QVector<ServerModelInfo> ServerModelInfosVector;

    struct SystemModelInfo
    {
        QString name;
        
        int selectedServers;
        int loggedServers;
        ServerModelInfosVector servers;

        SystemModelInfo();

        explicit SystemModelInfo(const QString &initName);
    };

    SystemModelInfo::SystemModelInfo()
        : name()
        , selectedServers(0)
        , loggedServers(0)
        , servers()
    {}

    SystemModelInfo::SystemModelInfo(const QString &initName)
        : name(initName)
        , selectedServers(0)
        , loggedServers(0)
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
        SelectionUpdaterGuard(rtu::ServerInfoManager *serverInfoManager);

        ~SelectionUpdaterGuard();

        void addServerToUpdate(const rtu::ServerInfo *info);

    private:
        rtu::ServerInfoManager * const m_serverInfoManager;
        rtu::ServerInfoContainer m_selectedServers;
    };

    SelectionUpdaterGuard::SelectionUpdaterGuard(rtu::ServerInfoManager *serverInfoManager)
        : m_serverInfoManager(serverInfoManager)
        , m_selectedServers()
    {
    }

    SelectionUpdaterGuard::~SelectionUpdaterGuard()
    {
        if (!m_selectedServers.empty())
            m_serverInfoManager->updateServerInfos(m_selectedServers);
    }

    void SelectionUpdaterGuard::addServerToUpdate(const rtu::ServerInfo *info)
    {
        if (info && info->hasExtraInfo())
            m_selectedServers.push_back(*info);
    }
}

namespace
{
    enum { kSystemItemCapacity = 1 };

    const QString factorySystemName;    /// empty system name shows if it is MAC 
    
    enum 
    {
        kFirstCustomRoleId = Qt::UserRole + 1

        /// Common role ids
        , kIsSystemRoleId = kFirstCustomRoleId
        , kNameRoleId 
        , kSelectedStateRoleId
        
        /// System's specific roles
        , kSelectedServersCountRoleId
        , kServersCountRoleId
        , kLoggedState
        
        /// Server's specific roles
        , kIdRoleId
        , kSystemNameRoleId
        , kMacAddressRoleId
        , kIpAddressRoleId
        , kPortRoleId
        , kLogged
        , kDefaultPassword
        
        , kLastCustomRoleId
    };
    
    const rtu::Roles kRolesDescription = []() -> rtu::Roles
    {
        rtu::Roles result;
        result.insert(kIsSystemRoleId, "isSystem");
        result.insert(kNameRoleId, "name");
        result.insert(kSelectedStateRoleId, "selectedState");

        result.insert(kSelectedServersCountRoleId, "selectedServersCount");
        result.insert(kServersCountRoleId, "serversCount");
        result.insert(kLoggedState, "loggedState");

        result.insert(kIdRoleId, "id");
        result.insert(kSystemNameRoleId, "systemName");
        result.insert(kMacAddressRoleId, "macAddress");
        result.insert(kIpAddressRoleId, "ipAddress");
        result.insert(kPortRoleId, "port");
        result.insert(kLogged, "logged");
        result.insert(kDefaultPassword, "defaultPassword");
        return result;
    }();
    
    typedef std::function<void (int startIndex, int finishIndex)> MultiIndexActionType; 
    typedef std::function<void ()> UnindexedActionType;
    
    typedef std::function<void ()> SelectionChangedActionType;
    typedef std::function<void (const rtu::ServerInfo &info)> ServerInfoActionType;
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
    
    int serversCount() const;
    
    QVariant data(const QModelIndex &index
        , int role) const;
    
    void changeItemSelectedState(int rowIndex
        , bool explicitSelection
        , bool updateSelection = true
        , SelectionUpdaterGuard *guard = nullptr);
    
    void setAllItemSelected(bool selected);
    
    int selectedCount() const;
    
    rtu::ServerInfoPtrContainer selectedServers();
    
    void tryLoginWith(const QString &password);
    
    ///

    bool selectionOutdated();

    void setSelectionOutdated(bool outdated);

    ///
    
    bool findAndMarkSelected(const QUuid &id
        , ItemSearchInfo &searchInfo);

    rtu::ExtraServerInfo& getExtraInfo(ItemSearchInfo &searchInfo);

    void updateTimeDateInfo(ItemSearchInfo &searchInfo
        , qint64 utcDateTimeMs
        , const QByteArray &timeZoneId
        , qint64 timestampMs);
    
    void updateInterfacesInfo(ItemSearchInfo &searchInfo
        , const InterfaceInfoList &interfaces
        , const QString &host = QString());
        
    void updatePasswordInfo(ItemSearchInfo &searchInfo
        , const QString &password);

    void updateSystemNameInfo(const QUuid &id
        , const QString &systemName);

    void updatePortInfo(const QUuid &id
        , int port);

    void serverDiscovered(const BaseServerInfo &baseInfo);\

    void addServer(const rtu::ServerInfo &info
        , Qt::CheckState selected);

    void changeServer(const rtu::BaseServerInfo &info
        , bool outdate = false);

    void removeServers(const rtu::IDsVector &removed);

    void updateExtraInfo(const QUuid &id
        , const ExtraServerInfo &extraServerInfo);

    void updateExtraInfoFailed(const QUuid &id);

    bool findServer(const QUuid &id
        , ItemSearchInfo &searchInfo);

private:
    SystemModelInfo *findSystemModelInfo(const QString &systemName
        , int &row);
    
    void setSystemSelectedStateImpl(SystemModelInfo &systemInfo
        , bool selected);
    
    
private:
    /// return true if no selected servers
    bool removeServerImpl(const QUuid &id
        , bool removeEmptySystem);
    
private:
    rtu::ServersSelectionModel * const m_owner;
    rtu::ModelChangeHelper * const m_changeHelper;
    
    SystemModelInfosVector m_systems;
    rtu::ServerInfoManager *m_serverInfoManager;
    bool m_selectionOutdated;
};

rtu::ServersSelectionModel::Impl::Impl(rtu::ServersSelectionModel *owner
    , rtu::ModelChangeHelper *changeHelper)
    : QObject(owner)
    , m_owner(owner)
    , m_changeHelper(changeHelper)
    , m_systems()
    , m_serverInfoManager(new ServerInfoManager(this))
    , m_selectionOutdated(false)
{
    QObject::connect(m_owner, &ServersSelectionModel::selectionChanged, this, [this]()
    {
        setSelectionOutdated(false);
    });

    QObject::connect(m_serverInfoManager, &ServerInfoManager::serverExtraInfoUpdated
        , this, &Impl::updateExtraInfo);
    QObject::connect(m_serverInfoManager, &ServerInfoManager::serverExtraInfoUpdateFailed
        , this, &Impl::updateExtraInfoFailed);

    QObject::connect(m_serverInfoManager, &ServerInfoManager::loginOperationSuccessfull
        , this, &Impl::updateExtraInfo);
    QObject::connect(m_serverInfoManager, &ServerInfoManager::loginOperationFailed
        , this, &Impl::updateExtraInfoFailed);
}

rtu::ServersSelectionModel::Impl::~Impl() {}

int rtu::ServersSelectionModel::Impl::rowCount() const
{
    return serversCount() + m_systems.count();
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


QVariant rtu::ServersSelectionModel::Impl::data(const QModelIndex &index
    , int role) const
{
    const int rowIndex = index.row();
    if ((role < kFirstCustomRoleId) || (role > kLastCustomRoleId)
        || (rowIndex < 0) || (rowIndex >= rowCount()))
        return QVariant();
    
    ItemSearchInfoConst searchInfo;
    if (!findItem(rowIndex, m_systems, searchInfo))
        return QVariant();


    const SystemModelInfo &systemInfo = *searchInfo.systemInfoIterator;
    if (searchInfo.serverInfoIterator != searchInfo.systemInfoIterator->servers.end())
    {
        /// Returns values for server model item
        
        const ServerModelInfo &serverInfo = *searchInfo.serverInfoIterator;
        const ServerInfo &info = searchInfo.serverInfoIterator->serverInfo;
        switch(role)
        {
        case kIsSystemRoleId:
            return false;
        case kSelectedStateRoleId:
            return serverInfo.selectedState;
        case kSystemNameRoleId:
            return systemInfo.name;
        case kNameRoleId:
            //TODO: #gdm #nx2 make correct implementation later on, when we will adopt the tool to the nx2 
            //with several network interfaces.
            return QString("%1 (%2)").arg(info.baseInfo().name).arg(info.baseInfo().hostAddress);
        case kIdRoleId:
            return info.baseInfo().id;
        case kMacAddressRoleId:
            return (info.hasExtraInfo() && !info.extraInfo().interfaces.empty() ? 
                info.extraInfo().interfaces.front().macAddress : "");//QString("--:--:--:--:--:--"));
        case kLogged:
            return info.hasExtraInfo();
        case kPortRoleId:
            return info.baseInfo().port;
        case kDefaultPassword:
            return (info.hasExtraInfo() && rtu::defaultAdminPasswords().contains(info.extraInfo().password));
        case kIpAddressRoleId: 
            return (!info.hasExtraInfo() || (info.extraInfo().interfaces.empty())
                ? QVariant() : QVariant::fromValue(info.extraInfo().interfaces));
        }
    }
    else                    
    {
        /// Returns values for system model item
        switch(role)
        {
        case kIsSystemRoleId:
            return true;
        case kNameRoleId:
        case kSystemNameRoleId:
            return systemInfo.name;
        case kSelectedStateRoleId:
            return (systemInfo.servers.empty() || !systemInfo.selectedServers ? Qt::Unchecked :
                (systemInfo.selectedServers == systemInfo.servers.size() ? Qt::Checked : Qt::PartiallyChecked));
        case kLoggedState:
            return (!systemInfo.loggedServers ? Constants::NotLogged 
                : (systemInfo.loggedServers == systemInfo.servers.size() ? Constants::LoggedToAllServers
                : Constants::PartiallyLogged));
        case kSelectedServersCountRoleId:
            return systemInfo.selectedServers;
        case kServersCountRoleId:
            return systemInfo.servers.size();
        }
    }
    
    return QVariant();
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
        if (server.serverInfo.hasExtraInfo())
        {
            server.selectedState = newState;
            ++selectedCount;
        }
    }
    systemInfo.selectedServers = (selected ? selectedCount : 0);
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
        localGuard.reset(new SelectionUpdaterGuard(m_serverInfoManager));
        guard = localGuard.data();
    }

    if (searchInfo.serverInfoIterator != searchInfo.systemInfoIterator->servers.end())
    {
        searchInfo.serverInfoIterator->serverInfo.hasExtraInfo();
        const ServerModelInfo &serverInfo = *searchInfo.serverInfoIterator;
        const bool wasSelected = (serverInfo.selectedState != Qt::Unchecked);

        const int prevSelectedCount = selectedCount();
        if (explicitSelection)
        {
            for(auto &system: m_systems)
            {
                setSystemSelectedStateImpl(system, false);
            }
            m_changeHelper->dataChanged(0, rowCount() - 1);
        }

        const bool selected = (!wasSelected || (explicitSelection && (prevSelectedCount > 1)));
        searchInfo.serverInfoIterator->selectedState =
            (selected ? Qt::Checked : Qt::Unchecked);

        if (selected)
        {
            ++searchInfo.systemInfoIterator->selectedServers;
            guard->addServerToUpdate(&searchInfo.serverInfoIterator->serverInfo);
        }
        else if (!explicitSelection)
        {
            --searchInfo.systemInfoIterator->selectedServers;
        }

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
            , searchInfo.systemInfoIterator->servers.size() + 1);
    }

    if (updateSelection)
        emit m_owner->selectionChanged();
}

void rtu::ServersSelectionModel::Impl::setAllItemSelected(bool selected)
{
    bool selectionChanged = false;
    int index = 0;

    SelectionUpdaterGuard guard(m_serverInfoManager);
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

rtu::ServerInfoPtrContainer rtu::ServersSelectionModel::Impl::selectedServers() 
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

void rtu::ServersSelectionModel::Impl::tryLoginWith(const QString &password)
{
    for (const SystemModelInfo &systemInfo: m_systems)
    {
        if (systemInfo.loggedServers == systemInfo.servers.size())
            continue;
        
        for(const ServerModelInfo &serverInfo: systemInfo.servers)
        {
            if (serverInfo.serverInfo.hasExtraInfo())
                continue;
            
            m_serverInfoManager->loginToServer(serverInfo.serverInfo.baseInfo(), password);
        }
    }
}

bool rtu::ServersSelectionModel::Impl::selectionOutdated()
{
    return m_selectionOutdated;
}

void rtu::ServersSelectionModel::Impl::setSelectionOutdated(bool outdated)
{
    m_selectionOutdated = outdated;
    emit m_owner->selectionOutdatedChanged();
}

bool rtu::ServersSelectionModel::Impl::findAndMarkSelected(const QUuid &id
    , ItemSearchInfo &searchInfo)
{
    if (!findServer(id, searchInfo))
        return false;

    ServerModelInfo &serverModelInfo = *searchInfo.serverInfoIterator;
    if (serverModelInfo.selectedState != Qt::Checked)
    {
        serverModelInfo.selectedState = Qt::Checked;
        ++searchInfo.systemInfoIterator->selectedServers;
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
    }
    return true;
}

rtu::ExtraServerInfo& rtu::ServersSelectionModel::Impl::getExtraInfo(ItemSearchInfo &searchInfo)
{
    ServerInfo &info = searchInfo.serverInfoIterator->serverInfo;
    if (!info.hasExtraInfo())
    {
        info.setExtraInfo(ExtraServerInfo());
        ++searchInfo.systemInfoIterator->loggedServers;
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
    }

    return info.writableExtraInfo();
}

void rtu::ServersSelectionModel::Impl::updateExtraInfo(const QUuid &id
    , const ExtraServerInfo &extraInfo)
{
    /// server is discovered and logged in

    ItemSearchInfo searchInfo;
    if (!findServer(id, searchInfo))
        return;

    updatePasswordInfo(searchInfo, extraInfo.password);
    updateTimeDateInfo(searchInfo, extraInfo.utcDateTimeMs
        , extraInfo.timeZoneId, extraInfo.timestampMs);
    updateInterfacesInfo(searchInfo, extraInfo.interfaces);
}

void rtu::ServersSelectionModel::Impl::updateExtraInfoFailed(const QUuid &id)
{
    /// Server is not available or can't be logged in
    ItemSearchInfo searchInfo;
    if (!findServer(id, searchInfo))
        return;

    if (searchInfo.serverInfoIterator->serverInfo.hasExtraInfo())
    {
        --searchInfo.systemInfoIterator->loggedServers;
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
    }

    searchInfo.serverInfoIterator->serverInfo.resetExtraInfo();
    if (searchInfo.serverInfoIterator->selectedState == Qt::Checked)
    {
        searchInfo.serverInfoIterator->selectedState = Qt::Unchecked;
        --searchInfo.systemInfoIterator->selectedServers;
        m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);

        if (!selectedCount())
            emit m_owner->selectionChanged();
    }

    m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
}

void rtu::ServersSelectionModel::Impl::updateTimeDateInfo(
    ItemSearchInfo &searchInfo
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , qint64 timestampMs)
{
    ExtraServerInfo &extra = getExtraInfo(searchInfo);

    enum { kMaxMsecDiff = 5000 };
    const auto timeDiffMs = std::abs(utcDateTimeMs - (extra.utcDateTimeMs + (timestampMs - extra.timestampMs)));
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
        setSelectionOutdated(true);
}

void rtu::ServersSelectionModel::Impl::updateInterfacesInfo(
    ItemSearchInfo &searchInfo
    , const InterfaceInfoList &interfaces
    , const QString &host)
{
    if (!host.isEmpty())
    {
        searchInfo.serverInfoIterator->serverInfo.writableBaseInfo().hostAddress = host;
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
    }

    ExtraServerInfo &extra = getExtraInfo(searchInfo);

    bool hasChanges = (extra.interfaces.size() != interfaces.size());
    for (const InterfaceInfo &itf: interfaces)
    {
        InterfaceInfoList &oldItf = extra.interfaces;
        const auto &it = std::find_if(oldItf.begin(), oldItf.end()
            , [itf](const InterfaceInfo &param)
        {
            return (param.name == itf.name);
        });
        
        if (it == oldItf.end())
        {
            oldItf.push_back(itf);
            hasChanges = true;
            continue;
        }    
        if (*it != itf)
            hasChanges = true;

        *it = itf;
    }

    if ((searchInfo.serverInfoIterator->selectedState == Qt::Checked) && hasChanges)
        setSelectionOutdated(true);
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
    {
        setSelectionOutdated(true);
    }
}

void rtu::ServersSelectionModel::Impl::updateSystemNameInfo(const QUuid &id
    , const QString &systemName)
{
    ItemSearchInfo searchInfo;
    if (!findServer(id, searchInfo))
        return;
    
    BaseServerInfo base =
        searchInfo.serverInfoIterator->serverInfo.baseInfo();
    base.systemName = systemName;
    base.flags &= ~Constants::IsFactoryFlag;
    changeServer(base);
}

void rtu::ServersSelectionModel::Impl::updatePortInfo(const QUuid &id
    , int port)
{
    ItemSearchInfo searchInfo;
    if (!findServer(id, searchInfo))
        return;
    
    BaseServerInfo base =
        searchInfo.serverInfoIterator->serverInfo.baseInfo();
    base.port = port;
    changeServer(base);
}

void rtu::ServersSelectionModel::Impl::serverDiscovered(const BaseServerInfo &baseInfo)
{
    ItemSearchInfo searchInfo;
    if (!findServer(baseInfo.id, searchInfo))
        return;

    const ServerInfo &foundInfo = searchInfo.serverInfoIterator->serverInfo;
    ServerInfo tmp = (!foundInfo.hasExtraInfo() ? ServerInfo(baseInfo)
        : ServerInfo(baseInfo, foundInfo.extraInfo()));

    m_serverInfoManager->updateServerInfos(ServerInfoContainer(1, tmp));
}

void rtu::ServersSelectionModel::Impl::addServer(const ServerInfo &info
    , Qt::CheckState selected)
{
    const QString systemName = (info.baseInfo().flags & Constants::IsFactoryFlag
        ? QString() : info.baseInfo().systemName);

    int row = 0;
    bool exist = true;
    SystemModelInfo *systemModelInfo = findSystemModelInfo(systemName, row);
    if (!systemModelInfo)
    {

        SystemModelInfo systemInfo(systemName);
        SystemModelInfosVector::iterator place = std::lower_bound(m_systems.begin(), m_systems.end(), systemInfo,
            [](const SystemModelInfo &left, const SystemModelInfo &right) { 
                return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0; });

        m_systems.insert(place, systemInfo);
        systemModelInfo = findSystemModelInfo(systemName, row);
        exist = false;
    }

    systemModelInfo->servers.push_back(ServerModelInfo(selected, info));
    systemModelInfo->loggedServers += (info.hasExtraInfo() ? 1 : 0);

    if (selected != Qt::Unchecked)
        ++systemModelInfo->selectedServers;

    const int rowStart = (exist ? row + systemModelInfo->servers.size() : row);
    const int rowFinish = (row + systemModelInfo->servers.size());
    
    const auto &modelChangeAction = m_changeHelper->insertRowsGuard(rowStart, rowFinish);
    
    if (!info.hasExtraInfo())
        m_serverInfoManager->loginToServer(info.baseInfo());

    if (exist)
        m_changeHelper->dataChanged(row, row);

    emit m_owner->serversCountChanged();
}

void rtu::ServersSelectionModel::Impl::changeServer(const BaseServerInfo &baseInfo
    , bool outdate)
{
    ItemSearchInfo searchInfo;
    if (!findServer(baseInfo.id, searchInfo))
        return;
    
    ServerInfo foundServer = searchInfo.serverInfoIterator->serverInfo;
    if ((searchInfo.serverInfoIterator->selectedState == Qt::Checked)
        && outdate && (foundServer.baseInfo() != baseInfo))
    {
        setSelectionOutdated(true);
    }

    if (foundServer.baseInfo().systemName != baseInfo.systemName)
    {
        const Qt::CheckState selected = searchInfo.serverInfoIterator->selectedState;

        int targetSystemRow = 0;
        const bool targetSystemExists = 
            (findSystemModelInfo(baseInfo.systemName, targetSystemRow) != nullptr);

        SystemModelInfo &system = *searchInfo.systemInfoIterator;
        const bool inplaceRename = !targetSystemExists && (system.servers.size() == 1);
        if (inplaceRename)
        {
            system.name = baseInfo.systemName;
            m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
        }

        removeServerImpl(baseInfo.id, targetSystemExists);
        foundServer.setBaseInfo(baseInfo);
        addServer(foundServer, selected);
    }
    else
    {
        /// just updates information
        searchInfo.serverInfoIterator->serverInfo.setBaseInfo(baseInfo);
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
    }
}

bool rtu::ServersSelectionModel::Impl::findServer(const QUuid &id
    , ItemSearchInfo &searchInfo)
{
    int rowIndex = 0;
    for(auto itSystem = m_systems.begin(); itSystem != m_systems.end(); ++itSystem)
    {
        for(auto itServer = itSystem->servers.begin(); itServer != itSystem->servers.end(); ++itServer)
        {
            if (itServer->serverInfo.baseInfo().id == id)
            {
                searchInfo = ItemSearchInfo(itSystem, itServer, rowIndex
                    , rowIndex + kSystemItemCapacity + (itServer - itSystem->servers.begin()));
                return true;
            }
        }
        rowIndex += kSystemItemCapacity + itSystem->servers.count();
    }
    return false;
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

bool rtu::ServersSelectionModel::Impl::removeServerImpl(const QUuid &id
    , bool removeEmptySystem)
{
    ItemSearchInfo searchInfo;
    
    if (!findServer(id, searchInfo))
        return false;

    const int selCount = searchInfo.systemInfoIterator->selectedServers;    
    
    {
        const auto &modelActionGuard = m_changeHelper->removeRowsGuard(
            searchInfo.serverRowIndex, searchInfo.serverRowIndex);
        Q_UNUSED(modelActionGuard);
        
        if (searchInfo.serverInfoIterator->selectedState == Qt::Checked)
            --searchInfo.systemInfoIterator->selectedServers;
        if (searchInfo.serverInfoIterator->serverInfo.hasExtraInfo())
            --searchInfo.systemInfoIterator->loggedServers;
    
        searchInfo.systemInfoIterator->servers.erase(searchInfo.serverInfoIterator);
    }

    const int newSelCount = searchInfo.systemInfoIterator->selectedServers;
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
    return (selCount && !newSelCount);
}

///

rtu::ServersSelectionModel::ServersSelectionModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_impl(new Impl(this, CREATE_MODEL_CHANGE_HELPER(this)))
{}

rtu::ServersSelectionModel::~ServersSelectionModel() {}

void rtu::ServersSelectionModel::changeItemSelectedState(int rowIndex)
{
    m_impl->changeItemSelectedState(rowIndex, false, true);
    emit dataChanged(index(0), index(m_impl->rowCount() - 1));
}

void rtu::ServersSelectionModel::setItemSelected(int rowIndex)
{
    m_impl->changeItemSelectedState(rowIndex, true, true);
    emit dataChanged(index(0), index(m_impl->rowCount() - 1));
}

void rtu::ServersSelectionModel::setAllItemSelected(bool selected)
{
    m_impl->setAllItemSelected(selected);
    emit dataChanged(index(0), index(m_impl->rowCount() - 1));
}

void rtu::ServersSelectionModel::tryLoginWith(const QString &password)
{
    m_impl->tryLoginWith(password);
}

void rtu::ServersSelectionModel::serverDiscovered(const BaseServerInfo &baseInfo)
{
    m_impl->serverDiscovered(baseInfo);
}

void rtu::ServersSelectionModel::addServer(const BaseServerInfo &baseInfo)
{
    m_impl->addServer(ServerInfo(baseInfo), Qt::Unchecked);
}

void rtu::ServersSelectionModel::changeServer(const BaseServerInfo &baseInfo)
{
    m_impl->changeServer(baseInfo, true);
}

void rtu::ServersSelectionModel::removeServers(const IDsVector &removed)
{
    m_impl->removeServers(removed);
}

void rtu::ServersSelectionModel::updateTimeDateInfo(const QUuid &id
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , qint64 timestampMs)
{
    ItemSearchInfo searchInfo;
    if (!m_impl->findAndMarkSelected(id, searchInfo))
        return;

    m_impl->updateTimeDateInfo(searchInfo, utcDateTimeMs, timeZoneId, timestampMs);
}

void rtu::ServersSelectionModel::updateInterfacesInfo(const QUuid &id
    , const QString &host                                                      
    , const InterfaceInfoList &interfaces)
{
    ItemSearchInfo searchInfo;
    if (!m_impl->findAndMarkSelected(id, searchInfo))
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
    if (!m_impl->findAndMarkSelected(id, searchInfo))
        return;

    m_impl->updatePasswordInfo(searchInfo, password);
}

bool rtu::ServersSelectionModel::selectionOutdated() const
{
    return m_impl->selectionOutdated();
}

int rtu::ServersSelectionModel::selectedCount() const
{
    return m_impl->selectedCount();
}

rtu::ServerInfoPtrContainer rtu::ServersSelectionModel::selectedServers()
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
