#pragma once

#include <QtCore/private/qconfig_p.h>

#if !defined(QT_FEATURE_system_zlib)
    #include <QtZlib/zlib.h>
#else
    #include <zlib.h>
#endif
