#include "media_server_module_fixture.h"

#include <fstream>

#include <nx/utils/log/log_message.h>
#include <nx/utils/std/cpp14.h>

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

    m_serverModule = std::make_unique<QnMediaServerModule>(QString(), confFilePath);

    const QnUuid moduleGuid("{A680980C-70D1-4545-A5E5-72D89E33648B}");
    m_serverModule->commonModule()->setModuleGUID(moduleGuid);
}
