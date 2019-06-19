#if defined(ENABLE_ONVIF)

#include "lilin_remote_archive_manager.h"

#include <vector>

#include <QtCore/QElapsedTimer>

#include <plugins/resource/lilin/lilin_resource.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/util.h>


namespace nx {
namespace vms::server {
namespace plugins {

namespace {

static const std::chrono::milliseconds kHttpTimeout(10000);
static const std::chrono::seconds kDefaultChunkDuration(60);

static const QString kListRequestTemplate("/sdlist?filelist=%1");
static const QString kChunkDownloadTemplate("/sdlist?download=%1");
static const QString kChunkDeleteTemplate("/sdlist?del=%1");
static const QString kListDirectoriesPath("/sdlist?dirlist=0");

static const QString kDeleteOkResponse("del OK");

static const QString kDateTimeFormat("yyyyMMddhhmmss");

static const std::chrono::minutes kMaxChunkDuration(1);

static const std::chrono::milliseconds kWaitBeforeSync(30000);
static const int kNumberOfSyncCycles = 2;
static const int kDefaultOverlappedId = 0;
static const int kTriesPerChunk = 2;

std::chrono::seconds extractSeconds(const QString& dateTimeString)
{
    const auto fileStartDateTime = QDateTime::fromString(dateTimeString, kDateTimeFormat);
    const auto timePart = fileStartDateTime.time();

    return std::chrono::seconds(timePart.second());
}

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

    const auto driftMs = m_resource->getTimeDrift();
    const auto dirs = getDirectoryList();
    for (const auto& dir: dirs)
    {
        std::vector<QString> files;
        seconds directoryTimeShift{0};

        fetchFileList(dir, &files);
        if (files.empty())
            continue;

        // Sometimes the first entry in the directory is shifted for a few seconds (relatively to
        // the time it reports in its name). This shift is usually equal to the seconds part of the
        // chunks timestamp that follow the first one.
        // Example: let's imagine that we have the following records:
        //  2019-08-08 20:21:23
        //  2019-08-08 20:22:04
        //  2019-08-08 20:23:04
        //  ...
        // In this case the real timestamp of the first record is almost likely 2019-08-08 20:21:19
        // And the timestamps of the subsequent records are:
        //  2019-08-08 20:22:00
        //  2019-08-08 20:23:00
        // i.e timestamps of all records are shifted for 4 seconds.
        const seconds firstEntrySecondsPart = extractSeconds(files[0]);
        if (firstEntrySecondsPart != seconds::zero())
        {
            for (int i = 1; i < files.size(); ++i)
            {
                directoryTimeShift = extractSeconds(files[i]);
                if (directoryTimeShift != seconds::zero())
                    break;
            }
        }

        QDateTime firstEntryStartTime = QDateTime::fromString(files[0], kDateTimeFormat);
        if (firstEntrySecondsPart < directoryTimeShift)
        {
            QTime time = firstEntryStartTime.time();
            time.setHMS(time.hour(), time.minute(), 0);
            firstEntryStartTime.setTime(time);
        }
        else
        {
            firstEntryStartTime.addSecs(-duration_cast<seconds>(directoryTimeShift).count());
        }

        const milliseconds firstEntryDuration =
            minutes(1) - seconds(firstEntryStartTime.time().second());

        (*outArchiveEntries)[kDefaultOverlappedId].emplace_back(
            files[0],
            firstEntryStartTime.toMSecsSinceEpoch() - driftMs,
            duration_cast<milliseconds>(firstEntryDuration).count(),
            kDefaultOverlappedId);

        // Usually all chunks in the directory (except for the first one) are aligned by the minute
        // border (i.e starts from  hh.mm.00). But its name may contain different slightly shifted
        // timestamp, which is not true. So we assign 0 seconds part to every chunk except for the
        // first one.
        for (int i = 1; i < files.size(); ++i)
        {
            QDateTime entryStartTime = QDateTime::fromString(files[i], kDateTimeFormat);
            QTime time = entryStartTime.time();
            time.setHMS(time.hour(), time.minute(), 0);
            entryStartTime.setTime(time);

            (*outArchiveEntries)[kDefaultOverlappedId].emplace_back(
                files[i],
                entryStartTime.toMSecsSinceEpoch() - driftMs,
                duration_cast<milliseconds>(minutes(1)).count(),
                kDefaultOverlappedId);
        }
    }

    return true;
}

nx::core::resource::ImportOrder LilinRemoteArchiveManager::overlappedIdImportOrder() const
{
    return nx::core::resource::ImportOrder::Direct;
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

std::unique_ptr<nx::network::http::HttpClient> LilinRemoteArchiveManager::initHttpClient() const
{
    auto httpClient = std::make_unique<nx::network::http::HttpClient>();
    auto auth = m_resource->getAuth();

    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setAuthType(nx::network::http::AuthType::authBasic);
    httpClient->setResponseReadTimeout(kHttpTimeout);
    httpClient->setSendTimeout(kHttpTimeout);
    httpClient->setMessageBodyReadTimeout(kHttpTimeout);

    return httpClient;
}

boost::optional<nx::network::http::BufferType> LilinRemoteArchiveManager::doRequest(
    const QString& requestPath,
    bool expectOnlyBody /*= false*/)
{
    auto httpClient = initHttpClient();

    httpClient->setExpectOnlyMessageBodyWithoutHeaders(expectOnlyBody);

    auto deviceUrl = nx::utils::Url(m_resource->getUrl());
    auto requestUrl = nx::utils::Url(
        lit("http://%1:%2%3")
            .arg(deviceUrl.host())
            .arg(deviceUrl.port(nx::network::http::DEFAULT_HTTP_PORT))
            .arg(requestPath));

    bool success = httpClient->doGet(requestUrl);
    if (!success)
        return boost::none;

    nx::network::http::BufferType response;
    while (!httpClient->eof())
        response.append(httpClient->fetchMessageBodyBuffer());

    return response;
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
    for (const auto& file: files)
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
