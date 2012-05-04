#include "settings.h"
#include <QDir>

QString getTempRecordingDir()
{
    QString path = qnSettings->mediaRoot() + QLatin1String("/_temp/");
    if (!QDir(path).exists())
        QDir().mkpath(path);
    return path;
}

QString getRecordingDir()
{
    return qnSettings->mediaRoot() + QLatin1String("/_Recorded/");
}
