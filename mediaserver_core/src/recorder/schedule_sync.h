#ifndef __SHEDULE_SYNC_H__
#define __SHEDULE_SYNC_H__

#include <atomic>
#include <vector>
#include <map>
#include <nx/utils/std/future.h>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <common/common_globals.h>
#include <api/model/backup_status_reply.h>
#include <recorder/storage_manager.h>
#include <core/resource/server_backup_schedule.h>
#include <nx_ec/data/api_media_server_data.h>
#include "nx/utils/thread/long_runnable.h"
#include <common/common_module_aware.h>
#include <nx/utils/std/future.h>

class QnScheduleSync: public QnLongRunnable, public QnCommonModuleAware
{
    Q_OBJECT
private:
    struct ChunkKey
    {
        DeviceFileCatalog::Chunk chunk;
        QString                  cameraId;
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
    QnScheduleSync(QnCommonModule* commonModule);
    ~QnScheduleSync();
signals:
    void backupFinished(
        qint64                      timestampMs,
        nx::vms::event::EventReason     status
    );

public:
    int forceStart();
    virtual void stop() override;
    int interrupt();

    void updateLastSyncChunk();
    QnBackupStatusData getStatus() const;
    virtual void run() override;
    virtual void pleaseStop() override;
private:
    DeviceFileCatalog::Chunk findLastSyncChunkUnsafe() const;

#define COPY_ERROR_LIST(APPLY) \
    APPLY(GetCatalogError) \
    APPLY(NoBackupStorageError) \
    APPLY(FromStorageError) \
    APPLY(SourceFileError) \
    APPLY(TargetFileError) \
    APPLY(ChunkError) \
    APPLY(NoError) \
    APPLY(Interrupted)

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
    nx::vms::event::EventReason synchronize(NeedMoveOnCB needMoveOn);

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

    // Interrupted by user OR current backup session is over
    std::atomic<bool>       m_interrupted;
    bool                    m_failReported;
    ec2::backup::DayOfWeek  m_curDow;

    QnServerBackupSchedule  m_schedule;
    qint64                  m_syncTimePoint;
    qint64                  m_syncEndTimePoint;
    QnMutex                 m_syncPointMutex;

    SyncDataMap           m_syncData;
    mutable QnMutex       m_syncDataMutex;
    CopyError             m_lastError;
    nx::utils::promise<void>    m_stopPromise;
};

#endif

