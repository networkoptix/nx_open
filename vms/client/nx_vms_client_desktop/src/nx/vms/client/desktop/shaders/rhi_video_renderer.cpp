// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_video_renderer.h"

#include <QtCore/QFile>
#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>

#include <nx/utils/log/log.h>
#include <utils/color_space/image_correction.h>

namespace nx::vms::client::desktop {

class RhiCleanupHelper
{
public:
    using KeyType = void*;
    using Callback = std::function<void(QRhi*)>;

public:
    static void set(QRhi* rhi, KeyType key, Callback callback);
    static void remove(QRhi* rhi, KeyType key);

private:
    // Per RHI data.
    struct Data;

    static Data* dataFor(QRhi* rhi, bool alloc = true);
};

struct RhiCleanupHelper::Data
{
    QRhi* const rhi;
    QHash<KeyType, Callback> callbacks;

    void add(KeyType key, Callback callback)
    {
        callbacks[key] = callback;
    }

    void remove(KeyType key)
    {
        callbacks.remove(key);
    }

    Data(QRhi* rhi): rhi(rhi) {}

    ~Data() //< RHI is destroyed.
    {
        const auto allCallbacks = callbacks.values();
        for (auto& callback: allCallbacks)
            callback(rhi);
    }
};

void RhiCleanupHelper::set(QRhi* rhi, KeyType key, RhiCleanupHelper::Callback callback)
{
    if (rhi)
        dataFor(rhi)->add(key, callback);
}

void RhiCleanupHelper::remove(QRhi* rhi, KeyType key)
{
    if (auto data = dataFor(rhi, false))
        data->remove(key);
}

RhiCleanupHelper::Data* RhiCleanupHelper::dataFor(QRhi* rhi, bool alloc)
{
    static QHash<QRhi*, Data*> cleanupData;
    auto it = cleanupData.constFind(rhi);
    if (it != cleanupData.constEnd())
        return *it;

    if (!alloc)
        return nullptr;

    auto data = new Data(rhi);
    rhi->addCleanupCallback(
        [data](QRhi* rhi)
        {
            cleanupData.remove(rhi);
            delete data;
        });

    cleanupData.insert(rhi, data);
    return data;
}

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

static size_t planeCountFromFormat(MediaOutputShaderProgram::Format format)
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

class VideoTextures: public QVideoFrameTextures
{
public:
    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane > all.size())
            return nullptr;
        return all[plane].get();
    }

    static constexpr size_t maxCount = 4;

    std::array<std::unique_ptr<QRhiTexture>, maxCount> all;
};

} // namespace

struct RhiVideoRenderer::Private
{
    Private() {}

    void cleanup()
    {
        ps.reset();
        vbuf.reset();
        ubuf.reset();
        sampler.reset();
        srb.reset();
        textures.reset();
        rhi = nullptr;
    }

    RhiVideoRenderer::Data data;

    QRhi* rhi = nullptr;

    std::unique_ptr<QRhiBuffer> vbuf;
    std::unique_ptr<QRhiBuffer> ubuf;
    std::unique_ptr<QRhiSampler> sampler;
    std::unique_ptr<QRhiShaderResourceBindings> srb;
    std::unique_ptr<QRhiGraphicsPipeline> ps;

    std::unique_ptr<QVideoFrameTextures> textures;
    size_t planeCount = 0;
    QVector2D textureScale;

    intptr_t lastUploadedFrame = 0;
};

RhiVideoRenderer::RhiVideoRenderer():
    d(new Private())
{
}

RhiVideoRenderer::~RhiVideoRenderer()
{
    RhiCleanupHelper::remove(d->rhi, this);
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

    auto textures = std::make_unique<VideoTextures>();
    const int minTexSize = rhi->resourceLimit(QRhi::TextureSizeMin);
    const size_t planeCount = planeCountFromFormat(d->data.data.key.format);
    for (size_t i = 0; i < planeCount; ++i)
    {
        std::unique_ptr<QRhiTexture> texture;
        texture.reset(rhi->newTexture(
            textureFormat(d->data.data.key.format, i),
            QSize(minTexSize, minTexSize),
            /*sampleCount*/ 1));
        texture->create();

        textures->all[i] = std::move(texture);
    }
    d->textures = std::move(textures);
    d->planeCount = planeCount;
    d->lastUploadedFrame = 0;

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

    for (size_t i = 0; i < d->planeCount; ++i)
    {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(
            1 + i,
            QRhiShaderResourceBinding::FragmentStage,
            d->textures->texture(i),
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

    for (size_t i = 0; i < d->planeCount; ++i)
    {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(
            1 + i,
            QRhiShaderResourceBinding::FragmentStage,
            d->textures->texture(i),
            d->sampler.get()));
    }

    d->srb->setBindings(bindings.begin(), bindings.end());
    NX_ASSERT(d->srb->create());

    NX_ASSERT(d->srb->isLayoutCompatible(prevSrb.get()));
}

void RhiVideoRenderer::ensureTextures(const AVFrame* frame)
{
    d->planeCount = MediaOutputShaderProgram::planeCount((AVPixelFormat) frame->format);

    auto textures = dynamic_cast<VideoTextures*>(d->textures.get());
    if (!textures)
    {
        d->textures = std::make_unique<VideoTextures>();
        d->textures.get();
    }

    for (size_t i = d->planeCount; i < VideoTextures::maxCount; ++i)
        textures->all[i].reset();

    bool createNewBindings = false;

    const auto frameFormat = MediaOutputShaderProgram::formatFromPlaneCount(d->planeCount);

    for (size_t i = 0; i < d->planeCount; ++i)
    {
        const auto format = textureFormat(frameFormat, i);
        const auto size = textureSize(
            (AVPixelFormat) frame->format,
            /* plane */ i,
            QSize(frame->width, frame->height));

        auto& texture = textures->all[i];
        if (!texture)
        {
            texture.reset(d->rhi->newTexture(format, size, /*sampleCount*/ 1));
            texture->create();
            createNewBindings = true;
        }
        else if (texture->format() != format || texture->pixelSize() != size)
        {
            texture->setFormat(format);
            texture->setPixelSize(size);
            texture->create();
            createNewBindings = true;
        }
    }

    if (createNewBindings)
        createBindings();

    d->lastUploadedFrame = 0;
}

void RhiVideoRenderer::uploadFrame(const AVFrame* frame, QRhiResourceUpdateBatch* rub)
{
    ensureTextures(frame);

    const uint8_t* const* planes = frame->data;
    const int* const lineSizes = frame->linesize;
    const QSize frameSize(frame->width, frame->height);

    for (size_t i = 0; i < d->planeCount; ++i)
    {
        const auto size = textureSize(
            (AVPixelFormat) frame->format, i, frameSize);
        const auto stride = lineSizes[i];
        QByteArray data((const char*)planes[i], size.height() * stride);
        QRhiTextureSubresourceUploadDescription desc(planes[i], size.height() * stride);
        desc.setDataStride(stride);
        rub->uploadTexture(d->textures->texture(i), QRhiTextureUploadEntry(0, 0, desc));
    }
}

void RhiVideoRenderer::uploadFrame(QVideoFrame* videoFrame, QRhiResourceUpdateBatch* rub)
{
    if (!videoFrame)
        return;

    d->planeCount = videoFrame->planeCount();

    if (auto textures = videoFrame->videoBuffer()->mapTextures(d->rhi))
    {
        d->textures = std::move(textures);
        if (d->textures->texture(0))
        {
            const auto textureSize = d->textures->texture(0)->pixelSize();
            const auto frameSize = videoFrame->size();
            // When coming from a HW decoder, texture size can be aligned to macroblock size,
            // so we need to scale texture coordinates to avoid sampling garbage.
            if (textureSize != frameSize && textureSize.width() > 0 && textureSize.height() > 0)
            {
                d->textureScale = QVector2D(
                    (float) frameSize.width() / textureSize.width(),
                    (float) frameSize.height() / textureSize.height());
            }
        }
        createBindings();
        return;
    }

    AVFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.format = d->data.frame->format;
    frame.width = videoFrame->width();
    frame.height = videoFrame->height();
    if (!videoFrame->map(QVideoFrame::ReadOnly))
    {
        NX_WARNING(this, "Unable to map video frame data");
        d->planeCount = 0;
        return;
    }
    for (size_t i = 0; i < d->planeCount; ++i)
    {
        frame.linesize[i] = videoFrame->bytesPerLine(i);
        frame.data[i] = videoFrame->bits(i);
    }

    uploadFrame(&frame, rub);
}

void RhiVideoRenderer::prepare(
    const RhiVideoRenderer::Data& data,
    const QMatrix4x4& mvp,
    QRhi* rhi,
    QRhiRenderPassDescriptor* rp,
    QRhiResourceUpdateBatch* rub)
{
    if (rhi != d->rhi)
    {
        RhiCleanupHelper::remove(d->rhi, this);
        if (d->rhi)
            d->cleanup();

        RhiCleanupHelper::set(rhi, this, [this](QRhi*){ d->cleanup(); });
        d->rhi = rhi;
    }

    if (!d->ps || data.data.key != d->data.data.key)
        init(data, rhi, rp);
    else
        d->data = data;

    // TODO: #ikulaychuk add 1x1 black texture for null frame.

    if ((intptr_t) d->data.frame.get() != d->lastUploadedFrame)
    {
        d->textureScale = {};
        if (d->data.frame->memoryType() == MemoryType::VideoMemory)
            uploadFrame(d->data.frame->getVideoSurface()->frame(), rub);
        else
            uploadFrame(d->data.frame.get(), rub);

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

    NX_ASSERT(buffer.size() == d->ubuf->size(), "%1 should be %2", buffer.size(), d->ubuf->size());
    rub->updateDynamicBuffer(d->ubuf.get(), 0, buffer.size(), buffer.data());

    const quint32 vsize = data.data.verts.size() * 2 * sizeof(float);
    const quint32 tsize = data.data.texCoords.size() * 2 * sizeof(float);
    rub->updateDynamicBuffer(d->vbuf.get(), 0, vsize, data.data.verts.constData());

    if (d->textureScale.isNull())
    {
        rub->updateDynamicBuffer(d->vbuf.get(), vsize, tsize, data.data.texCoords.constData());
    }
    else
    {
        QList<QVector2D> texCoords;
        texCoords.reserve(data.data.texCoords.size());
        for (const auto& point: data.data.texCoords)
            texCoords << point * d->textureScale;
        rub->updateDynamicBuffer(d->vbuf.get(), vsize, tsize, texCoords.constData());
    }
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
