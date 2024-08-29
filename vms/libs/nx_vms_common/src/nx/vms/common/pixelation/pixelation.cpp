// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation.h"

#include <memory>

#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtGui/QOffscreenSurface>
#include <QtGui/rhi/qrhi.h>

#include <nx/utils/log/assert.h>

#include "shaders.h"

namespace nx::vms::common::pixelation {
namespace {

struct Vertex
{
    float x;
    float y;
};

void addRectangle(std::vector<Vertex>& vertices, const QRectF& rect)
{
    vertices.push_back({(float)rect.left(), (float)rect.top()});
    vertices.push_back({(float)rect.right(), (float)rect.top()});
    vertices.push_back({(float)rect.left(), (float)rect.bottom()});
    vertices.push_back({(float)rect.left(), (float)rect.bottom()});
    vertices.push_back({(float)rect.right(), (float)rect.top()});
    vertices.push_back({(float)rect.right(), (float)rect.bottom()});
}

/**
 * Provides initialized resources and convenient methods for offscreen rendering.
 */
class RenderingProcess
{
public:
    RenderingProcess(QRhi* rhi);
    virtual ~RenderingProcess() = default;

    void init(const QSize& size);

    QRhi* rhi() const { return m_rhi; }
    QMatrix4x4 viewProjection() { return m_viewProjection; };
    QRhiGraphicsPipeline* pipeline() { return m_pipeline.get(); }
    QRhiTexture* targetTexture() const { return m_texture.get(); }

    void setSize(const QSize& size);
    void setCaptureImage(bool state) { m_captureImage = state; };
    QImage capturedImage() const { return m_capturedImage; };

protected:
    bool isDebugMode = false;

    virtual void initBuffers() = 0;
    virtual void initShaders() = 0;
    virtual void uploadData(QRhiResourceUpdateBatch* updateBatch) = 0;
    virtual void resultReady(QRhiResourceUpdateBatch* updateBatch) = 0;
    virtual void drawImpl(QRhiCommandBuffer* commandBuffer) = 0;

    void drawOffscreen(QRhiResourceUpdateBatch* uploadBatch = nullptr);

private:
    void initRenderTarget(const QSize& size);
    void initPipeline();
    void updateCapturedImage();

private:
    QRhi* const m_rhi = nullptr;

    QMatrix4x4 m_viewProjection;

    std::unique_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiTextureRenderTarget> m_renderTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_renderPass;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;

    bool m_captureImage = false;
    QRhiReadbackResult m_readbackResult;
    QImage m_capturedImage;
};

RenderingProcess::RenderingProcess(QRhi* rhi):
    m_rhi(rhi)
{
}

void RenderingProcess::init(const QSize& size)
{
    initRenderTarget(size);
    initBuffers();
    initPipeline();
}

void RenderingProcess::setSize(const QSize& size)
{
    if (size != m_texture->pixelSize())
    {
        // Textures can be resized after the pipeline is created.
        m_texture->setPixelSize(size);
        m_texture->create();
    }
}

void RenderingProcess::initRenderTarget(const QSize& size)
{
    m_texture.reset(m_rhi->newTexture(
        QRhiTexture::RGBA8,
        size,
        /*sampleCount*/ 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));

    m_texture->create();
    m_renderTarget.reset(m_rhi->newTextureRenderTarget({m_texture.get()}));
    m_renderPass.reset(m_renderTarget->newCompatibleRenderPassDescriptor());
    m_renderTarget->setRenderPassDescriptor(m_renderPass.get());
    m_renderTarget->create();

    m_viewProjection = m_rhi->clipSpaceCorrMatrix();
    m_viewProjection.ortho(QRectF{0, 0, 1, 1});
}

void RenderingProcess::initPipeline()
{
    m_pipeline.reset(rhi()->newGraphicsPipeline());

    initShaders();

    m_pipeline->setRenderPassDescriptor(m_renderPass.get());
    m_pipeline->create();
}

void RenderingProcess::drawOffscreen(QRhiResourceUpdateBatch* uploadBatch)
{
    QRhiCommandBuffer* commands;
    m_rhi->beginOffscreenFrame(&commands);

    if (!uploadBatch)
        uploadBatch = m_rhi->nextResourceUpdateBatch();

    uploadData(uploadBatch);

    commands->beginPass(
        m_renderTarget.get(), Qt::transparent, QRhiDepthStencilClearValue{}, uploadBatch);

    commands->setGraphicsPipeline(m_pipeline.get());

    const QSize size = m_renderTarget->pixelSize();
    commands->setViewport(QRhiViewport(0.0f, 0.0f, size.width(), size.height()));
    commands->setShaderResources();

    drawImpl(commands);

    QRhiResourceUpdateBatch* resultUpdate = m_rhi->nextResourceUpdateBatch();
    if (m_captureImage || isDebugMode)
        resultUpdate->readBackTexture({targetTexture()}, &m_readbackResult);

    resultReady(resultUpdate);

    commands->endPass(resultUpdate);
    m_rhi->endOffscreenFrame();

    if (m_captureImage || isDebugMode)
        updateCapturedImage();
}

void RenderingProcess::updateCapturedImage()
{
    m_capturedImage = QImage{
        reinterpret_cast<const uchar*>(m_readbackResult.data.constData()),
        m_readbackResult.pixelSize.width(),
        m_readbackResult.pixelSize.height(),
        QImage::Format_RGBA8888_Premultiplied};

    if (rhi()->isYUpInFramebuffer())
        m_capturedImage = m_capturedImage.mirrored();
}

class MaskRenderingProcess: public RenderingProcess
{
public:
    static constexpr int kVerticesPerRectangle = 6;
    static constexpr int kInitialRectangleCount = 256;

public:
    using RenderingProcess::RenderingProcess;
    void draw();

    void addRectangle(const QRectF& rect);
    void clear();

protected:
    virtual void initBuffers() override;
    virtual void initShaders() override;
    virtual void uploadData(QRhiResourceUpdateBatch* updateBatch) override;
    virtual void drawImpl(QRhiCommandBuffer* commandBuffer) override;
    virtual void resultReady(QRhiResourceUpdateBatch* updateBatch) override;

private:
    std::vector<Vertex> m_vertices;
    std::unique_ptr<QRhiBuffer> m_vertexBuffer;

    ColorShader m_shader;
    ColorShader::ShaderData m_shaderData;
    std::unique_ptr<QRhiBuffer> m_dataBuffer;

    bool m_hasChanges = true;
};

void MaskRenderingProcess::initBuffers()
{
    m_vertices.reserve(kInitialRectangleCount * kVerticesPerRectangle);

    m_vertexBuffer.reset(rhi()->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::VertexBuffer,
        m_vertices.capacity() * sizeof(Vertex)));
    m_vertexBuffer->create();

    m_dataBuffer.reset(rhi()->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::UniformBuffer,
        sizeof(ColorShader::ShaderData)));
    m_dataBuffer->create();
}

void MaskRenderingProcess::initShaders()
{
    m_shader.bindResources(m_dataBuffer.get());
    m_shader.setVertexInputLayout(
        /*stride*/ sizeof(Vertex),
        /*offset*/ 0,
        QRhiVertexInputAttribute::Float2);

    m_shader.bindToPipeline(pipeline());
}

void MaskRenderingProcess::uploadData(QRhiResourceUpdateBatch* updateBatch)
{
    if (!m_hasChanges)
        return;

    // Ensuring that the buffer can fit the data.
    if (m_vertices.capacity() * sizeof(Vertex) > (size_t)m_vertexBuffer->size())
    {
        m_vertexBuffer->setSize(m_vertices.capacity() * sizeof(Vertex));
        m_vertexBuffer->create();
    }

    updateBatch->updateDynamicBuffer(
        m_vertexBuffer.get(), /*offset*/ 0, m_vertices.size() * sizeof(Vertex), m_vertices.data());

    QMatrix4x4 matrix = viewProjection();
    if (rhi()->isYUpInFramebuffer())
    {
        matrix.scale(1, -1);
        matrix.translate(0, -1);
    }
    m_shaderData.matrix->set(matrix);
    m_shaderData.color->set(0.0, 1.0, 0.0, 1.0);
    updateBatch->updateDynamicBuffer(
        m_dataBuffer.get(), /*offset*/ 0, sizeof(m_shaderData), &m_shaderData);

    m_hasChanges = false;
}

void MaskRenderingProcess::drawImpl(QRhiCommandBuffer* commandBuffer)
{
    const QRhiCommandBuffer::VertexInput input(m_vertexBuffer.get(), /*offset*/ 0);
    commandBuffer->setVertexInput(0, 1, &input);
    commandBuffer->draw(m_vertices.size());
}

void MaskRenderingProcess::resultReady(QRhiResourceUpdateBatch*)
{
}

void MaskRenderingProcess::draw()
{
    drawOffscreen();

    if (isDebugMode)
        capturedImage().save("mask.png");
}

void MaskRenderingProcess::addRectangle(const QRectF& rect)
{
    ::nx::vms::common::pixelation::addRectangle(m_vertices, rect);
    m_hasChanges = true;
}

void MaskRenderingProcess::clear()
{
    m_vertices.clear();
    m_hasChanges = true;
}

class PixelationProcess: public RenderingProcess
{
public:
    PixelationProcess(QRhi* rhi, QRhiTexture* source, QRhiTexture* mask);
    void setIntensity(float value);
    void draw(QRhiResourceUpdateBatch* uploadBatch = nullptr);

protected:
    virtual void initBuffers() override;
    virtual void initShaders() override;
    virtual void uploadData(QRhiResourceUpdateBatch* updateBatch) override;
    virtual void drawImpl(QRhiCommandBuffer* commandBuffer) override;
    virtual void resultReady(QRhiResourceUpdateBatch* updateBatch) override;

private:
    std::vector<Vertex> m_vertices;
    std::unique_ptr<QRhiBuffer> m_vertexBuffer;
    bool m_isVertexBufferChanged = true;

    PixelationShader m_shader;
    PixelationShader::ShaderData m_shaderData;
    std::unique_ptr<QRhiBuffer> m_dataBuffer;

    QRhiTexture* m_sourceTexture = nullptr;
    std::unique_ptr<QRhiSampler> m_sourceSampler;
    QRhiTexture* m_maskTexture = nullptr;
    std::unique_ptr<QRhiSampler> m_maskSampler;

    float m_intensity = 1.0;
};

PixelationProcess::PixelationProcess(QRhi* rhi, QRhiTexture* source, QRhiTexture* mask):
    RenderingProcess(rhi),
    m_sourceTexture(source),
    m_maskTexture(mask)
{
}

void PixelationProcess::setIntensity(float value)
{
    m_intensity = value;
}

void PixelationProcess::initBuffers()
{
    m_vertexBuffer.reset(rhi()->newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::VertexBuffer,
        m_vertices.size() * sizeof(Vertex)));
    m_vertexBuffer->create();

    m_dataBuffer.reset(rhi()->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::UniformBuffer,
        sizeof(PixelationShader::ShaderData)));
    m_dataBuffer->create();

    m_sourceSampler.reset(rhi()->newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge));

    m_sourceSampler->create();

    m_maskSampler.reset(rhi()->newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge));

    m_maskSampler->create();

    addRectangle(m_vertices, {0, 0, 1, 1});
}

void PixelationProcess::initShaders()
{
    m_shader.bindResources(
        m_dataBuffer.get(),
        m_sourceTexture, m_sourceSampler.get(),
        m_maskTexture, m_maskSampler.get());

    m_shader.setupVertexInputLayout(
        /*stride*/ sizeof(Vertex),
        /*offset*/ 0,
        QRhiVertexInputAttribute::Float2);

    m_shader.bindToPipeline(pipeline());
}

void PixelationProcess::uploadData(QRhiResourceUpdateBatch* updateBatch)
{
    QMatrix4x4 matrix = viewProjection();

    // Flip vertically to prevent the result texture from being flipped due to the frame buffer
    // coordinate system.
    if (rhi()->isYUpInFramebuffer())
    {
        matrix.scale(1, -1);
        matrix.translate(0, -1);
    }

    m_shaderData.matrix->set(matrix);
    updateBatch->updateDynamicBuffer(
        m_dataBuffer.get(), /*offset*/ 0, sizeof(m_shaderData), &m_shaderData);

    if (m_isVertexBufferChanged)
    {
        updateBatch->uploadStaticBuffer(
            m_vertexBuffer.get(),
            /*offset*/ 0,
            m_vertices.size() * sizeof(Vertex),
            m_vertices.data());

        m_isVertexBufferChanged = false;
    }
}

void PixelationProcess::drawImpl(QRhiCommandBuffer* commandBuffer)
{
    const QRhiCommandBuffer::VertexInput input(m_vertexBuffer.get(), /*offset*/ 0);
    commandBuffer->setVertexInput(0, 1, &input);
    commandBuffer->draw(m_vertices.size());
}

void PixelationProcess::resultReady(QRhiResourceUpdateBatch* updateBatch)
{
    // Using the result as the source texture in the next iteration.
    updateBatch->copyTexture(m_sourceTexture, targetTexture());
}

void PixelationProcess::draw(QRhiResourceUpdateBatch* uploadBatch)
{
    constexpr int kMaxMaskSize = 2048;
    constexpr int kIterations = 4;

    for (int i = 0; i < kIterations; ++i)
    {
        const float step = (kIterations - i - 1) * m_intensity * 1.2;
        const QVector2D textureOffset(
            1.0 / kMaxMaskSize * step,
            1.0 / kMaxMaskSize * step);

        m_shaderData.textureOffset = textureOffset;

        m_shaderData.isHorizontalPass = true;
        drawOffscreen(i == 0 ? uploadBatch : nullptr);

        m_shaderData.isHorizontalPass = false;
        drawOffscreen();
    }

    if (isDebugMode)
        capturedImage().save("pixelation.png");
}

} // namespace

struct Pixelation::Private
{
    static constexpr int kMaskSizeFactor = 4;

    Private();

    std::unique_ptr<QRhi> rhi;
    std::unique_ptr<MaskRenderingProcess> mask;
    std::unique_ptr<PixelationProcess> pixelation;
    std::unique_ptr<QRhiTexture> sourceTexture;
    QPointer<QThread> rhiThread = nullptr;

    void initRhi();
    QImage pixelate(const QImage& source, const QVector<QRectF>& rectangles, double intensity);
};

Pixelation::Private::Private()
{
    initRhi();

    const QSize initialSize = {1, 1};
    mask = std::make_unique<MaskRenderingProcess>(rhi.get());
    mask->init(initialSize);

    sourceTexture.reset(rhi->newTexture(QRhiTexture::RGBA8, initialSize, /*sampleCount*/ 1));

    pixelation = std::make_unique<PixelationProcess>(
        rhi.get(), sourceTexture.get(), mask->targetTexture());

    pixelation->init(initialSize);
}

void Pixelation::Private::initRhi()
{
#if defined(__arm__)
    QRhiNullInitParams params;
    rhi.reset(QRhi::create(QRhi::Null, &params));
#else
    QRhiGles2InitParams params;
    params.format = QSurfaceFormat::defaultFormat();
    params.fallbackSurface = QRhiGles2InitParams::newFallbackSurface();
    rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
#endif

    rhiThread = QThread::currentThread();
}

QImage Pixelation::Private::pixelate(
    const QImage& source,
    const QVector<QRectF>& rectangles,
    double intensity)
{
    if (!NX_ASSERT(QThread::currentThread() == rhiThread))
        return {};

    mask->setSize(source.size() / kMaskSizeFactor);
    pixelation->setSize(source.size());
    pixelation->setCaptureImage(true);

    mask->clear();
    for (const auto& rectangle: rectangles)
        mask->addRectangle(rectangle);

    mask->draw();

    const QImage::Format originalFormat = source.format();
    QImage image = source.convertToFormat(QImage::Format_RGBA8888);
    if (rhi->isYUpInFramebuffer())
        image = source.mirrored();

    if (image.size() != sourceTexture->pixelSize())
    {
        sourceTexture->setPixelSize(image.size());
        sourceTexture->create();
    }

    // Upload the source texture during drawing.
    QRhiResourceUpdateBatch* uploadBatch = rhi->nextResourceUpdateBatch();
    uploadBatch->uploadTexture(sourceTexture.get(), image);

    pixelation->setIntensity(intensity);
    pixelation->draw(uploadBatch);

    return pixelation->capturedImage().convertToFormat(originalFormat);
}

Pixelation::Pixelation():
    d(new Pixelation::Private{})
{
}

Pixelation::~Pixelation()
{
    NX_ASSERT(QThread::currentThread() == d->rhiThread);
}

QImage Pixelation::pixelate(
    const QImage& source,
    const QVector<QRectF>& rectangles,
    double intensity)
{
    NX_ASSERT(QThread::currentThread() == d->rhiThread);

    return d->pixelate(
        source,
        rectangles,
        intensity);
}

QThread* Pixelation::thread() const
{
    return d->rhiThread;
}

namespace {

QShader load(const QString &name)
{
    QFile file(name);
    const bool isOpened = file.open(QIODevice::ReadOnly);
    return NX_ASSERT(isOpened, name) ? QShader::fromSerialized(file.readAll()) : QShader{};
};

std::unique_ptr<QRhiShaderResourceBindings> createBindings(
    QRhiBuffer* dataBuffer,
    QRhiTexture* sourceTexture,
    QRhiSampler* sourceSampler,
    QRhiTexture* maskTexture,
    QRhiSampler* maskSampler)
{
    std::unique_ptr<QRhiShaderResourceBindings> shaderResourceBindings(
        dataBuffer->rhi()->newShaderResourceBindings());

    shaderResourceBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            /*binding*/ 0,
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            dataBuffer),

        QRhiShaderResourceBinding::sampledTexture(
            /*binding*/ 1,
            QRhiShaderResourceBinding::FragmentStage,
            sourceTexture,
            sourceSampler),

        QRhiShaderResourceBinding::sampledTexture(
            /*binding*/ 2,
            QRhiShaderResourceBinding::FragmentStage,
            maskTexture,
            maskSampler),
    });
    shaderResourceBindings->create();
    return shaderResourceBindings;
}

} // namespace

class BlurTarget
{
public:
    void init(QRhi* rhi, const QSize& size)
    {
        m_rhi = rhi;

        m_texture.reset(m_rhi->newTexture(
            QRhiTexture::RGBA8,
            size,
            /*sampleCount*/ 1,
            QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));

        m_texture->create();

        m_renderTarget.reset(m_rhi->newTextureRenderTarget({m_texture.get()}));
        m_renderPass.reset(m_renderTarget->newCompatibleRenderPassDescriptor());
        m_renderTarget->setRenderPassDescriptor(m_renderPass.get());
        m_renderTarget->create();

        m_sampler.reset(m_rhi->newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge));
        m_sampler->create();
    }

    QRhi* m_rhi = nullptr;
    std::shared_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiTextureRenderTarget> m_renderTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_renderPass;
    std::unique_ptr<QRhiSampler> m_sampler;
};

class BlurProcess
{
public:
    BlurProcess() {}

    void init(QRhi* rhi, QSize size, QRhiTexture* maskTexture);
    void initBuffers();
    void initPipeline(std::unique_ptr<QRhiGraphicsPipeline>& pipeline);

    void uploadData(QRhiResourceUpdateBatch* updateBatch);
    void render();

    void renderPass(
        QRhiCommandBuffer* commands,
        QRhiTextureRenderTarget* target,
        QRhiShaderResourceBindings* bindings);

    void setIntensity(float value) { m_intensity = value; }

    BlurTarget* blurTarget() { return &m_blurTargetA; }

private:
    QRhi* m_rhi = nullptr;

    QMatrix4x4 m_viewProjection;

    BlurTarget m_blurTargetA;
    BlurTarget m_blurTargetB;

    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;

    std::unique_ptr<QRhiShaderResourceBindings> m_atobBindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_btoaBindings;

    std::vector<Vertex> m_vertices;
    std::unique_ptr<QRhiBuffer> m_vertexBuffer;
    bool m_isVertexBufferChanged = true;

    PixelationShader::ShaderData m_shaderData;
    std::unique_ptr<QRhiBuffer> m_dataBuffer;

    QRhiTexture* m_maskTexture = nullptr;
    std::unique_ptr<QRhiSampler> m_maskSampler;

    float m_intensity = 1.0;

    std::shared_ptr<QRhiTexture> m_tmpTexture;
};

void BlurProcess::init(QRhi* rhi, QSize size, QRhiTexture* maskTexture)
{
    m_rhi = rhi;
    m_blurTargetA.init(m_rhi, size);
    m_blurTargetB.init(m_rhi, size);

    m_tmpTexture.reset(m_rhi->newTexture(
        QRhiTexture::RGBA8,
        size,
        /*sampleCount*/ 1,
        {}));
    m_tmpTexture->create();

    m_viewProjection = m_rhi->clipSpaceCorrMatrix();
    m_viewProjection.ortho(QRectF{0, 0, 1, 1});

    // Flip vertically to prevent the result texture from being flipped due to the frame buffer
    // coordinate system.
    if (m_rhi->isYUpInFramebuffer())
    {
        m_viewProjection.scale(1, -1);
        m_viewProjection.translate(0, -1);
    }

    initBuffers();

    m_maskTexture = maskTexture;

    m_atobBindings = createBindings(
        m_dataBuffer.get(),
        m_blurTargetA.m_texture.get(), m_blurTargetA.m_sampler.get(),
        m_maskTexture, m_maskSampler.get());

    m_btoaBindings = createBindings(
        m_dataBuffer.get(),
        m_blurTargetB.m_texture.get(), m_blurTargetB.m_sampler.get(),
        m_maskTexture, m_maskSampler.get());

    initPipeline(m_pipeline);
}

void BlurProcess::initBuffers()
{
    m_vertexBuffer.reset(m_rhi->newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::VertexBuffer,
        m_vertices.size() * sizeof(Vertex)));
    m_vertexBuffer->create();

    m_dataBuffer.reset(m_rhi->newBuffer(
        QRhiBuffer::Dynamic,
        QRhiBuffer::UniformBuffer,
        sizeof(PixelationShader::ShaderData)));
    m_dataBuffer->create();

    m_maskSampler.reset(m_rhi->newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge));

    m_maskSampler->create();

    addRectangle(m_vertices, {0, 0, 1, 1});
}

void BlurProcess::initPipeline(std::unique_ptr<QRhiGraphicsPipeline>& pipeline)
{
    pipeline.reset(m_rhi->newGraphicsPipeline());

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        QRhiVertexInputBinding{sizeof(Vertex)}
    });
    inputLayout.setAttributes({
        {/*binding*/ 0, /*location*/ 0, QRhiVertexInputAttribute::Float2, /* vertexOffset */ 0}
    });

    pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, load(":/src/nx/vms/common/pixelation/shaders/pixelation.vert.qsb")},
        {QRhiShaderStage::Fragment, load(":/src/nx/vms/common/pixelation/shaders/pixelation.frag.qsb")}
    });

    pipeline->setVertexInputLayout(inputLayout);
    pipeline->setShaderResourceBindings(m_atobBindings.get());
    pipeline->setRenderPassDescriptor(m_blurTargetB.m_renderPass.get());
    pipeline->create();
}

void BlurProcess::uploadData(QRhiResourceUpdateBatch* updateBatch)
{
    m_shaderData.matrix->set(m_viewProjection);
    updateBatch->updateDynamicBuffer(
        m_dataBuffer.get(), /*offset*/ 0, sizeof(m_shaderData), &m_shaderData);

    if (m_isVertexBufferChanged)
    {
        updateBatch->uploadStaticBuffer(
            m_vertexBuffer.get(),
            /*offset*/ 0,
            m_vertices.size() * sizeof(Vertex),
            m_vertices.data());

        m_isVertexBufferChanged = false;
    }
}

void BlurProcess::renderPass(
    QRhiCommandBuffer* commands,
    QRhiTextureRenderTarget* target,
    QRhiShaderResourceBindings* bindings)
{
    QRhiResourceUpdateBatch* updateBatch = m_rhi->nextResourceUpdateBatch();

    uploadData(updateBatch);

    commands->beginPass(target, Qt::transparent, {}, updateBatch);

    commands->setGraphicsPipeline(m_pipeline.get());

    const QSize size = target->pixelSize();
    commands->setViewport(QRhiViewport(0.0f, 0.0f, size.width(), size.height()));
    commands->setShaderResources(bindings);

    const QRhiCommandBuffer::VertexInput input(m_vertexBuffer.get(), /*offset*/ 0);
    commands->setVertexInput(0, 1, &input);
    commands->draw(m_vertices.size());

    commands->endPass();
}

void BlurProcess::render()
{
    constexpr int kMaxMaskSize = 2048;
    constexpr int kIterations = 4;

    const auto getOffset =
        [this](int i, bool isHorizontal)
        {
            const int adjust = isHorizontal ? 1 : 0;
            const float step = ((kIterations - 1 - i) * 2 + adjust) * m_intensity * 1.2;
            return QVector2D(
                1.0 / kMaxMaskSize * step,
                1.0 / kMaxMaskSize * step);
        };

    QRhiCommandBuffer* commands = nullptr;

    for (int i = 0; i < kIterations; ++i)
    {
        // A -> B
        m_rhi->beginOffscreenFrame(&commands);
        m_shaderData.textureOffset = getOffset(i, /* isHorizontal */ true);
        m_shaderData.isHorizontalPass = true;
        renderPass(commands, m_blurTargetB.m_renderTarget.get(), m_atobBindings.get());

        m_rhi->endOffscreenFrame();

        // B -> A
        m_rhi->beginOffscreenFrame(&commands);
        m_shaderData.textureOffset = getOffset(i, /* isHorizontal */ false);
        m_shaderData.isHorizontalPass = false;
        renderPass(commands, m_blurTargetA.m_renderTarget.get(), m_btoaBindings.get());

        m_rhi->endOffscreenFrame();
    }
}

struct BlurBuffer::Private
{
    static constexpr int kMaskSizeFactor = 4;

    Private(QRhi* rhi, const QSize& size);

    QRhi* rhi;
    QSize size;
    std::unique_ptr<MaskRenderingProcess> mask;
    QVector<QRectF> maskRectangles;
    std::unique_ptr<BlurProcess> blurProcess;
};

BlurBuffer::Private::Private(QRhi* rhi, const QSize& size): rhi(rhi), size(size)
{
    const auto maskSize = size / kMaskSizeFactor;
    mask = std::make_unique<MaskRenderingProcess>(rhi);
    mask->init(maskSize);
    mask->setCaptureImage(false);

    blurProcess = std::make_unique<BlurProcess>();
    blurProcess->init(rhi, size, mask->targetTexture());
}

BlurBuffer::BlurBuffer(QRhi* rhi, const QSize& size): d(new BlurBuffer::Private(rhi, size))
{
}

BlurBuffer::~BlurBuffer()
{
}

void BlurBuffer::blur(
    const QVector<QRectF>& rectangles,
    double intensity)
{
    if (rectangles != d->maskRectangles)
    {
        d->mask->clear();
        for (const auto& rectangle: rectangles)
            d->mask->addRectangle(rectangle);
        d->mask->draw();
        d->maskRectangles = rectangles;
    }

    d->blurProcess->setIntensity(intensity);
    d->blurProcess->render();
}

QRhiRenderPassDescriptor* BlurBuffer::renderPassDescriptor() const
{
    return d->blurProcess->blurTarget()->m_renderPass.get();
}

QRhiRenderTarget* BlurBuffer::renderTarget() const
{
    return d->blurProcess->blurTarget()->m_renderTarget.get();
}

QSize BlurBuffer::size() const
{
    return d->size;
}

std::shared_ptr<QRhiTexture> BlurBuffer::texture() const
{
    return d->blurProcess->blurTarget()->m_texture;
}

} // namespace nx::vms::common::pixelation
