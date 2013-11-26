#ifndef _MEDIASERVER_SETTINGS_H_
#define _MEDIASERVER_SETTINGS_H_

#include <QtCore/QSettings>


/*!
    QSettings instance is initialized in \a initializeROSettingsFromConfFile or \a initializeRunTimeSettingsFromConfFile call or on first demand
*/
class MSSettings
{
public:
    static QString defaultROSettingsFilePath();
    static void initializeROSettingsFromConfFile( const QString& fileName );
    static QSettings* roSettings();

    static QString defaultRunTimeSettingsFilePath();
    static void initializeRunTimeSettingsFromConfFile( const QString& fileName );
    static QSettings* runTimeSettings();
};

#endif
