// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QFile>
#include <QProcess>
#include <QtCore/QDir>
#include <QMimeDatabase>

#include <nx/utils/log/log.h>

#include "shortcuts_linux.h"


QnLinuxShortcuts::QnLinuxShortcuts(QObject *parent /* = nullptr*/):
    QnPlatformShortcuts(parent)
{

}

bool QnLinuxShortcuts::createShortcut(
    const QString& sourceFile,
    const QString& destinationDir,
    const QString& name,
    /*unused*/ const QStringList& /*arguments*/,
    const QVariant& icon)
{
    QString result("[Desktop Entry]");
    result += "\nType=Application";
    result += "\nName=" + name;
    result += "\nStartupNotify=true";
    result += "\nTerminal=false";

    const bool isApplication = QFileInfo(sourceFile).isExecutable();

    if (isApplication)
        result += QString("\nExec=\"%1\"").arg(sourceFile);
    else
        result += QString("\nExec=xdg-open \"%1\"").arg(sourceFile);

    if (!icon.isNull() && icon.canConvert<QString>())
    {
        result += QString("\nIcon=%1").arg(icon.toString());
    }
    else
    {
        if (!isApplication)
        {
            QMimeType sourceMimeType = QMimeDatabase().mimeTypeForFile(sourceFile);
            result += QString("\nIcon=%1").arg(sourceMimeType.iconName());
        }
    }

    result += "\n";

    QString destinationFilePath = QDir(destinationDir).absoluteFilePath(name + ".desktop");

    QFile file(destinationFilePath);
    if (!file.open(QFile::WriteOnly))
        return false;

    file.write(result.toUtf8());
    file.close();

    return true;
}

bool QnLinuxShortcuts::deleteShortcut(const QString& /*destinationDir*/,
                                      const QString& /*name*/) const
{
    return true;
}

bool QnLinuxShortcuts::shortcutExists(const QString& /*destinationDir*/,
                                      const QString& /*name*/) const
{
    return false;
}

QnPlatformShortcuts::ShortcutInfo QnLinuxShortcuts::getShortcutInfo(
    const QString& /*destinationDir*/,
    const QString& /*name*/) const
{
    return {};
}

bool QnLinuxShortcuts::supported() const
{
    return false;
}
