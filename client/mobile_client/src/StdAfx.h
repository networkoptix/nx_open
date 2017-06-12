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
#include <common/common_globals.h>

/* STL headers. */
#include <algorithm>
#include <functional>

/* QT headers. */
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QStringListModel>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QQueue>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QDateTime>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QDataStream>
#include <QtCore/QTextStream>
#include <QtCore/QIODevice>
#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QEasingCurve>
#include <QtCore/qmath.h>
#include <QtCore/QtDebug>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QColor>
#include <QtGui/QDesktopServices>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPaintEngine>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <QtGui/QPixmapCache>
#include <QtGui/QRadialGradient>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkAddressEntry>
#include <QtNetwork/QNetworkInterface>
#include <QtNetwork/QUdpSocket>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QtConcurrent/QtConcurrentMap>

#endif

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
