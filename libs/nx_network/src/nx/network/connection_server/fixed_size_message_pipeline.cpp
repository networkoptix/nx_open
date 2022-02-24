// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fixed_size_message_pipeline.h"

namespace nx::network::server {

static constexpr int kMessageSizeBytes = sizeof(std::uint32_t);

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
    const nx::ConstBufferRefType& buf,
    size_t* bytesProcessed)
{
    auto readBuffer = buf;

    while (!readBuffer.empty())
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
                    *bytesProcessed = readBuffer.data() - buf.data();
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

bool FixedSizeMessageParser::readMessageSize(nx::ConstBufferRefType* readBuffer)
{
    const auto bytesToRead = std::min<int>(
        kMessageSizeBytes - m_messageSizeBuffer.size(),
        (int) readBuffer->size());

    m_messageSizeBuffer.append(readBuffer->data(), bytesToRead);
    readBuffer->remove_prefix(bytesToRead);

    if (m_messageSizeBuffer.size() < kMessageSizeBytes)
        return false;

    memcpy(&m_lastMessageSize, m_messageSizeBuffer.data(), kMessageSizeBytes);
    m_lastMessageSize = ntohl(m_lastMessageSize);
    m_message->data.reserve(m_lastMessageSize);
    return true;
}

bool FixedSizeMessageParser::readMessage(nx::ConstBufferRefType* readBuffer)
{
    const auto bytesToRead = std::min<int>(
        m_lastMessageSize - m_message->data.size(),
        (int) readBuffer->size());

    m_message->data.append(readBuffer->data(), bytesToRead);
    readBuffer->remove_prefix(bytesToRead);

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
    *bytesWritten += m_message->data.size();

    return SerializerState::done;
}

} // namespace nx::network::server
