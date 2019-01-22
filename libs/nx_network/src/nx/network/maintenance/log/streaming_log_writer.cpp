#include "streaming_log_writer.h"

#include "streaming_message_body.h"

namespace nx::network::maintenance::log {

StreamingLogWriter::StreamingLogWriter(StreamingMessageBody* messageBody)
    :
    AbstractWriter()
{
    setMessageBody(messageBody);
}

void StreamingLogWriter::write(nx::utils::log::Level /*level*/, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    if (m_messageBody)
        m_messageBody->writeBodyData(QByteArray().append(message));
}

void StreamingLogWriter::setMessageBody(StreamingMessageBody* messageBody)
{
    QnMutexLocker lock(&m_mutex);
    m_messageBody = messageBody;
}

} // namespace nx::network::maintenance::log
