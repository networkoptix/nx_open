#include "proxy_video_decoder_impl.h"
#if defined(ENABLE_PROXY_DECODER)

#include <nx/kit/debug.h>

#include <nx/utils/string.h>

#include "proxy_video_decoder_utils.h"
#include "proxy_video_decoder_gl_utils.h"

namespace nx {
namespace media {

namespace {

static const QRect kUndefinedVideoGeometry{-1, -1, -1, -1};
static const QRect kFullscreenVideoGeometry{0, 0, 0, 0};

class Impl: public ProxyVideoDecoderImpl
{
public:
    using ProxyVideoDecoderImpl::ProxyVideoDecoderImpl;

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;

private:
    class VideoBuffer;

private:
    /** Call doDisplayDecodedFrame() depending on .ini: either via Lambda, or immediately. */
    void displayDecodedFrame(void* frameHandle);

    /** Call proxydecoder().displayDecoded() immediately. */
    void doDisplayDecodedFrame(void* frameHandle);

private:
    QRect prevVideoGeometry = kUndefinedVideoGeometry;
};

class Impl::VideoBuffer: public QAbstractVideoBuffer
{
public:
    VideoBuffer(
        void* frameHandle,
        const std::shared_ptr<ProxyVideoDecoderImpl>& owner)
        :
        QAbstractVideoBuffer(GLTextureHandle),
        m_frameHandle(frameHandle),
        m_owner(std::dynamic_pointer_cast<Impl>(owner))
    {
    }

    ~VideoBuffer()
    {
    }

    virtual MapMode mapMode() const override
    {
        return NotMapped;
    }

    virtual uchar* map(MapMode, int*, int*) override
    {
        return 0;
    }

    virtual void unmap() override
    {
    }

    virtual QVariant handle() const override
    {
        if (!m_displayed)
        {
            m_displayed = true;
            if (auto owner = m_owner.lock())
            {
                NX_FPS(Handle);
                owner->displayDecodedFrame(m_frameHandle);
            }
            else
            {
                NX_OUTPUT << "VideoBuffer::handle(): already destroyed";
            }
        }
        return 0;
    }

private:
    mutable bool m_displayed = false;
    void* m_frameHandle;
    std::weak_ptr<Impl> m_owner;
};

int Impl::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData,
    QVideoFramePtr* outDecodedFrame)
{
    NX_TIME_BEGIN(Decode);
    NX_CRITICAL(outDecodedFrame);
    outDecodedFrame->reset();

    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    void* frameHandle = nullptr;
    int64_t ptsUs = 0;
    // Perform actual decoding from QnCompressedVideoData to display.
    int result = proxyDecoder().decodeToDisplayQueue(compressedFrame.get(), &ptsUs, &frameHandle);
    if (result > 0) //< Not "Buffering", no error.
    {
        if (frameHandle)
        {
            auto videoBuffer = new VideoBuffer(frameHandle, sharedPtrToThis());
            setQVideoFrame(outDecodedFrame, videoBuffer, QVideoFrame::Format_BGR32, ptsUs);
        }
        else
        {
            result = 0;
        }
    }
    NX_TIME_END(Decode);
    return result;
}

void Impl::displayDecodedFrame(void* frameHandle)
{
    if (ini().displayAsync)
    {
        auto selfPtr = std::weak_ptr<Impl>(std::dynamic_pointer_cast<Impl>(sharedPtrToThis()));
        allocator().execAtGlThreadAsync(
            [selfPtr, frameHandle]()
            {
                if (auto self = selfPtr.lock())
                {
                    if (ini().displayAsyncGlFinish)
                    {
                        NX_GL_GET_FUNCS(QOpenGLContext::currentContext());
                        NX_GL(funcs->glFlush());
                        NX_GL(funcs->glFinish());
                    }

                    if (ini().displayAsyncSleepMs > 0)
                        usleep(ini().displayAsyncSleepMs * 1000);

                    self->doDisplayDecodedFrame(frameHandle);
                }
                else
                {
                    NX_OUTPUT << "displayDecodedFrame() execAtGlThreadAsync: already destroyed";
                }
            });
    }
    else
    {
        doDisplayDecodedFrame(frameHandle);
    }
}

void Impl::doDisplayDecodedFrame(void* frameHandle)
{
    QRect r = getVideoGeometry();

    const char* logPrefix = nullptr; //< Do not log by default.
    if (r != prevVideoGeometry)
    {
        if (r == QRect()) //< getVideoGeometry() did not provide a value.
        {
            if (prevVideoGeometry == kUndefinedVideoGeometry)
            {
                logPrefix = "VideoGeometry: Initially not provided =>";
                r = kFullscreenVideoGeometry;
            }
            else
            {
                logPrefix = "VideoGeometry: Not provided => use previous";
                r = prevVideoGeometry;
            }
        }
        else
        {
            logPrefix = "VideoGeometry:";
        }
    }

    prevVideoGeometry = r;

    if (r.isNull())
    {
        if (logPrefix)
        {
            NX_OUTPUT << logPrefix << " Fullscreen";
        }
        proxyDecoder().displayDecoded(frameHandle, /*rect*/ nullptr); //< Null mean fullscreen.
    }
    else
    {
        if (logPrefix)
        {
            NX_OUTPUT << logPrefix << nx::utils::stringFormat(" %d x %d @(%d, %d)",
                r.width(), r.height(), r.left(), r.top());
        }
        const ProxyDecoder::Rect rect{r.left(), r.top(), r.width(), r.height()};
        proxyDecoder().displayDecoded(frameHandle, &rect);
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderImpl* ProxyVideoDecoderImpl::createImplDisplay(const Params& params)
{
    NX_PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
