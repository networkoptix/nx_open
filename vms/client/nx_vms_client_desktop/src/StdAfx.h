// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/* Windows headers. */
#ifdef _WIN32
#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>

#endif

/* FFMPEG headers. */
#ifdef __cplusplus
extern "C" {
#endif
#   include <libavcodec/avcodec.h>
#   include <libavformat/avformat.h>
#   include <libswscale/swscale.h>
#   include <libavutil/avstring.h>
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/* STL headers. */
#include <algorithm>
#include <functional>

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
