// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <d3d11_4.h>

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

#include "d3d_common.h"
#include "hw_video_api.h"
#include "texture_helper.h"

using namespace Microsoft::WRL;
using namespace std::chrono;

namespace nx::media {

namespace {

ComPtr<ID3D11Device1> GetD3DDevice(QRhi* rhi)
{
    const auto native = static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
    if (!native)
        return {};

    const ComPtr<ID3D11Device> rhiDevice = static_cast<ID3D11Device*>(native->dev);

    ComPtr<ID3D11Device1> dev1;
    if (FAILED(rhiDevice.As(&dev1)))
        return nullptr;

    return dev1;
}

struct SharedTexture
{
    ComPtr<ID3D11Texture2D> texture;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    QSize size;
    Wrappers::HandleT<Wrappers::HandleTraits::HANDLETraits> handle;
};

class TexturePool: public TexturePoolBase<TexturePool, SharedTexture>
{
    friend class TexturePoolBase<TexturePool, SharedTexture>;

public:
    TexturePool(QRhi* rhi): m_rhiDevice(GetD3DDevice(rhi))
    {
    }

private:
    std::shared_ptr<SharedTexture> newTexture(const CD3D11_TEXTURE2D_DESC& d, const QSize& size)
    {
        CD3D11_TEXTURE2D_DESC desc(d.Format, size.width(), size.height());
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MipLevels = 1;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

        auto shared = std::make_shared<SharedTexture>();

        throwIfFailed(
            m_rhiDevice->CreateTexture2D(
                &desc,
                /*pInitialData*/ nullptr,
                shared->texture.ReleaseAndGetAddressOf()));

        shared->format = desc.Format;
        shared->size = size;

        ComPtr<IDXGIResource1> res;
        throwIfFailed(shared->texture.As(&res));

        throwIfFailed(
            res->CreateSharedHandle(
                /* pAttributes */ nullptr,
                DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                /* lpName */ nullptr,
                shared->handle.GetAddressOf()));

        return shared;
    }

private:
    ComPtr<ID3D11Device1> m_rhiDevice;
};

// Helper for using either a fence or a query for waiting on GPU operations.
class WaitHelper
{
public:
    WaitHelper() = default;

    void init(
        const ComPtr<ID3D11Device1>& device,
        const ComPtr<ID3D11DeviceContext>& context)
    {
        clear();

        m_device = device;
        m_context = context;

        // Using ID3D11Fence requires Windows 10 Creators Update.
        m_device.As(&m_device5);
        m_context.As(&m_context4);

        if (m_device5 && m_context4)
        {
            throwIfFailed(m_device5->CreateFence(
                m_fenceValue, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        }
        else
        {
            const D3D11_QUERY_DESC queryDesc{.Query = D3D11_QUERY_EVENT, .MiscFlags = 0};
            throwIfFailed(m_device->CreateQuery(&queryDesc, &m_query));
        }
    }

    void clear()
    {
        m_device.Reset();
        m_context.Reset();
        m_device5.Reset();
        m_context4.Reset();
        m_fence.Reset();
        m_fenceValue = 0;
        if (m_handle)
            CloseHandle(m_handle);
        m_handle = nullptr;
        m_query.Reset();
    }

    ~WaitHelper()
    {
        clear();
    }

    // Wait for the device to finish processing. Uses either a fence or a query.
    void wait(
        const ComPtr<ID3D11Device1>& device,
        const ComPtr<ID3D11DeviceContext>& context)
    {
        if (m_context.Get() != context.Get())
            init(device, context);

        if (m_fence)
        {
            ++m_fenceValue;
            m_context4->Signal(m_fence.Get(), m_fenceValue);

            if (m_fence->GetCompletedValue() < m_fenceValue)
            {
                if (!m_handle)
                    m_handle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
                throwIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_handle));
                WaitForSingleObject(m_handle, INFINITE);
            }
        }
        else if (m_query)
        {
            int nextSleep = 2;

            m_context->End(m_query.Get());

            BOOL queryData = 0;
            while(S_OK != m_context->GetData(m_query.Get(), &queryData, sizeof(queryData), 0))
            {
                nextSleep = std::min(nextSleep * 2, 64);
                Sleep(nextSleep);
            }
        }
    }

private:
    ComPtr<ID3D11Device1> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11Device5> m_device5;
    ComPtr<ID3D11DeviceContext4> m_context4;
    ComPtr<ID3D11Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_handle = nullptr;

    ComPtr<ID3D11Query> m_query;
};

class DecoderData: public VideoApiDecoderData
{
public:
    DecoderData(QRhi* rhi): m_texturePool(rhi)
    {
    }

    ~DecoderData() override
    {
    }

    std::shared_ptr<SharedTexture> copyFrameTexture(
        const AVD3D11VADeviceContext* avDeviceCtx,
        const ComPtr<ID3D11Texture2D>& ffmpegTex,
        UINT index,
        const QSize& frameSize)
    {
        avDeviceCtx->lock(avDeviceCtx->lock_ctx);

        const auto autoUnlock = nx::utils::makeScopeGuard(
            [&avDeviceCtx]
            {
                avDeviceCtx->unlock(avDeviceCtx->lock_ctx);
            });

        CD3D11_TEXTURE2D_DESC desc{};
        ffmpegTex->GetDesc(&desc);

        auto shared = m_texturePool.getTexture(desc, frameSize);

        if (!shared)
            return {};

        // Ensure that texture is fully updated before we copy it.
        avDeviceCtx->device_context->Flush();

        ComPtr<ID3D11Device> dev = avDeviceCtx->device;
        ComPtr<ID3D11Device1> hwDevice;
        throwIfFailed(dev.As(&hwDevice));

        ComPtr<ID3D11Texture2D> destTex;

        throwIfFailed(
            hwDevice->OpenSharedResource1(
                shared->handle.Get(), IID_PPV_ARGS(&destTex)));

        ID3D11DeviceContext* hwCtx = avDeviceCtx->device_context;

        // Decoded frame texture may be larger than the frame size. Crop it to the frame size.
        const D3D11_BOX crop{0, 0, 0, (UINT) frameSize.width(), (UINT) frameSize.height(), 1};
        hwCtx->CopySubresourceRegion(destTex.Get(), 0, 0, 0, 0, ffmpegTex.Get(), index, &crop);
        hwCtx->Flush();

        m_waitHelper.wait(hwDevice, hwCtx);

        return shared;
    }

private:
    TexturePool m_texturePool;
    WaitHelper m_waitHelper;
};

class VideoFrameTextures: public QVideoFrameTextures
{
public:
    VideoFrameTextures(std::shared_ptr<SharedTexture> d3d11Texture):
        m_d3d11Texture(std::move(d3d11Texture))
    {
    }

    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane >= m_textures.size())
            return nullptr;

        return m_textures[plane].get();
    }

    std::shared_ptr<SharedTexture> m_d3d11Texture;
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
    D3D11MemoryBuffer(const AVFrame* frame, std::shared_ptr<DecoderData> decoderData):
        QHwVideoBuffer(QVideoFrame::NoHandle),
        m_frame(frame)
    {
        const auto fCtx = reinterpret_cast<AVHWFramesContext*>(m_frame->hw_frames_ctx->data);
        const auto ctx = fCtx->device_ctx;

        if (!ctx || ctx->type != AV_HWDEVICE_TYPE_D3D11VA)
            return;

        const ComPtr<ID3D11Texture2D> ffmpegTex = reinterpret_cast<ID3D11Texture2D*>(
            m_frame->data[0]);
        const int index = static_cast<int>(reinterpret_cast<intptr_t>(m_frame->data[1]));

        const auto* avDeviceCtx = static_cast<AVD3D11VADeviceContext*>(ctx->hwctx);

        if (!avDeviceCtx)
            return;

        const QSize frameSize{m_frame->width, m_frame->height};

        try
        {
            m_sharedTexture = decoderData->copyFrameTexture(
                avDeviceCtx, ffmpegTex, index, frameSize);
        }
        catch (const _com_error& e)
        {
            // DirectX API can throw COM exception.
            NX_WARNING(this, "COM Error: %1", QString::fromWCharArray(e.ErrorMessage()));
        }
        catch (const std::exception& e)
        {
            NX_WARNING(this, "Caught exception: %1", e.what());
        }
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

    virtual std::unique_ptr<QVideoFrameTextures> mapTextures(
        QRhi& rhi, QVideoFrameTexturesUPtr& /*oldTextures*/) override
    {
        if (!m_sharedTexture)
            return {};

        std::unique_ptr<VideoFrameTextures> textures(new VideoFrameTextures(m_sharedTexture));

        const QVideoFrameFormat frameFormat = format();
        quint64 handle = reinterpret_cast<quint64>(textures->m_d3d11Texture->texture.Get());
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
    std::shared_ptr<SharedTexture> m_sharedTexture;
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

    virtual nx::media::VideoFramePtr makeFrame(
        const AVFrame* frame,
        std::shared_ptr<VideoApiDecoderData> decoderData) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_D3D11) || !frame->hw_frames_ctx)
            return {};

        const auto fCtx = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
        const auto ctx = fCtx->device_ctx;

        if (!ctx || ctx->type != AV_HWDEVICE_TYPE_D3D11VA)
            return {};

        auto result = std::make_shared<VideoFrame>(
            std::make_unique<D3D11MemoryBuffer>(
                frame,
                std::dynamic_pointer_cast<DecoderData>(decoderData)));
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

    virtual std::shared_ptr<VideoApiDecoderData> createDecoderData(QRhi* rhi) const override
    {
        return std::make_shared<DecoderData>(rhi);
    }
};

D3D11VideoApiEntry g_d3d11VideoApiEntry;

} // namespace nx::media
