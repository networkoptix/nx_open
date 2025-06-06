// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gl_renderer.h"

#include <QtCore/QCoreApplication> //< For Q_DECLARE_TR_FUNCTIONS.
#include <QtCore/QScopedPointer>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QPainter>
#include <QtGui/rhi/qrhi.h>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QErrorMessage>

#include <nx/media/ffmpeg_video_decoder.h>
#include <nx/media/sse_helper.h>
#include <nx/media/video_frame.h>
#include <nx/media/yuvconvert.h>
#include <nx/pathkit/rhi_paint_engine.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/opengl/opengl_renderer.h>
#include <nx/vms/client/desktop/shaders/media_output_shader_data.h>
#include <nx/vms/client/desktop/shaders/media_output_shader_manager.h>
#include <nx/vms/client/desktop/shaders/media_output_shader_program.h>
#include <nx/vms/client/desktop/shaders/rhi_video_renderer.h>
#include <nx/vms/client/desktop/utils/blur_mask.h>
#include <nx/vms/common/pixelation/pixelation.h>
#include <ui/fisheye/fisheye_ptz_controller.h>
#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/shaders/blur_shader_program.h>
#include <ui/graphics/shaders/texture_color_shader_program.h>
#include <utils/color_space/image_correction.h>
#include <utils/common/util.h>

using nx::vms::client::core::Geometry;
using namespace nx::vms::client::desktop;
using namespace nx::vms::api;

namespace {

int minPow2(int value)
{
    int result = 1;
    while (value > result)
        result <<= 1;

    return result;
}

} // anonymous namespace

// ------------------------------------------------------------------------------------------------
// QnGLRenderer

bool QnGLRenderer::isPixelFormatSupported(AVPixelFormat pixfmt)
{
    switch( pixfmt )
    {
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUV444P:
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_BGR24:
            return true;

        default:
            return false;
    }
}

QnGLRenderer::QnGLRenderer(
    QOpenGLWidget* glWidget,
    QQuickWidget* quickWidget,
    const DecodedPictureToOpenGLUploader& decodedPictureProvider)
    :
    QnGlFunctions(glWidget),
    m_gl(glWidget ? (new QOpenGLFunctions(glWidget->context())) : nullptr),
    m_quickWidget(quickWidget),
    m_decodedPictureProvider( decodedPictureProvider ),
    m_brightness( 0 ),
    m_contrast( 0 ),
    m_hue( 0 ),
    m_saturation( 0 ),
    m_lastDisplayedTime( AV_NOPTS_VALUE ),
    m_lastDisplayedFlags( 0 ),
    m_prevFrameSequence( 0 ),
    m_timeChangeEnabled(true),
    m_paused(false),
    m_screenshotInterface(0),
    m_histogramConsumer(0),
    m_fisheyeController(0),

    m_initialized(false),
    m_positionBuffer(QOpenGLBuffer::VertexBuffer),
    m_textureBuffer(QOpenGLBuffer::VertexBuffer),
    m_blurFactor(0.0),
    m_prevBlurFactor(0.0)
{
    applyMixerSettings( m_brightness, m_contrast, m_hue, m_saturation );
}

QnGLRenderer::~QnGLRenderer()
{
}

void QnGLRenderer::beforeDestroy()
{
    m_vertices.clear();
}

bool QnGLRenderer::isBlurEnabled() const
{
    return m_blurEnabled;
}

void QnGLRenderer::setBlurEnabled(bool value)
{
    if (m_blurEnabled == value)
        return;

    m_blurEnabled = value;
    if (!value)
    {
        m_blurBufferA.reset();
        m_blurBufferB.reset();
        m_blurMaskBuffer.reset();
    }
}

QOpenGLFramebufferObject* QnGLRenderer::blurMaskFrameBuffer() const
{
    return m_blurMaskBuffer.get();
}

void QnGLRenderer::applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
    // normalize the values
    m_brightness = brightness * 128;
    m_contrast = contrast + 1.0;
    m_hue = hue * 180.;
    m_saturation = saturation + 1.0;
}

Qn::RenderStatus QnGLRenderer::prepareBlurBuffers()
{
    if (m_gl)
        NX_ASSERT(QOpenGLFramebufferObject::hasOpenGLFramebufferObjects());

    QSize result;
    DecodedPictureToOpenGLUploader::ScopedPictureLock picLock(m_decodedPictureProvider);
    if (!picLock.get())
    {
            return Qn::NothingRendered;
    }

    result = QSize(picLock->width(), picLock->height());
    if (result.width() > kMaxBlurMaskSize)
    {
        qreal ar = result.width() / (qreal) result.height();
        result = QSize(kMaxBlurMaskSize, kMaxBlurMaskSize / ar);
    }

    // QOpenGLFramebufferObject texture size must be power of 2 for compatibility reasons.
    result.setWidth(minPow2(result.width()));
    result.setHeight(minPow2(result.height()));

    if (m_gl)
    {
        if (!m_blurBufferA || m_blurBufferA->size() != result)
        {
            m_blurBufferA.reset(new QOpenGLFramebufferObject(result));
            m_blurBufferB.reset(new QOpenGLFramebufferObject(result));
            m_blurMaskBuffer.reset(new QOpenGLFramebufferObject(result));
            return Qn::NewFrameRendered;
        }
    }
    else
    {
        auto rhi = m_quickWidget->quickWindow()->rhi();

        if (!m_rhiBlurBuffer || m_rhiBlurBuffer->size() != result || m_rhiBlurBuffer->rhi() != rhi)
        {
            if (!rhi || rhi->isDeviceLost())
            {
                m_rhiBlurBuffer.reset();
                return Qn::CannotRender;
            }

            m_rhiBlurBuffer =
                std::make_unique<nx::vms::common::pixelation::BlurBuffer>(rhi, result);
            return Qn::NewFrameRendered;
        }
    }

    if (qFuzzyEquals(m_blurFactor, m_prevBlurFactor))
        return picLock->sequence() != m_prevFrameSequence ? Qn::NewFrameRendered : Qn::OldFrameRendered;
    m_prevBlurFactor = m_blurFactor;
    return Qn::NewFrameRendered;
}

void QnGLRenderer::doBlurStep(const QRectF& sourceRect, const QRectF& dstRect)
{
    const float v_array[] =
    {
        (float)dstRect.left(),
        (float)dstRect.bottom(),

        (float)dstRect.right(),
        (float)dstRect.bottom(),

        (float)dstRect.right(),
        (float)dstRect.top(),

        (float)dstRect.left(),
        (float)dstRect.top()

    };

    const float tx_array[8] =
    {
        (float)sourceRect.x(), (float)sourceRect.y(),
        (float)sourceRect.right(), (float)sourceRect.top(),
        (float)sourceRect.right(), (float)sourceRect.bottom(),
        (float)sourceRect.x(), (float)sourceRect.bottom()
    };

    const auto renderer = QnOpenGLRendererManager::instance(glWidget());
    const auto shader = renderer->getBlurShader();

    // Shader is already bound in renderBlurFBO.
    drawBindedTexture(shader, v_array, tx_array);
}

void QnGLRenderer::setBlurFactor(qreal value)
{
    m_blurFactor = value;
}

void QnGLRenderer::setBlurRectangles(const QVector<QRectF>& rectangles)
{
    m_blurRectangles = rectangles;
}

Qn::RenderStatus QnGLRenderer::renderBlurFBO(const QRectF &sourceRect)
{
    NX_ASSERT(QOpenGLFramebufferObject::hasOpenGLFramebufferObjects());

    QOpenGLFunctions gl(QOpenGLContext::currentContext());

    GLint prevViewPort[4];
    gl.glGetIntegerv(GL_VIEWPORT, prevViewPort);

    auto renderer = QnOpenGLRendererManager::instance(glWidget());
    auto prevMatrix = renderer->getModelViewMatrix();
    renderer->setModelViewMatrix(QMatrix4x4());

    QRectF dstPaintRect(prevViewPort[0], prevViewPort[1], prevViewPort[2], prevViewPort[3]);

    QSize blurSize = m_blurBufferA->size();
    gl.glViewport(0, 0, blurSize.width(), blurSize.height());

    m_blurBufferA->bind();
    static const qreal kOpaque = 1.0;
    auto result = drawVideoData(nullptr, sourceRect, dstPaintRect, kOpaque);
    m_blurBufferA->release();

    QnBlurShaderProgram* shader = renderer->getBlurShader();
    shader->bind();

    blur(
        [&]() { doBlurStep(sourceRect, dstPaintRect); },
        renderer.get(),
        shader,
        m_blurMaskBuffer.get(),
        m_blurBufferA.get(),
        m_blurBufferB.get(),
        m_blurFactor);

    shader->release();

    renderer->setModelViewMatrix(prevMatrix);
    gl.glViewport(prevViewPort[0], prevViewPort[1], prevViewPort[2], prevViewPort[3]);
    return result;
}

Qn::RenderStatus QnGLRenderer::paint(
    QPainter* painter, const QRectF &sourceRect, const QRectF &targetRect)
{
    if (!m_gl)
    {
        if (!m_blurEnabled || qFuzzyEquals(m_blurFactor, 0.0))
        {
            m_blurBufferA.reset();
            m_blurBufferB.reset();
            m_blurMaskBuffer.reset();
            m_rhiBlurBuffer.reset();
            m_prevBlurFactor = 0.0;
            return drawVideoData(
                painter, sourceRect, targetRect, m_decodedPictureProvider.opacity());
        }
        else
        {
            Qn::RenderStatus result = prepareBlurBuffers();
            if (result == Qn::NothingRendered || result == Qn::CannotRender)
                return result;

            if (result == Qn::NewFrameRendered)
            {
                // Render video frame into blur buffer.
                result = drawVideoData(
                    /* painter */ nullptr,
                    sourceRect,
                    /* targetRect */ {},
                    m_decodedPictureProvider.opacity());

                if (result == Qn::NewFrameRendered)
                    m_rhiBlurBuffer->blur(m_blurRectangles, m_blurFactor);
            }

            if (auto pe = dynamic_cast<nx::pathkit::RhiPaintEngine*>(painter->paintEngine()))
                pe->drawTexture(targetRect, m_rhiBlurBuffer->texture());

            return result;
        }
    }

    m_gl->initializeOpenGLFunctions();

    if (!m_blurEnabled
        || !QOpenGLFramebufferObject::hasOpenGLFramebufferObjects()
        || qFuzzyEquals(m_blurFactor, 0.0))
    {
        m_blurBufferA.reset();
        m_blurBufferB.reset();
        m_blurMaskBuffer.reset();
        m_prevBlurFactor = 0.0;
        return drawVideoData(painter, sourceRect, targetRect, m_decodedPictureProvider.opacity());
    }

    Qn::RenderStatus result = prepareBlurBuffers();
    if (result == Qn::NothingRendered || result == Qn::CannotRender)
        return result;

    if (result == Qn::NewFrameRendered)
        result = renderBlurFBO(sourceRect);

    // paint to screen
    const float v_array[] =
    {
        (float)targetRect.left(),
        (float)targetRect.bottom(),

        (float)targetRect.right(),
        (float)targetRect.bottom(),

        (float)targetRect.right(),
        (float)targetRect.top(),

        (float)targetRect.left(),
        (float)targetRect.top()

    };

    drawVideoTextureDirectly(sourceRect, m_blurBufferA->texture(),
        v_array, m_decodedPictureProvider.opacity());

    return result;
}

Qn::RenderStatus QnGLRenderer::discardFrame()
{
    DecodedPictureToOpenGLUploader::ScopedPictureLock picLock(m_decodedPictureProvider);
    if (!picLock.get())
    {
        NX_VERBOSE(this) << "Exited paint (1)";
        return Qn::NothingRendered;
    }

    m_lastDisplayedFlags = static_cast<unsigned>(picLock->flags());
    const auto result = (picLock->sequence() != m_prevFrameSequence)
        ? Qn::NewFrameRendered
        : Qn::OldFrameRendered;

    m_prevFrameSequence = picLock->sequence();

    return result;
}

Qn::RenderStatus QnGLRenderer::drawVideoData(
    QPainter* painter,
    const QRectF& sourceRect,
    const QRectF& targetRect,
    qreal opacity)
{
    DecodedPictureToOpenGLUploader::ScopedPictureLock picLock(m_decodedPictureProvider);
    if (!picLock.get())
        return Qn::NothingRendered;

    m_lastDisplayedFlags = picLock->flags();

    const int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    if (picLock->width() > maxTextureSize || picLock->height() > maxTextureSize)
        return Qn::CannotRender;

    if (picLock->width() <= 0 && picLock->height() <= 0)
        return Qn::NothingRendered;

    if (m_quickWidget && !painter && !m_rhiBlurBuffer)
        return Qn::NothingRendered;

    const int planeCount = MediaOutputShaderProgram::planeCount(picLock->colorFormat());
    if (!NX_ASSERT(planeCount > 0, "Unsupported color format: %1", picLock->colorFormat()))
        return Qn::CannotRender;

    QRectF textureRect = picLock->textureRect();
    if (auto frame = picLock->decodedFrame(); frame && textureRect.isNull())
        textureRect = QRectF(0, 0, 1, 1);

    if (!drawVideoTextures(
        picLock,
        painter,
        Geometry::subRect(textureRect, sourceRect),
        targetRect,
        planeCount,
        picLock->glTextures().data(),
        (picLock->flags() & QnAbstractMediaData::MediaFlags_StillImage) != 0,
        opacity))
    {
        return Qn::CannotRender;
    }

    NX_MUTEX_LOCKER lock(&m_mutex);
    if ((int64_t) picLock->pts() != AV_NOPTS_VALUE && m_prevFrameSequence != picLock->sequence())
    {
        if (m_timeChangeEnabled)
            m_lastDisplayedTime = picLock->pts();
    }

    const auto result = picLock->sequence() != m_prevFrameSequence
        ? Qn::NewFrameRendered
        : Qn::OldFrameRendered;

    m_prevFrameSequence = picLock->sequence();
    return result;
}

void QnGLRenderer::drawVideoTextureDirectly(
    const QRectF& tex0Coords,
    unsigned int tex0ID,
    const float* v_array,
    qreal opacity)
{
    float tx_array[8] = {
        (float)tex0Coords.x(), (float)tex0Coords.y(),
        (float)tex0Coords.right(), (float)tex0Coords.top(),
        (float)tex0Coords.right(), (float)tex0Coords.bottom(),
        (float)tex0Coords.x(), (float)tex0Coords.bottom()
    };

    m_gl->glActiveTexture(GL_TEXTURE0);
    m_gl->glBindTexture(GL_TEXTURE_2D, tex0ID);
    glCheckError("glBindTexture");

    auto renderer = QnOpenGLRendererManager::instance(glWidget());
    auto shader = renderer->getTextureShader();
    shader->bind();
    shader->setColor(QVector4D(1.0f,1.0f,1.0f, opacity));
    shader->setTexture(0);
    drawBindedTexture(shader, v_array, tx_array);
    shader->release();
}

void QnGLRenderer::setScreenshotInterface(ScreenshotInterface* value) {
    m_screenshotInterface = value;
}

ImageCorrectionResult QnGLRenderer::calcImageCorrection()
{
    if (m_screenshotInterface) {
        QImage img = m_screenshotInterface->getGrayscaleScreenshot();
        m_imageCorrector.analyseImage((quint8*)img.constBits(), img.width(), img.height(), img.bytesPerLine(), m_imgCorrectParam, m_displayedRect);
    }

    return m_imageCorrector;
}

bool QnGLRenderer::drawVideoTextures(
    const DecodedPictureToOpenGLUploader::ScopedPictureLock& picLock,
    QPainter* painter,
    const QRectF& textureRect,
    const QRectF& viewRect,
    int planeCount,
    const unsigned int* textureIds,
    bool isStillImage,
    qreal opacity)
{
    // TODO: #vkutin I'm not sure NV12 will work correctly.
    // Old code was written like this:
    // float textureCoordArray[8] = {
    //    0.0f, 0.0f,
    //    (float)textureRect.x(), 0.0f,
    //    (float)textureRect.x(), (float)textureRect.y(),
    //    0.0f, (float)textureRect.y()};
    //
    // Also there was this commented out code:
    // m_shaders->nv12ToRgb->setParameters(m_brightness / 256.0f, m_contrast, m_hue, m_saturation,
    //    m_decodedPictureProvider.opacity());

    if (!NX_ASSERT(planeCount >= 1 && planeCount <= 4))
        return false;

    MediaOutputShaderProgram::Key programKey;
    programKey.format = MediaOutputShaderProgram::formatFromPlaneCount(planeCount);

    if (m_imgCorrectParam.enabled && programKey.format != MediaOutputShaderProgram::Format::rgb)
        programKey.imageCorrection = MediaOutputShaderProgram::YuvImageCorrection::gamma;

    const auto mediaParams = m_fisheyeController
        ? m_fisheyeController->mediaDewarpingParams()
        : dewarping::MediaData();

    auto itemParams = m_fisheyeController
        ? m_fisheyeController->itemDewarpingParams()
        : dewarping::ViewData();

    float ar = 1.0;
    bool doExtraRotation = false;
    if (mediaParams.enabled && itemParams.enabled)
    {
        if (isDewarpingAllowed())
        {
            ar = float(picLock->width()) / picLock->height();
            const auto customAR = m_fisheyeController->customAR();
            if (customAR.isValid())
                ar = customAR.toFloat();

            programKey.dewarping = true;

            programKey.viewProjection = itemParams.panoFactor > 1.0
                ? MediaOutputShaderProgram::ViewProjection::equirectangular
                : MediaOutputShaderProgram::ViewProjection::rectilinear;

            programKey.cameraProjection = mediaParams.cameraProjection;
        }
        else
        {
            doExtraRotation = true;
            itemParams.enabled = false;
        }
    }

    if (!m_gl)
    {
        auto decodedFrame = picLock->decodedFrame();
        if (!decodedFrame)
            return false;

        // If painter is null, we are rendering into blur buffer.
        nx::pathkit::RhiPaintEngine* pe = painter
            ? dynamic_cast<nx::pathkit::RhiPaintEngine*>(painter->paintEngine())
            : nullptr;

        if (!pe && painter) //< Software.
        {
            const AVPixelFormat format = (AVPixelFormat)decodedFrame->format;
            const unsigned int width = decodedFrame->width;
            const unsigned int height = decodedFrame->height;

            const QRectF sourceRect(
                textureRect.x() * width,
                textureRect.y() * height,
                textureRect.width() * width,
                textureRect.height() * height);

            if (format == AV_PIX_FMT_RGBA)
            {
                QImage nonOwningImage(
                    decodedFrame->data[0],
                    width,
                    height,
                    decodedFrame->linesize[0],
                    QImage::Format_RGBA8888_Premultiplied);

                painter->drawImage(viewRect, nonOwningImage, sourceRect);
            }
            else
            {
                std::unique_ptr<nx::media::VideoFrame> nonOwning(
                    nx::media::FfmpegVideoDecoder::fromAVFrame(decodedFrame.get()));

                painter->drawImage(viewRect, nonOwning->toImage(), sourceRect);
            }
        }
        else
        {
            if (!m_rhiVideoRenderer)
                m_rhiVideoRenderer = std::make_shared<RhiVideoRenderer>();

            nx::vms::client::desktop::MediaOutputShaderData program(programKey);

            program.loadOpacity(opacity);

            if (programKey.imageCorrection == MediaOutputShaderProgram::YuvImageCorrection::gamma)
            {
                const auto parameters = isPaused() || isStillImage
                    ? calcImageCorrection()
                    : picLock->imageCorrectionResult();

                program.loadGammaCorrection(parameters);
                if (m_histogramConsumer)
                    m_histogramConsumer->setHistogramData(parameters);
            }

            program.loadDewarpingParameters(
                mediaParams,
                itemParams,
                ar,
                textureRect.right(),
                textureRect.bottom());

            const auto toDevice =
                [painter](QPointF p)
                {
                    if (!painter)
                        return QVector2D(p);
                    return QVector2D(
                        painter->transform().map(p)
                        * painter->device()->devicePixelRatioF());
                };

            program.verts[0] = toDevice(viewRect.topLeft());
            program.verts[1] = toDevice(viewRect.topRight());
            program.verts[2] = toDevice(viewRect.bottomLeft());
            program.verts[3] = toDevice(viewRect.bottomRight());

            if (doExtraRotation)
                std::reverse(program.verts.begin(), program.verts.end());

            program.texCoords[0] = QVector2D(textureRect.left(), textureRect.top());
            program.texCoords[1] = QVector2D(textureRect.right(), textureRect.top());
            program.texCoords[2] = QVector2D(textureRect.left(), textureRect.bottom());
            program.texCoords[3] = QVector2D(textureRect.right(), textureRect.bottom());

            RhiVideoRenderer::Data d = {
                .frame = decodedFrame,
                .data = program,
                .sourceRect = textureRect
            };

            if (pe)
            {
                // TODO: #ikulaychuk Implement RHI resources cleanup on renderer thread.
                pe->drawCustom({
                    .prepare = [d, renderer = m_rhiVideoRenderer](
                        QRhi* rhi,
                        QRhiRenderPassDescriptor* rp,
                        QRhiResourceUpdateBatch* u,
                        const QMatrix4x4& deviceMvp)
                    {
                        // This function is called  on the rendering thread and create resources.
                        // So the cleanup should happen on the same thread.
                        // But currently we are using this code only in QQuickWidget
                        // which itself does not use threaded rendering.
                        renderer->prepare(d, deviceMvp, rhi, rp, u);
                    },
                    .render = [renderer = m_rhiVideoRenderer](
                        QRhi*, QRhiCommandBuffer* cb, QSize viewportSize)
                    {
                        renderer->render(cb, viewportSize);
                    }
                });
            }
            else
            {
                QRhi* rhi = m_quickWidget->quickWindow()->rhi();
                if (!rhi || rhi->isDeviceLost())
                    return false;

                const QSize size = m_rhiBlurBuffer->size();

                QMatrix4x4 viewProjection = rhi->clipSpaceCorrMatrix();
                const float yCorr = rhi->isYUpInFramebuffer() ? 1 : -1;
                viewProjection.translate(-1, -1 * yCorr, 0);
                viewProjection.scale(2.0 / size.width(), yCorr * 2.0 / size.height());

                const QRectF viewRect = QRectF(QPointF(0, 0), size);
                d.data.verts[0] = QVector2D(viewRect.topLeft());
                d.data.verts[1] = QVector2D(viewRect.topRight());
                d.data.verts[2] = QVector2D(viewRect.bottomLeft());
                d.data.verts[3] = QVector2D(viewRect.bottomRight());

                QRhiCommandBuffer* commandBuffer = nullptr;

                rhi->beginOffscreenFrame(&commandBuffer);
                QRhiResourceUpdateBatch* updateBatch = rhi->nextResourceUpdateBatch();
                m_rhiVideoRenderer->prepare(
                    d,
                    viewProjection,
                    rhi,
                    m_rhiBlurBuffer->renderPassDescriptor(),
                    updateBatch);
                commandBuffer->beginPass(
                    m_rhiBlurBuffer->renderTarget(), Qt::black, {1.0f, 0}, updateBatch);
                commandBuffer->setViewport({
                    0,
                    0,
                    (float) size.width(),
                    (float) size.height()});
                m_rhiVideoRenderer->render(commandBuffer, size);
                commandBuffer->endPass();
                rhi->endOffscreenFrame();
            }
        }
        return true;
    }
    const auto program = MediaOutputShaderManager::instance(QOpenGLContext::currentContext())
        ->program(programKey);

    if (!NX_ASSERT(program && program->bind()))
        return false;

    switch (programKey.format)
    {
        case MediaOutputShaderProgram::Format::rgb:
            program->loadRgbParameters(0, opacity);
            break;

        case MediaOutputShaderProgram::Format::nv12:
            program->loadNv12Parameters(0, 1, opacity);
            break;

        case MediaOutputShaderProgram::Format::yv12:
            program->loadYv12Parameters(0, 1, 2, opacity);
            break;

        case MediaOutputShaderProgram::Format::yva12:
            program->loadYva12Parameters(0, 1, 2, 3, opacity);
            break;
    }

    if (programKey.imageCorrection == MediaOutputShaderProgram::YuvImageCorrection::gamma)
    {
        const auto parameters = isPaused() || isStillImage
            ? calcImageCorrection()
            : picLock->imageCorrectionResult();

        program->loadGammaCorrection(parameters);
        if (m_histogramConsumer)
            m_histogramConsumer->setHistogramData(parameters);
    }

    program->loadDewarpingParameters(mediaParams, itemParams, ar, textureRect.right(),
        textureRect.bottom());

    for (int i = planeCount - 1; i >= 0; --i)
    {
        m_gl->glActiveTexture(GL_TEXTURE0 + i);
        m_gl->glBindTexture(GL_TEXTURE_2D, textureIds[i]);
        glCheckError("glBindTexture");
    }

    const float textureCoordArray[8] = {(float) textureRect.x(),
        (float) textureRect.y(),
        (float) textureRect.right(),
        (float) textureRect.top(),
        (float) textureRect.right(),
        (float) textureRect.bottom(),
        (float) textureRect.x(),
        (float) textureRect.bottom()};

    float vertexCoordArray[] = {(float) viewRect.left(),
        (float) viewRect.top(),
        (float) viewRect.right(),
        (float) viewRect.top(),
        (float) viewRect.right(),
        (float) viewRect.bottom(),
        (float) viewRect.left(),
        (float) viewRect.bottom()};

    if (doExtraRotation)
        std::rotate(vertexCoordArray, vertexCoordArray + 4, vertexCoordArray + 8);

    drawBindedTexture(program, vertexCoordArray, textureCoordArray);
    program->release();
    return true;
}

void QnGLRenderer::drawBindedTexture(
    QnGLShaderProgram* shader, const float* v_array, const float* tx_array)
{
    QByteArray data, texData;
    QnGlBufferStream<GLfloat> vertexStream(&data), texStream(&texData);

    for(int i = 0; i < 8; i++) {
        vertexStream << v_array[i];
        texStream << tx_array[i];
    }

    const int VERTEX_POS_INDX = 0;
    const int VERTEX_TEXCOORD0_INDX = 1;
    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_TEXCOORD0_SIZE = 2; // s and t

    if (!m_initialized)
    {
        if (!m_vertices)
            m_vertices.reset(new QOpenGLVertexArrayObject());
        m_vertices->create();
        m_vertices->bind();

        m_positionBuffer.create();
        m_positionBuffer.setUsagePattern( QOpenGLBuffer::DynamicDraw );
        m_positionBuffer.bind();
        m_positionBuffer.allocate( data.data(), data.size() );
        shader->enableAttributeArray( VERTEX_POS_INDX );
        shader->setAttributeBuffer( VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE );

        m_textureBuffer.create();
        m_textureBuffer.setUsagePattern( QOpenGLBuffer::DynamicDraw );
        m_textureBuffer.bind();
        m_textureBuffer.allocate( texData.data(), texData.size());
        shader->enableAttributeArray( VERTEX_TEXCOORD0_INDX );
        shader->setAttributeBuffer( VERTEX_TEXCOORD0_INDX, GL_FLOAT, 0, VERTEX_TEXCOORD0_SIZE );

        m_positionBuffer.release();

        m_textureBuffer.release();
        m_vertices->release();

        m_initialized = true;
    }
    else
    {
        m_positionBuffer.bind();
        m_positionBuffer.write(0, data.data(), data.size());
        m_positionBuffer.release();

        m_textureBuffer.bind();
        m_textureBuffer.write(0, texData.data(), texData.size());
        m_textureBuffer.release();
    }

    if (!shader->initialized())
    {
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        shader->bindAttributeLocation("aTexCoord",VERTEX_TEXCOORD0_INDX);
        shader->markInitialized();
    };

    QnOpenGLRendererManager::instance(glWidget())->drawBindedTextureOnQuadVao(m_vertices.data(), shader);
}

qint64 QnGLRenderer::lastDisplayedTime() const
{
    NX_MUTEX_LOCKER locker( &m_mutex );
    return m_lastDisplayedTime;
}

bool QnGLRenderer::isLowQualityImage() const
{
    return m_lastDisplayedFlags & QnAbstractMediaData::MediaFlags_LowQuality;
}

bool QnGLRenderer::isHardwareDecoderUsed() const
{
    return m_lastDisplayedFlags & QnAbstractMediaData::MediaFlags_HWDecodingUsed;
}

bool QnGLRenderer::isYV12ToRgbShaderUsed() const
{
    // TODO: #vkutin Remove this function when possible.
    return !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV();
}

bool QnGLRenderer::isYV12ToRgbaShaderUsed() const
{
    // TODO: #vkutin Remove this function when possible.
    return !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV();
}

bool QnGLRenderer::isNV12ToRgbShaderUsed() const
{
    // TODO: #vkutin Remove this function when possible.
    return !(features() & QnGlFunctions::ShadersBroken)
        && !m_decodedPictureProvider.isForcedSoftYUV();
}

void QnGLRenderer::setDisplayedRect(const QRectF& rect)
{
    m_displayedRect = rect;
}

void QnGLRenderer::setHistogramConsumer(QnHistogramConsumer* value)
{
    m_histogramConsumer = value;
}

void QnGLRenderer::setFisheyeController(QnFisheyePtzController* controller)
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    m_fisheyeController = controller;
}

bool QnGLRenderer::isFisheyeEnabled() const
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    return m_fisheyeController && isDewarpingAllowed()
        && m_fisheyeController->getCapabilities({nx::vms::common::ptz::Type::operational})
            != Ptz::Capability::none;
}

bool QnGLRenderer::isDewarpingAllowed() const
{
    return !(m_lastDisplayedFlags & QnAbstractMediaData::MediaFlags_ReplacedVideo);
}
