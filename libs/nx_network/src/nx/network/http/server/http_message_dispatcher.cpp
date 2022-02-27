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
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto activeRequests = std::exchange(m_activeRequests, {});
    lock.unlock();

    if (!activeRequests.empty())
    {
        NX_INFO(this, "Some requests are still running: %1",
            nx::utils::join(activeRequests, "; ", [](const auto& val) { return val.second; }));
    }

    if (m_linger)
        NX_ASSERT(waitUntilAllRequestsCompleted(*m_linger));
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

int AbstractMessageDispatcher::dispatchFailures() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_dispatchFailures.getSumPerLastPeriod();
}

std::map<std::string, server::RequestPathStatistics>
    AbstractMessageDispatcher::requestPathStatistics() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    std::map<std::string, server::RequestPathStatistics> stats;
    for(const auto& [path, calculator]: m_requestPathStatsCalculators)
        stats.emplace(path, calculator.requestPathStatistics());
    return stats;
}

void AbstractMessageDispatcher::setLinger(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_linger = timeout;
}

void AbstractMessageDispatcher::incrementDispatchFailures() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const_cast<AbstractMessageDispatcher*>(this)->m_dispatchFailures.add(1);
}

void AbstractMessageDispatcher::startUpdatingRequestPathStatistics(
    const std::string& requestPathTemplate) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const_cast<AbstractMessageDispatcher*>(this)->
        m_requestPathStatsCalculators[requestPathTemplate].processingRequest();
}

void AbstractMessageDispatcher::finishUpdatingRequestPathStatistics(
    const std::string& requestPathTemplate,
    std::chrono::microseconds processingTime) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const_cast<AbstractMessageDispatcher*>(this)->
        m_requestPathStatsCalculators[requestPathTemplate].processedRequest(processingTime);
}

} // namespace nx::network::http
