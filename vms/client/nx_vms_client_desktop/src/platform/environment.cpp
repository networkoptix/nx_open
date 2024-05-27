// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "environment.h"

#ifndef QT_NO_PROCESS

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <nx/utils/string.h>
#include <nx/utils/platform/process.h>
#include <utils/mac_utils.h>

#ifdef Q_OS_WIN
namespace
{
    QString searchInPath(const QString& executable)
    {
        QString relativePath = executable;

        if (relativePath.isEmpty())
            return QString();

        QFileInfo fi(relativePath);
        if (fi.isAbsolute() && fi.exists())
            return relativePath;

        if (!relativePath.endsWith(lit(".exe")))
            relativePath.append(lit(".exe"));

        const QChar sep = QLatin1Char(';');
        const QChar slash = QLatin1Char('/');
        for (const QString &p : QProcessEnvironment::systemEnvironment().value(QLatin1String("PATH")).split(sep))
        {
            QString fullPath = p + slash + relativePath;

            const QFileInfo fi(fullPath);
            if (fi.exists())
                return fi.absoluteFilePath();
        }

        return QString();
    }
}
#endif // Q_OS_WIN

void QnEnvironment::showInGraphicalShell(const QString &path)
{
#if defined(Q_OS_MAC)
    mac_openInFinder(path);
    return;
#endif

#if defined(Q_OS_WIN)
    const QString explorer = searchInPath(QLatin1String("explorer.exe"));
    if (!explorer.isEmpty())
    {
        QStringList params;
        QFileInfo info(path);
        if (!info.exists())
        {
            // Same as on other systems - show folder contents
            params << QDir::toNativeSeparators(info.path());
        }
        else
        {
            if (!info.isDir())
                params << QLatin1String("/select,");
            params << QDir::toNativeSeparators(path);
        }
        nx::utils::startProcessDetached(explorer, params);
    }
    else
#endif
    {
        QDesktopServices::openUrl(QUrl(lit("file:///") + QFileInfo(path).path(), QUrl::TolerantMode));
    }
}

QString QnEnvironment::getUniqueFileName(const QString &dirName, const QString &baseName) {
    QStringList existingFiles;
    for (const QFileInfo &info: QDir(dirName).entryInfoList(QDir::Files)) {
        existingFiles << info.completeBaseName();
    }

    QString name = nx::utils::generateUniqueString(existingFiles, baseName, baseName + lit("_%1"));
    return QFileInfo(dirName, name).absoluteFilePath();
}

#endif // QT_NO_PROCESS
