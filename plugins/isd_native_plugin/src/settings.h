/**********************************************************
* Jul 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_ISD_EDGE_SETTINGS_H
#define NX_ISD_EDGE_SETTINGS_H

#include <string>


namespace isd_edge_settings
{
    static const char paramGroup[] = "isd_edge";

    static const char vbrMinPercent[] = "isd_edge/vbrMinPercent";
    static const int vbrMinPercentDefault = 20;

    static const char vbrMaxPercent[] = "isd_edge/vbrMaxPercent";
    static const int vbrMaxPercentDefault = 120;
}

#endif  //NX_ISD_EDGE_SETTINGS_H
