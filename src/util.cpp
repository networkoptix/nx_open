#include "util.h"
#include "settings.h"

QString fromNativePath(QString path)
{
    path = QDir::cleanPath(QDir::fromNativeSeparators(path));

    if (!path.isEmpty() && path.endsWith(QLatin1Char('/')))
        path.chop(1);

    return path;
}

QString getDataDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
}

QString getMoviesDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::MoviesLocation);
}

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

QString formatDuration(unsigned duration, unsigned total)
{
    unsigned hours = duration / 3600;
    unsigned minutes = (duration % 3600) / 60;
    unsigned seconds = duration % 60;

    if (total == 0)
    {
        if (hours == 0)
            return QString(QLatin1String("   %1:%2")).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));

        return QString(QLatin1String("%1:%2:%3")).arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }
    else
    {
        unsigned totalHours = total / 3600;
        unsigned totalMinutes = (total % 3600) / 60;
        unsigned totalSeconds = total % 60;

        QString secondsString, totalString;

        if (totalHours == 0)
        {
            secondsString = QString(QLatin1String("%1:%2")).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
            totalString = QString(QLatin1String("   %1:%2")).arg(totalMinutes, 2, 10, QLatin1Char('0')).arg(totalSeconds, 2, 10, QLatin1Char('0'));
        }
        else
        {
            secondsString = QString(QLatin1String("%1:%2:%3")).arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
            totalString = QString(QLatin1String("%1:%2:%3")).arg(totalHours).arg(totalMinutes, 2, 10, QLatin1Char('0')).arg(totalSeconds, 2, 10, QLatin1Char('0'));
        }

        return secondsString + QLatin1Char('/') + totalString;
    }
}

int digitsInNumber(unsigned num)
{
    int digits = 1;
    while(num /= 10)
        digits++;

    return digits;
}


QString getParamFromString(const QString& str, const QString& param)
{
    if (!str.contains(param))
        return QString();

    int param_index = str.indexOf(param);
    param_index += param.length();

    int first_index = str.indexOf(QLatin1Char('\"'), param_index);
    if (first_index == -1)
        return QString();

    int second_index = str.indexOf(QLatin1Char('\"'), first_index + 1);

    return str.mid(first_index+1, second_index - (first_index+1));
}
