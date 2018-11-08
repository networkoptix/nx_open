#pragma once

#include <QtCore/qsystemdetection.h>

#ifdef Q_OS_ANDROID

#include <QtCore/QUrl>

void registerIntentListener();
QUrl getInitialIntentData();

#endif
