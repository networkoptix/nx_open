#include "util.h"

static const QString DEFAULT_MEDIA_DIR = "c:/EVEMedia/";

QString getDataDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
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

int digitsInNumber(unsigned num)
{
    int digits = 1;
    while(num /= 10)
        digits++;

    return digits;
}

