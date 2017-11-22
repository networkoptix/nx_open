#include "multiserver_analytics_lookup_detected_objects.h"

#include <nx/utils/std/future.h>

#include <analytics/detected_objects_storage/analytics_events_storage.h>

namespace {

static const char* kFormatParamName = "format";

} // namespace

QnMultiserverAnalyticsLookupDetectedObjects::QnMultiserverAnalyticsLookupDetectedObjects(
    const QString& path,
    nx::analytics::storage::AbstractEventsStorage* eventStorage)
    :
    m_requestPath(path),
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
        return nx_http::StatusCode::badRequest;

    return execute(filter, outputFormat, &result, &contentType);
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
        return nx_http::StatusCode::badRequest;

    return execute(filter, outputFormat, &result, &resultContentType);
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

nx_http::StatusCode::Value QnMultiserverAnalyticsLookupDetectedObjects::execute(
    const nx::analytics::storage::Filter& filter,
    Qn::SerializationFormat outputFormat,
    QByteArray* body,
    QByteArray* contentType)
{
    using namespace nx::analytics::storage;

    LookupResult outputData;

    nx::utils::promise<ResultCode> lookupCompleted;
    m_eventStorage->lookup(
        filter,
        [this, &lookupCompleted, &outputData](
            ResultCode resultCode,
            LookupResult lookupResult)
        {
            outputData.swap(lookupResult);
            lookupCompleted.set_value(resultCode);
        });

    const auto resultCode = lookupCompleted.get_future().get();
    if (resultCode != ResultCode::ok)
    {
        NX_DEBUG(this, lm("Lookup with filter %1 failed with error code %2")
            .args("TODO", QnLexical::serialized(resultCode)));
        return toHttpStatusCode(resultCode);
    }

    *body = serializeOutputData(outputData, outputFormat);
    *contentType = Qn::serializationFormatToHttpContentType(outputFormat);
    return nx_http::StatusCode::ok;
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
