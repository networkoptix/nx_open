/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_PARSER_H
#define STUN_MESSAGE_PARSER_H
#include <memory>
#include <cstdint>
#include <deque>

#include <utils/network/abstract_socket.h>
#include <utils/network/stun/stun_message.h>
#include <utils/network/buffer.h>

#include "../connection_server/base_protocol_message_types.h"


namespace nx_stun
{
    class MessageParser {
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
            output_message_(NULL),
            left_message_length_(0),
            state_(HEADER_INITIAL_AND_TYPE){
        }

        //!set message object to parse to
        void setMessage( Message* const msg ) {
            output_message_ = msg;
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
        nx_api::ParserState::Type parse( const nx::Buffer& /*buf*/, size_t* /*bytesProcessed*/ );

        //!Returns current parse state
        nx_api::ParserState::Type state() const {
            return state_ == HEADER_INITIAL_AND_TYPE ? nx_api::ParserState::init : nx_api::ParserState::inProgress;
        }

        //!Resets parse state and prepares for parsing different data
        void reset() {
            state_ = HEADER_INITIAL_AND_TYPE;
        }
    private:
        class MessageParserBuffer;
        // Attribute value parsing
        attr::Attribute* parseXORMappedAddress();
        attr::Attribute* parseErrorCode();
        attr::Attribute* parseFingerprint();
        attr::Attribute* parseMessageIntegrity();
        attr::Attribute* parseUnknownAttribute();
        attr::Attribute* parseValue();
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
            state_ = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH;
            return ret;
        }
        int parseAttributeFingerprintLength( MessageParserBuffer& buffer ) {
            int ret = parseAttributeLength(buffer);
            state_ = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH;
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
                if( attribute_.type == static_cast<int>(attr::AttributeType::fingerprint) ) {
                    state_ = END_FINGERPRINT;
                } else {
                    state_ = MORE_VALUE;
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
            int message_class;
            int message_method;
            std::size_t message_length;
            TransactionID transaction_id;
        };
        struct STUNAttr {
            std::uint16_t length;
            nx::Buffer value;
            std::uint16_t type;
            void Clear() {
                length = 0;
                value.clear();
                type = 0;
            }
        };
        STUNHeader header_;
        STUNAttr attribute_;
        Message* output_message_;
        std::size_t left_message_length_;
        int state_;
        // This is for opaque usage  
        std::deque<char> temp_buffer_;
    };
}

#endif  //STUN_MESSAGE_PARSER_H
