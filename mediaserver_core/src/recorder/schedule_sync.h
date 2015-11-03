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
    
    typedef std::vector<ChunkKey> ChunkKeyVector;

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
    void backupFinished(qint64 timestampMs, QnServer::BackupResultCode status);
public:
    int forceStart(); 
    virtual void stop() override;
    int interrupt();
    QnBackupStatusData getStatus() const;
    virtual void run() override;

private:
#define COPY_ERROR_LIST(APPLY) \
    APPLY(GetCatalogError) \
    APPLY(NoBackupStorageError) \
    APPLY(FromStorageError) \
    APPLY(FileOpenError) \
    APPLY(ChunkError) \
    APPLY(NoError)

#define ENUM_APPLY(value) value,

    enum class CopyError {
        COPY_ERROR_LIST(ENUM_APPLY)
    };

#define TO_STRING_APPLY(value) case CopyError::value: return lit(#value);

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

    boost::optional<ChunkKeyVector> getOldestChunk();    
    ChunkKey getOldestChunk(
        const QString           &cameraId,
        QnServer::ChunksCatalog catalog
    );

private:
    std::atomic<bool>       m_backupSyncOn;
    std::atomic<bool>       m_syncing;
    std::atomic<bool>       m_forced;
    std::atomic<bool>       m_interrupted;
    bool                    m_backupDone;
    ec2::backup::DayOfWeek  m_curDow;

    QnServerBackupSchedule  m_schedule;
    std::atomic<int64_t>    m_syncTimePoint;

    std::map<ChunkKey, double> m_syncData;
    mutable std::mutex         m_syncDataMutex;
};

#endif

