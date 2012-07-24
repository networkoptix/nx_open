#define QT_NO_CAST_FROM_ASCII

#ifdef _WIN32
#define NOMINMAX 
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

// DXVA headers (should be included before ffmpeg headers)
#ifdef _USE_DXVA
#include <d3d9.h>
#include <dxva2api.h>
#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <Strsafe.h>
#endif
#endif

// ffmpeg headers
#if defined __cplusplus
extern "C" {
#endif
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#ifdef _USE_DXVA
	#include <libavcodec/dxva2.h>
	#endif
	#include <libswscale/swscale.h>
	#include <libavutil/avstring.h>
#if defined __cplusplus
}
#endif


#if defined __cplusplus

#include "utils/media/sse_helper.h"

// stl headers
#include <algorithm>
#include <functional>

// QT headers
#include <QAction>

#ifdef Q_OS_WIN
#include <QAudio>
#include <QAudioFormat>
#include <QAudioOutput>
#endif

#include <QAuthenticator>
#include <QBuffer>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QCompleter>
#include <QDataStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QEasingCurve>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QHttp>
#include <QIODevice>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QObject>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPair>
#include <QParallelAnimationGroup>
#include <QPixmap>
#include <QPixmapCache>
#include <QPoint>
#include <QPointF>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QQueue>
#include <QRadialGradient>
#include <QRadioButton>
#include <QRect>
#include <QRectF>
#include <QScrollBar>
#include <QSemaphore>
#include <QSet>
#include <QSize>
#include <QSlider>
#include <QString>
#include <QStringList>
#include <QStringListModel>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionGraphicsItem>
#include <QTabWidget>
#include <QTextStream>
#include <QThread>
#include <QThreadPool>
#include <QTime>
#include <QTimer>
#include <QUdpSocket>
#include <QVBoxLayout>
#include <QWaitCondition>
#include <QWheelEvent>
#include <QWidget>
#include <QtConcurrentMap>
#include <QtCore/qmath.h>
#include <QtDebug>
#include <QtGui/QApplication>
#include <QtGui/QMainWindow>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtGui>
#include <QtOpenGL/QGLWidget>
#include <QFileSystemWatcher>

#endif
