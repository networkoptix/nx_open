
#include <nx/utils/log/log.h>
#include <nx/media/quick_sync/quick_sync_video_decoder.h>

QnConstCompressedVideoDataPtr loadFrameFromFile(const char* filename)
{
    constexpr int kMaxFrameSzie = 1024 * 1024 * 10;
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        NX_DEBUG(NX_SCOPE_TAG, "Invalid file %1", filename);
        return nullptr;
    }

    int size = file.tellg();
    if (size > kMaxFrameSzie || size <= 0)
    {
        NX_DEBUG(NX_SCOPE_TAG,
            "Invalid file size %1, it should be less than: %2", size, kMaxFrameSzie);
        return nullptr;
    }

    auto result = QnWritableCompressedVideoDataPtr(
        new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, size));
    result->compressionType = AV_CODEC_ID_H264;
    file.seekg(0, std::ios::beg);
    file.read(result->m_data.data(), size);
    result->m_data.resize(size);
    return result;
}

int main()
{
    pthread_attr_t attr;
    size_t stacksize;
    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &stacksize);
    printf("Thread stack size = %d bytes \n", stacksize);

    NX_DEBUG(NX_SCOPE_TAG, "QQQQQQQQQQQQQQQQQQQQQQQQQQQQ");
    nx::media::QuickSyncVideoDecoder decode(nullptr, QSize());
    auto frame = loadFrameFromFile("frame.h264");
    if (!frame)
        return 1;
    //while(true)
    for (int i = 0; i < 10; ++i)
    {
        nx::QVideoFramePtr result;
        int status = decode.decode(frame, &result);
        NX_DEBUG(NX_SCOPE_TAG, "QQQQQQ decode.decode status %1", status);
        if (status != 0)
            return 1;
    }
    return 0;
}

