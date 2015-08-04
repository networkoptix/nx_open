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

    static const char hiVbrMinPercent[] = "isd_edge/hiVbrMinPercent";
    static const int hiVbrMinPercentDefault = 20;

    static const char hiVbrMaxPercent[] = "isd_edge/hiVbrMaxPercent";
    static const int hiVbrMaxPercentDefault = 80;

    static const char loVbrMinPercent[] = "isd_edge/loVbrMinPercent";
    static const int loVbrMinPercentDefault = 20;

    static const char loVbrMaxPercent[] = "isd_edge/loVbrMaxPercent";
    static const int loVbrMaxPercentDefault = 120;
}

#endif  //NX_ISD_EDGE_SETTINGS_H
