#pragma once

#include <QElapsedTimer>

#include <nx/utils/thread/mutex.h>

#include "audit/audit_manager.h"
#include <nx/vms/server/server_module_aware.h>

class QnMServerAuditManager:
    public QObject,
    public QnAuditManager,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:

    QnMServerAuditManager(QnMediaServerModule* serverModule);
    ~QnMServerAuditManager();

    void flushRecords();
    void stop();

    virtual AuditHandle notifyPlaybackStarted(const QnAuthSession& session,
        const QnUuid& id, qint64 timestampUsec, bool isExport = false) override;
    virtual void notifyPlaybackInProgress(const AuditHandle& handle, qint64 timestampUsec)  override;
    virtual void notifySettingsChanged(
        const QnAuthSession& authInfo, const QString& paramName)  override;

    /* return internal id of inserted record. Returns <= 0 if error */
    virtual int addAuditRecord(const QnAuditRecord& record)  override;
    virtual int updateAuditRecord(int internalId, const QnAuditRecord& record)  override;
    virtual QnTimePeriod playbackRange(const AuditHandle& handle) const  override;

    virtual void at_connectionOpened(const QnAuthSession& session) override;
    virtual void at_connectionClosed(const QnAuthSession& session) override;
private:
    int addAuditRecordInternal(const QnAuditRecord& record);
    int updateAuditRecordInternal(int internalId, const QnAuditRecord& record);
    int registerNewConnection(const QnAuthSession& authInfo, bool explicitCall);
    bool readLastRecordIdIfNeed();
private slots:
    void at_timer();
private:
    friend nx::vms::server::test::AuditManagerTest;

    void processRecords();
    bool enabled() const;
    bool hasSimilarRecentlyRecord(const QnAuditRecord& data) const;
    void setEnabled(bool value);

    static bool canJoinRecords(const QnAuditRecord& left, const QnAuditRecord& right);
    void cleanupExpiredSessions();

    template <class T>
    static std::vector<QnAuditRecord> processDelayedRecordsInternal(
        QVector<T>& recordsToAggregate,
        int recordAggregationTimeMs); // group and write to DB playback records
    template <class T>
    void processDelayedRecords(
        QVector<T>& recordsToAggregate); // group and write to DB playback records

private:

    struct AuditConnection
    {
        AuditConnection() : internalId(0) {}

        QnAuditRecord record;
        int internalId;
    };

    struct CameraPlaybackInfo
    {
        CameraPlaybackInfo() : startTimeUsec(0), creationTimeMs(0), isExport(false) {}

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

    std::deque<QnAuditRecord> m_recentlyAddedRecords;

    QElapsedTimer m_sessionCleanupTimer;
    QTimer m_timer;
    mutable QnMutex m_mutex;
    std::map<int, QnAuditRecord> m_recordsToAdd;
    int m_internalId;
    std::atomic<bool> m_enabled{false};
};
