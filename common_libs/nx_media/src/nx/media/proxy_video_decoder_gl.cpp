#include "proxy_video_decoder_private.h"
#if defined(ENABLE_PROXY_DECODER)

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLShaderProgram>

#include "proxy_video_decoder_gl_utils.h"

namespace nx {
namespace media {

namespace {

static const bool USE_SHARED_CTX = true;

class Impl
:
    public ProxyVideoDecoderPrivate
{
public:
    Impl(const Params& params)
    :
        ProxyVideoDecoderPrivate(params),
        m_program(nullptr),
        m_yTex(QOpenGLTexture::Target2D),
        m_uTex(QOpenGLTexture::Target2D),
        m_vTex(QOpenGLTexture::Target2D)
    {
    }

    ~Impl()
    {
        OUTPUT << "~Impl() BEGIN";
        if (m_threadGlCtx)
        {
            GL_GET_FUNCS(m_threadGlCtx.get());
            if (m_program)
            {
                GL(m_yTex.release());
                GL(m_yTex.destroy());
                GL(m_uTex.release());
                GL(m_uTex.destroy());
                GL(m_vTex.release());
                GL(m_vTex.destroy());
                GL(m_program->release());
                m_program.reset(nullptr);
                m_fboManager.reset(nullptr);
            }
            m_threadGlCtx->doneCurrent();
            m_threadGlCtx.reset(nullptr);
        }
        m_offscreenSurface.reset(nullptr);
        OUTPUT << "~Impl() END";
    }

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;

private:
    class TextureBuffer;

    /**
     * @return Value with the same semantics as AbstractVideoDecoder::decode().
     */
    static int decodeFrameToYuvBuffer(
        const ProxyDecoder::CompressedFrame* compressedFrame,
        ProxyDecoder& proxyDecoder,
        YuvBuffer* yuvBuffer);

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

class Impl::TextureBuffer
:
    public QAbstractVideoBuffer
{
public:
    TextureBuffer(
        const FboPtr& fbo,
        const std::shared_ptr<ProxyVideoDecoderPrivate>& owner,
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
        GL_GET_FUNCS(QOpenGLContext::currentContext());
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
        if (conf.useSharedGlContext && !conf.useGlGuiRendering)
        {
            // Create shared GL context.
            QOpenGLContext* sharedContext = QOpenGLContext::globalShareContext();
            NX_CRITICAL(sharedContext);

            m_threadGlCtx.reset(new QOpenGLContext());
            GL_GET_FUNCS(m_threadGlCtx.get());
            GL(m_threadGlCtx->setShareContext(sharedContext));
            GL(m_threadGlCtx->setFormat(sharedContext->format()));

            GL_CHECK(m_threadGlCtx->create());
            GL_CHECK(m_threadGlCtx->shareContext());
            PRINT << "Using shared openGL ctx";
            m_offscreenSurface.reset(new QOffscreenSurface());
            GL(m_offscreenSurface->setFormat(m_threadGlCtx->format()));
            GL(m_offscreenSurface->create());
            GL_CHECK(m_threadGlCtx->makeCurrent(m_offscreenSurface.get()));
        }
    }

    if (!m_fboManager)
        m_fboManager.reset(new FboManager(frameSize(), /*queueSize*/ 1));

    if (!m_program)
    {
        GL_GET_FUNCS(QOpenGLContext::currentContext())
#if 1 // all program
        m_program.reset(new QOpenGLShaderProgram());

        auto vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, m_program.get());
        GL_CHECK(vertexShader->compileSourceCode(
            "attribute mediump vec4 vertexCoordsArray; \n"
            "varying   mediump vec2 textureCoords; \n"
            "void main(void) \n"
            "{ \n"
            "    gl_Position = vertexCoordsArray; \n"
            "    textureCoords = ((vertexCoordsArray + vec4(1.0, 1.0, 0.0, 0.0)) * 0.5).xy; \n"
            "}\n"));
        GL_DUMP(vertexShader);
        GL(m_program->addShader(vertexShader));

        auto fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, m_program.get());
        GL_CHECK(fragmentShader->compileSourceCode(
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
        GL_DUMP(fragmentShader);
        GL(m_program->addShader(fragmentShader));

        GL(m_program->bindAttributeLocation("vertexCoordsArray", 0));
        GL(m_program->link());
        GL_DUMP(m_program);

#endif // 1 // all program

        GL(m_yTex.setFormat(QOpenGLTexture::LuminanceFormat));
        GL(m_yTex.setSize(frameSize().width(), frameSize().height()));
        GL(m_yTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        GL(m_yTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));

        GL(m_uTex.setFormat(QOpenGLTexture::LuminanceFormat));
        GL(m_uTex.setSize(frameSize().width() / 2, frameSize().height() / 2));
        GL(m_uTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        GL(m_uTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));

        GL(m_vTex.setFormat(QOpenGLTexture::LuminanceFormat));
        GL(m_vTex.setSize(frameSize().width() / 2, frameSize().height() / 2));
        GL(m_vTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        GL(m_vTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));
    }
}

int Impl::decodeFrameToYuvBuffer(
    const ProxyDecoder::CompressedFrame* compressedFrame, ProxyDecoder& proxyDecoder,
    YuvBuffer* yuvBuffer)
{
    int64_t outPts;
    int result = 1;
#if 1
    TIME_BEGIN(decodeToYuvPlanar);
        // Perform actual decoding from QnCompressedVideoData to QVideoFrame.
        result = proxyDecoder.decodeToYuvPlanar(compressedFrame, &outPts,
            yuvBuffer->y(), yuvBuffer->yLineSize(),
            yuvBuffer->u(), yuvBuffer->v(), yuvBuffer->uVLineSize());
    TIME_END(decodeToYuvPlanar);
#endif // 1
    if (result < 0)
        PRINT << "ERROR: ProxyDecoder::decodeToYuvPlanar() -> " << result;

    return result;
}

void Impl::renderYuvBufferToFbo(const YuvBuffer* yuvBuffer, FboPtr* outFbo)
{
    createGlResources();

    DebugTimer timer("renderYuvBufferToFbo");

    GL_GET_FUNCS(QOpenGLContext::currentContext());

    NX_CRITICAL(outFbo);
    *outFbo = m_fboManager->getFbo();
    NX_CRITICAL(*outFbo);

    // OLD: Measured time (Full-HDs frame): YUV: 155 ms; Y-only: 120 ms; Y memcpy: 2 ms (!!!).
#if 0
    TIME_BEGIN(flush_finish);
    GL(funcs->glFlush());
    GL(funcs->glFinish());
    TIME_END(flush_finish);
#endif // 0

    timer.mark("t1");

    TIME_BEGIN(renderYuvBufferToFbo_setData);
#if 0 // NO_QT
    GL(funcs->glBindTexture(GL_TEXTURE_2D, (*outFbo)->texture()));
    GL(funcs->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
        yuvBuffer->frameSize().width(), yuvBuffer->frameSize().height(), 0,
        GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvBuffer->y()));
#else // 0 // NO_QT
#if 1
    GL(m_yTex.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvBuffer->y()));
    GL(m_uTex.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvBuffer->u()));
    GL(m_vTex.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvBuffer->v()));
    //TIME_BEGIN(renderYuvBufferToFbo_setData_Flush_Finish);
    //    GL(funcs->glFlush());
    //    GL(funcs->glFinish());
    //TIME_END(renderYuvBufferToFbo_setData_Flush_Finish);
#endif // 1
#endif // 0 NO_QT
    TIME_END(renderYuvBufferToFbo_setData);

    timer.mark("t2");

#if 1 // ALL

    // save current render states
#if 1
    GLboolean stencilTestEnabled;
    GLboolean depthTestEnabled;
    GLboolean scissorTestEnabled;
    GLboolean blendEnabled;
    GL(glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled));
    GL(glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled));
    GL(glGetBooleanv(GL_SCISSOR_TEST, &scissorTestEnabled));
    GL(glGetBooleanv(GL_BLEND, &blendEnabled));
    if (stencilTestEnabled)
        GL(glDisable(GL_STENCIL_TEST));
    if (depthTestEnabled)
        GL(glDisable(GL_DEPTH_TEST));
    if (scissorTestEnabled)
        GL(glDisable(GL_SCISSOR_TEST));
    if (blendEnabled)
        GL(glDisable(GL_BLEND));
#endif // 1

#if 1
    GLint prevFbo = 0;
    GL(funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo));
    GL((*outFbo)->bind());
    checkGlFramebufferStatus();
#endif // 1

    GLint prevTexUnit = 0;
    GL(funcs->glGetIntegerv(GL_ACTIVE_TEXTURE, &prevTexUnit));
    GL(m_yTex.bind(0));
    GL(m_uTex.bind(1));
    GL(m_vTex.bind(2));

    GLvoid* prevPtr;
    glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &prevPtr);

    GLint prevType, prevStride, prevEnabled, prevVbo, prevArraySize, prevNorm;
    GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &prevType));
    GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &prevStride));
    GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &prevEnabled));
    GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &prevVbo));
    GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &prevArraySize));
    GL(funcs->glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &prevNorm));

    GLint prevProgram = 0;
    GL(funcs->glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram));
    GL(m_program->bind());
#if 1
    GL(m_program->setUniformValue("yTexture", 0));
    GL(m_program->setUniformValue("uTexture", 1));
    GL(m_program->setUniformValue("vTexture", 2));
#endif // 1
    GL(m_program->enableAttributeArray(0));

    GLint prevViewport[4];
    GL(funcs->glGetIntegerv(GL_VIEWPORT, prevViewport));
    GL(funcs->glViewport(0, 0, yuvBuffer->frameSize().width(), yuvBuffer->frameSize().height()));

    static const GLfloat g_vertex_data[] =
    {
        -1.f, 1.f,
        1.f, 1.f,
        1.f, -1.f,
        -1.f, -1.f
    };

    GL(funcs->glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL(funcs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_data));

    // Perform actual YUV->RGB transformation.
#if 1
    GL(funcs->glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
#endif // 1
    GL(funcs->glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]));

    GL(m_program->disableAttributeArray(0));
    GL(funcs->glUseProgram(prevProgram));

    GL(m_vTex.release(2));
    GL(m_uTex.release(1));
    GL(m_yTex.release(0));
    GL(funcs->glActiveTexture(prevTexUnit));

    GL(funcs->glBindBuffer(GL_ARRAY_BUFFER, prevVbo));

    GL(funcs->glVertexAttribPointer(0, prevArraySize, prevType, prevNorm, prevStride, prevPtr));
    if (prevEnabled)
        GL(funcs->glEnableVertexAttribArray(0));

#if 1
    GL(funcs->glBindFramebuffer(GL_FRAMEBUFFER, prevFbo)); // sets both READ and DRAW
    checkGlFramebufferStatus();
#endif // 1

    //GL(glFlush());
    //GL(glFinish());

    // restore render states
#if 1
    if (stencilTestEnabled)
        GL(glEnable(GL_STENCIL_TEST));
    if (depthTestEnabled)
        GL(glEnable(GL_DEPTH_TEST));
    if (scissorTestEnabled)
        GL(glEnable(GL_SCISSOR_TEST));
    if (blendEnabled)
        GL(glEnable(GL_BLEND));
#endif // 1

    timer.mark("t3");

#if 0
    if (m_threadGlCtx)
    {
        // we used decoder thread to render. flush everything to the texture
        TIME_BEGIN("renderYuvBufferToFbo::glFinish")
            funcs->glFlush();
        funcs->glFinish();
        TIME_END
            glFinish();
    }
#endif // 0

#endif // 1 // ALL

    timer.finish("t4");
}

int Impl::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData,
    QVideoFramePtr* outDecodedFrame)
{
    NX_CRITICAL(outDecodedFrame);

    auto yuvBuffer = std::make_shared<YuvBuffer>(frameSize());
    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    int result = decodeFrameToYuvBuffer(
        compressedFrame.get(), proxyDecoder(), yuvBuffer.get());
    if (result <= 0)
        return result;

    QAbstractVideoBuffer* textureBuffer;
    if (conf.useGlGuiRendering)
    {
        textureBuffer = new TextureBuffer(FboPtr(nullptr), sharedPtrToThis(), yuvBuffer);
        outDecodedFrame->reset(new QVideoFrame(
            textureBuffer, frameSize(), QVideoFrame::Format_BGR32));
    }
    else
    {
        FboPtr fboToRender;
        if (conf.useSharedGlContext)
        {
            renderYuvBufferToFbo(yuvBuffer.get(), &fboToRender);
            textureBuffer = new TextureBuffer(
                fboToRender, sharedPtrToThis(), ConstYuvBufferPtr(nullptr));
            outDecodedFrame->reset(new QVideoFrame(
                textureBuffer, frameSize(), QVideoFrame::Format_BGR32));
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

            textureBuffer = new TextureBuffer(FboPtr(nullptr), sharedPtrToThis(), yuvBuffer);

            outDecodedFrame->reset(new QVideoFrame(
                textureBuffer, frameSize(), QVideoFrame::Format_BGR32));

            auto decodedFrame = *outDecodedFrame;

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

ProxyVideoDecoderPrivate* ProxyVideoDecoderPrivate::createImplGl(const Params& params)
{
    PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
