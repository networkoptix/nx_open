#include "proxy_video_decoder_impl.h"
#if defined(ENABLE_PROXY_DECODER)

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLShaderProgram>

#include <nx/kit/debug.h>

#include "proxy_video_decoder_utils.h"
#include "proxy_video_decoder_gl_utils.h"

namespace nx {
namespace media {

namespace {

/**
 * Experimental implementation. On banana pi, works slowly, probably due to memory bandwidth.
 */
class Impl: public ProxyVideoDecoderImpl
{
public:
    Impl(const Params& params):
        ProxyVideoDecoderImpl(params),
        m_program(nullptr),
        m_yTex(QOpenGLTexture::Target2D),
        m_uTex(QOpenGLTexture::Target2D),
        m_vTex(QOpenGLTexture::Target2D)
    {
    }

    ~Impl()
    {
        NX_OUTPUT << "~Impl() BEGIN";
        if (m_threadGlCtx)
        {
            NX_GL_GET_FUNCS(m_threadGlCtx.get());
            if (m_program)
            {
                NX_GL(m_yTex.release());
                NX_GL(m_yTex.destroy());
                NX_GL(m_uTex.release());
                NX_GL(m_uTex.destroy());
                NX_GL(m_vTex.release());
                NX_GL(m_vTex.destroy());
                NX_GL(m_program->release());
                m_program.reset(nullptr);
                m_fboManager.reset(nullptr);
            }
            m_threadGlCtx->doneCurrent();
            m_threadGlCtx.reset(nullptr);
        }
        m_offscreenSurface.reset(nullptr);
        NX_OUTPUT << "~Impl() END";
    }

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;

private:
    class TextureBuffer;

    /** Initialize OpenGL resources on first call; subsequent calls do nothing. */
    void createGlResources();

    void renderYuvBufferToFbo(const YuvBuffer* yuvBuffer, FboPtr* outFbo);

private:
    std::unique_ptr<QOffscreenSurface> m_offscreenSurface;

    std::unique_ptr<QOpenGLContext> m_threadGlCtx;
    std::unique_ptr<QOpenGLShaderProgram> m_program;

    std::unique_ptr<FboManager> m_fboManager;

    QOpenGLTexture m_yTex;
    QOpenGLTexture m_uTex;
    QOpenGLTexture m_vTex;
};

class Impl::TextureBuffer: public QAbstractVideoBuffer
{
public:
    TextureBuffer(
        const FboPtr& fbo,
        const std::shared_ptr<ProxyVideoDecoderImpl>& owner,
        const ConstYuvBufferPtr& yuvBuffer)
        :
        QAbstractVideoBuffer(GLTextureHandle),
        m_fbo(fbo),
        m_owner(std::dynamic_pointer_cast<Impl>(owner)),
        m_yuvBuffer(yuvBuffer)
    {
        if (!fbo)
            NX_CRITICAL(owner);
    }

    ~TextureBuffer()
    {
    }

    virtual MapMode mapMode() const override
    {
        return NotMapped;
    }

    virtual uchar* map(MapMode, int*, int*) override
    {
        return 0;
    }

    virtual void unmap() override
    {
    }

    virtual QVariant handle() const override
    {
        NX_GL_GET_FUNCS(QOpenGLContext::currentContext());
        checkGlError(funcs, "in handle()");

        if (!m_fbo)
        {
            if (auto owner = m_owner.lock())
            {
                owner->renderYuvBufferToFbo(m_yuvBuffer.get(), &m_fbo);
            }
        }
        return m_fbo ? m_fbo->texture() : 0;
    }

private:
    mutable FboPtr m_fbo;
    std::weak_ptr<Impl> m_owner;
    ConstYuvBufferPtr m_yuvBuffer;
};

void Impl::createGlResources()
{
    if (!m_threadGlCtx)
    {
        if (ini().useSharedGlContext && !ini().useGlGuiRendering)
        {
            // Create shared GL context.
            QOpenGLContext* sharedContext = QOpenGLContext::globalShareContext();
            NX_CRITICAL(sharedContext);

            m_threadGlCtx.reset(new QOpenGLContext());
            NX_GL_GET_FUNCS(m_threadGlCtx.get());
            NX_GL(m_threadGlCtx->setShareContext(sharedContext));
            NX_GL(m_threadGlCtx->setFormat(sharedContext->format()));

            NX_GL_CHECK(m_threadGlCtx->create());
            NX_GL_CHECK(m_threadGlCtx->shareContext());
            NX_PRINT << "Using shared openGL ctx";
            m_offscreenSurface.reset(new QOffscreenSurface());
            NX_GL(m_offscreenSurface->setFormat(m_threadGlCtx->format()));
            NX_GL(m_offscreenSurface->create());
            NX_GL_CHECK(m_threadGlCtx->makeCurrent(m_offscreenSurface.get()));
        }
    }

    if (!m_fboManager)
        m_fboManager.reset(new FboManager(frameSize(), /*queueSize*/ 1));

    if (!m_program)
    {
        NX_GL_GET_FUNCS(QOpenGLContext::currentContext())
#if 1 // all program
        m_program.reset(new QOpenGLShaderProgram());

        auto vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, m_program.get());
        NX_GL_CHECK(vertexShader->compileSourceCode(
            "attribute mediump vec4 vertexCoordsArray; \n"
            "varying   mediump vec2 textureCoords; \n"
            "void main(void) \n"
            "{ \n"
            "    gl_Position = vertexCoordsArray; \n"
            "    textureCoords = ((vertexCoordsArray + vec4(1.0, 1.0, 0.0, 0.0)) * 0.5).xy; \n"
            "}\n"));
        NX_GL_DUMP(vertexShader);
        NX_GL(m_program->addShader(vertexShader));

        auto fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, m_program.get());
        NX_GL_CHECK(fragmentShader->compileSourceCode(
            "varying mediump vec2 textureCoords; \n"
#if 1
            "uniform sampler2D yTexture; \n"
            "uniform sampler2D uTexture; \n"
            "uniform sampler2D vTexture; \n"
#endif // 1
            "void main() \n"
            "{ \n"
#if 1
            "mediump mat4 colorTransform = mat4( \n"
            "    1.0,    0.0,  1.402, -0.701, \n"
            "    1.0, -0.344, -0.714,  0.529, \n"
            "    1.0,  1.772,    0.0, -0.886, \n"
            "    0.0,    0.0,    0.0,    1.0); \n"
#endif // 1
#if 0
            "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); \n"
#endif // 0
#if 0
            //"    gl_FragColor = vec4( \n"
            //"         texture2D(yTexture, textureCoords).a, \n"
            //"         0.0, 0.0, 1.0); \n"
#endif // 0
#if 0
            "    precision highp float; \n"
            "    float r, g, b, y, u, v; \n"
            "    y = texture2D(yTexture, textureCoords).r; \n"
            "    u = texture2D(uTexture, textureCoords).r; \n"
            "    v = texture2D(vTexture, textureCoords).r; \n"
            "    y = 1.1643 * (y - 0.0625); \n"
            "    u = u - 0.5; \n"
            "    v = v - 0.5; \n"
            "    r = y + 1.5958 * v; \n"
            "    g = y - 0.39173 * u - 0.81290 * v; \n"
            "    b = y + 2.017 * u; \n"
            "    gl_FragColor = vec4(r, g, b, 1.0); \n"
#endif // 0
#if 1
            "    gl_FragColor = vec4( \n"
            "         texture2D(yTexture, textureCoords).r, \n"
            "         texture2D(uTexture, textureCoords).r, \n"
            "         texture2D(vTexture, textureCoords).r, \n"
            "         1.0) * colorTransform; \n"
#endif // 1
            "} \n"));
        NX_GL_DUMP(fragmentShader);
        NX_GL(m_program->addShader(fragmentShader));

        NX_GL(m_program->bindAttributeLocation("vertexCoordsArray", 0));
        NX_GL(m_program->link());
        NX_GL_DUMP(m_program);

#endif // 1 // all program

        NX_GL(m_yTex.setFormat(QOpenGLTexture::LuminanceFormat));
        NX_GL(m_yTex.setSize(frameSize().width(), frameSize().height()));
        NX_GL(m_yTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        NX_GL(m_yTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));

        NX_GL(m_uTex.setFormat(QOpenGLTexture::LuminanceFormat));
        NX_GL(m_uTex.setSize(frameSize().width() / 2, frameSize().height() / 2));
        NX_GL(m_uTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        NX_GL(m_uTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));

        NX_GL(m_vTex.setFormat(QOpenGLTexture::LuminanceFormat));
        NX_GL(m_vTex.setSize(frameSize().width() / 2, frameSize().height() / 2));
        NX_GL(m_vTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        NX_GL(m_vTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));
    }
}

void Impl::renderYuvBufferToFbo(const YuvBuffer* yuvBuffer, FboPtr* outFbo)
{
    createGlResources();

    NX_TIME_BEGIN(RenderYuvBufferToFbo);

    NX_GL_GET_FUNCS(QOpenGLContext::currentContext());

    NX_CRITICAL(outFbo);
    *outFbo = m_fboManager->getFbo();
    NX_CRITICAL(*outFbo);

    // OLD: Measured time (Full-HDs frame): YUV: 155 ms; Y-only: 120 ms; Y memcpy: 2 ms (!!!).
#if 0
    NX_TIME_BEGIN(FlushFinish);
    NX_GL(funcs->glFlush());
    NX_GL(funcs->glFinish());
    NX_TIME_END(FlushFinish);
#endif // 0

    NX_TIME_MARK(RenderYuvBufferToFbo, "t1");

    NX_TIME_BEGIN(RenderYuvBufferToFbo_setData);
#if 0 // NO_QT
    NX_GL(funcs->glBindTexture(GL_TEXTURE_2D, (*outFbo)->texture()));
    NX_GL(funcs->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
        yuvBuffer->frameSize().width(), yuvBuffer->frameSize().height(), 0,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvBuffer->y()));
#else // 0 // NO_QT
#if 1
    NX_GL(m_yTex.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvBuffer->y()));
    NX_GL(m_uTex.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvBuffer->u()));
    NX_GL(m_vTex.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvBuffer->v()));
    //TIME_BEGIN(renderYuvBufferToFbo_setData_Flush_Finish);
    //NX_GL(funcs->glFlush());
    //NX_GL(funcs->glFinish());
    //TIME_END(renderYuvBufferToFbo_setData_Flush_Finish);
#endif // 1
#endif // 0 NO_QT
    NX_TIME_END(RenderYuvBufferToFbo_setData);

    NX_TIME_MARK(RenderYuvBufferToFbo, "t2");

#if 1 // ALL

    // save current render states
#if 1
    GLboolean stencilTestEnabled;
    GLboolean depthTestEnabled;
    GLboolean scissorTestEnabled;
    GLboolean blendEnabled;
    NX_GL(glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled));
    NX_GL(glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled));
    NX_GL(glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled));
    NX_GL(glGetBooleanv(GL_BLEND, &blendEnabled));
    if (stencilTestEnabled)
        NX_GL(glDisable(GL_STENCIL_TEST));
    if (depthTestEnabled)
        NX_GL(glDisable(GL_DEPTH_TEST));
    if (scissorTestEnabled)
        NX_GL(glDisable(GL_SCISSOR_TEST));
    if (blendEnabled)
        NX_GL(glDisable(GL_BLEND));
#endif // 1

#if 1
    GLint prevFbo = 0;
    NX_GL(funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo));
    NX_GL((*outFbo)->bind());
    checkGlFramebufferStatus();
#endif // 1

    GLint prevTexUnit = 0;
    NX_GL(funcs->glGetIntegerv(GL_ACTIVE_TEXTURE, &prevTexUnit));
    NX_GL(m_yTex.bind(0));
    NX_GL(m_uTex.bind(1));
    NX_GL(m_vTex.bind(2));

    GLvoid* prevPtr;
    glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &prevPtr);

    GLint prevType, prevStride, prevEnabled, prevVbo, prevArraySize, prevNorm;
    NX_GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &prevType));
    NX_GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &prevStride));
    NX_GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &prevEnabled));
    NX_GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &prevVbo));
    NX_GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &prevArraySize));
    NX_GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &prevNorm));

    GLint prevProgram = 0;
    NX_GL(funcs->glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram));
    NX_GL(m_program->bind());
#if 1
    NX_GL(m_program->setUniformValue("yTexture", 0));
    NX_GL(m_program->setUniformValue("uTexture", 1));
    NX_GL(m_program->setUniformValue("vTexture", 2));
#endif // 1
    NX_GL(m_program->enableAttributeArray(0));

    GLint prevViewport[4];
    NX_GL(funcs->glGetIntegerv(GL_VIEWPORT, prevViewport));
    NX_GL(funcs->glViewport(
        0, 0, yuvBuffer->frameSize().width(), yuvBuffer->frameSize().height()));

    static const GLfloat g_vertex_data[] =
    {
        -1.f, 1.f,
        1.f, 1.f,
        1.f, -1.f,
        -1.f, -1.f
    };

    NX_GL(funcs->glBindBuffer(GL_ARRAY_BUFFER, 0));
    NX_GL(funcs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data));

    // Perform actual YUV->RGB transformation.
#if 1
    NX_GL(funcs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
#endif // 1
    NX_GL(funcs->glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]));

    NX_GL(m_program->disableAttributeArray(0));
    NX_GL(funcs->glUseProgram(prevProgram));

    NX_GL(m_vTex.release(2));
    NX_GL(m_uTex.release(1));
    NX_GL(m_yTex.release(0));
    NX_GL(funcs->glActiveTexture(prevTexUnit));

    NX_GL(funcs->glBindBuffer(GL_ARRAY_BUFFER, prevVbo));

    NX_GL(funcs->glVertexAttribPointer(0, prevArraySize, prevType, prevNorm, prevStride, prevPtr));
    if (prevEnabled)
        NX_GL(funcs->glEnableVertexAttribArray(0));

#if 1
    NX_GL(funcs->glBindFramebuffer(GL_FRAMEBUFFER, prevFbo)); // sets both READ and DRAW
    checkGlFramebufferStatus();
#endif // 1

    //NX_GL(glFlush());
    //NX_GL(glFinish());

    // restore render states
#if 1
    if (stencilTestEnabled)
        NX_GL(glEnable(GL_STENCIL_TEST));
    if (depthTestEnabled)
        NX_GL(glEnable(GL_DEPTH_TEST));
    if (scissorTestEnabled)
        NX_GL(glEnable(GL_SCISSOR_TEST));
    if (blendEnabled)
        NX_GL(glEnable(GL_BLEND));
#endif // 1

    NX_TIME_MARK(RenderYuvBufferToFbo, "t3");

#if 0
    if (m_threadGlCtx)
    {
        // We used decoder thread to render. Flush everything to the texture.
        NX_TIME_BEGIN(RenderYuvBufferToFbo_glFinish)
        funcs->glFlush();
        funcs->glFinish();
        NX_TIME_END(RenderYuvBufferToFbo_glFinish)
        glFinish();
    }
#endif // 0

#endif // 1 // ALL

    NX_TIME_END(RenderYuvBufferToFbo);
}

int Impl::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData,
    QVideoFramePtr* outDecodedFrame)
{
    NX_CRITICAL(outDecodedFrame);

    auto yuvBuffer = std::make_shared<YuvBuffer>(frameSize());
    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    int64_t ptsUs;
#if 1
    NX_TIME_BEGIN(DecodeToYuvPlanar);
    // Perform actual decoding from QnCompressedVideoData to QVideoFrame.
    int result = proxyDecoder().decodeToYuvPlanar(compressedFrame.get(), &ptsUs,
        yuvBuffer->y(), yuvBuffer->yLineSize(),
        yuvBuffer->u(), yuvBuffer->v(), yuvBuffer->uVLineSize());
    NX_TIME_END(DecodeToYuvPlanar);
#endif // 1
    if (result < 0)
        NX_PRINT << "ERROR: ProxyDecoder::decodeToYuvPlanar() -> " << result;

    if (result <= 0)
        return result;

    if (ini().useGlGuiRendering)
    {
        auto textureBuffer = new TextureBuffer(FboPtr(nullptr), sharedPtrToThis(), yuvBuffer);
        setQVideoFrame(outDecodedFrame, textureBuffer, QVideoFrame::Format_BGR32, ptsUs);
    }
    else
    {
        FboPtr fboToRender;
        if (ini().useSharedGlContext)
        {
            renderYuvBufferToFbo(yuvBuffer.get(), &fboToRender);
            auto textureBuffer = new TextureBuffer(
                fboToRender, sharedPtrToThis(), ConstYuvBufferPtr(nullptr));
            setQVideoFrame(outDecodedFrame, textureBuffer, QVideoFrame::Format_BGR32, ptsUs);
        }
        else
        {
#if 0
            allocator().execAtGlThreadAsync(
            [yuvBuffer, &fboToRender, this]()
            {
                renderYuvBufferToFbo(yuvBuffer.get(), &fboToRender);
            });
#endif // 0

            auto textureBuffer = new TextureBuffer(FboPtr(nullptr), sharedPtrToThis(), yuvBuffer);
            setQVideoFrame(outDecodedFrame, textureBuffer, QVideoFrame::Format_BGR32, ptsUs);

            const auto decodedFrame = *outDecodedFrame;
            allocator().execAtGlThreadAsync(
                [decodedFrame]()
                {
                    decodedFrame->handle();
                });
        }
    }

    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderImpl* ProxyVideoDecoderImpl::createImplGl(const Params& params)
{
    NX_PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
