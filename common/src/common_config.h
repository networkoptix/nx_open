/**********************************************************
* 14 nov 2013
* a.kolesnikov
* Defines macro controlling compile-time presence of certain functionality
***********************************************************/

#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

//DISABLE_ALL_VENDORS introduced to simplify enabling specified vendors. E.g. to enable onvif only define DISABLE_ALL_VENDORS and ENABLE_ONVIF macros
    //note: at the moment only onvif functionality can be enabled/disabled

#if !defined(DISABLE_ONVIF) && !defined(DISABLE_ALL_VENDORS)
#define ENABLE_ONVIF
#endif

#endif  //COMMON_CONFIG_H
