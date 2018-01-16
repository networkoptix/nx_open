#include "multiserver_analytics_lookup_detected_objects.h"

#include <nx/utils/std/future.h>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace {

static const char* kFormatParamName = "format";

} // namespace

QnMultiserverAnalyticsLookupDetectedObjects::QnMultiserverAnalyticsLookupDetectedObjects(
    QnCommonModule* commonModule,
    nx::analytics::storage::AbstractEventsStorage* eventStorage)
    :
    m_commonModule(commonModule),
    m_eventStorage(eventStorage)
{
}

int QnMultiserverAnalyticsLookupDetectedObjects::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    using namespace nx::analytics::storage;

    Filter filter;
    Qn::SerializationFormat outputFormat = Qn::JsonFormat;
    if (!deserializeRequest(params, &filter, &outputFormat))
        return nx::network::http::StatusCode::badRequest;

    const bool isLocal = params.value("isLocal") == "true";
    return execute(filter, isLocal, outputFormat, &result, &contentType);
}

int QnMultiserverAnalyticsLookupDetectedObjects::executePost(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* /*owner*/)
{
    using namespace nx::analytics::storage;

    Filter filter;
    Qn::SerializationFormat outputFormat = Qn::JsonFormat;
    if (!deserializeRequest(params, body, srcBodyContentType, &filter, &outputFormat))
        return nx::network::http::StatusCode::badRequest;

    const bool isLocal = params.value("isLocal") == "true";
    return execute(filter, isLocal, outputFormat, &result, &resultContentType);
}

bool QnMultiserverAnalyticsLookupDetectedObjects::deserializeRequest(
    const QnRequestParamList& params,
    nx::analytics::storage::Filter* filter,
    Qn::SerializationFormat* outputFormat)
{
    if (!deserializeOutputFormat(params, outputFormat))
        return false;

    if (!deserializeFromParams(params, filter))
    {
        NX_INFO(this, lm("Failed to parse filter"));
        return false;
    }

    return true;
}

bool QnMultiserverAnalyticsLookupDetectedObjects::deserializeOutputFormat(
    const QnRequestParamList& params,
    Qn::SerializationFormat* outputFormat)
{
    if (params.contains(kFormatParamName))
    {
        bool isParsingSucceeded = false;
        *outputFormat = QnLexical::deserialized<Qn::SerializationFormat>(
            params.value(kFormatParamName), Qn::UnsupportedFormat, &isParsingSucceeded);
        if (!isParsingSucceeded ||
            !(*outputFormat == Qn::JsonFormat || *outputFormat == Qn::UbjsonFormat))
        {
            NX_INFO(this, lm("Unsupported output format %1")
                .args(params.value(kFormatParamName)));
            return false;
        }
    }

    return true;
}

bool QnMultiserverAnalyticsLookupDetectedObjects::deserializeRequest(
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    nx::analytics::storage::Filter* filter,
    Qn::SerializationFormat* outputFormat)
{
    if (!deserializeOutputFormat(params, outputFormat))
        return false;

    bool isParsingSucceded = false;
    const auto inputFormat = Qn::serializationFormatFromHttpContentType(srcBodyContentType);
    switch (inputFormat)
    {
        case Qn::JsonFormat:
            *filter = QJson::deserialized(
                body, nx::analytics::storage::Filter(), &isParsingSucceded);
            return isParsingSucceded;

        case Qn::UbjsonFormat:
            *filter = QnUbjson::deserialized(
                body, nx::analytics::storage::Filter(), &isParsingSucceded);
            return isParsingSucceded;

        default:
            return false;
    }
}

nx::network::http::StatusCode::Value QnMultiserverAnalyticsLookupDetectedObjects::execute(
    const nx::analytics::storage::Filter& filter,
    bool isLocal,
    Qn::SerializationFormat outputFormat,
    QByteArray* body,
    QByteArray* contentType)
{
    using namespace nx::analytics::storage;

    NX_VERBOSE(this, lm("Executing detected objects lookup. Filter (%1), isLocal %2")
        .args(filter, isLocal));

    nx::utils::promise<std::tuple<ResultCode, LookupResult>> localLookupCompleted;
    m_eventStorage->lookup(
        filter,
        [this, &localLookupCompleted](
            ResultCode resultCode,
            LookupResult lookupResult)
        {
            localLookupCompleted.set_value(
                std::make_tuple(resultCode, std::move(lookupResult)));
        });

    std::vector<LookupResult> lookupResults;
    nx::network::http::StatusCode::Value remoteServerLookupResult =
        nx::network::http::StatusCode::ok;
    if (!isLocal)
        remoteServerLookupResult = lookupOnEveryOtherServer(filter, &lookupResults);

    const auto localLookupResult = localLookupCompleted.get_future().get();
    const auto localLookupResultCode = std::get<0>(localLookupResult);
    if (localLookupResultCode != ResultCode::ok)
    {
        NX_DEBUG(this, lm("Lookup with filter (%1) failed with error code %2")
            .args(filter, QnLexical::serialized(localLookupResultCode)));
        return toHttpStatusCode(localLookupResultCode);
    }
    lookupResults.push_back(std::move(std::get<1>(localLookupResult)));

    if (remoteServerLookupResult != nx::network::http::StatusCode::ok)
    {
        NX_DEBUG(this, lm("Lookup with filter %1 failed on some remote server with result %2")
            .args(filter, nx::network::http::StatusCode::toString(remoteServerLookupResult)));
        return remoteServerLookupResult;
    }

    NX_VERBOSE(this, lm("Detected objects lookup with filter (%1), isLocal %2 completed. "
        "%3 objects were found").args(filter, isLocal, lookupResults.size()));

    *body = serializeOutputData(
        mergeResults(std::move(lookupResults), filter.sortOrder),
        outputFormat);
    *contentType = Qn::serializationFormatToHttpContentType(outputFormat);
    return nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value
    QnMultiserverAnalyticsLookupDetectedObjects::lookupOnEveryOtherServer(
        const nx::analytics::storage::Filter& filter,
        std::vector<nx::analytics::storage::LookupResult>* lookupResults)
{
    QnResourcePool* resourcePool = m_commonModule->resourcePool();

    nx::utils::SyncQueue<
        std::tuple<bool /*hasSucceded*/, nx::analytics::storage::LookupResult>
    > requestResultQueue;

    std::vector<std::unique_ptr<rest::ServerConnection>> serverConnections;
    const auto onlineServers = resourcePool->getAllServers(Qn::ResourceStatus::Online);
    for (auto server: onlineServers)
    {
        serverConnections.push_back(
            std::make_unique<rest::ServerConnection>(m_commonModule, server->getId()));
        serverConnections.back()->lookupDetectedObjects(
            filter,
            true /*isLocal*/,
            [this, &requestResultQueue](
                bool hasSucceded,
                rest::Handle,
                nx::analytics::storage::LookupResult lookupResult)
            {
                requestResultQueue.push(
                    std::make_tuple(hasSucceded, std::move(lookupResult)));
            });
    }

    for (auto server: onlineServers)
    {
        const auto requestResult = requestResultQueue.pop();
        if (!std::get<0>(requestResult))
            continue; //< Ignoring failed request. Considering partial data to be good enough.
        lookupResults->push_back(std::move(std::get<1>(requestResult)));
    }

    return nx::network::http::StatusCode::ok;
}

nx::analytics::storage::LookupResult
    QnMultiserverAnalyticsLookupDetectedObjects::mergeResults(
        std::vector<nx::analytics::storage::LookupResult> lookupResults,
        Qt::SortOrder resultSortOrder)
{
    using namespace nx::analytics::storage;

    if (lookupResults.size() == 1)
        return std::move(lookupResults.front());

    LookupResult aggregatedResult;
    for (auto& result: lookupResults)
        std::move(result.begin(), result.end(), std::back_inserter(aggregatedResult));

    std::sort(
        aggregatedResult.begin(), aggregatedResult.end(),
        [resultSortOrder](const DetectedObject& left, const DetectedObject& right)
        {
            if (resultSortOrder == Qt::SortOrder::AscendingOrder)
                return left.firstAppearanceTimeUsec < right.firstAppearanceTimeUsec;
            else
                return left.firstAppearanceTimeUsec > right.firstAppearanceTimeUsec;
        });

    return aggregatedResult;
}

template<typename T>
QByteArray QnMultiserverAnalyticsLookupDetectedObjects::serializeOutputData(
    const T& output,
    Qn::SerializationFormat outputFormat)
{
    switch (outputFormat)
    {
        case Qn::JsonFormat:
            return QJson::serialized(output);

        case Qn::UbjsonFormat:
            return QnUbjson::serialized(output);

        default:
            return QByteArray();
    }
}
