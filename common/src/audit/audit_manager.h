#ifndef __AUDIT_MANAGER_H__
#define __AUDIT_MANAGER_H__

#include <atomic>
#include <deque>

#include <QTimer>
#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/mutex.h>

#include "audit_manager_fwd.h"
#include "api/model/audit/audit_record.h"
#include "api/model/audit/auth_session.h"
#include "recording/time_period.h"
#include <nx/utils/singleton.h>
#include <common/common_module_aware.h>

class QnAuditManager:
    public QObject,
    public QnCommonModuleAware,
    public Singleton<QnAuditManager>
{
    Q_OBJECT

public:

    static const int MIN_PLAYBACK_TIME_TO_LOG = 1000 * 5;
    static const int AGGREGATION_TIME_MS = 1000 * 5;
    static const qint64 MIN_SEEK_DISTANCE_TO_LOG = 1000 * 60;

    QnAuditManager(QObject* parent);

public:
    virtual ~QnAuditManager() = default;

    static QnAuditRecord prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType);

    /* notify new playback was started from position timestamp
    *  return internal ID of started session
    */
    AuditHandle notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& id, qint64 timestampUsec, bool isExport = false);
    void notifyPlaybackInProgress(const AuditHandle& handle, qint64 timestampUsec);
    void notifySettingsChanged(const QnAuthSession& authInfo, const QString& paramName);

    /* return internal id of inserted record. Returns <= 0 if error */
    int addAuditRecord(const QnAuditRecord& record);
    int updateAuditRecord(int internalId, const QnAuditRecord& record);
    bool enabled() const;
    QnTimePeriod playbackRange(const AuditHandle& handle) const;

public slots:
    void at_connectionOpened(const QnAuthSession &session);
    void at_connectionClosed(const QnAuthSession &session);

private slots:
    void setEnabled(bool value);

protected:
    virtual int addAuditRecordInternal(const QnAuditRecord& record) = 0;
    virtual int updateAuditRecordInternal(int internalId, const QnAuditRecord& record) = 0;

private slots:
    void at_timer();

private:
    int registerNewConnection(const QnAuthSession &data, bool explicitCall);
    bool hasSimilarRecentlyRecord(const QnAuditRecord& data) const;

    struct AuditConnection
    {
        AuditConnection(): internalId(0) {}

        QnAuditRecord record;
        int internalId;
    };

    struct CameraPlaybackInfo
    {
        CameraPlaybackInfo(): startTimeUsec(0), creationTimeMs(0), isExport(false) {}

        QnAuthSession session;      // user's session
        QnUuid cameraId;            // watching camera
        QnTimePeriod period;        // watching playback range
        qint64 startTimeUsec;       // startTime from PLAY command
        qint64 creationTimeMs;      // record creation time
        QElapsedTimer aliveTimeout; // lifetime timeout
        QElapsedTimer timeout;      // how many ms session is unused
        bool isExport;              // either export or archive/live access
        AuditHandle handle;         // internal handle. It's int with counter. AuditManager will delete handle if it unused (refCnt=1) within some period of time

        QnAuditRecord toAuditRecord() const;
    };

    struct ChangedSettingInfo
    {
        ChangedSettingInfo() {}

        QnAuthSession session;         // user's session
        QString paramName;
        QElapsedTimer timeout;         // how many ms session is alive

        QnAuditRecord toAuditRecord() const;
    };

    QMap<int, CameraPlaybackInfo> m_alivePlaybackInfo; // opened cameras
    QVector<ChangedSettingInfo> m_changedSettings;
    QVector<CameraPlaybackInfo> m_closedPlaybackInfo;  // recently closed cameras
    std::atomic<int> m_internalIdCounter;

    QMap<QnUuid, AuditConnection> m_openedConnections;

    mutable QnMutex m_mutex;
    QTimer m_timer;
    std::atomic<bool> m_enabled;
    QElapsedTimer m_sessionCleanupTimer;
    std::deque<QnAuditRecord> m_recentlyAddedRecords;

private:
    bool canJoinRecords(const QnAuditRecord& left, const QnAuditRecord& right);
    void cleanupExpiredSessions();
    template <class T>
    void processDelayedRecords(QVector<T>& recordsToAggregate); // group and write to DB playback records
};

#define qnAuditManager QnAuditManager::instance()

#endif // __AUDIT_MANAGER_H__
