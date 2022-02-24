// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/private/qconfig_p.h>

#if !defined(QT_FEATURE_system_zlib) || QT_FEATURE_system_zlib == -1
    #include <QtZlib/zlib.h>
#else
    #include <zlib.h>
#endif
