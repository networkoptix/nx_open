#include <gtest/gtest.h>

#include "storage_utils.h"
#include <test_support/mediaserver_launcher.h>

#include <plugins/storage/file_storage/file_storage_resource.h>
#include <recorder/storage_manager.h>
#include <recorder/archive_integrity_watcher.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/avi/avi_archive_metadata.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/streaming/archive_stream_reader.h>
#include <media_server/media_server_module.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <decoders/video/ffmpeg_video_decoder.h>
}

namespace nx::vms::server::test::test_support {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Test storage

namespace {

class TestFileStorage: public QnFileStorageResource
{
public:
    using QnFileStorageResource::QnFileStorageResource;

    static QnStorageResourcePtr create(MediaServerLauncher* server, const QString& path)
    {
        QnStorageResourcePtr storage(new TestFileStorage(server->serverModule()));

        storage->setName("Test storage");
        storage->setParentId(server->commonModule()->moduleGUID());
        storage->setUrl(path);
        storage->setSpaceLimit(0);
        storage->setStorageType("local");
        storage->setUsedForWriting(
            storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());
        storage->setIdUnsafe(QnUuid::createUuid());

        NX_ASSERT(storage->isUsedForWriting());

        QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
        if (resType)
            storage->setTypeId(resType->getId());

        return storage;
    }

private:
    virtual bool isMounted() const override { return true; }
};

static bool storagePresent(MediaServerLauncher* server, const QString& storagePath)
{
    const auto existingStorages =
        server->serverModule()->resourcePool()->getResources<QnStorageResource>();

    const auto existingIt = std::find_if(
        existingStorages.cbegin(),
        existingStorages.cend(),
        [&storagePath](const auto& storage)
        {
            return storage->getUrl() == storagePath;
        });

    if (existingIt != existingStorages.cend())
        return true;

    return false;
}

} // namespace

void addStorage(MediaServerLauncher* server, const QString& path)
{
    NX_ASSERT(!path.isEmpty());
    if (storagePresent(server, path))
    {
        NX_DEBUG(
            typeid(TestFileStorage),
            "Storage %1 is already present. Won't be added second time",
            path);
    }

    server->serverModule()->normalStorageManager()->onNewResource(
        TestFileStorage::create(server, path));
    server->serverModule()->normalStorageManager()->initDone();

    NX_DEBUG(
        typeid(TestFileStorage),
        "Storage %1 has been successfully added to the mediaserver's storage pool",
        path);
}

class StorageFixtureResource: public StorageResource
{
public:
    StorageFixtureResource(
        QnMediaServerModule* serverModule, const QString& url, int64_t totalSpace,
        int64_t freeSpace, int64_t spaceLimit, bool isSystem, bool isOnline, bool isUsedForWriting)
        :
        StorageResource(serverModule), m_url(url), m_totalSpace(totalSpace),
        m_freeSpace(freeSpace), m_isSystem(isSystem), m_isOnline(isOnline)
    {
        setSpaceLimit(spaceLimit);
        setUsedForWriting(isUsedForWriting);
        setIdUnsafe(QnUuid::createUuid());
    }

    virtual QIODevice* openInternal(const QString& /*fileName*/, QIODevice::OpenMode /*openMode*/) override
    {
        return nullptr;
    }

    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& /*dirName*/) override
    {
        return QnAbstractStorageResource::FileInfoList();
    }

    virtual qint64 getFileSize(const QString& /*url*/) const override
    {
        return -1LL;
    }

    virtual bool removeFile(const QString& /*url*/) override
    {
        return true;
    }

    virtual bool removeDir(const QString& /*url*/) override
    {
        return true;
    }

    virtual bool renameFile(const QString& /*oldName*/, const QString& /*newName*/) override
    {
        return true;
    }

    virtual bool isFileExists(const QString& /*url*/) override
    {
        return true;
    }

    virtual bool isDirExists(const QString& /*url*/) override
    {
        return true;
    }

    virtual qint64 getFreeSpace() override
    {
        return m_freeSpace;
    }

    virtual qint64 getTotalSpace() const override
    {
        return m_totalSpace;
    }

    virtual int getCapabilities() const override
    {
        return ListFile | RemoveFile | ReadFile | WriteFile | DBReady;
    }

    virtual Qn::StorageInitResult initOrUpdate() override
    {
        return Qn::StorageInit_Ok;
    }

    virtual void setUrl(const QString& url) override
    {
        m_url = url;
    }

    virtual QString getUrl() const override
    {
        return m_url;
    }

    virtual bool isSystem() const override
    {
        return m_isSystem;
    }

    virtual QString getPath() const override
    {
        return QnStorageResource::getPath();
    }

    virtual Qn::ResourceStatus getStatus() const override
    {
        return m_isOnline ? Qn::ResourceStatus::Online : Qn::ResourceStatus::Offline;
    }

private:
    QString m_url;
    int64_t m_totalSpace = -1;
    int64_t m_freeSpace = -1;
    bool m_isSystem = false;
    bool m_isOnline = false;
};

StorageResourcePtr addStorageFixture(
    MediaServerLauncher* server, QnMediaServerModule* serverModule, const QString& url,
    int64_t totalSpace, int64_t freeSpace, int64_t spaceLimit, bool isSystem, bool isOnline,
    bool isUsedForWriting)
{
    auto storage = StorageResourcePtr(new StorageFixtureResource(
        serverModule, url, totalSpace, freeSpace, spaceLimit, isSystem, isOnline, isUsedForWriting));
    server->serverModule()->normalStorageManager()->addStorage(storage);
    return storage;
}

vms::server::ChunksDeque generateChunks(
    int storageIndex, int64_t startTimeMs, int32_t chunkSize, int64_t spaceToFill)
{
    int64_t totalSpaceFilled = 0LL;
    vms::server::ChunksDeque chunks;
    const int kDurationMs = 60*1000;
    while (totalSpaceFilled < spaceToFill)
    {
        chunks.push_back(Chunk(
            startTimeMs, storageIndex, vms::server::Chunk::FILE_INDEX_WITH_DURATION, kDurationMs,
            0, 0, chunkSize));
        startTimeMs += kDurationMs;
        totalSpaceFilled += chunkSize;
    }
    return chunks;
}

void setNxOccupiedSpace(
    MediaServerLauncher* server, const QnStorageResourcePtr& storage, int64_t nxOccupiedSpace)
{
    const auto storageManager = server->serverModule()->normalStorageManager();
    const int storageIndex = storageManager->storageDbPool()->getStorageIndex(storage);
    NX_ASSERT(storageIndex >= 0);
    const auto fileCatalog = storageManager->getFileCatalog(
        "camera-unique-id", QnServer::ChunksCatalog::LowQualityCatalog);
    fileCatalog->addChunks(generateChunks(storageIndex, 0LL, 10*1024*1024, nxOccupiedSpace));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Test data generation

static void replaceValue(QByteArray& payload, const QByteArray& key, const QByteArray& newValue)
{
    const int keyPos = payload.indexOf(key);
    NX_ASSERT(keyPos != -1);

    const int replacePos = keyPos + key.size() + 3;
    memcpy(payload.data() + replacePos, newValue.constData(), newValue.size());
}

void updateMkvMetaData(QByteArray& payload, int64_t startTimeMs)
{
    replaceValue(payload, "startTimeMs", QByteArray::number((qint64) startTimeMs));
    replaceValue(payload,
        "integrityHash",
        vms::server::IntegrityHashHelper::generateIntegrityHash(
            QByteArray::number((qint64) startTimeMs))
            .toBase64());
}

static ChunkDataList generateChunksForQuality(const QString& baseDir,
    const QString& cameraName,
    const QString& quality,
    qint64 startTimeMs,
    int count,
    QByteArray& payload)
{
    using namespace std::chrono;
    ChunkDataList result;
    for (int i = 0; i < count; ++i)
    {
        QString fileName = lit("%1_%2.mkv").arg(startTimeMs).arg(kMediaFileDurationMs);
        QString pathString =
            QnStorageManager::dateTimeStr(startTimeMs, currentTimeZone() / 60, "/");

        pathString = lit("%2/%1/%3").arg(cameraName).arg(quality).arg(pathString);
        auto fullDirPath = QDir(baseDir).absoluteFilePath(pathString);
        QString fullFileName = closeDirPath(baseDir) + closeDirPath(pathString) + fileName;

        updateMkvMetaData(payload, startTimeMs);

        NX_ASSERT(QDir().mkpath(fullDirPath));
        QFile dataFile(fullFileName);
        NX_ASSERT(dataFile.open(QIODevice::WriteOnly));
        NX_ASSERT(dataFile.write(payload));

        result.push_back(ChunkData{startTimeMs, kMediaFileDurationMs, fullFileName});
        startTimeMs += kMediaFileDurationMs + kMediaFilesGapMs;
    }

    return result;
}

Catalog generateCameraArchive(
    const QString& baseDir, const QString& cameraName, qint64 startTimeMs, int count)
{
    QByteArray hiQualityPayLoad = createTestMkvFileData(
        kMediaFileDurationMs / 1000, kHiQualityFileWidth, kHiQualityFileHeight);
    QByteArray lowQualityPayLoad = createTestMkvFileData(
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
static const int kdrawPixelSize = 16;

static void drawTimestampOnFrame(AVFrame* frame, uint64_t timestamp)
{
    NX_ASSERT(frame->linesize[0] >= kdrawPixelSize * 8);
    NX_ASSERT(frame->height >= kdrawPixelSize * 8);

    uint64_t bitMask = 1;
    for (int bitNum = 0; bitNum < 64; ++bitNum)
    {
        uint8_t color = (timestamp & bitMask) ? 0xff : 0x00;
        int posX = (bitNum % 8) * kdrawPixelSize;
        int posY = (bitNum / 8) * kdrawPixelSize;
        uint8_t* data = frame->data[0] + posY * frame->linesize[0] + posX;
        for (int y = 0; y < kdrawPixelSize; ++y)
        {
            for (int x = 0; x < kdrawPixelSize; ++x)
                data[y * frame->linesize[0] + x] = color;
        }
        bitMask <<= 1;
    }
}

static uint64_t getTimestampFromFrame(const AVFrame* frame)
{
    NX_ASSERT(frame->linesize[0] >= kdrawPixelSize * 8);
    NX_ASSERT(frame->height >= kdrawPixelSize * 8);

    uint64_t result = 0;
    uint64_t bitMask = 1;
    for (int bitNum = 0; bitNum < 64; ++bitNum)
    {
        int posX = (bitNum % 8) * kdrawPixelSize;
        int posY = (bitNum / 8) * kdrawPixelSize;
        uint8_t* data = frame->data[0] + posY * frame->linesize[0] + posX;
        int color = 0;
        for (int y = 0; y < kdrawPixelSize; ++y)
        {
            for (int x = 0; x < kdrawPixelSize; ++x)
                color += data[y * frame->linesize[0] + x];
        }
        float avarageColor = color / (float) (kdrawPixelSize * kdrawPixelSize);
        if (avarageColor >= 128.0)
            result |= bitMask;

        bitMask <<= 1;
    }

    return result;
}

class MkvBufferContext
{
public:
    MkvBufferContext(
        int width, int height, const QString& container, const QString& codec)
    {
        allocateFfmpegObjects(container, codec);
        setupCodecContext(width, height);
        setupStream();
        setupFrame();
        setupIoContext();
        setupMetadata(formatFromString(container));
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
    friend int ffmpegRead(void* userCtx, uint8_t* data, int size);
    friend int ffmpegWrite(void* userCtx, uint8_t* data, int size);
    friend int64_t ffmpegSeek(void* userCtx, int64_t pos, int whence);

    const AVCodec* m_codec = nullptr;
    AVCodecContext* m_codecContext = nullptr;
    AVFormatContext* m_formatContext = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
    int m_pos = 0;
    std::vector<char> m_buffer;
    int64_t m_pts = 0;

    void allocateFfmpegObjects(const QString& container, const QString& codec)
    {
        if (!(m_codec = avcodec_find_encoder_by_name(codec.toLatin1().constData())))
            throw std::runtime_error("Failed to initialize ffmpeg: codec");

        if (!(m_codecContext = avcodec_alloc_context3(m_codec)))
            throw std::runtime_error("Failed to initialize ffmpeg: codec context");

        if (!(m_packet = av_packet_alloc()))
            throw std::runtime_error("Failed to initialize ffmpeg: packet");

        if (!(m_frame = av_frame_alloc()))
            throw std::runtime_error("Failed to initialize ffmpeg: frame");

        if (avformat_alloc_output_context2(
                &m_formatContext, nullptr, container.toLatin1().constData(), nullptr) < 0)
        {
            throw std::runtime_error("Failed to initialize ffmpeg: alloc_output_context");
        }
    }

    void setupMetadata(QnAviArchiveMetadata::Format format)
    {
        QnAviArchiveMetadata metadata;
        metadata.startTimeMs = 1553604378060;
        metadata.version = QnAviArchiveMetadata::kIntegrityCheckVersion;
        metadata.integrityHash =
            vms::server::IntegrityHashHelper::generateIntegrityHash("1553604378060");
        metadata.saveToFile(m_formatContext, format);
    }

    void setupCodecContext(int width, int height)
    {
        m_codecContext->bit_rate = 400000;
        m_codecContext->width = width;
        m_codecContext->height = height;
        m_codecContext->time_base = {1, 25};
        m_codecContext->framerate = {25, 1};
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
            throw std::runtime_error(
                "Failed to initialize ffmpeg: avcodec_parameters_from_context");
    }

    void setupIoContext()
    {
        uint8_t* ioBuffer;
        ioBuffer = static_cast<uint8_t*>(av_malloc(kBlockSize));
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
            m_formatContext->pb->opaque = nullptr;
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

            const auto codecContextTimeBase = m_codecContext->time_base;
            const auto streamTimeBase = m_formatContext->streams[0]->time_base;

            auto timestampMs = av_rescale_q(m_frame->pts, codecContextTimeBase, {1, 1000});
            drawTimestampOnFrame(m_frame, timestampMs);
            auto writtenTimestamp = getTimestampFromFrame(m_frame);
            ASSERT_EQ(timestampMs, writtenTimestamp);

            if (avcodec_send_frame(m_codecContext, m_frame) < 0)
                throw std::runtime_error("Write frame failed: avcodec_send_frame");

            while (true)
            {
                int result = avcodec_receive_packet(m_codecContext, m_packet);
                if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
                    continue;
                else if (result < 0)
                    throw std::runtime_error("Write frame failed: avcodec_receive_packet");

                m_packet->pts = av_rescale_q(m_packet->pts, codecContextTimeBase, streamTimeBase);
                m_packet->dts = av_rescale_q(m_packet->dts, codecContextTimeBase, streamTimeBase);
                if (av_write_frame(m_formatContext, m_packet) < 0)
                    throw std::runtime_error("Write frame failed: av_write_frame");

                break;
            }
        }
    }

    static QnAviArchiveMetadata::Format formatFromString(const QString& format)
    {
        if (format == "avi")
            return QnAviArchiveMetadata::Format::avi;
        if (format == "mp4")
            return QnAviArchiveMetadata::Format::mp4;

        return QnAviArchiveMetadata::Format::custom;
    }
};

static int ffmpegRead(void* userCtx, uint8_t* data, int size)
{
    MkvBufferContext* mkvContext = (MkvBufferContext*) userCtx;
    const auto& buffer = mkvContext->m_buffer;

    int result = std::min<int>(size, (int) buffer.size() - mkvContext->m_pos);
    memcpy(data, buffer.data(), result);
    mkvContext->m_pos += result;

    return result;
}

static int ffmpegWrite(void* userCtx, uint8_t* data, int size)
{
    MkvBufferContext* mkvContext = (MkvBufferContext*) userCtx;
    auto& buffer = mkvContext->m_buffer;
    const auto resizeValue = mkvContext->m_pos + size;
    if (resizeValue > (int) buffer.size())
        buffer.resize(resizeValue);

    memcpy(buffer.data() + mkvContext->m_pos, data, size);
    mkvContext->m_pos += size;

    return size;
}

static int64_t ffmpegSeek(void* userCtx, int64_t pos, int whence)
{
    MkvBufferContext* mkvContext = (MkvBufferContext*) userCtx;
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

QByteArray createTestMkvFileData(
    int lengthSec, int width, int height, const QString& container, const QString& codec)
{
    try
    {
        const auto result = MkvBufferContext(width, height, container, codec).create(lengthSec);
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

QnVirtualCameraResourcePtr getCamera(QnResourcePool* resourcePool, const QString& uniqueIdOrName)
{
    const auto allCameras = resourcePool->getAllCameras();
    QnVirtualCameraResourcePtr result;
    for (const auto& camera: allCameras)
    {
        if (camera->getUniqueId() == uniqueIdOrName || camera->getName() == uniqueIdOrName)
        {
            result = camera;
            break;
        }
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Playback check

class PlaybackChecker: public QnAbstractMediaDataReceptor
{
public:
    enum State
    {
        failed,
        success,
        inProgress
    };

    PlaybackChecker(const QnTimePeriodList& timePeriods,
        int64_t mediaFileDurationMs,
        int64_t mediaFileGapMs,
        bool shouldSucceed):
        m_timePeriods(timePeriods),
        m_mediaFileDurationMs(mediaFileDurationMs), m_mediaFileGapMs(mediaFileGapMs),
        m_shouldSucceed(shouldSucceed)
    {
        std::sort(m_timePeriods.begin(), m_timePeriods.end());
        selectNextTimePeriod(false);
    }

    virtual bool canAcceptData() const override { return true; }

    virtual void putData(const QnAbstractDataPacketPtr& data) override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_state != inProgress)
            return;

        const int64_t dataTimestampMs = data->timestamp / 1000;
        if (dataTimestampMs == AV_NOPTS_VALUE)
            return setState(failed);

        if (dataTimestampMs < m_startTime)
        {
            if (!m_waitingForPeriodBeginning)
                return setState(failed);
            return;
        }

        m_waitingForPeriodBeginning = false;
        if (m_shouldSucceed)
            ASSERT_LT(dataTimestampMs - m_startTime, kTimeGapMs);
        if (dataTimestampMs - m_startTime >= kTimeGapMs)
            return setState(failed);
        m_startTime = dataTimestampMs;

        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(data);
        if (video)
        {
            auto& decoder = m_decoders[video->compressionType];
            if (!decoder)
            {
                decoder = std::make_unique<QnFfmpegVideoDecoder>(
                    DecoderConfig(), video->compressionType, video);
            }
            QSharedPointer<CLVideoDecoderOutput> outFrame(new CLVideoDecoderOutput());
            decoder->decode(video, &outFrame);
            auto frameTimestampMs = getTimestampFromFrame(outFrame.get());
            if (frameTimestampMs == 0
                && dataTimestampMs - m_firstFileFrameTime
                    == m_mediaFileDurationMs + m_mediaFileGapMs)
            {
                // A new file.
                m_firstFileFrameTime += m_mediaFileDurationMs + m_mediaFileGapMs;
            }
            if (m_shouldSucceed)
                ASSERT_EQ(dataTimestampMs - m_firstFileFrameTime, frameTimestampMs);
            if (dataTimestampMs - m_firstFileFrameTime != frameTimestampMs)
                return setState(failed);
        }

        if (m_endTime - m_startTime < kAcceptableTimeSpanToTheEndMs)
            selectNextTimePeriod(true);
    }

    void wait()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        while (m_state == inProgress)
            m_waitCondition.wait(&m_mutex);
    }

    State state() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_state;
    }

private:
    const int64_t kTimeGapMs = 1000;
    const int64_t kAcceptableTimeSpanToTheEndMs = 30000;

    mutable QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
    bool m_waitingForPeriodBeginning = true;
    QnTimePeriodList m_timePeriods;
    State m_state = inProgress;
    int64_t m_firstFileFrameTime = 0;
    int64_t m_startTime = 0;
    int64_t m_endTime = 0;
    std::map<AVCodecID, std::unique_ptr<QnFfmpegVideoDecoder>> m_decoders;
    int64_t m_mediaFileDurationMs = 0;
    int64_t m_mediaFileGapMs = 0;
    bool m_shouldSucceed;

    void selectNextTimePeriod(bool removeFirst)
    {
        if (m_timePeriods.isEmpty())
            return setState(success);

        if (removeFirst)
        {
            m_timePeriods.takeFirst();
            if (m_timePeriods.isEmpty())
                return setState(success);
        }

        m_firstFileFrameTime = m_startTime = m_timePeriods[0].startTimeMs;
        m_endTime = m_timePeriods[0].endTimeMs();
        m_waitingForPeriodBeginning = true;
    }

    void setState(State state)
    {
        m_state = state;
        m_waitCondition.wakeOne();
    }
};

void checkPlaybackCorrecteness(QnArchiveStreamReader* archiveReader,
    const QnTimePeriodList& expectedTimePeriods, int64_t mediaFileDurationMs,
    int64_t mediaFileGapMs, bool shouldSucceed)
{
    PlaybackChecker playbackChecker(
        expectedTimePeriods, mediaFileDurationMs, mediaFileGapMs, shouldSucceed);
    archiveReader->addDataProcessor(&playbackChecker);
    archiveReader->start();
    playbackChecker.wait();
    archiveReader->removeDataProcessor(&playbackChecker);

    if (shouldSucceed)
        ASSERT_EQ(PlaybackChecker::success, playbackChecker.state());
    else
        ASSERT_EQ(PlaybackChecker::failed, playbackChecker.state());
}

} // namespace nx::vms::server::test::test_support
