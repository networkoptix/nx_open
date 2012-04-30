#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

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
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
    #ifdef _USE_DXVA
    #include <libavcodec/dxva2.h>
    #endif
	#include <libswscale/swscale.h>
    #include <libavutil/avstring.h>
};

#if defined __cplusplus
// stl headers

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
#include <QFontmetrics>
#include <QGraphicsItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGraphicsitem>
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
#include <QWebView>
#include <QWheelEvent>
#include <QWidget>
#include <QWindowsStyle>
#include <QtConcurrentmap>
#include <QtCore/qmath.h>
#include <QtDebug>
#include <QtGui/QApplication>
#include <QtGui/QMainWindow>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtGui>
#include <QtOpenGL/QGLWidget>
#include <QtXmlPatterns/QXmlQuery>

#endif
