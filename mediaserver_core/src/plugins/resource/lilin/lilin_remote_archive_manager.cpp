#if defined(ENABLE_ONVIF)

#include "lilin_remote_archive_manager.h"

#include <vector>

#include <QtCore/QElapsedTimer>

#include <plugins/resource/lilin/lilin_resource.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/util.h>


namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const std::chrono::milliseconds kHttpTimeout(10000);
static const std::chrono::seconds kDefaultChunkDuration(60);

static const QString kListRequestTemplate("/sdlist?filelist=%1");
static const QString kStartDatePath("/sdlist?getrec=start");
static const QString kEndDatePath("/sdlist?getrec=end");
static const QString kChunkDownloadTemplate("/sdlist?download=%1");
static const QString kChunkDeleteTemplate("/sdlist?del=%1");
static const QString kListDirectoriesPath("/sdlist?dirlist=0");

static const QString kDeleteOkResponse("del OK");

static const QString kDateTimeFormat("yyyyMMddhhmmss");
static const QString kDateTimeFormat2("yyyy/MM/dd hh:mm:ss");

static const std::chrono::minutes kMaxChunkDuration(1);

static const std::chrono::milliseconds kWaitBeforeSync(30000);
static const int kNumberOfSyncCycles = 2;
static const int kDefaultOverlappedId = 0;
static const int kTriesPerChunk = 2;

} // namespace

using namespace nx::core::resource;

LilinRemoteArchiveManager::LilinRemoteArchiveManager(LilinResource* resource):
    m_resource(resource)
{
}

bool LilinRemoteArchiveManager::listAvailableArchiveEntries(
    OverlappedRemoteChunks* outArchiveEntries,
    int64_t startTimeMs,
    int64_t endTimeMs)
{
    using namespace std::chrono;

    auto dirs = getDirectoryList();

    std::vector<QString> files;
    for (const auto& dir : dirs)
        fetchFileList(dir, &files);

    auto driftMs = m_resource->getTimeDrift();
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

        auto durationMs = nextChunkStartMs
            ? nextChunkStartMs.get() - currentChunkStartMs.get()
            : duration_cast<milliseconds>(kMaxChunkDuration).count();

        (*outArchiveEntries)[kDefaultOverlappedId].emplace_back(
            files[i],
            currentChunkStartMs.get() - driftMs,
            durationMs,
            kDefaultOverlappedId);
    }

    return true;
}

bool LilinRemoteArchiveManager::fetchArchiveEntry(const QString& entryId, BufferType* outBuffer)
{
    auto response = doRequest(kChunkDownloadTemplate.arg(entryId), true);
    if (!response)
        return false;

    *outBuffer = response.get();

    return true;
}

bool LilinRemoteArchiveManager::removeArchiveEntries(const std::vector<QString>& entryIds)
{
    auto httpClient = initHttpClient();

    QString idString;

    for (const auto& id : entryIds)
        idString += id + lit(","); //< TODO: #dmishin check it;

    auto response = doRequest(kChunkDeleteTemplate.arg(idString));
    return response && response.get() == kDeleteOkResponse;
}

std::unique_ptr<nx_http::HttpClient> LilinRemoteArchiveManager::initHttpClient() const
{
    auto httpClient = std::make_unique<nx_http::HttpClient>();
    auto auth = m_resource->getAuth();

    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setAuthType(nx_http::AsyncHttpClient::AuthType::authBasic);
    httpClient->setResponseReadTimeoutMs(kHttpTimeout.count());
    httpClient->setSendTimeoutMs(kHttpTimeout.count());
    httpClient->setMessageBodyReadTimeoutMs(kHttpTimeout.count());

    return httpClient;
}

boost::optional<nx_http::BufferType> LilinRemoteArchiveManager::doRequest(
    const QString& requestPath,
    bool expectOnlyBody /*= false*/)
{
    auto httpClient = initHttpClient();

    httpClient->setExpectOnlyMessageBodyWithoutHeaders(expectOnlyBody);

    auto deviceUrl = QUrl(m_resource->getUrl());
    auto requestUrl = QUrl(
        lit("http://%1:%2%3")
            .arg(deviceUrl.host())
            .arg(deviceUrl.port(nx_http::DEFAULT_HTTP_PORT))
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

boost::optional<int64_t> LilinRemoteArchiveManager::getRecordingBound(RecordingBound bound)
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

boost::optional<int64_t> LilinRemoteArchiveManager::getRecordingStart()
{
    return getRecordingBound(RecordingBound::start);
}

boost::optional<int64_t> LilinRemoteArchiveManager::getRecordingEnd()
{
    return getRecordingBound(RecordingBound::end);
}

std::vector<QString> LilinRemoteArchiveManager::getDirectoryList()
{
    std::vector<QString> result;

    auto response = doRequest(kListDirectoriesPath);
    if (!response)
        return std::vector<QString>();


    auto dirStr = QString::fromLatin1(response.get()).trimmed();
    for (const auto& dir : dirStr.split(QRegExp(lit(",|:"))))
        result.push_back(dir.trimmed());

    return result;
}

bool LilinRemoteArchiveManager::fetchFileList(const QString& directory, std::vector<QString>* outFileLists)
{
    auto response = doRequest(kListRequestTemplate.arg(directory));
    if (!response)
        return false;

    auto files = response->split(L',');
    for (const auto& file : files)
    {
        auto split = file.split(L'.');
        if (split.size() != 2)
            continue;

        // Filter files that are being recorded at the moment
        auto entryExtension = QString::fromLatin1(split[1]);
        if (entryExtension.endsWith("_ing"))
            continue;

        outFileLists->push_back(QString::fromLatin1(split[0]));
    }

    return true;
}

boost::optional<int64_t> LilinRemoteArchiveManager::parseDate(const QString& dateTimeStr, const QString& dateTimeFormat)
{
    auto dateTime = QDateTime::fromString(dateTimeStr, dateTimeFormat);

    if (!dateTime.isValid())
        return boost::none;

    return dateTime.toMSecsSinceEpoch();
}

RemoteArchiveCapabilities LilinRemoteArchiveManager::capabilities() const
{
    return RemoteArchiveCapability::RemoveChunkCapability;
}

std::unique_ptr<QnAbstractArchiveDelegate> LilinRemoteArchiveManager::archiveDelegate(
    const RemoteArchiveChunk& /*chunk*/)
{
    return nullptr;
}

void LilinRemoteArchiveManager::beforeSynchronization()
{
    // Do nothing.
}

void LilinRemoteArchiveManager::afterSynchronization(bool /*isSynchronizationSuccessful*/)
{
    // Do nothing.
}

RemoteArchiveSynchronizationSettings LilinRemoteArchiveManager::settings() const
{
    return {
        kWaitBeforeSync,
        std::chrono::milliseconds::zero(),
        kNumberOfSyncCycles,
        kTriesPerChunk
    };
}

} // namespace plugins
} // namespace mediasever_core
} // namespace nx

#endif // ENABLE_ONVIF
