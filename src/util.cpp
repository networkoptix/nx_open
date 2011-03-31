#include "util.h"

static const QString DEFAULT_MEDIA_DIR = "c:/EVEMedia/";

QString getDataDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
}

QString getMoviesDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::MoviesLocation);
}

QString getMediaRootDir()
{
    QFile settingsFile(getDataDirectory() + "/settings.xml");
    if (!settingsFile.exists())
        return DEFAULT_MEDIA_DIR;

    if (!settingsFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return DEFAULT_MEDIA_DIR;

    QXmlQuery query;
    query.setFocus(&settingsFile);
    query.setQuery("settings/config/mediaRoot/text()");
    if (!query.isValid())
        return DEFAULT_MEDIA_DIR;

    QString rootDir;
    query.evaluateTo(&rootDir);
    settingsFile.close();

    rootDir = QDir::fromNativeSeparators(rootDir.trimmed());
    if (!QDir(rootDir).exists())
        return DEFAULT_MEDIA_DIR;

    if (rootDir.length()<1)
        return DEFAULT_MEDIA_DIR;


    if (rootDir.at(rootDir.length()-1) != '/')
        rootDir += QString("/");


    return rootDir;
}


QString getTempRecordingDir()
{
    return getMediaRootDir()  + QString("_temp/");
}

QString getRecordingDir()
{
    return getMediaRootDir()  + QString("_Recorded/");
}

QString formatDuration(unsigned duration, unsigned total)
{
    unsigned hours = duration / 3600;
    unsigned minutes = (duration % 3600) / 60;
    unsigned seconds = duration % 60;

    if (total == 0)
    {
        if (hours == 0)
            return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));

        return QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    }
    else
    {
        unsigned totalHours = total / 3600;
        unsigned totalMinutes = (total % 3600) / 60;
        unsigned totalSeconds = total % 60;

        QString secondsString, totalString;

        if (totalHours == 0)
        {
            secondsString = QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
            totalString = QString("%1:%2").arg(totalMinutes, 2, 10, QChar('0')).arg(totalSeconds, 2, 10, QChar('0'));
        }
        else
        {
            secondsString = QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
            totalString = QString("%1:%2:%3").arg(totalHours).arg(totalMinutes, 2, 10, QChar('0')).arg(totalSeconds, 2, 10, QChar('0'));
        }

        return secondsString + "/" + totalString;
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
        return "";

    
    int param_index = str.indexOf(param);
    param_index+=param.length();

    int first_index = str.indexOf("\"", param_index);
    if (first_index<0)
        return "";

    int second_index = str.indexOf("\"", first_index + 1);

    QString result = str.mid(first_index+1, second_index - (first_index+1));

    return result;    
}