#include "basic_service_settings.h"

#include <QtCore/QStandardPaths>

#include <nx/utils/app_info.h>

namespace nx::utils {

static constexpr char kDataDir[] = "dataDir";

BasicServiceSettings::BasicServiceSettings(
    const QString& organizationName,
    const QString& applicationName,
    const QString& moduleName)
    :
    m_settings(
        organizationName,
        applicationName,
        moduleName),
    m_moduleName(moduleName)
{
}

void BasicServiceSettings::load(int argc, const char **argv)
{
    m_settings.parseArgs(argc, argv);
    if (m_settings.contains("--help"))
        m_showHelp = true;

    m_logging.load(settings());

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

QString BasicServiceSettings::dataDir() const
{
    const QString& dataDirFromSettings = settings().value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

#if defined(Q_OS_LINUX)
    QString defVarDirName = QString("/opt/%1/%2/var")
        .arg(nx::utils::AppInfo::linuxOrganizationName()).arg(m_moduleName);
    QString varDirName = settings().value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList =
        QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

utils::log::Settings BasicServiceSettings::logging() const
{
    return m_logging;
}

const QnSettings& BasicServiceSettings::settings() const
{
    return m_settings;
}

} // namespace nx::utils
