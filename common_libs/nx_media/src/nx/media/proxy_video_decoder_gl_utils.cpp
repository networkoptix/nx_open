#if defined(ENABLE_PROXY_DECODER)

#define OUTPUT_PREFIX "ProxyVideoDecoder[gl_utils]: "
#include "proxy_video_decoder_utils.h"

#include "proxy_video_decoder_gl_utils.h" //< Should be included after proxy_video_decoder_utils.h

namespace nx {
namespace media {

namespace {

static const char* glFramebufferStatusStr(GLenum status)
{
    switch (status)
    {
        case GL_FRAMEBUFFER_COMPLETE: return "GL_FRAMEBUFFER_COMPLETE";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";
        default: return nullptr;
    }
}

static const char* glErrorStr(GLenum err)
{
    switch (err)
    {
        case GL_NO_ERROR: return "GL_NO_ERROR == 0";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM == 0x0500";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE == 0x0501";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION == 0x0502";
        //case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW == 0x0503";
        //case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW == 0x0504";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION == 0x0506";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY == 0x0505";
        default: return nullptr;
    }
}

static void dumpPrecision(const char*tag, GLenum shaderType, GLenum precisionType)
{
    int range[2];
    int precision;

    glGetShaderPrecisionFormat(shaderType, precisionType, range, &precision);

    qWarning().nospace() << tag << " range 2^" << range[0] << "..2^" << range[1]
        << ", precision 2^" << precision;
}

} // namespace

void checkGlError(QOpenGLFunctions* funcs, const char* tag)
{
    bool anyErrors = false;
    for (;;)
    {
        GLenum err = funcs->glGetError();
        if (err == GL_NO_ERROR)
            break;

        if (!anyErrors)
        {
            anyErrors = true;
            PRINT << "OpenGL ERROR:" << tag << "->";
        }

        const char* const s = glErrorStr(err);
        if (s)
            qWarning().nospace() << "    " << s;
        else
            qWarning().nospace() << "    " << std::hex << err;

        if (conf.stopOnGlErrors)
            NX_CRITICAL(false);
        NX_CRITICAL(err != GL_OUT_OF_MEMORY); //< Abort because GL state is undefined.
    }
}

void outputGlFbo(const char* tag)
{
    if (!QOpenGLContext::currentContext())
    {
        OUTPUT << tag << ": Unable to log FBO: no current context";
    }
    else
    {
        GL_GET_FUNCS(QOpenGLContext::currentContext());
        GLint curFbo = 0;
        GL(funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &curFbo));
        OUTPUT << tag << ": current FBO ==" << curFbo;
    }
}

void checkGlFramebufferStatus()
{
    GL_GET_FUNCS(QOpenGLContext::currentContext());
    GLenum res = 0;
    GL(res = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if (res != GL_FRAMEBUFFER_COMPLETE)
    {
        const char* const s = glFramebufferStatusStr(res);
        if (s)
            PRINT << "OpenGL FAILED CHECK: glCheckFramebufferStatus() -> " << s;
        else
            PRINT << "OpenGL FAILED CHECK: glCheckFramebufferStatus() -> " << res;
        NX_CRITICAL(false);
    }
}

void debugTextureTest()
{
    PRINT << "debugTextureTest() BEGIN";
    static QOpenGLTexture tex(QOpenGLTexture::Target2D);
    static QOpenGLContext *ctx;
    static QOpenGLFunctions *funcs;
    static void* buffer;
    static uint8_t* bufferStart;
    static void* buffer2;
    static void* buffer2Start;
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;

        ctx = QOpenGLContext::currentContext();
        NX_CRITICAL(ctx);
        funcs = ctx->functions();
        NX_CRITICAL(funcs);

        GL(tex.setFormat(QOpenGLTexture::AlphaFormat));
        GL(tex.setSize(1920, 1080));
        GL(tex.allocateStorage(QOpenGLTexture::Alpha, QOpenGLTexture::UInt8));
        GL(tex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));

        buffer = malloc(17 + kMediaAlignment + 1920 * 1080);
        bufferStart = nx::utils::debugUnalignPtr(buffer);

        buffer2 = malloc(17 + kMediaAlignment + 1920 * 1080);
        buffer2Start = nx::utils::debugUnalignPtr(buffer2);
    }

    funcs->glFlush();
    funcs->glFinish();
    NX_TIME_BEGIN(debugTextureTest_setData_glFlush_glFinish);
        GL(tex.setData(QOpenGLTexture::Alpha, QOpenGLTexture::UInt8, bufferStart));
    funcs->glFlush();
    funcs->glFinish();
    NX_TIME_END(debugTextureTest_setData_glFlush_glFinish);

    NX_TIME_BEGIN(debugTextureTest_memcpy);
    memcpy(buffer2Start, bufferStart, 1920 * 1080);
    NX_TIME_END(debugTextureTest_memcpy);

    PRINT << "debugTextureTest() END";
}

void debugGlGetAttribsAndAbort(QSize frameSize)
{
    PRINT << "error=" << eglGetError();

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    PRINT << "eglGetDisplay -> " << display;
    PRINT << "error=" << eglGetError();

    EGLint configAttribs[] =
    {
        //EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        //EGL_RED_SIZE, 8,
        //EGL_GREEN_SIZE, 8,
        //EGL_BLUE_SIZE, 8,
        //EGL_DEPTH_SIZE, 0,
        //EGL_STENCIL_SIZE, 0,
        //EGL_SAMPLE_BUFFERS, 0,
        //EGL_SAMPLES, 0,
        //EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
        //EGL_LOCK_SURFACE_BIT_KHR, EGL_TRUE,
        EGL_NONE
    };

    EGLConfig configs[100];
    EGLint numConfigs(0);
    eglChooseConfig(display, configAttribs, configs, 100, &numConfigs);
    PRINT << "eglChooseConfig num configs -> " << numConfigs;
    PRINT << "error=" << eglGetError();

    for (int i = 0; i < numConfigs; ++i)
    {
        PRINT << "config " << i;
        EGLint isLockSupp(0);
        eglGetConfigAttrib(display, configs[i], EGL_LOCK_SURFACE_BIT_KHR, &isLockSupp);
        PRINT << "isLockSupp=" << isLockSupp;
    }

    EGLint attribs[] = {
        EGL_WIDTH, frameSize.width(),
        EGL_HEIGHT, frameSize.height(),
        EGL_MIPMAP_TEXTURE, EGL_FALSE,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGB,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };

    EGLSurface pBuffer = eglCreatePbufferSurface(display, configs[0], attribs);
    PRINT << "eglCreatePbufferSurface -> " << pBuffer;
    PRINT << "error=" << eglGetError();

    EGLint width(0), height(0);
    eglQuerySurface(display, pBuffer, EGL_WIDTH, &width);
    eglQuerySurface(display, pBuffer, EGL_HEIGHT, &height);
    PRINT << "width=" << width << ", height=" << height;

    // Abort.
    NX_CRITICAL(false);
}

void debugTestImageExtension(QOpenGLTexture& yTex)
{
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    PRINT << "eglCreateImageKHR =" << eglCreateImageKHR;

    EGLImageKHR eglImageHandle = eglCreateImageKHR(
        eglGetCurrentDisplay(),
        eglGetCurrentContext(),
        EGL_GL_TEXTURE_2D_KHR,
        (EGLClientBuffer) yTex.textureId(),
        nullptr);

    PRINT << "eglImageHandle ==" << eglImageHandle;
    PRINT << "eglGetError() returned" << eglGetError();

    GLvoid* bitmapAddress(0);
    eglQuerySurface(eglGetCurrentDisplay(), eglImageHandle, EGL_BITMAP_POINTER_KHR, (GLint*) &bitmapAddress);

    PRINT << "bitmapAddress ==" << bitmapAddress;
    PRINT << "eglGetError() returned" << eglGetError();

    // Abort.
    NX_CRITICAL(false);
}

void debugDumpGlPrecisions()
{
    dumpPrecision("Fragment highp float", GL_FRAGMENT_SHADER, GL_HIGH_FLOAT);
    dumpPrecision("Fragment mediump float", GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT);
    dumpPrecision("Fragment lowp float", GL_FRAGMENT_SHADER, GL_LOW_FLOAT);
    dumpPrecision("Fragment highp int", GL_FRAGMENT_SHADER, GL_HIGH_INT);
    dumpPrecision("Fragment mediump int", GL_FRAGMENT_SHADER, GL_MEDIUM_INT);
    dumpPrecision("Fragment lowp int", GL_FRAGMENT_SHADER, GL_LOW_INT);
    dumpPrecision("Vertex highp float", GL_VERTEX_SHADER, GL_HIGH_FLOAT);
    dumpPrecision("Vertex mediump float", GL_VERTEX_SHADER, GL_MEDIUM_FLOAT);
    dumpPrecision("Vertex lowp float", GL_VERTEX_SHADER, GL_LOW_FLOAT);
    dumpPrecision("Vertex highp int", GL_VERTEX_SHADER, GL_HIGH_INT);
    dumpPrecision("Vertex mediump int", GL_VERTEX_SHADER, GL_MEDIUM_INT);
    dumpPrecision("Vertex lowp int", GL_VERTEX_SHADER, GL_LOW_INT);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
