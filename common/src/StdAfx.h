#define QT_NO_CAST_FROM_ASCII

#include <common/config.h>
#ifdef __cplusplus
    #include <common/common_globals.h>
#endif

/* Windows headers. */
#ifdef _WIN32
#   define FD_SETSIZE 2048

#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>

#   if _MSC_VER < 1800
#       define noexcept
#   endif


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
#else
#    include <arpa/inet.h>
#endif


// ffmpeg headers
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

#include "utils/media/sse_helper.h"

/* STL headers. */
#include <algorithm>
#include <functional>

/* Boost headers. */
#include <boost/foreach.hpp>
#include <boost/array.hpp>

/* QT headers. */

//#ifdef Q_OS_WIN
//#include <QtMultimedia/QAudio>
//#include <QtMultimedia/QAudioFormat>
//#include <QtMultimedia/QAudioOutput>
//#endif

#include <QAuthenticator>
#include <QtCore/QBuffer>
#include <QtGui/QCloseEvent>
#include <QtGui/QColor>
#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QtCore/QEasingCurve>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtNetwork/QHostAddress>
#include <QtCore/QIODevice>
#include <QtGui/QKeyEvent>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtGui/QMouseEvent>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtNetwork/QNetworkAddressEntry>
#include <QtNetwork/QNetworkInterface>
#include <QtCore/QObject>
#include <QtGui/QPaintEngine>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtCore/QPair>
#include <QtCore/QParallelAnimationGroup>
#include <QtGui/QPixmap>
#include <QPixmapCache>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QQueue>
#include <QtGui/QRadialGradient>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSemaphore>
#include <QtCore/QSet>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QStringListModel>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtNetwork/QUdpSocket>
#include <QtCore/QWaitCondition>
#include <QtGui/QWheelEvent>
#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/qmath.h>
#include <QtCore/QtDebug>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QFileSystemWatcher>
#include <QtCore/QUrlQuery>
#include <QtCore/QUuid>

#endif
