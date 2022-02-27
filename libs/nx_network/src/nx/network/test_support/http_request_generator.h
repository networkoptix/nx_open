// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <list>
#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/url.h>

namespace nx::network::test {

class NX_NETWORK_API RequestGenerator:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    RequestGenerator(std::size_t maxSimultaneousRequestCount);
    ~RequestGenerator();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void start();

    std::size_t completedRequestCount() const;

protected:
    /**
     * The started request is considered running until completionHandler has been invoked.
     */
    virtual std::unique_ptr<nx::network::aio::BasicPollable> startRequest(
        nx::utils::MoveOnlyFunc<void()> completionHandler) = 0;

private:
    struct Request
    {
        std::unique_ptr<nx::network::aio::BasicPollable> client;
    };

    const std::size_t m_maxSimultaneousRequestCount;
    std::list<Request> m_requests;
    std::atomic<std::size_t> m_completedRequestCount = 0;

    virtual void stopWhileInAioThread() override;

    void startNewRequestsIfNeeded();
};

} // nx::network::test
