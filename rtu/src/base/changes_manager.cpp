
#include "changes_manager.h"

#include <QString>

#include <base/requests.h>
#include <base/server_info.h>
#include <base/rtu_context.h>
#include <helpers/time_helper.h>
#include <models/servers_selection_model.h>
#include <models/changes_summary_model.h>

namespace
{
    enum { kInvalidPort = 0 };

    struct IpChangeRequest
    {
        bool inProgress;
        int tries;
        qint64 timestamp;
        rtu::ServerInfo info;
        rtu::InterfaceInfoList changes;

        IpChangeRequest(const rtu::ServerInfo &info
            , const rtu::InterfaceInfoList &changes);
    };

    IpChangeRequest::IpChangeRequest(const rtu::ServerInfo &initInfo
        , const rtu::InterfaceInfoList &initChanges)
        : inProgress(false)
        , tries(0)
        , timestamp(QDateTime::currentMSecsSinceEpoch())
        , info(initInfo)
        , changes(initChanges)
    {
    }

}

class rtu::ChangesManager::Impl : public QObject
{
public:
    Impl(rtu::ChangesManager *owner
        , rtu::RtuContext *context
        , rtu::HttpClient *httpClient
        , rtu::ServersSelectionModel *selectionModel);
    
    virtual ~Impl();
    
    void addSystemChangeRequest(const QString &newSystemName);
    
    void addPasswordChangeRequest(const QString &password);
    
    void addPortChangeRequest(int port);
    
    void addIpChangeRequest(const QString &name
        , bool useDHCP
        , const QString &address
        , const QString &subnetMask);
    
    void addDateTimeChange(const QDate &date
        , const QTime &time
        , const QByteArray &timeZoneId);
    
    void clearChanges();

    void applyChanges();
    
    void onChangesetApplied(int changesCount);
    
    ///
    
    int totalChangesCount() const;
    
    void setTotalChangesCount(int count);
    
    ///
    
    int appliedChangesCount() const;
    
    void setAppliedChangesCount(int count);
    
    ///
    
    int errorsCount() const;
    
    void setErrorsCount(int count);
    
    void onServerPing(const rtu::BaseServerInfo &info);

    ///
    rtu::ChangesSummaryModel *successfulResultsModel();
    
    rtu::ChangesSummaryModel *failedResultsModel();

    void onAppliedIpChanges(const rtu::ServerInfo &info
        , const rtu::InterfaceInfoList &extraInfo
        , const QString &kErrorDesc
        , rtu::AffectedEntities affected);

    
private:    
    void addRequestResult(const ServerInfo &info
        , const QString &value
        , const QString &valueName
        , rtu::AffectedEntities entityFlag
        , rtu::AffectedEntities affectedEntities
        , const QString &errorReason);
    
    rtu::OperationCallback makeAddressOpCallback(const ServerInfo &info
        , const rtu::InterfaceInfoList &changes);
    
    rtu::OperationCallback makeSystemOpCallback(const ServerInfo &info, int selectedCount);
    
    void applyDateTimeChanges(const rtu::ServerInfoList &servers);
    
    void applyIpChange(const ServerInfo &info, int selectedCount);

public:
    typedef QHash<QUuid, IpChangeRequest> ChangesContainer;

    rtu::ChangesManager * const m_owner;
    rtu::RtuContext * const m_context;
    rtu::HttpClient * const m_httpClient;
    rtu::ServersSelectionModel * const m_selectionModel;
    
    QString m_newSystemName;
    QString m_newPassword;
    int m_newPort;
    
    rtu::InterfaceInfoList m_addressChanges;
    ChangesContainer m_ipChanges;

    QDateTime m_newUtcDateTime;
    QTimeZone m_newTimeZone;

    int m_totalChanges;
    int m_appliedChanges;
    int m_errorCount;
    
    ChangesSummaryModel *m_sucessfulChangesSummary;
    ChangesSummaryModel *m_failedChangesSummary;
};

rtu::ChangesManager::Impl::Impl(
    ChangesManager *owner
    , rtu::RtuContext *context
    , rtu::HttpClient *httpClient
    , rtu::ServersSelectionModel *selectionModel)
    : QObject(owner)
    , m_owner(owner)
    , m_context(context)
    , m_httpClient(httpClient)
    , m_selectionModel(selectionModel)
    , m_newSystemName()
    , m_newPassword()
    , m_newPort(kInvalidPort)
   
    , m_addressChanges()
    , m_ipChanges()
    
    , m_newUtcDateTime()
    , m_newTimeZone()
    
    , m_totalChanges(0)
    , m_appliedChanges(0)
    , m_errorCount(0)
    
    , m_sucessfulChangesSummary(nullptr)
    , m_failedChangesSummary(nullptr)
{
    
}

rtu::ChangesManager::Impl::~Impl()
{
    
}

void rtu::ChangesManager::Impl::addSystemChangeRequest(const QString &newSystemName)
{
    m_newSystemName = newSystemName;
}

void rtu::ChangesManager::Impl::addPasswordChangeRequest(const QString &password)
{
    m_newPassword = password;
}

void rtu::ChangesManager::Impl::addPortChangeRequest(int port)
{
    m_newPort = port;
}

void rtu::ChangesManager::Impl::addIpChangeRequest(const QString &name
    , bool useDHCP
    , const QString &address
    , const QString &subnetMask)
{
    const InterfaceInfo newInfo = InterfaceInfo(name, address, "", subnetMask, ""
        , (useDHCP ? Qt::Checked : Qt::Unchecked));
    const auto &it = std::find_if(m_addressChanges.begin(), m_addressChanges.end()
        , [&newInfo](const InterfaceInfo &addrInfo) { return (addrInfo.name == newInfo.name); });
    
    if (it == m_addressChanges.end())
    {
        m_addressChanges.push_back(newInfo);
        return;
    }
    
    *it = newInfo;
}

void rtu::ChangesManager::Impl::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    m_newTimeZone = QTimeZone(timeZoneId);
    m_newUtcDateTime = utcFromTimeZone(date, time, m_newTimeZone);
}

void rtu::ChangesManager::Impl::clearChanges()
{
    m_newSystemName.clear();
    m_newPassword.clear();
    m_newPort = kInvalidPort;
    
    m_addressChanges.clear();
    m_ipChanges.clear();

    m_newUtcDateTime = QDateTime();
    m_newTimeZone = QTimeZone();
    
    setAppliedChangesCount(0);
    setTotalChangesCount(0);
    setErrorsCount(0);
}

void rtu::ChangesManager::Impl::applyChanges()
{
    const ServerInfoList &selectedServers = m_selectionModel->selectedServers();
    
    const int systemNameChangesCount = (m_newSystemName.isEmpty() ? 0 : selectedServers.size());
    const int passwordChangesCount = (m_newPassword.isEmpty() ? 0 : selectedServers.size());
    const int portChangesCount = (!m_newPort ? 0 : selectedServers.size());
    const int systemChangesCount = systemNameChangesCount + passwordChangesCount + portChangesCount;
    
    setAppliedChangesCount(0);
    setTotalChangesCount(systemChangesCount);
    setErrorsCount(0);
    
    m_sucessfulChangesSummary->deleteLater();
    m_sucessfulChangesSummary = (new ChangesSummaryModel(true, this));

    m_failedChangesSummary->deleteLater();
    m_failedChangesSummary =(new ChangesSummaryModel(false, this));
            
    m_context->setCurrentPage(Constants::ProgressPage);
    
    applyDateTimeChanges(selectedServers);


    {
        const auto getIpChangesCount = [this, selectedServers]() -> int
        {
            if (m_addressChanges.empty())
                return 0;

            int changes = 0;
            for (ServerInfo *srv: selectedServers)
            {
                if (!srv->hasExtraInfo())
                    continue;
                changes += srv->extraInfo().interfaces.size();
            }
            return changes;
        };

        const int addressChanges = (selectedServers.size() == 1 ? m_addressChanges.size()
            : getIpChangesCount());

        setTotalChangesCount(totalChangesCount() + addressChanges);
    }

    if (systemChangesCount)
    {
        for(const ServerInfo *info: selectedServers)
        {
            rtu::configureRequest(m_httpClient, makeSystemOpCallback(*info, selectedServers.size())
                , *info, m_newSystemName, m_newPassword, m_newPort);
        }
    }
    else
    {
        for (ServerInfo *info: selectedServers)
        {
            applyIpChange(*info, selectedServers.size());
        }
    }
}

void rtu::ChangesManager::Impl::applyIpChange(const ServerInfo &info, int selectedCount)
{
    if (m_addressChanges.empty())
        return;

    if (selectedCount == 1)
    {
        /// change all requested ips by single request fo signle selected server
        rtu::changeIpsRequest(m_httpClient, info, m_addressChanges
            , makeAddressOpCallback(info, m_addressChanges));
    }
    else
    {
        if (!info.hasExtraInfo())
            return;

        const InterfaceInfo &addressChanges = *m_addressChanges.begin();
        InterfaceInfoList changes;

        for(const InterfaceInfo &addInfo: info.extraInfo().interfaces)
        {
            const InterfaceInfo interface = InterfaceInfo(addInfo.name, addressChanges.ip
                , "", addressChanges.mask, "", addressChanges.useDHCP);
            changes.push_back(interface);
        }

        rtu::changeIpsRequest(m_httpClient, info, changes
            , makeAddressOpCallback(info, changes));
    }
}

/*
void rtu::ChangesManager::Impl::applyIpChanges(const rtu::ServerInfoList &servers)
{
    if (m_addressChanges.empty())
        return;

    if (servers.size() == 1)
    {
        const ServerInfo &serverInfo = **servers.begin();
        /// change all requested ips by single request fo signle selected server
        rtu::changeIpsRequest(m_httpClient, serverInfo, m_addressChanges
            , makeAddressOpCallback(serverInfo, m_addressChanges));
    }
    else if (m_addressChanges.size() == 1)
    {
        const InterfaceInfo &addressChanges = *m_addressChanges.begin();
        for(const ServerInfo *info: servers)
        {
            InterfaceInfoList changes;
            for(const InterfaceInfo &addInfo: info->extraInfo().interfaces)
            {
                const InterfaceInfo interface = InterfaceInfo(addInfo.name, addressChanges.ip
                    , "", addressChanges.mask, "", addressChanges.useDHCP);
                changes.push_back(interface);
            }

            rtu::changeIpsRequest(m_httpClient, *info, changes
                , makeAddressOpCallback(*info, changes));
        }
    }
    else
    {
    }
}
*/

void rtu::ChangesManager::Impl::onChangesetApplied(int changesCount)
{
    setAppliedChangesCount(appliedChangesCount() + changesCount);
    if (appliedChangesCount() != totalChangesCount())
        return;

    m_context->setCurrentPage(Constants::SummaryPage);

    emit m_selectionModel->selectionChanged();
}

int rtu::ChangesManager::Impl::totalChangesCount() const
{
    return m_totalChanges;
}

void rtu::ChangesManager::Impl::setTotalChangesCount(int count)
{
    if (m_totalChanges == count)
        return;
   
    m_totalChanges = count;
    emit m_owner->totalChangesCountChanged();
}


int rtu::ChangesManager::Impl::appliedChangesCount() const
{
    return m_appliedChanges;
}

void rtu::ChangesManager::Impl::setAppliedChangesCount(int count)
{
    if (m_appliedChanges == count)
        return;
   
    m_appliedChanges = count;
    emit m_owner->appliedChangesCountChanged();
}


int rtu::ChangesManager::Impl::errorsCount() const
{
    return m_errorCount;
}

void rtu::ChangesManager::Impl::setErrorsCount(int count)
{
    if (m_errorCount == count)
        return;
   
    m_errorCount = count;
    emit m_owner->errorsCountChanged();
}

bool ipChangeApplied(const rtu::InterfaceInfoList &itf
    , const rtu::InterfaceInfoList &changes)
{
    for (const rtu::InterfaceInfo &change: changes)
    {
        const auto &it = std::find_if(itf.begin(), itf.end()
            , [change](const rtu::InterfaceInfo &info)
        {
            return (change.name == info.name);
        });

        if (it == itf.end())
            return false;

        const rtu::InterfaceInfo &info = *it;

        const bool applied = ((change.ip.isEmpty() || (change.ip == info.ip))
            && (change.mask.isEmpty() || (change.mask == info.mask))
            && (change.useDHCP == info.useDHCP));

        if (!applied)
            return false;
    }
    return true;
}

void rtu::ChangesManager::Impl::onServerPing(const BaseServerInfo &info)
{
    const auto it = m_ipChanges.find(info.id);
    if (it == m_ipChanges.end())
        return;

    IpChangeRequest &request = it.value();
    if (request.inProgress)
        return;

    const qint64 current = QDateTime::currentMSecsSinceEpoch();
    enum { kRequestsMinPeriod = 2 * 1000};    /// 5 seconds
    if ((current - request.timestamp) < kRequestsMinPeriod)
        return;

    enum { kMaxTriesCount = 10 };
    static const QString kErrorDesc = tr("request timeout");

    const auto processCallback =
        [this](bool sucessfull, const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        const auto it = m_ipChanges.find(id);
        if (it == m_ipChanges.end())
            return;

        IpChangeRequest &request = it.value();
        request.inProgress = false;
        request.timestamp = QDateTime::currentMSecsSinceEpoch();
        if (sucessfull && ipChangeApplied(extraInfo.interfaces, request.changes))
        {
            onAppliedIpChanges(request.info, extraInfo.interfaces, QString(), 0);
            m_ipChanges.erase(it);
        }
        else if (++request.tries > kMaxTriesCount)
        {
            onAppliedIpChanges(request.info, request.changes, kErrorDesc, kAllFlags);
            m_ipChanges.erase(it);
        }
    };

    const auto successful =
        [processCallback](const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        processCallback(true, id, extraInfo);
    };

    const auto failed = [processCallback](const QUuid &id)
    {
        processCallback(false, id, ExtraServerInfo());
    };

    request.inProgress = true;
    interfacesListRequest(m_httpClient, info, request.info.extraInfo().password, successful, failed);
}

rtu::ChangesSummaryModel *rtu::ChangesManager::Impl::successfulResultsModel()
{
    return m_sucessfulChangesSummary;
}

rtu::ChangesSummaryModel *rtu::ChangesManager::Impl::failedResultsModel()
{
    return m_failedChangesSummary;
}

void rtu::ChangesManager::Impl::onAppliedIpChanges(const rtu::ServerInfo &info
    , const rtu::InterfaceInfoList &changes
    , const QString &errorDesc
    , rtu::AffectedEntities affected)
{
    InterfaceInfoList newInterfaces = info.extraInfo().interfaces;

    const auto findInterface = [](InterfaceInfoList &list, const QString &name) -> InterfaceInfo *
    {
        for (InterfaceInfo &ifInfo: list)
        {
            if (ifInfo.name == name)
                return &ifInfo;
        }
        return nullptr;
    };

    const auto updateInterface = [findInterface](InterfaceInfoList &list, const InterfaceInfo &change)
    {
        InterfaceInfo * const itf = findInterface(list, change.name);
        if (!itf)
            return;

        itf->useDHCP = change.useDHCP;
        if (!change.ip.isEmpty())
            itf->ip = change.ip;
        if (!change.mask.isEmpty())
            itf->mask = change.mask;
    };

    for(const InterfaceInfo &change: changes)
    {
        static const QString kYes = tr("yes");
        static const QString kNo = tr("no");

        if (change.ip.isEmpty() && change.mask.isEmpty())
        {
            static const QString requestNameTemplate = tr("use dhcp (%1)");

            addRequestResult(info, (change.useDHCP ? kYes : kNo)
                , requestNameTemplate.arg(change.name), kDHCPUsage, affected, errorDesc);

        }
        else
        {
            static const QString requestNameTemplate = tr("ip / subnet mask / use dhcp (%1)");
            static const QString valueTemplate = "%1 / %2 / %3";

            const QString valueName = requestNameTemplate.arg(change.name);
            const QString value = valueTemplate.arg(change.ip)
                .arg(change.mask).arg(change.useDHCP ? kYes : kNo);

            addRequestResult(info, value, valueName
                , kIpAddress | kSubnetMask | kDHCPUsage, affected, errorDesc);
        }
        updateInterface(newInterfaces, change);
    }

    if (errorDesc.isEmpty())
        m_selectionModel->updateInterfacesInfo(info.baseInfo().id, newInterfaces);
}

rtu::OperationCallback rtu::ChangesManager::Impl::makeAddressOpCallback(const ServerInfo &info
    , const InterfaceInfoList &changes)
{

    const auto callback = [this, info, changes](const QString &errorReason, AffectedEntities affectedEntities)
    {
        const auto it = m_ipChanges.find(info.baseInfo().id);
        if (it == m_ipChanges.end())
            return;

        IpChangeRequest &request = *it;
        onAppliedIpChanges(info, request.changes, errorReason, affectedEntities);
        m_ipChanges.erase(it);
    };
    
    m_ipChanges.insert(info.baseInfo().id, IpChangeRequest(info, changes));

    return callback;
}

void rtu::ChangesManager::Impl::addRequestResult(const ServerInfo &info
    , const QString &value
    , const QString &valueName
    , rtu::AffectedEntities entityFlag
    , rtu::AffectedEntities affectedEntities
    , const QString &errorReason)
{
    if (value.isEmpty())
        return;
    
    if (affectedEntities & entityFlag)
    {
        m_failedChangesSummary->addRequestResult(info, valueName, value, errorReason);
        setErrorsCount(errorsCount() + 1);   
    }
    else
    {
        m_sucessfulChangesSummary->addRequestResult(info, valueName, value);
    }
    onChangesetApplied(1);
}

rtu::OperationCallback rtu::ChangesManager::Impl::makeSystemOpCallback(const ServerInfo &info
    , int selectedCount)
{
    const auto callback = 
        [this, info, selectedCount](const QString &errorReason, AffectedEntities affectedEntities)
    {
        static const QString kPasswordRequestDesc = tr("password");
        static const QString kSystemNameRequestDesc = tr("system name");
        static const QString kPortRequestDesc = tr("port");
        
        ServerInfo newInfo(info);

        if (!m_newSystemName.isEmpty())
        {
            m_selectionModel->updateSystemNameInfo(info.baseInfo().id, m_newSystemName);
            addRequestResult(info, m_newSystemName, kSystemNameRequestDesc
                , kSystemName, affectedEntities, errorReason);
        }

        if (!m_newPassword.isEmpty() && !(affectedEntities & kPassword))
        {
            if (!newInfo.hasExtraInfo())
                newInfo.setExtraInfo(ExtraServerInfo());
            newInfo.writableExtraInfo().password = m_newPassword;

            m_selectionModel->updatePasswordInfo(info.baseInfo().id, m_newPassword);
            addRequestResult(info, m_newPassword, kPasswordRequestDesc
                , kPassword, affectedEntities, errorReason);
        }

        

        if (m_newPort)
        {
            newInfo.writableBaseInfo().port = m_newPort;

            m_selectionModel->updatePortInfo(info.baseInfo().id, m_newPort);
            addRequestResult(info, (m_newPort ? QString::number(m_newPort) : QString())
                , kPortRequestDesc, kPort, affectedEntities, errorReason);
        }

        applyIpChange(newInfo, selectedCount);
    };
    
    return callback;
}

void rtu::ChangesManager::Impl::applyDateTimeChanges(const rtu::ServerInfoList &servers)
{
    if (m_newUtcDateTime.isNull() || !m_newUtcDateTime.isValid())
        return;
    
    const int changesCount = (servers.size() * 2); /// Date/Time + Time zone
    setTotalChangesCount(totalChangesCount() + changesCount);
    
    const QDateTime utcDateTime = m_newUtcDateTime;
    const QTimeZone timeZone = m_newTimeZone;

    for (const ServerInfo *info: servers)
    {
        static const QString &kDateTimeRequestDesc = tr("date/time");
        static const QString &kTimeZoneRequestDesc = tr("time zone");
        
        const ServerInfo &serverInfoRef = *info;
        const auto &successful = [serverInfoRef, this](const QUuid &serverId
            , const QDateTime &utcDateTime, const QTimeZone &timeZone, const QDateTime &timestamp)
        {
            m_selectionModel->updateTimeDateInfo(serverId, utcDateTime, timeZone, timestamp);

            const QDateTime pseudo = convertUtcToTimeZone(utcDateTime, timeZone);
            addRequestResult(serverInfoRef, pseudo.toString(), kDateTimeRequestDesc
                , kDateTime, kNoEntitiesAffected, QString());
            addRequestResult(serverInfoRef, timeZone.id()
                , kTimeZoneRequestDesc, kTimeZone, kNoEntitiesAffected, QString());
            
            /// todo
        };
        
        const auto &failed = [serverInfoRef, this, utcDateTime, timeZone]
            (const QString &errorReason, AffectedEntities affected)
        {  
            const QDateTime pseudo = convertUtcToTimeZone(utcDateTime, timeZone);
            addRequestResult(serverInfoRef, pseudo.toString(), kDateTimeRequestDesc
                , kDateTime, affected, errorReason);
            addRequestResult(serverInfoRef, timeZone.id()
                , kTimeZoneRequestDesc, kTimeZone, affected, errorReason);
        };
        
        rtu::setTimeRequest(m_httpClient, *info, m_newUtcDateTime, m_newTimeZone, successful, failed);
    }
}

///

rtu::ChangesManager::ChangesManager(RtuContext *context
    , HttpClient *httpClient
    , ServersSelectionModel *selectionModel
    , QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this, context, httpClient, selectionModel))
{
}

rtu::ChangesManager::~ChangesManager()
{
}

QObject *rtu::ChangesManager::successfulResultsModel()
{
    return m_impl->successfulResultsModel();
}

QObject *rtu::ChangesManager::failedResultsModel()
{
    return m_impl->failedResultsModel();
}

void rtu::ChangesManager::addSystemChangeRequest(const QString &newSystemName)
{
    m_impl->addSystemChangeRequest(newSystemName);
}

void rtu::ChangesManager::addPasswordChangeRequest(const QString &password)
{
    m_impl->addPasswordChangeRequest(password);
}

void rtu::ChangesManager::addPortChangeRequest(int port)
{
    m_impl->addPortChangeRequest(port);
}

void rtu::ChangesManager::addIpChangeRequest(const QString &name
    , bool useDHCP
    , const QString &address
    , const QString &subnetMask)
{
    m_impl->addIpChangeRequest(name, useDHCP, address, subnetMask);
}

void rtu::ChangesManager::addSelectionDHCPStateChange(bool useDHCP)
{
    m_impl->addIpChangeRequest(QString(), useDHCP, QString(), QString());
}

void rtu::ChangesManager::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    m_impl->addDateTimeChange(date, time, timeZoneId);
}

void rtu::ChangesManager::clearChanges()
{
    m_impl->clearChanges();
}

void rtu::ChangesManager::applyChanges()
{
    m_impl->applyChanges();
}

int rtu::ChangesManager::totalChangesCount() const
{
    return m_impl->totalChangesCount();
}

int rtu::ChangesManager::appliedChangesCount() const
{
    return m_impl->appliedChangesCount();
}

int rtu::ChangesManager::errorsCount() const
{
    return m_impl->errorsCount();
}

void rtu::ChangesManager::serverDiscovered(const BaseServerInfo &info)
{
    m_impl->onServerPing(info);
}




