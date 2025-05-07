// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <d3d12.h>
#include <dxgiformat.h>
#include <dxgi1_3.h>

#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/private/qhwvideobuffer_p.h>

#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d12va.h>
} // extern "C"

#include "d3d_common.h"
#include "hw_video_api.h"
#include "texture_helper.h"

// Missing defines in older DirectX SDK.
#define DXGI_FORMAT_NV12 103
#define DXGI_FORMAT_420_OPAQUE 106

using namespace Microsoft::WRL;

namespace nx::media {

namespace {

ComPtr<ID3D12Device> GetD3DDevice(QRhi* rhi)
{
    const auto native = static_cast<const QRhiD3D12NativeHandles*>(rhi->nativeHandles());
    if (!native)
        return {};

    ComPtr<ID3D12Device> rhiDevice = static_cast<ID3D12Device*>(native->dev);

    return rhiDevice;
}

static inline int planeCountDxgi(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_NV12:
            return 2;
        case DXGI_FORMAT_420_OPAQUE:
            return 3;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return 1;
        default:
            NX_ASSERT(false, "Unsupported DXGI format %1", format);
            return 1;
    }
}

struct SharedTexture
{
    ComPtr<ID3D12Resource> texture;
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
    std::shared_ptr<SharedTexture> newTexture(const D3D12_RESOURCE_DESC& d, const QSize&)
    {
        D3D12_RESOURCE_DESC desc = d;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

        const D3D12_HEAP_PROPERTIES defaultHeapProperties{
            .Type = D3D12_HEAP_TYPE_DEFAULT,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1
        };

        auto shared = std::make_shared<SharedTexture>();

        throwIfFailed(m_rhiDevice->CreateCommittedResource(
            &defaultHeapProperties,
            D3D12_HEAP_FLAG_SHARED,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            /*pOptimizedClearValue*/ nullptr,
            IID_PPV_ARGS(shared->texture.ReleaseAndGetAddressOf())));

        shared->format = desc.Format;
        shared->size = QSize((int) desc.Width, (int) desc.Height);

        throwIfFailed(m_rhiDevice->CreateSharedHandle(
            shared->texture.Get(),
            nullptr,
            GENERIC_ALL,
            nullptr,
            shared->handle.GetAddressOf()));

        return shared;
    }

private:
    ComPtr<ID3D12Device> m_rhiDevice;
};

// Holds fence for waiting on GPU operations.
class WaitHelper
{
public:
    WaitHelper() = default;

    ~WaitHelper()
    {
        if (m_fenceEvent)
            CloseHandle(m_fenceEvent);
    }

    void wait(const ComPtr<ID3D12Device>& device, ID3D12CommandQueue* commandQueue)
    {
        if (m_device.Get() != device.Get())
            init(device);

        ++m_fenceValue;

        throwIfFailed(commandQueue->Signal(m_fence.Get(), m_fenceValue));

        if (m_fence->GetCompletedValue() < m_fenceValue)
        {
            if (!m_fenceEvent)
                m_fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
            throwIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

private:
    void init(const ComPtr<ID3D12Device>& device)
    {
        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }
        m_fence.Reset();
        m_fenceValue = 0;
        m_device.Reset();
        throwIfFailed(device->CreateFence(
            m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));

        m_device = device;
    }

private:
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

};

class DecoderData: public VideoApiDecoderData
{
    using base_type = VideoApiDecoderData;

public:
    DecoderData(QRhi* rhi): base_type(rhi), m_texturePool(rhi)
    {
    }

    ~DecoderData() override
    {
    }

    std::shared_ptr<SharedTexture> copyFrameTexture(
        const AVD3D12VADeviceContext* avDeviceCtx,
        AVD3D12VAFrame* frame)
    {
        // Wait for decoder to finish processing the frame.
        if (frame->sync_ctx.fence->GetCompletedValue() < frame->sync_ctx.fence_value)
        {
            throwIfFailed(
                frame->sync_ctx.fence->SetEventOnCompletion(
                    frame->sync_ctx.fence_value,
                    frame->sync_ctx.event));
            WaitForSingleObject(frame->sync_ctx.event, INFINITE);
        }

        // Protect access to frame texture while we copy it into a shared texture.
        avDeviceCtx->lock(avDeviceCtx->lock_ctx);
        const auto autoUnlock = nx::utils::makeScopeGuard(
            [&avDeviceCtx]()
            {
                avDeviceCtx->unlock(avDeviceCtx->lock_ctx);
            });

        ComPtr<ID3D12Resource> ffmpegTex = frame->texture;
        D3D12_RESOURCE_DESC desc = ffmpegTex->GetDesc();

        auto shared = m_texturePool.getTexture(desc, QSize(desc.Width, desc.Height));

        if (!shared)
            return {};

        // Copy texture data from ffmpeg to shared texture.
        ComPtr<ID3D12Resource> destTex;
        throwIfFailed(
            avDeviceCtx->device->OpenSharedHandle(
                shared->handle.Get(), IID_PPV_ARGS(&destTex)));

        setupCommandQueue(avDeviceCtx->device);

        D3D12_TEXTURE_COPY_LOCATION dstLoc{
            .pResource = destTex.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = 0};

        D3D12_TEXTURE_COPY_LOCATION srcLoc{
            .pResource = ffmpegTex.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = 0};

        const int planeCount = planeCountDxgi(desc.Format);

        for (int plane = 0; plane < planeCount; ++plane)
        {
            dstLoc.SubresourceIndex = plane;
            srcLoc.SubresourceIndex = plane;

            m_commandList->CopyTextureRegion(
                &dstLoc,
                0, 0, 0, //< DstX, DstY, DstZ
                &srcLoc,
                /*pSrcBox*/ nullptr);
        }

        throwIfFailed(m_commandList->Close());

        ID3D12CommandList* ppCommandLists[] = {m_commandList.Get()};
        m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

        // Wait for the command queue to finish execution
        m_waitHelper.wait(avDeviceCtx->device, m_commandQueue.Get());

        return shared;
    }

private:
    void setupCommandQueue(const ComPtr<ID3D12Device>& device)
    {
        static constexpr auto kListType = D3D12_COMMAND_LIST_TYPE_COPY;

        if (!m_commandAllocator)
        {
            throwIfFailed(
                device->CreateCommandAllocator(
                    kListType,
                    IID_PPV_ARGS(m_commandAllocator.ReleaseAndGetAddressOf())));
        }
        else
        {
            m_commandAllocator->Reset();
        }

        if (!m_commandList)
        {
            throwIfFailed(
                device->CreateCommandList(
                    /*nodeMask*/ 0,
                    kListType,
                    m_commandAllocator.Get(),
                    /*pInitialState*/ nullptr,
                    IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
        }
        else
        {
            m_commandList->Reset(m_commandAllocator.Get(), nullptr);
        }

        if (!m_commandQueue)
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {
                .Type = kListType,
                .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            };

            throwIfFailed(
                device->CreateCommandQueue(
                    &queueDesc,
                    IID_PPV_ARGS(&m_commandQueue)));
        }
    }

private:
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    TexturePool m_texturePool;
    WaitHelper m_waitHelper;
};

class VideoFrameTextures: public QVideoFrameTextures
{
public:
    VideoFrameTextures(std::shared_ptr<SharedTexture> d3d12Texture):
        m_d3d12Texture(std::move(d3d12Texture))
    {
    }

    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane >= m_textures.size())
            return nullptr;

        return m_textures[plane].get();
    }

    std::shared_ptr<SharedTexture> m_d3d12Texture;
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

class D3D12MemoryBuffer: public QHwVideoBuffer
{
public:
    D3D12MemoryBuffer(const AVFrame* frame, std::shared_ptr<DecoderData> decoderData):
        QHwVideoBuffer(QVideoFrame::NoHandle),
        m_frame(frame)
    {
        const auto framesCtx = reinterpret_cast<AVHWFramesContext*>(m_frame->hw_frames_ctx->data);
        const auto ctx = framesCtx->device_ctx;

        if (!ctx || ctx->type != AV_HWDEVICE_TYPE_D3D12VA)
            return;

        const auto* avDeviceCtx = static_cast<AVD3D12VADeviceContext*>(ctx->hwctx);

        if (!avDeviceCtx)
            return;

        auto f = reinterpret_cast<AVD3D12VAFrame*>(m_frame->data[0]);

        try
        {
            m_sharedTexture = decoderData->copyFrameTexture(avDeviceCtx, f);
            m_rhi = decoderData->rhi();
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

    virtual ~D3D12MemoryBuffer() override
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

        if (&rhi != m_rhi)
            return {};

        std::unique_ptr<VideoFrameTextures> textures(new VideoFrameTextures(m_sharedTexture));

        const QVideoFrameFormat frameFormat = format();

        QRhiTexture::Flags planeFlag[4] = {
            QRhiTexture::Plane0,
            QRhiTexture::Plane1,
            QRhiTexture::Plane2,
            {}};

        quint64 handle = reinterpret_cast<quint64>(textures->m_d3d12Texture->texture.Get());
        for (int plane = 0; plane < 4; ++plane)
        {
            textures->m_textures[plane] = createTextureFromHandle(
                frameFormat, rhi, plane, handle, /*layout*/ 0, planeFlag[plane]);
        }

        return textures;
    }

    QVideoFrameFormat format() const override
    {
        if (!m_frame || !m_frame->hw_frames_ctx || !m_frame->data[0])
            return {};

        D3D12_RESOURCE_DESC desc =
            reinterpret_cast<AVD3D12VAFrame*>(m_frame->data[0])->texture->GetDesc();

        const auto qtPixelFormat = toQtPixelFormat(desc.Format);

        return QVideoFrameFormat(QSize(m_frame->width, m_frame->height), qtPixelFormat);
    }

private:
    const AVFrame* m_frame = nullptr;
    QRhi* m_rhi = nullptr;
    std::shared_ptr<SharedTexture> m_sharedTexture;
};

static int findAdapterIndexByVendorId(quint64 vendorId)
{
    ComPtr<IDXGIFactory2> dxgiFactory;
    const HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));

    if (FAILED(hr))
    {
        NX_WARNING(NX_SCOPE_TAG,
            "CreateDXGIFactory2() failed to create DXGI factory: 0x%1",
            QString::number((uint32_t) hr, 16));
        return -1;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (int adapterIndex = 0;
        dxgiFactory->EnumAdapters1(UINT(adapterIndex), adapter.ReleaseAndGetAddressOf())
            != DXGI_ERROR_NOT_FOUND;
        ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.VendorId == vendorId)
            return adapterIndex;
    }

    return -1;
}

} // namespace

class D3D12VideoApiEntry: public VideoApiRegistry::Entry
{
public:
    D3D12VideoApiEntry()
    {
        VideoApiRegistry::instance()->add(AV_HWDEVICE_TYPE_D3D12VA, this);
    }

    virtual AVHWDeviceType deviceType() const override
    {
        return AV_HWDEVICE_TYPE_D3D12VA;
    }

    virtual nx::media::VideoFramePtr makeFrame(
        const AVFrame* frame,
        std::shared_ptr<VideoApiDecoderData> decoderData) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_D3D12) || !frame->hw_frames_ctx)
            return {};

        const auto fCtx = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
        const auto ctx = fCtx->device_ctx;

        if (!ctx || ctx->type != AV_HWDEVICE_TYPE_D3D12VA)
            return {};

        auto result = std::make_shared<VideoFrame>(
            std::make_unique<D3D12MemoryBuffer>(
                frame,
                std::dynamic_pointer_cast<DecoderData>(decoderData)));
        result->setStartTime(frame->pkt_dts);

        return result;
    }

    virtual std::string device(QRhi* rhi) const override
    {
        if (!NX_ASSERT(rhi && rhi->backend() == QRhi::Implementation::D3D12))
            return {};

        const int deviceIndex = findAdapterIndexByVendorId(rhi->driverInfo().vendorId);

        if (deviceIndex < 0)
            return {};

        return std::to_string(deviceIndex);
    }

    virtual std::shared_ptr<VideoApiDecoderData> createDecoderData(QRhi* rhi) const override
    {
        return std::make_shared<DecoderData>(rhi);
    }
};

D3D12VideoApiEntry g_d3d12VideoApiEntry;

} // namespace nx::media
