/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#ifndef ILP_SETTINGS_H
#define ILP_SETTINGS_H

#include <list>
#include <string>


class Settings
{
public:
    //!Period (in microseconds) for picture to stay on screen. By default, 3000000
    unsigned int frameDurationUsec;

    Settings();

    static Settings* instance();
};

#endif  //ILP_SETTINGS_H
