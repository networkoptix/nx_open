/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "settings.h"


Settings::Settings()
:
    frameDurationUsec( 1*1000*1000 )
{
}

Settings* Settings::instance()
{
    static Settings settingsInstance;
    return &settingsInstance;
}
