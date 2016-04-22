#include "proxy_video_decoder.h"

class ProxyVideoDecoderPrivate
{
private:
    ProxyVideoDecoder* q;

public:
    ProxyVideoDecoderPrivate(ProxyVideoDecoder* owner)
    :
        q(owner),
        m_initialized(false),
        m_proxyDecoder(nullptr)
    {
    }

    ~ProxyVideoDecoderPrivate()
    {
    }

    void setAllocator(AbstractResourceAllocator* /*allocator*/)
    {
    }

    bool isInitialized()
    {
        return m_initialized;
    }

    /**
     * If initialization fails, fail with 'assert'.
     */
    void initialize(const QSize& frameSize);

    /**
     * @param compressedVideoData Has non-null data().
     * @return Value with the same semantics as AbstractVideoDecoder::decode().
     */
    int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame);

private:
    class VideoBuffer;

private:
    bool m_initialized;

    std::shared_ptr<ProxyDecoder> m_proxyDecoder;

    QSize m_frameSize;

private:
    void displayDecodedFrame(void* frameHandle);
};

class ProxyVideoDecoderPrivate::VideoBuffer
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
        m_owner(owner)
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
    std::weak_ptr<ProxyVideoDecoderPrivate> m_owner;
};

void ProxyVideoDecoderPrivate::initialize(const QSize& frameSize)
{
    QLOG("ProxyVideoDecoderPrivate<display>::initialize() BEGIN");
    NX_ASSERT(!m_initialized);
    m_initialized = true;

    m_frameSize = frameSize;
    m_proxyDecoder.reset(ProxyDecoder::create(m_frameSize.width(), m_frameSize.height()));

    QLOG("ProxyVideoDecoderPrivate<display>::initialize() END");
}

int ProxyVideoDecoderPrivate::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    NX_CRITICAL(outDecodedFrame);
    outDecodedFrame->reset();

    // TODO mike: Move to Private base class, as a common util.
    std::unique_ptr<ProxyDecoder::CompressedFrame> compressedFrame;
    if (compressedVideoData)
    {
        NX_CRITICAL(compressedVideoData->data());
        NX_CRITICAL(compressedVideoData->dataSize() > 0);
        compressedFrame.reset(new ProxyDecoder::CompressedFrame);
        compressedFrame->data = (const uint8_t*) compressedVideoData->data();
        compressedFrame->dataSize = compressedVideoData->dataSize();
        compressedFrame->pts = compressedVideoData->timestamp;
        compressedFrame->isKeyFrame =
            (compressedVideoData->flags & QnAbstractMediaData::MediaFlags_AVKey) != 0;
    }

    // Perform actual decoding from QnCompressedVideoData to display.
    void* frameHandle;
    int64_t outPts;
    int result = m_proxyDecoder->decodeToDisplayQueue(compressedFrame.get(), &outPts, &frameHandle);
    if (result <= 0) //< "Buffering".
        return result;
    if (!frameHandle)
        return 0;

    QAbstractVideoBuffer* videoBuffer = new VideoBuffer(frameHandle, q->d);
    outDecodedFrame->reset(new QVideoFrame(videoBuffer, m_frameSize, QVideoFrame::Format_BGR32));
    (*outDecodedFrame)->setStartTime(outPts);

    return result;
}

void ProxyVideoDecoderPrivate::displayDecodedFrame(void* frameHandle)
{
    m_proxyDecoder->displayDecoded(frameHandle);
}
