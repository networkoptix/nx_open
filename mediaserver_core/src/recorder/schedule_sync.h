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
#include <nx_ec/data/api_media_server_data.h>
#include "utils/common/long_runnable.h"

class QnScheduleSync: public QnLongRunnable
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

    struct SyncData 
    {
        int totalChunks;
        int startIndex;
        int currentIndex;

        SyncData() 
            : totalChunks(0), 
              startIndex(0), 
              currentIndex(0) 
        {}
        explicit SyncData(int startIndex) 
            : totalChunks(0),
              startIndex(startIndex),
              currentIndex(startIndex)
        {}
    };
    
    typedef std::vector<ChunkKey>         ChunkKeyVector;
    typedef std::map<ChunkKey, SyncData>  SyncDataMap;

public:
    enum code {
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
    void backupFinished(
        qint64                      timestampMs,
        QnServer::BackupResultCode  status,
        const QString               &description
    );

public:
    int forceStart(); 
    virtual void stop() override;
    int interrupt();

    void updateLastSyncChunk();
    QnBackupStatusData getStatus() const;
    virtual void run() override;

private:
    DeviceFileCatalog::Chunk findLastSyncChunkUnsafe() const;

#define COPY_ERROR_LIST(APPLY) \
    APPLY(GetCatalogError, "Chunks catalog error") \
    APPLY(NoBackupStorageError, "No available backup storages with sufficient free space") \
    APPLY(FromStorageError, "Source storage error") \
    APPLY(SourceFileError, "Source file error") \
    APPLY(TargetFileError, "Target file error") \
    APPLY(ChunkError, "Chunk error") \
    APPLY(NoError, "No error") \
    APPLY(Interrupted, "Interrupted")

#define ENUM_APPLY(value, _) value,

    enum class CopyError {
        COPY_ERROR_LIST(ENUM_APPLY)
    };

#define TO_STRING_APPLY(value, desc) case CopyError::value: return lit(desc);

    QString copyErrorString(CopyError error) {
        switch (error) {
            COPY_ERROR_LIST(TO_STRING_APPLY)
        }
        return QString();
    }

#undef TO_STRING_APPLY
#undef ENUM_APPLY
#undef COPY_ERROR_LIST

    enum class SyncCode {
        Interrupted,
        OutOfTime,
        WrongBackupType,
        Ok
    };

private:
    template<typename NeedMoveOnCB>
    QnServer::BackupResultCode synchronize(NeedMoveOnCB needMoveOn);

    void renewSchedule();
    CopyError copyChunk(const ChunkKey &chunkKey);

    int state() const;
    void initSyncData();

    void addSyncDataKey(
        QnServer::ChunksCatalog quality,
        const QString           &cameraId
    );

    boost::optional<ChunkKeyVector> getOldestChunk(qint64 fromTimeMs) const;

    ChunkKey getOldestChunk(
        const QString           &cameraId, 
        QnServer::ChunksCatalog catalog, 
        qint64                  fromTimeMs,
        SyncData                *syncData = nullptr
    ) const;

private:
    std::atomic<bool>       m_backupSyncOn;
    std::atomic<bool>       m_syncing;
    std::atomic<bool>       m_forced;
    std::atomic<bool>       m_interrupted;
    bool                    m_failReported;
    ec2::backup::DayOfWeek  m_curDow;

    QnServerBackupSchedule  m_schedule;
    qint64                  m_syncTimePoint;
    std::atomic<qint64>     m_syncEndTimePoint;
    QnMutex                 m_syncPointMutex;

    SyncDataMap           m_syncData;
    mutable QnMutex       m_syncDataMutex;
    CopyError             m_lastError;
};

#endif

