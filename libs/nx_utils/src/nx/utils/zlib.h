#pragma once

#include <QtCore/private/qconfig_p.h>

#if !defined(QT_FEATURE_system_zlib) || QT_FEATURE_system_zlib == -1
    #include <QtZlib/zlib.h>
#else
    #include <zlib.h>
#endif
