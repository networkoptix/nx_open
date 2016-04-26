// Configuration
#define xENABLE_GL_LOG
#define ENABLE_GL_FATAL_ERRORS
static const bool USE_GUI_RENDERING = false;
static const bool USE_SHARED_CTX = true;

#include "proxy_video_decoder_gl_utils.cxx"

class ProxyVideoDecoderPrivate
{
private:
    ProxyVideoDecoder* q;

public:
    ProxyVideoDecoderPrivate(
        ProxyVideoDecoder* owner, const ResourceAllocatorPtr& allocator, const QSize& resolution)
    :
        q(owner),
        m_frameSize(resolution),
        m_allocator(allocator),
        m_proxyDecoder(ProxyDecoder::create(resolution.width(), resolution.height())),
        m_program(nullptr),
        m_yTex(QOpenGLTexture::Target2D),
        m_uTex(QOpenGLTexture::Target2D),
        m_vTex(QOpenGLTexture::Target2D)
    {
        // Currently, only even frame dimensions are supported due to UV having half-res.
        NX_CRITICAL(m_frameSize.width() % 2 == 0 || m_frameSize.height() % 2 == 0);
    }

    ~ProxyVideoDecoderPrivate()
    {
        QLOG("ProxyVideoDecoderPrivate<gl>::~ProxyVideoDecoderPrivate() BEGIN");
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

        QLOG("ProxyVideoDecoderPrivate<gl>::~ProxyVideoDecoderPrivate() END");
    }

    /**
     * @param compressedVideoData Has non-null data().
     * @return Value with the same semantics as AbstractVideoDecoder::decode().
     */
    int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame);

private:
    class TextureBuffer;

    /** @return Value with the same semantics as AbstractVideoDecoder::decode(). */
    static int decodeFrameToYuvBuffer(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        ProxyDecoder* proxyDecoder,
        YuvBuffer* yuvBuffer);

    /** Initialize OpenGL resources on first call; subsequent calls do nothing. */
    void createGlResources();

    void renderYuvBufferToFbo(const YuvBuffer* yuvBuffer, FboPtr* outFbo);

private:
    QSize m_frameSize;
    ResourceAllocatorPtr m_allocator;
    std::unique_ptr<ProxyDecoder> m_proxyDecoder;

    std::unique_ptr<QOffscreenSurface> m_offscreenSurface;

    std::unique_ptr<QOpenGLContext> m_threadGlCtx;
    std::unique_ptr<QOpenGLShaderProgram> m_program;

    std::unique_ptr<FboManager> m_fboManager;

    QOpenGLTexture m_yTex;
    QOpenGLTexture m_uTex;
    QOpenGLTexture m_vTex;
};

class ProxyVideoDecoderPrivate::TextureBuffer
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
        m_owner(owner),
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
        // TODO mike: REMOVE
        //debugTextureTest();
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
    std::weak_ptr<ProxyVideoDecoderPrivate> m_owner;
    ConstYuvBufferPtr m_yuvBuffer;
};

void ProxyVideoDecoderPrivate::createGlResources()
{
    if (!m_threadGlCtx)
    {
        if (USE_SHARED_CTX && !USE_GUI_RENDERING)
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
            NX_LOG(lm("Using shared openGL ctx"), cl_logINFO);
            m_offscreenSurface.reset(new QOffscreenSurface());
            GL(m_offscreenSurface->setFormat(m_threadGlCtx->format()));
            GL(m_offscreenSurface->create());
            GL_CHECK(m_threadGlCtx->makeCurrent(m_offscreenSurface.get()));
        }
    }

    if (!m_fboManager)
        m_fboManager.reset(new FboManager(m_frameSize, /*queueSize*/ 1));

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
        LOG_GL(vertexShader);
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
        LOG_GL(fragmentShader);
        GL(m_program->addShader(fragmentShader));

        GL(m_program->bindAttributeLocation("vertexCoordsArray", 0));
        GL(m_program->link());
        LOG_GL(m_program);

#endif // 1 // all program

        GL(m_yTex.setFormat(QOpenGLTexture::LuminanceFormat));
        GL(m_yTex.setSize(m_frameSize.width(), m_frameSize.height()));
        GL(m_yTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        GL(m_yTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));

        GL(m_uTex.setFormat(QOpenGLTexture::LuminanceFormat));
        GL(m_uTex.setSize(m_frameSize.width() / 2, m_frameSize.height() / 2));
        GL(m_uTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        GL(m_uTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));

        GL(m_vTex.setFormat(QOpenGLTexture::LuminanceFormat));
        GL(m_vTex.setSize(m_frameSize.width() / 2, m_frameSize.height() / 2));
        GL(m_vTex.allocateStorage(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8));
        GL(m_vTex.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest));
    }
}

int ProxyVideoDecoderPrivate::decodeFrameToYuvBuffer(
    const QnConstCompressedVideoDataPtr& compressedVideoData,
    ProxyDecoder* proxyDecoder,
    YuvBuffer* yuvBuffer)
{
    std::unique_ptr<ProxyDecoder::CompressedFrame> compressedFrame;
    if (compressedVideoData)
    {
        NX_CRITICAL(compressedVideoData->data());
        NX_CRITICAL(compressedVideoData->dataSize() > 0);
        compressedFrame.reset(new ProxyDecoder::CompressedFrame);
        compressedFrame->data = (const uint8_t*) compressedVideoData->data();
        compressedFrame->dataSize = compressedVideoData->dataSize();
        compressedFrame->pts = compressedVideoData->timestamp;
        compressedFrame->isKeyFrame =
            (compressedVideoData->flags & QnAbstractMediaData::MediaFlags_AVKey) != 0;
    }

    int64_t outPts;
    int result = 1;
#if 1
    TIME_BEGIN("decodeToYuvPlanar")
        // Perform actual decoding from QnCompressedVideoData to QVideoFrame.
        result = proxyDecoder->decodeToYuvPlanar(compressedFrame.get(), &outPts,
            yuvBuffer->y(), yuvBuffer->yLineSize(),
            yuvBuffer->u(), yuvBuffer->v(), yuvBuffer->uVLineSize());
    TIME_END
#endif // 1
        if (result < 0)
            NX_LOG(lm("ERROR: ProxyDecoder::decodeToYuvPlanar() -> %1").arg(result), cl_logERROR);

    return result;
}

void ProxyVideoDecoderPrivate::renderYuvBufferToFbo(const YuvBuffer* yuvBuffer, FboPtr* outFbo)
{
    createGlResources();

    showFps();
    TIME_START();

    GL_GET_FUNCS(QOpenGLContext::currentContext());

    NX_CRITICAL(outFbo);
    *outFbo = m_fboManager->getFbo();
    NX_CRITICAL(*outFbo);

    // OLD: Measured time (Full-HDs frame): YUV: 155 ms; Y-only: 120 ms; Y memcpy: 2 ms (!!!).
#if 0
    TIME_BEGIN("Flush && Finish")
    GL(funcs->glFlush());
    GL(funcs->glFinish());
    TIME_END
#endif // 0

        TIME_PUSH("t1");

    TIME_BEGIN("renderYuvBufferToFbo::setData")
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
    //TIME_BEGIN("renderYuvBufferToFbo::setData::Flush && Finish")
    //    GL(funcs->glFlush());
    //    GL(funcs->glFinish());
    //TIME_END
#endif // 1
#endif // 0 NO_QT
    TIME_END

    TIME_PUSH("t2");

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

    TIME_PUSH("t3");

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

    TIME_FINISH("renderYuvBufferToFbo");
}

int ProxyVideoDecoderPrivate::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    NX_CRITICAL(outDecodedFrame);

    auto yuvBuffer = std::make_shared<YuvBuffer>(m_frameSize);

    int result = decodeFrameToYuvBuffer(
        compressedVideoData, m_proxyDecoder.get(), yuvBuffer.get());
    if (result <= 0)
        return result;

    QAbstractVideoBuffer* textureBuffer;
    if (USE_GUI_RENDERING)
    {
        textureBuffer = new TextureBuffer(FboPtr(nullptr), q->d, yuvBuffer);
        outDecodedFrame->reset(new QVideoFrame(
            textureBuffer, m_frameSize, QVideoFrame::Format_BGR32));
    }
    else
    {
        FboPtr fboToRender;
        if (USE_SHARED_CTX)
        {
            renderYuvBufferToFbo(yuvBuffer.get(), &fboToRender);
            textureBuffer = new TextureBuffer(fboToRender, q->d, ConstYuvBufferPtr(nullptr));
            outDecodedFrame->reset(new QVideoFrame(
                textureBuffer, m_frameSize, QVideoFrame::Format_BGR32));
        }
        else
        {
#if 0
            m_allocator->execAtGlThreadAsync(
            [yuvBuffer, &fboToRender, this]()
            {
            renderYuvBufferToFbo(yuvBuffer.get(), &fboToRender);
            });
#endif // 0

            textureBuffer = new TextureBuffer(FboPtr(nullptr), q->d, yuvBuffer);

            outDecodedFrame->reset(new QVideoFrame(
                textureBuffer, m_frameSize, QVideoFrame::Format_BGR32));

            auto decodedFrame = *outDecodedFrame;

            m_allocator->execAtGlThreadAsync(
                [decodedFrame]()
            {
                decodedFrame->handle();
            });
        }
    }

    return result;
}
