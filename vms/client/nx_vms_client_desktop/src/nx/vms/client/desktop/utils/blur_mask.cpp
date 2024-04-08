// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "blur_mask.h"

#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLVertexArrayObject>

#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/desktop/opengl/opengl_renderer.h>
#include <ui/graphics/shaders/color_shader_program.h>

namespace nx::vms::client::desktop {

BlurMask::BlurMask(
    const core::AbstractAnalyticsMetadataProvider* provider,
    QnOpenGLRenderer* renderer)
    :
    m_analyticsProvider(provider),
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
    m_vertices.push_back({(GLfloat)rect.left(), (GLfloat)rect.top()});
    m_vertices.push_back({(GLfloat)rect.right(), (GLfloat)rect.top()});
    m_vertices.push_back({(GLfloat)rect.left(), (GLfloat)rect.bottom()});
    m_vertices.push_back({(GLfloat)rect.left(), (GLfloat)rect.bottom()});
    m_vertices.push_back({(GLfloat)rect.right(), (GLfloat)rect.top()});
    m_vertices.push_back({(GLfloat)rect.right(), (GLfloat)rect.bottom()});
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
    std::chrono::microseconds timestamp,
    int channel)
{
    if (!frameBuffer || !m_analyticsProvider)
        return;

    // Taking metadata within some range.
    const QList<nx::common::metadata::ObjectMetadataPacketPtr> metadataPackets =
        m_analyticsProvider->metadataRange(
            timestamp - std::chrono::milliseconds(1),
            timestamp + std::chrono::milliseconds(1),
            channel);

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

    m_vertices.clear();
    for (const common::metadata::ObjectMetadataPacketPtr& packet: metadataPackets)
    {
        for (const common::metadata::ObjectMetadata& metadata: packet->objectMetadataList)
            addRectangle(metadata.boundingBox);
    }

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

} // namespace nx::vms::client::desktop
