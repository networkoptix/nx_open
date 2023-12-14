// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_video_renderer.h"

#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
    #include <rhi/qrhi.h>
#else
    #include <QtGui/private/qrhi_p.h>
#endif

#include <QtCore/QFile>

#include <nx/utils/log/assert.h>
#include <utils/color_space/image_correction.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kYPlaneIndex = 0;
static constexpr int kAPlaneIndex = 3;

static const QRhiShaderResourceBinding::StageFlags kCommonVisibility =
    QRhiShaderResourceBinding::VertexStage
    | QRhiShaderResourceBinding::FragmentStage;

static QShader getShader(const QString &name)
{
    QFile f(name);
    if (NX_ASSERT(f.open(QIODevice::ReadOnly), "Shader not loaded: %1", name))
        return QShader::fromSerialized(f.readAll());

    return QShader();
}

static QString keyToString(MediaOutputShaderProgram::Key programKey)
{
    QStringList result;

    // This should match with shader generation code in CMakeLists.txt

    switch (programKey.format)
    {
        case MediaOutputShaderProgram::Format::rgb: result << "rgb"; break;
        case MediaOutputShaderProgram::Format::yv12: result << "yv12"; break;
        case MediaOutputShaderProgram::Format::nv12: result << "nv12"; break;
        case MediaOutputShaderProgram::Format::yva12: result << "yva12"; break;
    }

    switch (programKey.imageCorrection)
    {
        case MediaOutputShaderProgram::YuvImageCorrection::none: result << "no_gamma"; break;
        case MediaOutputShaderProgram::YuvImageCorrection::gamma: result << "yuv_gamma"; break;
    }

    if (programKey.dewarping)
    {
        switch (programKey.cameraProjection)
        {
            case nx::vms::api::dewarping::CameraProjection::equidistant: result << "equidistant"; break;
            case nx::vms::api::dewarping::CameraProjection::stereographic: result << "stereographic"; break;
            case nx::vms::api::dewarping::CameraProjection::equisolid: result << "equisolid"; break;
            case nx::vms::api::dewarping::CameraProjection::equirectangular360: result << "equirectangular360"; break;
        }

        switch (programKey.viewProjection)
        {
            case nx::vms::api::dewarping::ViewProjection::rectilinear: result << "rectilinear"; break;
            case nx::vms::api::dewarping::ViewProjection::equirectangular: result << "equirectangular"; break;
        }
    }

    return result.join("_");
}

static int planeCountFromFormat(MediaOutputShaderProgram::Format format)
{
    switch (format)
    {
        case MediaOutputShaderProgram::Format::rgb: return 1;
        case MediaOutputShaderProgram::Format::nv12: return 2;
        case MediaOutputShaderProgram::Format::yv12: return 3;
        case MediaOutputShaderProgram::Format::yva12: return 4;
        default: return 0;
    }
};

QRhiTexture::Format textureFormat(MediaOutputShaderProgram::Format format, int plane)
{
    switch (format)
    {
        case MediaOutputShaderProgram::Format::rgb:
            return QRhiTexture::RGBA8;

        case MediaOutputShaderProgram::Format::nv12:
            return plane == kYPlaneIndex
                ? QRhiTexture::R8
                : QRhiTexture::RG8;

        case MediaOutputShaderProgram::Format::yv12:
        case MediaOutputShaderProgram::Format::yva12:
            return QRhiTexture::R8;

        default:
            return QRhiTexture::UnknownFormat;
    }
};

QSize textureSize(AVPixelFormat format, int plane, const QSize& size)
{
    switch (format)
    {
        case AV_PIX_FMT_YUV444P:
            return size;

        case AV_PIX_FMT_YUV422P:
            return plane == kYPlaneIndex
                ? size
                : QSize(size.width() / 2, size.height());

        case AV_PIX_FMT_NV12:
            return plane == kYPlaneIndex
                ? size
                : QSize(size.width() / 2, size.height() / 2);

        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVA420P:
            return (plane == kYPlaneIndex || plane == kAPlaneIndex)
                ? size
                : QSize(size.width() / 2, size.height() / 2);

        default:
            return size;
    }
}

int textureLineSize(MediaOutputShaderProgram::Format format, int plane, const int* lineSizes)
{
    if (format == MediaOutputShaderProgram::Format::nv12 && plane != 0)
        return lineSizes[plane] / 2;
    return lineSizes[plane];
}

/** Helper class for building data in std140-qualified uniform block. */
class Std140BufferBuilder
{
public:
    void append(const QMatrix4x4 mat)
    {
        ensureAligned();
        m_data.append((const char *)mat.constData(), 4 * 4 * sizeof(float));
    }

    void append(const QMatrix3x3 mat)
    {
        ensureAligned();

        // mat3 is stored as 3 vec4.
        for (int i = 0; i < 3; ++i)
        {
            m_data.append((const char *)(mat.constData() + 3 * i), 3 * sizeof(float));
            append(0.0f);
        }
    }

    void append(float value)
    {
        m_data.append((const char *)&value, sizeof(float));
    }

    const void* data() const { return m_data.constData(); }
    quint32 size() const { return m_data.size(); }
    void reserve(qsizetype asize) { m_data.reserve(asize); }

private:
    void ensureAligned()
    {
        const qsizetype kAlignmentMask = 0xF; // Align to 16 bytes (4 floats).
        while (m_data.size() & kAlignmentMask)
            append(0.0f);
    }

    QByteArray m_data;
};

} // namespace

struct RhiVideoRenderer::Private
{
    Private() {}

    RhiVideoRenderer::Data data;

    QRhi* rhi;

    std::unique_ptr<QRhiBuffer> vbuf;
    std::unique_ptr<QRhiBuffer> ubuf;
    std::unique_ptr<QRhiSampler> sampler;
    std::unique_ptr<QRhiShaderResourceBindings> srb;
    std::unique_ptr<QRhiGraphicsPipeline> ps;

    std::vector<std::unique_ptr<QRhiTexture>> textures;

    intptr_t lastUploadedFrame = 0;
};

RhiVideoRenderer::RhiVideoRenderer():
    d(new Private())
{
}

RhiVideoRenderer::~RhiVideoRenderer()
{
}

void RhiVideoRenderer::init(
    const RhiVideoRenderer::Data& data,
    QRhi* rhi,
    QRhiRenderPassDescriptor* desc)
{
    d->data = data;

    const quint32 vsize = d->data.data.verts.size() * 2 * sizeof(float);
    const quint32 tsize = d->data.data.texCoords.size() * 2 * sizeof(float);
    const quint32 vbufSize = vsize + tsize;
    d->vbuf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, vbufSize));
    d->vbuf->create();

    d->textures.clear();
    const int minTexSize = rhi->resourceLimit(QRhi::TextureSizeMin);
    const int planeCount = planeCountFromFormat(d->data.data.key.format);
    for (int i = 0; i < planeCount; ++i)
    {
        std::unique_ptr<QRhiTexture> texture;
        texture.reset(rhi->newTexture(
            textureFormat(d->data.data.key.format, i),
            QSize(minTexSize, minTexSize),
            /*sampleCount*/ 1));
        texture->create();

        d->textures.push_back(std::move(texture));
    }

    const bool gammaCorrection =
        d->data.data.key.imageCorrection == MediaOutputShaderProgram::YuvImageCorrection::gamma;

    int ubufSize =
        4 * 4 * sizeof(float)    //< uModelViewProjectionMatrix
        + 4 * 3 * sizeof(float)  //< uTexCoordMatrix
        + 4 * 4 * sizeof(float); //< colorMatrix

    if (gammaCorrection)
        ubufSize += 3 * sizeof(float);

    if (d->data.data.key.dewarping)
    {
        if (gammaCorrection)
            ubufSize += sizeof(float); //< padding)

        ubufSize +=
            4 * 3 * sizeof(float) //< texCoordFragMatrix
            + 4 * 3 * sizeof(float); //< sphereRotationMatrix or unprojectionMatrix

        if (d->data.data.key.cameraProjection
            == MediaOutputShaderProgram::CameraProjection::equirectangular360)
        {
            ubufSize += 4 * 3 * sizeof(float); //< rotationCorrectionMatrix
        }
    }

    d->ubuf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, ubufSize));
    NX_ASSERT(d->ubuf->create());

    d->sampler.reset(rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    d->sampler->create();

    d->srb.reset(rhi->newShaderResourceBindings());

    std::vector<QRhiShaderResourceBinding> bindings;

    bindings.push_back(QRhiShaderResourceBinding::uniformBuffer(
        0, kCommonVisibility, d->ubuf.get()));

    for (int i = 0; i < planeCount; ++i)
    {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(
            1 + i,
            QRhiShaderResourceBinding::FragmentStage,
            d->textures[i].get(),
            d->sampler.get()));
    }

    d->srb->setBindings(bindings.begin(), bindings.end());
    d->srb->create();

    d->ps.reset(rhi->newGraphicsPipeline());
    d->ps->setDepthTest(false);
    d->ps->setDepthWrite(false);
    d->ps->setStencilTest(false);
    d->ps->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    d->ps->setShaderStages({
        { QRhiShaderStage::Vertex, getShader(":/shaders/rhi/media_output/main.vert.qsb") },
        { QRhiShaderStage::Fragment, getShader(
            QString(":/shaders/rhi/media_output/main_%1.frag.qsb").arg(keyToString(d->data.data.key))) }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 2 * sizeof(float) },
        { 2 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 1, 1, QRhiVertexInputAttribute::Float2, 0 }
    });
    d->ps->setVertexInputLayout(inputLayout);
    d->ps->setShaderResourceBindings(d->srb.get());

    d->ps->setRenderPassDescriptor(desc);
    d->ps->create();
}

void RhiVideoRenderer::createBindings()
{
    std::unique_ptr<QRhiShaderResourceBindings> prevSrb = std::move(d->srb);

    d->srb.reset(d->ps->rhi()->newShaderResourceBindings());

    std::vector<QRhiShaderResourceBinding> bindings;

    bindings.push_back(QRhiShaderResourceBinding::uniformBuffer(
        0, kCommonVisibility, d->ubuf.get()));

    for (int i = 0; i < (int) d->textures.size(); ++i)
    {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(
            1 + i,
            QRhiShaderResourceBinding::FragmentStage,
            d->textures[i].get(),
            d->sampler.get()));
    }

    d->srb->setBindings(bindings.begin(), bindings.end());
    NX_ASSERT(d->srb->create());

    NX_ASSERT(d->srb->isLayoutCompatible(prevSrb.get()));
}

void RhiVideoRenderer::ensureTextures()
{
    const size_t planeCount = planeCountFromFormat(d->data.data.key.format);

    if (planeCount == d->textures.size() && d->textures.front()->pixelSize() == d->data.frame->size())
        return;

    d->textures.clear();

    for (size_t i = 0; i < planeCount; ++i)
    {
        std::unique_ptr<QRhiTexture> texture;
        texture.reset(d->rhi->newTexture(
            textureFormat(d->data.data.key.format, i),
            textureSize((AVPixelFormat) d->data.frame->format, i, d->data.frame->size()),
            /*sampleCount*/ 1));
        texture->create();

        d->textures.push_back(std::move(texture));
    }

    createBindings();
    d->lastUploadedFrame = 0;
}

void RhiVideoRenderer::prepare(
    const RhiVideoRenderer::Data& data,
    const QMatrix4x4& mvp,
    QRhi* rhi,
    QRhiRenderPassDescriptor* rp,
    QRhiResourceUpdateBatch* rub)
{
    d->rhi = rhi;

    if (!d->ps || data.data.key != d->data.data.key)
        init(data, rhi, rp);
    else
        d->data = data;

    // Allocate textures.
    ensureTextures();

    // Upload textures.
    const uint8_t* const* planes = d->data.frame->data;
    const int* const lineSizes = d->data.frame->linesize;

    const int planeCount = planeCountFromFormat(d->data.data.key.format);

    // TODO: #ikulaychuk add 1x1 black texture for null frame.

    if ((intptr_t) d->data.frame.get() != d->lastUploadedFrame)
    {
        for(int i = 0; i < planeCount; ++i)
        {
            const auto size = textureSize(
                (AVPixelFormat) d->data.frame->format, i, d->data.frame->size());
            const auto stride = textureLineSize(d->data.data.key.format, i, lineSizes);

            QRhiTextureSubresourceUploadDescription desc(planes[i], size.height() * stride);
            desc.setDataStride(stride);
            rub->uploadTexture(d->textures[i].get(), QRhiTextureUploadEntry(0, 0, desc));
        }
        d->lastUploadedFrame = (intptr_t) d->data.frame.get();
    }

    // Upload buffers.
    Std140BufferBuilder buffer;
    buffer.append(mvp);
    buffer.append(data.data.uTexCoordMatrix);
    buffer.append(data.data.colorMatrix);

    if (d->data.data.key.imageCorrection == MediaOutputShaderProgram::YuvImageCorrection::gamma)
    {
        buffer.append(data.data.gamma);
        buffer.append(data.data.luminocityShift);
        buffer.append(data.data.luminocityFactor);
    }

    if (d->data.data.key.dewarping)
    {
        buffer.append(data.data.texCoordFragMatrix);
        buffer.append(data.data.sphereRotationOrUnprojectionMatrix);

        if (d->data.data.key.cameraProjection
            == MediaOutputShaderProgram::CameraProjection::equirectangular360)
        {
            buffer.append(data.data.rotationCorrectionMatrix);
        }
    }

    NX_ASSERT(buffer.size() == d->ubuf->size(), "%1 shoud be %2", buffer.size(), d->ubuf->size());
    rub->updateDynamicBuffer(d->ubuf.get(), 0, buffer.size(), buffer.data());

    const quint32 vsize = data.data.verts.size() * 2 * sizeof(float);
    const quint32 tsize = data.data.texCoords.size() * 2 * sizeof(float);
    rub->updateDynamicBuffer(d->vbuf.get(), 0, vsize, data.data.verts.constData());
    rub->updateDynamicBuffer(d->vbuf.get(), vsize, tsize, data.data.texCoords.constData());
}

void RhiVideoRenderer::render(
    QRhiCommandBuffer* cb,
    QSize viewportSize)
{
    cb->setGraphicsPipeline(d->ps.get());
    cb->setViewport(QRhiViewport(0, 0, viewportSize.width(), viewportSize.height()));
    cb->setShaderResources(d->srb.get());
    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
        { d->vbuf.get(), 0 },
        { d->vbuf.get(), quint32(d->data.data.verts.size() * 2 * sizeof(float)) }
    };
    cb->setVertexInput(0, 2, vbufBindings);
    cb->draw(d->data.data.verts.size());
}

} // namespace nx::vms::client::desktop
