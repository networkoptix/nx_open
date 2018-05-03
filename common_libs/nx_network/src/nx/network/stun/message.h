#pragma once

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

#include <nx/utils/system_error.h>

#include "stun_attributes.h"

//!Implementation of STUN protocol (rfc5389)
namespace nx {
namespace network {
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
        NOTE: transactionId is generated using Header::makeTransactionId
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
        \warning value.count() MUST NOT be greater than std::numeric_limits<int>::max()
    */
    template<typename Rep, typename Period>
    void addAttribute(int type, std::chrono::duration<Rep, Period> value)
    {
        NX_ASSERT(value.count() <= std::numeric_limits<int>::max());
        addAttribute(std::make_shared<attrs::IntAttribute>(type, value.count()));
    }

    /** Add attribute of composite type.
        Attribute MUST be represented as a structure. E.g, MessageIntegrity
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

} // namespace stun
} // namespace network
} // namespace nx
