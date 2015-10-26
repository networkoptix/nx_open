#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <utils/common/long_runnable.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <recorder/schedule_sync.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <media_server/settings.h>
#include "utils/common/util.h"

#include <cstdio>

struct Singletons 
{
    std::unique_ptr<QnLongRunnablePool> runnablePool;
    std::unique_ptr<QnResourcePool>     resourcePool;
    std::unique_ptr<QnScheduleSync>     scheduleSync;
    std::unique_ptr<QnStorageManager>   normalManager;
    std::unique_ptr<QnStorageManager>   backupManager;
#ifndef _WIN32
    std::unique_ptr<QnPlatformAbstraction> platformAbstraction;
#endif

    Singletons() 
        : runnablePool(new QnLongRunnablePool),
          resourcePool(new QnResourcePool),
          scheduleSync(new QnScheduleSync),
          normalManager(new QnStorageManager(QnServer::StoragePool::Normal)),
          backupManager(new QnStorageManager(QnServer::StoragePool::Backup))
#ifndef _WIN32
          ,platformAbstraction(new QnPlatformAbstraction)
#endif
    {}

    ~Singletons() 
    {
        backupManager.reset();
        normalManager.reset();
        scheduleSync.reset();
        resourcePool.reset();
        runnablePool.reset();
    }
};

TEST(ServerArchiveDelegateTest, Main) 
{
    Singletons s_;
}