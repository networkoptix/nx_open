#include <utils/common/config.h>

#ifdef _WIN32
#define __STDC_CONSTANT_MACROS
#define NOMINMAX 

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif // _WIN32

// stl headers
#include <algorithm>
#include <functional>

/* Boost headers. */
#include <boost/foreach.hpp>

// QT headers
#include <QAction>
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
#include <QVBoxLayout>
#include <QWaitCondition>
#include <QWheelEvent>
#include <QWidget>
#include <QtConcurrentMap>
#include <QtDebug>
#include <QtGui>


