#ifndef _MEDIASERVER_SETTINGS_H_
#define _MEDIASERVER_SETTINGS_H_

#include <QtCore/QSettings>


namespace nx_ms_conf
{
    static const QLatin1String MIN_STORAGE_SPACE( "minStorageSpace" );
}

/*!
    QSettings instance is initialized in \a initializeROSettingsFromConfFile or \a initializeRunTimeSettingsFromConfFile call or on first demand
*/
class MSSettings
{
public:
    static const int DEFAUT_RTSP_PORT = 7001;
    static const int DEFAULT_STREAMING_PORT = 7001;

    static QString defaultROSettingsFilePath();
    static void initializeROSettingsFromConfFile( const QString& fileName );
    static QSettings* roSettings();

    static QString defaultRunTimeSettingsFilePath();
    static void initializeRunTimeSettingsFromConfFile( const QString& fileName );
    static QSettings* runTimeSettings();
};

#endif
