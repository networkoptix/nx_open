#include "settings.h"

#include <QtCore/QStandardPaths>

#include <nx/utils/app_info.h>

#include "time_server_app_info.h"

namespace nx {
namespace time_server {
namespace conf {

namespace {
const QLatin1String kDataDir("dataDir");
} // namespace

static const QString kModuleName = lit("time_server");

Settings::Settings():
    base_type(
        nx::utils::AppInfo::organizationNameForSettings(),
        TimeServerAppInfo::applicationName(),
        kModuleName)
{
}

QString Settings::dataDir() const
{
    return m_dataDir;
}

nx::utils::log::Settings Settings::logging() const
{
    return m_logging;
}

void Settings::loadSettings()
{
    const QString& dataDirFromSettings = settings().value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
    {
        m_dataDir = dataDirFromSettings;
        return;
    }

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/%2/var")
        .arg(nx::utils::AppInfo::linuxOrganizationName()).arg(kModuleName);
    QString varDirName = settings().value("varDir", defVarDirName).toString();
    m_dataDir = varDirName;
#else
    const QStringList& dataDirList =
        QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    m_dataDir = dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

} // namespace conf
} // namespace time_server
} // namespace nx
