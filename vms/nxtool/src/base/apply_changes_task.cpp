
#include "apply_changes_task.h"

#include <set>

#include <QTimer>
#include <QElapsedTimer>
#include <QHostAddress>

#include <nx/mediaserver/api/client.h>
#include <base/changeset.h>
#include <base/selection.h>

#include <helpers/awaiting_op.h>
#include <helpers/time_helper.h>
#include <helpers/qml_helpers.h>

#include <models/changes_summary_model.h>

namespace api = nx::mediaserver::api;

namespace
{
    enum { kSingleInterfaceChangeCount = 1 };

    enum
    {
        kIfListRequestsPeriod = 2 * 1000

        /// Accepted wait time is 360 seconds due to restart on network interface parameters change
        , kRestartTimeout = 360 * 1000

        /// Long 30-minutes timeout for reset to factory defaults operation
        , kFactoryDefaultsTimeout = 30 * 60 * 1000

    };

    typedef std::function<void ()> Request;
    typedef std::function<bool (const api::BaseServerInfo &info)> ExtraPred;

    int changesFromUpdateInfos(const api::Client::ItfUpdateInfoContainer &itfUpdateInfos)
    {
        int result = 0;
        for (const api::Client::ItfUpdateInfo &updateInfo: itfUpdateInfos)
        {
            result += (updateInfo.useDHCP ? 1 : 0)
                + (updateInfo.ip ? 1 : 0)
                + (updateInfo.mask ? 1 : 0)
                + (updateInfo.dns ? 1 : 0)
                + (updateInfo.gateway ? 1 : 0);
        }
        return result;
    }

    api::InterfaceInfoList::const_iterator changeAppliedPos(const api::InterfaceInfoList &itf
        , const api::Client::ItfUpdateInfo &change)
    {
        const auto &it = std::find_if(itf.begin(), itf.end()
            , [change](const api::InterfaceInfo &info)
        {
            return (change.name == info.name);
        });

        if (it == itf.end())
            return it;

        const api::InterfaceInfo &info = *it;
        const bool applied =
            ((!change.useDHCP || (*change.useDHCP == (info.useDHCP == Qt::Checked)))
            && (!change.ip || (*change.ip == info.ip))
            && (!change.mask || (*change.mask == info.mask))
            && (!change.dns || (*change.dns == info.dns))
            && (!change.gateway || (*change.gateway == info.gateway)));

        return (applied ? it : itf.end());
    }

    bool changesWasApplied(const api::InterfaceInfoList &itf
        , const api::Client::ItfUpdateInfoContainer &changes)
    {
        for (const api::Client::ItfUpdateInfo &change: changes)
        {
            if (changeAppliedPos(itf, change) == itf.end())
                return false;
        }
        return true;
    }

    typedef QPair<api::BaseServerInfoPtr, api::ExtraServerInfo> ServerInfoCacheItem;
    typedef QVector<ServerInfoCacheItem> ServerInfosCache;

    ServerInfosCache makeServerInfosCache(const api::ServerInfoPtrContainer &pointers)
    {
        ServerInfosCache result;
        result.reserve(pointers.size());

        for(const api::ServerInfo *info: pointers)
        {
            if (!info->hasExtraInfo())
                continue;

            result.push_back(ServerInfoCacheItem(
                std::make_shared<api::BaseServerInfo>(info->baseInfo()), info->extraInfo()));
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

    api::Client::AffectedEntities calcAffected(const api::Client::ItfUpdateInfoContainer &changes)
    {
        api::Client::AffectedEntities affected = api::Client::kNoEntitiesAffected;

        for (const auto &change: changes)
        {
            if (change.useDHCP)
                affected |= api::Client::kDHCPUsageAffected;

            if (change.ip && !change.ip->isEmpty())
                affected |= api::Client::kIpAddressAffected;

            if (change.mask && !change.mask->isEmpty())
                affected |= api::Client::kMaskAffected;

            if (change.dns)
                affected |= api::Client::kDNSAffected;

            if (change.gateway)
                affected |= api::Client::kGatewayAffected;
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
            , api::Client::ItfUpdateInfoContainer &itfUpdateInfo)
            : inProgress(false)
            , timestamp(QDateTime::currentMSecsSinceEpoch())
        {
            /// Forces send dhcp on every call
            /// Forces send old ip (or mask) if new one hasn't been assigned
            const auto &extraInfo = item->second;
            const auto &interfaces = extraInfo.interfaces;
            for (auto &item: itfUpdateInfo)
            {
                const bool hasMaskChange = (!!item.mask);
                const bool hasIpChange = (!!item.ip);
                const bool fixMaskIp = (hasMaskChange != hasIpChange);
                if (!item.useDHCP || !(*item.useDHCP) || fixMaskIp)
                {
                    ///searching the specified in current existing
                    const auto it = std::find_if(interfaces.begin(), interfaces.end()
                        , [item](const api::InterfaceInfo &itf)
                    {
                        return (itf.name == item.name);
                    });

                    if (it != interfaces.end())
                    {
                        /// If we turned off dhcp force old values (if new were not presented)
                        if (item.useDHCP && !(*item.useDHCP))
                        {
                            if (!item.ip)
                               item.ip.reset(new QString(it->ip));
                            if (!item.mask)
                                item.mask.reset(new QString(it->mask));
                        }

                        if (!item.useDHCP)  /// Force dhcp old value anyway
                            item.useDHCP.reset(new bool(it->useDHCP == Qt::Checked));

                        /// We should force old value to ip (or mask) if only mask (or only ip) has changed
                        if (fixMaskIp)
                        {
                            if (hasMaskChange)
                            {
                                if (!item.ip)           /// Force old ip usage if mask was changed
                                    item.ip.reset(new QString(it->ip));
                            }
                            else if (!item.mask)        /// Force old mask usage if ip was changed
                            {
                                item.mask.reset(new QString(it->mask));
                            }
                        }
                    }
                    else
                        Q_ASSERT_X(false, Q_FUNC_INFO, "Interface name not found!");
                }
            }
        }
    };

    typedef std::shared_ptr<ItfChangeRequest> ItfChangeRequestPtr;
    typedef std::function<void (api::Client::ResultCode error
        , api::Client::AffectedEntities affected)> BeforeDeleteAction;

    typedef std::pair<rtu::AwaitingOp::Holder
        , BeforeDeleteAction> AwaitingOpData;
    typedef std::multimap<QUuid, AwaitingOpData> AwaitingOps;
    typedef std::function<void (const rtu::AwaitingOp::Holder op)> OpHandler;


    struct ItfChangeAwaitngOpData
    {
        QUuid initRuntimeId;
        ServerInfoCacheItem *item;
        rtu::AwaitingOp::WeakPtr weakOp;
        api::Client::ItfUpdateInfoContainer initialChanges;
        bool needRestartProperty;

        ItfChangeAwaitngOpData(const rtu::AwaitingOp::WeakPtr &initWeakOp
            , ServerInfoCacheItem *initItem
            , const api::Client::ItfUpdateInfoContainer &initInitialChanges
            , bool initNeedRestartProperty
            , const QUuid &runtimeId);
    };

    typedef std::shared_ptr<ItfChangeAwaitngOpData> ItfChangeAwaitngOpDataPtr;

    ItfChangeAwaitngOpData::ItfChangeAwaitngOpData(const rtu::AwaitingOp::WeakPtr &initWeakOp
        , ServerInfoCacheItem *initItem
        , const api::Client::ItfUpdateInfoContainer &initInitialChanges
        , bool initNeedRestartProperty
        , const QUuid &runtimeId)
        : initRuntimeId(runtimeId)
        , item(initItem)
        , weakOp(initWeakOp)
        , initialChanges(initInitialChanges)
        , needRestartProperty(initNeedRestartProperty)
    {
    }
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

    void applyOp(const QUuid &serverId
        , const OpHandler &handler);

    void serverDiscovered(const api::BaseServerInfo &baseInfo);

    void serversDisappeared(const rtu::IDsVector &ids);

    bool unknownAdded(const QString &ip);

private:
    void checkActionDiscoveredServer(const BaseServerInfo &baseInfo);

private:
    void onChangesetApplied(int changesCount);

    void sendRequests();

    api::Client::OperationCallback makeAwaitingRequestCallback(const AwaitingOp::WeakPtr &weakOp);

    void removeAwatingOp(const AwaitingOp::WeakPtr &op
        , api::Client::ResultCode error
        , api::Client::AffectedEntities affected);

    bool addRestartAuthAwaitingOp(const ServerInfoCacheItem &item
        , const QUuid &initRuntimeId
        , int changesCount);

    rtu::AwaitingOp::Holder addAwaitingOp(const QUuid &id
        , int changesCount
        , qint64 timeout
        , const BeforeDeleteAction &beforeDeleteAction
        , const api::Client::AffectedEntities affected);

    Request makeActionRequest(ServerInfoCacheItem &item
        , api::Constants::SystemCommand cmd
        , int changesCount
        , const ExtraPred &pred
        , const QString &description
        , int timeout
        , int falseTimeout);

    void addActions();

    void addDateTimeChangeRequests();

    void addSystemNameChangeRequests();

    void addPortChangeRequests();

    AwaitingOp::ServerDiscoveredAction makeItfChangeDiscoveredHandler(
        const ItfChangeAwaitngOpDataPtr data
        , const ItfChangeRequestPtr &itfChangeRequest
        , api::Client::AffectedEntities affected);

    Callback makeItfChangeDisappearedHandler(const ItfChangeAwaitngOpDataPtr &data);

    AwaitingOp::UnknownAddedHandler makeItfChangeUnknownAddedHandler(
        const ItfChangeAwaitngOpDataPtr &data);

    void addItfsChangeRequest(ServerInfoCacheItem &item
        , const api::Client::ItfUpdateInfoContainer &changes);

    void addIpChangeRequests();

    void addPasswordChangeRequests(bool forceNewLocalSystemId);

    bool addSummaryItem(const ServerInfoCacheItem &item
        , const QString &description
        , const QString &value
        , api::Client::ResultCode errorCode
        , api::Client::AffectedEntities checkFlags
        , api::Client::AffectedEntities affected);

    bool addUpdateInfoSummaryItem(const ServerInfoCacheItem &item
        , api::Client::ResultCode errorCode
        , const api::Client::ItfUpdateInfo &change
        , api::Client::AffectedEntities affected);

    typedef QHash<QUuid, ItfChangeRequest> ItfChangesContainer;

    void itfRequestSuccessfullyApplied(const ItfChangeAwaitngOpDataPtr &data);

    typedef std::weak_ptr<Impl> ThisWeakPtr;
    void checkReachability(const ThisWeakPtr &weak
        , const AwaitingOp::WeakPtr &weakOp
        , const ServerInfoCacheItem &item);

private:
    typedef QPair<bool, Request> RequestData;
    typedef QVector<RequestData>  RequestContainer;
    typedef std::set<QUuid> UuidsSet;

    QElapsedTimer m_msecsCounter;

    const QUuid m_id;
    const ChangesetPointer m_changeset;
    ApplyChangesTaskPtr m_owner;


    ServerInfosCache m_serversCache;
    const IDsVector m_targetServerIds;

    ChangesSummaryModel m_succesfulChangesModel;
    ChangesSummaryModel m_failedChangesModel;

    RequestContainer m_requests;
    AwaitingOps m_awaiting;
    UuidsSet m_restartAuthAwaitingIds;

    int m_appliedChangesIndex;
    int m_lastChangesetIndex;

    int m_totalChangesCount;
    int m_appliedChangesCount;
};

rtu::ApplyChangesTask::Impl::Impl(const ChangesetPointer &changeset
    , const ServerInfoPtrContainer &targets)

    : std::enable_shared_from_this<ApplyChangesTask::Impl>()
    , QObject()

    , m_msecsCounter()
    , m_id(QUuid::createUuid())
    , m_changeset(changeset)
    , m_owner()

    , m_serversCache(makeServerInfosCache(targets))
    , m_targetServerIds(extractIds(m_serversCache))

    , m_succesfulChangesModel(true, this)
    , m_failedChangesModel(false, this)

    , m_requests()
    , m_awaiting()
    , m_restartAuthAwaitingIds()

    , m_appliedChangesIndex(0)
    , m_lastChangesetIndex(0)

    , m_totalChangesCount(0)
    , m_appliedChangesCount(0)

{
    m_msecsCounter.start();
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

    const bool forceChangeLocalSystemId =
        m_changeset->systemName() && m_changeset->password();
    addPasswordChangeRequests(forceChangeLocalSystemId);
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

void rtu::ApplyChangesTask::Impl::applyOp(const QUuid &serverId
    , const OpHandler &handler)
{
    if (!handler)
        return;

    const auto range = (!serverId.isNull() ? m_awaiting.equal_range(serverId)
        : std::make_pair(m_awaiting.begin(), m_awaiting.end()));

    typedef std::vector<AwaitingOpData> TargetData;
    TargetData data;

    for (auto it = range.first; it != range.second; ++it)
        data.push_back(it->second);

    for (auto &opData: data)
        handler(opData.first);
}

void rtu::ApplyChangesTask::Impl::serverDiscovered(const api::BaseServerInfo &info)
{
    applyOp(info.id, [info](const AwaitingOp::Holder &op)
    {
        op->processServerDiscovered(info);
    });
}

void rtu::ApplyChangesTask::Impl::serversDisappeared(const rtu::IDsVector &ids)
{
    for (const QUuid &id: ids)
    {
        applyOp(id, [](const AwaitingOp::Holder &op)
        {
            op->processServerDisappeared();
        });
    }
}

bool rtu::ApplyChangesTask::Impl::unknownAdded(const QString &ip)
{
    for(const auto &awaiting: m_awaiting)
    {
        if (awaiting.second.first->processUnknownAdded(ip))
            return true;
    }
    return false;
}

void rtu::ApplyChangesTask::Impl::itfRequestSuccessfullyApplied(const ItfChangeAwaitngOpDataPtr &data)
{
    if (data->weakOp.expired())
        return;

    for(const auto &change: data->initialChanges)
    {
        addUpdateInfoSummaryItem(*data->item, api::Client::ResultCode::kSuccess
            , change, api::Client::kAllEntitiesAffected);
    }

    if (data->needRestartProperty)
    {
        const auto sharedOp = data->weakOp.lock();
        const int changesCount = sharedOp->changesCount();
        if (addRestartAuthAwaitingOp(*data->item, data->initRuntimeId, changesCount))
            sharedOp->resetChangesCount();
    }
    removeAwatingOp(data->weakOp, api::Client::ResultCode::kSuccess, api::Client::kAllEntitiesAffected);
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

api::Client::OperationCallback rtu::ApplyChangesTask::Impl::makeAwaitingRequestCallback(const AwaitingOp::WeakPtr &weakOp)
{
    const ThisWeakPtr weak = shared_from_this();
    const auto callback = [weakOp, weak]
        (api::Client::ResultCode error, api::Client::AffectedEntities affected)
    {
        if (weak.expired())
            return;

        if (error != api::Client::ResultCode::kSuccess)
        {
            const auto shared = weak.lock();
            shared->removeAwatingOp(weakOp, error, affected);
        }
    };

    return callback;
}

void rtu::ApplyChangesTask::Impl::removeAwatingOp(const AwaitingOp::WeakPtr &op
    , api::Client::ResultCode error
    , api::Client::AffectedEntities affected)
{
    if (op.expired())
        return;

    const auto sharedOp = op.lock();
    const auto id = sharedOp->serverId();

    const auto range = m_awaiting.equal_range(id);
    for (auto it = range.first; it != range.second; ++it)
    {
        const auto &data = (it->second);
        const auto &operation = data.first;
        if (operation != sharedOp)
            continue;

        if (data.second)
            data.second(error, affected);

        onChangesetApplied(data.first->changesCount());
        m_awaiting.erase(it);
        break;
    }
}

void rtu::ApplyChangesTask::Impl::checkReachability(const ThisWeakPtr &weak
    , const AwaitingOp::WeakPtr &weakOp
    , const ServerInfoCacheItem &item)
{
    /// Send after delay to give a chance to discover server in ServersFinder
    enum { kDiscoveryDelay = 15 * 1000 };
    QTimer::singleShot(kDiscoveryDelay, [weak, weakOp, &item]()
    {
        if (weak.expired())
            return;

        const auto success = [weak, weakOp, &item](const QUuid &id, const ExtraServerInfo &extraInfo)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            emit shared->m_owner->extraInfoUpdated(id, extraInfo, item.first->hostAddress);

            shared->removeAwatingOp(weakOp, api::Client::ResultCode::kSuccess, api::Client::kNoEntitiesAffected);
        };

        const auto failed = [weak, weakOp](const api::Client::ResultCode /* errorCode */
            , api::Client::AffectedEntities affectedEntities)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            shared->removeAwatingOp(weakOp, api::Client::ResultCode::kSuccess, affectedEntities);
        };

        api::Client::getServerExtraInfo(item.first, item.second.password, success, failed, api::Client::kDefaultTimeoutMs);
    });
}

bool rtu::ApplyChangesTask::Impl::addRestartAuthAwaitingOp(const ServerInfoCacheItem &item
    , const QUuid &initRuntimeId
    , int changesCount)
{
    const auto &serverId = item.first->id;
    const auto it = m_restartAuthAwaitingIds.find(serverId);
    if (it != m_restartAuthAwaitingIds.end())
    {
        /// Awaiting operation has been added already
        return false;
    }

    const ThisWeakPtr weak = shared_from_this();
    const auto request = [weak, serverId, initRuntimeId, changesCount, &item]()
    {
        if (weak.expired())
            return;

        const auto shared = weak.lock();
        const auto op = shared->addAwaitingOp(serverId, changesCount, kRestartTimeout
            , BeforeDeleteAction(), api::Client::kNoEntitiesAffected);

        const AwaitingOp::WeakPtr weakOp = op;
        const BoolPointer firstRequestProperty(new bool(true));
        const auto discovered = [weak, weakOp, initRuntimeId, firstRequestProperty, &item]
            (const BaseServerInfo &info)
        {
            if (weak.expired() || (info.runtimeId == initRuntimeId) || (!*firstRequestProperty))
                return;

            *firstRequestProperty = false;

            /// Server has restarted
            /// Sending authorization request

            const auto shared = weak.lock();
            shared->checkReachability(weak, weakOp, item);
        };

        op->setServerDiscoveredHandler(discovered);
    };

    m_requests.push_back(RequestData(kNonBlockingRequest, request));
    return true;
}


rtu::AwaitingOp::Holder rtu::ApplyChangesTask::Impl::addAwaitingOp(const QUuid &id
    , int changesCount
    , qint64 timeout
    , const BeforeDeleteAction &beforeDeleteAction
    , api::Client::AffectedEntities affected)
{
    const ThisWeakPtr weak = shared_from_this();
    const auto removeByTimeout = [weak, affected](const AwaitingOp::WeakPtr op)
    {
        if (weak.expired())
            return;

        const auto shared = weak.lock();
        shared->removeAwatingOp(op, api::Client::ResultCode::kRequestTimeout, affected);
    };

    const auto op = AwaitingOp::create(id, changesCount, timeout, removeByTimeout);
    m_awaiting.insert(std::make_pair(id, AwaitingOpData(op, beforeDeleteAction)));
    return op;
}

Request rtu::ApplyChangesTask::Impl::makeActionRequest(ServerInfoCacheItem &item
    , api::Constants::SystemCommand cmd
    , int changesCount
    , const ExtraPred &pred
    , const QString &description
    , int timeout
    , int falseTimeout)
{
    static const auto kOpAffected = api::Client::kNoEntitiesAffected;
    static const auto kOpValue = QString();

    const ThisWeakPtr weak = shared_from_this();

    const auto request = [&item, weak, cmd, pred, description, changesCount, falseTimeout, timeout]()
    {
        if (weak.expired())
            return;

        const auto id = item.first->id;
        const auto initialRuntimeId = item.first->runtimeId;
        const auto beforeRemoveHandler = [weak, &item, cmd, description]
            (api::Client::ResultCode error, api::Client::AffectedEntities /* affected */)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            shared->addSummaryItem(item, description, kOpValue, error
                , kOpAffected, kOpAffected);
        };


        const auto shared = weak.lock();

        const auto op = shared->addAwaitingOp(id, changesCount
            , timeout, beforeRemoveHandler, kOpAffected);


        const AwaitingOp::WeakPtr weakOp = op;
        const BoolPointer firstRequestProperty(new bool(true));
        const auto initialTime = shared->m_msecsCounter.elapsed();
        const auto discoveredHandler = [weak, weakOp, initialTime, initialRuntimeId
            , pred, firstRequestProperty, falseTimeout, &item]
            (const BaseServerInfo &info)
        {
            if (weak.expired())
                return;

            if (!*firstRequestProperty)
                return;

            const auto shared = weak.lock();
            if (info.runtimeId == initialRuntimeId)
            {
                if ((shared->m_msecsCounter.elapsed() - initialTime) > falseTimeout)
                {
                    *firstRequestProperty = false;

                    const auto shared = weak.lock();
                    shared->removeAwatingOp(weakOp, api::Client::ResultCode::kRequestTimeout, api::Client::kNoEntitiesAffected);
                }
            }
            else if (!pred || pred(info))
            {
                /// Server has restarted, thus we are waiting until it gets in normal (not safe) mode
                *firstRequestProperty = false;
                shared->checkReachability(weak, weakOp, item);
            }
        };

        op->setServerDiscoveredHandler(discoveredHandler);

        const auto &baseServerInfo = item.first;
        const auto &password = item.second.password;
        const auto &callback = shared->makeAwaitingRequestCallback(op);

        api::Client::sendSystemCommand(baseServerInfo, password, cmd, callback);
    };

    return request;
}

void rtu::ApplyChangesTask::Impl::addActions()
{
    const auto cmd = (m_changeset->softRestart() ? api::Constants::RestartServerCmd :
        (m_changeset->osRestart() ? api::Constants::RebootCmd :
        (m_changeset->factoryDefaults() ? api::Constants::FactoryDefaultsCmd :
        (m_changeset->factoryDefaultsButNetwork() ? api::Constants::FactoryDefaultsButNetworkCmd
            : api::Constants::NoCommands))));

    if (cmd == api::Constants::NoCommands)
        return;

    static const auto kRestartOpDescription = QStringLiteral("restart operation");
    static const auto kRebootOpDescription = QStringLiteral("reboot OS operation");
    static const auto kButNetworkRequest = QStringLiteral("reset to factory defaults\n but network operation");
    static const auto kFactoryDefautls = QStringLiteral("reset to factory defaults");

    const QString &description = (cmd == api::Constants::RestartServerCmd ? kRestartOpDescription :
        (cmd == api::Constants::RebootCmd ? kRebootOpDescription :
        (cmd == api::Constants::FactoryDefaultsCmd ? kFactoryDefautls : kButNetworkRequest)));

    static const ExtraPred restartExtraPred = ExtraPred(); /// We don't use extra predicate to check simple restart
    static const ExtraPred factoryExtraPred = [](const BaseServerInfo &info) { return !info.safeMode; };

    const ExtraPred extraPred = ((cmd == api::Constants::RestartServerCmd) || (cmd == api::Constants::RebootCmd)
        ? restartExtraPred : factoryExtraPred);

    const int timeout = ((cmd == api::Constants::RestartServerCmd) || (cmd == api::Constants::RebootCmd) ?
        kRestartTimeout : kFactoryDefaultsTimeout);

    enum
    {
        kFalseRestartTimeout = 60 * 1000
        , kFalseRestoreTimeout = kFactoryDefaultsTimeout / 2
    };
    const int falseTimeout = ((cmd == api::Constants::RestartServerCmd) || (cmd == api::Constants::RebootCmd) ?
        kFalseRestartTimeout:  kFalseRestoreTimeout);

    for (ServerInfoCacheItem &item: m_serversCache)
    {
        enum { kOpChangesCount = 1};

        const auto request = makeActionRequest(item, cmd
            , kOpChangesCount, extraPred, description, timeout, falseTimeout);

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

    for (auto &item: m_serversCache)
    {
        enum { kDateTimeChangesetSize = 1};

        const auto initRuntimeId = item.first->runtimeId;
        const auto request = [weak, &item, change, timestampMs, initRuntimeId]()
        {
            if (weak.expired())
                return;

            const auto &callback = [weak, &item, change, timestampMs, initRuntimeId]
                (const api::Client::ResultCode errorCode, api::Client::AffectedEntities affected, bool needRestart)
            {
                static const QString kTimeDescription = "server time";
                static const QString kTimeTemplate("%1\n%2");

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                QString timeZoneName = timeZoneNameWithOffset(QTimeZone(change.timeZoneId), QDateTime::currentDateTime());

                QDateTime timeValue = convertUtcToTimeZone(change.utcDateTimeMs, QTimeZone(change.timeZoneId));
                const QString &timeStr = timeValue.toString(Qt::SystemLocaleLongDate);

                const auto val = kTimeTemplate.arg(timeStr, timeZoneName);
                const auto flags = (api::Client::kDateTimeAffected | api::Client::kTimeZoneAffected);
                const bool successful = shared->addSummaryItem(item, kTimeDescription
                    , val, errorCode, flags, affected);

                bool waitForRestart = false;
                if(successful)
                {
                    emit shared->m_owner->dateTimeUpdated(item.first->id
                        , change.utcDateTimeMs, change.timeZoneId, timestampMs);

                    ExtraServerInfo &extraInfo = item.second;
                    extraInfo.timestampMs = timestampMs;
                    extraInfo.timeZoneId = change.timeZoneId;
                    extraInfo.utcDateTimeMs = change.utcDateTimeMs;

                    if (needRestart)
                        waitForRestart = shared->addRestartAuthAwaitingOp(item, initRuntimeId, kDateTimeChangesetSize);
                }


                shared->onChangesetApplied(waitForRestart ? 0 : kDateTimeChangesetSize);
            };

            const auto shared = weak.lock();
            api::Client::sendSetTimeRequest(item.first, item.second.password
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
    for (auto &item: m_serversCache)
    {
        enum { kSystemNameChangesetSize = 1 };

        const auto initRuntimeId = item.first->runtimeId;
        const auto request = [weak, initRuntimeId, &item, systemName]()
        {
            if (weak.expired())
                return;

            const auto &callback = [weak, initRuntimeId, &item, systemName]
                (const api::Client::ResultCode errorCode, api::Client::AffectedEntities affected, bool needRestart)
            {
                static const QString &kSystemNameDescription = "system name";

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                bool waitForRestart = false;
                if(shared->addSummaryItem(item, kSystemNameDescription, systemName
                    , errorCode, api::Client::kSystemNameAffected, affected))
                {
                    emit shared->m_owner->systemNameUpdated(item.first->id, systemName);
                    item.first->systemName = systemName;

                    if (needRestart)
                        waitForRestart = shared->addRestartAuthAwaitingOp(item, initRuntimeId, kSystemNameChangesetSize);
                }

                shared->onChangesetApplied(waitForRestart ? 0 : kSystemNameChangesetSize);
            };

            const auto shared = weak.lock();
            api::Client::sendSetSystemNameRequest(item.first, item.second.password, systemName, callback);
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

    for (auto &item: m_serversCache)
    {
        enum { kPortChangesetCapacity = 1 };
        const auto initRuntimeId = item.first->runtimeId;
        const auto request = [weak, initRuntimeId, &item, port]()
        {
            if (weak.expired())
                return;

            const auto &callback = [weak, initRuntimeId, &item, port]
                (const api::Client::ResultCode errorCode, api::Client::AffectedEntities affected, bool needRestart)
            {
                static const QString kPortDescription = "port";

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                bool waitForRestart = false;
                if (shared->addSummaryItem(item, kPortDescription, QString::number(port)
                    , errorCode, api::Client::kPortAffected, affected))
                {
                    emit shared->m_owner->portUpdated(item.first->id, port);
                    item.first->port = port;

                    if (needRestart)
                        waitForRestart = shared->addRestartAuthAwaitingOp(item, initRuntimeId, kPortChangesetCapacity);
                }

                shared->onChangesetApplied(waitForRestart ? 0 : kPortChangesetCapacity);
            };


            const auto shared = weak.lock();
            api::Client::sendSetPortRequest(item.first, item.second.password, port, callback);
        };

        m_totalChangesCount += kPortChangesetCapacity;
        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

rtu::AwaitingOp::ServerDiscoveredAction rtu::ApplyChangesTask::Impl::makeItfChangeDiscoveredHandler(
    const ItfChangeAwaitngOpDataPtr data
    , const ItfChangeRequestPtr &itfChangeRequest
    , api::Client::AffectedEntities affected)
{
    const ThisWeakPtr weak = shared_from_this();
    const auto handler = [weak, data, itfChangeRequest, affected]
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

        const auto processCallback = [weak, data, itfChangeRequest, affected]
            (bool successful, const QUuid &id, const ExtraServerInfo &extraInfo)
        {
            if (weak.expired() || data->weakOp.expired())
                return;

            const auto shared = weak.lock();

            itfChangeRequest->inProgress = false;
            itfChangeRequest->timestamp = QDateTime::currentMSecsSinceEpoch();

            const bool totalApplied = changesWasApplied(extraInfo.interfaces, data->initialChanges);

            if (!successful || !totalApplied)
                return;

            for (const api::Client::ItfUpdateInfo &change: data->initialChanges)
            {
                const auto &it = changeAppliedPos(extraInfo.interfaces, change);
                const bool applied = (it != extraInfo.interfaces.end());
                const auto affected = (applied ? api::Client::kAllEntitiesAffected : api::Client::kNoEntitiesAffected);
                const auto code = (applied ? api::Client::ResultCode::kSuccess : api::Client::ResultCode::kUnspecified);
                shared->addUpdateInfoSummaryItem(*data->item, code, change, affected);
            }

            api::Client::ResultCode finalError = api::Client::ResultCode::kUnspecified;
            if (successful && totalApplied)
            {
                finalError = api::Client::ResultCode::kSuccess;
                emit shared->m_owner->extraInfoUpdated(
                    id, extraInfo, data->item->first->hostAddress);
            }


            const auto sharedOp = data->weakOp.lock();
            const int changesCount = sharedOp->changesCount();
            if (data->needRestartProperty && (finalError == api::Client::ResultCode::kSuccess)
                && shared->addRestartAuthAwaitingOp(*data->item, data->initRuntimeId, changesCount))
            {
                sharedOp->resetChangesCount();
            }

            shared->removeAwatingOp(data->weakOp, finalError, affected);
        };

        const auto &successful =
            [processCallback](const QUuid &id, const ExtraServerInfo &extraInfo)
        {
            processCallback(true, id, extraInfo);
        };

        const auto &failed =
            [processCallback, info](const api::Client::ResultCode /* errorCode */, int /* affected */)
        {
            processCallback(false, info.id, ExtraServerInfo());
        };

        BaseServerInfo &currentBase = *data->item->first;

        const auto currentHttpAccessMethod = currentBase.httpAccessMethod;
        currentBase = info;
        currentBase.httpAccessMethod = currentHttpAccessMethod;
        itfChangeRequest->inProgress = true;

        api::Client::getServerExtraInfo(data->item->first, data->item->second.password
            , successful, failed, kIfListRequestsPeriod);
    };

    return handler;
}

rtu::Callback rtu::ApplyChangesTask::Impl::makeItfChangeDisappearedHandler(
    const ItfChangeAwaitngOpDataPtr &data)
{
    const ThisWeakPtr weak = shared_from_this();

    const auto handler = [weak, data]()
    {
        /// TODO: extend logic for several interface changes
        if (weak.expired()
            || (data->initialChanges.size() != kSingleInterfaceChangeCount))
            return;

        bool isDHCPTurnOnChange = true;
        for (const auto &change: data->initialChanges)
        {
            isDHCPTurnOnChange &= (change.useDHCP && *change.useDHCP);
        }

        if (!isDHCPTurnOnChange)
            return;

        //// If server is disappeared and all changes are dhcp "on"
        const auto shared = weak.lock();
        shared->itfRequestSuccessfullyApplied(data);
    };

    return handler;
}

rtu::AwaitingOp::UnknownAddedHandler rtu::ApplyChangesTask::Impl::makeItfChangeUnknownAddedHandler(
    const ItfChangeAwaitngOpDataPtr &data)
{
    const ThisWeakPtr weak = shared_from_this();

    const auto handler = [weak, data]
        (const QString &ip) -> bool
    {
        if (weak.expired())
            return false;

        /// Searches for the change request
        const auto &it = std::find_if(data->initialChanges.begin(), data->initialChanges.end()
            , [ip](const api::Client::ItfUpdateInfo &updateInfo)
        {
            return ((!updateInfo.useDHCP || !*updateInfo.useDHCP) && updateInfo.ip && *updateInfo.ip == ip);
        });

        if (it == data->initialChanges.end())
        {
            return false;
        }

        /// We've found change request with dhcp "off" state and ip that is
        /// equals to ip of found unknown entity.
        /// So, we suppose that request to change ip is successfully applied

        const auto shared = weak.lock();
        shared->itfRequestSuccessfullyApplied(data);
        return true;
    };

    return handler;
}

void rtu::ApplyChangesTask::Impl::addItfsChangeRequest(ServerInfoCacheItem &item
    , const api::Client::ItfUpdateInfoContainer &initialChanges)
{
    const ThisWeakPtr weak = shared_from_this();

    const QUuid initRuntimeId = item.first->runtimeId;

    const auto affected = calcAffected(initialChanges);
    const auto changesCount = changesFromUpdateInfos(initialChanges);

    const auto &request =
        [weak, initRuntimeId, &item, initialChanges, changesCount, affected]()
    {
        if (weak.expired())
            return;

        auto augmentedChanges = initialChanges;
        const auto itfChangeRequest = ItfChangeRequestPtr(new ItfChangeRequest(&item, augmentedChanges));

        const auto beforeDeleteCallback = [weak, itfChangeRequest, initialChanges, &item]
            (api::Client::ResultCode errorCode, api::Client::AffectedEntities affected)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            if (errorCode != api::Client::ResultCode::kSuccess)
            {
                for (const auto &change: initialChanges)
                    shared->addUpdateInfoSummaryItem(item, errorCode, change, affected);
            }
        };

        const auto shared = weak.lock();

        const auto op = shared->addAwaitingOp(item.first->id, changesCount
            , kRestartTimeout, beforeDeleteCallback, affected);

        const bool needRestartProperty = false;
        const ItfChangeAwaitngOpDataPtr data(new ItfChangeAwaitngOpData(
            op, &item, initialChanges, needRestartProperty, initRuntimeId));

        op->setServerDiscoveredHandler(shared->makeItfChangeDiscoveredHandler(
            data, itfChangeRequest, affected));
        op->setServerDisappearedHandler(shared->makeItfChangeDisappearedHandler(data));
        op->setUnknownAddedHandler(shared->makeItfChangeUnknownAddedHandler(data));

        const auto failedCallback = [weak, data]
            (const api::Client::ResultCode errorCode, api::Client::AffectedEntities affected)
        {
            if (weak.expired())
                return;

            const auto shared = weak.lock();
            shared->removeAwatingOp(data->weakOp, errorCode, affected);
        };

        const auto callback = [failedCallback, data]
            (const api::Client::ResultCode errorCode, api::Client::AffectedEntities affected, bool needRestart)
        {
            if (errorCode != api::Client::ResultCode::kSuccess)
                failedCallback(errorCode , affected);
            else
                data->needRestartProperty = needRestart;
        };

        const auto baseInfo = item.first;
        const auto password = item.second.password;
        api::Client::sendChangeItfRequest(baseInfo, password, augmentedChanges, callback);
    };

    const auto shared = weak.lock();
    shared->m_totalChangesCount += changesCount;

    /// Request is non blocking due to restart on interface parameters change.
    // I.e. we send multiple unqueued requests and not wait until they complete before next send.
    shared->m_requests.push_back(RequestData(kNonBlockingRequest, request));
}

void rtu::ApplyChangesTask::Impl::addIpChangeRequests()
{
    if (!m_changeset->itfUpdateInfo() || m_changeset->itfUpdateInfo()->empty())
        return;

    const auto &updateInfos = m_changeset->itfUpdateInfo();
    const bool isForMultipleSelection = std::any_of(updateInfos->begin(), updateInfos->end()
        , [](const api::Client::ItfUpdateInfo &info)
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
            , "Can't apply changes on several servers with multiple or empty interfaces");

        if (cacheInfo.second.interfaces.empty())
            continue;

        api::Client::ItfUpdateInfo change(updateInfo);
        change.name = cacheInfo.second.interfaces.front().name;

        if (updateInfo.ip)
        {
            const QString newAddr = QHostAddress(initAddr + i).toString();
            change.ip.reset(new QString(newAddr));
        }

        addItfsChangeRequest(cacheInfo, api::Client::ItfUpdateInfoContainer(1, change));
    }
}

void rtu::ApplyChangesTask::Impl::addPasswordChangeRequests(bool forceNewLocalSystemId)
{
    if (!m_changeset->password())
        return;

    const QString &newPassword = *m_changeset->password();
    const ThisWeakPtr weak = shared_from_this();

    const QUuid newLocalSystemId = forceNewLocalSystemId ? QUuid::createUuid() : QUuid();
    for (auto &item: m_serversCache)
    {
        enum { kPasswordChangesetSize = 1 };
        const auto initRuntimeId = item.first->runtimeId;
        const auto request = [weak, initRuntimeId, &item, newPassword, newLocalSystemId]()
        {
            if (weak.expired())
                return;

            const auto finalCallback = [weak, initRuntimeId, &item, newPassword, newLocalSystemId]
                (const api::Client::ResultCode errorCode, api::Client::AffectedEntities affected, bool needRestart)
            {
                static const QString kPasswordDescription = "password";

                if (weak.expired())
                    return;

                const auto shared = weak.lock();
                bool waitForRestart = false;
                if (shared->addSummaryItem(item, kPasswordDescription, newPassword
                    , errorCode, api::Client::kPasswordAffected, affected))
                {
                    item.second.password = newPassword;
                    emit shared->m_owner->passwordUpdated(item.first->id, newPassword);

                    if (needRestart)
                        waitForRestart = shared->addRestartAuthAwaitingOp(item, initRuntimeId, kPasswordChangesetSize);
                }

                shared->onChangesetApplied(waitForRestart ? 0 : kPasswordChangesetSize);
            };

            const auto &callback = [weak, &item, newPassword, finalCallback, newLocalSystemId]
                (const api::Client::ResultCode errorCode, api::Client::AffectedEntities affected, bool needRestart)
            {
                if (weak.expired())
                    return;

                if ((errorCode != api::Client::ResultCode::kSuccess) && (errorCode != api::Client::ResultCode::kUnauthorized))
                {
                    finalCallback(errorCode, affected, needRestart);
                }
                else
                {
                    const auto shared = weak.lock();
                    api::Client::sendSetPasswordAndLocalIdRequest(item.first, item.second.password,
                        newPassword, newLocalSystemId, true, finalCallback);
                }
            };

            const auto shared = weak.lock();
            api::Client::sendSetPasswordAndLocalIdRequest(item.first, item.second.password,
                newPassword, newLocalSystemId, false, callback);
        };

        m_totalChangesCount += kPasswordChangesetSize;
        m_requests.push_back(RequestData(kBlockingRequest, request));
    }
}

bool rtu::ApplyChangesTask::Impl::addSummaryItem(const ServerInfoCacheItem &item
    , const QString &description
    , const QString &value
    , api::Client::ResultCode errorCode
    , api::Client::AffectedEntities checkFlags
    , api::Client::AffectedEntities affected)
{
    const bool successResult = (errorCode == api::Client::ResultCode::kSuccess) &&
        ((affected & checkFlags) == checkFlags);
    ChangesSummaryModel * const model = (successResult ? &m_succesfulChangesModel : &m_failedChangesModel);

    const auto codeToString = [](api::Client::ResultCode code) -> QString
    {
        switch(code)
        {
        case api::Client::ResultCode::kSuccess:
            return QString();
        case api::Client::ResultCode::kRequestTimeout:
            return tr("Request Timeout");
        case api::Client::ResultCode::kUnauthorized:
            return tr("Unauthorized");
        case api::Client::ResultCode::kInternalAppError:
            return tr("Internal error");
        case api::Client::ResultCode::kInvalidParam:
            return tr("Invalid parameter");
        default:
            return tr("Unknown error");
        }
    };

    model->addRequestResult(api::ServerInfo(*item.first, item.second)
        , description, value, codeToString(errorCode));
    return successResult;
}

bool rtu::ApplyChangesTask::Impl::addUpdateInfoSummaryItem(const ServerInfoCacheItem &item
    , api::Client::ResultCode errorCode
    , const api::Client::ItfUpdateInfo &change
    , api::Client::AffectedEntities affected)
{
    bool success = (errorCode == api::Client::ResultCode::kSuccess);

    if (change.useDHCP)
    {
        static const QString &kDHCPDescription = "dhcp";
        static const QString &kYes = "on";
        static const QString &kNo = "off";
        const QString useDHCPValue = (*change.useDHCP? kYes : kNo);
        success &= addSummaryItem(item, kDHCPDescription, useDHCPValue, errorCode
            , api::Client::kDHCPUsageAffected, affected);
    }

    if (change.ip)
    {
        static const QString &kIpDescription = "ip";
        success &= addSummaryItem(item, kIpDescription, *change.ip, errorCode
            , api::Client::kIpAddressAffected, affected);
    }

    if (change.mask)
    {
        static const QString &kMaskDescription = "mask";
        success &= addSummaryItem(item, kMaskDescription, *change.mask, errorCode
            , api::Client::kMaskAffected, affected);
    }

    if (change.dns)
    {
        static const QString &kDnsDescription = "dns";
        success &= addSummaryItem(item, kDnsDescription, *change.dns, errorCode
            , api::Client::kDNSAffected, affected);
    }

    if (change.gateway)
    {
        static const QString &kGatewayDescription = "gateway";
        success &= addSummaryItem(item, kGatewayDescription, *change.gateway, errorCode
            , api::Client::kGatewayAffected, affected);
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

void rtu::ApplyChangesTask::serverDiscovered(const api::BaseServerInfo &baseInfo)
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
