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
/* This file should be included only main in vms projects. */
#include <common/common_globals.h>
#endif

#ifdef __cplusplus

/* STL headers. */
#include <algorithm>
#include <functional>

/* Boost headers. */
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

/* QT headers. */
#include <QtWidgets/QAction>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QBuffer>
#include <QtWidgets/QCheckBox>
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
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtNetwork/QHostAddress>
#include <QtCore/QIODevice>
#include <QtWidgets/QInputDialog>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QLineEdit>
#include <QtCore/QList>
#include <QtWidgets/QListView>
#include <QtCore/QMap>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>
#include <nx/utils/thread/mutex.h>
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
#include <QtGui/QPixmapCache>
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
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <QtWidgets/QTabWidget>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtWidgets/QVBoxLayout>
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

#include <nx/utils/thread/wait_condition.h>
#include <ui/dialogs/common/message_box.h>

#endif

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
