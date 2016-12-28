/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_H
#define STUN_MESSAGE_H

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <map>
#include <vector>

#ifdef _DEBUG
	#include <map>
#else
	#include <unordered_map>
#endif

#include <nx/network/buffer.h>
#include <nx/network/connection_server/base_protocol_message_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/systemerror.h>

//!Implementation of STUN protocol (rfc5389)
namespace nx {
namespace stun {

class MessageParserBuffer;
class MessageSerializerBuffer;

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
    errorResponse,
    unknown = -1,
};

//!Contains STUN method types defined in RFC
enum MethodType
{
    // STUN RFC 5389.
    bindingMethod = 1,

    // Starting value for custom NX methods in the middle of unasigned range 0x00D-0x0FF:
    // https://www.iana.org/assignments/stun-parameters/stun-parameters.txt
    userMethod = 0x050,

    // According to RFC 6062 indications used method codes from the same range.
    userIndication = 0x0F0,

    // Reserved by RFC 7983.
    reservedBegin = 0x100,
    reservedEnd = 0xFFF,

    invalid = -1,
};

class NX_NETWORK_API Header
{
public:
    Header();
    Header(const Header&);
    Header(Header&&);
    /**
        \note \a transactionId is generated using \a Header::makeTransactionId
    */
    Header(MessageClass messageClass_, int method_);
    Header(MessageClass messageClass_, int method_, Buffer transactionId_);

    Header& operator=(Header&& rhs);    //TODO #ak #msvc2015 =default

    static Buffer makeTransactionId();
    static Buffer nullTransactionId;
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
        tryAlternate        = 300,
        badRequest          = 400,
        unauthtorized       = 401,
        notFound            = 404,
        unknownAttribute    = 420,
        staleNonce          = 438,
        serverError         = 500,
    };
}

//!Contains STUN attributes
namespace attrs
{
    enum Type
    {
        mappedAddress       = 0x0001,
        userName            = 0x0006,
        messageIntegrity    = 0x0008,
        errorCode           = 0x0009,
        unknown             = 0x000A,
        realm               = 0x0014,
        nonce               = 0x0015,
        xorMappedAddress    = 0x0020,

        software            = 0x8022,
        alternateServer     = 0x8023,
        fingerPrint         = 0x8028,

        // Starting value for custom NX attributes in the middle of unasigned range 0xC003-0xFFFF:
        // https://www.iana.org/assignments/stun-parameters/stun-parameters.txt
        userDefined         = 0xE000,
    };

    struct NX_NETWORK_API Attribute
    {
        virtual ~Attribute() {}

        virtual int getType() const = 0;
    };

    class NX_NETWORK_API SerializableAttribute:
        public Attribute
    {
    public:
        virtual nx_api::SerializerState::Type serialize(
            MessageSerializerBuffer* buffer,
            std::size_t* bytesWritten) const = 0;
        virtual bool deserialize(MessageParserBuffer* buffer) = 0;
    };

    class AttributeFactory
    {
    public:
        static std::unique_ptr<SerializableAttribute> create(int attributeType);
    };

    class NX_NETWORK_API MappedAddress:
        public SerializableAttribute
    {
    public:
        static const int TYPE = mappedAddress;

        MappedAddress();
        MappedAddress(SocketAddress endpoint);

        virtual int getType() const override;
        virtual nx_api::SerializerState::Type serialize(
            MessageSerializerBuffer* buffer,
            std::size_t* bytesWritten) const override;
        virtual bool deserialize(MessageParserBuffer* buffer) override;

        const SocketAddress& endpoint() const;

    private:
        static const int kAddressTypeIpV4 = 1;
        static const int kAddressTypeIpV6 = 2;

        SocketAddress m_endpoint;
    };

    class NX_NETWORK_API AlternateServer:
        public MappedAddress
    {
    public:
        static const int TYPE = alternateServer;

        AlternateServer();
        AlternateServer(SocketAddress endpoint);

        virtual int getType() const override;
    };

    struct NX_NETWORK_API XorMappedAddress : Attribute
    {
        static const int TYPE = xorMappedAddress;

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

        virtual int getType() const override { return TYPE; }

        int family;
        int port;
        union {
            uint32_t ipv4;
            Ipv6 ipv6;
        } address;  //!< address in host byte order
    };

    struct NX_NETWORK_API BufferedValue
    {
        BufferedValue( nx::Buffer buffer_ = nx::Buffer() );
        const Buffer& getBuffer() const; //!< Value to serialize
        void setBuffer(Buffer buf);
        String getString() const; //!< Convert to string

    private:
        Buffer m_buffer;
    };

    struct NX_NETWORK_API UserName : Attribute, BufferedValue
    {
        static const int TYPE = userName;

        UserName( const String& value = String() );
        virtual int getType() const override { return TYPE; }
    };

    struct NX_NETWORK_API ErrorCode: Attribute, BufferedValue
    {
        static const int TYPE = errorCode;

        ErrorCode( int code_, const nx::String& phrase = nx::String() );
        virtual int getType() const override { return TYPE; }

        inline int getClass() const { return m_code / 100; }
        inline int getNumber() const { return m_code % 100; }
        inline int getCode() const { return m_code; }
        inline const nx::String& getReason() const { return m_reason; }

    private:
        int m_code;           //!< This value is full error code
        nx::String m_reason;  //!< utf8 string, limited to 127 characters
    };

    struct NX_NETWORK_API FingerPrint : Attribute
    {
        static const int TYPE = fingerPrint;

        FingerPrint( uint32_t crc32_ );
        virtual int getType() const override { return TYPE; }
        uint32_t getCrc32() const { return crc32; }

    private:
        uint32_t crc32;
    };


    struct NX_NETWORK_API MessageIntegrity : Attribute, BufferedValue
    {
        static const int TYPE = messageIntegrity;
        static const int SIZE = 20;

        MessageIntegrity( Buffer hmac = Buffer( SIZE, 0 ) );
        virtual int getType() const override { return TYPE; }
    };

    struct NX_NETWORK_API Nonce : Attribute, BufferedValue
    {
        static const int TYPE = nonce;

        Nonce( Buffer nonce = Buffer() );
        virtual int getType() const override { return TYPE; }
    };

    struct NX_NETWORK_API Unknown : Attribute, BufferedValue
    {
        static const int TYPE = unknown;

        Unknown( int userType_, nx::Buffer value_= nx::Buffer() );
        virtual int getType() const override { return userType; }

    private:
        int userType;
    };

    /** Base class for attributes with integer value */
    struct NX_NETWORK_API IntAttribute: Unknown
    {
        IntAttribute(int userType, int value = 0);

        int value() const;
    };
}

struct TransportHeader
{
    SocketAddress requestedEndpoint;
    SocketAddress locationEndpoint;
};

class NX_NETWORK_API Message
{
public:
    //TODO #ak is std::shared_ptr really needed here?
	typedef std::shared_ptr< attrs::Attribute > AttributePtr;

	typedef std::map< int, AttributePtr > AttributesMap;

    TransportHeader transportHeader;
    Header header;
    AttributesMap attributes;

    explicit Message(
        Header header_ = Header(),
        AttributesMap attributes_ = AttributesMap());

    void clear();
    void addAttribute(AttributePtr&& attribute);

    /** Adds integer attribute */
    void addAttribute(int type, int value)
    {
        addAttribute(std::make_shared<attrs::IntAttribute>(type, value));
    }

    void addAttribute(int type, bool value)
    {
        addAttribute(std::make_shared<attrs::IntAttribute>(type, value ? 1 : 0));
    }

    /** Add std::chrono::duration attribute.
        \warning \a value.count() MUST NOT be greater than \a std::numeric_limits<int>::max()
    */
    template<typename Rep, typename Period>
    void addAttribute(int type, std::chrono::duration<Rep, Period> value)
    {
        NX_ASSERT(value.count() <= std::numeric_limits<int>::max());
        addAttribute(std::make_shared<attrs::IntAttribute>(type, value.count()));
    }

    /** Add attribute of composite type.
        Attribute MUST be represented as a structure. E.g, \a MessageIntegrity
    */
    template<typename T, typename... Args>
    void newAttribute(Args... args)
    {
        addAttribute(std::make_shared<T>(std::move(args)...));
    }

    template< typename T >
    const T* getAttribute(int type) const
    {
        auto it = attributes.find(type);
        if (it == attributes.end())
            return nullptr;

        return static_cast<const T*>(it->second.get());
    }

    template< typename T >
    const T* getAttribute() const
    {
        return getAttribute<T>(T::TYPE);
    }

    void insertIntegrity(const String& userName, const String& key);
    bool verifyIntegrity(const String& userName, const String& key);

    boost::optional<QString> hasError(SystemError::ErrorCode code) const;
};

} // namespase stun
} // namespase nx

#endif  //STUN_MESSAGE_H
