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

#include <utils/common/cpp14.h>

//!Implementation of STUN protocol (rfc5389)
namespace nx {
namespace stun {

static const std::uint32_t MAGIC_COOKIE = 0x2112A442;

// Four components for MAGIC_COOKIE which is used for parsing/serialization of XorMappedAddress
static const std::uint16_t MAGIC_COOKIE_HIGH = static_cast<std::uint16_t>( MAGIC_COOKIE >> 16 );
static const std::uint16_t MAGIC_COOKIE_LOW  = static_cast<std::uint16_t>( MAGIC_COOKIE & 0x0000ffff );

// For STUN crc32 calculation
// 0x5354554e;
static const std::uint32_t STUN_FINGERPRINT_XORMASK[4] =
{
    0x0000004e,
    0x00005500,
    0x00540000,
    0x53000000
};

enum class MessageClass
{
    request = 0,
    indication,
    successResponse,
    errorResponse
};

//!Contains STUN method types defined in RFC
enum MethodType
{
    BINDING = 1,
    USER_METHOD //!< Starting value for custom STUN methods
};

class Header
{
public:
    Header();
    Header( MessageClass messageClass_, int method_ );
    Header( MessageClass messageClass_, int method_, Buffer transactionId_ );

    static Buffer makeTransactionId();

    static const int TRANSACTION_ID_SIZE = 12;

public:
    MessageClass messageClass;
    int method;
    Buffer transactionId;
};

namespace error
{
    enum Value
    {
        TRY_ALTERNATE       = 300,
        BAD_REQUEST         = 400,
        UNAUTHTORIZED       = 401,
        UNKNOWN_ATTRIBUTE   = 420,
        STALE_NONCE         = 438,
        SERVER_ERROR        = 500,
    };
}

//!Contains STUN attributes
namespace attrs
{
    enum Type
    {
        MAPPED_ADDRESS      = 0x0001,
        USER_NAME           = 0x0006,
        MESSAGE_INTEGRITY   = 0x0008,
        ERROR_CODE          = 0x0009,
        UNKNOWN_ATTRIBUTE   = 0x000A,
        REALM               = 0x0014,
        NONCE               = 0x0015,
        XOR_MAPPED_ADDRESS  = 0x0020,

        SOFTWARE            = 0x8022,
        ALTERNATE_SERVER    = 0x8023,
        FINGER_PRINT        = 0x8028,

        USER_DEFINED        = 0x9000, //!<we need this to identify our private extended stun attr
        UNKNOWN             = 0xFFFF,
    };

    class Attribute
    {
    public:
        virtual int type() const = 0;
        virtual ~Attribute() {}
    };

    /*!
        \note All values are decoded
    */
    class XorMappedAddress
    :
        public Attribute
    {
    public:
        static const int TYPE = XOR_MAPPED_ADDRESS;

        enum
        {
            IPV4 = 1,
            IPV6
        };

        union Ipv6
        {
            struct {
                uint64_t hi;
                uint64_t lo;
            } numeric;
            uint16_t array[8];
        };

        XorMappedAddress();
        XorMappedAddress( int port_, uint32_t ipv4_ );
        XorMappedAddress( int port_, Ipv6 ipv6_ );

        virtual int type() const override { return TYPE; }

    public:
        int family;
        int port;
        union {
            uint32_t ipv4;
            Ipv6 ipv6;
        } address;  //!< address in host byte order
    };

    //!ERROR-CODE attribute [rfc5389, 15.6]
    class ErrorDescription
    :
        public Attribute
    {
    public:
        static const int TYPE = ERROR_CODE;

        ErrorDescription( int code_, nx::String phrase = nx::String() );
        virtual int type() const override { return TYPE; }

        inline int getClass() const { return code / 100; }
        inline int getNumber() const { return code % 100; }

    public:
        int code;           //!< This value is full error code
        nx::String reason;  //!< utf8 string, limited to 127 characters
    };

    class FingerPrint
    :
        public Attribute
    {
    public:
        static const int TYPE = FINGER_PRINT;

        FingerPrint( uint32_t crc32_ );
        virtual int type() const override { return TYPE; }

    public:
        uint32_t crc32;
    };


    class MessageIntegrity
    :
        public Attribute
    {
    public:
        static const int TYPE = MESSAGE_INTEGRITY;
        static const int SHA1_HASH_SIZE = 20;

        virtual int type() const override { return TYPE; }

    public:
        std::array<uint8_t, SHA1_HASH_SIZE> hmac; // for me to avoid raw loop when doing copy
    };

    class Unknown
    :
        public Attribute
    {
    public:
        static const int TYPE = UNKNOWN_ATTRIBUTE;

        Unknown( int userType_, nx::Buffer value_ = nx::Buffer() );
        virtual int type() const override { return userType; }

    public:
        int userType;
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
    typedef std::unordered_multimap<int, std::unique_ptr<attrs::Attribute> > AttributesMap;

    Header header;
    AttributesMap attributes;

    explicit Message( Header header = Header() );

    Message( Message&& message );
    Message& operator = ( Message&& message );

    void clear();
    void addAttribute( std::unique_ptr<attrs::Attribute>&& attribute );

    template< typename T >
    const T* getAttribute( int aType = T::TYPE, size_t index = 0 ) const;

    // TODO: variadic template when possible
    template< typename T, typename A1 >
    void newAttribute( A1 a1 )
    { addAttribute( std::make_unique<T>( std::move(a1) ) ); }

    template< typename T, typename A1, typename A2 >
    void newAttribute( A1 a1, A2 a2 )
    { addAttribute( std::make_unique<T>( std::move(a1), std::move(a2) ) ); }

    template< typename T, typename A1, typename A2, typename A3 >
    void newAttribute( A1 a1, A2 a2, A3 a3 )
    { addAttribute( std::make_unique<T>( std::move(a1), std::move(a2), std::move(a3) ) ); }

private:
    Message( const Message& );
    Message& operator=( const Message& );
};

template< typename T >
const T* Message::getAttribute( int aType, size_t index ) const
{
    auto it = attributes.find( aType );
    if( it == attributes.end() )
        return nullptr;

    while( index )
    {
        ++it;
        --index;
        if( it == attributes.end() || it->first != aType )
            return nullptr;
    }

    return static_cast< const T* >( it->second.get() );
}

} // namespase stun
} // namespase nx

#endif  //STUN_MESSAGE_H
