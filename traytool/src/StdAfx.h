#define QT_NO_CAST_FROM_ASCII

#include <common/config.h>
#ifdef __cplusplus
#   include <common/common_globals.h>
#endif

/* Windows headers. */
#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <iphlpapi.h>
#endif

#ifdef __cplusplus

/* STL headers. */
#include <algorithm>
#include <functional>

/* Boost headers. */
#include <boost/foreach.hpp>
#include <boost/array.hpp>

/* QT headers. */
#include <QtWidgets/QAction>

/*
#ifdef Q_OS_WIN

#include <QtMultimedia/QAudio>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>
#endif

#include <QtNetwork/QAuthenticator>
#include <QtCore/QBuffer>
#include <QtWidgets/QCheckBox>
#include <QtGui/QCloseEvent>
#include <QtGui/QColor>
#include <QtWidgets/QCompleter>
#include <QDataStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QEasingCurve>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QFontMetrics>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QHostAddress>
#include <QIODevice>
#include <QtWidgets/QInputDialog>
#include <QKeyEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QList>
#include <QtWidgets/QListView>
#include <QMap>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
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
*/

#endif
