// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/qsystemdetection.h>

#ifdef Q_OS_ANDROID

    #include <QtCore/QUrl>

void registerIntentListener();
QUrl getInitialIntentData();

#endif
