// Configuration
static const bool ENABLE_YUV_NATIVE = false; //< Use ProxyDecoder::decodeToYuvNative().

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
    void initialize(const QnConstCompressedVideoDataPtr& compressedVideoData);

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

void ProxyVideoDecoderPrivate::initialize(const QnConstCompressedVideoDataPtr& compressedVideoData)
{
    QLOG("ProxyVideoDecoderPrivate<yuv>::initialize() BEGIN");
    NX_ASSERT(!m_initialized);
    NX_CRITICAL(compressedVideoData);
    NX_CRITICAL(compressedVideoData->data());
    NX_CRITICAL(compressedVideoData->dataSize() > 0);
    m_initialized = true;

    // TODO mike: Remove when frame size is passed from SeamlessVideoDecoder.
    extractSpsPps(compressedVideoData, &m_frameSize, nullptr);

    m_proxyDecoder.reset(new ProxyDecoder(m_frameSize.width(), m_frameSize.height()));

    QLOG("ProxyVideoDecoderPrivate<yuv>::initialize() END");
}

int ProxyVideoDecoderPrivate::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    showFps();

    NX_CRITICAL(outDecodedFrame);

    const int alignedWidth = qPower2Ceil((unsigned) m_frameSize.width(), (unsigned) kMediaAlignment);
    const int numBytes = avpicture_get_size(PIX_FMT_YUV420P, alignedWidth, m_frameSize.height());
    const int lineSize = alignedWidth;

    auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, lineSize);

    outDecodedFrame->reset(new QVideoFrame(
        alignedBuffer, m_frameSize, QVideoFrame::Format_YUV420P));
    QVideoFrame* const decodedFrame = outDecodedFrame->get();

    decodedFrame->map(QAbstractVideoBuffer::WriteOnly);
    uchar* const buffer = decodedFrame->bits();

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

    uint8_t* const yBuffer = buffer;
    uint8_t* const uBuffer = buffer + lineSize * m_frameSize.height();
    uint8_t* const vBuffer = uBuffer + lineSize * m_frameSize.height() / 4;

    int64_t outPts = 0;
    int result = -1;

    if (ENABLE_YUV_NATIVE)
    {
        // No complete impl yet exists for native YUV.
        uint8_t* nativeBuffer = nullptr;
        int nativeBufferSize = 0;
        TIME_BEGIN("decodeToYuvNative")
        // Perform actual decoding from QnCompressedVideoData to memory.
        result = m_proxyDecoder->decodeToYuvNative(compressedFrame.get(), &outPts,
            &nativeBuffer, &nativeBufferSize);
        TIME_END

        //if (result > 0)
        //    memcpy(buffer, nativeBuffer, m_frameSize.width() * m_frameSize.height());
    }
    else
    {
        TIME_BEGIN("decodeToYuvPlanar")
        // Perform actual decoding from QnCompressedVideoData to QVideoFrame.
        result = m_proxyDecoder->decodeToYuvPlanar(compressedFrame.get(), &outPts,
            yBuffer, lineSize, uBuffer, vBuffer, lineSize / 2);
        TIME_END
    }

    decodedFrame->unmap();

    if (result <= 0) //< Error or no frame (buffering).
        outDecodedFrame->reset();
    else
        decodedFrame->setStartTime(outPts);

    return result;
}
