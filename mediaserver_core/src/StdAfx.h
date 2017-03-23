#include <nx/utils/compiler_options.h>

/* Windows headers. */
#ifdef _WIN32
#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>

#endif

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #ifdef _USE_DXVA
    #include <libavcodec/dxva2.h>
    #endif
    #include <libswscale/swscale.h>
    #include <libavutil/avstring.h>
}

#ifdef __cplusplus
/* This file should be included only main in vms projects. */
#include <common/common_globals.h>
#endif

/* Boost headers. */
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#ifdef __cplusplus

#include <QtCore/QObject>
#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>
#include <QtCore/QIODevice>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QQueue>
#include <QtCore/QSet>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/qmath.h>
#include <QtCore/QtDebug>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAuthenticator>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#endif

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
