#include "appserver2_process_settings.h"

#include <nx/utils/app_info.h>
#include <nx/utils/timer_manager.h>

namespace ec2 {
namespace conf {

static const std::chrono::seconds kDefaultDelayBeforeSettingMasterFlag(31);

CloudIntegration::CloudIntegration():
    delayBeforeSettingMasterFlag(kDefaultDelayBeforeSettingMasterFlag)
{
}

//-------------------------------------------------------------------------------------------------

Settings::Settings():
    m_settings(nx::utils::AppInfo::organizationNameForSettings(), "Nx Appserver2", "appserver2"),
    m_showHelp(false)
{
}

void Settings::load(int argc, char** argv)
{
    using namespace std::chrono;

    m_settings.parseArgs(argc, (const char**)argv);

    loadCloudIntegration();
}

bool Settings::showHelp()
{
    return m_settings.value("help", "not present").toString() != "not present";
}

QString Settings::dbFilePath() const
{
    return m_settings.value("dbFile", "ecs.sqlite").toString();
}

bool Settings::isP2pMode() const
{
    return m_settings.value("p2pMode", false).toBool();
}

int Settings::moduleInstance() const
{
    return m_settings.value("moduleInstance").toInt();
}

SocketAddress Settings::endpoint() const
{
    return SocketAddress(m_settings.value("endpoint", "0.0.0.0:0").toString());
}

QnUuid Settings::moduleGuid() const
{
    return QnUuid(m_settings.value("moduleGuid").toString());
}

bool Settings::isAuthDisabled() const
{
    return m_settings.contains("disableAuth");
}

const CloudIntegration& Settings::cloudIntegration() const
{
    return m_cloudIntegration;
}

void Settings::loadCloudIntegration()
{
    m_cloudIntegration.delayBeforeSettingMasterFlag =
        nx::utils::parseTimerDuration(
            m_settings.value("cloudIntegration/delayBeforeSettingMasterFlag").toString(),
            kDefaultDelayBeforeSettingMasterFlag);

    if (m_settings.contains("cloudIntegration/cloudDbUrl"))
    {
        m_cloudIntegration.cloudDbUrl = 
            m_settings.value("cloudIntegration/cloudDbUrl").toString();
    }
}

} // namespace conf
} // namespace ec2
