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
#include <core/resource_management/status_dictionary.h>
#include <media_server/settings.h>
#include "utils/common/util.h"

#include <stdexcept>
#include <functional>

const QString normalStorageUrl = lit("C/HD Witness Media");
const QString backupStorageUrl = lit("D/HD Witness Media");

const QString nLqPath = 
    normalStorageUrl + lit("/low_quality/camera_unique_id/2015/10/22/19");

const QString nHqPath =
    normalStorageUrl + lit("/hi_quality/camera_unique_id/2015/10/22/19");

const QString bLqPath = 
    backupStorageUrl + lit("/low_quality/camera_unique_id/2015/10/22/19");

const QString bHqPath = 
    backupStorageUrl + lit("/hi_quality/camera_unique_id/2015/10/22/19");

struct AuxData
{
    QnStorageResourcePtr        normalStorage;
    QnStorageResourcePtr        backupStorage;
    QnLongRunnablePool          runnablePool;
    QnResourcePool              resourcePool;
    QnScheduleSync              scheduleSync;
    QnStorageManager            normalManager;
    QnStorageManager            backupManager;
    QnResourceStatusDictionary  rdict;
#ifndef _WIN32
    QnPlatformAbstraction platformAbstraction;
#endif

    AuxData() 
        : normalManager(QnServer::StoragePool::Normal),
          backupManager(QnServer::StoragePool::Backup)
    {
        if (!createTestPhysicalData())
            throw std::runtime_error("createTestDirs() failed");

        normalStorage.reset(new QnFileStorageResource);
        normalStorage->setUrl(normalStorageUrl);

        backupStorage.reset(new QnFileStorageResource);
        backupStorage->setUrl(backupStorageUrl);

        qnNormalStorageMan->addStorage(normalStorage);
        qnNormalStorageMan->addStorage(backupStorage);
    }

    bool createTestPhysicalData()
    {
        if (!QDir().mkpath(nLqPath) || !QDir().mkpath(nHqPath) || 
            !QDir().mkpath(bLqPath) || !QDir().mkpath(bHqPath))
        {
            return false;
        }

        if (!QFile::copy(":/1445529661948_77158.mkv", nLqPath + lit("/1445529661948_77158.mkv")) || 
            !QFile::copy(":/1445529661948_77158.mkv", nHqPath + lit("/1445529661948_77158.mkv")))
        {
            return false;
        }
        return true;
    }

    bool fillCatalogs()
    {
    }

    void cleanup()
    {
        std::function<bool (const QString &)> recursiveCleaner =
            [](const QString &path)
            {
                QDir dir(path);
                QFileInfoList entryList = dir.entryInfoList(
                    QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
                    QDir::DirsFirst
                );
                for (auto &entry : entryList) 
                {
                    if 
                }
            };

        QDir().rmpath(nLqPath);
        QDir().rmpath(nHqPath);
        QDir().rmpath(bLqPath);
        QDir().rmpath(bHqPath);
    }

    ~AuxData() 
    {
        cleanup();
    }
};

TEST(ServerArchiveDelegateTest, Main) 
{
    try
    {
        AuxData auxData;
    }
    catch (const std::exception &e)
    {
        ASSERT_TRUE(false) << e.what();
    }
}