// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_service_settings.h"

#include <QtCore/QStandardPaths>

#include <nx/branding.h>

namespace nx::utils {

static constexpr char kDataDir[] = "general/dataDir";
static constexpr char kDataDirCompat[] = "dataDir";

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
    QString dataDirFromSettings;

    if (settings().contains(kDataDir))
        dataDirFromSettings = settings().value(kDataDir).toString();
    else if (settings().contains(kDataDirCompat))
        dataDirFromSettings = settings().value(kDataDirCompat).toString();

    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

#if defined(Q_OS_LINUX)
    QString defVarDirName = QString("/opt/%1/%2/var")
        .arg(nx::branding::companyId()).arg(m_moduleName);
    QString varDirName = settings().value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList =
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

log::Settings BasicServiceSettings::logging() const
{
    return m_logging;
}

const QnSettings& BasicServiceSettings::settings() const
{
    return m_settings;
}

} // namespace nx::utils
