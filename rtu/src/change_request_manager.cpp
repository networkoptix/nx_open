
#include "change_request_manager.h"

#include <QString>

#include <rtu_context.h>
#include <requests/requests.h>
#include <models/servers_selection_model.h>
#include <models/changes_summary_model.h>

namespace
{
    enum { kInvalidPort = 0 };
}

class rtu::ChangeRequestManager::Impl : public QObject
{
public:
    Impl(rtu::ChangeRequestManager *owner
        , rtu::RtuContext *context
        , rtu::ServersSelectionModel *selectionModel
        , QObject *parent);
    
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
    
    ///
    rtu::ChangesSummaryModel *successfulResultsModel();
    
    rtu::ChangesSummaryModel *failedResultsModel();
    
private:    
    void addRequestResult(const ServerInfo &info
        , const QString &value
        , const QString &valueName
        , rtu::AffectedEntities entityFlag
        , rtu::AffectedEntities affectedEntities
        , const QString &errorReason);
    
    rtu::OperationCallback makeAddressOpCallback(const ServerInfo &info
        , const rtu::InterfaceInfoList &changes);
    
    rtu::OperationCallback makeSystemOpCallback(const ServerInfo &info);
    
    void applyDateTimeChanges(const rtu::ServerInfoList &servers);
    
public:
    rtu::ChangeRequestManager * const m_owner;
    rtu::RtuContext * const m_context;
    rtu::ServersSelectionModel * const m_selectionModel;
    
    QString m_newSystemName;
    QString m_newPassword;
    int m_newPort;
    
    rtu::InterfaceInfoList m_addressChanges;
    
    QDateTime m_newDateTime;

    int m_totalChanges;
    int m_appliedChanges;
    int m_errorCount;
    
    typedef QScopedPointer<rtu::ChangesSummaryModel> ChangesSummaryModelPointer;
    ChangesSummaryModelPointer m_sucessfulChangesSummary;
    ChangesSummaryModelPointer m_failedChangesSummary;
};

rtu::ChangeRequestManager::Impl::Impl(
    ChangeRequestManager *owner
    , rtu::RtuContext *context
    , rtu::ServersSelectionModel *selectionModel
    , QObject *parent)
    : QObject(parent)
    , m_owner(owner)
    , m_context(context)
    , m_selectionModel(selectionModel)
    , m_newSystemName()
    , m_newPassword()
    , m_newPort(kInvalidPort)
   
    , m_addressChanges()
    
    , m_newDateTime()
    
    , m_totalChanges(0)
    , m_appliedChanges(0)
    , m_errorCount(0)
    
    , m_sucessfulChangesSummary()
    , m_failedChangesSummary()
{
    
}

rtu::ChangeRequestManager::Impl::~Impl()
{
    
}

void rtu::ChangeRequestManager::Impl::addSystemChangeRequest(const QString &newSystemName)
{
    m_newSystemName = newSystemName;
}

void rtu::ChangeRequestManager::Impl::addPasswordChangeRequest(const QString &password)
{
    m_newPassword = password;
}

void rtu::ChangeRequestManager::Impl::addPortChangeRequest(int port)
{
    m_newPort = port;
}

void rtu::ChangeRequestManager::Impl::addIpChangeRequest(const QString &name
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

void rtu::ChangeRequestManager::Impl::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    m_newDateTime = QDateTime(date, time, QTimeZone(timeZoneId));
}

void rtu::ChangeRequestManager::Impl::clearChanges()
{
    m_newSystemName.clear();
    m_newPassword.clear();
    m_newPort = kInvalidPort;
    
    m_addressChanges.clear();
    m_newDateTime = QDateTime();
    
    setAppliedChangesCount(0);
    setTotalChangesCount(0);
    setErrorsCount(0);
}

void rtu::ChangeRequestManager::Impl::applyChanges()
{
    const ServerInfoList &selectedServers = m_selectionModel->selectedServers();
    
    const int systemNameChangesCount = (m_newSystemName.isEmpty() ? 0 : selectedServers.size());
    const int passwordChangesCount = (m_newPassword.isEmpty() ? 0 : selectedServers.size());
    const int portChangesCount = (!m_newPort ? 0 : selectedServers.size());
    const int systemChangesCount = systemNameChangesCount + passwordChangesCount + portChangesCount;
    
    setAppliedChangesCount(0);
    setTotalChangesCount(systemChangesCount);
    setErrorsCount(0);
    
    m_sucessfulChangesSummary.reset(new ChangesSummaryModel(true));
    m_failedChangesSummary.reset(new ChangesSummaryModel(false));
            
    m_context->setCurrentPage(RtuContext::kProgressPage);
    
    HttpClient * const httpClient = m_context->httpClient();
    
    if (!m_addressChanges.empty())
    {
        if (selectedServers.size() == 1)
        {
            const ServerInfo &serverInfo = **selectedServers.begin();
            setTotalChangesCount(totalChangesCount() + m_addressChanges.size());
            /// change all requested ips by single request fo signle selected server
            rtu::changeIpsRequest(httpClient, serverInfo, m_addressChanges
                , makeAddressOpCallback(serverInfo, m_addressChanges));
        }
        else if (m_addressChanges.size() == 1)
        {
            const InterfaceInfo &addressChanges = *m_addressChanges.begin();
            for(const ServerInfo *info: selectedServers)
            {
                InterfaceInfoList changes;
                for(const InterfaceInfo &addInfo: info->extraInfo().interfaces)
                {
                    const InterfaceInfo interface = InterfaceInfo(addInfo.name, addressChanges.ip
                        , "", addressChanges.mask, "", addressChanges.useDHCP);
                    changes.push_back(interface);
                }
                
                setTotalChangesCount(totalChangesCount() + changes.size());
                rtu::changeIpsRequest(httpClient, *info, changes
                    , makeAddressOpCallback(*info, changes));
            }
        }
        else
        {
        }
    }
    
    if (systemChangesCount)
    {
        for(const ServerInfo *info: selectedServers)
        {
            rtu::configureRequest(httpClient, makeSystemOpCallback(*info)
                , *info, m_newSystemName, m_newPassword, m_newPort);
        }
    }
    
    applyDateTimeChanges(selectedServers);
}

void rtu::ChangeRequestManager::Impl::onChangesetApplied(int changesCount)
{
    setAppliedChangesCount(appliedChangesCount() + changesCount);
    if (appliedChangesCount() != totalChangesCount())
        return;
    if (!m_newPassword.isEmpty())
        m_selectionModel->selectionPasswordChanged(m_newPassword);
    
    m_context->setCurrentPage(RtuContext::kSummaryPage);
}

int rtu::ChangeRequestManager::Impl::totalChangesCount() const
{
    return m_totalChanges;
}

void rtu::ChangeRequestManager::Impl::setTotalChangesCount(int count)
{
    if (m_totalChanges == count)
        return;
   
    m_totalChanges = count;
    emit m_owner->totalChangesCountChanged();
}


int rtu::ChangeRequestManager::Impl::appliedChangesCount() const
{
    return m_appliedChanges;
}

void rtu::ChangeRequestManager::Impl::setAppliedChangesCount(int count)
{
    if (m_appliedChanges == count)
        return;
   
    m_appliedChanges = count;
    emit m_owner->appliedChangesCountChanged();
}


int rtu::ChangeRequestManager::Impl::errorsCount() const
{
    return m_errorCount;
}

void rtu::ChangeRequestManager::Impl::setErrorsCount(int count)
{
    if (m_errorCount == count)
        return;
   
    m_errorCount = count;
    emit m_owner->errorsCountChanged();
}

rtu::ChangesSummaryModel *rtu::ChangeRequestManager::Impl::successfulResultsModel()
{
    return m_sucessfulChangesSummary.take();
}

rtu::ChangesSummaryModel *rtu::ChangeRequestManager::Impl::failedResultsModel()
{
    return m_failedChangesSummary.take();
}

rtu::OperationCallback rtu::ChangeRequestManager::Impl::makeAddressOpCallback(const ServerInfo &info
    , const InterfaceInfoList &changes)
{
    const auto callback = [this, info, changes](const QString &errorReason, AffectedEntities affectedEntities)
    {
        for(const InterfaceInfo &addrInfo: changes)
        {
            static const QString kYes = tr("yes");
            static const QString kNo = tr("no");
            
            if (addrInfo.ip.isEmpty())
            {
                static const QString requestNameTemplate = tr("use dhcp (%1)");
                addRequestResult(info, (addrInfo.useDHCP ? kYes : kNo)
                    , requestNameTemplate.arg(addrInfo.name), kDHCPUsage, affectedEntities, errorReason);
            }
            else
            {
                static const QString requestNameTemplate = tr("ip / subnet mask / use dhcp (%1)");
                static const QString valueTemplate = "%1 / %2 / %3";
                
                const QString valueName = requestNameTemplate.arg(addrInfo.name);
                const QString value = valueTemplate.arg(addrInfo.ip)
                    .arg(addrInfo.mask).arg(addrInfo.useDHCP ? kYes : kNo);
                
                addRequestResult(info, value, valueName
                    , kIpAddress | kSubnetMask | kDHCPUsage, affectedEntities, errorReason);
            }
        }
    };
    
    return callback;
}

void rtu::ChangeRequestManager::Impl::addRequestResult(const ServerInfo &info
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

rtu::OperationCallback rtu::ChangeRequestManager::Impl::makeSystemOpCallback(const ServerInfo &info)
{
    const auto callback = 
        [this, info](const QString &errorReason, AffectedEntities affectedEntities)
    {
        static const QString kPasswordRequestDesc = tr("password");
        static const QString kSystemNameRequestDesc = tr("system name");
        static const QString kPortRequestDesc = tr("port");
        
        addRequestResult(info, m_newPassword, kPasswordRequestDesc
            , kPassword, affectedEntities, errorReason);
        
        if (!m_newSystemName.isEmpty())
            m_selectionModel->updateSystemNameInfo(info.baseInfo().id, m_newSystemName);
        addRequestResult(info, m_newSystemName, kSystemNameRequestDesc
            , kSystemName, affectedEntities, errorReason);

        if (m_newPort)
            m_selectionModel->updatePortInfo(info.baseInfo().id, m_newPort);
        addRequestResult(info, (m_newPort ? QString::number(m_newPort) : QString())
            , kPortRequestDesc, kPort, affectedEntities, errorReason);
    };
    
    return callback;
}

void rtu::ChangeRequestManager::Impl::applyDateTimeChanges(const rtu::ServerInfoList &servers)
{
    if (m_newDateTime.isNull() || !m_newDateTime.isValid())
        return;
    
    const int changesCount = (servers.size() * 2); /// Date/Time + Time zone
    setTotalChangesCount(totalChangesCount() + changesCount);
    
    const QDateTime applyingDateTime = m_newDateTime;
    
    HttpClient *client = m_context->httpClient();
    for (const ServerInfo *info: servers)
    {
        static const QString &kDateTimeRequestDesc = tr("date/time");
        static const QString &kTimeZoneRequestDesc = tr("time zone");
        
        const ServerInfo &serverInfoRef = *info;
        const auto &successful = [serverInfoRef, this](
            const QUuid &serverId, const QDateTime &serverTime, const QDateTime &timestamp)
        {
            m_selectionModel->updateTimeDateInfo(serverId, serverTime, timestamp);
            addRequestResult(serverInfoRef, serverTime.toString(Qt::SystemLocaleShortDate), kDateTimeRequestDesc
                , kDateTime, kNoEntitiesAffected, QString());
            addRequestResult(serverInfoRef, serverTime.timeZone().id()
                , kTimeZoneRequestDesc, kTimeZone, kNoEntitiesAffected, QString());
            
            /// todo
        };
        
        const auto &failed = [serverInfoRef, this, applyingDateTime](const QString &errorReason, AffectedEntities affected)
        {  
            addRequestResult(serverInfoRef, applyingDateTime.toString(), kDateTimeRequestDesc
                , kDateTime, affected, errorReason);
            addRequestResult(serverInfoRef, applyingDateTime.timeZone().id()
                , kTimeZoneRequestDesc, kTimeZone, affected, errorReason);
        };
        
        rtu::setTimeRequest(client, *info, m_newDateTime, successful, failed);
    }
}

///

rtu::ChangeRequestManager::ChangeRequestManager(RtuContext *context
    , ServersSelectionModel *selectionModel
    , QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this, context, selectionModel, this))
{
}

rtu::ChangeRequestManager::~ChangeRequestManager()
{
}

QObject *rtu::ChangeRequestManager::successfulResultsModel()
{
    return m_impl->successfulResultsModel();
}

QObject *rtu::ChangeRequestManager::failedResultsModel()
{
    return m_impl->failedResultsModel();
}

void rtu::ChangeRequestManager::addSystemChangeRequest(const QString &newSystemName)
{
    m_impl->addSystemChangeRequest(newSystemName);
}

void rtu::ChangeRequestManager::addPasswordChangeRequest(const QString &password)
{
    m_impl->addPasswordChangeRequest(password);
}

void rtu::ChangeRequestManager::addPortChangeRequest(int port)
{
    m_impl->addPortChangeRequest(port);
}

void rtu::ChangeRequestManager::addIpChangeRequest(const QString &name
    , bool useDHCP
    , const QString &address
    , const QString &subnetMask)
{
    m_impl->addIpChangeRequest(name, useDHCP, address, subnetMask);
}

void rtu::ChangeRequestManager::addSelectionDHCPStateChange(bool useDHCP)
{
    m_impl->addIpChangeRequest(QString(), useDHCP, QString(), QString());
}

void rtu::ChangeRequestManager::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    m_impl->addDateTimeChange(date, time, timeZoneId);
}

void rtu::ChangeRequestManager::clearChanges()
{
    m_impl->clearChanges();
}

void rtu::ChangeRequestManager::applyChanges()
{
    m_impl->applyChanges();
}

int rtu::ChangeRequestManager::totalChangesCount() const
{
    return m_impl->totalChangesCount();
}

int rtu::ChangeRequestManager::appliedChangesCount() const
{
    return m_impl->appliedChangesCount();
}

int rtu::ChangeRequestManager::errorsCount() const
{
    return m_impl->errorsCount();
}



