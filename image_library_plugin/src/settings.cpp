/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "settings.h"


Settings::Settings()
:
    frameDurationUsec( 3000000 )
{
    imageDirectories.push_back( "C:\\Users\\a.kolesnikov\\Documents\\girl\\" );
}

Settings* Settings::instance()
{
    static Settings settingsInstance;
    return &settingsInstance;
}
