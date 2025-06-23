// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug.h"

#if defined(Q_OS_WIN)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>

#include <nx/branding.h>

namespace nx::utils::debug {

static OnAboutToBlockHandler onAboutToBlockHandler;

OnAboutToBlockHandler setOnAboutToBlockHandler(
    OnAboutToBlockHandler handler)
{
    return std::exchange(onAboutToBlockHandler, std::move(handler));
}

void onAboutToBlock()
{
    if (onAboutToBlockHandler)
        onAboutToBlockHandler();
}

QString formatFileName(QString name, FormatFilePattern formatPattern, const QDir& defaultDir)
{
    name.replace(
        QRegularExpression(QLatin1String("%T")),
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz")));

    #if defined(Q_OS_WIN)
        const qulonglong processId = GetCurrentProcessId();
    #else
        const qulonglong processId = getpid();
    #endif

    name.replace(
        QRegularExpression(QLatin1String("%P")),
        QString::number(processId));

    if (formatPattern == FormatFilePattern::desktopClient)
    {
        name.replace(
            QRegularExpression(QLatin1String("%N")),
            nx::branding::desktopClientInternalName());

        name.replace(
            QRegularExpression(QLatin1String("%V")),
            QCoreApplication::applicationVersion());
    }

    return QFileInfo(name).isAbsolute()
        ? name
        : defaultDir.absoluteFilePath(name);
}

QString formatFileName(QString name, const QDir& defaultDir)
{
    return formatFileName(std::move(name), FormatFilePattern::desktopClient, defaultDir);
}

} // namespace nx::utils::debug
