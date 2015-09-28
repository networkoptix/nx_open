
#include "apply_changes_task.h"

#include <QTimer>
#include <QHostAddress>

#include <base/types.h>
#include <base/requests.h>
#include <base/changeset.h>
#include <base/selection.h>

#include <helpers/awaiting_op.h>
#include <helpers/time_helper.h>
#include <helpers/qml_helpers.h>

#include <models/changes_summary_model.h>

namespace
{
    enum { kSingleInterfaceChangeCount = 1 };

    enum 
    {
        kIfListRequestsPeriod = 2 * 1000

        /// Accepted wait time is 300 seconds due to restart on network interface parameters change
        , kRestartTimeout = 300 * 1000

        /// Long 30-minutes timeout for reset to factory defautls operation
        , kFactoryDefaultsTimeout = 30 * 60 * 1000

    };

    typedef std::function<void ()> Request;
    typedef std::function<bool (const rtu::BaseServerInfo &info)> ExtraPred;

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

    rtu::Constants::AffectedEntities calcAffected(const rtu::ItfUpdateInfoContainer &changes)
    {
        rtu::Constants::AffectedEntities affected = rtu::Constants::kNoEntitiesAffected;

        for (const auto &change: changes)
        {   
            if (change.useDHCP)
                affected |= rtu::Constants::kDHCPUsageAffected;
        
            if (change.ip && !change.ip->isEmpty())
                affected |= rtu::Constants::kIpAddressAffected;
        
            if (change.mask && !change.mask->isEmpty())
                affected |= rtu::Constants::kMaskAffected;
        
            if (change.dns)
                affected |= rtu::Constants::kDNSAffected;
        
            if (change.gateway)
                affected |= rtu::Constants::kGatewayAffected;       
        }
        return affected;
    }
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
        qint64 timestamp;

        ItfChangeRequest(ServerInfoCacheItem *item
            , rtu::ItfUpdateInfoContainer &itfUpdateInfo)
            : inProgress(false)
            , timestamp(QDateTime::currentMSecsSinceEpoch())
        {
            /// Forces send dhcp on every call
            /// Forces send old ip if new one hasn't been assigned
            const auto &extraInfo = item->second;
            const auto &interfaces = extraInfo.interfaces;
            for (auto &item: itfUpdateInfo)
            {
                const bool hasMaskChange = (!!item.mask);
                const bool hasIpChange = (!!item.ip);
                const bool fixMaskIp = (hasMaskChange != hasIpChange);
                if (!item.useDHCP || fixMaskIp)
                {
                    ///searching the specified in current existing
                    const auto it = std::find_if(interfaces.begin(), interfaces.end()
                        , [item](const rtu::InterfaceInfo &itf)
                    {
                        return (itf.name == item.name);
                    });

                    if (it != interfaces.end())
                    {
                        if (!item.useDHCP)  /// Force dhcp old value
                            item.useDHCP.reset(new bool(it->useDHCP == Qt::Checked));

                        if (fixMaskIp)
                        {
                            if (hasMaskChange)              /// Force old ip usage if mask was changed
                               item.ip.reset(new QString(it->ip));
                            else                            /// Force old mask usage if ip was changed
                                item.mask.reset(new QString(it->mask));
                        }
                    }
                    else
                        Q_ASSERT_X(false, Q_FUNC_INFO, "Interface name not found!");
                }
            }
        }
    };

    typedef std::shared_ptr<ItfChangeRequest> ItfChangeRequestPtr;
    typedef std::function<void (rtu::RequestError error
        , rtu::Constants::AffectedEntities affected)> BeforeDeleteAction;
    typedef QPair<rtu::AwaitingOp::Holder, BeforeDeleteAction> AwaitingOpData;
    typedef QHash<QUuid, AwaitingOpData> AwaitingOps;
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

    const QUuid &id() const;

    QString progressPageHeaderText() const;

    QString minimizedText() const;

    ChangesSummaryModel *successfulModel();

    ChangesSummaryModel *failedModel();

    int totalChangesCount() const;
        
    int appliedChangesCount() const;

    void serverDiscovered(const rtu::BaseServerInfo &baseInfo);

    void serversDisappeared(const rtu::IDsVector &ids);

    bool unknownAdded(const QString &ip);

private:
    void checkActionDiscoveredServer(const BaseServerInfo &baseInfo);

private:
    void onChangesetApplied(int changesCount);

    void sendRequests();

    rtu::OperationCallback makeAwaitingRequestCallback(const QUuid &id);

    void removeAwatingOp(const QUuid &id
        , RequestError error
        , Constants::AffectedEntities affected);

    rtu::AwaitingOp::Holder addAwaitingOp(const QUuid &id
        , int changesCount
        , qint64 timeout
        , const AwaitingOp::ServerDiscoveredAction &discovered
        , const Callback &disappeared
        , const AwaitingOp::UnknownAddedHandler &unknownAdded
        , const BeforeDeleteAction &beforeDeleteAction
        , const Constants::AffectedEntities affected);

    Request makeActionRequest(ServerInfoCacheItem &item
        , Constants::AvailableSysCommand cmd
        , int changesCount
        , const ExtraPred &pred
        , const QString &description
        , int timeout);

    void addActions();

    void addDateTimeChangeRequests();

    void addSystemNameChangeRequests();
    
    void addPortChangeRequests();
    
    AwaitingOp::ServerDiscoveredAction makeItfChangeDiscoveredHandler(
        ServerInfoCacheItem &item
        , const ItfChangeRequestPtr &itfChangeRequest
        , const ItfUpdateInfoContainer &initialChanges
        , Constants::AffectedEntities affected);

    Callback makeItfChangeDisappearedHandler(const ServerInfoCacheItem &item
        , const rtu::ItfUpdateInfoContainer &initialChanges);
    
    AwaitingOp::UnknownAddedHandler makeItfChangeUnknownAddedHandler(
        const ServerInfoCacheItem &item
        , const ItfUpdateInfoContainer &initialChanges);

    void addItfsChangeRequest(ServerInfoCacheItem &item
        , const ItfUpdateInfoContainer &changes);

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
    
    void itfRequestSuccessfullyApplied(const rtu::ItfUpdateInfoContainer &initialChanges
        , const ServerInfoCacheItem &item);

private:
    typedef QPair<bool, Request> RequestData;
    typedef QVector<RequestData>  RequestContainer;
    typedef std::weak_ptr<Impl> ThisWeakPtr;

    const QUuid m_id;
    const ChangesetPointer m_changeset;
    ApplyChangesTaskPtr m_owner;


    ServerInfosCache m_serversCache;
    const IDsVector m_targetServerIds;

    ChangesSummaryModel m_succesfulChangesModel;
    ChangesSummaryModel m_failedChangesModel;
    
    RequestContainer m_requests;
    AwaitingOps m_awaiting;

    int m_appliedChangesIndex;
    int m_lastChangesetIndex;

    int m_totalChangesCount;
    int m_appliedChangesCount;
};

rtu::ApplyChangesTask::Impl::Impl(const ChangesetPointer &changeset
    , const ServerInfoPtrContainer &targets)
    
    : std::enable_shared_from_this<ApplyChangesTask::Impl>()
    , QObject()

    , m_id(QUuid::createUuid())
    , m_changeset(changeset)
    , m_owner()

    , m_serversCache(makeServerInfosCache(targets))
    , m_targetServerIds(extractIds(m_serversCache))
    
    , m_succesfulChangesModel(true, this)
    , m_failedChangesModel(false, this)
    
    , m_requests()
    , m_awaiting()
    
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

    addActions();
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

const QUuid &rtu::ApplyChangesTask::Impl::id() const
{
    return m_id;
}

QString rtu::ApplyChangesTask::Impl::progressPageHeaderText() const
{
    return m_changeset->getProgressText();
}

QString rtu::ApplyChangesTask::Impl::minimizedText() const
{
    return m_changeset->getMinimizedProgressText();
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
    /// Awating operations processor
    const auto it = m_awaiting.find(info.id);
    if (it != m_awaiting.end())
    {
        const auto awatingOp = it.value().first;
        awatingOp->processServerDiscovered(info);
    }
}

void rtu::ApplyChangesTask::Impl::serversDisappeared(const rtu::IDsVector &ids)
{
    //// TODO: add awaiting operation handling

    for (const QUuid &id: ids)
    {
        const auto it = m_awaiting.find(id);
        if (it == m_awaiting.end())
            continue;

        const auto awaitingOp = it.value().first;
        awaitingOp->processServersDisappeared();
    }
}

bool rtu::ApplyChangesTask::Impl::unknownAdded(const QString &ip)
{
    for(const auto &awaiting: m_awaiting)
    {
        if (awaiting.first->processUnknownAdded(ip))
            return true;
    }
    return false;
}

void rtu::ApplyChangesTask::Impl::itfRequestSuccessfullyApplied(
    const rtu::ItfUpdateInfoContainer &initialChanges
    , const ServerInfoCacheItem &item)
{
    for(const auto &change: initialChanges)
    {
        addUpdateInfoSummaryItem(item, RequestError::kSuccess
            , change, Constants::kAllEntitiesAffected);
    }

    removeAwatingOp(item.first->id, RequestError::kSuccess, Constants::kAllEntitiesAffected);
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

rtu::OperationCallback rtu::ApplyChangesTask::Impl::makeAwaitingRequestCallback(const QUuid &id)
{
    const ThisWeakPtr weak = shared_from_this();
    const auto callback = [id, weak]
        (RequestError error, Constants::AffectedEntities affected)
    {
        if (weak.expired())
            return;

        if (error != RequestError::kSuccess)
        {
            const auto shared = weak.lock();
            shared->removeAwatingOp(id, error, affected);
        }
    };

    return callback;
}

void rtu::ApplyChangesTask::Impl::removeAwatingOp(const QUuid &id
    , RequestError error
    , Constants::AffectedEntities affected)
{
    const auto itOp = m_awaiting.find(id);
    if (itOp == m_awaiting.end())
        return;

    const auto &data = (*itOp);
    if (data.second)
        data.second(error, affected);

    onChangesetApplied(data.first->changesCount());
    m_awaiting.erase(itOp);
}

rtu::AwaitingOp::Holder rtu::ApplyChangesTask::Impl::addAwaitingOp(const QUuid &id
    , int changesCount
    , qint64 timeout
    , const AwaitingOp::ServerDiscoveredAction &discovered
    , const Callback &disappeared
    , const AwaitingOp::UnknownAddedHandler &unknownAdded
    , const BeforeDeleteAction &beforeDeleteAction
    , Constants::AffectedEntities affected)
{
    const ThisWeakPtr weak = shared_from_this(); 
    const auto removeByTimeout = [weak, affected](const QUuid &id)
    {
        if (weak.expired())
            return;

        const auto shared = weak.lock();
        shared->removeAwatingOp(id, RequestError::kRequestTimeout, affected);
    };

    const auto op = AwaitingOp::create(id, changesCount, timeout
        , discovered, disappeared, unknownAdded, removeByTimeout);
    m_awaiting.insert(id, AwaitingOpData(op, beforeDeleteAction));
    return op;
}

Request rtu::ApplyChangesTask::Impl::makeActionRequest(ServerInfoCacheItem &item
    , Constants::AvailableSysCommand cmd
    , int changesCount
    , const ExtraPred &pred
    , const QString &description
    , int timeout)
{
    static const auto kOpAffected = Constants::kAllEntitiesAffected;
    static const auto kOpValue = QString();

    const ThisWeakPtr weak = shared_from_this();

    const auto request = [&item, weak, cmd, pred, description, changesCount, timeout]()
    {

        if (weak.expired())
            return;

        const auto id = item.first->id;
        const auto initialRuntimeId = item.first->runtimeId;

        const auto discoveredHandler = [weak, id, initialRuntimeId, pred] (const BaseServerInfo &info)
        {
            if (weak.expired())
                return;

            /// Server has restarted, thus we are waiting until it gets in normal (not safe) mode
            if ((info.runtimeId != initialRuntimeId) && (!pred || pred(info))) 
                                                        
            {
                const auto shared = weak.lock();
                shared->removeAwatingOp(id, RequestError::kSuccess, kOpAffected);
            }
        };


        const auto beforeRemoveHandler = [weak, &item, cmd, description]
            (RequestError error, Constants::AffectedEntities /* affected */)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            shared->addSummaryItem(item, description, kOpValue, error
                , kOpAffected, kOpAffected);
        };
    
        
        const auto shared = weak.lock();
        
        shared->addAwaitingOp(id, changesCount, timeout, discoveredHandler
            , Callback(), AwaitingOp::UnknownAddedHandler(), beforeRemoveHandler, kOpAffected);

        const auto &baseServerInfo = item.first;
        const auto &password = item.second.password;
        const auto &callback = shared->makeAwaitingRequestCallback(id);

        sendSystemCommand(baseServerInfo, password, cmd, callback);
    };

    return request;
}

void rtu::ApplyChangesTask::Impl::addActions()
{
    const auto cmd = (m_changeset->softRestart() ? Constants::RestartServerCmd : 
        (m_changeset->osRestart() ? Constants::RebootCmd : 
        (m_changeset->factoryDefaults() ? Constants::FactoryDefaultsCmd :
        (m_changeset->factoryDefaultsButNetwork() ? Constants::FactoryDefaultsButNetworkCmd
            : Constants::NoCommands))));

    if (cmd == Constants::NoCommands)
        return;

    static const auto kRestartOpDescription = QStringLiteral("Restart operation");
    static const auto kRebootOpDescription = QStringLiteral("Reboot OS operation");
    static const auto kButNetworkRequest = QStringLiteral("Reset to factory defaults\n but network operation");
    static const auto kFactoryDefautls = QStringLiteral("Reset to factory defaults");

    const QString &description = (cmd == Constants::RestartServerCmd ? kRestartOpDescription : 
        (cmd == Constants::RebootCmd ? kRebootOpDescription :
        (cmd == Constants::FactoryDefaultsCmd ? kFactoryDefautls : kButNetworkRequest)));

    static const ExtraPred restartExtraPred = ExtraPred(); /// We don't use extra predicate to check simple restart
    static const ExtraPred factoryExtraPred = [](const BaseServerInfo &info) { return !info.safeMode; };

    const ExtraPred extraPred = ((cmd == Constants::RestartServerCmd) || (cmd == Constants::RebootCmd) 
        ? restartExtraPred : factoryExtraPred);

    const int timeout = ((cmd == Constants::RestartServerCmd) || (cmd == Constants::RebootCmd) ? 
        kRestartTimeout : kFactoryDefaultsTimeout);

    const ThisWeakPtr weak = shared_from_this();
    for (ServerInfoCacheItem &item: m_serversCache)
    {
        enum { kOpChangesCount = 1};

        const auto request = makeActionRequest(item, cmd
            , kOpChangesCount, extraPred, description, timeout);

        m_totalChangesCount += kOpChangesCount;
        m_requests.push_back(RequestData(kNonBlockingRequest, request));
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
        enum { kDateTimeChangesetSize = 1};
        const auto request = [weak, &info, change, timestampMs]()
        {
            if (weak.expired())
                return;

            const auto &callback = [weak, &info, change, timestampMs]
                (const RequestError errorCode, Constants::AffectedEntities affected)
            {
                static const QString kTimeDescription = "Server time";
                static const QString kTimeTemplate("%1\n%2");

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                QString timeZoneName = timeZoneNameWithOffset(QTimeZone(change.timeZoneId), QDateTime::currentDateTime());

                QDateTime timeValue = convertUtcToTimeZone(change.utcDateTimeMs, QTimeZone(change.timeZoneId));
                const QString &timeStr = timeValue.toString(Qt::SystemLocaleLongDate);

                const auto flags = (Constants::kDateTimeAffected | Constants::kTimeZoneAffected);
                const bool successful = shared->addSummaryItem(info, kTimeDescription
                    , kTimeTemplate.arg(timeStr, timeZoneName), errorCode, flags, affected);

                if (successful)
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

rtu::AwaitingOp::ServerDiscoveredAction rtu::ApplyChangesTask::Impl::makeItfChangeDiscoveredHandler(
    ServerInfoCacheItem &item
    , const ItfChangeRequestPtr &itfChangeRequest
    , const rtu::ItfUpdateInfoContainer &initialChanges
    , Constants::AffectedEntities affected)
{
    const ThisWeakPtr weak = shared_from_this();

    const auto handler = [weak, &item, itfChangeRequest, initialChanges, affected]
        (const BaseServerInfo &info)
    {
        if (weak.expired())
            return;

        const auto shared = weak.lock();
        if (shared->unknownAdded(info.hostAddress))
            return;

        if (itfChangeRequest->inProgress)
            return;

        const qint64 current = QDateTime::currentMSecsSinceEpoch();
        if ((current - itfChangeRequest->timestamp) < kIfListRequestsPeriod)
            return;

        const auto processCallback = [weak, &item, itfChangeRequest, initialChanges, affected]
            (bool successful, const QUuid &id, const ExtraServerInfo &extraInfo)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
        
            itfChangeRequest->inProgress = false;
            itfChangeRequest->timestamp = QDateTime::currentMSecsSinceEpoch();
        
            const bool totalApplied = changesWasApplied(extraInfo.interfaces, initialChanges);
        
            if (!successful || !totalApplied)
                return;

            for (const ItfUpdateInfo &change: initialChanges)
            {
                const auto &it = changeAppliedPos(extraInfo.interfaces, change);
                const bool applied = (it != extraInfo.interfaces.end());
                const auto affected = (applied ? Constants::kAllEntitiesAffected : Constants::kNoEntitiesAffected);
                const auto code = (applied ? RequestError::kSuccess : RequestError::kUnspecified);
                shared->addUpdateInfoSummaryItem(item, code, change, affected);
            }

            RequestError finalError = RequestError::kUnspecified;
            if (successful && totalApplied)
            {
                finalError = RequestError::kSuccess;
                emit shared->m_owner->extraInfoUpdated(
                    id, extraInfo, item.first->hostAddress);
            }

            shared->removeAwatingOp(id, finalError, affected);
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
    
        BaseServerInfo &currentBase = *item.first;

        const bool currentDiscoverByHttp = currentBase.accessibleByHttp;
        currentBase = info;
        currentBase.accessibleByHttp = currentDiscoverByHttp;
        itfChangeRequest->inProgress = true;
    
        getServerExtraInfo(item.first, item.second.password
            , successful, failed, kIfListRequestsPeriod);
    };

    return handler;
}

rtu::Callback rtu::ApplyChangesTask::Impl::makeItfChangeDisappearedHandler(
    const ServerInfoCacheItem &item
    , const rtu::ItfUpdateInfoContainer &initialChanges)
{
    const ThisWeakPtr weak = shared_from_this();

    const auto handler = [weak, &item, initialChanges]()
    {
        /// TODO: extend logic for several interface changes
        if (weak.expired() 
            || (initialChanges.size() != kSingleInterfaceChangeCount))
            return;

        bool isDHCPTurnOnChange = true;
        for (const auto &change: initialChanges)
        {
            isDHCPTurnOnChange &= (change.useDHCP && *change.useDHCP);
        }
    
        if (!isDHCPTurnOnChange)
            return;

        //// If server is disappeared and all changes are dhcp "on"
        const auto shared = weak.lock();
        shared->itfRequestSuccessfullyApplied(initialChanges, item);
    };

    return handler;
}

rtu::AwaitingOp::UnknownAddedHandler rtu::ApplyChangesTask::Impl::makeItfChangeUnknownAddedHandler(
    const ServerInfoCacheItem &item
    , const ItfUpdateInfoContainer &initialChanges)
{
    const ThisWeakPtr weak = shared_from_this();

    const auto handler = [weak, &item, initialChanges](const QString &ip) -> bool
    {
        if (weak.expired())
            return false;

        /// Searches for the change request
        const auto &it = std::find_if(initialChanges.begin(), initialChanges.end()
            , [ip](const ItfUpdateInfo &updateInfo) 
        {
            return ((!updateInfo.useDHCP || !*updateInfo.useDHCP) && updateInfo.ip && *updateInfo.ip == ip);
        });

        if (it == initialChanges.end())
        {
            return false;
        }

        /// We've found change request with dhcp "off" state and ip that is 
        /// equals to ip of found unknown entity.
        /// So, we suppose that request to change ip is successfully applied

        const auto shared = weak.lock();
        shared->itfRequestSuccessfullyApplied(initialChanges, item);
        return true;
    };

    return handler;
}

void rtu::ApplyChangesTask::Impl::addItfsChangeRequest(ServerInfoCacheItem &item
                                                       , const ItfUpdateInfoContainer &initialChanges)
{
    const ThisWeakPtr weak = shared_from_this();

    const QUuid id = item.first->id;

    const auto affected = calcAffected(initialChanges);
    const auto changesCount = changesFromUpdateInfos(initialChanges);

    const auto &request = 
        [weak, id, &item, initialChanges, changesCount, affected]()
    {
        if (weak.expired())
            return;

        const auto failedCallback = [weak, id, &item, initialChanges]
            (const RequestError errorCode, Constants::AffectedEntities affected)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            shared->removeAwatingOp(id, errorCode, affected);
        };

        const auto callback = [failedCallback, weak, initialChanges, item]
            (const RequestError errorCode, Constants::AffectedEntities affected)
        {
            if (errorCode != RequestError::kSuccess)
                failedCallback(errorCode , affected);
        };

        auto augmentedChanges = initialChanges;
        const auto itfChangeRequest = ItfChangeRequestPtr(new ItfChangeRequest(&item, augmentedChanges));

        const auto beforeDeleteCallback = [weak, itfChangeRequest, initialChanges, &item]
            (RequestError errorCode, Constants::AffectedEntities affected)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            if (errorCode != RequestError::kSuccess)
            {
                for (const auto &change: initialChanges)
                    shared->addUpdateInfoSummaryItem(item, errorCode, change, affected);
            }
        };

        const auto shared = weak.lock();
        const auto discoveredHandler = shared->makeItfChangeDiscoveredHandler(
            item, itfChangeRequest, initialChanges, affected);

        const auto disappearedHandler = shared->makeItfChangeDisappearedHandler(
            item, initialChanges);
        const auto unknownAdded = shared->makeItfChangeUnknownAddedHandler(
            item, initialChanges);

        shared->addAwaitingOp(id, changesCount, kRestartTimeout, discoveredHandler
            , disappearedHandler, unknownAdded, beforeDeleteCallback, affected);

        const auto baseInfo = item.first;
        const auto password = item.second.password;
        sendChangeItfRequest(baseInfo, password, augmentedChanges, callback);
    };
        
    const auto shared = weak.lock();
    shared->m_totalChangesCount += changesCount;

    /// Request is non blocking due to restart on interface parameters change. 
    // I.e. we send multiple unqued requests and not wait until they complete before next send.
    shared->m_requests.push_back(RequestData(kNonBlockingRequest, request));
}

void rtu::ApplyChangesTask::Impl::addIpChangeRequests()
{
    if (!m_changeset->itfUpdateInfo() || m_changeset->itfUpdateInfo()->empty())
        return;
    
    const auto &updateInfos = m_changeset->itfUpdateInfo();
    const bool isForMultipleSelection = std::any_of(updateInfos->begin(), updateInfos->end()
        , [](const ItfUpdateInfo &info)
    {
        return (info.name == Selection::kMultipleSelectionItfName);
    });

    enum { kSingleSelection = 1 };

    if (!isForMultipleSelection && (m_serversCache.size() == 1))
    {
        /// Allow multiple interface changes on single server
        addItfsChangeRequest(m_serversCache.first(), *updateInfos);
        return;
    }

    /// Now, there are servers with single interface only in m_serverCache

    Q_ASSERT_X(updateInfos->size() == 1, Q_FUNC_INFO, "Multiple interfaces changes on severals servers");

    const auto &updateInfo = updateInfos->front();

    quint32 initAddr = (updateInfo.ip ? QHostAddress(*updateInfo.ip).toIPv4Address() : 0);
    for (int i = 0; i != m_serversCache.size(); ++i)
    {
        auto &cacheInfo = m_serversCache[i];
        Q_ASSERT_X(cacheInfo.second.interfaces.size() == 1, Q_FUNC_INFO
            , "Can't apply changese on several servers with multiple or empty interfaces");
        
        if (cacheInfo.second.interfaces.empty())
            continue;

        ItfUpdateInfo change(updateInfo);
        change.name = cacheInfo.second.interfaces.front().name;

        if (updateInfo.ip)
        {
            const QString newAddr = QHostAddress(initAddr + i).toString();
            change.ip.reset(new QString(newAddr));
        }

        addItfsChangeRequest(cacheInfo, ItfUpdateInfoContainer(1, change));
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

                if ((errorCode != RequestError::kSuccess) && (errorCode != RequestError::kUnauthorized))
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
        case RequestError::kInternalAppError:
            return tr("Internal error");
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

const QUuid &rtu::ApplyChangesTask::id() const
{
    return m_impl->id();
}

QObject *rtu::ApplyChangesTask::successfulModel()
{
    return m_impl->successfulModel();
}

QObject *rtu::ApplyChangesTask::failedModel()
{
    return m_impl->failedModel();
}

QString rtu::ApplyChangesTask::progressPageHeaderText() const
{
    return m_impl->progressPageHeaderText();
}

QString rtu::ApplyChangesTask::minimizedText() const
{
    return m_impl->minimizedText();
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
