#include "audit_manager.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

static QnAuditManager* m_globalInstance = 0;

namespace
{
    const int MIN_PLAYBACK_TIME_TO_LOG = 1000 * 5;
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
    result.timestamp = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    result.eventType = recordType;
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
        connection.record.endTimestamp = qnSyncTime->currentMSecsSinceEpoch() / 1000;
        updateAuditRecord(connection.internalId, connection.record);
    }
    m_openedConnections.remove(data.sessionId);
}

int QnAuditManager::notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& cameraId, qint64 timestampUsec)
{
    CameraPlaybackInfo pbInfo;
    pbInfo.session = session;
    pbInfo.cameraId = cameraId;
    pbInfo.startTimeUsec = timestampUsec;
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

bool QnAuditManager::canJoinRecords(const CameraPlaybackInfo& left, const CameraPlaybackInfo& right)
{
    bool peridOK = qAbs(left.startTimeUsec - right.startTimeUsec) < MAX_FRAME_DURATION * 1000ll;
    bool sessionOK = left.session == right.session;
    return peridOK && sessionOK;
}

void QnAuditManager::at_timer()
{
    std::vector<QnAuditRecord> recordsToAdd;
    bool recordProcessed;
    do
    {
        QMutexLocker lock(&m_mutex);
        recordProcessed = false;
        for (auto itr = m_closedPlaybackInfo.begin(); itr != m_closedPlaybackInfo.end(); ++itr)
        {
            if (itr->timeout.elapsed() > MIN_PLAYBACK_TIME_TO_LOG) 
            {
                // aggregate data. join other records to this one
                CameraPlaybackInfo pbInfo = *itr;
                QnAuditRecord record = prepareRecord(pbInfo.session, pbInfo.startTimeUsec == DATETIME_NOW ? Qn::AR_ViewLive : Qn::AR_ViewArchive);
                while (itr != m_closedPlaybackInfo.end())
                {
                    if (canJoinRecords(pbInfo, *itr)) {
                        record.resources.push_back(itr->cameraId);
                        pbInfo.period.addPeriod(itr->period); // use union for all periods if they similar to each other
                        itr = m_closedPlaybackInfo.erase(itr);
                    }
                    else
                        ++itr;
                }
                record.timestamp = pbInfo.period.startTimeMs / 1000;
                record.endTimestamp = pbInfo.period.endTimeMs() / 1000;
                recordsToAdd.push_back(std::move(record));
                recordProcessed = true;
                break;
            }
        }
    } while (recordProcessed);
    for (const auto& record: recordsToAdd)
        addAuditRecord(record);
}
