#include "utils/common/long_runnable.h"
#include "media_server/media_server_module.h"
#include "media_server/settings.h"
#include "utils.h"
#include "utils/common/util.h"
#include "core/resource/storage_resource.h"
#include "core/resource_management/resource_pool.h"
#include "media_server_helper.h"

namespace nx {
namespace ut {
namespace utils {

MediaServerHelper::MediaServerHelper(const MediaServerTestFuncTypeList& testList) :
    m_testList(testList)
{
    int argc = 2;
    char* argv[] = { "", "-e" };

    m_platform.reset(new QnPlatformAbstraction());
    MSSettings::roSettings()->setValue(lit("serverGuid"), QnUuid::createUuid().toString());
    MSSettings::roSettings()->setValue(lit("removeDbOnStartup"), lit("1"));
    MSSettings::roSettings()->setValue(lit("dataDir"), *m_workDirResource.getDirName());
    MSSettings::roSettings()->setValue(lit("varDir"), *m_workDirResource.getDirName());

    QString dbDir = MSSettings::roSettings()->value("eventsDBFilePath", closeDirPath(getDataDirectory())).toString();
    MSSettings::roSettings()->setValue(lit("systemName"), QnUuid::createUuid().toString()); // random value
    MSSettings::roSettings()->setValue(lit("port"), 0);

    m_serverProcess.reset(new MediaServerProcess(argc, argv));

    QObject::connect(
        m_serverProcess.get(), 
        &MediaServerProcess::started, 
        [this]() 
        {
            for (auto test : m_testList)
                test();
            m_serverProcess->stopSync();
        });
}

void MediaServerHelper::start()
{
    m_serverProcess->run();
}

}
}
}