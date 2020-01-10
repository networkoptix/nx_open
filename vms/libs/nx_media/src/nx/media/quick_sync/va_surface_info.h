#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>
#include <QtGui/QOpenGLFunctions>

#include <va/va.h>
#include <va/va_glx.h>
#include <va/va_x11.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

constexpr int kHandleTypeVaSurface = QAbstractVideoBuffer::UserHandle + 1;

struct VaSurfaceInfo
{
    VASurfaceID id;
    VADisplay display;
    VAImage image;
};
Q_DECLARE_METATYPE(VaSurfaceInfo);
