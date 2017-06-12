#include "proxy_video_decoder_impl.h"
#if defined(ENABLE_PROXY_DECODER)

#include <nx/kit/debug.h>

#include <nx/media/aligned_mem_video_buffer.h>

#include "proxy_video_decoder_utils.h"

namespace nx {
namespace media {

namespace {

class Impl: public ProxyVideoDecoderImpl
{
public:
    using ProxyVideoDecoderImpl::ProxyVideoDecoderImpl;

    virtual int decode(
        const QnConstCompressedVideoDataPtr& compressedVideoData,
        QVideoFramePtr* outDecodedFrame) override;

private:
    class VideoBuffer;
};

class Impl::VideoBuffer: public AlignedMemVideoBuffer
{
public:
    VideoBuffer(
        uchar* data[4], int bytesPerLine[4], int planeCount,
        const std::shared_ptr<ProxyVideoDecoderImpl>& owner)
        :
        AlignedMemVideoBuffer(data, bytesPerLine, planeCount),
        m_owner(std::dynamic_pointer_cast<Impl>(owner))
    {
    }

    ~VideoBuffer() override {}

    virtual uchar* map(MapMode /*mode*/, int* /*numBytes*/, int* /*bytesPerLine*/) override
    {
        // Should never be called - the buffer is multi-plane, and this method is for single-plane.
        NX_OUTPUT << "INTERNAL ERROR: VideoBuffer::map(/*single_plane*/) called.";
        return nullptr;
    }

    virtual void unmap() override
    {
        m_ownerLocked.reset();
    }

    virtual QVariant handle() const override
    {
        if (auto owner = m_owner.lock()) //< Owner exists.
        {
            return AlignedMemVideoBuffer::handle();
        }
        else
        {
            NX_OUTPUT << "VideoBuffer::handle(): already destroyed";
            return 0;
        }
    }

protected:
    int doMapPlanes(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) override
    {
        m_ownerLocked = m_owner.lock();
        if (m_ownerLocked) //< Owner exists.
        {
            return AlignedMemVideoBuffer::doMapPlanes(mode, numBytes, bytesPerLine, data);
        }
        else
        {
            NX_OUTPUT << "VideoBuffer::doMapPlanes(): already destroyed";
            return 0;
        }
    }

private:
    std::weak_ptr<Impl> m_owner;
    std::shared_ptr<Impl> m_ownerLocked;
};

int Impl::decode(
    const QnConstCompressedVideoDataPtr& compressedVideoData,
    QVideoFramePtr* outDecodedFrame)
{
    NX_CRITICAL(outDecodedFrame);

    uint8_t* nativeBuffer = nullptr;
    int nativeBufferSize = 0;

    auto compressedFrame = createUniqueCompressedFrame(compressedVideoData);
    int64_t ptsUs = 0;
    NX_TIME_BEGIN(DecodeToYuvNative);
    // Perform actual decoding to video memory and retrieve the pointer to it.
    int result = proxyDecoder().decodeToYuvNative(
        compressedFrame.get(), &ptsUs, &nativeBuffer, &nativeBufferSize);
    NX_TIME_END(DecodeToYuvNative);

    if (result > 0)
    {
        NX_CRITICAL(nativeBuffer);
        NX_CRITICAL(nativeBufferSize > 0);

        quint8* srcData[4];
        srcData[0] = (quint8*) nativeBuffer;
        srcData[1] = srcData[0] + qPower2Ceil(
            (unsigned int) frameSize().width(), 32)
            * qPower2Ceil((unsigned int) frameSize().height(), 32);

        int srcLineSize[4];
        srcLineSize[0] = srcLineSize[1] = qPower2Ceil((unsigned int) frameSize().width(), 32);

        auto videoBuffer = new VideoBuffer(
            srcData, srcLineSize, /*planeCount*/ 2, sharedPtrToThis());

        static const QVideoFrame::PixelFormat kFormat_Tiled32x32NV12 =
            QVideoFrame::PixelFormat(QVideoFrame::Format_User + 17);

        setQVideoFrame(outDecodedFrame, videoBuffer, kFormat_Tiled32x32NV12, ptsUs);
    }
    else
    {
        // Error or no frame (buffering).
        outDecodedFrame->reset();
    }

    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyVideoDecoderImpl* ProxyVideoDecoderImpl::createImplYuvNative(const Params& params)
{
    NX_PRINT << "Using this impl";
    return new Impl(params);
}

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
