
#include "changes_manager.h"

#include <QObject>

#include <base/types.h>
#include <base/requests.h>
#include <base/rtu_context.h>
#include <base/server_info.h>

#include <helpers/http_client.h>
#include <helpers/time_helper.h>

#include <models/changes_summary_model.h>
#include <models/servers_selection_model.h>

#include <QThread>

namespace
{
    struct DateTime
    {
        QDateTime utcDateTime;
        QTimeZone timeZone;

        DateTime(const QDateTime &initUtcDateTime
            , const QTimeZone &initTimeZone);
    };

    DateTime::DateTime(const QDateTime &initUtcDateTime
        , const QTimeZone &initTimeZone)
        : utcDateTime(initUtcDateTime)
        , timeZone(initTimeZone)
    {}


    struct Changeset
    {
        typedef QScopedPointer<DateTime> DateTimePointer;

        rtu::IntPointer port;
        rtu::StringPointer password;
        rtu::StringPointer systemName;
        rtu::ItfUpdateInfoContainer itfUpdateInfo;
        DateTimePointer dateTime;
    };
    
    struct ItfChangeRequest
    {
        bool inProgress;
        int tries;
        qint64 timestamp;
        rtu::ServerInfo *info;
        rtu::ItfUpdateInfoContainer itfUpdateInfo;

        ItfChangeRequest(rtu::ServerInfo *info
            , const rtu::ItfUpdateInfoContainer &initItfUpdateInfo);
    };

    ItfChangeRequest::ItfChangeRequest(rtu::ServerInfo *initInfo
        , const rtu::ItfUpdateInfoContainer &initItfUpdateInfo)
        : inProgress(false)
        , tries(0)
        , timestamp(QDateTime::currentMSecsSinceEpoch())
        , info(initInfo)
        , itfUpdateInfo(initItfUpdateInfo)
    {
    }

}

namespace
{
    const bool kNonBlockingRequest = true;
    const bool kBlockingRequest = true;
}

class rtu::ChangesManager::Impl : public QObject
{
public:
    Impl(rtu::ChangesManager *owner
        , rtu::RtuContext *context
        , rtu::HttpClient *httpClient
        , rtu::ServersSelectionModel *selectionModel);

    virtual ~Impl();

public:
    rtu::ChangesSummaryModel *successfulResultsModel();
    
    rtu::ChangesSummaryModel *failedResultsModel();
        
public:
    Changeset &getChangeset();

    rtu::ItfUpdateInfo &getItfUpdateInfo(const QString &name);
    
    void applyChanges();

    void clearChanges();
    
    int totalChangesCount() const;
    
    int appliedChangesCount() const;
    
    void serverDiscovered(const rtu::BaseServerInfo &info);
    
private:
    void onChangeApplied();

    void sendRequests();

    void addDateTimeChangeRequests();

    void addSystemNameChangeRequests();
    
    void addPortChangeRequests();
    
    void addIpChangeRequests();

    void addPasswordChangeRequests();    

    bool addSummaryItem(const rtu::ServerInfo &info
        , const QString &description
        , const QString &value
        , const QString &error
        , AffectedEntities checkFlags
        , AffectedEntities affected);

    bool addUpdateInfoSummaryItem(const rtu::ServerInfo &info
        , const QString &error
        , const ItfUpdateInfo &change
        , AffectedEntities affected);
    
private:
    typedef QHash<QUuid, ItfChangeRequest> ItfChangesContainer;
    typedef QScopedPointer<Changeset> ChangesetPointer;
    typedef QVector<rtu::ServerInfo> ServerInfoValueContainer;
    
    typedef std::function<void ()> Request;
    typedef QPair<bool, Request> RequestData;
    typedef QVector<RequestData>  RequestContainer;

    rtu::ChangesManager * const m_owner;
    rtu::RtuContext * const m_context;
    rtu::HttpClient * const m_client;
    rtu::ServersSelectionModel * const m_model;

    rtu::ChangesSummaryModel *m_succesfulChangesModel;
    rtu::ChangesSummaryModel *m_failedChangesModel;
    
    ServerInfoValueContainer m_serverValues;
    rtu::ServerInfoList m_targetServers;
    ChangesetPointer m_changeset;
    RequestContainer m_requests;
    ItfChangesContainer m_itfChanges;

    int m_applied;
    int m_current;
};

///

rtu::ChangesManager::Impl::Impl(rtu::ChangesManager *owner
    , rtu::RtuContext *context
    , rtu::HttpClient *httpClient
    , rtu::ServersSelectionModel *selectionModel)
    : QObject(owner)
    , m_owner(owner)
    , m_context(context)
    , m_client(httpClient)
    , m_model(selectionModel)
    
    , m_succesfulChangesModel(nullptr)
    , m_failedChangesModel(nullptr)
    
    , m_serverValues()
    , m_targetServers()
    , m_changeset()
    , m_requests()
    
    , m_applied(0)
    , m_current(0)
{
}

rtu::ChangesManager::Impl::~Impl()
{
}

rtu::ChangesSummaryModel *rtu::ChangesManager::Impl::successfulResultsModel()
{
    return m_succesfulChangesModel;
}

rtu::ChangesSummaryModel *rtu::ChangesManager::Impl::failedResultsModel()
{
    return m_failedChangesModel;
}

rtu::ItfUpdateInfo &rtu::ChangesManager::Impl::getItfUpdateInfo(const QString &name)
{
    Changeset &changeset = getChangeset();
    ItfUpdateInfoContainer &itfUpdate = changeset.itfUpdateInfo;
    auto it = std::find_if(itfUpdate.begin(), itfUpdate.end()
        , [&name](const ItfUpdateInfo &itf)
    {
        return (itf.name == name);
    });
    
    if (it == itfUpdate.end())
    {
        itfUpdate.push_back(ItfUpdateInfo(name));
        return itfUpdate.back();
    }
    return *it;
}

void rtu::ChangesManager::Impl::clearChanges()
{
    m_changeset.reset();
}

void rtu::ChangesManager::Impl::applyChanges()
{
    if (!m_changeset || !m_model->selectedCount())
        return;

    m_serverValues = [](const ServerInfoList &infos) -> ServerInfoValueContainer 
    {
        ServerInfoValueContainer result;
        result.reserve(infos.size());
        for(const ServerInfo *info: infos)
        {
            result.push_back(*info);
        }
        return result;
    }(m_model->selectedServers());
    
    m_targetServers = [](ServerInfoValueContainer &values)
    {
        ServerInfoList result;
        result.reserve(values.size());
        for(ServerInfo &info: values)
        {
            result.push_back(&info);
        }
        return result;
    }(m_serverValues);

    m_requests.clear();
    addDateTimeChangeRequests();
    addSystemNameChangeRequests();
    addIpChangeRequests();
    addPortChangeRequests();
    addPasswordChangeRequests();
    
    if (m_succesfulChangesModel)
        m_succesfulChangesModel->deleteLater();
    if (m_failedChangesModel)
        m_failedChangesModel->deleteLater();
    
    m_succesfulChangesModel = new ChangesSummaryModel(true, this);
    m_failedChangesModel = new ChangesSummaryModel(false, this);
    
    m_current = 0;
    m_applied = 0;
    emit m_owner->totalChangesCountChanged();
    emit m_owner->appliedChangesCountChanged();
    
    m_context->setCurrentPage(Constants::ProgressPage);
    
    sendRequests();
}

int rtu::ChangesManager::Impl::totalChangesCount() const
{
    return m_requests.size();
}

int rtu::ChangesManager::Impl::appliedChangesCount() const
{
    return m_applied;
}

rtu::InterfaceInfoList::const_iterator changeAppliedPos(const rtu::InterfaceInfoList &itf
    , const rtu::ItfUpdateInfo &change)
{
    const auto &it = std::find_if(itf.begin(), itf.end()
        , [change](const rtu::InterfaceInfo &info)
    {
        return (change.name == info.name);
    });

    if (it == itf.end())
        return it;

    const rtu::InterfaceInfo &info = *it;
    const bool applied = 
        ((!change.useDHCP || (*change.useDHCP == (info.useDHCP == Qt::Checked)))
        && (!change.ip || (*change.ip == info.ip))
        && (!change.mask || (*change.mask == info.mask))
        && (!change.dns || (*change.dns == info.dns))
        && (!change.gateway || (*change.gateway == info.gateway)));
    
    return (applied ? it : itf.end());
}

bool changesApplied(const rtu::InterfaceInfoList &itf
    , const rtu::ItfUpdateInfoContainer &changes)
{
    for (const rtu::ItfUpdateInfo &change: changes)
    {
        if (changeAppliedPos(itf, change) == itf.end())
            return false;
    }
    return true;
}

void rtu::ChangesManager::Impl::serverDiscovered(const rtu::BaseServerInfo &baseInfo)
{
    const auto &it = m_itfChanges.find(baseInfo.id);
    if (it == m_itfChanges.end())
        return;
    
    ItfChangeRequest &request = it.value();
    if (request.inProgress)
        return;

    //TODO: #ynikitenkov #high invalid scenario
    if (!request.info->hasExtraInfo()) {
        m_itfChanges.erase(it);
        onChangeApplied();
        return;
    }
    
    enum { kRequestsMinOffset = 2 * 1000};
    const qint64 current = QDateTime::currentMSecsSinceEpoch();
    if ((current - request.timestamp) < kRequestsMinOffset)
        return;
    
    const auto processCallback =
         [this, baseInfo](bool successful, const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        const auto &it = m_itfChanges.find(id);
        if (it == m_itfChanges.end())
            return;
        
        ItfChangeRequest &request = it.value();
        request.inProgress = false;
        request.timestamp = QDateTime::currentMSecsSinceEpoch();
        
        const bool totalApplied = changesApplied(extraInfo.interfaces, request.itfUpdateInfo);
        
        enum { kMaxTriesCount = 20 };
        if (!(successful && totalApplied) && (++request.tries < kMaxTriesCount))
            return;

        InterfaceInfoList newInterfaces;
        const ServerInfo &info = *request.info; 
        for (ItfUpdateInfo &change: request.itfUpdateInfo)
        {
            const auto &it = changeAppliedPos(extraInfo.interfaces, change);
            const bool applied = (it != extraInfo.interfaces.end());
            const QString errorReason = (applied ? QString() : QString("Can't apply this parameter"));

            //TODO: #ynikitenkov I do not understand this strange logic
            //const AffectedEntities affected = (applied ? kNoEntitiesAffected : kAllEntitiesAffected);
            const AffectedEntities affected = (applied ? kAllEntitiesAffected : kNoEntitiesAffected);
            addUpdateInfoSummaryItem(info, errorReason, change, affected);
            
            if (applied)
                newInterfaces.push_back(*it);
        }
        if (successful && totalApplied)
        {
            request.info->writableBaseInfo().hostAddress = baseInfo.hostAddress;
            m_model->updateInterfacesInfo(info.baseInfo().id
                , baseInfo.hostAddress, newInterfaces);
        }
        
        m_itfChanges.erase(it);
        onChangeApplied();
    };
    
    const auto &successful =
        [processCallback](const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        processCallback(true, id, extraInfo);
    };

    const auto &failed = 
        [processCallback, baseInfo](const QString &, int)
    {
        processCallback(false, baseInfo.id, ExtraServerInfo());
    };
    
    request.inProgress = true;
    sendIfListRequest(m_client, baseInfo
        , request.info->extraInfo().password, successful, failed);
}


Changeset &rtu::ChangesManager::Impl::getChangeset()
{
    if (!m_changeset)
        m_changeset.reset(new Changeset());

    return *m_changeset;
}

void rtu::ChangesManager::Impl::onChangeApplied()
{
    ++m_applied;
    emit m_owner->appliedChangesCountChanged();
    
    if (m_requests.size() != m_applied)
    {
        sendRequests();
        return;
    }
    
    m_changeset.reset();
    m_context->setCurrentPage(Constants::SummaryPage);
    emit m_model->selectionChanged();
}

void rtu::ChangesManager::Impl::sendRequests()
{
    while(true)
    {
        if (m_current >= m_requests.size())
            return;

        const RequestData &request = m_requests.at(m_current);
        if ((request.first == kBlockingRequest) && (m_applied < m_current))
            return;

        request.second();
        ++m_current;
    }
}

void rtu::ChangesManager::Impl::addDateTimeChangeRequests()
{
    if (!m_changeset || !m_changeset->dateTime)
        return;

    const QDateTime timestamp = QDateTime::currentDateTimeUtc();
    const DateTime &change = *m_changeset->dateTime;
    for (ServerInfo *info: m_targetServers)
    {
        const auto request = [this, info, change, timestamp]()
        {
            const auto &callback = 
                [this, info, change, timestamp](const QString &errorReason, AffectedEntities affected)
            {
                static const QString kDateTimeDescription = "date / time";
                static const QString kTimeZoneDescription = "time zone";
                
                const QString &value= convertUtcToTimeZone(change.utcDateTime, change.timeZone).toString();
                const bool dateTimeOk = addSummaryItem(*info, kDateTimeDescription
                    , value, errorReason, kDateTimeAffected, affected);
                const bool timeZoneOk = addSummaryItem(*info, kTimeZoneDescription
                    , QString(change.timeZone.id()), errorReason, kTimeZoneAffected, affected);
                
                if (dateTimeOk && timeZoneOk)
                {
                    m_model->updateTimeDateInfo(info->baseInfo().id, change.utcDateTime
                        , change.timeZone, timestamp);
                    
                    ExtraServerInfo &extraInfo = info->writableExtraInfo();
                    extraInfo.timestamp = timestamp;
                    extraInfo.timeZone = change.timeZone;
                    extraInfo.utcDateTime = change.utcDateTime;
                }
                
                onChangeApplied();
            };

            sendSetTimeRequest(m_client, *info, change.utcDateTime, change.timeZone, callback);
        };

        m_requests.push_back(RequestData(kNonBlockingRequest, request));
    }
}

void rtu::ChangesManager::Impl::addSystemNameChangeRequests()
{
    if (!m_changeset || !m_changeset->systemName)
        return;

    const QString &systemName = *m_changeset->systemName;
    for (ServerInfo *info: m_targetServers)
    {
        const auto request = [this, info, systemName]()
        {
            const auto &callback = 
                [this, info, systemName](const QString &errorReason, AffectedEntities affected)
            {
                static const QString &kSystemNameDescription = "system name";

                if(addSummaryItem(*info, kSystemNameDescription, systemName
                    , errorReason, kSystemNameAffected, affected))
                {
                    m_model->updateSystemNameInfo(info->baseInfo().id, systemName);
                    info->writableBaseInfo().systemName = systemName;
                }
                onChangeApplied();
            };

            sendSetSystemNameRequest(m_client, *info, systemName, callback);
        };

        m_requests.push_back(RequestData(kNonBlockingRequest, request));
    }
}

void rtu::ChangesManager::Impl::addPortChangeRequests()
{
    if (!m_changeset || !m_changeset->port)
        return;

    const int port = *m_changeset->port;
    for (ServerInfo *info: m_targetServers)
    {
        const auto request = [this, info, port]()
        {
            const auto &callback = 
                [this, info, port](const QString &errorReason, AffectedEntities affected)
            {
                static const QString kPortDescription = "port";
                
                
                if (addSummaryItem(*info, kPortDescription, QString::number(port)
                    , errorReason, kPortAffected, affected))
                {
                    m_model->updatePortInfo(info->baseInfo().id, port);
                    info->writableBaseInfo().port = port;
                }
                onChangeApplied();
            };

            sendSetPortRequest(m_client, *info, port, callback);
        };

        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

void rtu::ChangesManager::Impl::addIpChangeRequests()
{
    if (!m_changeset || m_changeset->itfUpdateInfo.empty())
        return;
    
    const auto &addRequest = 
        [this](ServerInfo *info, const ItfUpdateInfoContainer &changes)
    {
        const auto &request = 
            [this, info, changes]()
        {
            const auto &callback = 
                [this, info, changes](const QString &errorReason, AffectedEntities affected)
            {
                /* In case of success we need to wait more until server is rediscovered. */
                if (errorReason.isEmpty()) //TODO: #ynikitenkov very strange way to check error
                    return;
                
                for (const auto &change: changes)
                    addUpdateInfoSummaryItem(*info, errorReason, change, affected);

                const auto &it = m_itfChanges.find(info->baseInfo().id);
                if (it != m_itfChanges.end())
                    m_itfChanges.erase(it);

                onChangeApplied();
            };

            m_itfChanges.insert(info->baseInfo().id, ItfChangeRequest(info, changes));
            sendChangeItfRequest(m_client, *info, changes, callback);
        };
        
        m_requests.push_back(RequestData(kBlockingRequest, request));
    };

    enum { kSingleSelection = 1 };
    if (m_targetServers.size() == 1)
    {
        addRequest(m_targetServers.first(), m_changeset->itfUpdateInfo);
        return;
    }

    /// In case of multiple selection, it is allowed to turn on DHCP only
    for (ServerInfo *info: m_targetServers)
    {
        if (!info->hasExtraInfo())
            return;
        
        ItfUpdateInfoContainer changes;
        for (const InterfaceInfo &itfInfo: info->extraInfo().interfaces)
        {
            ItfUpdateInfo turnOnDhcpUpdate(itfInfo.name);
            turnOnDhcpUpdate.useDHCP.reset(new bool(true));
            changes.push_back(turnOnDhcpUpdate);
        }
        addRequest(info, changes);
    }
}

void rtu::ChangesManager::Impl::addPasswordChangeRequests()
{
    if (!m_changeset || !m_changeset->password)
        return;
    
    const QString &password = *m_changeset->password;
    for (ServerInfo *info: m_targetServers)
    {
        const auto request = [this, info, password]()
        {
            const auto finalCallback = 
                [this, info, password](const QString &errorReason, AffectedEntities affected)
            {
                static const QString kPasswordDescription = "password";

                if (addSummaryItem(*info, kPasswordDescription, password
                    , errorReason, kPasswordAffected, affected))
                {
                    info->writableExtraInfo().password = password;
                    m_model->updatePasswordInfo(info->baseInfo().id, password);
                }
                
                onChangeApplied();
            };
            
            const auto &callback = 
                [this, info, password, finalCallback](const QString &errorReason, AffectedEntities affected)
            {
                if (errorReason.isEmpty())
                {
                    finalCallback(errorReason, affected);
                }
                else
                {
                    sendSetPasswordRequest(m_client, *info, password, true, finalCallback);
                }
            };

            sendSetPasswordRequest(m_client, *info, password, false, callback );
        };

        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

bool rtu::ChangesManager::Impl::addSummaryItem(const rtu::ServerInfo &info
    , const QString &description
    , const QString &value
    , const QString &error
    , AffectedEntities checkFlags
    , AffectedEntities affected)
{
    static const Qt::HANDLE h = QThread::currentThreadId();
    if (h != QThread::currentThreadId())
        qDebug() << "***************************************************";
                    
    const bool successResult = error.isEmpty() && 
        ((affected & checkFlags) == checkFlags);
    ChangesSummaryModel * const model = (successResult ?
        m_succesfulChangesModel : m_failedChangesModel);

    model->addRequestResult(info, description, value, error);
    return successResult;
}

bool rtu::ChangesManager::Impl::addUpdateInfoSummaryItem(const rtu::ServerInfo &info
    , const QString &error
    , const ItfUpdateInfo &change
    , AffectedEntities affected)
{
    bool success = error.isEmpty();

    if (change.useDHCP)
    {
        static const QString &kDHCPDescription = "dhcp";
        static const QString &kYes = "yes";
        static const QString &kNo = "no";
        const QString useDHCPValue = (*change.useDHCP? kYes : kNo);
        success &= addSummaryItem(info, kDHCPDescription, useDHCPValue, error
            , kDHCPUsageAffected, affected);
    }

    if (change.ip)
    {
        static const QString &kIpDescription = "ip";
        success &= addSummaryItem(info, kIpDescription, *change.ip, error
            , kIpAddressAffected, affected);
    }

    if (change.mask)
    {
        static const QString &kMaskDescription = "mask";
        success &= addSummaryItem(info, kMaskDescription, *change.mask, error
            , kMaskAffected, affected);
    }

    if (change.dns)
    {
        static const QString &kDnsDescription = "dns";
        success &= addSummaryItem(info, kDnsDescription, *change.dns, error
            , kDNSAffected, affected);
    }

    if (change.gateway)
    {
        static const QString &kGatewayDescription = "gateway";
        success &= addSummaryItem(info, kGatewayDescription, *change.gateway, error
            , kGatewayAffected, affected);
    }

    return success;
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

void rtu::ChangesManager::addSystemChange(const QString &systemName)
{
    m_impl->getChangeset().systemName.reset(new QString(systemName));
}

void rtu::ChangesManager::addPasswordChange(const QString &password)
{
    m_impl->getChangeset().password.reset(new QString(password));
}

void rtu::ChangesManager::addPortChange(int port)
{
    m_impl->getChangeset().port.reset(new int(port));
}

void rtu::ChangesManager::addDHCPChange(const QString &name
    , bool useDHCP)
{
    m_impl->getItfUpdateInfo(name).useDHCP.reset(new bool(useDHCP));
}

void rtu::ChangesManager::addAddressChange(const QString &name
    , const QString &address)
{
    m_impl->getItfUpdateInfo(name).ip.reset(new QString(address));
}

void rtu::ChangesManager::addMaskChange(const QString &name
    , const QString &mask)
{
    m_impl->getItfUpdateInfo(name).mask.reset(new QString(mask));
}

void rtu::ChangesManager::addDNSChange(const QString &name
    , const QString &dns)
{
    m_impl->getItfUpdateInfo(name).dns.reset(new QString(dns));
}

void rtu::ChangesManager::addGatewayChange(const QString &name
    , const QString &gateway)
{
    m_impl->getItfUpdateInfo(name).gateway.reset(new QString(gateway));
}

void rtu::ChangesManager::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    const QDateTime &utcTime = utcFromTimeZone(date, time, QTimeZone(timeZoneId));
    m_impl->getChangeset().dateTime.reset(new DateTime(utcTime, QTimeZone(timeZoneId)));
}

void rtu::ChangesManager::turnOnDhcp()
{
    static const QString &kAnyName = "blah blah";
    /// In this case it is multiple selection. Just add empty interface
    /// change information to indicate that we should force dhcp on
    m_impl->getItfUpdateInfo(kAnyName);
}

void rtu::ChangesManager::applyChanges()
{
    m_impl->applyChanges();
}

void rtu::ChangesManager::clearChanges()
{
    m_impl->clearChanges();
}

int rtu::ChangesManager::totalChangesCount() const
{
    return m_impl->totalChangesCount();
}

int rtu::ChangesManager::appliedChangesCount() const
{
    return m_impl->appliedChangesCount();
}

void rtu::ChangesManager::serverDiscovered(const rtu::BaseServerInfo &info)
{
    m_impl->serverDiscovered(info);
}
