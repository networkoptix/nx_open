#pragma once

#include <nx/utils/log/log_writers.h>

namespace nx::network::maintenance::log {

class StreamingMessageBody;

class NX_NETWORK_API StreamingLogWriter:
    public nx::utils::log::AbstractWriter
{
public:
    StreamingLogWriter(StreamingMessageBody* mediator);
    ~StreamingLogWriter() = default;

    virtual void write(nx::utils::log::Level level, const QString& message) override;

    void setMessageBody(StreamingMessageBody* messageBody);

private:
    QnMutex m_mutex;
    StreamingMessageBody* m_messageBody = nullptr;
};

} 