#pragma once

//-------------------------------------------------------------------------------------------------
// Public

// Uses configuration:
// #define ENABLE_GL_LOG
// #define ENABLE_GL_FATAL_ERRORS

#include <memory>

#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLTexture>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "proxy_video_decoder_utils.h"

// #define GL_GET_FUNCS(Q_OPENGL_CONTEXT_PTR) ...
// #define GL_CHECK(BOOL_CALL) ... //< For CALL which returns bool - asserted to be true.
// #define GL(CALL) ... //< For arbitrary CALL.
// #define LOG_GL(OBJ) ...
void logGlFbo(const char* tag);

typedef std::shared_ptr<QOpenGLFramebufferObject> FboPtr;

class FboManager;

void checkGlFramebufferStatus();

void debugGlGetAttribsAndAbort(QSize frameSize);
void debugTextureTest();

void debugLogPrecision();

//-------------------------------------------------------------------------------------------------

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

static void checkGlError(QOpenGLFunctions* funcs, const char* tag)
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
            qWarning() << "OpenGL ERROR:" << tag << "->";
        }

        const char* const s = glErrorStr(err);
        if (s)
            qWarning() << "    " << s;
        else
            qWarning() << "    " << std::hex << err;

        #ifdef ENABLE_GL_FATAL_ERRORS
            NX_CRITICAL(false);
        #endif // ENABLE_GL_FATAL_ERRORS
        NX_CRITICAL(err != GL_OUT_OF_MEMORY); //< Abort because GL state is undefined.
    }
}

static void logGlCall(const char *tag)
{
    #ifdef ENABLE_GL_LOG
        qWarning() << "OpenGL:" << tag;
    #else // ENABLE_GL_LOG
        QN_UNUSED(tag);
    #endif // ENABLE_GL_LOG
}

#define GL_GET_FUNCS(Q_OPENGL_CONTEXT_PTR) \
    QOpenGLContext* qOpenGlContextPtr = (Q_OPENGL_CONTEXT_PTR); \
    NX_CRITICAL(qOpenGlContextPtr); \
    QOpenGLFunctions* funcs = qOpenGlContextPtr->functions(); \
    NX_CRITICAL(funcs);

#define GL(CALL) do \
{ \
    logGlCall(#CALL); \
    checkGlError(funcs, "{prior to:}" #CALL); \
    CALL; \
    checkGlError(funcs, #CALL); \
} while (0)

#define GL_CHECK(BOOL_CALL) do \
{ \
    logGlCall(#BOOL_CALL); \
    checkGlError(funcs, "{prior to:}" #BOOL_CALL); \
    bool result = BOOL_CALL; \
    if (!result) \
        qWarning() << "OpenGL FAILED CALL:" << #BOOL_CALL << "-> false"; \
    checkGlError(funcs, #BOOL_CALL); \
    NX_CRITICAL(result); \
} while (0)

#define LOG_GL(OBJ) do \
{ \
    QLOG("OpenGL " #OBJ " BEGIN"); \
    QLOG(OBJ->log()); \
    QLOG("OpenGL " #OBJ " END"); \
} while (0)

void logGlFbo(const char* tag)
{
    QN_UNUSED(tag);

    if (!QOpenGLContext::currentContext())
    {
        QLOG(tag << ": Unable to log FBO: no current context");
    }
    else
    {
        GL_GET_FUNCS(QOpenGLContext::currentContext());
        GLint curFbo = 0;
        GL(funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &curFbo));
        QLOG(tag << ": current FBO ==" << curFbo);
    }
}

//-------------------------------------------------------------------------------------------------

class FboManager
{
public:
    FboManager(const QSize& frameSize, int queueSize)
    :
        m_frameSize(frameSize),
        m_index(0),
        m_queueSize(queueSize)
    {
        QLOG("FboManager::FboManager(frameSize:" << frameSize
            << ", queueSize:" << queueSize << ")");
    }

    FboPtr getFbo()
    {
        while ((int) m_fboQueue.size() < m_queueSize)
        {
            logGlFbo("getFbo 1");
            GL_GET_FUNCS(QOpenGLContext::currentContext());
            GLint prevFbo = 0;
            GL(funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo));
            QOpenGLFramebufferObject* fbo;

            QOpenGLFramebufferObjectFormat fmt;
            fmt.setMipmap(false);
            fmt.setSamples(1);
            fmt.setTextureTarget(GL_TEXTURE_2D);
            fmt.setInternalTextureFormat(GL_RGBA);

            GL(fbo = new QOpenGLFramebufferObject(m_frameSize, fmt));
            NX_CRITICAL(fbo->isValid());
            logGlFbo("getFbo 2");
            m_fboQueue.push_back(FboPtr(fbo));
            glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
            logGlFbo("getFbo 3");
        }

        return m_fboQueue[m_index++ % m_fboQueue.size()];

        //if (!m_fbo)
        //    m_fbo = FboPtr(new QOpenGLFramebufferObject(m_frameSize));
        //return m_fbo;
    }

private:
    //FboPtr m_fbo;
    std::vector<FboPtr> m_fboQueue;
    QSize m_frameSize;
    int m_index;
    const int m_queueSize;
};

//-------------------------------------------------------------------------------------------------

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

void checkGlFramebufferStatus()
{
    GL_GET_FUNCS(QOpenGLContext::currentContext());
    GLenum res = 0;
    GL(res = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if (res != GL_FRAMEBUFFER_COMPLETE)
    {
        const char* const s = glFramebufferStatusStr(res);
        if (s)
            qWarning() << "OpenGL FAILED CHECK: glCheckFramebufferStatus() ->" << s;
        else
            qWarning() << "OpenGL FAILED CHECK: glCheckFramebufferStatus() ->" << res;
        NX_CRITICAL(false);
    }
}

//-------------------------------------------------------------------------------------------------

void debugTextureTest()
{
    QLOG("####### debugTextureTest() BEGIN");
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
        bufferStart = debugUnalignPtr(buffer);

        buffer2 = malloc(17 + kMediaAlignment + 1920 * 1080);
        buffer2Start = debugUnalignPtr(buffer2);
    }

    funcs->glFlush();
    funcs->glFinish();
    TIME_BEGIN("debugTextureTest::setData && glFlush && glFinish")
        GL(tex.setData(QOpenGLTexture::Alpha, QOpenGLTexture::UInt8, bufferStart));
    funcs->glFlush();
    funcs->glFinish();
    TIME_END

    TIME_BEGIN("debugTextureTest::memcpy")
    memcpy(buffer2Start, bufferStart, 1920 * 1080);
    TIME_END

    QLOG("####### debugTextureTest() END");
}

void debugGlGetAttribsAndAbort(QSize frameSize)
{
    QLOG("error=" << eglGetError());

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    QLOG("eglGetDisplay ->" << display);
    QLOG("error=" << eglGetError());

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
    QLOG("eglChooseConfig num configs ->" << numConfigs);
    QLOG("error=" << eglGetError());

    for (int i = 0; i < numConfigs; ++i)
    {
        QLOG("config " << i);
        EGLint isLockSupp(0);
        eglGetConfigAttrib(display, configs[i], EGL_LOCK_SURFACE_BIT_KHR, &isLockSupp);
        QLOG("isLockSupp=" << isLockSupp);
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
    QLOG("eglCreatePbufferSurface ->" << pBuffer);
    QLOG("error=" << eglGetError());

    EGLint width(0), height(0);
    eglQuerySurface(display, pBuffer, EGL_WIDTH, &width);
    eglQuerySurface(display, pBuffer, EGL_HEIGHT, &height);
    QLOG("width=" << width << ", height=" << height);

    // Abort.
    NX_CRITICAL(false);
}

void debugTestImageExtension(QOpenGLTexture& yTex)
{
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    QLOG("eglCreateImageKHR =" << eglCreateImageKHR);

    EGLImageKHR eglImageHandle = eglCreateImageKHR(
        eglGetCurrentDisplay(),
        eglGetCurrentContext(),
        EGL_GL_TEXTURE_2D_KHR,
        (EGLClientBuffer) yTex.textureId(),
        nullptr);

    QLOG("eglImageHandle ==" << eglImageHandle);
    QLOG("eglGetError() returned" << eglGetError());

    GLvoid* bitmapAddress(0);
    eglQuerySurface(eglGetCurrentDisplay(), eglImageHandle, EGL_BITMAP_POINTER_KHR, (GLint*) &bitmapAddress);

    QLOG("bitmapAddress ==" << bitmapAddress);
    QLOG("eglGetError() returned" << eglGetError());

    NX_CRITICAL(false);
}

void debugLogPrecision()
{
    int range[2], precision;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);

    fprintf(stderr, "Fragment highp float range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, range, &precision);
    fprintf(stderr, "Fragment mediump float range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, range, &precision);
    fprintf(stderr, "Fragment lowp float range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);

    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_INT, range, &precision);
    fprintf(stderr, "Fragment highp int range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_INT, range, &precision);
    fprintf(stderr, "Fragment mediump int range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_INT, range, &precision);
    fprintf(stderr, "Fragment lowp int range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);

    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_FLOAT, range, &precision);
    fprintf(stderr, "Vertex highp float range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_MEDIUM_FLOAT, range, &precision);
    fprintf(stderr, "Vertex mediump float range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_FLOAT, range, &precision);
    fprintf(stderr, "Vertex lowp float range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);

    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_INT, range, &precision);
    fprintf(stderr, "Vertex highp int range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_MEDIUM_INT, range, &precision);
    fprintf(stderr, "Vertex mediump int range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_INT, range, &precision);
    fprintf(stderr, "Vertex lowp int range 2^%d..2^%d, precision 2^%d\n", range[0], range[1], precision);
}
