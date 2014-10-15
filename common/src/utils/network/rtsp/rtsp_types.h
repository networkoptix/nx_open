/**********************************************************
* 31 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_RTSP_TYPES_H
#define NX_RTSP_TYPES_H

#include "../http/httptypes.h"


namespace nx_rtsp
{
    const int DEFAULT_RTSP_PORT = 554;

    static const nx_http::MimeProtoVersion rtsp_1_0 = { "RTSP", "1.0" };
}

#endif  //NX_RTSP_TYPES_H
