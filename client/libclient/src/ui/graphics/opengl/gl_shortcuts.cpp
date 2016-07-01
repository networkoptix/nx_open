#include "gl_shortcuts.h"

//#ifdef Q_OS_MACX
//#include <glu.h>
//#else
//#include <GL/glu.h>
//#endif

#include <utils/common/warnings.h>

/**
 * \def QN_GL_RENDERER_DEBUG
 * 
 * Enable OpenGL error reporting. Note that this will result in a LOT of
 * redundant <tt>glGetError</tt> calls, which may affect performance.
 */
//#define QN_GL_RENDERER_DEBUG

int glCheckError(const char *context) {
#ifdef QN_GL_RENDERER_DEBUG
    int error = glGetError();
    if (error != GL_NO_ERROR) {
        qnWarning("OpenGL error in '%1': %2", context, error);
        //const char *errorString = reinterpret_cast<const char *>(gluErrorString(error));
        //if (errorString != NULL)
        //    qnWarning("OpenGL error in '%1': %2", context, errorString);
    }
    return error;
#else
    Q_UNUSED(context);
    return 0;
#endif
}

