#include "gl_shortcuts.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#include <utils/common/warnings.h>

/**
 * \def QN_GL_RENDERER_DEBUG
 *
 * Enable OpenGL error reporting. Note that this will result in a LOT of
 * redundant <tt>glGetError</tt> calls, which may affect performance.
 */
//#define QN_GL_RENDERER_DEBUG

int glCheckError(const char *context)
{
#ifdef QN_GL_RENDERER_DEBUG
    const auto currentContext = QOpenGLContext::currentContext();
    if (!currentContext)
    {
        qnWarning("There is no current OpenGL context in %1", context);
        return GL_INVALID_OPERATION;
    }

    const auto error = currentContext->functions()->glGetError();
    if (error != GL_NO_ERROR)
        qnWarning("OpenGL error in '%1': %2", context, error);

    return error;
#else
    Q_UNUSED(context);
    return 0;
#endif
}

