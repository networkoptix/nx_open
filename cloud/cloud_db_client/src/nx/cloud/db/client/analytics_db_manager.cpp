// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_db_manager.h"

#include <cloud_db_client_ini.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "cdb_request_path.h"

static constexpr char kTracksPath[] = "/analyticsDb/v1/tracks";
static constexpr char kBestShotImagePath[] = "/analyticsDb/v1/tracks/{id}/image/bestShot";
static constexpr char kTitleImagePath[] = "/analyticsDb/v1/tracks{id}/image/title";

namespace nx::cloud::db::client {

using namespace nx::network::http;

AnalyticsDbManager::AnalyticsDbManager(ApiRequestsExecutor* requestsExecutor):
    m_requestsExecutor(requestsExecutor)
{
    const QString customUrl(nx::cloud::db::api::ini().customizedAnalyticsDbUrl);
    if (customUrl.isEmpty())
    {
        m_requestsExecutor = requestsExecutor;
    }
    else
    {
        m_customRequestExecutor = std::make_unique<ApiRequestsExecutor>(
            nx::Url::fromUserInput(customUrl),
            nx::network::ssl::kDefaultCertificateCheck);
        m_customRequestExecutor->setRequestTimeout(std::chrono::seconds(60));

        m_customRequestExecutor->bindToAioThread(m_requestsExecutor->getAioThread());
        m_customRequestExecutor->httpClientOptions().setCredentials(m_requestsExecutor->httpClientOptions().credentials());
        m_customRequestExecutor->httpClientOptions().setAdditionalHeaders(m_requestsExecutor->httpClientOptions().additionalHeaders());
        m_requestsExecutor = m_customRequestExecutor.get();
    }
}

void AnalyticsDbManager::saveTracks(
    const api::SaveTracksData& data,
    nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<void>(
        Method::post,
        kTracksPath,
        /*urlQuery*/{ "vectorize=true" },
        data,
        std::move(completionHandler));
}

void AnalyticsDbManager::getTracks(
    const api::GetTracksParamsData& filter,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::ReadTracksData& data)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::ReadTracksData>(
        Method::get,
        kTracksPath,
        filter.toUrlQuery(),
        std::move(completionHandler));
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
    const nx::Uuid& trackId,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::TrackImageData& image)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<void, ResponseFetcher>(
        Method::get,
        nx::network::http::rest::substituteParameters(kBestShotImagePath, { trackId.toSimpleStdString() }),
        /*urlQuery*/{},
        std::move(completionHandler));
}

void AnalyticsDbManager::getTitleImage(
    const nx::Uuid& trackId,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::TrackImageData& image)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<void, ResponseFetcher>(
        Method::get,
        nx::network::http::rest::substituteParameters(kTitleImagePath, { trackId.toSimpleStdString()}),
        /*urlQuery*/{},
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
