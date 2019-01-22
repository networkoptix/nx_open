#pragma once

#include <nx/network/http/writable_message_body.h>

#include <nx/utils/log/logger_collection.h>

namespace nx::network::maintenance::log {

class StreamingLogWriter;

class NX_NETWORK_API StreamingMessageBody:
    public nx::network::http::WritableMessageBody
{
public:
    StreamingMessageBody(nx::utils::log::LoggerCollection* collection);
    ~StreamingMessageBody();

    void setLoggerId(int loggerId);

private:
    int m_loggerId = nx::utils::log::LoggerCollection::kInvalidId;
    nx::utils::log::LoggerCollection* m_loggerCollection = nullptr;
};

} // namespace nx::network::maintenance::log
