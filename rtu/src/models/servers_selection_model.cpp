
#include "servers_selection_model.h"

#include <functional>

#include <QUuid>

#include <server_info.h>
#include <requests/requests.h>
#include <helpers/login_manager.h>
#include <helpers/servers_finder.h>
#include <helpers/model_change_helper.h>

namespace
{
    struct ServerModelInfo
    {
        Qt::CheckState selectedState;
        rtu::ServerInfo serverInfo;
        
        /*
        ServerModelInfo& operator=(ServerModelInfo other)
        {
            std::swap(*this, other);
            return *this;
        }
        */
    };
    
    typedef QVector<ServerModelInfo> ServerModelInfosVector;

    struct SystemModelInfo
    {
        QString name;
        
        int selectedServers;
        int loggedServers;
        ServerModelInfosVector servers;
    };

    typedef QVector<SystemModelInfo> SystemModelInfosVector;
    
    struct ServerSearchInfo
    {
        SystemModelInfosVector::iterator systemInfoIterator;
        ServerModelInfosVector::iterator serverInfoIterator;
        int systemRowIndex;
        int serverRowIndex;
        
        ServerSearchInfo()
            : systemInfoIterator()
            , serverInfoIterator()
            , systemRowIndex()
            , serverRowIndex()
        {}
        
        ServerSearchInfo(SystemModelInfosVector::iterator sysInfoIterator
            , ServerModelInfosVector::iterator srvInfoIterator
            , int sysRowIndex
            , int serverRowIndex)
            : systemInfoIterator(sysInfoIterator)
            , serverInfoIterator(srvInfoIterator)
            , systemRowIndex(sysRowIndex)
            , serverRowIndex(serverRowIndex)
        {}
    };
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
        , kLoggedToAllServersRoleId
        
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
        result.insert(kLoggedToAllServersRoleId, "loggedToAllServers");

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
    typedef std::function<void (rtu::ServerInfo *info)> ServerInfoActionType;
}

namespace
{
    template<typename SystemType, typename ServerType, typename SystemsContainer>
    SystemType *findItemImpl(int rowIndex
        , SystemsContainer &container
        , ServerType *&serverInfo)
    {
        int index = 0;
        const int systemsCount = container.size();
        for (int systemIndex = 0; systemIndex != systemsCount; ++systemIndex)
        {
            SystemType &systemInfo = container[systemIndex];
            if (index == rowIndex)
                return &systemInfo;
            
            enum { kSystemItemOffset = 1 };
            index += kSystemItemOffset;
            
            const int serversCount = systemInfo.servers.size();
            if ((index + serversCount) > rowIndex)
            {
                serverInfo = &systemInfo.servers[rowIndex - index];
                return &systemInfo;
            }
            index += serversCount;
        }
        return nullptr;
    }
}

class rtu::ServersSelectionModel::Impl : public QObject
{
public:
    Impl(QObject *parent
         , rtu::ModelChangeHelper *changeHelper
         , const SelectionChangedActionType &selectedServersChanged
         , const ServerInfoActionType &updateServerInfo);
    
    virtual ~Impl();
    
    int rowCount() const;
    
    QVariant data(const QModelIndex &index
        , int role) const;
    
    void changeItemSelectedState(int rowIndex
        , bool explicitSelection);
    
    int selectedCount() const;
    
    rtu::ServerInfoList selectedServers();
    
    void tryLoginWith(const QString &password);
    
   
    void selectionPasswordChanged(const QString &password);
    
private:
    SystemModelInfo *findItem(int rowIndex
        , ServerModelInfo *&serverInfo);
    
    const SystemModelInfo *findItem(int rowIndex
        , const ServerModelInfo *&serverInfo) const;
    
    SystemModelInfo *findSystemModelInfo(const QString &systemName
        , int &row);
    
    bool findServer(const QUuid &id
        , ServerSearchInfo &searchInfo);
    
    void setSystemSelectedStateImpl(SystemModelInfo &systemInfo
        , bool selected);
    
    void onAddedServer(const rtu::BaseServerInfo &info);
    
    void onChangedServer(const rtu::BaseServerInfo &info);
    
    void onRemovedServers(const rtu::IDsVector &removed);
    
private:
    /// return true if no selected servers
    bool removeServerImpl(const QUuid &id
        , bool removeEmptySystem);
    
private:
    rtu::ModelChangeHelper * const m_changeHelper;
    const SelectionChangedActionType m_selectedServersChanged;
    const ServerInfoActionType m_updateServerInfo;
    
    SystemModelInfosVector m_systems;
    ServersFinder::Holder m_serversFinder;
    rtu::LoginManager *m_loginManager;
};

rtu::ServersSelectionModel::Impl::Impl(QObject *parent
    , rtu::ModelChangeHelper *changeHelper
    , const SelectionChangedActionType &selectedServersChanged
    , const ServerInfoActionType &updateServerInfo)
    : QObject(parent)

    , m_changeHelper(changeHelper)
    , m_selectedServersChanged(selectedServersChanged)
    , m_updateServerInfo(updateServerInfo)
    
    , m_systems()
    , m_serversFinder(ServersFinder::create())
    , m_loginManager(new LoginManager(this))
{
    QObject::connect(m_serversFinder.data(), &ServersFinder::onAddedServer, this, &Impl::onAddedServer);
    QObject::connect(m_serversFinder.data(), &ServersFinder::onChangedServer, this, &Impl::onChangedServer);
    QObject::connect(m_serversFinder.data(), &ServersFinder::onRemovedServers, this, &Impl::onRemovedServers);
    
    QObject::connect(m_loginManager, &LoginManager::loginOperationSuccessfull, this
        , [this](const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        ServerSearchInfo searchInfo;
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
        ServerSearchInfo searchInfo;
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
    int count = 0;
    std::for_each(m_systems.begin(), m_systems.end(), [&count](const SystemModelInfo& systemInfo)
    {
        count += kSystemItemCapacity + systemInfo.servers.size();
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
    
    const ServerModelInfo *serverInfo = nullptr;
    const SystemModelInfo *systemInfo = findItem(rowIndex, serverInfo);
    
    if (!systemInfo)
        return QVariant();
    
    if (serverInfo)
    {
        /// Returns values for server model item
        
        const ServerInfo &info = serverInfo->serverInfo;
        switch(role)
        {
        case kIsSystemRoleId:
            return false;
        case kSelectedStateRoleId:
            return serverInfo->selectedState;
        case kSystemNameRoleId:
            return systemInfo->name;
        case kNameRoleId:
            return info.baseInfo().name;
        case kIdRoleId:
            return info.baseInfo().id;
        case kMacAddressRoleId:
            return info.baseInfo().id.toString();
        case kLogged:
            return info.hasExtraInfo();
        case kPortRoleId:
            return info.baseInfo().port;
        case kDefaultPassword:
            return (info.hasExtraInfo() && info.extraInfo().password == rtu::defaultAdminPassword());
            
        case kIpAddressRoleId: 
            return (!serverInfo->serverInfo.hasExtraInfo() || (info.extraInfo().interfaces.empty()) 
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
            return systemInfo->name;
        case kSelectedStateRoleId:
            return (systemInfo->servers.empty() || !systemInfo->selectedServers ? Qt::Unchecked : 
                (systemInfo->selectedServers == systemInfo->servers.size() ? Qt::Checked : Qt::PartiallyChecked));
            
        case kLoggedToAllServersRoleId:
            return (systemInfo->loggedServers == systemInfo->servers.size());
        case kSelectedServersCountRoleId:
            return systemInfo->selectedServers;
        case kServersCountRoleId:
            return systemInfo->servers.size();
        }
    }
    
    return QVariant();
}

SystemModelInfo *rtu::ServersSelectionModel::Impl::findItem(int rowIndex
    , ServerModelInfo *&serverInfo)
{
    return findItemImpl<SystemModelInfo>(rowIndex, m_systems, serverInfo);
}

const SystemModelInfo *rtu::ServersSelectionModel::Impl::findItem(int rowIndex
    , const ServerModelInfo *&serverInfo) const
{
    return findItemImpl<const SystemModelInfo>(rowIndex, m_systems, serverInfo);
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
    , bool explicitSelection)
{
    ServerModelInfo *serverInfo = nullptr;
    SystemModelInfo *systemInfo = findItem(rowIndex, serverInfo);
    
    if (!systemInfo)
        return;
    
    const bool explWasSelected = (explicitSelection ? systemInfo->selectedServers == 1 : true);
    const bool wasSelected = (serverInfo ? 
        (serverInfo->selectedState != Qt::Unchecked) && explWasSelected
        : systemInfo->selectedServers);
    if (!wasSelected)   /// going to be selected, unselect all other systems
    {
        for(auto &system: m_systems)
        {
            if ((&system != systemInfo) || explicitSelection)
                setSystemSelectedStateImpl(system, false);
        }
    }
    
    if (!serverInfo)
    {
        setSystemSelectedStateImpl(*systemInfo, explicitSelection || !wasSelected);
    }
    else
    {
        enum 
        {
            kIncOneSelectedValue = 1
            , kDecOneSelectedValue = -1
        };
           
        systemInfo->selectedServers += (wasSelected ? kDecOneSelectedValue : kIncOneSelectedValue);
        serverInfo->selectedState = (wasSelected ? Qt::Unchecked : Qt::Checked);
    }

    m_selectedServersChanged();
}

int rtu::ServersSelectionModel::Impl::selectedCount() const
{
    const SystemModelInfo *systemInfo = nullptr;
    for(const SystemModelInfo &info: m_systems)
    {
        if (info.selectedServers)
        {
            systemInfo = &info;
            break;
        }
    }
    if (!systemInfo)
        return 0;
    
    return systemInfo->selectedServers;
}

rtu::ServerInfoList rtu::ServersSelectionModel::Impl::selectedServers() 
{
    SystemModelInfo *systemInfo = nullptr;
    for(SystemModelInfo &info: m_systems)
    {
        if (info.selectedServers)
        {
            systemInfo = &info;
            break;
        }
    }
    if (!systemInfo)
        return ServerInfoList();
    
    ServerInfoList result;
    std::for_each(systemInfo->servers.begin(), systemInfo->servers.end(), [&result](ServerModelInfo &info)
    {
        if (info.selectedState != Qt::Unchecked)
            result.push_back(&info.serverInfo);
    });
    
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

void rtu::ServersSelectionModel::Impl::selectionPasswordChanged(const QString &password)
{
    SystemModelInfo *systemInfo = nullptr;
    for (SystemModelInfo &system: m_systems)
    {
        if (system.selectedServers)
        {
            systemInfo = &system;
            break;
        }
    }
    
    if (!systemInfo)
        return;
    
    for (ServerModelInfo &info: systemInfo->servers)
    {
        if (info.selectedState == Qt::Unchecked)
            continue;
        
        if (info.serverInfo.hasExtraInfo())
        {
            info.serverInfo.resetExtraInfo();
            --systemInfo->loggedServers;
        }
        m_loginManager->loginToServer(info.serverInfo.baseInfo(), password);
    }
}


void rtu::ServersSelectionModel::Impl::onAddedServer(const BaseServerInfo &baseInfo)
{
    const QString systemName = (baseInfo.flags & ServerFlag::kIsFactory 
        ? QString() : baseInfo.systemName);

    int row = 0;
    bool exist = true;
    SystemModelInfo *systemModelInfo = findSystemModelInfo(systemName, row);
    if (!systemModelInfo)
    {
        std::for_each(m_systems.begin(), m_systems.end()
            , [&row](const SystemModelInfo &info){ row += kSystemItemCapacity + info.servers.size(); });
        m_systems.push_back( { systemName, 0, 0, ServerModelInfosVector() } );
        systemModelInfo = &m_systems.last();
        exist = false;
    }
    
    systemModelInfo->servers.push_back(
        ServerModelInfo({ Qt::Unchecked, ServerInfo(baseInfo)}));   
    
    const int rowStart = (exist ? row + systemModelInfo->servers.size() : row);
    const int rowFinish = (row + systemModelInfo->servers.size());
    
    const auto &modelChangeAction = m_changeHelper->insertRowsGuard(rowStart, rowFinish);
    
    m_loginManager->loginToServer(baseInfo);
}

void rtu::ServersSelectionModel::Impl::onChangedServer(const BaseServerInfo &baseInfo)
{
    ServerSearchInfo searchInfo;
    if (!findServer(baseInfo.id, searchInfo))
        return;
    
    const ServerInfo &foundServer = searchInfo.serverInfoIterator->serverInfo;
    if (foundServer.baseInfo().systemName != baseInfo.systemName)
    {
        int targetSystemRow = 0;
        const bool targetSystemExists = 
            (findSystemModelInfo(baseInfo.systemName, targetSystemRow) != nullptr);
        removeServerImpl(baseInfo.id, targetSystemExists);
        
        SystemModelInfo &system = *searchInfo.systemInfoIterator;
        if (!targetSystemExists && system.servers.empty())
        {
            system.name = baseInfo.systemName;
            m_changeHelper->dataChanged(searchInfo.systemRowIndex, searchInfo.systemRowIndex);
        }
        onAddedServer(baseInfo);
    }
    else
    {
        /// just updates information
        searchInfo.serverInfoIterator->serverInfo.setBaseInfo(baseInfo);
        m_changeHelper->dataChanged(searchInfo.serverRowIndex, searchInfo.serverRowIndex);
        if (searchInfo.serverInfoIterator->selectedState != Qt::Unchecked)
            m_updateServerInfo(&searchInfo.serverInfoIterator->serverInfo);
    }
        
        
}

bool rtu::ServersSelectionModel::Impl::findServer(const QUuid &id
    , ServerSearchInfo &searchInfo)
{
    int rowIndex = 0;
    for(auto itSystem = m_systems.begin(); itSystem != m_systems.end(); ++itSystem)
    {
        for(auto itServer = itSystem->servers.begin(); itServer != itSystem->servers.end(); ++itServer)
        {
            if (itServer->serverInfo.baseInfo().id == id)
            {
                searchInfo = ServerSearchInfo(itSystem, itServer, rowIndex
                    , rowIndex + kSystemItemCapacity + (itServer - itSystem->servers.begin()));
                return true;
            }
        }
        rowIndex += kSystemItemCapacity + itSystem->servers.count();
    }
    return false;
}

void rtu::ServersSelectionModel::Impl::onRemovedServers(const IDsVector &removed)
{
    for(auto &id: removed)
    {
        removeServerImpl(id, true);
    }
}

bool rtu::ServersSelectionModel::Impl::removeServerImpl(const QUuid &id
    , bool removeEmptySystem)
{
    ServerSearchInfo searchInfo;
    
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
    , m_impl(new Impl(this, CREATE_MODEL_CHANGE_HELPER(this)
        , [this]() { emit selectionChanged(); }
        , [this](ServerInfo *info) { emit serverInfoChanged(info); }
    )) 
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

void rtu::ServersSelectionModel::tryLoginWith(const QString &password)
{
    m_impl->tryLoginWith(password);
}

void rtu::ServersSelectionModel::selectionPasswordChanged(const QString &password)
{
    m_impl->selectionPasswordChanged(password);
}

int rtu::ServersSelectionModel::selectedCount() const
{
    return m_impl->selectedCount();
}

rtu::ServerInfoList rtu::ServersSelectionModel::selectedServers()
{
    return m_impl->selectedServers();
}

int rtu::ServersSelectionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_impl->rowCount();
}

QVariant rtu::ServersSelectionModel::data(const QModelIndex &index
    , int role) const
{
    return m_impl->data(index, role);
}

rtu::Roles rtu::ServersSelectionModel::roleNames() const
{
    return kRolesDescription;
}
