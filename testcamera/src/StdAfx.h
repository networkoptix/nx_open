#include <nx/utils/compiler_options.h>

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

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
