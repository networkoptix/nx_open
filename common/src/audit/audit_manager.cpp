#include "audit_manager.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "api/global_settings.h"

static QnAuditManager* m_globalInstance = 0;

namespace
{
    const qint64 GROUP_TIME_THRESHOLD = 1000ll * 30;
}

QnAuditRecord QnAuditManager::CameraPlaybackInfo::toAuditRecord() const
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

QnAuditRecord QnAuditManager::ChangedSettingInfo::toAuditRecord() const
{
    Qn::AuditRecordType eventType;
    if (paramName.startsWith(lit("email")) || paramName.startsWith(lit("smtp")))
        eventType = Qn::AR_EmailSettings;
    else
        eventType = Qn::AR_SettingsChange;

    return QnAuditManager::prepareRecord(session, eventType);
}

QnAuditManager* QnAuditManager::instance()
{
    return m_globalInstance;
}

QnAuditManager::QnAuditManager()
{
    m_enabled = QnGlobalSettings::instance()->isAuditTrailEnabled();
    connect(QnGlobalSettings::instance(), &QnGlobalSettings::auditTrailEnableChanged, this,
        [this]() {
            setEnabled(QnGlobalSettings::instance()->isAuditTrailEnabled()); 
        }
    );

    assert( m_globalInstance == nullptr );
    m_globalInstance = this;
    connect(&m_timer, &QTimer::timeout, this, &QnAuditManager::at_timer);
    m_timer.start(1000 * 5);
}

QnAuditRecord QnAuditManager::prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType)
{
    QnAuditRecord result;
    result.authSession = authInfo;
    result.createdTimeSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    result.eventType = recordType;
    return result;
}

void QnAuditManager::at_connectionOpened(const QnAuthSession& authInfo)
{
    if (!enabled())
        return;

    AuditConnection connection;
    connection.record = prepareRecord(authInfo, Qn::AR_Login);
    connection.internalId = addAuditRecord(connection.record);
    if (connection.internalId > 0) {
        QMutexLocker lock(&m_mutex);
        m_openedConnections[authInfo.id] = connection;
    }
}

void QnAuditManager::at_connectionClosed(const QnAuthSession &data)
{
    if (!enabled())
        return;

    QMutexLocker lock(&m_mutex);
    auto itr = m_openedConnections.find(data.id);
    if (itr != m_openedConnections.end()) {
        AuditConnection& connection = itr.value();
        connection.record.rangeStartSec = connection.record.createdTimeSec;
        connection.record.rangeEndSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
        updateAuditRecord(connection.internalId, connection.record);
    }
    m_openedConnections.remove(data.id);
}

AuditHandle QnAuditManager::notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& cameraId, qint64 timestampUsec, bool isExport)
{
    if (!enabled())
        return AuditHandle();

    // check for existing data

    QMutexLocker lock(&m_mutex);
    for (auto itr = m_alivePlaybackInfo.begin(); itr != m_alivePlaybackInfo.end(); ++itr)
    {
        CameraPlaybackInfo& pbInfo = *itr;
        if (pbInfo.session == session && pbInfo.cameraId == cameraId && pbInfo.isExport == isExport) 
        {
            bool isLive = pbInfo.startTimeUsec == DATETIME_NOW && timestampUsec == DATETIME_NOW;
            if (isLive || pbInfo.period.distanceToTime(timestampUsec/1000) < MIN_SEEK_DISTANCE_TO_LOG) 
            {
                if (timestampUsec != DATETIME_NOW)
                    pbInfo.period.addPeriod(QnTimePeriod(timestampUsec/1000, 1));
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
        pbInfo.period.addPeriod(QnTimePeriod(timestampUsec/1000, 1));
    pbInfo.creationTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    pbInfo.isExport = isExport;
    pbInfo.timeout.invalidate();
    pbInfo.aliveTimeout.restart();
    pbInfo.handle = std::make_shared<int>(++m_internalIdCounter);
    AuditHandle handle = pbInfo.handle;
    m_alivePlaybackInfo.insert(*handle, std::move(pbInfo));
    return handle;
}


QnTimePeriod QnAuditManager::playbackRange(const AuditHandle& handle) const
{
    QMutexLocker lock(&m_mutex);
    if (!handle)
        return QnTimePeriod();
    auto itr = m_alivePlaybackInfo.find(*handle);
    if (itr != m_alivePlaybackInfo.end())
        return itr.value().period;
    else
        return QnTimePeriod();
}

void QnAuditManager::notifyPlaybackInProgress(const AuditHandle& handle, qint64 timestampUsec)
{
    if (!enabled())
        return;
    if (timestampUsec == DATETIME_NOW || timestampUsec == AV_NOPTS_VALUE || timestampUsec <= 0)
        return;

    qint64 timestampMs = timestampUsec / 1000;
    QMutexLocker lock(&m_mutex);
    auto itr = m_alivePlaybackInfo.find(*handle);
    if (itr != m_alivePlaybackInfo.end())
    {
        CameraPlaybackInfo& pbInfo = itr.value();
        if (!pbInfo.period.contains(timestampMs))
            pbInfo.period.addPeriod(QnTimePeriod(timestampMs, 1));
    }
}

bool QnAuditManager::canJoinRecords(const QnAuditRecord& left, const QnAuditRecord& right)
{
    if (left.eventType != right.eventType)
        return false;
    bool peridOK = qAbs(left.rangeStartSec - right.rangeStartSec) * 1000ll < GROUP_TIME_THRESHOLD;
    bool sessionOK = left.authSession == right.authSession;
    return peridOK && sessionOK;
}

void QnAuditManager::notifySettingsChanged(const QnAuthSession& authInfo, const QString& paramName)
{
    if (!enabled())
        return;

    ChangedSettingInfo info;
    info.paramName = paramName;
    info.session = authInfo;
    info.timeout.restart();
    QMutexLocker lock(&m_mutex);
    m_changedSettings << info;
}

void QnAuditManager::at_timer()
{
    QMutexLocker lock(&m_mutex);

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
                if ((pbInfo.isExport  && pbInfo.period.durationMs > 0) || 
                    (pbInfo.aliveTimeout.elapsed() >= (MIN_PLAYBACK_TIME_TO_LOG + AGGREGATION_TIME_MS / 2) && pbInfo.period.durationMs >= MIN_PLAYBACK_TIME_TO_LOG)) 
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

template <class T>
void QnAuditManager::processDelayedRecords(QVector<T>& recordsToAggregate)
{
    std::vector<QnAuditRecord> recordsToAdd;
    bool recordProcessed;
    do
    {
        recordProcessed = false;
        for (auto itr = recordsToAggregate.begin(); itr != recordsToAggregate.end(); ++itr)
        {
            if (itr->timeout.elapsed() > AGGREGATION_TIME_MS) 
            {
                // aggregate data. join other records to this one
                QnAuditRecord record = itr->toAuditRecord();
                while (itr != recordsToAggregate.end())
                {
                    QnAuditRecord newRecord = itr->toAuditRecord();
                    if (canJoinRecords(record, newRecord)) {
                        for (const auto& res: newRecord.resources) {
                            if( std::find(record.resources.begin(), record.resources.end(), res) == record.resources.end())
                                record.resources.push_back(res);
                        }
                        record.createdTimeSec = qMin(record.createdTimeSec, newRecord.createdTimeSec);
                        record.rangeStartSec = qMin(record.rangeStartSec, newRecord.rangeStartSec);
                        record.rangeEndSec = qMax(record.rangeEndSec, newRecord.rangeEndSec);
                        itr = recordsToAggregate.erase(itr);
                    }
                    else
                        ++itr;
                }
                recordsToAdd.push_back(std::move(record));
                recordProcessed = true;
                break;
            }
        }
    } while (recordProcessed);
    for (const auto& record: recordsToAdd)
        addAuditRecordInternal(record);
}

int QnAuditManager::addAuditRecord(const QnAuditRecord& record)
{
    if (!enabled())
        return -1;
    else
        return addAuditRecordInternal(record);
}

int QnAuditManager::updateAuditRecord(int internalId, const QnAuditRecord& record)
{
    if (!enabled())
        return -1;
    else
        return updateAuditRecordInternal(internalId, record);
}

bool QnAuditManager::enabled() const
{
    return m_enabled;
}

void QnAuditManager::setEnabled(bool value)
{
    QMutexLocker lock(&m_mutex);

    m_enabled = value;
    if (!m_enabled) {
        m_openedConnections.clear();
        m_alivePlaybackInfo.clear();
        //m_closedPlaybackInfo.clear();
        //m_changedSettings.clear(); // keep it to write audit record if option is turned off
    }
}
