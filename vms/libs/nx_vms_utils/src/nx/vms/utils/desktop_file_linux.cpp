// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_file_linux.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <nx/branding.h>

namespace nx {
namespace vms {
namespace utils {

using nx::utils::SoftwareVersion;

QString iconFileName()
{
    return QString("vmsclient-%1.png").arg(nx::branding::customization());
}

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

    QString result("[Desktop Entry]");
    result += "\nType=Application";
    result += "\nName=" + applicationName;
    result += "\nComment=" + description;
    result += "\nIcon=" + icon;
    result += "\nExec=\"" + applicationBinaryPath + "\" %u";
    result += "\nStartupNotify=true";
    result += "\nStartupWMClass=" + nx::branding::brand();
    result += "\nTerminal=false";

    if (!version.isNull())
        result += "\nVersion=" + version.toString();

    if (!protocol.isEmpty())
    {
        result += "\nNoDisplay=true";
        result += "\nMimeType=x-scheme-handler/" + protocol;
    }

    result += "\n";

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
