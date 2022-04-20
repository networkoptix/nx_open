// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uploader.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/logger_builder.h>
#include <nx/utils/log/log_writers.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/utils/time.h>

#include "collector_api_paths.h"

namespace nx::network::maintenance::log {

static constexpr std::chrono::milliseconds kCheckUploadQueuePeriod = std::chrono::milliseconds(10);

//-------------------------------------------------------------------------------------------------

/**
 * Forwards log messages to the specified functor.
 */
class ProxyLogWriter:
    public nx::utils::log::AbstractWriter
{
public:
    using Func = nx::utils::MoveOnlyFunc<void(
        nx::utils::log::Level /*level*/,
        const QString& /*message*/)>;

    ProxyLogWriter(Func func):
        m_func(std::move(func))
    {
    }

    virtual void write(
        nx::utils::log::Level level,
        const QString& message) override
    {
        m_func(level, message);
    }

private:
    Func m_func;
};

//-------------------------------------------------------------------------------------------------

/**
 * Saves log messages in an internal buffer which may be taken.
 */
class BufferedLogWriter:
    public nx::utils::log::AbstractWriter
{
public:
    BufferedLogWriter(std::size_t maxBufferSize):
        m_maxBufferSize(maxBufferSize)
    {
        m_buffer.reserve(m_maxBufferSize);
    }

    virtual void write(
        nx::utils::log::Level /*level*/,
        const QString& message) override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (m_buffer.size() + message.size() > m_maxBufferSize)
            return; //< Skip message.

        m_buffer += message.toStdString();
    }

    nx::Buffer takeBuffer()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto result = std::exchange(m_buffer, {});
        m_buffer.reserve(m_maxBufferSize);
        return result;
    }

private:
    nx::Mutex m_mutex;
    nx::Buffer m_buffer;
    std::size_t m_maxBufferSize = 0;
};

//-------------------------------------------------------------------------------------------------

Uploader::Uploader(
    const nx::utils::Url& logCollectorUrl,
    const std::string& sessionId,
    const nx::utils::log::LevelSettings& logFilter)
    :
    m_uploadLogFragmentUrl(url::Builder(logCollectorUrl).appendPath(
        http::rest::substituteParameters(kLogSessionFragmentsPath, {sessionId}))),
    m_logFilter(logFilter)
{
    bindToAioThread(getAioThread());
}

Uploader::~Uploader()
{
    pleaseStopSync();
}

void Uploader::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
    m_checkBufferTimer.bindToAioThread(aioThread);
    if (m_uploadClient)
        m_uploadClient->bindToAioThread(aioThread);
}

void Uploader::setLogBufferSize(std::size_t size)
{
    m_logBufferSize = size;
}

void Uploader::start(
    std::optional<std::chrono::milliseconds> timeLimit,
    Handler handler)
{
    NX_VERBOSE(this, "Starting log uploader. Filter %1, url %2",
        m_logFilter, m_uploadLogFragmentUrl);

    if (auto err = installLogger(); err)
        return post([this, err]() { reportError(*err); });

    post([this, timeLimit, handler = std::move(handler)]() mutable {
        m_handler = std::move(handler);

        if (timeLimit)
            m_timer.start(*timeLimit, [this]() { onTimeout(); });

        m_checkBufferTimer.start(
            kCheckUploadQueuePeriod,
            [this]() { checkUploadQueue(); });

        m_startTime = nx::utils::monotonicTime();
    });
}

void Uploader::stopSync()
{
    executeInAioThreadSync([this]() {
        removeLogger();
        stopUpload();
        m_timer.pleaseStopSync();
        m_checkBufferTimer.pleaseStopSync();
        if (m_logWriter)
        {
            m_progress.bytesDropped += m_logWriter->takeBuffer().size();
            m_logWriter = nullptr;
        }
        m_handler = nullptr;
    });
}

UploadResult Uploader::progress() const
{
    return m_progress;
}

void Uploader::stopWhileInAioThread()
{
    stopSync();
}

std::optional<std::string /*error*/> Uploader::installLogger()
{
    using namespace nx::utils::log;

    if (!m_loggerCollection)
        m_loggerCollection = LoggerCollection::instance();

    LoggerSettings loggerSettings;
    loggerSettings.level = m_logFilter;
    Settings logSettings;
    logSettings.loggers.push_back(loggerSettings);

    auto logWriter = std::make_shared<BufferedLogWriter>(m_logBufferSize);
    auto proxyLogWriter = std::make_unique<ProxyLogWriter>(
        [logWriter](auto&&... args) { logWriter->write(std::forward<decltype(args)>(args)...); });

    auto newLogger = std::shared_ptr<AbstractLogger>(
        LoggerBuilder::buildLogger(
            logSettings,
            /*application name*/ "",
            /*application binary path*/ "",
            utils::toFilters(loggerSettings.level.filters),
            std::move(proxyLogWriter)));
    if (!newLogger)
    {
        NX_DEBUG(this, "Failed to create logger %1", m_logFilter);
        return nx::format("Failed to create logger %1", m_logFilter).toStdString();
    }

    m_loggerId = m_loggerCollection->add(newLogger);
    if (m_loggerId == LoggerCollection::kInvalidId)
    {
        NX_DEBUG(this, "Failed to install logger %1", m_logFilter);
        return nx::format("Failed to install logger %1", m_logFilter).toStdString();
    }

    m_logWriter = logWriter;
    return std::nullopt;
}

void Uploader::removeLogger()
{
    if (m_loggerId != nx::utils::log::LoggerCollection::kInvalidId)
    {
        m_loggerCollection->remove(m_loggerId);
        m_loggerId = nx::utils::log::LoggerCollection::kInvalidId;
    }
}

void Uploader::onTimeout()
{
    reportResult();
}

void Uploader::checkUploadQueue()
{
    if (m_uploading)
        return;

    auto buf = m_logWriter->takeBuffer();
    if (buf.empty())
        return;

    if (!m_uploadClient)
        m_uploadClient = std::make_unique<http::AsyncClient>(ssl::kDefaultCertificateCheck);

    m_lastSentBufSize = buf.size();
    m_uploadClient->doPost(
        m_uploadLogFragmentUrl,
        std::make_unique<http::BufferSource>("text/plain", std::move(buf)),
        [this]() { onUploadCompleted(); });
    m_uploading = true;
}

void Uploader::onUploadCompleted()
{
    m_uploading = false;
    if (m_uploadClient->failed())
    {
        m_progress.bytesDropped += m_lastSentBufSize;
        m_progress.lastErrorText = SystemError::toString(m_uploadClient->lastSysErrorCode());
    }
    else if (!http::StatusCode::isSuccessCode(m_uploadClient->response()->statusLine.statusCode))
    {
        m_progress.bytesDropped += m_lastSentBufSize;
        m_progress.lastHttpError =
            (http::StatusCode::Value) m_uploadClient->response()->statusLine.statusCode;
        m_progress.lastErrorText = m_uploadClient->response()->statusLine.reasonPhrase;
    }
    else if (http::StatusCode::isSuccessCode(m_uploadClient->response()->statusLine.statusCode))
    {
        m_progress.bytesUploaded += m_lastSentBufSize;
    }

    checkUploadQueue();
}

void Uploader::stopUpload()
{
    m_uploadClient.reset();
    m_uploading = false;
}

void Uploader::reportError(std::string text)
{
    m_progress.lastErrorText = std::move(text);

    reportResult();
}

void Uploader::reportResult()
{
    auto handler = std::exchange(m_handler, nullptr);
    stopSync();

    m_progress.duration = std::chrono::floor<std::chrono::milliseconds>(
        nx::utils::monotonicTime() - m_startTime);
    handler(std::exchange(m_progress, {}));
}

//-------------------------------------------------------------------------------------------------

UploaderManager::UploaderManager(
    const nx::utils::Url& logCollectorApiBaseUrl,
    const nx::utils::log::LevelSettings& logFilter)
    :
    m_logCollectorApiBaseUrl(logCollectorApiBaseUrl),
    m_logFilter(logFilter)
{
}

UploaderManager::~UploaderManager()
{
    m_aioBinder.executeInAioThreadSync([this]() { m_uploader.reset(); });
}

void UploaderManager::setLogBufferSize(std::size_t size)
{
    m_logBufferSize = size;
}

std::string UploaderManager::start(std::chrono::milliseconds timeLimit)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_sessionId.empty())
    {
        m_sessionId = QnUuid::createUuid().toSimpleStdString();
        m_aioBinder.post([this, sessionId = m_sessionId, timeLimit]() {
            startLogUpload(sessionId, timeLimit);
        });
    }

    return m_sessionId;
}

bool UploaderManager::isRunning() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return !m_sessionId.empty();
}

std::string UploaderManager::currentSessionId() const
{
    return m_sessionId;
}

void UploaderManager::startLogUpload(
    const std::string& sessionId,
    std::chrono::milliseconds timeLimit)
{
    m_uploader = std::make_unique<Uploader>(
        m_logCollectorApiBaseUrl, sessionId, m_logFilter);

    if (m_logBufferSize)
        m_uploader->setLogBufferSize(*m_logBufferSize);

    m_uploader->start(
        timeLimit,
        [this](auto&&... args) {
            onLoggingCompletion(std::forward<decltype(args)>(args)...);
        });
}

void UploaderManager::onLoggingCompletion(UploadResult /*result*/)
{
    m_uploader.reset();

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_sessionId.clear();
}

} // namespace nx::network::maintenance::log
