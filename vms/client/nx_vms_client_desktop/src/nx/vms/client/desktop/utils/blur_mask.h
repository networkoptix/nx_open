// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtGui/QOpenGLFunctions>

class QOpenGLFramebufferObject;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class QnOpenGLRenderer;
class QnColorGLShaderProgram;

namespace nx::vms::client::core { class AbstractAnalyticsMetadataProvider; }

namespace nx::vms::client::desktop {

class BlurMask
{
public:
    BlurMask(
        const core::AbstractAnalyticsMetadataProvider* provider,
        QnOpenGLRenderer* renderer);

    ~BlurMask();

    /**
     * Draws blur mask into a frame buffer.
     */
    void draw(
        QOpenGLFramebufferObject* frameBuffer,
        std::chrono::microseconds timestamp,
        int channel);

    void drawFull(QOpenGLFramebufferObject* frameBuffer);

private:
    struct Vertex
    {
        GLfloat x = 0.0F;
        GLfloat y = 0.0F;
    };

    static constexpr int kVerticesPerRectangle = 6;
    static constexpr int kInitialRectangleCount = 256;

    void addRectangle(const QRectF& rect);
    void draw();

private:
    const core::AbstractAnalyticsMetadataProvider* m_analyticsProvider = nullptr;
    QnOpenGLRenderer* const m_renderer = nullptr;
    QnColorGLShaderProgram* m_shader = nullptr;
    std::vector<Vertex> m_vertices = {};
    std::unique_ptr<QOpenGLBuffer> m_buffer;
    std::unique_ptr<QOpenGLVertexArrayObject> m_object;
};

} // namespace nx::vms::client::desktop
