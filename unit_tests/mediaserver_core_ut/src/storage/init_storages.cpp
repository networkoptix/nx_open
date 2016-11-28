#include "media_server_process.h"
#include "platform/platform_abstraction.h"
#include "utils/common/long_runnable.h"
#include "media_server/media_server_module.h"
#include "media_server/settings.h"
#include "utils.h"
#include "utils/common/util.h"
#include "core/resource/storage_resource.h"
#include "core/resource_management/resource_pool.h"

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
    MSSettings::roSettings()->setValue(nx_ms_conf::MIN_STORAGE_SPACE, std::numeric_limits<int64_t>::max());

    MediaServerProcess mserverProcessor(argc, argv);
    InitStoragesTestWorker worker(mserverProcessor);

    mserverProcessor.run();

    ASSERT_TRUE(1);
}
