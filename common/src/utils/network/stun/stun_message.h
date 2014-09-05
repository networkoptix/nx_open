/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_H
#define STUN_MESSAGE_H

#include <cstdint>
#include <utils/network/buffer.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <array>

//!Implementation of STUN protocol (rfc5389)
namespace nx_stun
{
    static const std::uint32_t MAGIC_COOKIE = 0x2112A442;
    // Four components for MAGIC_COOKIE which is used for parsing/serialization of XorMappedAddress
    static const std::uint16_t MAGIC_COOKIE_HIGH = static_cast<std::uint16_t>( MAGIC_COOKIE >> 16 );
    static const std::uint16_t MAGIC_COOKIE_LOW  = static_cast<std::uint16_t>( MAGIC_COOKIE & 0x0000ffff );
    // For STUN crc32 calculation 
    // 0x5354554e;
    static const std::uint32_t STUN_FINGERPRINT_XORMASK[4] = {
        0x0000004e,
        0x00005500,
        0x00540000,
        0x53000000
    };
    //96-bit transaction ID
    struct TransactionID
    {
        static const std::size_t TRANSACTION_ID_LENGTH = 12;
        union {
            char bytes[TRANSACTION_ID_LENGTH];
            struct {
                uint32_t high;
                uint64_t lo;
            } value;
        };
    };

    enum class MessageClass
    {
        request = 0,
        indication,
        successResponse,
        errorResponse
    };

    //!Contains STUN method types defined in RFC
    enum class MethodType
    {
        binding = 1
    };

    class Header
    {
    public:
        MessageClass messageClass;
        int method;
        int messageLength;
        TransactionID transactionID;

        Header()
        :
            messageClass( MessageClass::request ),
            method( 0 ),
            messageLength( 0 ),
            transactionID()
        {
        }
    };

    //!Contains STUN attributes
    namespace attr
    {
        enum class AttributeType
        {
            mappedAddress = 0x01,
            username = 0x06,
            messageIntegrity = 0x08,
            errorCode = 0x09,
            unknownAttribute = 0x0a,
            realm = 0x14,
            nonce = 0x15,
            xorMappedAddress = 0x20,

            software = 0x8022,
            alternateServer = 0x8023,
            fingerprint = 0x8028,

            unknown = 0xFFFF
        };

        class Attribute
        {
        public:
            AttributeType type;
            int length;
        };

        /*!
            \note All values are decoded
        */
        class XorMappedAddress
        :
            public Attribute
        {
        public:
            enum {
                IPV4 = 1,
                IPV6
            };
            int family;
            int port;
            union
            {
                uint32_t ipv4;
                union {
                    struct {
                        uint64_t hi;
                        uint64_t lo;
                    } numeric;
                    uint16_t array[8];
                } ipv6;
            } address;  //!< address in host byte order
        };

        class ErrorCode
        :
            public Attribute
        {
        public:
            //!contains hundreds of \a code
            int _class;
            //!This value is full error code (not modulo 100)
            int code;
            //!utf8 string, limited to 127 characters
            std::string reasonPhrase;
        };

        class FingerPrint
        :
            public Attribute
        {
        public:
            uint32_t crc32;
        };


        class MessageIntegrity
        :
            public Attribute
        {
        public:
            static const int SHA1_HASH_SIZE = 20;
            // for me to avoid raw loop when doing copy.
            std::array<uint8_t,SHA1_HASH_SIZE> hmac;
        };

        struct UnknownAttribute : public Attribute {
            nx::Buffer value;
        };

        class UnknownAttributes
        {
        public:
            std::vector<int> attributeTypes;
        };
    }

    class Message
    {
    public:
        Header header;
        typedef std::unordered_multimap<attr::AttributeType, std::unique_ptr<attr::Attribute> > AttributesMap;
        AttributesMap attributes;
        Message( Message&& message ) :
            attributes( std::move(message.attributes) ),
            header( std::move(message.header) ) {}
        Message& operator = ( Message&& message ) {
            if( this == &message ) return *this;
            attributes = std::move(message.attributes);
            header = message.header;
            return *this;
        }
        Message(){}
        ~Message(){}
        void clear();
    };

}

#endif  //STUN_MESSAGE_H
