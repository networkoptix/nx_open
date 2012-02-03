#include "opengl.h"
#include <utils/common/warnings.h>

int glCheckError(const char *context) {
    int error = glGetError();
    if (error != GL_NO_ERROR) {
        const char *errorString = reinterpret_cast<const char *>(gluErrorString(error));
        if (errorString != NULL)
            qnWarning("OpenGL error in '%1': %2", context, errorString);
    }
    return error;
}

