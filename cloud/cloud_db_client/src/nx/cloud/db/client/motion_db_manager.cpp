// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_db_manager.h"

#include <nx/cloud/cloud_services_ini.h>
#include <nx/cloud/db/client/time_periods_helper.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "cdb_request_path.h"

static constexpr char kDelPostMotionPath[] = "/motion/v4/internal/footage";
static constexpr char kGetMotionPath[] = "/motion/v4/footage/flat";

static const std::chrono::milliseconds kDefaultAgregationInterval{5000};

namespace nx::cloud::db::client {

using namespace nx::network::http;

QString MotionDbManager::m_customUrl;

MotionDbManager::MotionDbManager(
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

ApiRequestsExecutor* MotionDbManager::requestExecutor()
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

void MotionDbManager::saveMotion(
    const std::vector<api::Motion>& data,
    nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    requestExecutor()->makeAsyncCall<void>(
        Method::post,
        kDelPostMotionPath,
        /*urlQuery*/{},
        data,
        std::move(completionHandler));
}

void MotionDbManager::getMotionPeriods(
    const api::GetMotionParamsData& filter,
    nx::MoveOnlyFunc<void(api::ResultCode, const api::TimePeriodList& data)> completionHandler)
{
    auto query = filter.toUrlQuery();
    query.addQueryItem("resultInIntervals", "true");
    requestExecutor()->makeAsyncCall<std::vector<int64_t>>(
        Method::get,
        kGetMotionPath,
        query,
        [handler = std::move(completionHandler), filter = std::move(filter)]
        (api::ResultCode code, std::vector<int64_t> compressed)
        {
            using namespace nx::cloud::db::client;
            handler(code,
                uncompressTimePeriods<api::TimePeriodList>(
                    compressed,
                    filter.order == api::SortOrder::ascending,
                    filter.interval.value_or(kDefaultAgregationInterval).count()));
        });
}

void MotionDbManager::setCustomUrl(const QString& url)
{
    m_customUrl = url;
}

} // namespace nx::cloud::db::client
