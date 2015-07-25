#ifndef __AUDIT_MANAGER_H__
#define __AUDIT_MANAGER_H__

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

    QnAuditManager();

    static QnAuditManager* instance();
public:
    QnAuditRecord prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType);

    /* notify new playback was started from position timestamp
    *  return internal ID of started session
    */
    int notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& id, qint64 timestampUsec);
    void notifyPlaybackFinished(int internalId);
    void notifyPlaybackInProgress(int internalId, qint64 timestampUsec);

    virtual int addAuditRecord(const QnAuditRecord& record) = 0;
    virtual int updateAuditRecord(int internalId, const QnAuditRecord& record) = 0;
public slots:
    void at_connectionOpened(const QnAuthSession &data);
    void at_connectionClosed(const QnAuthSession &data);
private slots:
    void at_timer();
protected:
    /* return internal id of inserted record. Returns <= 0 if error */
private:

    struct AuditConnection
    {
        AuditConnection(): internalId(0) {}

        QnAuditRecord record;
        int internalId;
    };

    /*
    struct PlaybackAggregationInfo
    {
        //QMap<QnUuid, QnTimePeriod> m_playRange;
        //QnTimePeriod period;
        //QSet<QnUuid> cameras;
    };
    typedef QMap<qint64, PlaybackAggregationInfo> PlaybackAggregationInfoMap;

    QMap<QnUuid, PlaybackAggregationInfoMap> m_playbackInfo;
    */

    struct CameraPlaybackInfo
    {
        CameraPlaybackInfo(): startTimeUsec(0), creationTimeMs(0) {}

        QnAuthSession session;       // user's session
        QnUuid cameraId;        // watching camera
        QnTimePeriod period;    // watching playback range
        qint64 startTimeUsec;       // startTime from PLAY command
        qint64 creationTimeMs;       // record creation time
        QElapsedTimer timeout;  // how many ms session is alive
    };

    QMap<int, CameraPlaybackInfo> m_alivePlaybackInfo;   // opened cameras
    QVector<CameraPlaybackInfo> m_closedPlaybackInfo;  // recently closed cameras
    std::atomic<int> m_internalIdCounter;

    QMap<QnUuid, AuditConnection> m_openedConnections;

    mutable QMutex m_mutex;
    QTimer m_timer;
private:
    bool canJoinRecords(const CameraPlaybackInfo& left, const CameraPlaybackInfo& right);
};
#define qnAuditManager QnAuditManager::instance()

#endif // __AUDIT_MANAGER_H__
