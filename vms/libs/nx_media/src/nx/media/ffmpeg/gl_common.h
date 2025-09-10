// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QOpenGLFunctions>

#if defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
    #include <EGL/egl.h>
#endif

namespace nx::media {

#if defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
    // Get error description for EGL error.
    QString eglErrorString(EGLint error);
#endif

// Get error description for OpenGL error.
QString glErrorString(GLenum error);

// Get descriptions for all recent OpenGL errors. Drains up to 8 errors.
QStringList glAllErrors(QOpenGLFunctions* funcs);

} // namespace nx::media
