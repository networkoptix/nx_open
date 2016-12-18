#include "media_server_process.h"
#include "platform/platform_abstraction.h"
#include "utils/common/long_runnable.h"
#include "media_server/media_server_module.h"
#include "media_server/settings.h"
#include "utils.h"
#include "utils/common/util.h"
#include "core/resource/storage_resource.h"
#include "core/resource_management/resource_pool.h"
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <common/common_module.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

class InitStoragesTestWorker : public QObject
{
public:

    InitStoragesTestWorker(MediaServerProcess& processor):
        m_processor(processor)
        {
            connect(&processor, &MediaServerProcess::started, this,
                [this]()
                {
                    auto storages = qnResPool->getResources<QnStorageResource>();
                    ASSERT_TRUE(storages.isEmpty());
                    m_processor.stopAsync();
                });
        }

private:
    MediaServerProcess& m_processor;
};

TEST(InitStoragesTest, main)
{
    //QApplication::instance()->args();
    int argc = 2;
    char* argv[] = { "", "-e" };

    nx::ut::utils::WorkDirResource workDirResource;
    ASSERT_TRUE((bool)workDirResource.getDirName());

    QScopedPointer<QnPlatformAbstraction> platform(new QnPlatformAbstraction());
    MSSettings::roSettings()->setValue(lit("serverGuid"), QnUuid::createUuid().toString());
    MSSettings::roSettings()->setValue(lit("removeDbOnStartup"), lit("1"));
    MSSettings::roSettings()->setValue(lit("dataDir"), *workDirResource.getDirName());
    MSSettings::roSettings()->setValue(lit("varDir"), *workDirResource.getDirName());

    QString dbDir = MSSettings::roSettings()->value("eventsDBFilePath", closeDirPath(getDataDirectory())).toString();
    MSSettings::roSettings()->setValue(lit("systemName"), QnUuid::createUuid().toString()); // random value
    MSSettings::roSettings()->setValue(lit("port"), 0);
    MSSettings::roSettings()->setValue(
        nx_ms_conf::MIN_STORAGE_SPACE,
        (qint64) std::numeric_limits<int64_t>::max());

    MediaServerProcess mserverProcessor(argc, argv);
    InitStoragesTestWorker worker(mserverProcessor);

    mserverProcessor.run();

    ASSERT_TRUE(1);
}

TEST(SaveRestoreStoragesInfoFromConfig, main)
{
    auto resourcePool = std::unique_ptr<QnResourcePool>(new QnResourcePool);

    auto commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
    commonModule->setModuleGUID(QnUuid("6F789D28-B675-49D9-AEC0-CEFFC99D674E"));

    auto platformAbstraction = std::unique_ptr<QnPlatformAbstraction>(new QnPlatformAbstraction);

    QnStorageResourcePtr storage1(new QnFileStorageResource);
    QnStorageResourcePtr storage2(new QnFileStorageResource);

    QString path1 = lit("c:\\some path\\data1");
    QString path2 = lit("f:\\some\\other\\path\\data1");

    qint64 spaceLimit1 = 50 * 1024 * 1024l;
    qint64 spaceLimit2 = 255 * 1024 * 1024l;

    storage1->setUrl(path1);
    storage2->setUrl(path2);
    storage1->setSpaceLimit(spaceLimit1);
    storage2->setSpaceLimit(spaceLimit2);

    QnStorageResourceList storageList;
    BeforeRestoreDbData restoreData;
    storageList << storage1 << storage2;

    aux::saveStoragesInfoToBeforeRestoreData(&restoreData, storageList);

    ASSERT_TRUE(restoreData.hasInfoForStorage(path1));
    ASSERT_TRUE(restoreData.hasInfoForStorage(path2));

    ASSERT_EQ(restoreData.getSpaceLimitForStorage(path1), spaceLimit1);
    ASSERT_EQ(restoreData.getSpaceLimitForStorage(path2), spaceLimit2);
}
