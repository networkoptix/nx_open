#include "streaming_message_body.h"

#include "streaming_log_writer.h"

namespace nx::network::maintenance::log {

StreamingMessageBody::StreamingMessageBody(nx::utils::log::LoggerCollection* collection)
    :
    WritableMessageBody("text/plain"),
    m_loggerCollection(collection)
{
}

nx::network::maintenance::log::StreamingMessageBody::~StreamingMessageBody()
{
    m_logWriter->setMessageBody(nullptr);
    m_loggerCollection->remove(m_loggerId);
}

void StreamingMessageBody::setLoggerId(int loggerId)
{
    m_loggerId = loggerId;
}

void StreamingMessageBody::setLogWriter(StreamingLogWriter* logWriter)
{
    m_logWriter = logWriter;
}

} // namespace nx::network::maintenance::log
