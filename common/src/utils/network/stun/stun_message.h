/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_H
#define STUN_MESSAGE_H

#include <stdint.h>
#include <utils/network/buffer.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <array>

//!Implementation of STUN protocol (rfc5389)
namespace nx_stun
{
    static const uint32_t MAGIC_COOKIE = 0x2112A442;

    //96-bit transaction ID
    // RFC indicates this should be treated as a bytes group, but here
    // we have numeric high and low associated with the bytes , I don't
    // know what should I put for high and low separately.
    struct TransactionID
    {
        union {
            unsigned char bytes[12];
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
            unknownAttributes = 0x0a,
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
        std::unordered_multimap<attr::AttributeType, std::unique_ptr<attr::Attribute> > attributes;

        void clear();

    };
}

#endif  //STUN_MESSAGE_H
