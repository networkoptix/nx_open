// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_db_manager.h"

#include <nx/cloud/cloud_services_ini.h>
#include <nx/cloud/db/client/time_periods_helper.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "cdb_request_path.h"

static constexpr char kDelPostTracksPath[] = "/analytics/v4/internal/objectTracks";
static constexpr char kGetTracksPath[] = "/analytics/v4/objectTracks";
static constexpr char kCompressedTrackPeriodsPath[] = "/analytics/v4/footage/flat";
static constexpr char kBestShotImagePath[] = "/analytics/v4/objectTracks/{id}/bestShotImage";
static constexpr char kTitleImagePath[] = "/analytics/v4/objectTracks{id}/titleImage";
static constexpr char kTaxonomyPath[] = "/analytics/v4/internal/taxonomy";

static const int kAgregationIntervalSec = 5;
static const int kAgregationIntervalMs = kAgregationIntervalSec * 1000;

namespace nx::cloud::db::client {

using namespace nx::network::http;

QString AnalyticsDbManager::m_customUrl;

AnalyticsDbManager::AnalyticsDbManager(
    ApiRequestsExecutor* requestsExecutor)
    :
    m_requestsExecutor(requestsExecutor)
{
    auto customUrl = m_customUrl;
    if (customUrl.isEmpty())
        customUrl = nx::cloud::ini().customizedAnalyticsDbUrl;
    if (!customUrl.isEmpty())
    {
        m_customRequestExecutor = std::make_unique<ApiRequestsExecutor>(
            nx::Url::fromUserInput(customUrl),
            nx::network::ssl::kDefaultCertificateCheck);
        m_customRequestExecutor->setRequestTimeout(std::chrono::seconds(60));

        m_customRequestExecutor->bindToAioThread(m_requestsExecutor->getAioThread());
        m_customRequestExecutor->httpClientOptions().setAdditionalHeaders(m_requestsExecutor->httpClientOptions().additionalHeaders());
    }
}

ApiRequestsExecutor* AnalyticsDbManager::requestExecutor()
{
    if (m_customRequestExecutor)
    {
        auto credentials = m_requestsExecutor->getHttpCredentials();
        if (credentials)
            m_customRequestExecutor->httpClientOptions().setCredentials(*credentials);
        return m_customRequestExecutor.get();
    }
    return m_requestsExecutor;
}

void AnalyticsDbManager::saveTracks(
    bool autoVectorize,
    const api::SaveTracksData& data,
    nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    const auto vectorizeOnCloud = nx::format("vectorize=%1", autoVectorize);
    requestExecutor()->makeAsyncCall<void>(
        Method::post,
        kDelPostTracksPath,
        /*urlQuery*/{ vectorizeOnCloud },
        data,
        std::move(completionHandler));
}

void AnalyticsDbManager::getTracks(
    const api::GetTracksParamsData& filter,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::ReadTracksData& data)> completionHandler)
{
    requestExecutor()->makeAsyncCall<api::ReadTracksData>(
        Method::get,
        kGetTracksPath,
        filter.toUrlQuery(),
        std::move(completionHandler));
}

void AnalyticsDbManager::getTrackPeriods(
    const api::GetTracksParamsData& filter,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::TimePeriodList& data)> completionHandler)
{
    auto query = filter.toUrlQuery();
    query.addQueryItem("interval", kAgregationIntervalMs); //< Aggregation interval. So far use same value as VMS.
    query.addQueryItem("resultInIntervals", "true");
    requestExecutor()->makeAsyncCall<std::vector<int64_t>>(
        Method::get,
        kCompressedTrackPeriodsPath,
        query,
        [handler = std::move(completionHandler)](api::ResultCode code, std::vector<int64_t> compressed)
        {
            using namespace nx::cloud::db::client;
            handler(code, uncompressTimePeriods<api::TimePeriodList>(compressed, /*ascOrder*/ true, kAgregationIntervalMs));
        });
}

struct ResponseFetcher
{
    api::TrackImageData operator()(const network::http::Response& response)
    {
        api::TrackImageData result;
        auto contentType = network::http::getHeaderValue(response.headers, "Content-Type");
        if (!contentType.empty() && contentType != "application/json")
        {
            result.mimeType = contentType;
            const auto& buffer = response.messageBody;
            result.data.resize(buffer.size());
            memcpy(result.data.data(), buffer.data(), result.data.size());
        }
        return result;
    }
};

void AnalyticsDbManager::getBestShotImage(
    const std::string& orgId,
    const std::string& siteId,
    const nx::Uuid& trackId,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::TrackImageData& image)> completionHandler)
{
    nx::UrlQuery query;
    if (!orgId.empty())
        query.addQueryItem("organizationId", orgId);
    query.addQueryItem("siteId", siteId);
    requestExecutor()->makeAsyncCall<void, ResponseFetcher>(
        Method::get,
        nx::network::http::rest::substituteParameters(kBestShotImagePath, { trackId.toSimpleStdString() }),
        query,
        std::move(completionHandler));
}

void AnalyticsDbManager::getTitleImage(
    const std::string& orgId,
    const std::string& siteId,
    const nx::Uuid& trackId,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::TrackImageData& image)> completionHandler)
{
    nx::UrlQuery query;
    if (!orgId.empty())
        query.addQueryItem("organizationId", orgId);
    query.addQueryItem("siteId", siteId);
    requestExecutor()->makeAsyncCall<void, ResponseFetcher>(
        Method::get,
        nx::network::http::rest::substituteParameters(kTitleImagePath, { trackId.toSimpleStdString()}),
        query,
        std::move(completionHandler));
}

void AnalyticsDbManager::deleteTracks(
    const std::string& orgId,
    const std::string& siteId,
    std::chrono::milliseconds time,
    nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    nx::UrlQuery query;
    if (!orgId.empty())
        query.addQueryItem("organizationId", orgId);
    query.addQueryItem("siteId", siteId);
    query.addQueryItem("endTimeMs", time.count());
    requestExecutor()->makeAsyncCall<void>(
        Method::delete_,
        kDelPostTracksPath,
        query,
        std::move(completionHandler));
}

void AnalyticsDbManager::saveTaxonomy(
    const std::string& orgId,
    const std::string& siteId,
    const api::TaxonomyData& data,
    nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    nx::UrlQuery query;
    if (!orgId.empty())
        query.addQueryItem("organizationId", orgId);
    query.addQueryItem("siteId", siteId);

    requestExecutor()->makeAsyncCall<void>(
        Method::post,
        kTaxonomyPath,
        query,
        nx::reflect::json::serialize(data),
        std::move(completionHandler));
}

void AnalyticsDbManager::getTaxonomy(
    const std::string& orgId,
    const std::string& siteId,
    nx::MoveOnlyFunc<void(api::ResultCode, api::TaxonomyData)> completionHandler)
{
    nx::UrlQuery query;
    if (!orgId.empty())
        query.addQueryItem("organizationId", orgId);
    query.addQueryItem("siteId", siteId);

    requestExecutor()->makeAsyncCall<QByteArray>(
        Method::get,
        kTaxonomyPath,
        query,
        [handler = std::move(completionHandler)](api::ResultCode result, QByteArray str)
        {
            using namespace nx::reflect;
            const auto [data, success] = json::deserialize<api::TaxonomyData>(str.data());
            handler(result, data);
        });
}

void AnalyticsDbManager::setCustomUrl(const QString& url)
{
    m_customUrl = url;
}

} // namespace nx::cloud::db::client
