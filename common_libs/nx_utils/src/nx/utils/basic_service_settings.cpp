#include "basic_service_settings.h"

namespace nx {
namespace utils {

BasicServiceSettings::BasicServiceSettings(
    const QString& organizationName,
    const QString& applicationName,
    const QString& moduleName)
    :
    m_settings(
        organizationName,
        applicationName,
        moduleName),
    m_showHelp(false)
{
}

void BasicServiceSettings::load(int argc, const char **argv)
{
    m_settings.parseArgs(argc, argv);
    if (m_settings.contains("--help"))
        m_showHelp = true;

    loadSettings();
}

bool BasicServiceSettings::isShowHelpRequested() const
{
    return m_showHelp;
}

void BasicServiceSettings::printCmdLineArgsHelp()
{
    // TODO
}

const QnSettings& BasicServiceSettings::settings() const
{
    return m_settings;
}

} // namespace utils
} // namespace nx
