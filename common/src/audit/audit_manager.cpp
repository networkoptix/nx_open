#include "audit_manager.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

static QnAuditManager* m_globalInstance = 0;

namespace
{
    const int MIN_PLAYBACK_TIME_TO_LOG = 1000 * 5;
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
    assert( m_globalInstance == nullptr );
    m_globalInstance = this;
    connect(&m_timer, &QTimer::timeout, this, &QnAuditManager::at_timer);
    m_timer.start(1000 * 5);
}

QnAuditRecord QnAuditManager::prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType)
{
    QnAuditRecord result;
    result.fillAuthInfo(authInfo);
    result.createdTimeSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    result.eventType = recordType;
    if (result.isLoginType() && !authInfo.userAgent.isEmpty())
        result.addParam("description", authInfo.userAgent.toUtf8());
    return result;
}

void QnAuditManager::at_connectionOpened(const QnAuthSession& authInfo)
{
    AuditConnection connection;
    connection.record = prepareRecord(authInfo, Qn::AR_Login);
    connection.internalId = addAuditRecord(connection.record);
    if (connection.internalId > 0) {
        QMutexLocker lock(&m_mutex);
        m_openedConnections[authInfo.sessionId] = connection;
    }
}

void QnAuditManager::at_connectionClosed(const QnAuthSession &data)
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_openedConnections.find(data.sessionId);
    if (itr != m_openedConnections.end()) {
        AuditConnection& connection = itr.value();
        connection.record.rangeStartSec = connection.record.createdTimeSec;
        connection.record.rangeEndSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
        updateAuditRecord(connection.internalId, connection.record);
    }
    m_openedConnections.remove(data.sessionId);
}

int QnAuditManager::notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& cameraId, qint64 timestampUsec, bool isExport)
{
    CameraPlaybackInfo pbInfo;
    pbInfo.session = session;
    pbInfo.cameraId = cameraId;
    pbInfo.startTimeUsec = timestampUsec;
    pbInfo.creationTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    pbInfo.isExport = isExport;
    pbInfo.timeout.restart();
    int handle = ++m_internalIdCounter;
    QMutexLocker lock(&m_mutex);
    m_alivePlaybackInfo.insert(handle, std::move(pbInfo));
    return handle;
}

void QnAuditManager::notifyPlaybackFinished(int internalId)
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_alivePlaybackInfo.find(internalId);
    if (itr != m_alivePlaybackInfo.end())
    {
        CameraPlaybackInfo& pbInfo = itr.value();
        if (pbInfo.timeout.elapsed() >= MIN_PLAYBACK_TIME_TO_LOG && pbInfo.period.durationMs >= MIN_PLAYBACK_TIME_TO_LOG) {
            pbInfo.timeout.restart();
            m_closedPlaybackInfo.push_back(std::move(pbInfo)); // finalize old playback record
        }
        m_alivePlaybackInfo.erase(itr);
    }
}

void QnAuditManager::notifyPlaybackInProgress(int internalId, qint64 timestampUsec)
{
    qint64 timestampMs = timestampUsec / 1000;
    QMutexLocker lock(&m_mutex);
    auto itr = m_alivePlaybackInfo.find(internalId);
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
    bool peridOK = qAbs(left.rangeStartSec - right.rangeStartSec) * 1000ll < MAX_FRAME_DURATION;
    bool sessionOK = left.sessionId == right.sessionId && left.userName == right.userName && left.userHost == right.userHost;
    return peridOK && sessionOK;
}

void QnAuditManager::notifySettingsChanged(const QnAuthSession& authInfo, const QString& paramName)
{
    ChangedSettingInfo info;
    info.paramName = paramName;
    info.session = authInfo;
    info.timeout.restart();
    QMutexLocker lock(&m_mutex);
    m_changedSettings << info;
}

void QnAuditManager::at_timer()
{
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
        QMutexLocker lock(&m_mutex);
        recordProcessed = false;
        for (auto itr = recordsToAggregate.begin(); itr != recordsToAggregate.end(); ++itr)
        {
            if (itr->timeout.elapsed() > MIN_PLAYBACK_TIME_TO_LOG) 
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
        addAuditRecord(record);
}
