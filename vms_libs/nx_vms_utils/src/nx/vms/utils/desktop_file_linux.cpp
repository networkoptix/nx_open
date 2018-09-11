#include "desktop_file_linux.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <nx/utils/literal.h>

#include "app_info.h"

namespace nx {
namespace vms {
namespace utils {

using nx::utils::SoftwareVersion;

bool createDesktopFile(const QString& filePath,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& icon,
    const SoftwareVersion& version,
    const QString& protocol)
{
    // We can't use QSettings to write the file because it escapes spaces in the group names.
    // This behavior cannot be changed.

    QString result(lit("[Desktop Entry]\n"));
    result += lit("Type=%1\n").arg(lit("Application"));
    result += lit("Name=%1\n").arg(applicationName);
    result += lit("Comment=%1\n").arg(description);
    result += lit("Icon=%1\n").arg(icon);
    result += lit("Exec=\"%1\" %u\n").arg(applicationBinaryPath);
    result += lit("StartupNotify=%1\n").arg(lit("true"));
    result += lit("StartupWMClass=%1\n").arg(AppInfo::productNameShort());
    result += lit("Terminal=%1\n").arg(lit("false"));

    if (!version.isNull())
        result += lit("Version=%1\n").arg(version.toString());

    if (!protocol.isEmpty())
    {
        result += lit("NoDisplay=%1\n").arg(lit("true"));
        result += lit("MimeType=x-scheme-handler/%1\n").arg(protocol);
    }

    QFile file(filePath);
    if (!file.open(QFile::WriteOnly))
        return false;

    file.write(result.toUtf8());
    file.close();

    return true;
}

} // namespace utils
} // namespace vms
} // namespace nx
