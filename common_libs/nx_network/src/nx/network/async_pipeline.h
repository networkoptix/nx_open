#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include "aio/basic_pollable.h"

namespace nx {
namespace network {

class AsynchronousChannel:
    public aio::BasicPollable
{
public:
    static constexpr std::size_t kDefaultReadBufferSize = 16 * 1024;
    static constexpr std::size_t kDefaultSendQueueSizeBytes = 128 * 1024;

    virtual ~AsynchronousChannel() override = default;

    virtual void start() = 0;
    virtual void setOnDone(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;
};

template<typename LeftFile, typename RightFile>
class AsyncChannelImpl:
    public AsynchronousChannel
{
    using base_type = AsynchronousChannel;

public:
    AsyncChannelImpl(LeftFile leftFile, RightFile rightFile)
    {
        m_leftFileContext.file = std::move(leftFile);
        m_leftFileContext.readBuffer.reserve(kDefaultReadBufferSize);

        m_rightFileContext.file = std::move(rightFile);
        m_rightFileContext.readBuffer.reserve(kDefaultReadBufferSize);

        bindToAioThread(getAioThread());
    }

    virtual ~AsyncChannelImpl() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_leftFileContext.file->bindToAioThread(aioThread);
        m_rightFileContext.file->bindToAioThread(aioThread);
    }

    virtual void start() override
    {
        scheduleRead(&m_leftFileContext, &m_rightFileContext);
        scheduleRead(&m_rightFileContext, &m_leftFileContext);
    }

    virtual void setOnDone(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        m_onDoneHandler = std::move(handler);
    }

private:
    template<typename FileType>
    struct FileContext
    {
        FileType file;
        nx::Buffer readBuffer;
        std::list<nx::Buffer> sendQueue;
        bool isReading = false;
    };

    FileContext<LeftFile> m_leftFileContext;
    FileContext<RightFile> m_rightFileContext;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onDoneHandler;

    virtual void stopWhileInAioThread() override
    {
        if (m_leftFileContext.file)
        {
            m_leftFileContext.file->pleaseStopSync();
            m_leftFileContext.file = nullptr;
        }
        if (m_rightFileContext.file)
        {
            m_rightFileContext.file->pleaseStopSync();
            m_rightFileContext.file = nullptr;
        }
    }

    template<typename FileTypeOne, typename FileTypeTwo>
    void scheduleRead(FileContext<FileTypeOne>* ctx, FileContext<FileTypeTwo>* anotherCtx)
    {
        using namespace std::placeholders;

        ctx->file->readSomeAsync(
            &ctx->readBuffer,
            [this, ctx, anotherCtx](
                SystemError::ErrorCode sysErrorCode, std::size_t bytesRead)
            {
                onDataRead(ctx, anotherCtx, sysErrorCode, bytesRead);
            });
        ctx->isReading = true;
    }

    template<typename FileTypeOne, typename FileTypeTwo>
    void onDataRead(
        FileContext<FileTypeOne>* ctx,
        FileContext<FileTypeTwo>* anotherCtx,
        SystemError::ErrorCode sysErrorCode,
        std::size_t /*bytesRead*/)
    {
        ctx->isReading = false;

        if (sysErrorCode != SystemError::noError)
            return reportFailure(sysErrorCode);

        nx::Buffer tmp;
        ctx->readBuffer.swap(tmp);
        scheduleSend(std::move(tmp), anotherCtx, ctx);
        ctx->readBuffer.reserve(kDefaultReadBufferSize);
        if (!readyToAcceptMoreData(anotherCtx))
            return;
        scheduleRead(ctx, anotherCtx);
    }

    template<typename FileTypeOne, typename FileTypeTwo>
    void scheduleSend(
        nx::Buffer data,
        FileContext<FileTypeOne>* ctx,
        FileContext<FileTypeTwo>* anotherCtx)
    {
        ctx->sendQueue.push_back(std::move(data));
        if (ctx->sendQueue.size() > 1) //< Send already in progress?
            return;
        sendNextData(ctx, anotherCtx);
    }

    template<typename FileType>
    bool readyToAcceptMoreData(FileContext<FileType>* /*fileContext*/)
    {
        // TODO
        return true;
    }

    template<typename FileTypeOne, typename FileTypeTwo>
    void sendNextData(FileContext<FileTypeOne>* ctx, FileContext<FileTypeTwo>* anotherCtx)
    {
        using namespace std::placeholders;

        ctx->file->sendAsync(
            ctx->sendQueue.front(),
            [this, ctx, anotherCtx](
                SystemError::ErrorCode sysErrorCode, std::size_t bytesRead)
            {
                onDataSent(ctx, anotherCtx, sysErrorCode, bytesRead);
            });
    }

    template<typename FileTypeOne, typename FileTypeTwo>
    void onDataSent(
        FileContext<FileTypeOne>* ctx,
        FileContext<FileTypeTwo>* anotherCtx,
        SystemError::ErrorCode sysErrorCode,
        std::size_t /*bytesRead*/)
    {
        if (sysErrorCode != SystemError::noError)
            return reportFailure(sysErrorCode);
        ctx->sendQueue.pop_front();

        if (!ctx->sendQueue.empty())
            sendNextData(ctx, anotherCtx);

        if (!readyToAcceptMoreData(ctx))
        {
            NX_ASSERT(!ctx->sendQueue.empty());
            return;
        }

        // Resuming read, if needed.
        if (!anotherCtx->isReading)
            scheduleRead(anotherCtx, ctx);
    }
    
    void reportFailure(SystemError::ErrorCode sysErrorCode)
    {
        m_leftFileContext.file->cancelIOSync(aio::EventType::etNone);
        m_rightFileContext.file->cancelIOSync(aio::EventType::etNone);

        if (m_onDoneHandler)
            m_onDoneHandler(sysErrorCode);
    }
};

template<typename LeftFile, typename RightFile>
std::unique_ptr<AsyncChannelImpl<LeftFile, RightFile>> makeAsyncChannel(
    LeftFile leftFile, RightFile rightFile)
{
    return std::make_unique<AsyncChannelImpl<LeftFile, RightFile>>(
        std::move(leftFile),
        std::move(rightFile));
}

} // namespace network
} // namespace nx
