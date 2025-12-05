// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_message_dispatcher.h"

#include <chrono>

#include <nx/utils/string.h>

#include "handler/http_server_handler_redirect.h"

namespace nx::network::http {

AbstractMessageDispatcher::AbstractMessageDispatcher():
    m_runningRequestCounter(std::make_shared<nx::utils::Counter>())
{
}

AbstractMessageDispatcher::~AbstractMessageDispatcher()
{
    printActiveRequests();

    if (m_linger)
        NX_ASSERT(waitUntilAllRequestsCompleted(*m_linger));
}

void AbstractMessageDispatcher::serve(
    RequestContext requestContext,
    nx::MoveOnlyFunc<void(RequestResult)> completionHandler)
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

std::map<std::string, server::RequestStatistics>
    AbstractMessageDispatcher::requestLineStatistics() const
{
    std::map</*requestLine*/ std::string, server::RequestStatistics> result;
    m_requestStatisticsCalculators.forEach(
        [&result](
            const std::pair<const std::string, server::RequestStatisticsCalculator>& item)
        {
            auto requestStatistics = item.second.statistics();
            if (requestStatistics.requestsServedPerMinute > 0)
                result.emplace(item.first, std::move(requestStatistics));
        });

    return result;
}

void AbstractMessageDispatcher::setLinger(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_linger = timeout;
}

void AbstractMessageDispatcher::recordStatistics(
    const RequestResult& result,
    const std::string& requestPathTemplate,
    std::chrono::microseconds processingTime) const
{
    m_requestStatisticsCalculators.modify(
        requestPathTemplate,
        [processingTime, statusCode = result.statusCode](
            server::RequestStatisticsCalculator& val)
        {
            val.processedRequest(processingTime, statusCode);
        });
}

void AbstractMessageDispatcher::printActiveRequests() const
{
    using namespace std::chrono;
    auto activeRequests = m_activeRequests.takeAll();

    if (!activeRequests.empty())
    {
        NX_INFO(this,
            "Some requests are still running: %1",
            nx::utils::join(activeRequests,
                "; ",
                [](const auto& val)
                {
                    return nx::format("%1 - %2 [%3ms]")
                        .args(val.second.traceId,
                            val.second.requestLine,
                            duration_cast<milliseconds>(steady_clock::now() - val.second.startTime)
                                .count())
                        .toStdString();
                }));
    }
}

} // namespace nx::network::http
