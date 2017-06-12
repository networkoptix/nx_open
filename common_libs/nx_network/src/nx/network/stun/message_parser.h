/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_PARSER_H
#define STUN_MESSAGE_PARSER_H

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
    enum {
        // Header
        HEADER_INITIAL_AND_TYPE ,
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
    // parsing component states
    enum {
        IN_PROGRESS,
        SECTION_FINISH,
        FINISH,
        FAILED
    };
public:
    MessageParser() :
        m_outputMessage(NULL),
        m_leftMessageLength(0),
        m_state(HEADER_INITIAL_AND_TYPE)
    {
        m_header.transactionId.resize( Header::TRANSACTION_ID_SIZE );
    }

    virtual void setMessage( Message* const msg ) override
    {
        m_outputMessage = msg;
    }
    //!Returns current parse state
    /*!
        Methods returns if:\n
            - end of message found
            - source data depleted

        \param buf
        \param bytesProcessed Number of bytes from \a buf which were read and parsed is stored here
        \note \a *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte)
        \note Reads whole message even if parse error occurred
    */
    virtual nx::network::server::ParserState parse(
        const nx::Buffer& /*buf*/,
        size_t* /*bytesProcessed*/ ) override;

    //!Returns current parse state
    nx::network::server::ParserState state() const {
        return m_state == HEADER_INITIAL_AND_TYPE
            ? nx::network::server::ParserState::init
            : nx::network::server::ParserState::readingMessage;
    }

    //!Resets parse state and prepares for parsing different data
    virtual void reset() override {
        m_state = HEADER_INITIAL_AND_TYPE;
    }
private:
    // Attribute value parsing
    attrs::Attribute* parseXORMappedAddress();
    attrs::Attribute* parseErrorCode();
    attrs::Attribute* parseFingerprint();
    attrs::Attribute* parseMessageIntegrity();
    attrs::Attribute* parseUnknownAttribute();
    attrs::Attribute* parseValue();
    // LAP implementation
    int parseHeaderInitialAndType( MessageParserBuffer& buffer );
    int parseHeaderLength( MessageParserBuffer& buffer );
    int parseHeaderMagicCookie( MessageParserBuffer& buffer );
    int parseHeaderTransactionID( MessageParserBuffer& buffer );
    int parseAttributeType( MessageParserBuffer& buffer );
    int parseAttributeLength( MessageParserBuffer& buffer );
    int parseAttributeValueNotAdd( MessageParserBuffer& buffer );
    int parseAttributeValue( MessageParserBuffer& buffer );
    int parseAttributeFingerprintType( MessageParserBuffer& buffer ) {
        int ret = parseAttributeType(buffer);
        m_state = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH;
        return ret;
    }
    int parseAttributeFingerprintLength( MessageParserBuffer& buffer ) {
        int ret = parseAttributeLength(buffer);
        m_state = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH;
        return ret;
    }
    int parseAttributeFingerprintValue( MessageParserBuffer& buffer ) {
        int ret = parseAttributeValueNotAdd(buffer);
        if( ret != SECTION_FINISH ) return ret;
        else {
            // Based on RFC , it says we MUST ignore the message following by message
            // integrity only fingerprint will need to handle. And we just check the
            // fingerprint message followed by the STUN packet. If we have found one
            // our parser state converts to a END_FINGERPINT states
            if( m_attribute.type == attrs::fingerPrint ) {
                m_state = END_FINGERPRINT;
            } else {
                m_state = MORE_VALUE;
            }
            return SECTION_FINISH;
        }
    }
    int parseMoreValue( MessageParserBuffer& buffer );
    // Ending states
    int parseEndMessageIntegrity( MessageParserBuffer& buffer );
    int parseEndWithFingerprint( MessageParserBuffer& buffer );
    std::size_t calculatePaddingSize( std::size_t value_bytes ) {
        // STUN RFC indicates that the length for the attribute is the length
        // prior to padding. So we still need to calculate the padding by ourself.
        static const std::size_t kAlignMask = 3;
        return (value_bytes + kAlignMask) & ~kAlignMask;
    }
private:
    struct STUNHeader {
        int messageClass;
        int method;
        std::size_t length;
        nx::Buffer transactionId;
    };
    struct STUNAttr {
        std::uint16_t length;
        nx::Buffer value;
        std::uint16_t type;
        void clear() {
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

} // namespase stun
} // namespase nx

#endif  //STUN_MESSAGE_PARSER_H
