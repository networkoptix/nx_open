
#include "apply_changes_task.h"

#include <QTimer>

#include <base/types.h>
#include <base/requests.h>
#include <base/changeset.h>
#include <base/rtu_context.h>
#include <base/server_info.h>
#include <base/changes_manager.h>

#include <helpers/rest_client.h>
#include <helpers/time_helper.h>
#include <helpers/qml_helpers.h>

#include <models/changes_summary_model.h>
#include <models/servers_selection_model.h>

namespace
{
    const QString kFakeErrorReason = "!change me!"; /// TODO: #ynikitenkov add errors handling

    enum { kSingleInterfaceChangeCount = 1 };

    enum 
    {
        kIfListRequestsPeriod = 2 * 1000
        , kWaitTriesCount = 90

        /// Accepted wait time is 180 seconds due to restart on network interface parameters change
        , kTotalIfConfigTimeout = kIfListRequestsPeriod * kWaitTriesCount   
    };

    int changesFromUpdateInfos(const rtu::ItfUpdateInfoContainer &itfUpdateInfos)
    {
        int result = 0;
        for (const rtu::ItfUpdateInfo &updateInfo: itfUpdateInfos)
        {
            result += (updateInfo.useDHCP ? 1 : 0)
                + (updateInfo.ip ? 1 : 0)
                + (updateInfo.mask ? 1 : 0)
                + (updateInfo.dns ? 1 : 0)
                + (updateInfo.gateway ? 1 : 0);
        }
        return result;
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

    bool changesWasApplied(const rtu::InterfaceInfoList &itf
        , const rtu::ItfUpdateInfoContainer &changes)
    {
        for (const rtu::ItfUpdateInfo &change: changes)
        {
            if (changeAppliedPos(itf, change) == itf.end())
                return false;
        }
        return true;
    }

    typedef QPair<rtu::BaseServerInfoPtr, rtu::ExtraServerInfo> ServerInfoCacheItem;
    typedef QVector<ServerInfoCacheItem> ServerInfosCache;

    ServerInfosCache makeServerInfosCache(const rtu::ServerInfoPtrContainer &pointers)
    {
        ServerInfosCache result;
        result.reserve(pointers.size());

        for(const rtu::ServerInfo *info: pointers)
        {
            if (!info->hasExtraInfo())
                continue;

            result.push_back(ServerInfoCacheItem(
                std::make_shared<rtu::BaseServerInfo>(info->baseInfo()), info->extraInfo()));
        }
        return result;
    }

    rtu::IDsVector extractIds(const ServerInfosCache &values)
    {
        rtu::IDsVector result;

        for(const auto &info: values)
            result.push_back(info.first->id);

        return result;
    };

}

namespace
{
    const bool kNonBlockingRequest = false;
    const bool kBlockingRequest = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{
    struct ItfChangeRequest
    {
        bool inProgress;
        int tries;
        qint64 timestamp;
        ServerInfoCacheItem *item;
        rtu::ItfUpdateInfoContainer itfUpdateInfo;
        int changesCount;

        ItfChangeRequest(ServerInfoCacheItem *initItem
            , const rtu::ItfUpdateInfoContainer &initItfUpdateInfo)
            : inProgress(false)
            , tries(0)
            , timestamp(QDateTime::currentMSecsSinceEpoch())
            , item(initItem)
            , itfUpdateInfo(initItfUpdateInfo)
            , changesCount(changesFromUpdateInfos(initItfUpdateInfo))
        {}
    };

}

class rtu::ApplyChangesTask::Impl : public std::enable_shared_from_this<rtu::ApplyChangesTask::Impl>
    , public QObject
{
public:
    Impl(const ChangesetPointer &changeset
        , const ServerInfoPtrContainer &targets);

    ~Impl();

    void apply(const ApplyChangesTaskPtr &owner);

    rtu::IDsVector targetServerIds() const;

    ChangesSummaryModel *successfulModel();

    ChangesSummaryModel *failedModel();

    int totalChangesCount() const;
        
    int appliedChangesCount() const;

    void serverDiscovered(const rtu::BaseServerInfo &baseInfo);

    void serversDisappeared(const rtu::IDsVector &ids);

    bool unknownAdded(const QString &ip);

private:
    void onChangesetApplied(int changesCount);

    void sendRequests();

    void addDateTimeChangeRequests();

    void addSystemNameChangeRequests();
    
    void addPortChangeRequests();
    
    void addIpChangeRequests();

    void addPasswordChangeRequests();    

    bool addSummaryItem(const ServerInfoCacheItem &item
        , const QString &description
        , const QString &value
        , RequestError errorCode
        , Constants::AffectedEntities checkFlags
        , Constants::AffectedEntities affected);

    bool addUpdateInfoSummaryItem(const ServerInfoCacheItem &item
        , RequestError errorCode
        , const ItfUpdateInfo &change
        , Constants::AffectedEntities affected);

    typedef QHash<QUuid, ItfChangeRequest> ItfChangesContainer;
    void itfRequestSuccessfullyApplied(const ItfChangesContainer::iterator it);

private:
    typedef std::function<void ()> Request;
    typedef QPair<bool, Request> RequestData;
    typedef QVector<RequestData>  RequestContainer;
    typedef std::weak_ptr<Impl> ThisWeakPtr;
    const ChangesetPointer m_changeset;
    ApplyChangesTaskPtr m_owner;


    ServerInfosCache m_serversCache;
    const IDsVector m_targetServerIds;

    ChangesSummaryModel m_succesfulChangesModel;
    ChangesSummaryModel m_failedChangesModel;
    
    RequestContainer m_requests;
    ItfChangesContainer m_itfChanges;

    int m_appliedChangesIndex;
    int m_lastChangesetIndex;

    int m_totalChangesCount;
    int m_appliedChangesCount;
};

rtu::ApplyChangesTask::Impl::Impl(const ChangesetPointer &changeset
    , const ServerInfoPtrContainer &targets)
    
    : std::enable_shared_from_this<ApplyChangesTask::Impl>()
    , QObject()

    , m_changeset(changeset)
    , m_owner()

    , m_serversCache(makeServerInfosCache(targets))
    , m_targetServerIds(extractIds(m_serversCache))
    
    , m_succesfulChangesModel(true, this)
    , m_failedChangesModel(false, this)
    
    , m_requests()
    , m_itfChanges()
    
    , m_appliedChangesIndex(0)
    , m_lastChangesetIndex(0)

    , m_totalChangesCount(0)
    , m_appliedChangesCount(0)

{
}

rtu::ApplyChangesTask::Impl::~Impl()
{
}

void rtu::ApplyChangesTask::Impl::apply(const ApplyChangesTaskPtr &owner)
{
    m_owner = owner;

    addDateTimeChangeRequests();
    addSystemNameChangeRequests();
    addPasswordChangeRequests();
    addPortChangeRequests();
    addIpChangeRequests();
    
    sendRequests();
}

rtu::IDsVector rtu::ApplyChangesTask::Impl::targetServerIds() const
{
    return m_targetServerIds;
}


rtu::ChangesSummaryModel *rtu::ApplyChangesTask::Impl::successfulModel()
{
    return &m_succesfulChangesModel;
}

rtu::ChangesSummaryModel *rtu::ApplyChangesTask::Impl::failedModel()
{
    return &m_failedChangesModel;
}

int rtu::ApplyChangesTask::Impl::totalChangesCount() const
{
    return m_totalChangesCount;
}
        
int rtu::ApplyChangesTask::Impl::appliedChangesCount() const
{
    return m_appliedChangesCount;
}

void rtu::ApplyChangesTask::Impl::serverDiscovered(const rtu::BaseServerInfo &info)
{
    if (unknownAdded(info.hostAddress))
        return;

    const auto &it = m_itfChanges.find(info.id);
    if (it == m_itfChanges.end())
        return;
    
    ItfChangeRequest &request = it.value();
    if (request.inProgress)
        return;

    const qint64 current = QDateTime::currentMSecsSinceEpoch();
    if ((current - request.timestamp) < kIfListRequestsPeriod)
        return;
    
    const ThisWeakPtr &weak = shared_from_this();

    const auto processCallback =
        [weak](bool successful, const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        if (weak.expired())
            return;

        const auto shared = weak.lock();
        const auto &it = shared->m_itfChanges.find(id);
        if (it == shared->m_itfChanges.end())
            return;
        
        ItfChangeRequest &request = it.value();
        request.inProgress = false;
        request.timestamp = QDateTime::currentMSecsSinceEpoch();
        
        const bool totalApplied = changesWasApplied(extraInfo.interfaces, request.itfUpdateInfo);
        
        if (!(successful && totalApplied) && (++request.tries < kWaitTriesCount))
            return;

        InterfaceInfoList newInterfaces;
        for (ItfUpdateInfo &change: request.itfUpdateInfo)
        {
            const auto &it = changeAppliedPos(extraInfo.interfaces, change);
            const bool applied = (it != extraInfo.interfaces.end());
            const Constants::AffectedEntities affected = 
                (applied ? Constants::kAllEntitiesAffected : Constants::kNoEntitiesAffected);
            shared->addUpdateInfoSummaryItem(*request.item
                , (applied ? RequestError::kSuccess : RequestError::kUnspecified) , change, affected);
            
            if (applied)
                newInterfaces.push_back(*it);
        }

        if (successful && totalApplied)
        {
            const BaseServerInfo &inf = *request.item->first;
            emit shared->m_owner->itfUpdated(inf.id
                , inf.hostAddress, newInterfaces);
        }
        
        const int changesCount = request.changesCount;
        shared->m_itfChanges.erase(it);
        shared->onChangesetApplied(changesCount);
    };
    
    const auto &successful =
        [processCallback](const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        processCallback(true, id, extraInfo);
    };

    const auto &failed = 
        [processCallback, info](const RequestError /* errorCode */, int /* affected */)
    {
        processCallback(false, info.id, ExtraServerInfo());
    };
    
    BaseServerInfo &currentBase = *request.item->first;
    const bool currentDiscoverByHttp = currentBase.discoveredByHttp;
    currentBase = info;
    currentBase.discoveredByHttp = currentDiscoverByHttp;
    request.inProgress = true;
    
    sendIfListRequest(request.item->first, request.item->second.password
        , successful, failed, kIfListRequestsPeriod);
}

void rtu::ApplyChangesTask::Impl::serversDisappeared(const rtu::IDsVector &ids)
{
    for (const QUuid &id: ids)
    {
        const auto &it = m_itfChanges.find(id);
        if (it == m_itfChanges.end())
            continue;

        ItfChangeRequest &request = it.value();
    
        /// TODO: extend logic for several interface changes
        if (request.itfUpdateInfo.size() != kSingleInterfaceChangeCount)
            continue;


        bool isDHCPTurnOnChange = true;
        for (const auto &change: request.itfUpdateInfo)
        {
            isDHCPTurnOnChange &= (change.useDHCP && *change.useDHCP);
        }
    
        if (!isDHCPTurnOnChange)
            continue;

        //// If server is disappeared and all changes are dhcp "on"
        itfRequestSuccessfullyApplied(it);
    }
}

bool rtu::ApplyChangesTask::Impl::unknownAdded(const QString &ip)
{
    /// Searches for the change request
    const auto &it = std::find_if(m_itfChanges.begin(), m_itfChanges.end()
        , [ip](const ItfChangeRequest &request)
    {
        /// Currently it is supposed to be only one-interface-change action
        /// TODO: extend logic for several interface changes
        if (request.itfUpdateInfo.size() != kSingleInterfaceChangeCount)
            return false;

        const auto &it = std::find_if(request.itfUpdateInfo.begin(), request.itfUpdateInfo.end()
            , [ip](const ItfUpdateInfo &updateInfo) 
        {
            return ((!updateInfo.useDHCP || !*updateInfo.useDHCP) && updateInfo.ip && *updateInfo.ip == ip);
        });

        return (it != request.itfUpdateInfo.end());
    });

    if (it == m_itfChanges.end())
    {
        return false;
    }

    /// We've found change request with dhcp "off" state and ip that is 
    /// equals to ip of found unknown entity.
    /// So, we suppose that request to change ip is successfully applied

    itfRequestSuccessfullyApplied(it);
    return true;
}

void rtu::ApplyChangesTask::Impl::itfRequestSuccessfullyApplied(const ItfChangesContainer::iterator it)
{
    const ItfChangeRequest &request = *it;
    for(const auto &change: request.itfUpdateInfo)
    {
        addUpdateInfoSummaryItem(*request.item, RequestError::kSuccess
            , change, Constants::kAllEntitiesAffected);
    }

    const int changesCount = request.changesCount;
    m_itfChanges.erase(it);
    onChangesetApplied(changesCount);
}

void rtu::ApplyChangesTask::Impl::onChangesetApplied(int changesCount)
{
    m_appliedChangesCount += changesCount;
    emit m_owner->appliedChangesCountChanged();

    ++m_appliedChangesIndex;
    
    if (m_requests.size() != m_appliedChangesIndex)
    {
        sendRequests();
        return;
    }

    emit m_owner->changesApplied();
    m_owner.reset(); /// Destructs this apply task if there are no other links to it
}

void rtu::ApplyChangesTask::Impl::sendRequests()
{
    while(true)
    {
        if (m_lastChangesetIndex >= m_requests.size())
            return;

        const RequestData &request = m_requests.at(m_lastChangesetIndex);
        if ((request.first == kBlockingRequest) && (m_appliedChangesIndex < m_lastChangesetIndex))
            return;

        request.second();
        ++m_lastChangesetIndex;
    }
}

void rtu::ApplyChangesTask::Impl::addDateTimeChangeRequests()
{
    if (!m_changeset->dateTime())
        return;

    const qint64 timestampMs = QDateTime::currentMSecsSinceEpoch();
    const DateTime &change = *m_changeset->dateTime();
    const ThisWeakPtr weak = shared_from_this();

    for (auto &info: m_serversCache)
    {
        enum { kDateTimeChangesetSize = 2};
        const auto request = [weak, &info, change, timestampMs]()
        {
            if (weak.expired())
                return;

            const auto &callback = [weak, &info, change, timestampMs]
                (const RequestError errorCode, Constants::AffectedEntities affected)
            {
                static const QString kDateTimeDescription = "date / time";
                static const QString kTimeZoneDescription = "time zone";

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                QString timeZoneName = timeZoneNameWithOffset(QTimeZone(change.timeZoneId), QDateTime::currentDateTime());

                QDateTime timeValue = convertUtcToTimeZone(change.utcDateTimeMs, QTimeZone(change.timeZoneId));
                const QString &value = timeValue.toString(Qt::SystemLocaleLongDate);
                const bool dateTimeOk = shared->addSummaryItem(info, kDateTimeDescription
                    , value, errorCode, Constants::kDateTimeAffected, affected);
                const bool timeZoneOk = shared->addSummaryItem(info, kTimeZoneDescription
                    , timeZoneName, errorCode, Constants::kTimeZoneAffected, affected);
                
                if (dateTimeOk && timeZoneOk)
                {
                    emit shared->m_owner->dateTimeUpdated(info.first->id
                        , change.utcDateTimeMs, change.timeZoneId, timestampMs);
                    
                    ExtraServerInfo &extraInfo = info.second;
                    extraInfo.timestampMs = timestampMs;
                    extraInfo.timeZoneId = change.timeZoneId;
                    extraInfo.utcDateTimeMs = change.utcDateTimeMs;
                }
                
                shared->onChangesetApplied(kDateTimeChangesetSize);
            };

            const auto shared = weak.lock();
            sendSetTimeRequest(info.first, info.second.password
                , change.utcDateTimeMs, change.timeZoneId, callback);
        };

        m_totalChangesCount += kDateTimeChangesetSize;
        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

void rtu::ApplyChangesTask::Impl::addSystemNameChangeRequests()
{
    if (!m_changeset->systemName())
        return;

    const QString &systemName = *m_changeset->systemName();
    const ThisWeakPtr weak = shared_from_this();
    for (auto &info: m_serversCache)
    {
        enum { kSystemNameChangesetSize = 1 };
        const auto request = [weak, &info, systemName]()
        {
            if (weak.expired())
                return;

            const auto &callback = [weak, &info, systemName]
                (const RequestError errorCode, Constants::AffectedEntities affected)
            {
                static const QString &kSystemNameDescription = "system name";

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                if(shared->addSummaryItem(info, kSystemNameDescription, systemName
                    , errorCode, Constants::kSystemNameAffected, affected))
                {
                    emit shared->m_owner->systemNameUpdated(info.first->id, systemName);
                    info.first->systemName = systemName;
                }
                
                shared->onChangesetApplied(kSystemNameChangesetSize);
            };

            const auto shared = weak.lock();
            sendSetSystemNameRequest(info.first, info.second.password, systemName, callback);
        };

        m_totalChangesCount += kSystemNameChangesetSize;
        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

void rtu::ApplyChangesTask::Impl::addPortChangeRequests()
{
    if (!m_changeset->port())
        return;

    const int port = *m_changeset->port();
    const ThisWeakPtr weak = shared_from_this();

    for (auto &info: m_serversCache)
    {
        enum { kPortChangesetCapacity = 1 };
        const auto request = [weak, &info, port]()
        {
            if (weak.expired())
                return;


            const auto &callback = [weak, &info, port]
                (const RequestError errorCode, Constants::AffectedEntities affected)
            {
                static const QString kPortDescription = "port";

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                if (shared->addSummaryItem(info, kPortDescription, QString::number(port)
                    , errorCode, Constants::kPortAffected, affected))
                {
                    emit shared->m_owner->portUpdated(info.first->id, port);
                    info.first->port = port;
                }

                shared->onChangesetApplied(kPortChangesetCapacity);
            };


            const auto shared = weak.lock();
            sendSetPortRequest(info.first, info.second.password, port, callback);
        };

        m_totalChangesCount += kPortChangesetCapacity;
        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

void rtu::ApplyChangesTask::Impl::addIpChangeRequests()
{
    if (!m_changeset->itfUpdateInfo() || m_changeset->itfUpdateInfo()->empty())
        return;
    
    const ThisWeakPtr weak = shared_from_this();

    const auto &addRequest = 
        [weak](ServerInfoCacheItem &item, const ItfUpdateInfoContainer &changes)
    {
        if (weak.expired())
            return;

        const QUuid id = item.first->id;
        const ItfChangeRequest changeRequest(&item, changes);
        const auto &request = 
            [weak, id, &item, changes, changeRequest]()
        {
            if (weak.expired())
                return;

            const auto &failedCallback =
                [weak, id, &item, changes](const RequestError errorCode, Constants::AffectedEntities affected)
            {
                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                const auto &it = shared->m_itfChanges.find(id);
                if (it == shared->m_itfChanges.end())
                    return;

                for (const auto &change: changes)
                    shared->addUpdateInfoSummaryItem(item, errorCode, change, affected);

                const int changesCount = it.value().changesCount;
                shared->m_itfChanges.erase(it);
                shared->onChangesetApplied(changesCount);
            };

            const auto &callback = 
                [failedCallback](const RequestError errorCode, Constants::AffectedEntities affected)
            {
                if (errorCode != RequestError::kSuccess)
                {
                    failedCallback(errorCode , affected);
                }
                else
                {
                    /* In case of success we need to wait more until server is rediscovered. */
                    QTimer::singleShot(kTotalIfConfigTimeout, [failedCallback, affected]() 
                    {
                        failedCallback(RequestError::kRequestTimeout, affected);
                    });
                }
                
            };

            const auto shared = weak.lock();
            shared->m_itfChanges.insert(id, changeRequest);
            sendChangeItfRequest(changeRequest.item->first, changeRequest.item->second.password
                , changes, callback);
        };
        
        const auto shared = weak.lock();
        shared->m_totalChangesCount += changeRequest.changesCount;

        /// Request is non blocking due to restart on interface parameters change. 
        // I.e. we send multiple unqued requests and not wait until they complete before next send.
        shared->m_requests.push_back(RequestData(kNonBlockingRequest, request));
    };

    enum { kSingleSelection = 1 };
    if (m_serversCache.size() == 1)
    {
        const ItfUpdateInfoContainerPointer &itf = m_changeset->itfUpdateInfo();
        if (!itf)
            return;

        addRequest(m_serversCache.first(), *itf);
        return;
    }

    /// In case of multiple selection, it is allowed to turn on DHCP only
    for (auto &info: m_serversCache)
    {
        ItfUpdateInfoContainer changes;
        for (const InterfaceInfo &itfInfo: info.second.interfaces)
        {
            if (itfInfo.useDHCP)   /// Do not switch DHCP "ON" where it has turned on already
                continue;

            ItfUpdateInfo turnOnDhcpUpdate(itfInfo.name);
            turnOnDhcpUpdate.useDHCP.reset(new bool(true));
            changes.push_back(turnOnDhcpUpdate);
        }

        if (!changes.empty())
            addRequest(info, changes);
    }
}

void rtu::ApplyChangesTask::Impl::addPasswordChangeRequests()
{
    if (!m_changeset->password())
        return;
    
    const QString &newPassword = *m_changeset->password();
    const ThisWeakPtr weak = shared_from_this();

    for (auto &info: m_serversCache)
    {
        enum { kPasswordChangesetSize = 1 };
        const auto request = [weak, &info, newPassword ]()
        {
            if (weak.expired())
                return;

            const auto finalCallback =
                [weak, &info, newPassword ](const RequestError errorCode, Constants::AffectedEntities affected)
            {
                static const QString kPasswordDescription = "password";

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                if (shared->addSummaryItem(info, kPasswordDescription, newPassword 
                    , errorCode, Constants::kPasswordAffected, affected))
                {
                    info.second.password = newPassword;
                    emit shared->m_owner->passwordUpdated(info.first->id, newPassword);
                }
                
                shared->onChangesetApplied(kPasswordChangesetSize);
            };
            
            const auto &callback = 
                [weak, &info, newPassword, finalCallback](const RequestError errorCode, Constants::AffectedEntities affected)
            {
                if (weak.expired())
                    return;

                if (errorCode != RequestError::kSuccess)
                {
                    finalCallback(errorCode, affected);
                }
                else
                {
                    const auto shared = weak.lock();
                    sendSetPasswordRequest(info.first, info.second.password, newPassword, true, finalCallback);
                }
            };

            const auto shared = weak.lock();
            sendSetPasswordRequest(info.first, info.second.password, newPassword, false, callback);
        };

        m_totalChangesCount += kPasswordChangesetSize;
        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

bool rtu::ApplyChangesTask::Impl::addSummaryItem(const ServerInfoCacheItem &item
    , const QString &description
    , const QString &value
    , RequestError errorCode
    , Constants::AffectedEntities checkFlags
    , Constants::AffectedEntities affected)
{
    const bool successResult = (errorCode == RequestError::kSuccess) && 
        ((affected & checkFlags) == checkFlags);
    ChangesSummaryModel * const model = (successResult ? &m_succesfulChangesModel : &m_failedChangesModel);

    const auto codeToString = [](RequestError code) -> QString
    {
        switch(code)
        {
        case RequestError::kSuccess:
            return QString();
        case RequestError::kRequestTimeout:
            return tr("Request Timeout");
        case RequestError::kUnauthorized:
            return tr("Unauthorized");
        default:
            return tr("Unknown error");
        }
    };

    model->addRequestResult(ServerInfo(*item.first, item.second)
        , description, value, codeToString(errorCode));
    return successResult;
}

bool rtu::ApplyChangesTask::Impl::addUpdateInfoSummaryItem(const ServerInfoCacheItem &item
    , RequestError errorCode
    , const ItfUpdateInfo &change
    , Constants::AffectedEntities affected)
{
    bool success = (errorCode == RequestError::kSuccess);

    if (change.useDHCP)
    {
        static const QString &kDHCPDescription = "dhcp";
        static const QString &kYes = "on";
        static const QString &kNo = "off";
        const QString useDHCPValue = (*change.useDHCP? kYes : kNo);
        success &= addSummaryItem(item, kDHCPDescription, useDHCPValue, errorCode
            , Constants::kDHCPUsageAffected, affected);
    }

    if (change.ip)
    {
        static const QString &kIpDescription = "ip";
        success &= addSummaryItem(item, kIpDescription, *change.ip, errorCode
            , Constants::kIpAddressAffected, affected);
    }

    if (change.mask)
    {
        static const QString &kMaskDescription = "mask";
        success &= addSummaryItem(item, kMaskDescription, *change.mask, errorCode
            , Constants::kMaskAffected, affected);
    }

    if (change.dns)
    {
        static const QString &kDnsDescription = "dns";
        success &= addSummaryItem(item, kDnsDescription, *change.dns, errorCode
            , Constants::kDNSAffected, affected);
    }

    if (change.gateway)
    {
        static const QString &kGatewayDescription = "gateway";
        success &= addSummaryItem(item, kGatewayDescription, *change.gateway, errorCode
            , Constants::kGatewayAffected, affected);
    }

    return success;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

rtu::ApplyChangesTaskPtr rtu::ApplyChangesTask::create(const ChangesetPointer &changeset
    , const ServerInfoPtrContainer &target)
{
    const ApplyChangesTaskPtr result(new ApplyChangesTask(changeset, target));

    /// we have to use this function because std::shared_from_this cannot be called from 
    /// constructor (when there is no one shared pointer exists)
    result->m_impl->apply(result);
    return result;
}

rtu::ApplyChangesTask::ApplyChangesTask(const ChangesetPointer &changeset
    , const ServerInfoPtrContainer &targets)
    
    : QObject(helpers::qml_objects_parent())
    , m_impl(new Impl(changeset, targets))
{
    QObject::connect(m_impl->failedModel(), &ChangesSummaryModel::changesCountChanged
        , this, &ApplyChangesTask::errorsCountChanged);
}

rtu::ApplyChangesTask::~ApplyChangesTask()
{
}

rtu::IDsVector rtu::ApplyChangesTask::targetServerIds() const
{
    return m_impl->targetServerIds();
}

QObject *rtu::ApplyChangesTask::successfulModel()
{
    return m_impl->successfulModel();
}

QObject *rtu::ApplyChangesTask::failedModel()
{
    return m_impl->failedModel();
}

bool rtu::ApplyChangesTask::inProgress() const
{
    const int totalChanges = totalChangesCount();
    return (totalChanges && (totalChanges != appliedChangesCount()));
}

int rtu::ApplyChangesTask::totalChangesCount() const
{
    return m_impl->totalChangesCount();
}

int rtu::ApplyChangesTask::appliedChangesCount() const
{
    return m_impl->appliedChangesCount();
}

int rtu::ApplyChangesTask::errorsCount() const
{
    return m_impl->failedModel()->changesCount();
}

void rtu::ApplyChangesTask::serverDiscovered(const rtu::BaseServerInfo &baseInfo)
{
    m_impl->serverDiscovered(baseInfo);
}

void rtu::ApplyChangesTask::serversDisappeared(const rtu::IDsVector &ids)
{
    m_impl->serversDisappeared(ids);
}

void rtu::ApplyChangesTask::unknownAdded(const QString &ip)
{
    m_impl->unknownAdded(ip);
}
