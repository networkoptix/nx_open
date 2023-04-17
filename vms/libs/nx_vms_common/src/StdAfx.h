// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#if !defined(_WIN32)
    #include <arpa/inet.h>
#endif

#ifdef __cplusplus

    /* Useful utils */
    #include <common/common_globals.h>
    #include <nx/fusion/model_functions_fwd.h>
    #include <nx/reflect/enum_instrument.h>
    #include <nx/utils/string.h>
    #include <nx/utils/uuid.h>

    /* STL headers. */
    #include <algorithm>
    #include <functional>

/* QT headers. */

    #include <QtCore/QDateTime>
    #include <QtCore/QDir>
    #include <QtCore/QFile>
    #include <QtCore/QFileInfo>
    #include <QtCore/QFileInfoList>
    #include <QtCore/QIODevice>
    #include <QtCore/QList>
    #include <QtCore/QMap>
    #include <QtCore/QObject>
    #include <QtCore/QPair>
    #include <QtCore/QPoint>
    #include <QtCore/QPointF>
    #include <QtCore/QQueue>
    #include <QtCore/QRect>
    #include <QtCore/QRectF>
    #include <QtCore/QSet>
    #include <QtCore/QSharedPointer>
    #include <QtCore/QSize>
    #include <QtCore/QString>
    #include <QtCore/QStringList>
    #include <QtCore/QThread>
    #include <QtCore/QThreadPool>
    #include <QtCore/QTime>
    #include <QtCore/QTimer>
    #include <QtCore/QUrlQuery>

#endif

#include <nx/utils/literal.h>
