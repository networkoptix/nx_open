#ifndef __SHEDULE_SYNC_H__
#define __SHEDULE_SYNC_H__

#include <future>
#include <atomic>

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
        Qn::BackupTypes type;
    };

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

    template<typename NeedStopCB>
    void synchronize(
        QnServer::ChunksCatalog catalog,
        const QString           &cameraId,
        NeedStopCB              needStop
    );

    void start();
    void renewSchedule();

private:
    std::atomic<bool>   m_backupSyncOn;
    std::atomic<bool>   m_syncing;
    std::atomic<bool>   m_forced;
    std::future<void>   m_backupFuture;
    schedule            m_schedule;
};

#define qnScheduleSync QnScheduleSync::instance()

#endif

