#ifndef __AUDIT_MANAGER_H__
#define __AUDIT_MANAGER_H__

#include <QMutex>
#include <QTimer>
#include <QElapsedTimer>

#include "api/model/audit/audit_record.h"
#include "api/model/audit/auth_session.h"
#include "recording/time_period.h"
#include <atomic>

class QnAuditManager: public QObject
{
    Q_OBJECT
public:

    static const int MIN_PLAYBACK_TIME_TO_LOG = 1000 * 5;

    QnAuditManager();

    static QnAuditManager* instance();
public:
    static QnAuditRecord prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType);

    /* notify new playback was started from position timestamp
    *  return internal ID of started session
    */
    int notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& id, qint64 timestampUsec, bool isExport = false);
    void notifyPlaybackFinished(int internalId);
    void notifyPlaybackInProgress(int internalId, qint64 timestampUsec);
    void notifySettingsChanged(const QnAuthSession& authInfo, const QString& paramName);

    /* return internal id of inserted record. Returns <= 0 if error */
    int addAuditRecord(const QnAuditRecord& record);
    int updateAuditRecord(int internalId, const QnAuditRecord& record);
    bool enabled() const;
public slots:
    void at_connectionOpened(const QnAuthSession &data);
    void at_connectionClosed(const QnAuthSession &data);
private slots:
    void setEnabled(bool value);
protected:
    virtual int addAuditRecordInternal(const QnAuditRecord& record) = 0;
    virtual int updateAuditRecordInternal(int internalId, const QnAuditRecord& record) = 0;
private slots:
    void at_timer();
protected:
private:

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
        QElapsedTimer timeout;      // how many ms session is alive
        bool isExport;

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

    mutable QMutex m_mutex;
    QTimer m_timer;
    std::atomic<bool> m_enabled;

private:
    bool canJoinRecords(const QnAuditRecord& left, const QnAuditRecord& right);
    
    template <class T>
    void processDelayedRecords(QVector<T>& recordsToAggregate); // group and write to DB playback records
};
#define qnAuditManager QnAuditManager::instance()

#endif // __AUDIT_MANAGER_H__
