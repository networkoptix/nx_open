#include "proxy_video_decoder_private.h"
#if defined(ENABLE_PROXY_DECODER)

#define OUTPUT_PREFIX "ProxyVideoDecoder<display>: "
#include "proxy_video_decoder_utils.h"

#include "proxy_video_decoder_gl_utils.h"

namespace nx {
namespace media {

namespace {

class Impl
:
    public ProxyVideoDecoderPrivate
{
public:
    using ProxyVideoDecoderPrivate::ProxyVideoDecoderPrivate;

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;

private:
    class VideoBuffer;

private:
    void displayDecodedFrame(void* frameHandle);
};

class Impl::VideoBuffer
:
    public QAbstractVideoBuffer
{
public:
    VideoBuffer(
        void* frameHandle,
        const std::shared_ptr<ProxyVideoDecoderPrivate>& owner)
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
                NX_SHOW_FPS("handle");
                owner->displayDecodedFrame(m_frameHandle);
            }
            else
            {
                OUTPUT << "VideoBuffer::handle(): already destroyed";
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
    NX_TIME_BEGIN(decode);
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
    NX_TIME_END(decode);
    return result;
}

void Impl::displayDecodedFrame(void* frameHandle)
{
    if (conf.displayAsync)
    {
        auto selfPtr = std::weak_ptr<Impl>(std::dynamic_pointer_cast<Impl>(sharedPtrToThis()));
        allocator().execAtGlThreadAsync(
            [selfPtr, frameHandle]()
            {
                if (auto self = selfPtr.lock())
                {
                    if (conf.displayAsyncGlFinish)
                    {
                        GL_GET_FUNCS(QOpenGLContext::currentContext());
                        GL(funcs->glFlush());
                        GL(funcs->glFinish());
                    }

                    if (conf.displayAsyncSleepMs > 0)
                        usleep(conf.displayAsyncSleepMs * 1000);
                    // TODO: All-zeroes mean full-screen. Implement passing coords from QML.
                    self->proxyDecoder().displayDecoded(frameHandle, 0, 0, 0, 0);
                }
                else
                {
                    OUTPUT << "displayDecodedFrame() execAtGlThreadAsync: already destroyed";
                }
            });
    }
    else
    {
        // TODO: All-zeroes mean full-screen. Implement passing coords from QML.
        proxyDecoder().displayDecoded(frameHandle, 0, 0, 0, 0);
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderPrivate* ProxyVideoDecoderPrivate::createImplDisplay(const Params& params)
{
    PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
