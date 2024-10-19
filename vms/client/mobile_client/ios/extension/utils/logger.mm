// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logger.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include <nx/vms/client/mobile/push_notification/details/push_ipc_data.h>

#include "helpers.h"

namespace extension {

static const NSString* kLogTag = @"[Mobile client]";

using namespace nx::vms::client::mobile::details;

std::chrono::milliseconds currentTime()
{
    return std::chrono::seconds(static_cast<long long>([[NSDate date] timeIntervalSince1970]));
}

NSString* currentTimeString()
{
    static const NSDateFormatter* kTimeFormatter =
        []()
        {
            const auto formatter = [[NSDateFormatter alloc] init];
            [formatter setDateFormat: @"MM/dd/yyyy HH:mm:ss"];
            return formatter;
        }();
    return [kTimeFormatter stringFromDate: [NSDate date]];
}

//-------------------------------------------------------------------------------------------------

class Logger::Private
{
public:
    Private();

    ~Private();

    void log(const NSString* message);

private:
    void uploadWorker();

    void resetCurrentSessionDataUnsafe();

    void setCurrentSessionDataUnsafe(
        const std::string& sessionId,
        const std::chrono::milliseconds& sessionEndTime);

private:
    std::mutex m_mutex;

    bool m_exiting = false;
    std::condition_variable m_cv;
    std::thread m_uploadThread;

    std::string m_sessionId;
    std::string m_buffer;
    std::chrono::milliseconds m_sessionEndTime;
};

Logger::Private::Private():
    m_uploadThread([this]() { uploadWorker(); })
{
}

Logger::Private::~Private()
{
    {
        const std::scoped_lock<std::mutex> lock(m_mutex);
        m_exiting = true;
    }

    m_cv.notify_one();
    m_uploadThread.join();
}

void Logger::Private::log(const NSString* message)
{
    if ([message length] <= 0)
        return;

    Logger::logToSystemOnly(message);

    std::string sessionId;
    std::chrono::milliseconds sessionEndTime;
    PushIpcData::cloudLoggerOptions(sessionId, sessionEndTime);

    {
        const std::scoped_lock lock(m_mutex);

        if (sessionId != m_sessionId)
        {
            if (sessionId.empty())
                resetCurrentSessionDataUnsafe();
            else
                setCurrentSessionDataUnsafe(sessionId, sessionEndTime);
        }

        // Check if current session is outdated.
        if (m_sessionEndTime != std::chrono::milliseconds::zero()
            && currentTime() > m_sessionEndTime)
        {
            Logger::logToSystemOnly(@"Cloud logging session is outdated, resetting options");
            PushIpcData::setCloudLoggerOptions(/*logSessionId*/ {}, /*sessionEndTime*/ {});
            resetCurrentSessionDataUnsafe();
            return;
        }

        if (m_sessionId.empty())
            return;

        const auto finalMessage = [NSString stringWithFormat: @"%@ %@: %@\r\n",
            currentTimeString(), kLogTag, message];
        m_buffer += std::string([finalMessage UTF8String]);
        m_cv.notify_one();
    }
}

void Logger::Private::uploadWorker()
{
    while (true)
    {
        std::string sessionId;
        std::string buffer;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock,
                [this]() { return m_exiting || !(m_sessionId.empty() || m_buffer.empty()); });

            if (m_exiting)
                return;

            sessionId = m_sessionId;
            buffer = m_buffer;
            m_buffer.clear();
        }

        const auto resultHandler =
            [sessionId, buffer](long httpErrorCode, NSData* /*data*/)
            {
                const auto message = helpers::isSuccessHttpStatus(httpErrorCode)
                    ? [NSString stringWithFormat: @"successfully uploaded %lu bytes", buffer.size()]
                    : [NSString stringWithFormat: @"failure, code is %ld", httpErrorCode];

                Logger::logToSystemOnly([NSString stringWithFormat:
                    @"log upload for session %s: %@", sessionId.c_str(), message]);
            };

        const auto path = [NSString stringWithFormat:
            @"log-collector/v1/log-session/%s/log/fragment", sessionId.c_str()];
        const auto url = helpers::urlFromHostAndPath(helpers::cloudHost(), path);

        const auto payload = [NSData dataWithBytes: buffer.c_str() length: buffer.size()];
        helpers::syncRequest(url, {helpers::ContentType::text, payload}, resultHandler);
    }
}

void Logger::Private::resetCurrentSessionDataUnsafe()
{
    Logger::logToSystemOnly([NSString stringWithFormat:
        @"Resetting current cloud log upload session with id %s", m_sessionId.c_str()]);

    m_sessionId = {};
    m_buffer = {};
    m_sessionEndTime = {};
}

void Logger::Private::setCurrentSessionDataUnsafe(
    const std::string& sessionId,
    const std::chrono::milliseconds& sessionEndTime)
{
    Logger::logToSystemOnly([NSString stringWithFormat:
        @"Initializing new cloud log upload session with id %s.", sessionId.c_str()]);

    m_sessionId = sessionId;
    m_buffer = {};
    m_sessionEndTime = sessionEndTime;
}

//-------------------------------------------------------------------------------------------------

Logger::Logger():
    d(new Private())
{
}

Logger::~Logger()
{
}

Logger& Logger::instance()
{
    static Logger sInstance;
    return sInstance;
}

void Logger::log(const NSString* message)
{
    if (message && [message length])
        instance().d->log(message);
}

void Logger::log(const NSString* logTag, const NSString* message)
{
    if (message && [message length] && logTag && [logTag length])
        log([NSString stringWithFormat: @"%@: %@", logTag, message]);
}

void Logger::logToSystemOnly(const NSString* message)
{
    if (message && [message length])
        NSLog(@"%@: %@", kLogTag, message);
}

void Logger::logToSystemOnly(
    const NSString* logTag,
    const NSString* message)
{
    if (message && [message length] && logTag && [logTag length])
        logToSystemOnly([NSString stringWithFormat: @"%@: %@", logTag, message]);
}

} // namespace extension
