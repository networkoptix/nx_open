#include "protocol_handler.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <nx/utils/software_version.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace utils {

bool registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& customization,
    const SoftwareVersion& version)
{
    const auto appsLocation = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (appsLocation.isEmpty())
        return false;

    const QString handlerFile(QDir(appsLocation).filePath(protocol + lit(".desktop")));

    const QString kEntry(lit("Desktop Entry"));

    if (QFile::exists(handlerFile))
    {
        QScopedPointer<QSettings> settings(new QSettings(handlerFile, QSettings::IniFormat));
        settings->beginGroup(kEntry);
        SoftwareVersion existingVersion(settings->value(lit("Version")).toString());
        if (existingVersion > version)
            return true;
        if (existingVersion == version &&
            settings->value(lit("Exec")).toString() == applicationBinaryPath)
        {
            return true;
        }
    }

    // We can't use QSettings to write the file because it escapes spaces in the group names.
    // This behavior cannot be changed.

    QFile file(handlerFile);
    if (!file.open(QFile::WriteOnly))
        return false;

    QString result(lit("[%1]\n").arg(kEntry));

    result += lit("Version=%1\n").arg(version.toString());
    result += lit("Name=%1\n").arg(applicationName);
    result += lit("Exec=%1\n").arg(applicationBinaryPath);
    result += lit("Comment=%1\n").arg(description);
    result += lit("Icon=vmsclient-%1.png\n").arg(customization);
    result += lit("Type=%1\n").arg(lit("Application"));
    result += lit("StartupNotify=%1\n").arg(lit("true"));
    result += lit("Terminal=%1\n").arg(lit("false"));
    result += lit("NoDisplay=%1\n").arg(lit("true"));
    result += lit("MimeType=x-scheme-handler/%1\n").arg(protocol);

    file.write(result.toUtf8());
    file.close();

    NX_LOG(lit("Scheme handler file updated: %1").arg(handlerFile), cl_logINFO);

    return true;
}

} // namespace utils
} // namespace nx
