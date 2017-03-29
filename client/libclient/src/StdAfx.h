#define QT_NO_CAST_FROM_ASCII

#include <nx/utils/compiler_options.h>

/* Windows headers. */
#ifdef _WIN32
#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>

/* DXVA headers (should be included before ffmpeg headers). */
#   ifdef _USE_DXVA
#       include <d3d9.h>
#       include <dxva2api.h>
#       include <windows.h>
#       include <windowsx.h>
#       include <ole2.h>
#       include <commctrl.h>
#       include <shlwapi.h>
#       include <Strsafe.h>
#   endif
#endif

/* FFMPEG headers. */
#ifdef __cplusplus
extern "C" {
#endif
#   include <libavcodec/avcodec.h>
#   include <libavformat/avformat.h>
#   ifdef _USE_DXVA
#       include <libavcodec/dxva2.h>
#   endif
#   include <libswscale/swscale.h>
#   include <libavutil/avstring.h>
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/* STL headers. */
#include <algorithm>
#include <functional>

/* Boost headers. */
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

/* QT headers. */

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <ui/dialogs/common/message_box.h>

#endif

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
