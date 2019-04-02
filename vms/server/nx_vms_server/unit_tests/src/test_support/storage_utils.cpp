#include "storage_utils.h"
#include <test_support/mediaserver_launcher.h>

#include <plugins/storage/file_storage/file_storage_resource.h>
#include <recorder/storage_manager.h>
#include <recorder/archive_integrity_watcher.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management//resource_pool.h>
#include <core/resource/avi/avi_archive_metadata.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/streaming/archive_stream_reader.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
}

namespace nx::test_support {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Test storage

static void waitForStorageToAppear(MediaServerLauncher* server, const QString& storagePath)
{
    while (true)
    {
        const auto allStorages = server->serverModule()->normalStorageManager()->getStorages();

        const auto testStorageIt = std::find_if(
            allStorages.cbegin(), allStorages.cend(),
            [&storagePath](const auto& storage) { return storage->getUrl() == storagePath; });

        if (testStorageIt == allStorages.cend())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        while (true) //< Waiting for initialization
        {
            const auto storage = *testStorageIt;
            const auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
            NX_ASSERT(fileStorage);
            fileStorage->setMounted(true);

            if (!(storage->isWritable() && storage->isOnline() && storage->isUsedForWriting()))
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            else
                break;
        }

        break;
    }
}

void addTestStorage(MediaServerLauncher* server, const QString& storagePath)
{
    NX_ASSERT(!storagePath.isEmpty());

    const auto existingStorages = server->serverModule()->resourcePool()->getResources<QnStorageResource>();
    const auto existingIt = std::find_if(
        existingStorages.cbegin(), existingStorages.cend(),
        [&storagePath](const auto& storage) { return storage->getUrl() == storagePath; });

    if (existingIt != existingStorages.cend())
        return;

    QnStorageResourcePtr storage(server->serverModule()->storagePluginFactory()->createStorage(
            server->commonModule(), "ufile"));

    storage->setName("Test storage");
    storage->setParentId(server->commonModule()->moduleGUID());
    storage->setUrl(storagePath);
    storage->setSpaceLimit(0);
    storage->setStorageType("local");
    storage->setUsedForWriting(storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());

    NX_ASSERT(storage->isUsedForWriting());

    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    if (resType)
        storage->setTypeId(resType->getId());

    QList<QnStorageResourcePtr> storages{ storage };
    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(QList<QnStorageResourcePtr>{storage}, apiStorages);

    const auto ec2Connection = server->serverModule()->ec2Connection();
    const auto saveResult =
        ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages);

    NX_ASSERT(ec2::ErrorCode::ok == saveResult);

    for (const auto &storage : storages)
    {
        server->serverModule()->commonModule()->messageProcessor()->updateResource(
            storage, ec2::NotificationSource::Local);
    }

    server->serverModule()->normalStorageManager()->initDone();
    waitForStorageToAppear(server, storagePath);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Test data generation

static const int64_t kMediaFileDurationMs = 60000;

static void replaceValue(QByteArray& payload, const QByteArray& key, const QByteArray& newValue)
{
    const int keyPos = payload.indexOf(key);
    NX_ASSERT(keyPos != -1);

    const int replacePos = keyPos + key.size() + 3;
    memcpy(payload.data() + replacePos, newValue.constData(), newValue.size());
}

static void updateMetaData(QByteArray& payload, int64_t startTimeMs)
{
    replaceValue(payload, "startTimeMs", QByteArray::number((qint64) startTimeMs));
    replaceValue(
        payload, "integrityHash",
        vms::server::IntegrityHashHelper::generateIntegrityHash(QByteArray::number((qint64) startTimeMs)).toBase64());
}

static ChunkDataList generateChunksForQuality(
    const QString& baseDir, const QString& cameraName, const QString& quality, qint64 startTimeMs,
    int count, QByteArray& payload)
{
    using namespace std::chrono;
    ChunkDataList result;
    for (int i = 0; i < count; ++i)
    {
        QString fileName = lit("%1_%2.mkv").arg(startTimeMs).arg(kMediaFileDurationMs);
        QString pathString = QnStorageManager::dateTimeStr(
            startTimeMs, currentTimeZone() / 60, "/");

        pathString = lit("%2/%1/%3").arg(cameraName).arg(quality).arg(pathString);
        auto fullDirPath = QDir(baseDir).absoluteFilePath(pathString);
        QString fullFileName =
            closeDirPath(baseDir) + closeDirPath(pathString) + fileName;

        updateMetaData(payload, startTimeMs);

        NX_ASSERT(QDir().mkpath(fullDirPath));
        QFile dataFile(fullFileName);
        NX_ASSERT(dataFile.open(QIODevice::WriteOnly));
        NX_ASSERT(dataFile.write(payload));

        result.push_back(ChunkData{ startTimeMs, kMediaFileDurationMs, fullFileName });
        startTimeMs += kMediaFileDurationMs + 5;
    }

    return result;
}

Catalog generateCameraArchive(
    const QString& baseDir, const QString& cameraName, qint64 startTimeMs, int count)
{
    QByteArray hiQualityPayLoad = createTestMkvFile(
        kMediaFileDurationMs / 1000, kHiQualityFileWidth, kHiQualityFileHeight);
    QByteArray lowQualityPayLoad = createTestMkvFile(
        kMediaFileDurationMs / 1000, kLowQualityFileWidth, kLowQualityFileHeight);

    Catalog result;
    result.lowQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "low_quality", startTimeMs, count, lowQualityPayLoad);

    result.highQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "hi_quality", startTimeMs, count, hiQualityPayLoad);

    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// MKV test file

static const int kBlockSize = 1024 * 1024;

static int ffmpegRead(void* userCtx, uint8_t* data, int size);
static int ffmpegWrite(void* userCtx, uint8_t* data, int size);
static int64_t ffmpegSeek(void* userCtx, int64_t pos, int whence);

class MkvBufferContext
{
public:
    MkvBufferContext(int width, int height)
    {
        allocateFfmpegObjects();
        setupCodecContext(width, height);
        setupStream();
        setupFrame();
        setupIoContext();
        setupMetadata();
        av_log_set_level(AV_LOG_QUIET);
    }

    std::vector<char> create(int lengthSec)
    {
        m_pos = 0;
        m_pts = 0;
        m_buffer.clear();

        if (avformat_write_header(m_formatContext, NULL) < 0)
            throw std::runtime_error("Write failed: avformat_write_header");

        for (int i = 0; i < lengthSec; ++i)
            writeVideoFrame();

        if (av_write_trailer(m_formatContext) < 0)
            throw std::runtime_error("Write failed: av_write_trailer");

        return m_buffer;
    }

    ~MkvBufferContext()
    {
        destroyIOContext();
        avformat_free_context(m_formatContext);
        avcodec_free_context(&m_codecContext);
        av_frame_free(&m_frame);
        av_packet_free(&m_packet);
    }

private:
    friend int ffmpegRead(void *userCtx, uint8_t *data, int size);
    friend int ffmpegWrite(void *userCtx, uint8_t *data, int size);
    friend int64_t ffmpegSeek(void *userCtx, int64_t pos, int whence);

    const AVCodec* m_codec = nullptr;
    AVCodecContext* m_codecContext = nullptr;
    AVFormatContext* m_formatContext = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
    int m_pos = 0;
    std::vector<char> m_buffer;
    int64_t m_pts = 0;

    void allocateFfmpegObjects()
    {
        if (!(m_codec = avcodec_find_encoder_by_name("h263p")))
            throw std::runtime_error("Failed to initialize ffmpeg: codec");

        if (!(m_codecContext = avcodec_alloc_context3(m_codec)))
            throw std::runtime_error("Failed to initialize ffmpeg: codec context");

        if (!(m_packet = av_packet_alloc()))
            throw std::runtime_error("Failed to initialize ffmpeg: packet");

        if (!(m_frame = av_frame_alloc()))
            throw std::runtime_error("Failed to initialize ffmpeg: frame");

        if (avformat_alloc_output_context2(&m_formatContext, nullptr, "matroska", nullptr) < 0)
            throw std::runtime_error("Failed to initialize ffmpeg: alloc_output_context");
    }

    void setupMetadata()
    {
        QnAviArchiveMetadata metadata;
        metadata.startTimeMs = 1553604378060;
        metadata.version = QnAviArchiveMetadata::kIntegrityCheckVersion;
        metadata.integrityHash =
            vms::server::IntegrityHashHelper::generateIntegrityHash("1553604378060");
        metadata.saveToFile(m_formatContext, QnAviArchiveMetadata::Format::custom);
    }

    void setupCodecContext(int width, int height)
    {
        m_codecContext->bit_rate = 400000;
        m_codecContext->width = width;
        m_codecContext->height = height;
        m_codecContext->time_base = { 1, 25 };
        m_codecContext->framerate = { 25, 1 };
        m_codecContext->gop_size = 10;
        m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

        if (avcodec_open2(m_codecContext, m_codec, NULL) < 0)
            throw std::runtime_error("Failed to initialize ffmpeg: avcodec_open2");
    }

    void setupFrame()
    {
        m_frame->format = m_codecContext->pix_fmt;
        m_frame->width = m_codecContext->width;
        m_frame->height = m_codecContext->height;
        if (av_frame_get_buffer(m_frame, 64) < 0)
            throw std::runtime_error("Failed to initialize ffmpeg: av_frame_get_buffer");
    }

    void setupStream()
    {
        AVStream* outputStream = avformat_new_stream(m_formatContext, m_codec);
        if (!outputStream)
            throw std::runtime_error("Failed to initialize ffmpeg: avformat_new_stream");

        if (avcodec_parameters_from_context(outputStream->codecpar, m_codecContext) < 0)
            throw std::runtime_error("Failed to initialize ffmpeg: avcodec_parameters_from_context");
    }

    void setupIoContext()
    {
        uint8_t* ioBuffer;
        ioBuffer = (uint8_t*)av_malloc(kBlockSize);
        m_formatContext->pb = avio_alloc_context(
            ioBuffer, kBlockSize, 1, this, &ffmpegRead, &ffmpegWrite, &ffmpegSeek);
        if (!m_formatContext->pb)
            throw std::runtime_error("Failed to initialize ffmpeg: avio_alloc_context");
    }

    void destroyIOContext()
    {
        if (m_formatContext->pb)
        {
            avio_flush(m_formatContext->pb);
            m_formatContext->pb->opaque = 0;
            av_freep(&m_formatContext->pb->buffer);
            av_opt_free(m_formatContext->pb);
            av_free(m_formatContext->pb);
        }
    }

    void writeVideoFrame()
    {
        for (int i = 0; i < 25; i++)
        {
            if (av_frame_make_writable(m_frame) < 0)
                throw std::runtime_error("Write frame failed: av_frame_make_writable");

            for (int y = 0; y < m_codecContext->height; y++)
            {
                for (int x = 0; x < m_codecContext->width; x++)
                    m_frame->data[0][y * m_frame->linesize[0] + x] = x + y + i * 3;
            }

            for (int y = 0; y < m_codecContext->height / 2; y++)
            {
                for (int x = 0; x < m_codecContext->width / 2; x++)
                {
                    m_frame->data[1][y * m_frame->linesize[1] + x] = 128 + y + i * 2;
                    m_frame->data[2][y * m_frame->linesize[2] + x] = 64 + x + i * 5;
                }
            }

            m_frame->pts = m_pts++;
            if (avcodec_send_frame(m_codecContext, m_frame) < 0)
                throw std::runtime_error("Write frame failed: avcodec_send_frame");

            while (true)
            {
                int result = avcodec_receive_packet(m_codecContext, m_packet);
                if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
                    continue;
                else if (result < 0)
                    throw std::runtime_error("Write frame failed: avcodec_receive_packet");

                const auto codecContextTimeBase = m_codecContext->time_base;
                const auto streamTimeBase = m_formatContext->streams[0]->time_base;

                m_packet->pts = av_rescale_q(m_packet->pts, codecContextTimeBase, streamTimeBase);
                m_packet->dts = av_rescale_q(m_packet->dts, codecContextTimeBase, streamTimeBase);
                if (av_write_frame(m_formatContext, m_packet) < 0)
                    throw std::runtime_error("Write frame failed: av_write_frame");

                break;
            }
        }
    }
};

static int ffmpegRead(void* userCtx, uint8_t* data, int size)
{
    MkvBufferContext* mkvContext = (MkvBufferContext*)userCtx;
    const auto& buffer = mkvContext->m_buffer;

    int result = std::min<int>(size, (int) buffer.size() - mkvContext->m_pos);
    memcpy(data, buffer.data(), result);
    mkvContext->m_pos += result;

    return result;
}

static int ffmpegWrite(void* userCtx, uint8_t* data, int size)
{
    MkvBufferContext* mkvContext = (MkvBufferContext*)userCtx;
    auto& buffer = mkvContext->m_buffer;
    const auto resizeValue = mkvContext->m_pos + size;
    if (resizeValue > buffer.size())
        buffer.resize(resizeValue);

    memcpy(buffer.data() + mkvContext->m_pos, data, size);
    mkvContext->m_pos += size;

    return size;
}

static int64_t ffmpegSeek(void* userCtx, int64_t pos, int whence)
{
    MkvBufferContext* mkvContext = (MkvBufferContext*)userCtx;
    int64_t absolutePos = pos;
    switch (whence)
    {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            absolutePos = mkvContext->m_pos + pos;
            break;
        case SEEK_END:
            absolutePos = mkvContext->m_buffer.size() + pos;
            break;
        case 65536:
            return mkvContext->m_buffer.size();
        default:
            return AVERROR(ENOENT);
    }

    return (mkvContext->m_pos = absolutePos);
}

QByteArray createTestMkvFile(int lengthSec, int width, int height)
{
    try
    {
        const auto result = MkvBufferContext(width, height).create(lengthSec);
        return QByteArray(result.data(), (int) result.size());
    }
    catch (const std::exception& e)
    {
        qWarning() << "createTestMkvFile failed:" << e.what();
        return QByteArray();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Camera archive RTSP reader

std::unique_ptr<QnArchiveStreamReader> createArchiveStreamReader(
    const QnVirtualCameraResourcePtr& camera)
{
    std::unique_ptr<QnArchiveStreamReader> archiveReader =
        std::make_unique<QnArchiveStreamReader>(camera);

    const auto rtspArchiveDelegate = new QnRtspClientArchiveDelegate(archiveReader.get());
    rtspArchiveDelegate->setStreamDataFilter(vms::api::StreamDataFilter::media);
    rtspArchiveDelegate->setCamera(camera);

    archiveReader->setArchiveDelegate(rtspArchiveDelegate);
    archiveReader->setPlaybackRange(QnTimePeriod(0, std::numeric_limits<int64_t>::max()));

    return archiveReader;
}

} // namespace nx::test_support