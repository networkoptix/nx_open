// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings.h"

#include <QtCore/QStandardPaths>

#include <nx/branding.h>
#include <nx/utils/app_info.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/timer_manager.h>

namespace nx::network::http::server::test {

namespace {
static constexpr char kDataDir[] = "dataDir";
} // namespace

//-------------------------------------------------------------------------------------------------

static constexpr char kModuleName[] = "simple_service";

QString Settings::dataDir() const
{
    const QString& dataDirFromSettings = settings().value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

#if defined(Q_OS_LINUX)
    QString defVarDirName =
        QString("/opt/%1/%2/var").arg(nx::branding::companyId()).arg(kModuleName);
    QString varDirName = settings().value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList =
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

log::Settings Settings::logging() const
{
    return m_logging;
}

//-------------------------------------------------------------------------------------------------
// Settings

Settings::Settings():
    base_type(
        nx::utils::AppInfo::organizationNameForSettings(), nx::branding::company(), kModuleName)
{
}

void Settings::loadSettings()
{
    m_logging.load(settings(), "log");
    m_httpSettings.load(settings());
}

} // namespace nx::network::http::server::test
