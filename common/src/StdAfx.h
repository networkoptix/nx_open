#define QT_NO_CAST_FROM_ASCII

#include <common/config.h>
#ifdef __cplusplus
    #include <common/common_globals.h>
#endif

/* Windows headers. */
#ifdef _WIN32
#   include <winsock2.h>
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
#include <QtWidgets/QAction>

#ifdef Q_OS_WIN
#include <QtMultimedia/QAudio>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>
#endif

#include <QAuthenticator>
#include <QtCore/QBuffer>
#include <QCheckBox>
#include <QtGui/QCloseEvent>
#include <QtGui/QColor>
#include <QtWidgets/QCompleter>
#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtCore/QDir>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QtCore/QEasingCurve>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QGraphicsItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtNetwork/QHostAddress>
#ifndef USE_NX_HTTP
#include <QHttp>
#endif
#include <QtCore/QIODevice>
#include <QtWidgets/QInputDialog>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QLineEdit>
#include <QtCore/QList>
#include <QtWidgets/QListView>
#include <QtCore/QMap>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
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
#include <QtWidgets/QPushButton>
#include <QtCore/QQueue>
#include <QtGui/QRadialGradient>
#include <QtWidgets/QRadioButton>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtWidgets/QScrollBar>
#include <QtCore/QSemaphore>
#include <QtCore/QSet>
#include <QtCore/QSize>
#include <QtWidgets/QSlider>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QStringListModel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QStyleOptionGraphicsItem>
#include <QtWidgets/QTabWidget>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtNetwork/QUdpSocket>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QWaitCondition>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QWidget>
#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/qmath.h>
#include <QtCore/QtDebug>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QtWidgets>
#include <QtOpenGL/QGLWidget>
#include <QFileSystemWatcher>

#endif
