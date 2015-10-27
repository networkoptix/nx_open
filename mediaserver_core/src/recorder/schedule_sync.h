#ifndef __SHEDULE_SYNC_H__
#define __SHEDULE_SYNC_H__

#include <QtConcurrent>
#include <atomic>
#include <vector>
#include <map>
#include <boost/optional.hpp>

#include <common/common_globals.h>
#include <api/model/backup_status_reply.h>
#include <recorder/storage_manager.h>
#include <core/resource/server_backup_schedule.h>

class QnScheduleSync: public QObject
{
    Q_OBJECT
private:
    struct ChunkKey 
    {
        DeviceFileCatalog::Chunk chunk;
        QString                  cameraID;
        QnServer::ChunksCatalog  catalog;
    };
    friend bool operator < (const ChunkKey &key1, const ChunkKey &key2);
    
    typedef std::vector<ChunkKey> ChunkKeyVector;

public:
    enum code
    {
        Ok = 0,
        Started,
        Syncing,
        Idle,
        Forced
    };

public:
    QnScheduleSync();
    ~QnScheduleSync();
signals:
    void backupFinished(qint64 timestampMs);
public:
    static QnScheduleSync *instance();
    int forceStart(); 
    int stop();
    int interrupt();
    QnBackupStatusData getStatus() const;
    void start();
private:
    template<typename NeedStopCB>
    void synchronize(NeedStopCB needStop);

    void renewSchedule();
    void copyChunk(const ChunkKey &chunkKey);

    int state() const;

    boost::optional<ChunkKeyVector> getOldestChunk();    
    ChunkKey getOldestChunk(
        const QString           &cameraId,
        QnServer::ChunksCatalog catalog
    );

private:
    std::atomic<bool>    m_backupSyncOn;
    std::atomic<bool>    m_syncing;
    std::atomic<bool>    m_forced;
    QFuture<void>        m_backupFuture;
    QnServerBackupSchedule  m_schedule;
    std::atomic<int64_t> m_syncTimePoint;

    std::map<ChunkKey, double> m_syncData;
    mutable std::mutex         m_syncDataMutex;
};

#define qnScheduleSync QnScheduleSync::instance()

#endif

