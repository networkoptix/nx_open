// Configuration
static const bool ENABLE_YUV_NATIVE = true; //< Use ProxyDecoder::decodeToYuvNative().

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

    std::shared_ptr<ProxyDecoder> m_proxyDecoder;

    QSize m_frameSize;

    // TODO mike: REMOVE
    bool m_once = false;
};

void ProxyVideoDecoderPrivate::initialize(const QSize& frameSize)
{
    QLOG("ProxyVideoDecoderPrivate<yuv>::initialize() BEGIN");
    NX_ASSERT(!m_initialized);
    m_initialized = true;

    m_frameSize = frameSize;
    m_proxyDecoder.reset(ProxyDecoder::create(m_frameSize.width(), m_frameSize.height()));

    QLOG("ProxyVideoDecoderPrivate<yuv>::initialize() END");
}

int ProxyVideoDecoderPrivate::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData, QVideoFramePtr* outDecodedFrame)
{
    NX_CRITICAL(outDecodedFrame);

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

    int64_t outPts = 0;
    int result = -1;

    if (ENABLE_YUV_NATIVE)
    {
        uint8_t* nativeBuffer = nullptr;
        int nativeBufferSize = 0;
        TIME_BEGIN("decodeToYuvNative")
        // Perform actual decoding from QnCompressedVideoData to memory.
        result = m_proxyDecoder->decodeToYuvNative(
            compressedFrame.get(), &outPts, &nativeBuffer, &nativeBufferSize);
        TIME_END
        
        if (result > 0)
        {
            NX_CRITICAL(nativeBuffer);
            NX_CRITICAL(nativeBufferSize > 0);

// TODO mike: REMOVE
#if 0
///memset(nativeBuffer, 0, nativeBufferSize);
//for (int i = 0; i < nativeBufferSize; ++i)
//    nativeBuffer[i] = rand();
static uint8_t* tempBuf = (uint8_t*) malloc(1920 * 1080 * 4 + 1000);
if (!m_once)
{
    m_once = true;
    memcpy(tempBuf, nativeBuffer, nativeBufferSize);
}
nativeBuffer = tempBuf;
#endif // 0

            const QVideoFrame::PixelFormat Format_Tiled32x32NV12 =
                QVideoFrame::PixelFormat(QVideoFrame::Format_User + 17);
  
            quint8* srcData[4];
            srcData[0] = (quint8*) nativeBuffer;
            srcData[1] = srcData[0] + qPower2Ceil(
                (unsigned) m_frameSize.width(), 32) * qPower2Ceil((unsigned) m_frameSize.height(), 32);

            int srcLineSize[4];
            srcLineSize[0] = srcLineSize[1] = qPower2Ceil((unsigned) m_frameSize.width(), 32);

            auto alignedBuffer = new AlignedMemVideoBuffer(srcData, srcLineSize, 2); //< two planes buffer
            outDecodedFrame->reset(new QVideoFrame(alignedBuffer, m_frameSize, Format_Tiled32x32NV12));
        }
    }
    else
    {
        const int alignedWidth = qPower2Ceil((unsigned) m_frameSize.width(), (unsigned) kMediaAlignment);
        const int numBytes = avpicture_get_size(PIX_FMT_YUV420P, alignedWidth, m_frameSize.height());
        const int lineSize = alignedWidth;

        auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, lineSize);

        outDecodedFrame->reset(new QVideoFrame(
            alignedBuffer, m_frameSize, QVideoFrame::Format_YUV420P));
        QVideoFrame* const decodedFrame = outDecodedFrame->get();

        decodedFrame->map(QAbstractVideoBuffer::WriteOnly);
        uchar* const buffer = decodedFrame->bits();

        uint8_t* const yBuffer = buffer;
        uint8_t* const uBuffer = buffer + lineSize * m_frameSize.height();
        uint8_t* const vBuffer = uBuffer + lineSize * m_frameSize.height() / 4;

        TIME_BEGIN("decodeToYuvPlanar")
        // Perform actual decoding from QnCompressedVideoData to QVideoFrame.
        result = m_proxyDecoder->decodeToYuvPlanar(compressedFrame.get(), &outPts,
            yBuffer, lineSize, uBuffer, vBuffer, lineSize / 2);
        TIME_END

        decodedFrame->unmap();
    }

    if (result <= 0) //< Error or no frame (buffering).
        outDecodedFrame->reset();
    else
        (*outDecodedFrame)->setStartTime(outPts);

    return result;
}
