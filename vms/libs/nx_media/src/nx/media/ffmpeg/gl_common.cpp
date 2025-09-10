// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gl_common.h"

namespace nx::media {

#if defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
QString eglErrorString(EGLint error)
{
    switch (error)
    {
        case EGL_SUCCESS: return "No error";
        case EGL_NOT_INITIALIZED: return "EGL not initialized or failed to initialize";
        case EGL_BAD_ACCESS: return "Resource inaccessible";
        case EGL_BAD_ALLOC: return "Cannot allocate resources";
        case EGL_BAD_ATTRIBUTE: return "Unrecognized attribute or attribute value";
        case EGL_BAD_CONTEXT: return "Invalid EGL context";
        case EGL_BAD_CONFIG: return "Invalid EGL frame buffer configuration";
        case EGL_BAD_CURRENT_SURFACE: return "Current surface is no longer valid";
        case EGL_BAD_DISPLAY: return "Invalid EGL display";
        case EGL_BAD_SURFACE: return "Invalid surface";
        case EGL_BAD_MATCH: return "Inconsistent arguments";
        case EGL_BAD_PARAMETER: return "Invalid argument";
        case EGL_BAD_NATIVE_PIXMAP: return "Invalid native pixmap";
        case EGL_BAD_NATIVE_WINDOW: return "Invalid native window";
        case EGL_CONTEXT_LOST: return "Context lost";
    }
    return QString("Unknown error 0x%1").arg((int) error, 0, 16);
}
#endif

QString glErrorString(GLenum error)
{
    switch (error)
    {
        case GL_NO_ERROR:
            return "No error has been recorded.";
        case GL_INVALID_ENUM:
            return "An unacceptable value is specified for an enumerated argument.";
        case GL_INVALID_VALUE:
            return "A numeric argument is out of range.";
        case GL_INVALID_OPERATION:
            return "The specified operation is not allowed in the current state.";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "The framebuffer object is not complete.";
        case GL_OUT_OF_MEMORY:
            return "There is not enough memory left to execute the command.";
#if !defined(Q_OS_IOS)
    #if defined(Q_OS_ANDROID)
        case GL_STACK_UNDERFLOW_KHR:
    #else
        case GL_STACK_UNDERFLOW:
    #endif
            return "An attempt has been made to perform an operation that would cause an internal"
                " stack to underflow.";
    #if defined(Q_OS_ANDROID)
        case GL_STACK_OVERFLOW_KHR:
    #else
        case GL_STACK_OVERFLOW:
    #endif
            return "An attempt has been made to perform an operation that would cause an internal"
                " stack to overflow.";
#endif // !defined(Q_OS_IOS)
        default:
            return QString("Unknown error 0x%1").arg((int) error, 0, 16);
    }
}

QStringList glAllErrors(QOpenGLFunctions* funcs)
{
    QStringList result;
    // OpenGL docs say that glGetError should always be called in a loop to get all error flags.
    for (size_t i = 0; i < 8; ++i)
    {
        const GLenum errCode = funcs->glGetError();
        if (errCode == GL_NO_ERROR)
            break;
        result.append(glErrorString(errCode));
    }
    return result;
}

} // namespace nx::media
