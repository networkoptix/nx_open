#include "proxy_video_decoder_private.h"
#if defined(ENABLE_PROXY_DECODER)

#include "proxy_video_decoder_utils.h"

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
                owner->displayDecodedFrame(m_frameHandle);
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
    NX_CRITICAL(outDecodedFrame);
    outDecodedFrame->reset();

    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    void* frameHandle = nullptr;
    int64_t outPts = 0;
    // Perform actual decoding from QnCompressedVideoData to display.
    int result = proxyDecoder().decodeToDisplayQueue(compressedFrame.get(), &outPts, &frameHandle);
    if (result <= 0) //< "Buffering".
        return result;
    if (!frameHandle)
        return 0;

    QAbstractVideoBuffer* videoBuffer = new VideoBuffer(frameHandle, sharedPtrToThis());
    outDecodedFrame->reset(new QVideoFrame(videoBuffer, frameSize(), QVideoFrame::Format_BGR32));
    (*outDecodedFrame)->setStartTime(outPts);

    return result;
}

void Impl::displayDecodedFrame(void* frameHandle)
{
    proxyDecoder().displayDecoded(frameHandle);
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderPrivate* ProxyVideoDecoderPrivate::createImplDisplay(const Params& params)
{
    qDebug() << "ProxyVideoDecoder: Using 'display' impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
