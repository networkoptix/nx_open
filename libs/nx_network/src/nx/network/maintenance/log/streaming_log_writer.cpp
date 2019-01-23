#include "streaming_log_writer.h"

#include "streaming_message_body.h"

namespace nx::network::maintenance::log {

StreamingLogWriter::StreamingLogWriter(StreamingMessageBody* messageBody)
    :
    m_messageBody(messageBody)
{
    m_messageBody->setOnBeforeDestructionHandler(
        [this]()
        {
            QnMutexLocker lock(&m_mutex);
            m_messageBody = nullptr;
        });
}

void StreamingLogWriter::write(nx::utils::log::Level /*level*/, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    if (m_messageBody)
        m_messageBody->writeBodyData(message.toUtf8());
}

} // namespace nx::network::maintenance::log
