#include "settings.h"

#include <QtCore/QStandardPaths>

#include <utils/common/app_info.h>

#include "time_server_app_info.h"

namespace nx {
namespace time_server {
namespace conf {

namespace {
const QLatin1String kDataDir("dataDir");
} // namespace

static const QString kModuleName = lit("time_server");

Settings::Settings():
    m_settings(TimeServerAppInfo::applicationName(), kModuleName),
    m_showHelpRequested(false)
{
}

bool Settings::isShowHelpRequested() const
{
    return m_showHelpRequested;
}

void Settings::load(int argc, char **argv)
{
    m_commandLineParser.parse(argc, argv, stderr);
    m_settings.parseArgs(argc, (const char**)argv);

    m_logging.load(m_settings, QLatin1String("log"));
}

void Settings::printCmdLineArgsHelp()
{
    // TODO: 
}

QString Settings::dataDir() const
{
    const QString& dataDirFromSettings = m_settings.value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/%2/var")
        .arg(QnAppInfo::linuxOrganizationName()).arg(kModuleName);
    QString varDirName = m_settings.value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList =
        QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

const QnLogSettings& Settings::logging() const
{
    return m_logging;
}

} // namespace conf
} // namespace time_server
} // namespace nx
