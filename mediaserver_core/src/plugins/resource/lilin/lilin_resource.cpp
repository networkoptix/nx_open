#include "lilin_resource.h"

#include <vector>

#include <QtCore/QElapsedTimer>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/random.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <plugins/storage/memory/ext_iodevice_storage.h>
#include <plugins/resource/avi/avi_archive_delegate.h>
#include <utils/common/util.h>
#include <utils/common/concurrent.h>
#include <recorder/storage_manager.h>
#include <recording/stream_recorder.h>
#include <transcoding/transcoding_utils.h>
#include <core/resource/dummy_resource.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const std::chrono::milliseconds kHttpTimeout(20000);
static const QString kListRequestTemplate("/sdlist?filelist=%1");
static const QString kStartDatePath("/sdlist?getrec=start");
static const QString kEndDatePath("/sdlist?getrec=end");
static const QString kChunkDownloadTemplate("/sdlist?download=%1");
static const QString kChunkDeleteTemplate("/sdlist?del=%1");
static const QString kListDirectoriesPath("/sdlist?dirlist=0");

static const QString kDeleteOkResponse("del OK");

static const std::chrono::seconds kDefaultChunkDuration(60);

static const int kYearStringLength(4);
static const int kMonthStringLength(2);
static const int kDayStringLength(2);
static const int kHourStringLength(2);
static const int kMinuteStringLength(2);
static const int kSecondStringLength(2);

static const QString kArchiveContainer("matroska");

static const int kDateStringLength = kYearStringLength
    + kMonthStringLength
    + kDayStringLength
    + kHourStringLength
    + kMinuteStringLength
    + kSecondStringLength;

static const QString kDateTimeFormat("yyyyMMddhhmmss");
static const QString kDateTimeFormat2("yyyy/MM/dd hh:mm:ss");

} // namespace 

LilinResource::LilinResource():
    QnPlOnvifResource()
{

}

LilinResource::~LilinResource()
{

}

CameraDiagnostics::Result LilinResource::initInternal()
{
    base_type::initInternal();

    QnConcurrent::run([&](){synchronizeArchive();});

    return CameraDiagnostics::NoErrorResult();
}

bool LilinResource::listAvailableArchiveEntries(
    std::vector<RemoteArchiveEntry>* outArchiveEntries, 
    uint64_t startTimeMs, 
    uint64_t endTimeMs)
{
    auto dirs = getDirectoryList();

    std::vector<QString> files;
    for (const auto& dir: dirs)
        fetchFileList(dir, &files);

    auto driftSeconds = getTimeDrift();
    auto fileCount = files.size();
    for (auto i = 0; i < files.size(); ++i)
    {
        auto currentChunkStartMs = parseDate(files[i], kDateTimeFormat);
        decltype(currentChunkStartMs) nextChunkStartMs = boost::none;

        if (!currentChunkStartMs)
            continue;

        if (i < fileCount - 1)
        {
            nextChunkStartMs = parseDate(files[i + 1], kDateTimeFormat);

            if (!currentChunkStartMs || !nextChunkStartMs)
                continue;
        }

        const uint64_t kMillisecondsInMinute = 60000;

        auto duration = nextChunkStartMs
            ? nextChunkStartMs.get() - currentChunkStartMs.get()
            : kMillisecondsInMinute;

        outArchiveEntries->emplace_back(
            files[i],
            currentChunkStartMs.get() - driftSeconds,
            duration);
    }

    return true; //< TODO #dmishin Wrong!
}

bool LilinResource::fetchArchiveEntry(const QString& entryId, BufferType* outBuffer)
{
    auto response = doRequest(kChunkDownloadTemplate.arg(entryId), true);
    if (!response)
        return false;

    *outBuffer = response.get();

    return true;
}

bool LilinResource::removeArchiveEntries(const std::vector<QString>& entryIds)
{
    auto httpClient = initHttpClient();

    QString idString;

    for (const auto& id: entryIds)
        idString += id + lit(","); //< TODO: #dmishin check it;
    
    auto response  = doRequest(kChunkDeleteTemplate.arg(idString));
    return response && response.get() == kDeleteOkResponse;
}

std::unique_ptr<nx_http::HttpClient> LilinResource::initHttpClient() const
{
    auto httpClient = std::make_unique<nx_http::HttpClient>();
    auto auth = getAuth();

    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setResponseReadTimeoutMs(kHttpTimeout.count());
    httpClient->setSendTimeoutMs(kHttpTimeout.count());
    httpClient->setMessageBodyReadTimeoutMs(kHttpTimeout.count());

    return httpClient;
}

boost::optional<nx_http::BufferType> LilinResource::doRequest(const QString& requestPath, bool expectOnlyBody)
{
    auto httpClient = initHttpClient();

    httpClient->setExpectOnlyMessageBodyWithoutHeaders(expectOnlyBody);
    
    auto deviceUrl = QUrl(getUrl());
    auto requestUrl = QUrl(
        lit("http://%1:%2%3")
            .arg(deviceUrl.host())
            .arg(deviceUrl.port())
            .arg(requestPath));

    bool success = httpClient->doGet(requestUrl);

    auto httpResponse = httpClient->response();

    if (!success)
        return boost::none;

    nx_http::BufferType response;
    while (!httpClient->eof())
        response.append(httpClient->fetchMessageBodyBuffer());

    return response;
}

boost::optional<uint64_t> LilinResource::getRecordingBound(RecordingBound bound)
{
    auto path = bound == RecordingBound::start 
        ? kStartDatePath
        : kEndDatePath;

    auto response = doRequest(path);
    if (!response)
        return boost::none;

    auto datetime = parseDate(response.get(), kDateTimeFormat2);
    return datetime;
}

boost::optional<uint64_t> LilinResource::getRecordingStart()
{
    return getRecordingBound(RecordingBound::start);
}

boost::optional<uint64_t> LilinResource::getRecordingEnd()
{
    return getRecordingBound(RecordingBound::end);
}

std::vector<QString> LilinResource::getDirectoryList()
{
    std::vector<QString> result;

    auto response = doRequest(kListDirectoriesPath);
    if (!response)
        return std::vector<QString>();

    
    auto dirStr = QString::fromLatin1(response.get()).trimmed();
    for (const auto& dir: dirStr.split(QRegExp(lit("\,|\:"))))
        result.push_back(dir.trimmed());

    return result;
}

bool LilinResource::fetchFileList(const QString& directory, std::vector<QString>* outFileLists)
{
    auto response  = doRequest(kListRequestTemplate.arg(directory));
    if (!response)
        return false;

    auto files = response->split(L',');
    for (const auto& file: files)
    {
        auto split = file.split(L'.');
        if (split.size() != 2)
            continue;

        outFileLists->push_back(QString::fromLatin1(split[0]));
    }

    return true;
}

boost::optional<uint64_t> LilinResource::parseDate(
    const QString& dateTimeStr,
    const QString& dateTimeFormat)
{
    auto dateTime = QDateTime::fromString(dateTimeStr, dateTimeFormat);

    if (!dateTime.isValid())
        return boost::none;

    return dateTime.toMSecsSinceEpoch();
}

bool LilinResource::synchronizeArchive()
{
    qDebug() << "Starting archive synchronization.";
    NX_LOGX(lit("Starting archive synchronization."), cl_logDEBUG1);

    // 1. Get list of records from camera.
    std::vector<RemoteArchiveEntry> sdCardArchiveEntries;
    listAvailableArchiveEntries(&sdCardArchiveEntries);

    // 2. Filter list (keep only needed records)
    auto filtered = filterEntries(sdCardArchiveEntries);

    // 3. In cycle move all records to archive.
    for (const auto& entry: filtered)
        moveEntryToArchive(entry);

    qDebug() << "Archive synchronization is done.";
    NX_LOGX(lit("Archive synchronization is done."), cl_logDEBUG1);
    return true; //< Seems to be a nonsense
}

std::vector<RemoteArchiveEntry> LilinResource::filterEntries(const std::vector<RemoteArchiveEntry>& allEntries)
{
    auto uniqueId = getUniqueId();
    auto catalog = qnNormalStorageMan->getFileCatalog(
        uniqueId,
        DeviceFileCatalog::prefixByCatalog(QnServer::ChunksCatalog::HiQualityCatalog));

    std::vector<RemoteArchiveEntry> filtered;

    for (const auto& entry: allEntries)
    {
        if (!catalog->containTime(entry.startTimeMs + entry.durationMs / 2))
            filtered.push_back(entry);
    }

    qDebug() << "Filtered entries count" << filtered.size() << ". All entries count" << allEntries.size();
    NX_LOGX(
        lm("Filtered entries count %1. All entries count %2.")
            .arg(filtered.size())
            .arg(allEntries.size()),
        cl_logDEBUG1);
    return filtered;
}

bool LilinResource::moveEntryToArchive(const RemoteArchiveEntry& entry)
{
    nx_http::BufferType buffer;
    bool result = fetchArchiveEntry(entry.id, &buffer);

    qDebug() << "Archive entry with id" << entry.id << "has been fetched, result" << result;
    NX_LOGX(
        lm("Archive entry with id %1 has been fetched, result %2")
            .arg(entry.id)
            .arg(result),
        cl_logDEBUG1);

    if (!result)
        return false;

    qDebug() << "Archive entry with id " << entry.id << " has size" << buffer.size();
    NX_LOGX(
        lm("Archive entry with id %1 has size %2")
            .arg(entry.id)
            .arg(buffer.size()),
        cl_logDEBUG1);

    return writeEntry(entry, &buffer);
}

bool LilinResource::writeEntry(const RemoteArchiveEntry& entry, nx_http::BufferType* buffer)
{
    QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(toSharedPointer(this));
    NX_ASSERT(netResource != 0, Q_FUNC_INFO, "Only network resources can be used with storage manager!");

    auto normalStorage = qnNormalStorageMan->getOptimalStorageRoot();
    if (!normalStorage)
        return false;

    QString fileName;
    
    fileName = qnNormalStorageMan->getFileName(
        entry.startTimeMs,
        currentTimeZone() / 60,
        netResource,
        DeviceFileCatalog::prefixByCatalog(QnServer::ChunksCatalog::HiQualityCatalog),
        normalStorage);

    QString fileNameWithExtension = fileName + lit(".mkv");

    qnNormalStorageMan->fileStarted(
        entry.startTimeMs,
        currentTimeZone() / 60,
        fileNameWithExtension,
        nullptr);

    QFileInfo info(fileNameWithExtension);
    QDir dir = info.dir();

    if (!dir.exists())
    {
        if (!dir.mkpath("."))
            return false;
    }

    int64_t realDurationMs = AV_NOPTS_VALUE;
    if (!convertAndWriteBuffer(buffer, fileNameWithExtension, &realDurationMs))
        return false;
    
    const uint64_t kEpsilonMs = 100; 
    auto diff = entry.durationMs - realDurationMs;
    if (diff < 0)
        diff = -diff; 

    if (realDurationMs == AV_NOPTS_VALUE || diff <= kEpsilonMs)
        realDurationMs = entry.durationMs;

    return qnNormalStorageMan->fileFinished(
        realDurationMs,
        fileNameWithExtension,
        nullptr,
        buffer->size());

    return true;
}

bool LilinResource::convertAndWriteBuffer(
    nx_http::BufferType* buffer,
    const QString& fileName,
    int64_t* outDurationMs)
{
    qDebug() << "Converting and writing file" << fileName;
    NX_LOGX(lm("Converting and writing file %1").arg(fileName), cl_logDEBUG1);

    const QString temporaryFilePath = QString::number(nx::utils::random::number());
    QnExtIODeviceStorageResourcePtr storage(new QnExtIODeviceStorageResource());
    QnResourcePtr resource(new DummyResource());

    QBuffer* ioDevice = new QBuffer();
    ioDevice->setBuffer(buffer);
    ioDevice->open(QIODevice::ReadOnly);

    auto audioLayout = nx::transcoding::audioLayout(ioDevice);
    ioDevice->reset();

    *outDurationMs = std::round(nx::transcoding::mediaDurationMs(ioDevice) / 1000.0);
    ioDevice->reset();

    storage->registerResourceData(temporaryFilePath, ioDevice);
    resource->setUrl(temporaryFilePath);

    QnAviArchiveDelegatePtr mediaFileReader(new QnAviArchiveDelegate());
    mediaFileReader->setStorage(storage);
    if (!mediaFileReader->open(resource))
        return false;

    mediaFileReader->setAudioChannel(0);

    auto recorder = std::make_unique<QnStreamRecorder>(toResourcePtr());
    recorder->clearUnprocessedData();
    recorder->addRecordingContext(fileName, qnNormalStorageMan->getOptimalStorageRoot());
    recorder->setContainer(kArchiveContainer);
    recorder->forceAudioLayout(audioLayout);
    recorder->disableRegisterFile(true);

    while (auto mediaData = mediaFileReader->getNextData())
    {
        if (mediaData->dataType == QnAbstractMediaData::EMPTY_DATA)
            break;

        recorder->processData(mediaData);
    }
    
    return true;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx