#pragma once
#if defined(ENABLE_PROXY_DECODER)

// Should be included after proxy_video_decoder_utils.h (for OUTPUT_PREFIX).

#include <memory>

#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLTexture>
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace nx {
namespace media {

void checkGlError(QOpenGLFunctions* funcs, const char* tag);

#define GL_GET_FUNCS(Q_OPENGL_CONTEXT_PTR) \
    QOpenGLContext* qOpenGlContextPtr = (Q_OPENGL_CONTEXT_PTR); \
    NX_CRITICAL(qOpenGlContextPtr); \
    QOpenGLFunctions* funcs = qOpenGlContextPtr->functions(); \
    NX_CRITICAL(funcs);

#define GL(CALL) do \
{ \
    if (conf.outputGlCalls) \
        PRINT << "OpenGL CALL: " << #CALL; \
    checkGlError(funcs, "{prior to:}" #CALL); \
    CALL; \
    checkGlError(funcs, #CALL); \
} while (0)

#define GL_CHECK(BOOL_CALL) do \
{ \
    if (conf.outputGlCalls) \
        PRINT << "OpenGL CALL: " << #BOOL_CALL; \
    checkGlError(funcs, "{prior to:}" #BOOL_CALL); \
    bool result = BOOL_CALL; \
    if (!result) \
        PRINT << "OpenGL FAILED CALL: " << #BOOL_CALL << " -> false"; \
    checkGlError(funcs, #BOOL_CALL); \
    NX_CRITICAL(result); \
} while (0)

#define GL_DUMP(OBJ) do \
{ \
    OUTPUT << "OpenGL " #OBJ " BEGIN"; \
    OUTPUT << OBJ->log(); \
    OUTPUT << "OpenGL " #OBJ " END"; \
} while (0)

void outputGlFbo(const char* tag);

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
        OUTPUT << "FboManager::FboManager(frameSize:" << frameSize
            << ", queueSize:" << queueSize << ")";
    }

    FboPtr getFbo()
    {
        while ((int) m_fboQueue.size() < m_queueSize)
        {
            outputGlFbo("getFbo 1");
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
            outputGlFbo("getFbo 2");
            m_fboQueue.push_back(FboPtr(fbo));
            glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
            outputGlFbo("getFbo 3");
        }

        return m_fboQueue[m_index++ % m_fboQueue.size()];
    }

private:
    std::vector<FboPtr> m_fboQueue;
    QSize m_frameSize;
    int m_index;
    const int m_queueSize;
};

void checkGlFramebufferStatus();

void debugTestImageExtensionAndAbort(QOpenGLTexture& yTex);
void debugGlGetAttribsAndAbort(QSize frameSize);
void debugTextureTest();
void debugDumpGlPrecision();

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
