// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/repetitive_timer.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/maintenance/log/utils.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/log/logger_collection.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

namespace nx::network::maintenance::log {

struct UploadResult
{
    std::chrono::milliseconds duration = std::chrono::milliseconds::zero();
    std::size_t bytesUploaded = 0;
    std::size_t bytesDropped = 0;
    std::optional<std::string> lastErrorText;
    std::optional<http::StatusCode::Value> lastHttpError;
};

NX_REFLECTION_INSTRUMENT(
    UploadResult,
    (duration)(bytesUploaded)(bytesDropped)(lastErrorText)(lastHttpError))

class BufferedLogWriter;

/**
 * Uploads log to a given endpoint.
 * Log is uploaded in fragments.
 * Fragments are never uploaded concurrently to preserve the ordering.
 * On upload error received log is cached up to internal buffer size.
 * If buffer is filled, then log messages are dropped until some space is freed in the buffer.
 */
class NX_NETWORK_API Uploader:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    using Handler = nx::utils::MoveOnlyFunc<void(UploadResult)>;

    Uploader(
        const nx::utils::Url& logCollectorUrl,
        const std::string& sessionId,
        const nx::log::LevelSettings& logFilter);

    ~Uploader();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    static constexpr std::size_t kDefaultBufferSize = 128 * 1024;
    static constexpr std::size_t kMinBufSizeToUpload = 4 * 1024;
    static constexpr std::chrono::milliseconds kAccumulateDataTimeout = std::chrono::seconds(3);

    /**
     * Set buffer size for not-yet-sent log messages. By default, kDefaultBufferSize.
     * Must be set before Uploader::start.
     */
    void setLogBufferSize(std::size_t size);

    /**
     * If non-zero, then uploader accumulates at least size data before uploading.
     * If data was not accumulated during AccumulateDataTimeout, then existing data
     * is uploaded.
     * If zero, then the data is uploaded right away, without any delay.
     * By default, kMinBufSizeToUpload.
     */
    void setMinBufSizeToUpload(std::size_t);

    /**
     * See Uploader::setMinBufSizeToUpload.
     * Default value is kAccumulateDataTimeout.
     */
    void setAccumulateDataTimeout(std::chrono::milliseconds);

    /**
     * @param handler invoked when timeLimit has been exceeded.
     */
    void start(
        std::optional<std::chrono::milliseconds> timeLimit,
        Handler handler);

    /**
     * Stops the operation. Completion handler is not invoked.
     * The call does not block when called within the AIO thread.
     */
    void stopSync();

    UploadResult progress() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::optional<std::string /*error*/> installLogger();
    void removeLogger();

    void onTimeout();
    void checkUploadQueue();
    void onUploadCompleted();
    void stopUpload();

    void reportError(std::string text);
    void reportResult();

private:
    enum class State
    {
        init,
        working,
        stopRequested,
        flushing,
        done,
    };

    const nx::utils::Url m_uploadLogFragmentUrl;
    const nx::log::LevelSettings m_logFilter;
    UploadResult m_progress;
    Handler m_handler;
    aio::Timer m_timer;
    aio::RepetitiveTimer m_checkBufferTimer;
    nx::log::LoggerCollection* m_loggerCollection = nullptr;
    int m_loggerId = nx::log::LoggerCollection::kInvalidId;
    std::shared_ptr<BufferedLogWriter> m_logWriter;
    std::chrono::steady_clock::time_point m_startTime;
    std::size_t m_logBufferSize = kDefaultBufferSize;
    std::optional<std::chrono::steady_clock::time_point> m_minBufWaitStartClock;
    std::size_t m_minBufSizeToUpload = kMinBufSizeToUpload;
    std::chrono::milliseconds m_accumulateDataTimeout = kAccumulateDataTimeout;

    State m_state = State::init;
    bool m_uploading = false;
    std::unique_ptr<http::AsyncClient> m_uploadClient;
    std::size_t m_lastSentBufSize = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Convenience class for managing a single log streaming session.
 */
class NX_NETWORK_API UploaderManager
{
public:
    UploaderManager(
        const nx::utils::Url& logCollectorApiBaseUrl,
        const nx::log::LevelSettings& logFilter);

    ~UploaderManager();

    /**
     * Set buffer size for not-yet-sent log messages. By default, Uploader::kDefaultBufferSize.
     * Affects upload session started in the next UploaderManager::start call.
     */
    void setLogBufferSize(std::size_t size);

    /**
     * Starts log upload which will last for the given time.
     * If invoked when there is a running session already, then the current session id is returned.
     * @param timeLimit Duration for the logging process
     * @param forceSessionId Specific session identifier. Could be used to continue logging process
     * after application restart.
     * @return Session id.
     */
    std::string start(std::chrono::milliseconds timeLimit, const std::string& forceSessionId = {});

    bool isRunning() const;

    std::string currentSessionId() const;

private:
    void startLogUpload(
        const std::string& sessionId,
        std::chrono::milliseconds timeLimit);

    void onLoggingCompletion(UploadResult result);

private:
    const nx::utils::Url m_logCollectorApiBaseUrl;
    const nx::log::LevelSettings m_logFilter;
    std::string m_sessionId;
    std::unique_ptr<Uploader> m_uploader;
    nx::network::aio::BasicPollable m_aioBinder;
    mutable nx::Mutex m_mutex;
    std::optional<std::size_t> m_logBufferSize;
};

} // namespace nx::network::maintenance::log
