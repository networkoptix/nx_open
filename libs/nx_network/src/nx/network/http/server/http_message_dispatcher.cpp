// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_message_dispatcher.h"

#include <nx/utils/string.h>

namespace nx::network::http {

AbstractMessageDispatcher::AbstractMessageDispatcher():
    m_runningRequestCounter(std::make_shared<nx::utils::Counter>())
{
}

AbstractMessageDispatcher::~AbstractMessageDispatcher()
{
    auto activeRequests = m_activeRequests.takeAll();

    if (!activeRequests.empty())
    {
        NX_INFO(this, "Some requests are still running: %1",
            nx::utils::join(activeRequests, "; ", [](const auto& val) { return val.second; }));
    }

    if (m_linger)
        NX_ASSERT(waitUntilAllRequestsCompleted(*m_linger));
}

void AbstractMessageDispatcher::serve(
    RequestContext requestContext,
    nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler)
{
    auto sharedHandler = std::make_shared<decltype(completionHandler)>(std::move(completionHandler));

    const bool handlerFound = dispatchRequest(
        std::move(requestContext),
        [sharedHandler](RequestResult result)
        {
            (*sharedHandler)(std::move(result));
        });

    if (!handlerFound)
        (*sharedHandler)(RequestResult(StatusCode::notFound));
}

bool AbstractMessageDispatcher::registerRedirect(
    const std::string& sourcePath,
    const std::string& destinationPath,
    const Method& method)
{
    return registerRequestProcessor(
        sourcePath,
        [destinationPath]()
        {
            return std::make_unique<server::handler::Redirect>(destinationPath);
        },
        method);
}

bool AbstractMessageDispatcher::waitUntilAllRequestsCompleted(
    std::optional<std::chrono::milliseconds> timeout)
{
    if (timeout)
    {
        NX_DEBUG(this, "Waiting for %1 for requests to complete", *timeout);
        return m_runningRequestCounter->waitFor(*timeout);
    }
    else
    {
        m_runningRequestCounter->wait();
        return true;
    }
}

std::map<int, int> AbstractMessageDispatcher::statusCodesReported() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    std::map<int, int> result;
    for (const auto& [httpStatusCode, counter]: m_statusCodesPerMinute)
        result.emplace(httpStatusCode, counter.getSumPerLastPeriod());

    return result;
}

std::map<std::string, server::RequestStatistics>
    AbstractMessageDispatcher::requestPathStatistics() const
{
    std::map</*path*/ std::string, server::RequestStatistics> stats;
    m_requestPathStatsCalculators.forEach(
        [&stats](const auto& value)
        {
            auto requestPathStats = value.second.requestStatistics();
            if (requestPathStats.requestsServedPerMinute > 0)
                stats.emplace(value.first, std::move(requestPathStats));
        });

    return stats;
}

void AbstractMessageDispatcher::setLinger(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_linger = timeout;
}

void AbstractMessageDispatcher::recordDispatchFailure() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_statusCodesPerMinute[StatusCode::notFound].add(1);
}

void AbstractMessageDispatcher::recordStatistics(
    const RequestResult& result,
    const std::string& requestPathTemplate,
    std::chrono::microseconds processingTime) const
{
    m_requestPathStatsCalculators.modify(
        requestPathTemplate,
        [processingTime](auto& val) { val.processedRequest(processingTime); });

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_statusCodesPerMinute[result.statusCode].add(1);
}

} // namespace nx::network::http
