#include "streaming_message_body.h"

#include "streaming_log_writer.h"

namespace nx::network::maintenance::log {

using namespace nx::utils::log;

StreamingMessageBody::StreamingMessageBody(LoggerCollection* collection)
    :
    WritableMessageBody("text/plain"),
    m_loggerCollection(collection)
{
}

nx::network::maintenance::log::StreamingMessageBody::~StreamingMessageBody()
{
    m_loggerCollection->remove(m_loggerId);
}

void StreamingMessageBody::setLoggerId(int loggerId)
{
    m_loggerId = loggerId;
}

} // namespace nx::network::maintenance::log
