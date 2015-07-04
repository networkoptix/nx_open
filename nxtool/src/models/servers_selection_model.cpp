
#include "servers_selection_model.h"

#include <functional>

#include <QUuid>

#include <base/server_info.h>
#include <base/requests.h>
#include <helpers/login_manager.h>
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
        , bool updateSelection = true);
    
    void setAllItemSelected(bool selected);
    
    int selectedCount() const;
    
    rtu::ServerInfoList selectedServers();
    
    void tryLoginWith(const QString &password);
    
    ///
    
    bool findAndMarkSelected(const QUuid &id
        , ItemSearchInfo &searchInfo);

    rtu::ExtraServerInfo& getExtraInfo(ItemSearchInfo &searchInfo);

    void updateTimeDateInfo(const QUuid &id
        , qint64 utcDateTimeMs
        , const QByteArray &timeZoneId
        , qint64 timestampMs);
    
    void updateInterfacesInfo(const QUuid &id
        , const QString &host
        , const InterfaceInfoList &interfaces);
        
    void updatePasswordInfo(const QUuid &id
        , const QString &password);

    void updateSystemNameInfo(const QUuid &id
        , const QString &systemName);

    void updatePortInfo(const QUuid &id
        , int port);

    void addServer(const rtu::ServerInfo &info
        , Qt::CheckState selected);

    void changeServer(const rtu::BaseServerInfo &info);

    void removeServers(const rtu::IDsVector &removed);

private:
    SystemModelInfo *findSystemModelInfo(const QString &systemName
        , int &row);
    
    bool findServer(const QUuid &id
        , ItemSearchInfo &searchInfo);
    
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
    rtu::LoginManager *m_loginManager;
};

rtu::ServersSelectionModel::Impl::Impl(rtu::ServersSelectionModel *owner
    , rtu::ModelChangeHelper *changeHelper)
    : QObject(owner)
    , m_owner(owner)
    , m_changeHelper(changeHelper)
    , m_systems()
    , m_loginManager(new LoginManager(this))
{
    QObject::connect(m_loginManager, &LoginManager::loginOperationSuccessfull, this
        , [this](const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        ItemSearchInfo searchInfo;
        if (!findServer(id, searchInfo))
            return;

        ServerModelInfo &serverInfo = *searchInfo.serverInfoIterator;
        if (!serverInfo.serverInfo.hasExtraInfo())
        {
            ++searchInfo.systemInfoIterator->loggedServers;
            m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
        }

        serverInfo.serverInfo.setExtraInfo(extraInfo);
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
    });
    
    QObject::connect(m_loginManager, &LoginManager::loginOperationFailed, this
        , [this](const QUuid &id)
    {
        ItemSearchInfo searchInfo;
        if (!findServer(id, searchInfo))
            return;
        
        ServerModelInfo &serverInfo = *searchInfo.serverInfoIterator;
        if (serverInfo.serverInfo.hasExtraInfo())
        {
            --searchInfo.systemInfoIterator->loggedServers;
            m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
        }
        
        serverInfo.serverInfo.resetExtraInfo();
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
    });
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
            return info.baseInfo().name;
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
            return (!serverInfo.serverInfo.hasExtraInfo() || (info.extraInfo().interfaces.empty())
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
    , bool updateSelection)
{
    ItemSearchInfo searchInfo;
    if (!findItem(rowIndex, m_systems, searchInfo))
        return;
    
    if (searchInfo.serverInfoIterator != searchInfo.systemInfoIterator->servers.end())
    {
        if (!searchInfo.serverInfoIterator->serverInfo.hasExtraInfo())
        {
            int i = 0;
        }
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
                    changeItemSelectedState(index + serverIndex, false, false);
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

rtu::ServerInfoList rtu::ServersSelectionModel::Impl::selectedServers() 
{
    ServerInfoList result;
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
            
            m_loginManager->loginToServer(serverInfo.serverInfo.baseInfo(), password);
        }
    }
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

void rtu::ServersSelectionModel::Impl::updateTimeDateInfo(const QUuid &id
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , qint64 timestampMs)
{
    ItemSearchInfo searchInfo;
    if (!findAndMarkSelected(id, searchInfo))
        return;

    ExtraServerInfo &extra = getExtraInfo(searchInfo);
    extra.utcDateTimeMs = utcDateTimeMs;
    extra.timeZoneId = timeZoneId;
    extra.timestampMs = timestampMs;
}

void rtu::ServersSelectionModel::Impl::updateInterfacesInfo(const QUuid &id
    , const QString &host
    , const InterfaceInfoList &interfaces)
{
    ItemSearchInfo searchInfo;
    if (!findAndMarkSelected(id, searchInfo))
        return;

    ExtraServerInfo &extra = getExtraInfo(searchInfo);
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
            continue;
        }    
        *it = itf;
    }
}

void rtu::ServersSelectionModel::Impl::updatePasswordInfo(const QUuid &id
    , const QString &password)
{
    ItemSearchInfo searchInfo;
    if (!findAndMarkSelected(id, searchInfo))
        return;

    ExtraServerInfo &extra = getExtraInfo(searchInfo);
    const QString oldPassword = extra.password;
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
        std::for_each(m_systems.begin(), m_systems.end()
            , [&row](const SystemModelInfo &info){ row += kSystemItemCapacity + info.servers.size(); });
        m_systems.push_back(SystemModelInfo(systemName));
        systemModelInfo = &m_systems.last();
        exist = false;
    }
    
    systemModelInfo->servers.push_back(ServerModelInfo(selected, info));
    systemModelInfo->loggedServers += (info.hasExtraInfo() ? 1 : 0);

    if (selected != Qt::Unchecked)
    {
        ++systemModelInfo->selectedServers;
        m_changeHelper->dataChanged(row, row);
    }

    const int rowStart = (exist ? row + systemModelInfo->servers.size() : row);
    const int rowFinish = (row + systemModelInfo->servers.size());
    
    const auto &modelChangeAction = m_changeHelper->insertRowsGuard(rowStart, rowFinish);
    
    if (!info.hasExtraInfo())
        m_loginManager->loginToServer(info.baseInfo());
    
    emit m_owner->serversCountChanged();
}

void rtu::ServersSelectionModel::Impl::changeServer(const BaseServerInfo &baseInfo)
{
    ItemSearchInfo searchInfo;
    if (!findServer(baseInfo.id, searchInfo))
        return;
    
    ServerInfo foundServer = searchInfo.serverInfoIterator->serverInfo;
    if (foundServer.baseInfo().systemName != baseInfo.systemName)
    {
        const Qt::CheckState selected = searchInfo.serverInfoIterator->selectedState;

        int targetSystemRow = 0;
        const bool targetSystemExists = 
            (findSystemModelInfo(baseInfo.systemName, targetSystemRow) != nullptr);
        removeServerImpl(baseInfo.id, targetSystemExists);
        
        SystemModelInfo &system = *searchInfo.systemInfoIterator;
        const bool inplaceRename = !targetSystemExists && system.servers.empty();
        if (inplaceRename)
            system.name = baseInfo.systemName;
//            m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);

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
    m_impl->changeItemSelectedState(rowIndex, false);
    emit dataChanged(index(0), index(m_impl->rowCount() - 1));
}

void rtu::ServersSelectionModel::setItemSelected(int rowIndex)
{
    m_impl->changeItemSelectedState(rowIndex, true);
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

void rtu::ServersSelectionModel::addServer(const BaseServerInfo &baseInfo)
{
    m_impl->addServer(ServerInfo(baseInfo), Qt::Unchecked);
}

void rtu::ServersSelectionModel::changeServer(const BaseServerInfo &baseInfo)
{
    m_impl->changeServer(baseInfo);
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
    m_impl->updateTimeDateInfo(id, utcDateTimeMs, timeZoneId, timestampMs);
}

void rtu::ServersSelectionModel::updateInterfacesInfo(const QUuid &id
    , const QString &host                                                      
    , const InterfaceInfoList &interfaces)
{
    m_impl->updateInterfacesInfo(id, host, interfaces);
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
    m_impl->updatePasswordInfo(id, password);
}

int rtu::ServersSelectionModel::selectedCount() const
{
    return m_impl->selectedCount();
}

rtu::ServerInfoList rtu::ServersSelectionModel::selectedServers()
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
