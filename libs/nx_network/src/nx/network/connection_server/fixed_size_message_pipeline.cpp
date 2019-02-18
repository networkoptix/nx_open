#include "fixed_size_message_pipeline.h"

namespace nx::network::server {

static constexpr int kMessageSizeBytes = 4;

static_assert(
    kMessageSizeBytes == sizeof(int),
    "Message size field length is expected to be equal to int size");

FixedSizeMessageParser::FixedSizeMessageParser()
{
    m_messageSizeBuffer.reserve(kMessageSizeBytes);
}

void FixedSizeMessageParser::setMessage(FixedSizeMessage* message)
{
    m_message = message;
    m_message->data.clear();
}

ParserState FixedSizeMessageParser::parse(
    const nx::Buffer& buf,
    size_t* bytesProcessed)
{
    QnByteArrayConstRef readBuffer(buf);

    while (!readBuffer.isEmpty())
    {
        switch (m_state)
        {
            case State::readingMessageSize:
                if (readMessageSize(&readBuffer))
                    m_state = State::readingMessage;
                break;

            case State::readingMessage:
                if (readMessage(&readBuffer))
                {
                    *bytesProcessed = readBuffer.constData() - buf.constData();
                    return nx::network::server::ParserState::done;
                }
                break;
        }
    }

    *bytesProcessed = buf.size();
    return nx::network::server::ParserState::readingMessage;
}

nx::Buffer FixedSizeMessageParser::fetchMessageBody()
{
    return nx::Buffer();
}

void FixedSizeMessageParser::reset()
{
    m_state = State::readingMessageSize;
    m_messageSizeBuffer.clear();
    m_lastMessageSize = 0;
}

bool FixedSizeMessageParser::readMessageSize(QnByteArrayConstRef* readBuffer)
{
    const auto bytesToRead = std::min<int>(
        kMessageSizeBytes - m_messageSizeBuffer.size(),
        (int) readBuffer->size());

    m_messageSizeBuffer.append(readBuffer->data(), bytesToRead);
    readBuffer->pop_front(bytesToRead);

    if (m_messageSizeBuffer.size() < kMessageSizeBytes)
        return false;

    memcpy(&m_lastMessageSize, m_messageSizeBuffer.constData(), kMessageSizeBytes);
    m_lastMessageSize = ntohl(m_lastMessageSize);
    m_message->data.reserve(m_lastMessageSize);
    return true;
}

bool FixedSizeMessageParser::readMessage(QnByteArrayConstRef* readBuffer)
{
    const auto bytesToRead = std::min<int>(
        m_lastMessageSize - m_message->data.size(),
        (int) readBuffer->size());

    m_message->data.append(readBuffer->data(), bytesToRead);
    readBuffer->pop_front(bytesToRead);

    return m_message->data.size() == m_lastMessageSize;
}

//-------------------------------------------------------------------------------------------------

void FixedSizeMessageSerializer::setMessage(const FixedSizeMessage* message)
{
    m_message = message;
}

SerializerState FixedSizeMessageSerializer::serialize(
    nx::Buffer* buffer,
    size_t* bytesWritten)
{
    const int messageSize = htonl(m_message->data.size());
    buffer->append(
        reinterpret_cast<const char*>(&messageSize),
        sizeof(messageSize));

    buffer->append(m_message->data);
    bytesWritten += m_message->data.size();

    return SerializerState::done;
}

} // namespace nx::network::server
