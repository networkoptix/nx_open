#pragma once
#if defined(ENABLE_PROXY_DECODER)

#include <memory>

#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLTexture>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "proxy_video_decoder_utils.h"

void logGlCall(const char *tag);
void checkGlError(QOpenGLFunctions* funcs, const char* tag);

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

void logGlFbo(const char* tag);

typedef std::shared_ptr<QOpenGLFramebufferObject> FboPtr;

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

void checkGlFramebufferStatus();

void debugTestImageExtensionAndAbort(QOpenGLTexture& yTex);
void debugGlGetAttribsAndAbort(QSize frameSize);
void debugTextureTest();

void debugLogPrecision();

#endif // ENABLE_PROXY_DECODER
