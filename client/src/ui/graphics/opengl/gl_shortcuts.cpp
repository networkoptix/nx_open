#include "gl_shortcuts.h"

//#ifdef Q_OS_MACX
//#include <glu.h>
//#else
//#include <GL/glu.h>
//#endif

#include <utils/common/warnings.h>


int glCheckError(const char *context) {
    int error = glGetError();
    if (error != GL_NO_ERROR) {
        qnWarning("OpenGL error in '%1': %2", context, error);
        //const char *errorString = reinterpret_cast<const char *>(gluErrorString(error));
        //if (errorString != NULL)
        //    qnWarning("OpenGL error in '%1': %2", context, errorString);
    }
    return error;
}

