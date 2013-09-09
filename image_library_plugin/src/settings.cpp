/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "settings.h"


Settings::Settings()
:
    frameDurationUsec( 3*1000*1000 )
{
    //imageDirectories.push_back( "C:\\Users\\a.kolesnikov\\Documents\\girl\\" );
    imageDirectories.push_back( "C:\\temp\\telki\\" );
}

Settings* Settings::instance()
{
    static Settings settingsInstance;
    return &settingsInstance;
}
