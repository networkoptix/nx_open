#include "streaming_log_writer.h"

#include <nx/network/http/writable_message_body.h>

namespace nx::network::maintenance::log {

using namespace nx::network::http;

StreamingLogWriter::StreamingLogWriter(WritableMessageBody* messageBody)
{
    setMessageBody(messageBody);
}

void StreamingLogWriter::write(nx::utils::log::Level /*level*/, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    if (m_messageBody)
        m_messageBody->writeBodyData(message.toUtf8());  
}

void StreamingLogWriter::setMessageBody(WritableMessageBody * messageBody)
{
    QnMutexLocker lock(&m_mutex);
    m_messageBody = messageBody;
}

} // namespace nx::network::maintenance::log
