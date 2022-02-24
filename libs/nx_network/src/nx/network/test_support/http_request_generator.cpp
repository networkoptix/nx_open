// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_request_generator.h"

namespace nx::network::test {

RequestGenerator::RequestGenerator(std::size_t maxSimultaneousRequestCount):
    m_maxSimultaneousRequestCount(maxSimultaneousRequestCount)
{
}

RequestGenerator::~RequestGenerator()
{
    pleaseStopSync();
}

void RequestGenerator::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& request: m_requests)
        request.client->bindToAioThread(aioThread);
}

void RequestGenerator::start()
{
    dispatch([this]() { startNewRequestsIfNeeded(); });
}

std::size_t RequestGenerator::completedRequestCount() const
{
    return m_completedRequestCount;
}

void RequestGenerator::stopWhileInAioThread()
{
    m_requests.clear();
}

void RequestGenerator::startNewRequestsIfNeeded()
{
    while (m_requests.size() < m_maxSimultaneousRequestCount)
    {
        m_requests.push_back(Request());
        m_requests.back().client = startRequest(
            [this, requestIter = std::prev(m_requests.end())]()
            {
                m_requests.erase(requestIter);
                ++m_completedRequestCount;
                startNewRequestsIfNeeded();
            });
    }
}

} // nx::network::test
