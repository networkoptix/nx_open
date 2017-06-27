#include "environment.h"

#ifndef QT_NO_PROCESS

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <nx/utils/string.h>
#include <utils/mac_utils.h>

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

#ifdef Q_OS_WIN
        if (!relativePath.endsWith(lit(".exe")))
            relativePath.append(lit(".exe"));
#endif

#ifdef Q_OS_WIN
        const QChar sep = QLatin1Char(';');
#else
        const QChar sep = QLatin1Char(':');
#endif

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
        if (!QFileInfo(path).isDir())
            params << QLatin1String("/select,");
        params << QDir::toNativeSeparators(path);
        QProcess::startDetached(explorer, params);
    }
    else
#endif
    {
        // TODO: #Elric implement as in Qt Creator.
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
