#define QT_NO_CAST_FROM_ASCII

#include <nx/utils/compiler_options.h>

#ifdef _WIN32
#   define FD_SETSIZE 2048
#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#else
#   include <arpa/inet.h>
#endif

#ifdef __cplusplus

/* Useful utils */
#include <common/common_globals.h>

/* STL headers. */
#include <algorithm>
#include <functional>

/* Boost headers. */
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

/* QT headers. */

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>
#include <QtCore/QIODevice>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QQueue>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSemaphore>
#include <QtCore/QSet>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QtDebug>
#include <QtCore/QUrlQuery>

#endif

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
