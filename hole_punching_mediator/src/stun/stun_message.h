/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_H
#define STUN_MESSAGE_H

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>


//!Implementation of STUN protocol (rfc5389)
namespace nx_stun
{
    static const uint32_t MAGIC_COOKIE = 0x2112A442;

    //96-bit transaction ID
    struct TransactionID
    {
        uint32_t high;
        uint64_t lo;
    };

    enum class MessageClass
    {
        request = 0,
        indication,
        successResponse,
        errorResponse
    };

    class Header
    {
    public:
        MessageClass messageClass;
        int messageType;
        int messageLength;
        TransactionID transactionID;

        Header()
        :
            messageClass( MessageClass::request ),
            messageType( 0 ),
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
                struct
                {
                    uint64_t hi;
                    uint64_t lo;
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

        static const int SHA1_HASH_SIZE = 20;

        class MessageIntegrity
        :
            public Attribute
        {
        public:
            uint8_t hmac[SHA1_HASH_SIZE];
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
        std::unordered_multimap<attr::AttributeType, std::unique_ptr<attr::Attribute>> attributes;

        Message();

        void clear();
    };
}

#endif  //STUN_MESSAGE_H
