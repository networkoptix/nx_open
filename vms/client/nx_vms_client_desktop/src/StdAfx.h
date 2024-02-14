// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/system_network_headers.h>

/* FFMPEG headers. */
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

    /* STL headers. */
    #include <algorithm>
    #include <functional>

/* QT headers. */

    #include <QtCore/QList>
    #include <QtCore/QMap>
    #include <QtCore/QObject>
    #include <QtCore/QString>
    #include <QtCore/QStringList>
    #include <QtQml/QtQml>

    #include <nx/utils/thread/mutex.h>
    #include <nx/utils/thread/wait_condition.h>
    #include <ui/dialogs/common/message_box.h>

#endif

#include <nx/fusion/model_functions.h>
#include <nx/utils/literal.h>
