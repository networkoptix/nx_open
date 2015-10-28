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
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <media_server/serverutil.h>
#include <core/resource/media_server_user_attributes.h>

#include <functional>
#include <thread>
#include <chrono>

const QString normalStorageUrl = lit("C/HD_Witness_Media");
const QString backupStorageUrl = lit("D/HD_Witness_Media");

const QString nLqPath = normalStorageUrl + lit("/low_quality/camera_unique_id/2015/10/22/19");
const QString nHqPath = normalStorageUrl + lit("/hi_quality/camera_unique_id/2015/10/22/19");

const QString bLqPath = backupStorageUrl + lit("/low_quality/camera_unique_id/2015/10/22/19");
const QString bHqPath = backupStorageUrl + lit("/hi_quality/camera_unique_id/2015/10/22/19");

const QString sGuid = lit("D71EC6D8-8EFD-4137-A098-0437AB2044CC");

class AuxData
{
public:
    AuxData() {}

    int prepare()
    {
        m_resourcePool.reset(new QnResourcePool);
        m_resourceTypePool.reset(new QnResourceTypePool);

        m_commonModule.reset(new QnCommonModule);
        m_commonModule->setModuleGUID(sGuid);
        
        //m_mediaServer = QnMediaServerResourcePtr(new QnMediaServerResource(qnResTypePool));
        //m_mediaServer->setId(sGuid);
        //m_resourcePool->addResource(m_mediaServer);

        m_runnablePool.reset(new QnLongRunnablePool);
        m_normalManager.reset(new QnStorageManager(QnServer::StoragePool::Normal));
        m_backupManager.reset(new QnStorageManager(QnServer::StoragePool::Backup));
        //m_scheduleSync.reset(new QnScheduleSync);

        //qnNormalStorageMan->initDone();
        //qnBackupStorageMan->initDone();

        m_normalStorage = QnStorageResourcePtr(new QnFileStorageResource);
        m_normalStorage->setUrl(normalStorageUrl);

        m_backupStorage = QnStorageResourcePtr(new QnFileStorageResource);
        m_backupStorage->setUrl(backupStorageUrl);

        qnNormalStorageMan->addStorage(m_normalStorage);
        qnNormalStorageMan->addStorage(m_backupStorage);

        if (!createTestPhysicalData())
            return -1;

        return 0;
    }

    ~AuxData() 
    {
        //cleanup();
    }

    void cleanup()
    {
        qnNormalStorageMan->stopAsyncTasks();
        qnBackupStorageMan->stopAsyncTasks();

        m_normalManager.reset();
        m_backupManager.reset();
        m_scheduleSync.reset();
        //m_mediaServer.reset();
        m_commonModule.reset();
        m_runnablePool.reset();
        m_resourceTypePool.reset();

        std::function<bool (const QString &)> recursiveClean =
            [&](const QString &path) 
            {
                QDir dir(path);
                QFileInfoList entryList = dir.entryInfoList(
                    QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot, 
                    QDir::DirsFirst
                );
                
                for (auto &entry : entryList) 
                {
                    auto tmp = entry.absoluteFilePath();
                    if (entry.isDir()) 
                    {
                         if (!recursiveClean(entry.absoluteFilePath()))
                            return false;
                    } 
                    else if (entry.isFile()) 
                    {
                        bool result = dir.remove(entry.absoluteFilePath());
                        if (!result)
                            return false;
                    }
                }
                dir.rmpath(path);
                return true;
            };

        recursiveClean(normalStorageUrl);
        recursiveClean(backupStorageUrl);
    }

private:
    bool createTestPhysicalData()
    {
        if (!QDir().mkpath(nLqPath) || !QDir().mkpath(nHqPath) || 
            !QDir().mkpath(bLqPath) || !QDir().mkpath(bHqPath)) {            
            return false;
        }

        if (!QFile::copy(":/1445529661948_77158.mkv", nLqPath + lit("/1445529661948_77158.mkv")) || 
            !QFile::copy(":/1445529661948_77158.mkv", nHqPath + lit("/1445529661948_77158.mkv"))) {
            return false;
        }
        return true;
    }

    bool fillCatalogs()
    {
    }

private:
    QnStorageResourcePtr                m_normalStorage;
    QnStorageResourcePtr                m_backupStorage;
    std::unique_ptr<QnCommonModule>     m_commonModule;
    std::unique_ptr<QnLongRunnablePool> m_runnablePool;
    std::unique_ptr<QnResourcePool>     m_resourcePool;
    std::unique_ptr<QnScheduleSync>     m_scheduleSync;
    std::unique_ptr<QnStorageManager>   m_normalManager;
    std::unique_ptr<QnStorageManager>   m_backupManager;
    std::unique_ptr<QnResourceTypePool> m_resourceTypePool;
    //QnMediaServerResourcePtr            m_mediaServer;    
    QnResourceStatusDictionary          m_rdict;
    //QnMediaServerUserAttributesPool     m_mediaServerAttrPool;
#ifndef _WIN32
    QnPlatformAbstraction platformAbstraction;
#endif

};

TEST(ServerArchiveDelegateTest, Main) 
{
    AuxData auxData;
    ASSERT_TRUE(auxData.prepare() == 0) << "Prepare test data failed";
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auxData.cleanup();
}