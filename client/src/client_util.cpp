#include "settings.h"



QString getTempRecordingDir()
{
    QString path = Settings::instance().mediaRoot() + QLatin1String("/_temp/");
    if (!QDir(path).exists())
        QDir().mkpath(path);
    return path;
}

QString getRecordingDir()
{
    return Settings::instance().mediaRoot() + QLatin1String("/_Recorded/");
}
