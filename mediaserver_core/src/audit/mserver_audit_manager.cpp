#include "mserver_audit_manager.h"

#include <api/common_message_processor.h>
#include <database/server_db.h>

#include "utils/common/synctime.h"
#include <nx/utils/log/assert.h>
#include <media_server/media_server_module.h>
#include <api/global_settings.h>

namespace
{

const qint64 GROUP_TIME_THRESHOLD = 1000ll * 30;
static const qint64 SESSION_KEEP_PERIOD_SECS = 3600ll * 24;
static const qint64 SHORT_SESSION_KEEP_PERIOD_SECS = 15;
static const qint64 SESSION_CLEANUP_INTERVAL_MS = 1000;

static const int kRecentlyRecordsCacheSize = 100;
static const int kRecentlyRecordsTimeGapSec = 5;
static const int kMaxOpenedConnections = 100 * 1000;

} // namespace


QnAuditRecord filteredRecord(QnAuditRecord record)
{
    if (!record.isLoginType())
        record.authSession.userAgent.clear(); // optimization. It used for login type only
    return record;
}

QnMServerAuditManager::QnMServerAuditManager(QnMediaServerModule* serverModule)
:
    nx::mediaserver::ServerModuleAware(serverModule),
    m_internalId(-1)
{
    const auto maxTime =
        std::chrono::duration_cast<std::chrono::seconds>(serverModule->lastRunningTime());
    serverModule->serverDb()->closeUnclosedAuditRecords((int)maxTime.count());

    const auto& settings = serverModule->commonModule()->globalSettings();
    m_sessionCleanupTimer.restart();
    m_enabled = settings->isAuditTrailEnabled();
    connect(settings, &QnGlobalSettings::auditTrailEnableChanged, this,
        [this]()
        {
            setEnabled(commonModule()->globalSettings()->isAuditTrailEnabled());
        });

    m_internalId = serverModule->serverDb()->auditRecordMaxId();
    connect(&m_timer, &QTimer::timeout, this, &QnMServerAuditManager::at_timer);
    m_timer.start(1000 * 5);
}

QnMServerAuditManager::~QnMServerAuditManager()
{
    m_timer.stop();
}

QnAuditRecord QnMServerAuditManager::CameraPlaybackInfo::toAuditRecord() const
{
    Qn::AuditRecordType eventType;
    if (isExport)
        eventType = Qn::AR_ExportVideo;
    else if (startTimeUsec == DATETIME_NOW)
        eventType = Qn::AR_ViewLive;
    else
        eventType = Qn::AR_ViewArchive;

    QnAuditRecord record = QnAuditManager::prepareRecord(session, eventType);
    record.createdTimeSec = creationTimeMs / 1000;
    record.rangeStartSec = period.startTimeMs / 1000;
    record.rangeEndSec = period.endTimeMs() / 1000;
    record.resources.push_back(cameraId);
    return record;
}

QnAuditRecord QnMServerAuditManager::ChangedSettingInfo::toAuditRecord() const
{
    Qn::AuditRecordType eventType;
    if (paramName.startsWith(lit("email")) || paramName.startsWith(lit("smtp")))
        eventType = Qn::AR_EmailSettings;
    else
        eventType = Qn::AR_SettingsChange;

    return QnAuditManager::prepareRecord(session, eventType);
}

void QnMServerAuditManager::cleanupExpiredSessions()
{
    qint64 currentTimeSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    for (auto itr = m_openedConnections.begin(); itr != m_openedConnections.end();)
    {
        AuditConnection& connection = itr.value();
        auto maxTimeoutSec = connection.record.authSession.isAutoGenerated
            ? SHORT_SESSION_KEEP_PERIOD_SECS : SESSION_KEEP_PERIOD_SECS;
        if (currentTimeSec - connection.record.rangeEndSec > maxTimeoutSec
            || m_openedConnections.size() > kMaxOpenedConnections)
        {
            itr = m_openedConnections.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void QnMServerAuditManager::at_connectionOpened(const QnAuthSession &session)
{
    addAuditRecord(prepareRecord(session, Qn::AR_Login));
}

void QnMServerAuditManager::at_connectionClosed(const QnAuthSession &data)
{
    if (!enabled())
        return;

    QnMutexLocker lock(&m_mutex);
    auto itr = m_openedConnections.find(data.id);
    if (itr != m_openedConnections.end()) {
        AuditConnection& connection = itr.value();
        connection.record.rangeEndSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
        updateAuditRecord(connection.internalId, connection.record);
    }
}

AuditHandle QnMServerAuditManager::notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& cameraId, qint64 timestampUsec, bool isExport)
{
    if (!enabled())
        return AuditHandle();

    // check for existing data

    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_alivePlaybackInfo.begin(); itr != m_alivePlaybackInfo.end(); ++itr)
    {
        CameraPlaybackInfo& pbInfo = *itr;
        if (pbInfo.session == session && pbInfo.cameraId == cameraId && pbInfo.isExport == isExport)
        {
            bool isLive = pbInfo.startTimeUsec == DATETIME_NOW && timestampUsec == DATETIME_NOW;
            if (isLive || pbInfo.period.distanceToTime(timestampUsec / 1000) < MIN_SEEK_DISTANCE_TO_LOG)
            {
                if (timestampUsec != DATETIME_NOW)
                    pbInfo.period.addPeriod(QnTimePeriod(timestampUsec / 1000, 1));
                pbInfo.timeout.invalidate();
                return pbInfo.handle;
            }
        }
    }

    // create new one

    CameraPlaybackInfo pbInfo;
    pbInfo.session = session;
    pbInfo.cameraId = cameraId;
    pbInfo.startTimeUsec = timestampUsec;
    if (timestampUsec != DATETIME_NOW)
        pbInfo.period.addPeriod(QnTimePeriod(timestampUsec / 1000, 1));
    pbInfo.creationTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    pbInfo.isExport = isExport;
    pbInfo.timeout.invalidate();
    pbInfo.aliveTimeout.restart();
    pbInfo.handle = std::make_shared<int>(++m_internalIdCounter);
    AuditHandle handle = pbInfo.handle;
    m_alivePlaybackInfo.insert(*handle, std::move(pbInfo));
    return handle;
}


QnTimePeriod QnMServerAuditManager::playbackRange(const AuditHandle& handle) const
{
    QnMutexLocker lock(&m_mutex);
    if (!handle)
        return QnTimePeriod();
    auto itr = m_alivePlaybackInfo.find(*handle);
    if (itr != m_alivePlaybackInfo.end())
        return itr.value().period;
    else
        return QnTimePeriod();
}

void QnMServerAuditManager::notifyPlaybackInProgress(const AuditHandle& handle, qint64 timestampUsec)
{
    if (!enabled())
        return;
    if (timestampUsec == DATETIME_NOW || timestampUsec == DATETIME_INVALID || timestampUsec <= 0)
        return;

    qint64 timestampMs = timestampUsec / 1000;
    QnMutexLocker lock(&m_mutex);
    auto itr = m_alivePlaybackInfo.find(*handle);
    if (itr != m_alivePlaybackInfo.end())
    {
        CameraPlaybackInfo& pbInfo = itr.value();
        if (!pbInfo.period.contains(timestampMs))
            pbInfo.period.addPeriod(QnTimePeriod(timestampMs, 1));
    }
}

bool QnMServerAuditManager::canJoinRecords(const QnAuditRecord& left, const QnAuditRecord& right)
{
    if (left.eventType != right.eventType)
        return false;
    bool peridOK = qAbs(left.rangeStartSec - right.rangeStartSec) * 1000ll < GROUP_TIME_THRESHOLD;
    bool sessionOK = left.authSession == right.authSession;
    return peridOK && sessionOK;
}

void QnMServerAuditManager::notifySettingsChanged(const QnAuthSession& authInfo, const QString& paramName)
{
    if (!enabled())
        return;

    ChangedSettingInfo info;
    info.paramName = paramName;
    info.session = authInfo;
    info.timeout.restart();
    QnMutexLocker lock(&m_mutex);
    m_changedSettings << info;
}

template <class T>
std::vector<QnAuditRecord> QnMServerAuditManager::processDelayedRecordsInternal(
    QVector<T>& recordsToAggregate, int recordAggregationTimeMs)
{
    std::vector<QnAuditRecord> recordsToAdd;
    int currentIndex = 0;
    while (currentIndex < recordsToAggregate.size())
    {
        if (recordsToAggregate[currentIndex].timeout.elapsed() > recordAggregationTimeMs)
        {
            // aggregate data. join other records to this one
            QnAuditRecord record = recordsToAggregate[currentIndex].toAuditRecord();

            recordsToAggregate.remove(currentIndex);
            for (int j = currentIndex; j < recordsToAggregate.size();)
            {
                QnAuditRecord newRecord = recordsToAggregate[j].toAuditRecord();
                if (canJoinRecords(record, newRecord))
                {
                    for (const auto& res : newRecord.resources)
                    {
                        if (std::find(record.resources.begin(), record.resources.end(), res) == record.resources.end())
                            record.resources.push_back(res);
                    }
                    record.createdTimeSec = qMin(record.createdTimeSec, newRecord.createdTimeSec);
                    record.rangeStartSec = qMin(record.rangeStartSec, newRecord.rangeStartSec);
                    record.rangeEndSec = qMax(record.rangeEndSec, newRecord.rangeEndSec);
                    recordsToAggregate.remove(j);
                }
                else
                {
                    ++j;
                }
            }
            recordsToAdd.push_back(std::move(record));
        }
        else
        {
            ++currentIndex;
        }
    }

    return recordsToAdd;
}

template <class T>
void QnMServerAuditManager::processDelayedRecords(QVector<T>& recordsToAggregate)
{
    auto recordsToAdd =
        processDelayedRecordsInternal(recordsToAggregate, AGGREGATION_TIME_MS);
    for (const auto& record : recordsToAdd)
    {
        registerNewConnection(record.authSession, record.eventType == Qn::AR_Login); //< add new session if not exists
        addAuditRecordInternal(record);
    }
}

bool QnMServerAuditManager::hasSimilarRecentlyRecord(const QnAuditRecord& data) const
{
    for (const auto& record : m_recentlyAddedRecords)
    {
        if (record.eventType == data.eventType &&
            record.resources == data.resources &&
            record.params == data.params &&
            record.authSession == data.authSession)
        {
            if (qAbs(record.createdTimeSec - data.createdTimeSec) < kRecentlyRecordsTimeGapSec)
                return true;
        }
    }
    return false;
}

bool QnMServerAuditManager::enabled() const
{
    return m_enabled;
}

void QnMServerAuditManager::setEnabled(bool value)
{
    QnMutexLocker lock(&m_mutex);

    m_enabled = value;
    if (!m_enabled)
    {
        m_openedConnections.clear();
        m_alivePlaybackInfo.clear();
        // m_closedPlaybackInfo.clear();
        // m_changedSettings.clear(); // keep it to write audit record if option is turned off
    }
}

void QnMServerAuditManager::at_timer()
{
    processRecords();
    flushRecords();
}

void QnMServerAuditManager::processRecords()
{
    QnMutexLocker lock(&m_mutex);

    for (auto itr = m_alivePlaybackInfo.begin(); itr != m_alivePlaybackInfo.end();)
    {
        CameraPlaybackInfo& pbInfo = itr.value();
        if (pbInfo.handle.use_count() == 1)
        {
            if (!pbInfo.timeout.isValid())
                pbInfo.timeout.restart(); // record isn't using any more. start remove timer
            if (pbInfo.timeout.elapsed() >= AGGREGATION_TIME_MS / 2)
            {
                // move to delete list
                if ((pbInfo.isExport && pbInfo.period.durationMs > 0) ||
                    (pbInfo.aliveTimeout.elapsed() >=
                            (MIN_PLAYBACK_TIME_TO_LOG + AGGREGATION_TIME_MS / 2) &&
                        pbInfo.period.durationMs >= MIN_PLAYBACK_TIME_TO_LOG))
                {
                    m_closedPlaybackInfo.push_back(std::move(itr.value()));
                }
                itr = m_alivePlaybackInfo.erase(itr);
            }
            else
                ++itr;
        }
        else
            ++itr;
    }

    processDelayedRecords(m_closedPlaybackInfo);
    processDelayedRecords(m_changedSettings);
}

void QnMServerAuditManager::stop()
{
    m_timer.stop();
    flushRecords();
    serverModule()->serverDb()->closeUnclosedAuditRecords((int) (qnSyncTime->currentMSecsSinceEpoch() / 1000));
}

int QnMServerAuditManager::addAuditRecordInternal(const QnAuditRecord& record)
{
    if (m_internalId < 0)
        return -1; //< error writing to server database

    if (record.isLoginType())
        NX_ASSERT(record.resources.empty());

    QnMutexLocker lock(&m_mutex);
    auto internalId = ++m_internalId;
    m_recordsToAdd[internalId] = filteredRecord(record);
    return internalId;
}

int QnMServerAuditManager::updateAuditRecordInternal(int internalId, const QnAuditRecord& record)
{
    if (m_internalId < 0)
        return -1; //< error writing to server database

    if (record.isLoginType())
        NX_ASSERT(record.resources.empty());

    QnMutexLocker lock(&m_mutex);
    m_recordsToAdd[internalId] = filteredRecord(record);
    return internalId;
}

void QnMServerAuditManager::flushRecords()
{
    decltype(m_recordsToAdd) records;
    {
        QnMutexLocker lock(&m_mutex);
        if (m_recordsToAdd.empty())
            return;

        std::swap(records, m_recordsToAdd);
    }

    if (!serverModule()->serverDb()->addAuditRecords(records))
        qWarning() << "Failed to add" << records.size() << "audit trail records";
}

int QnMServerAuditManager::addAuditRecord(const QnAuditRecord& record)
{
    if (!enabled())
        return -1;

    {
        QnMutexLocker lock(&m_mutex);
        if (m_sessionCleanupTimer.elapsed() > SESSION_CLEANUP_INTERVAL_MS)
        {
            m_sessionCleanupTimer.restart();
            cleanupExpiredSessions();
        }

        if (hasSimilarRecentlyRecord(record))
            return -1; //< ignore if same record has been added recently

        m_recentlyAddedRecords.push_back(record);
        if (m_recentlyAddedRecords.size() > kRecentlyRecordsCacheSize)
            m_recentlyAddedRecords.pop_front();
    }

    if (record.eventType == Qn::AR_UnauthorizedLogin)
        return addAuditRecordInternal(record);
    else
    {
        int internalId =
            registerNewConnection(record.authSession, record.eventType == Qn::AR_Login);
        if (record.eventType == Qn::AR_Login)
            return internalId;
        else
            return addAuditRecordInternal(record);
    }
}

int QnMServerAuditManager::updateAuditRecord(int internalId, const QnAuditRecord& record)
{
    if (!enabled())
        return -1;
    else
        return updateAuditRecordInternal(internalId, record);
}

int QnMServerAuditManager::registerNewConnection(const QnAuthSession& authInfo, bool explicitCall)
{
    cleanupExpiredSessions();
    if (!enabled())
        return 0;

    auto itr = m_openedConnections.find(authInfo.id);
    if (itr == m_openedConnections.end())
    {
        AuditConnection connection;
        connection.record = prepareRecord(authInfo, Qn::AR_Login);
        connection.record.rangeStartSec = connection.record.createdTimeSec;
        connection.internalId = addAuditRecordInternal(connection.record);
        itr = m_openedConnections.insert(authInfo.id, connection);
    }

    AuditConnection& connection = itr.value();
    if (explicitCall && connection.record.rangeEndSec)
    {
        connection.record.rangeEndSec = 0;
        updateAuditRecord(
            connection.internalId, connection.record); // make database record opened again
    }

    int endTime = explicitCall ? INT_MAX
                               : qnSyncTime->currentMSecsSinceEpoch() /
            1000; // do not auto delete if explicit log in (log out is expected);
    connection.record.rangeEndSec = qMax(endTime, connection.record.rangeEndSec);

    return connection.internalId;
}
