// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <d3d11_1.h>

#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/private/qhwvideobuffer_p.h>

#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
} // extern "C"

#include "texture_helper.h"
#include "hw_video_api.h"

using namespace Microsoft::WRL;

namespace nx::media {

namespace {

/**
 * Helper class for synchronized transfer of a texture between two D3D devices. FFmpeg and RHI
 * use different D3D devices so we need to copy the texture between them.
 */
class TextureBridge final
{
public:
    /**
     * Copy a texture at position index from device dev into a shared texture and crop texture
     * size to the frame size
     */
    bool copyToSharedTex(
        ID3D11Device* dev,
        ID3D11DeviceContext* ctx,
        const ComPtr<ID3D11Texture2D>& tex,
        UINT index,
        const QSize& frameSize);

    /** Get a copy of the texture on a provided device */
    ComPtr<ID3D11Texture2D> copyFromSharedTex(
        const ComPtr<ID3D11Device1>& dev,
        const ComPtr<ID3D11DeviceContext>& ctx);

private:
    bool ensureDestTex(const ComPtr<ID3D11Device1>& dev);
    bool ensureSrcTex(
        ID3D11Device* dev,
        const ComPtr<ID3D11Texture2D>& tex,
        const QSize& frameSize);
    bool isSrcInitialized(
        const ID3D11Device* dev,
        const ComPtr<ID3D11Texture2D>& tex,
        const QSize& frameSize) const;

    bool recreateSrc(ID3D11Device* dev, const ComPtr<ID3D11Texture2D>& tex, const QSize& frameSize);

    Wrappers::HandleT<Wrappers::HandleTraits::HANDLETraits> m_sharedHandle{};

    const UINT m_srcKey = 0;
    ComPtr<ID3D11Texture2D> m_srcTex;
    ComPtr<IDXGIKeyedMutex> m_srcMutex;

    const UINT m_destKey = 1;
    ComPtr<ID3D11Device1> m_destDevice;
    ComPtr<ID3D11Texture2D> m_destTex;
    ComPtr<IDXGIKeyedMutex> m_destMutex;

    ComPtr<ID3D11Texture2D> m_outputTex;
};

bool TextureBridge::copyToSharedTex(
    ID3D11Device* dev,
    ID3D11DeviceContext* ctx,
    const ComPtr<ID3D11Texture2D>& tex,
    UINT index,
    const QSize& frameSize)
{
    if (!ensureSrcTex(dev, tex, frameSize))
        return false;

    // Ensure that texture is fully updated before we share it.
    ctx->Flush();

    if (m_srcMutex->AcquireSync(m_srcKey, INFINITE) != S_OK)
        return false;

    // Decoded frame texture may be larger than the frame size. Crop it to the frame size.
    const D3D11_BOX crop{0, 0, 0, (UINT) frameSize.width(), (UINT) frameSize.height(), 1};
    ctx->CopySubresourceRegion(m_srcTex.Get(), 0, 0, 0, 0, tex.Get(), index, &crop);

    m_srcMutex->ReleaseSync(m_destKey);
    return true;
}

ComPtr<ID3D11Texture2D> TextureBridge::copyFromSharedTex(
    const ComPtr<ID3D11Device1>& dev,
    const ComPtr<ID3D11DeviceContext>& ctx)
{
    if (!ensureDestTex(dev))
        return {};

    if (m_destMutex->AcquireSync(m_destKey, INFINITE) != S_OK)
        return {};

    ctx->CopySubresourceRegion(m_outputTex.Get(), 0, 0, 0, 0, m_destTex.Get(), 0, nullptr);

    m_destMutex->ReleaseSync(m_srcKey);

    return m_outputTex;
}

bool TextureBridge::ensureDestTex(const ComPtr<ID3D11Device1>& dev)
{
    if (m_destDevice != dev)
    {
        // We need to recreate texture when destination device changes.
        m_destTex = nullptr;
        m_destDevice = dev;
    }

    if (m_destTex)
        return true;

    if (m_destDevice->OpenSharedResource1(m_sharedHandle.Get(), IID_PPV_ARGS(&m_destTex)) != S_OK)
        return false;

    CD3D11_TEXTURE2D_DESC desc{};
    m_destTex->GetDesc(&desc);

    desc.MiscFlags = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (m_destDevice->CreateTexture2D(&desc, nullptr, m_outputTex.ReleaseAndGetAddressOf()) != S_OK)
        return false;

    return m_destTex.As(&m_destMutex) == S_OK;
}

bool TextureBridge::ensureSrcTex(
    ID3D11Device* dev,
    const ComPtr<ID3D11Texture2D>& tex,
    const QSize& frameSize)
{
    if (!isSrcInitialized(dev, tex, frameSize))
        return recreateSrc(dev, tex, frameSize);

    return true;
}

bool TextureBridge::isSrcInitialized(
    const ID3D11Device* dev,
    const ComPtr<ID3D11Texture2D>& tex,
    const QSize &frameSize) const
{
    if (!m_srcTex)
        return false;

    // We should reinitalize if device has changed.
    ComPtr<ID3D11Device> texDevice;
    m_srcTex->GetDevice(texDevice.GetAddressOf());
    if (dev != texDevice.Get())
        return false;

    // Reinitialize if shared texture format or size has changed.
    CD3D11_TEXTURE2D_DESC inputDesc{};
    tex->GetDesc(&inputDesc);

    CD3D11_TEXTURE2D_DESC currentDesc{};
    m_srcTex->GetDesc(&currentDesc);

    if (inputDesc.Format != currentDesc.Format)
        return false;

    const auto width = (UINT) frameSize.width();
    const auto height = (UINT) frameSize.height();

    return currentDesc.Width == width && currentDesc.Height == height;
}

bool TextureBridge::recreateSrc(
    ID3D11Device* dev,
    const ComPtr<ID3D11Texture2D>& tex,
    const QSize& frameSize)
{
    m_sharedHandle.Close();

    CD3D11_TEXTURE2D_DESC desc{};
    tex->GetDesc(&desc);

    const auto width = (UINT) frameSize.width();
    const auto height = (UINT) frameSize.height();

    CD3D11_TEXTURE2D_DESC texDesc{desc.Format, width, height};
    texDesc.MipLevels = 1;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    if (dev->CreateTexture2D(
            &texDesc,
            /* pInitialData */ nullptr,
            m_srcTex.ReleaseAndGetAddressOf()) != S_OK)
    {
        return false;
    }

    ComPtr<IDXGIResource1> res;
    if (m_srcTex.As(&res) != S_OK)
        return false;

    const HRESULT hr = res->CreateSharedHandle(
        /* pAttributes */ nullptr,
        DXGI_SHARED_RESOURCE_READ,
        /* lpName */ nullptr,
        m_sharedHandle.GetAddressOf());

    if (hr != S_OK || !m_sharedHandle.IsValid())
        return false;

    if (m_srcTex.As(&m_srcMutex) != S_OK || !m_srcMutex)
        return false;

    m_destTex = nullptr;
    m_destMutex = nullptr;
    return true;
}

ComPtr<ID3D11Device1> GetD3DDevice(QRhi* rhi)
{
    const auto native = static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
    if (!native)
        return {};

    const ComPtr<ID3D11Device> rhiDevice = static_cast<ID3D11Device*>(native->dev);

    ComPtr<ID3D11Device1> dev1;
    if (rhiDevice.As(&dev1) != S_OK)
        return nullptr;

    return dev1;
}

class TextureConverter
{
public:
    TextureConverter(QRhi* rhi):
        m_rhi(rhi),
        m_rhiDevice(GetD3DDevice(rhi))
    {
        if (!m_rhiDevice)
            return;

        m_rhiDevice->GetImmediateContext(m_rhiCtx.GetAddressOf());
    }

    ComPtr<ID3D11Texture2D> copyTexture(
        const AVD3D11VADeviceContext* avDeviceCtx,
        const ComPtr<ID3D11Texture2D>& ffmpegTex,
        UINT index,
        const QSize& frameSize)
    {
        // Lock the FFmpeg device context while we copy from FFmpeg's
        // frame pool into a shared texture because the underlying ID3D11DeviceContext
        // is not thread safe.
        avDeviceCtx->lock(avDeviceCtx->lock_ctx);
        const auto autoUnlock = nx::utils::makeScopeGuard(
            [&]
            {
                avDeviceCtx->unlock(avDeviceCtx->lock_ctx);
            });

        if (!m_bridge.copyToSharedTex(avDeviceCtx->device, avDeviceCtx->device_context,
            ffmpegTex, index, frameSize))
        {
            return {};
        }

        // Get a copy of the texture on the RHI device
        return m_bridge.copyFromSharedTex(m_rhiDevice, m_rhiCtx);
    }

private:
    QRhi* const m_rhi;
    ComPtr<ID3D11Device1> m_rhiDevice;
    ComPtr<ID3D11DeviceContext> m_rhiCtx;
    TextureBridge m_bridge;
};

std::shared_ptr<TextureConverter> getTextureConverterForRhi(QRhi* rhi)
{
    static std::mutex mutex;
    static QHash<QRhi*, std::shared_ptr<TextureConverter>> allRhis;

    std::lock_guard lock(mutex);

    if (auto converter = allRhis.value(rhi, {}))
        return converter;

    std::shared_ptr<TextureConverter> converter = std::make_shared<TextureConverter>(rhi);

    rhi->addCleanupCallback(
        [](QRhi* rhi)
        {
            std::lock_guard lock(mutex);
            allRhis.remove(rhi);
        });

    allRhis[rhi] = converter;

    return converter;
}

class VideoFrameTextures: public QVideoFrameTextures
{
public:
    VideoFrameTextures(ComPtr<ID3D11Texture2D> d3d11Texture):
        m_d3d11Texture(std::move(d3d11Texture))
    {
    }

    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane >= m_textures.size())
            return nullptr;

        return m_textures[plane].get();
    }

    ComPtr<ID3D11Texture2D> m_d3d11Texture;
    std::array<std::unique_ptr<QRhiTexture>, 4> m_textures;
};

QVideoFrameFormat::PixelFormat toQtPixelFormat(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_NV12:
            return QVideoFrameFormat::Format_NV12;
        case DXGI_FORMAT_420_OPAQUE:
            return QVideoFrameFormat::Format_YUV420P;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return QVideoFrameFormat::Format_RGBX8888;
        default:
            return QVideoFrameFormat::Format_Invalid;
    }
}

class D3D11MemoryBuffer: public QHwVideoBuffer
{
public:
    D3D11MemoryBuffer(const AVFrame* frame):
        QHwVideoBuffer(QVideoFrame::NoHandle),
        m_frame(frame)
    {
    }

    virtual ~D3D11MemoryBuffer() override
    {
    }

    virtual MapData map(QVideoFrame::MapMode mode) override
    {
        return {};
    }

    virtual void unmap() override
    {
    }

    virtual std::unique_ptr<QVideoFrameTextures> mapTextures(QRhi* rhi) override
    {
        const auto fCtx = reinterpret_cast<AVHWFramesContext*>(m_frame->hw_frames_ctx->data);
        const auto ctx = fCtx->device_ctx;

        if (!ctx || ctx->type != AV_HWDEVICE_TYPE_D3D11VA)
            return {};

        const ComPtr<ID3D11Texture2D> ffmpegTex = reinterpret_cast<ID3D11Texture2D*>(
            m_frame->data[0]);
        const int index = static_cast<int>(reinterpret_cast<intptr_t>(m_frame->data[1]));

        const auto* avDeviceCtx = static_cast<AVD3D11VADeviceContext*>(ctx->hwctx);

        if (!avDeviceCtx)
            return {};

        auto converter = getTextureConverterForRhi(rhi);

        const QSize frameSize{m_frame->width, m_frame->height};
        ComPtr<ID3D11Texture2D> output = converter->copyTexture(
            avDeviceCtx, ffmpegTex, index, frameSize);

        std::unique_ptr<VideoFrameTextures> textures(new VideoFrameTextures(std::move(output)));

        const QVideoFrameFormat frameFormat = format();
        quint64 handle = reinterpret_cast<quint64>(textures->m_d3d11Texture.Get());
        for (int plane = 0; plane < 4; ++plane)
            textures->m_textures[plane] = createTextureFromHandle(frameFormat, rhi, plane, handle);

        return textures;
    }

    QVideoFrameFormat format() const override
    {
        if (!m_frame)
            return {};

        const ComPtr<ID3D11Texture2D> ffmpegTex = reinterpret_cast<ID3D11Texture2D*>(
            m_frame->data[0]);
        D3D11_TEXTURE2D_DESC desc;
        memset(&desc, 0, sizeof(desc));
        ffmpegTex->GetDesc(&desc);
        const auto qtPixelFormat = toQtPixelFormat(desc.Format);

        return QVideoFrameFormat(QSize(m_frame->width, m_frame->height), qtPixelFormat);
    }

private:
    const AVFrame* m_frame = nullptr;
};

} // namespace

class D3D11VideoApiEntry: public VideoApiRegistry::Entry
{
public:
    D3D11VideoApiEntry()
    {
        VideoApiRegistry::instance()->add(AV_HWDEVICE_TYPE_D3D11VA, this);
    }

    virtual AVHWDeviceType deviceType() const override
    {
        return AV_HWDEVICE_TYPE_D3D11VA;
    }

    virtual nx::media::VideoFramePtr makeFrame(const AVFrame* frame) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_D3D11) || !frame->hw_frames_ctx)
            return {};

        const auto fCtx = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
        const auto ctx = fCtx->device_ctx;

        if (!ctx || ctx->type != AV_HWDEVICE_TYPE_D3D11VA)
            return {};

        auto result = std::make_shared<VideoFrame>(std::make_unique<D3D11MemoryBuffer>(frame));
        result->setStartTime(frame->pkt_dts);

        return result;
    }

    virtual std::unique_ptr<nx::media::ffmpeg::AvOptions> options(QRhi* rhi) const override
    {
        if (!NX_ASSERT(rhi && rhi->backend() == QRhi::Implementation::D3D11))
            return {};

        auto options = std::make_unique<nx::media::ffmpeg::AvOptions>();
        options->set("vendor_id", QByteArray::number(rhi->driverInfo().vendorId));
        return options;
    }
};

D3D11VideoApiEntry g_d3d11VideoApiEntry;

} // namespace nx::media
