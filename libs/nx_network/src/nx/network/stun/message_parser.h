// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <deque>
#include <string>

#include <boost/crc.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/network/connection_server/base_protocol_message_types.h>
#include <nx/network/stun/message.h>
#include <nx/utils/buffer.h>

#include "stun_message_parser_buffer.h"
#include "../connection_server/base_protocol_message_types.h"

namespace nx::network::stun {

class NX_NETWORK_API MessageParser:
    public nx::network::server::AbstractMessageParser<Message>
{
private:
    enum class LegacyState
    {
        // Header
        HEADER_INITIAL_AND_TYPE = 0,
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
        END_FINGERPRINT,
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
    static constexpr std::size_t kMessageHeaderSize = 20;
    static constexpr std::size_t kAttrHeaderSize = 4;

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
        const nx::ConstBufferRefType& /*buf*/,
        size_t* /*bytesProcessed*/) override;

    /**
     * @return Current parse state.
     */
    nx::network::server::ParserState state() const;

    /**
     * Resets parse state and prepares for parsing different data.
     */
    virtual void reset() override;

    /**
     * If set to true, then message without valid FINGERPRINT attribute causes
     * nx::network::server::ParserState::failed parse result.
     * By default, it is set to false.
     */
    void setFingerprintRequired(bool val);

private:
    struct STUNHeader
    {
        int messageClass = 0;
        int method = 0;
        std::size_t length = 0;
        nx::Buffer transactionId;
    };

    struct STUNAttr
    {
        std::uint16_t length = 0;
        nx::Buffer value;
        std::uint16_t type = 0;

        void clear()
        {
            length = 0;
            value.clear();
            type = 0;
        }
    };

    enum class State
    {
        header,
        attrHeader,
        attrValue,
        failed,
        done,
    };

    /**
     * If there is at least m_bytesToCache bytes in the cache or the input buffer then refrence
     * to either input data or cache is returned.
     * If there is not enough data, then input data is copied to the cache and
     * server::ParserState::readingMessage is returned.
     * @param input Read data is removed from this buffer
     * @param bytesProcessed increased for the number of bytes read from the input.
     * @return tuple<data to parse, error>. data is defined only if error == std::nullopt.
     */
    std::tuple<nx::ConstBufferRefType, std::optional<server::ParserState>> getDataToParse(
        nx::ConstBufferRefType* input,
        std::size_t* bytesProcessed);

    void parseInternal(const nx::ConstBufferRefType& buf, size_t* bytesProcessed);

    // Attribute value parsing
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
    // Ending states

    bool parseHeader(MessageParserBuffer* buffer);

    bool validateCachedData();
    void updateCurrentMessageCrc32(const char* buf, std::size_t size);

private:
    STUNHeader m_header;
    STUNAttr m_attribute;
    Message* m_message = nullptr;
    LegacyState m_legacyState = LegacyState::HEADER_INITIAL_AND_TYPE;
    boost::crc_32_type m_currentMessageCrc32;
    bool m_fingerprintFound = false;

    State m_state = State::header;
    std::size_t m_bytesToCache = kMessageHeaderSize;
    std::size_t m_messageBytesParsed = 0;
    nx::Buffer m_cache;
    bool m_fingerprintRequired = false;
};

} // namespace nx::network::stun
