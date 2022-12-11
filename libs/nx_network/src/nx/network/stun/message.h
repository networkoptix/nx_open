// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <map>
#include <optional>
#include <vector>

#include <nx/utils/buffer.h>
#include <nx/network/connection_server/base_protocol_message_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/cpp14.h>

#include <nx/utils/system_error.h>

#include "message_integrity.h"
#include "stun_attributes.h"

/**
 * Implementation of STUN protocol (rfc5389).
 */
namespace nx::network::stun {

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

NX_NETWORK_API const char* toString(MessageClass);

//!Contains STUN method types defined in RFC
enum MethodType
{
    // STUN RFC 5389.
    bindingMethod = 1,
    // TURN RFC 8656
    allocateMethod = 3,
    refreshMethod = 4,
    sendMethod = 6,
    dataMethod = 7,
    createPermissionMethod = 8,

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

    Header& operator=(Header&& rhs);    //TODO #akolesnikov #msvc2015 =default

    std::string toString() const;

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
    // TODO: #akolesnikov is std::shared_ptr really needed here?
    using AttributePtr = std::shared_ptr< attrs::Attribute >;

    TransportHeader transportHeader;
    Header header;

    explicit Message(Header header_ = Header());

    void clear();

    // Adds attribute replacing existing with the same type, if any.
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
     * \warning value.count() MUST NOT be greater than std::numeric_limits<int>::max().
     */
    template<typename Rep, typename Period>
    void addAttribute(int type, std::chrono::duration<Rep, Period> value)
    {
        NX_ASSERT(value.count() <= std::numeric_limits<int>::max());
        addAttribute(std::make_shared<attrs::IntAttribute>(type, value.count()));
    }

    /**
     * Add attribute of composite type.
     * Attribute MUST be represented as a structure. E.g, MessageIntegrity.
     */
    template<typename T, typename... Args>
    void newAttribute(Args... args)
    {
        addAttribute(std::make_shared<T>(std::move(args)...));
    }

    template< typename T >
    const T* getAttribute(int type) const
    {
        auto it = findAttr(type);
        if (it == m_attributes.end())
            return nullptr;

        return static_cast<const T*>(it->get());
    }

    template< typename T >
    const T* getAttribute() const
    {
        return getAttribute<T>(T::TYPE);
    }

    std::size_t attributeCount() const;

    void eraseAttribute(int type);
    void eraseAllAttributes();

    template<typename Func>
    // requires std::is_invocable_v<Func, const attrs::Attribute*>
    void forEachAttribute(Func func) const
    {
        std::for_each(
            m_attributes.begin(), m_attributes.end(),
            [&func](const auto& attr) { func(attr.get()); });
    }

    void insertIntegrity(const std::string& userName, const std::string& key);

    bool verifyIntegrity(
        const std::string& userName,
        const std::string& key,
        MessageIntegrityOptions options = {}) const;

    std::optional<std::string> hasError(SystemError::ErrorCode code) const;

private:
    using Attributes = std::vector<AttributePtr>;

    Attributes::iterator findAttr(int type);
    Attributes::const_iterator findAttr(int type) const;

private:
    Attributes m_attributes;
};

} // namespace nx::network::stun
