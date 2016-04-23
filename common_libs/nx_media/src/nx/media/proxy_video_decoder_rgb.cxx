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
    bool m_initialized;

    std::unique_ptr<ProxyDecoder> m_proxyDecoder;

    QSize m_frameSize;
};

void ProxyVideoDecoderPrivate::initialize(const QSize& frameSize)
{
    QLOG("ProxyVideoDecoderPrivate<rgb>::initialize() BEGIN");
    NX_ASSERT(!m_initialized);
    m_initialized = true;

    m_frameSize = frameSize;
    m_proxyDecoder.reset(ProxyDecoder::create(m_frameSize.width(), m_frameSize.height()));

    QLOG("ProxyVideoDecoderPrivate<rgb>::initialize() END");
}

int ProxyVideoDecoderPrivate::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    showFps();

    NX_CRITICAL(outDecodedFrame);

    const int alignedWidth = qPower2Ceil((unsigned) m_frameSize.width(), (unsigned) kMediaAlignment);
    const int numBytes = avpicture_get_size(PIX_FMT_BGRA, alignedWidth, m_frameSize.height());
    const int argbLineSize = alignedWidth * 4;

    auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, argbLineSize);

    outDecodedFrame->reset(new QVideoFrame(alignedBuffer, m_frameSize, QVideoFrame::Format_RGB32));
    QVideoFrame* const decodedFrame = outDecodedFrame->get();

    decodedFrame->map(QAbstractVideoBuffer::WriteOnly);
    uchar* argbBuffer = decodedFrame->bits();

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

    int64_t outPts;

    int result;

    TIME_BEGIN("decodeToRgb")
    // Perform actual decoding from QnCompressedVideoData to QVideoFrame.
    result = m_proxyDecoder->decodeToRgb(compressedFrame.get(), &outPts, argbBuffer, argbLineSize);
    TIME_END

    decodedFrame->unmap();

    if (result <= 0) //< Error or no frame (buffering).
        outDecodedFrame->reset();
    else
        decodedFrame->setStartTime(outPts);

    return result;
}
