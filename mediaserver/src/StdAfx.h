#include <common/config.h>
#include <common/common_globals.h>

/* Boost headers. */
#include <boost/foreach.hpp>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #ifdef _USE_DXVA
    #include <libavcodec/dxva2.h>
    #endif
    #include <libswscale/swscale.h>
    #include <libavutil/avstring.h>
}
