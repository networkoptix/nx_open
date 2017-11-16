#pragma once

#include <memory>
#include <cstdint>
#include <deque>

#include <nx/network/abstract_socket.h>
#include <nx/network/stun/message.h>
#include <nx/network/buffer.h>

#include "../connection_server/base_protocol_message_types.h"
#include "stun_message_parser_buffer.h"

namespace nx {
namespace stun {

class NX_NETWORK_API MessageParser:
    public nx::network::server::AbstractMessageParser<Message>
{
private:
    enum
    {
        // Header
        HEADER_INITIAL_AND_TYPE,
        HEADER_LENGTH,
        HEADER_MAGIC_ID,
        HEADER_TRANSACTION_ID,
        // Attributes
        ATTRIBUTE_TYPE,
        ATTRIBUTE_LENGTH,
        ATTRIBUTE_VALUE,

        // Attributes for fingerprint only
        ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH,
        ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_TYPE,
        ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_VALUE,

        // Other states
        MORE_VALUE,
        END_MESSAGE_INTEGRITY,
        END_FINGERPRINT
    };

    /**
     * Parsing component states.
     */
    enum
    {
        IN_PROGRESS,
        SECTION_FINISH,
        FINISH,
        FAILED
    };

public:
    MessageParser();

    virtual void setMessage(Message* const msg) override;

    /**
     * Returns current parse state.
     * Methods returns if:
     * - end of message found
     * - source data depleted

     * @param bytesProcessed Number of bytes from buf which were read and parsed is stored here.
     * NOTE: *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte).
     * NOTE: Reads whole message even if parse error occurred.
     */
    virtual nx::network::server::ParserState parse(
        const nx::Buffer& /*buf*/,
        size_t* /*bytesProcessed*/) override;

    /**
     * @return Current parse state.
     */
    nx::network::server::ParserState state() const;

    /**
     * Resets parse state and prepares for parsing different data.
     */
    virtual void reset() override;

private:
    // Attribute value parsing
    attrs::Attribute* parseXORMappedAddress();
    attrs::Attribute* parseErrorCode();
    attrs::Attribute* parseFingerprint();
    attrs::Attribute* parseMessageIntegrity();
    attrs::Attribute* parseUnknownAttribute();
    attrs::Attribute* parseValue();
    // LAP implementation
    int parseHeaderInitialAndType(MessageParserBuffer& buffer);
    int parseHeaderLength(MessageParserBuffer& buffer);
    int parseHeaderMagicCookie(MessageParserBuffer& buffer);
    int parseHeaderTransactionID(MessageParserBuffer& buffer);
    int parseAttributeType(MessageParserBuffer& buffer);
    int parseAttributeLength(MessageParserBuffer& buffer);
    int parseAttributeValueNotAdd(MessageParserBuffer& buffer);
    int parseAttributeValue(MessageParserBuffer& buffer);
    int parseAttributeFingerprintType(MessageParserBuffer& buffer);
    int parseAttributeFingerprintLength(MessageParserBuffer& buffer);
    int parseAttributeFingerprintValue(MessageParserBuffer& buffer);
    int parseMoreValue(MessageParserBuffer& buffer);
    // Ending states
    int parseEndMessageIntegrity(MessageParserBuffer& buffer);
    int parseEndWithFingerprint(MessageParserBuffer& buffer);
    std::size_t calculatePaddingSize(std::size_t value_bytes);

private:
    struct STUNHeader
    {
        int messageClass;
        int method;
        std::size_t length;
        nx::Buffer transactionId;
    };

    struct STUNAttr
    {
        std::uint16_t length;
        nx::Buffer value;
        std::uint16_t type;
        void clear()
        {
            length = 0;
            value.clear();
            type = 0;
        }
    };

    STUNHeader m_header;
    STUNAttr m_attribute;
    Message* m_outputMessage;
    std::size_t m_leftMessageLength;
    int m_state;
    // This is for opaque usage
    std::deque<char> m_tempBuffer;
};

} // namespace stun
} // namespace nx
