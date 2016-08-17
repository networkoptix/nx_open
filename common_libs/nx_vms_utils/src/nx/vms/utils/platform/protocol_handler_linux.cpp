#include "protocol_handler.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <nx/utils/software_version.h>
#include <nx/utils/log/log.h>
#include <nx/vms/utils/desktop_file_linux.h>

namespace nx {
namespace vms {
namespace utils {

using nx::utils::SoftwareVersion;

bool registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& macOsBundleId,
    const QString& applicationName,
    const QString& description,
    const QString& customization,
    const SoftwareVersion& version)
{
    Q_UNUSED(macOsBundleId)
    
    const auto appsLocation = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (appsLocation.isEmpty())
        return false;

    const QString handlerFile(QDir(appsLocation).filePath(protocol + lit(".desktop")));

    if (QFile::exists(handlerFile))
    {
        QScopedPointer<QSettings> settings(new QSettings(handlerFile, QSettings::IniFormat));
        settings->beginGroup(lit("Desktop Entry"));
        SoftwareVersion existingVersion(settings->value(lit("Version")).toString());
        if (existingVersion > version)
            return true;
        if (existingVersion == version &&
            settings->value(lit("Exec")).toString() == applicationBinaryPath)
        {
            return true;
        }
    }

    bool ok = createDesktopFile(
        handlerFile,
        applicationBinaryPath,
        applicationName,
        description,
        customization,
        version,
        protocol);

    if (!ok)
        return false;

    NX_LOG(lit("Scheme handler file updated: %1").arg(handlerFile), cl_logINFO);

    return true;
}

} // namespace utils
} // namespace vms
} // namespace nx
