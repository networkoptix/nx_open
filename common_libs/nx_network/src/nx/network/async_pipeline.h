#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include "abstract_socket.h"
#include "aio/basic_pollable.h"

namespace nx {
namespace network {

class AbstractAsyncPipeline:
    public aio::BasicPollable
{
public:
    virtual ~AbstractAsyncPipeline() override = default;

    virtual void start() = 0;
    virtual void setOnDone(nx::utils::MoveOnlyFunc<void()> handler) = 0;
};

template<typename LeftFile, typename RightFile>
class AsyncPipeline:
    public AbstractAsyncPipeline
{
    using base_type = AbstractAsyncPipeline;

public:
    AsyncPipeline(LeftFile leftFile, RightFile rightFile):
        m_leftFile(std::move(leftFile)),
        m_rightFile(std::move(rightFile))
    {
        bindToAioThread(getAioThread());
    }

    virtual ~AsyncPipeline() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_leftFile->bindToAioThread(aioThread);
        m_rightFile->bindToAioThread(aioThread);
    }

    virtual void start() override
    {
        // TODO
    }

    virtual void setOnDone(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_onDoneHandler = std::move(handler);
    }

private:
    LeftFile m_leftFile;
    RightFile m_rightFile;
    nx::utils::MoveOnlyFunc<void()> m_onDoneHandler;

    virtual void stopWhileInAioThread() override
    {
        m_leftFile = nullptr;
        m_rightFile = nullptr;
    }
};

template<typename LeftFile, typename RightFile>
std::unique_ptr<typename AsyncPipeline<LeftFile, RightFile>> makeAsyncPipeline(
    LeftFile leftFile, RightFile rightFile)
{
    return std::make_unique<typename AsyncPipeline<LeftFile, RightFile>>(
        std::move(leftFile),
        std::move(rightFile));
}

} // namespace network
} // namespace nx
