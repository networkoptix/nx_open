#include "media_server_module_fixture.h"

#include <fstream>

#include <nx/utils/log/log_message.h>
#include <nx/utils/std/cpp14.h>
#include <recorder/storage_manager.h>
#include <platform/platform_abstraction.h>
#include <nx/vms/server/command_line_parameters.h>

MediaServerModuleFixture::MediaServerModuleFixture():
    nx::utils::test::TestWithTemporaryDirectory("MediaServerModuleTest", QString())
{
}

QnMediaServerModule& MediaServerModuleFixture::serverModule()
{
    return *m_serverModule;
}

void MediaServerModuleFixture::SetUp()
{
    const auto confFilePath = lm("%1/mserver.conf").args(testDataDir()).toQString();

    std::ofstream iniFile;
    iniFile.open(confFilePath.toStdString().c_str());
    ASSERT_TRUE(iniFile.is_open());
    iniFile << "dataDir = " << testDataDir().toStdString() << std::endl;
    iniFile.close();
    nx::vms::server::CmdLineArguments arguments(QCoreApplication::arguments());
    arguments.configFilePath = confFilePath;
    m_serverModule = std::make_unique<QnMediaServerModule>(&arguments);

    const QnUuid moduleGuid("{A680980C-70D1-4545-A5E5-72D89E33648B}");
    m_serverModule->commonModule()->setModuleGUID(moduleGuid);
    m_serverModule->setPlatform(&m_platform);
    m_serverModule->platform()->monitor()->setServerModule(m_serverModule.get());
}

void MediaServerModuleFixture::TearDown()
{
    m_serverModule.reset();
}
