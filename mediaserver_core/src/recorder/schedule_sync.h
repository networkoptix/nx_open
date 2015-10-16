#ifndef __SHEDULE_SYNC_H__
#define __SHEDULE_SYNC_H__

#include <future>
#include <atomic>
#include <vector>
#include <boost/optional.hpp>

#include <common/common_globals.h>

class QnScheduleSync
{
private:
    struct schedule
    {
        int dow;
        int start;
        int duration;
        int bitrate;
        Qn::BackupType type;
    };

    struct ChunkKey 
    {
        DeviceFileCatalog::Chunk chunk;
        QString                  cameraID;
        QnServer::ChunksCatalog  catalog;
    };
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
public:
    static QnScheduleSync *instance();
    int forceStart(); 
    int stop();
    int interrupt();
    int state() const;

private:
    template<typename NeedStopCB>
    void synchronize(NeedStopCB needStop);

    void start();
    void renewSchedule();
    void copyChunk(const ChunkKey &chunkKey);
    
    boost::optional<ChunkKeyVector> getOldestChunk();    
    ChunkKey getOldestChunk(
        const QString           &cameraId,
        QnServer::ChunksCatalog catalog
    );


private:
    std::atomic<bool>    m_backupSyncOn;
    std::atomic<bool>    m_syncing;
    std::atomic<bool>    m_forced;
    std::future<void>    m_backupFuture;
    schedule             m_schedule;
    std::atomic<int64_t> m_syncTimePoint;
};

#define qnScheduleSync QnScheduleSync::instance()

#endif

