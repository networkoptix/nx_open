// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/rhi/qrhi.h>

#if QT_CONFIG(vulkan) && !defined(Q_OS_ANDROID)

#include <QtMultimedia/private/qhwvideobuffer_p.h>
#include <QtMultimedia/private/qvideotexturehelper_p.h>

#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>

#include "texture_helper.h"
#include "hw_video_api.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
} // extern "C"

namespace nx::media {

namespace {

class VideoFrameTextures: public QVideoFrameTextures
{
public:
    VideoFrameTextures(int nPlanes): m_nPlanes(nPlanes)
    {
    }

    virtual ~VideoFrameTextures() override
    {
    }

    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane >= m_textures.size())
            return nullptr;

        return m_textures[plane].get();
    }

    int m_nPlanes = 0;
    std::array<std::unique_ptr<QRhiTexture>, 4> m_textures;
};

class VulkanMemoryBuffer: public QHwVideoBuffer
{
    using base_type = QHwVideoBuffer;

public:
    VulkanMemoryBuffer(const AVFrame* frame):
        base_type(QVideoFrame::NoHandle),
        m_frame(frame)
    {
    }

    ~VulkanMemoryBuffer()
    {
    }

    virtual MapData map(QVideoFrame::MapMode /*mode*/) override
    {
        return {};
    }

    virtual void unmap() override
    {
    }

    virtual std::unique_ptr<QVideoFrameTextures> mapTextures(QRhi* rhi) override
    {
        if (!m_frame->hw_frames_ctx)
            return {};

        AVHWFramesContext* hwfc = (AVHWFramesContext*) m_frame->hw_frames_ctx->data;
        const AVVulkanFramesContext* vkfc = (const AVVulkanFramesContext*) hwfc->hwctx;

        auto vkFrame = (AVVkFrame*) m_frame->data[0];

        auto qtFormat = toQtPixelFormat(m_frame);
        QVideoFrameFormat format(QSize(hwfc->width, hwfc->height), qtFormat);

        int numImages = 0;
        while (vkFrame->img[numImages] != VK_NULL_HANDLE)
            ++numImages;

        const auto texDesc = QVideoTextureHelper::textureDescription(qtFormat);
        int nPlanes = texDesc->nplanes;

        vkfc->lock_frame(hwfc, vkFrame);

        auto textures = std::make_unique<VideoFrameTextures>(nPlanes);

        for (int plane = 0; plane < nPlanes; ++plane)
        {
            QRhiTexture::Flags flags = {};
            int index = plane;

            // If we have multiple planes and one image, then that is a multiplane
            // frame. Anything else is treated as one-image-per-plane.
            if (nPlanes > 1 && numImages == 1)
            {
                index = 0;

                switch (plane)
                {
                    case 0:
                        flags = QRhiTexture::Plane0;
                        break;
                    case 1:
                        flags = QRhiTexture::Plane1;
                        break;
                    case 2:
                        flags = QRhiTexture::Plane2;
                        break;
                    default:
                        return {};
                }
            }

            textures->m_textures[plane] = createTextureFromHandle(
                format, rhi, plane, (quint64) vkFrame->img[index], vkFrame->layout[index], flags);
        }

        vkfc->unlock_frame(hwfc, vkFrame);

        return textures;
    }

    QVideoFrameFormat format() const override
    {
        if (!m_frame)
            return {};

        return QVideoFrameFormat(QSize(m_frame->width, m_frame->height), toQtPixelFormat(m_frame));
    }

private:
    const AVFrame* m_frame = nullptr;
};

int selectQueueFamily(
    const QList<VkQueueFamilyProperties>& queueFamilyProps,
    VkQueueFlagBits flag,
    int& count)
{
    for (int i = 0; i < queueFamilyProps.size(); ++i)
    {
        if (queueFamilyProps[i].queueFlags & flag)
        {
            count = queueFamilyProps[i].queueCount;
            return i;
        }
    }
    count = 0;
    return -1;
}

} // namespace

class VulkanVideoApiEntry: public VideoApiRegistry::Entry
{
public:
    VulkanVideoApiEntry()
    {
        VideoApiRegistry::instance()->add(AV_HWDEVICE_TYPE_VULKAN, this);
    }

    virtual AVHWDeviceType deviceType() const override
    {
        return AV_HWDEVICE_TYPE_VULKAN;
    }

    virtual nx::media::VideoFramePtr makeFrame(const AVFrame* frame) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_VULKAN))
            return {};

        auto result = std::make_shared<VideoFrame>(std::make_unique<VulkanMemoryBuffer>(frame));
        result->setStartTime(frame->pkt_dts);

        return result;
    }

    virtual nx::media::ffmpeg::HwVideoDecoder::InitFunc initFunc(QRhi* rhi) const override
    {
        if (!NX_ASSERT(rhi->backend() == QRhi::Vulkan))
            return {};

        return
            [rhi](AVHWDeviceContext* deviceContext) -> bool
            {
                AVVulkanDeviceContext* ctx = (AVVulkanDeviceContext*) deviceContext->hwctx;
                auto native = (QRhiVulkanNativeHandles*) rhi->nativeHandles();

                deviceContext->user_opaque = rhi;

                ctx->get_proc_addr = (PFN_vkGetInstanceProcAddr) native->inst->getInstanceProcAddr(
                    "vkGetInstanceProcAddr");

                ctx->inst = native->inst->vkInstance();
                ctx->phys_dev = native->physDev;
                ctx->act_dev = native->dev;

                ctx->device_features = native->physDevFeatures2;

                ctx->enabled_inst_extensions = native->instExts.constData();
                ctx->nb_enabled_inst_extensions = native->instExts.size();

                ctx->enabled_dev_extensions = native->devExts.constData();
                ctx->nb_enabled_dev_extensions = native->devExts.size();

                // TODO: Switch to new API (ctx->qf and ctx->nb_qf) after FFmpeg update.
                ctx->queue_family_index = selectQueueFamily(
                    native->queueFamilyProps, VK_QUEUE_GRAPHICS_BIT, ctx->nb_graphics_queues);
                ctx->queue_family_tx_index = selectQueueFamily(
                    native->queueFamilyProps, VK_QUEUE_TRANSFER_BIT, ctx->nb_tx_queues);
                ctx->queue_family_comp_index = selectQueueFamily(
                    native->queueFamilyProps, VK_QUEUE_COMPUTE_BIT, ctx->nb_comp_queues);
                ctx->queue_family_decode_index = selectQueueFamily(
                    native->queueFamilyProps, VK_QUEUE_VIDEO_DECODE_BIT_KHR, ctx->nb_decode_queues);
                if (ctx->queue_family_decode_index == -1)
                {
                    NX_WARNING(NX_SCOPE_TAG, "Unable to find video decode queue");
                    return false;
                }
                ctx->queue_family_encode_index = selectQueueFamily(
                    native->queueFamilyProps, VK_QUEUE_VIDEO_ENCODE_BIT_KHR, ctx->nb_encode_queues);

                ctx->lock_queue =
                    [](AVHWDeviceContext *ctx, uint32_t, uint32_t)
                    {
                        auto rhi = static_cast<QRhi*>(ctx->user_opaque);
                        auto native = (QRhiVulkanNativeHandles*) rhi->nativeHandles();
                        native->gfxQueueMutex.lock();
                    };

                ctx->unlock_queue =
                    [](AVHWDeviceContext *ctx, uint32_t, uint32_t)
                    {
                        auto rhi = static_cast<QRhi*>(ctx->user_opaque);
                        auto native = (QRhiVulkanNativeHandles*) rhi->nativeHandles();
                        native->gfxQueueMutex.unlock();
                    };

                return true;
            };
    }
};

VulkanVideoApiEntry g_vulkanVideoApiEntry;

} // namespace nx::media

#endif // #if QT_CONFIG(vulkan) && !defined(Q_OS_ANDROID)
