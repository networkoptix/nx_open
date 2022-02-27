// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/qsystemdetection.h>

#if defined(Q_OS_WIN)
    #include "systemexcept_win.h"
#elif defined(Q_OS_LINUX)
    #include "systemexcept_linux.h"
#endif
