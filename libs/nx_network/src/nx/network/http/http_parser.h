// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>

#include "http_stream_reader.h"
#include "http_types.h"
#include "../connection_server/base_protocol_message_types.h"

namespace nx::network::http {

/**
 * This class is just a wrapper on top of nx::network::http::HttpStreamReader
 * for use with nx::network::server::BaseStreamProtocolConnection class.
 */
class NX_NETWORK_API MessageParser:
    public nx::network::server::AbstractMessageParser<Message>
{
public:
    MessageParser();

    virtual void setMessage(Message* const message) override;

    /**
     * Methods returns if:
     *   - end of message found
     *   - source data depleted
     *
     * @param bytesProcessed Number of bytes from buf which were read and parsed is stored here.
     * Returns current parse state.
     * NOTE: *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte).
     * NOTE: Reads whole message even if parse error occured.
     */
    virtual nx::network::server::ParserState parse(
        const nx::ConstBufferRefType& buf,
        size_t* bytesProcessed) override;

    virtual nx::Buffer fetchMessageBody() override;

    /** Resets parse state and prepares for parsing different data. */
    virtual void reset() override;

    const HttpStreamReader& streamReader() const;
    HttpStreamReader& streamReader();

private:
    HttpStreamReader m_httpStreamReader;
    Message* m_message = nullptr;
    bool m_messageTaken = false;

    void provideMessageIfNeeded();
};

//-------------------------------------------------------------------------------------------------

namespace deprecated {

/**
 * This parser provides only whole message without providing message body on availability.
 * Introduced for backward compatibility.
 * Should be removed and all usages refactored to use MessageParser.
 */
class NX_NETWORK_API MessageParser:
    public nx::network::server::AbstractMessageParser<Message>
{
public:
    virtual void setMessage(Message* const message) override;

    virtual nx::network::server::ParserState parse(
        const nx::ConstBufferRefType& buffer,
        size_t* bytesProcessed) override;

    virtual void reset() override;

private:
    HttpStreamReader m_httpStreamReader;
    Message* m_message = nullptr;
};

} // namespace deprecated

} // namespace nx::network::http
