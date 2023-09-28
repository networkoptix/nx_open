// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gl_shortcuts.h"

#include <QtCore/QString>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#include <nx/utils/log/log.h>

/**
 * \def QN_GL_RENDERER_DEBUG
 *
 * Enable OpenGL error reporting. Note that this will result in a LOT of
 * redundant <tt>glGetError</tt> calls, which may affect performance.
 */
//#define QN_GL_RENDERER_DEBUG

namespace {

#ifdef QN_GL_RENDERER_DEBUG
QString glErrorString(GLenum err)
{
    switch (err)
    {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";

        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";

        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";

        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";

        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";

        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";

        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";

        case GL_TABLE_TOO_LARGE:
            return "GL_TABLE_TOO_LARGE";

        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";

        default:
            return QString("UNKNOWN 0x%1").arg(err, 0, 16);
    }
}
#endif // QN_GL_RENDERER_DEBUG

} // namespace

int glCheckError(const char *context)
{
#ifdef QN_GL_RENDERER_DEBUG
    const auto currentContext = QOpenGLContext::currentContext();
    if (!currentContext)
    {
        NX_WARNING(NX_SCOPE_TAG, "There is no current OpenGL context in %1", context);
        return GL_INVALID_OPERATION;
    }

    const auto error = currentContext->functions()->glGetError();
    if (error != GL_NO_ERROR)
        NX_WARNING(NX_SCOPE_TAG, "OpenGL error in '%1': %2", context, glErrorString(error));

    return error;
#else
    Q_UNUSED(context);
    return 0;
#endif
}
