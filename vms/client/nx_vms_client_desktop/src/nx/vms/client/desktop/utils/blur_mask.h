// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QtGui/QOpenGLFunctions>
#include <QtQuick/QQuickFramebufferObject>

class QOpenGLFramebufferObject;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;
class QnOpenGLRenderer;
class QnColorGLShaderProgram;
class QnBlurShaderProgram;
class QOpenGLTexture;

namespace nx::vms::client::core { class AbstractAnalyticsMetadataProvider; }

namespace nx::vms::client::desktop {

class BlurMask
{
public:
    struct Vertex
    {
        GLfloat x = 0.0F;
        GLfloat y = 0.0F;
    };

    static constexpr int kVerticesPerRectangle = 6;
    static constexpr int kInitialRectangleCount = 256;

public:
    BlurMask(QnOpenGLRenderer* renderer);

    ~BlurMask();

    /**
     * Adds a rectangle to a mask.
     */
    void addRectangle(const QRectF& rect);

    /**
     * Draws blur mask into a frame buffer.
     */
    void draw(QOpenGLFramebufferObject* frameBuffer);

    /**
     * Draws full blur mask into a frame buffer.
     */
    void drawFull(QOpenGLFramebufferObject* frameBuffer);

    /**
     * Draws analytics objects blur mask into a frame buffer.
     */
    void draw(
        QOpenGLFramebufferObject* frameBuffer,
        const core::AbstractAnalyticsMetadataProvider* analyticsProvider,
        const std::optional<QStringList>& objectTypeIds,
        std::chrono::microseconds timestamp,
        int channel);

private:
    void draw();

private:
    QnOpenGLRenderer* const m_renderer = nullptr;
    QnColorGLShaderProgram* m_shader = nullptr;
    std::vector<Vertex> m_vertices = {};
    std::unique_ptr<QOpenGLBuffer> m_buffer;
    std::unique_ptr<QOpenGLVertexArrayObject> m_object;
};

class BlurMaskPreview: public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QString imageSource MEMBER m_imageSource NOTIFY imageSourceChanged)
    Q_PROPERTY(QList<QRectF> blurRectangles MEMBER m_blurRectangles NOTIFY blurRectanglesChanged)
    Q_PROPERTY(double intensity MEMBER m_intensity NOTIFY intensityChanged)

public:
    BlurMaskPreview();
    virtual ~BlurMaskPreview() override;

    virtual Renderer* createRenderer() const;

    static void registerQmlType();

signals:
    void imageSourceChanged();
    void blurRectanglesChanged();
    void intensityChanged(double value);

private:
    QnOpenGLRenderer* const m_renderer = nullptr;
    std::unique_ptr<QOpenGLTexture> m_imageTexture;

    QString m_imageSource;
    QList<QRectF> m_blurRectangles;
    double m_intensity = 1.0;
};

class BlurMaskPreviewRenderer:
    public QObject,
    public QQuickFramebufferObject::Renderer
{
    Q_OBJECT
public:
    BlurMaskPreviewRenderer(
        QnOpenGLRenderer* renderer,
        QOpenGLTexture* texture,
        const QList<QRectF>& blurRectangles);

    virtual ~BlurMaskPreviewRenderer() override;

    virtual QOpenGLFramebufferObject* createFramebufferObject(const QSize &size) override;
    virtual void render() override;

    void setIntensity(double value);

private:
    QnOpenGLRenderer* const m_renderer = nullptr;
    QOpenGLTexture* const m_texture = nullptr;
    QnBlurShaderProgram* const m_shader = nullptr;

    std::unique_ptr<QOpenGLBuffer> m_vertexBuffer;
    std::unique_ptr<QOpenGLBuffer> m_texCoordBuffer;
    std::unique_ptr<QOpenGLVertexArrayObject> m_object;

    std::unique_ptr<QOpenGLFramebufferObject> m_maskFrameBuffer;

    // Temporary frame buffer required for multiple iterations.
    std::unique_ptr<QOpenGLFramebufferObject> m_tempFrameBuffer;

    BlurMask m_mask;
    double m_intensity = 1.0;
};

constexpr int kMaxBlurMaskSize = 2048;

/**
 * Blurs an image contained in a frame buffer.
 * @param drawFunc Function that draws geometry (texture and buffer are bound accordingly during
 *     the process).
 */
void blur(
    std::function<void()> drawFunc,
    QnOpenGLRenderer* renderer,
    QnBlurShaderProgram* shader,
    QOpenGLFramebufferObject* mask,
    QOpenGLFramebufferObject* frameBuffer,
    QOpenGLFramebufferObject* tempBuffer,
    double factor);

} // namespace nx::vms::client::desktop
