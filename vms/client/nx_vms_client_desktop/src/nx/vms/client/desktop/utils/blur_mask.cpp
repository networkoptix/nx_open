// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "blur_mask.h"

#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/desktop/opengl/opengl_renderer.h>
#include <ui/graphics/shaders/blur_shader_program.h>
#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/shaders/texture_color_shader_program.h>

namespace nx::vms::client::desktop {

namespace {

void addRectangle(std::vector<BlurMask::Vertex>& vertices, const QRectF& rect)
{
    vertices.push_back({(GLfloat)rect.left(), (GLfloat)rect.top()});
    vertices.push_back({(GLfloat)rect.right(), (GLfloat)rect.top()});
    vertices.push_back({(GLfloat)rect.left(), (GLfloat)rect.bottom()});
    vertices.push_back({(GLfloat)rect.left(), (GLfloat)rect.bottom()});
    vertices.push_back({(GLfloat)rect.right(), (GLfloat)rect.top()});
    vertices.push_back({(GLfloat)rect.right(), (GLfloat)rect.bottom()});
}

} // namespace

BlurMask::BlurMask(QnOpenGLRenderer* renderer):
    m_renderer(renderer),
    m_buffer(std::make_unique<QOpenGLBuffer>()),
    m_object(std::make_unique<QOpenGLVertexArrayObject>())
{
    m_vertices.reserve(kInitialRectangleCount * kVerticesPerRectangle);
    m_shader = renderer->getColorShader();
    m_shader->bind();

    m_buffer->create();
    m_buffer->bind();
    m_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_buffer->allocate(m_vertices.capacity() * sizeof(Vertex));

    m_object->create();
    m_object->bind();

    m_shader->enableAttributeArray("aPosition");
    m_shader->setAttributeBuffer(
        "aPosition", GL_FLOAT, /*offset*/ 0, /*tupleSize*/ sizeof(Vertex) / sizeof(GLfloat));

    m_object->release();
    m_buffer->release();

    m_shader->release();
}

BlurMask::~BlurMask()
{
    m_buffer->destroy();
    m_object->destroy();
}

void BlurMask::addRectangle(const QRectF& rect)
{
    ::nx::vms::client::desktop::addRectangle(m_vertices, rect);
}

void BlurMask::draw()
{
    if (m_vertices.size() == 0)
        return;

    m_shader->setColor(QVector4D(1.0F, 1.0F, 1.0F, 1.0F));

    m_buffer->bind();

    if (m_vertices.capacity() * sizeof(Vertex) > (size_t)m_buffer->size())
        m_buffer->allocate(m_vertices.capacity() * sizeof(Vertex));

    m_buffer->write(/*offset*/ 0, m_vertices.data(), m_vertices.size() * sizeof(Vertex));

    m_buffer->release();

    m_renderer->glDrawArrays(GL_TRIANGLES, /*first*/ 0, m_vertices.size());
}

void BlurMask::draw(
    QOpenGLFramebufferObject* frameBuffer,
    const core::AbstractAnalyticsMetadataProvider* analyticsProvider,
    const std::optional<QStringList>& objectTypeIds,
    std::chrono::microseconds timestamp,
    int channel)
{
    if (!NX_ASSERT(frameBuffer) || !NX_ASSERT(analyticsProvider))
        return;

    // Taking metadata within some range.
    const QList<nx::common::metadata::ObjectMetadataPacketPtr> metadataPackets =
        analyticsProvider->metadataRange(
            timestamp - std::chrono::milliseconds(1),
            timestamp + std::chrono::milliseconds(1),
            channel);

    m_vertices.clear();

    for (const common::metadata::ObjectMetadataPacketPtr& packet: metadataPackets)
    {
        for (const common::metadata::ObjectMetadata& metadata: packet->objectMetadataList)
        {
            if (!objectTypeIds || objectTypeIds->contains(metadata.typeId))
                addRectangle(metadata.boundingBox);
        }
    }

    draw(frameBuffer);
}

void BlurMask::draw(QOpenGLFramebufferObject* frameBuffer)
{
    m_shader->bind();

    QMatrix4x4 matrix;
    matrix.ortho(QRectF{0.0F, 0.0F, 1.0F, 1.0F}); //< Bounding boxes have [0..1] coordinates.
    m_shader->setModelViewProjectionMatrix(matrix);

    // Restore view port after drawing.
    GLint prevViewPort[4];
    m_renderer->glGetIntegerv(GL_VIEWPORT, prevViewPort);
    m_renderer->glViewport(0, 0, frameBuffer->width(), frameBuffer->height());

    m_object->bind();
    frameBuffer->bind();

    m_renderer->glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    m_renderer->glClear(GL_COLOR_BUFFER_BIT);

    draw();

    frameBuffer->release();
    m_object->release();

    m_renderer->glViewport(prevViewPort[0], prevViewPort[1], prevViewPort[2], prevViewPort[3]);

    m_shader->release();
}

void BlurMask::drawFull(QOpenGLFramebufferObject* frameBuffer)
{
    if (!frameBuffer)
        return;

    frameBuffer->bind();
    m_renderer->glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    m_renderer->glClear(GL_COLOR_BUFFER_BIT);
    frameBuffer->release();
}

BlurMaskPreviewRenderer::BlurMaskPreviewRenderer(
    QnOpenGLRenderer* renderer,
    QOpenGLTexture* texture,
    const QList<QRectF>& m_blurRectangles)
    :
    m_renderer(renderer),
    m_texture(texture),
    m_shader(renderer->getBlurShader()),
    m_mask(renderer)
{
    m_vertexBuffer = std::make_unique<QOpenGLBuffer>();
    m_texCoordBuffer = std::make_unique<QOpenGLBuffer>();
    m_object = std::make_unique<QOpenGLVertexArrayObject>();

    std::vector<BlurMask::Vertex> vertices;
    addRectangle(vertices, {0, 0, 1, 1});

    m_shader->bind();

    m_object->create();
    m_object->bind();

    m_vertexBuffer->create();
    m_vertexBuffer->bind();
    m_vertexBuffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vertexBuffer->allocate(vertices.data(), vertices.size() * sizeof(BlurMask::Vertex));

    m_shader->enableAttributeArray("aPosition");
    m_shader->setAttributeBuffer(
        "aPosition",
        GL_FLOAT,
        /*offset*/ 0,
        /*tupleSize*/ sizeof(BlurMask::Vertex) / sizeof(GLfloat));

    m_texCoordBuffer->create();
    m_texCoordBuffer->bind();
    m_texCoordBuffer->setUsagePattern(QOpenGLBuffer::StaticDraw);

    // Texture coordinates matches vertices.
    m_texCoordBuffer->allocate(vertices.data(), vertices.size() * sizeof(BlurMask::Vertex));

    m_shader->enableAttributeArray("aTexCoord");
    m_shader->setAttributeBuffer(
        "aTexCoord", GL_FLOAT, 0, sizeof(BlurMask::Vertex) / sizeof(GLfloat));

    m_object->release();
    m_vertexBuffer->release();
    m_texCoordBuffer->release();

    m_shader->release();

    for (const QRectF& rect: m_blurRectangles)
    {
        // Transforming coordinates to [0..1].
        m_mask.addRectangle({
            rect.x() / texture->width(),
            rect.y() / texture->height(),
            rect.width() / texture->width(),
            rect.height() / texture->height()});
    }
}

BlurMaskPreviewRenderer::~BlurMaskPreviewRenderer()
{
}

void BlurMaskPreviewRenderer::setIntensity(double value)
{
    m_intensity = value;
    update();
}

QOpenGLFramebufferObject* BlurMaskPreviewRenderer::createFramebufferObject(const QSize &size)
{
    m_maskFrameBuffer = std::make_unique<QOpenGLFramebufferObject>(size);
    m_tempFrameBuffer = std::make_unique<QOpenGLFramebufferObject>(size);
    return new QOpenGLFramebufferObject(size);
}

void BlurMaskPreviewRenderer::render()
{
    m_mask.draw(m_maskFrameBuffer.get());

    m_shader->bind();

    m_renderer->glActiveTexture(GL_TEXTURE0);
    m_renderer->glBindTexture(GL_TEXTURE_2D, m_texture->textureId());

    m_shader->setTexture(0); //< GL_TEXTURE0.

    QMatrix4x4 matrix;
    matrix.ortho(QRectF{0.0F, 1.0F, 1.0F, -1.0F}); //< Flipped vertically.
    m_shader->setModelViewProjectionMatrix(matrix);

    m_object->bind();

    // Draw into the main frame buffer.
    framebufferObject()->bind();
    m_renderer->glDrawArrays(GL_TRIANGLES, /*first*/ 0, 1 * BlurMask::kVerticesPerRectangle);

    blur(
        [&]()
        {
            m_renderer->glDrawArrays(
                GL_TRIANGLES, /*first*/ 0, 1 * BlurMask::kVerticesPerRectangle);
        },
        m_renderer,
        m_shader,
        m_maskFrameBuffer.get(),
        framebufferObject(),
        m_tempFrameBuffer.get(),
        m_intensity);

    m_object->release();
    m_shader->release();
}

BlurMaskPreview::BlurMaskPreview():
    m_renderer(new QnOpenGLRenderer(this))
{
    setMirrorVertically(true);

    connect(this, &BlurMaskPreview::imageSourceChanged, this,
        [this]()
        {
            QImage image(m_imageSource);
            setImplicitSize(image.width(), image.height());
            m_imageTexture = std::make_unique<QOpenGLTexture>(image.mirrored());
        });
}

BlurMaskPreview::~BlurMaskPreview()
{
}

QQuickFramebufferObject::Renderer* BlurMaskPreview::createRenderer() const
{
    auto renderer =
        new BlurMaskPreviewRenderer(m_renderer, m_imageTexture.get(), m_blurRectangles);

    connect(this, &BlurMaskPreview::intensityChanged,
        renderer, &BlurMaskPreviewRenderer::setIntensity);

    return renderer;
}

void BlurMaskPreview::registerQmlType()
{
    qmlRegisterType<BlurMaskPreview>("nx.vms.client.desktop", 1, 0, "BlurMaskPreview");
}

void blur(
    std::function<void()> drawFunc,
    QnOpenGLRenderer* renderer,
    QnBlurShaderProgram* shader,
    QOpenGLFramebufferObject* mask,
    QOpenGLFramebufferObject* frameBuffer,
    QOpenGLFramebufferObject* tempBuffer,
    double intensity)
{
    constexpr int kIterations = 8;

    shader->setTexture(0); //< GL_TEXTURE0.

    renderer->glActiveTexture(GL_TEXTURE1);
    renderer->glBindTexture(GL_TEXTURE_2D, mask->texture());
    shader->setMask(1); //< GL_TEXTURE1.

    for (int i = 0; i < kIterations; ++i)
    {
        // blur A->B, B->A several times.
        const float blurStep = (kIterations - i - 1) * intensity * 1.2;
        const QVector2D textureOffset(
            1.0 / kMaxBlurMaskSize * blurStep,
            1.0 / kMaxBlurMaskSize * blurStep);

        tempBuffer->bind();

        shader->setTextureOffset(textureOffset);
        shader->setHorizontalPass(i % 2 == 0);
        renderer->glActiveTexture(GL_TEXTURE0);
        renderer->glBindTexture(GL_TEXTURE_2D, frameBuffer->texture());

        drawFunc();

        tempBuffer->release();
        std::swap(frameBuffer, tempBuffer);
    }
}

} // namespace nx::vms::client::desktop
