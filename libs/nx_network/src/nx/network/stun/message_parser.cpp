// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_parser.h"

#include <bitset>
#include <cstdint>
#include <deque>

#include <nx/utils/qnbytearrayref.h>
#include <nx/utils/string.h>

#include "parse_utils.h"

namespace nx::network::stun {

using namespace attrs;

static constexpr std::uint32_t kMagicCookie = 0x2112A442;

MessageParser::MessageParser()
{
    reset();
}

void MessageParser::setMessage(Message* const msg)
{
    m_message = msg;
}

nx::network::server::ParserState MessageParser::parse(
    const nx::ConstBufferRefType& buf,
    std::size_t* bytesProcessed)
{
    // This caching function added as a workaround since the parsing logic
    // has numerous errors when parsing fragmented message.
    // So, providing only header / attributes / full message to the actual parser.

    nx::ConstBufferRefType input(buf);
    *bytesProcessed = 0;

    while (!input.empty())
    {
        const auto [bufToParse, readResult] = getDataToParse(&input, bytesProcessed);
        if (readResult)
            return *readResult;

        std::size_t bytesParsed = 0;
        parseInternal(bufToParse, &bytesParsed);
        m_cache.clear();

        if (bytesParsed != bufToParse.size())
            return nx::network::server::ParserState::failed;

        if (m_state == State::done || m_state == State::failed)
        {
            // Parsing completed or failed. Anyway, current message cannot be continued.
            auto result = m_state == State::done
                ? server::ParserState::done
                : server::ParserState::failed;

            if (m_fingerprintRequired && !m_fingerprintFound)
                result = server::ParserState::failed;

            reset();

            // The whole message has been processed (the size defined by the message header),
            // but did the parser stop? If not - likely, the message is malformed.
            // TODO: Move this condition somewhere.
            return result;
        }
    }

    return server::ParserState::readingMessage;
}

nx::network::server::ParserState MessageParser::state() const
{
    return m_legacyState == LegacyState::HEADER_INITIAL_AND_TYPE
        ? nx::network::server::ParserState::init
        : nx::network::server::ParserState::readingMessage;
}

void MessageParser::reset()
{
    m_header = {};
    m_header.transactionId.resize(Header::TRANSACTION_ID_SIZE);

    m_attribute = {};
    m_legacyState = LegacyState::HEADER_INITIAL_AND_TYPE;

    m_state = State::header;
    m_bytesToCache = kMessageHeaderSize;
    m_messageBytesParsed = 0;
    m_cache.clear();

    m_currentMessageCrc32 = boost::crc_32_type{};
    m_fingerprintFound = false;
}

void MessageParser::setFingerprintRequired(bool val)
{
    m_fingerprintRequired = val;
}

// Parsing for each specific type
Attribute* MessageParser::parseXORMappedAddress()
{
    if (m_attribute.value.size() < 8 || m_attribute.value.front() != 0)
        return NULL;
    MessageParserBuffer buffer(m_attribute.value);
    bool ok;

    // Parsing the family
    std::unique_ptr<XorMappedAddress> attribute(new XorMappedAddress());
    std::uint16_t family = buffer.NextUint16(&ok);
    NX_ASSERT(ok);
    // We only need the lower part of the word.
    family = family & 0x000000ff;
    if (family != XorMappedAddress::IPV4 && family != XorMappedAddress::IPV6)
        return NULL;

    attribute->family = family;
    // Parsing the port
    std::uint16_t xor_port = buffer.NextUint16(&ok);
    NX_ASSERT(ok);
    attribute->port = xor_port ^ MAGIC_COOKIE_HIGH;

    // Parsing the address
    if (attribute->family == XorMappedAddress::IPV4)
    {
        std::uint32_t xor_addr = buffer.NextUint32(&ok);
        NX_ASSERT(ok);
        attribute->address.ipv4 = xor_addr ^ MAGIC_COOKIE;
    }
    else
    {
        // ensure the buffer
        if (m_attribute.value.size() != 20)
            return NULL;

        // The RFC doesn't indicate how to concatenate, I just assume it with the natural byte order
        std::uint16_t xor_comp;
        // XOR with high part of MAGIC_COOKIE
        xor_comp = buffer.NextUint16(&ok);
        NX_ASSERT(ok);
        attribute->address.ipv6.words[0] = xor_comp ^ MAGIC_COOKIE_LOW;
        // XOR with low part of MAGIC_COOKIE
        xor_comp = buffer.NextUint16(&ok);
        NX_ASSERT(ok);
        attribute->address.ipv6.words[1] = xor_comp ^ MAGIC_COOKIE_HIGH;
        // XOR with rest of the transaction id
        for (std::size_t i = 0; i < 6; ++i)
        {
            xor_comp = buffer.NextUint16(&ok);
            NX_ASSERT(ok);
            attribute->address.ipv6.words[i + 2] =
                xor_comp ^ *reinterpret_cast< const std::uint16_t* >(
                    m_header.transactionId.data() + i * 2);
        }
    }

    return attribute.release();
}

Attribute* MessageParser::parseErrorCode()
{
    // Checking for the reserved bits
    if (*reinterpret_cast<const std::uint16_t*>(m_attribute.value.data()) != 0 || m_attribute.value.size() < 4)
        return NULL;
    nx::ConstBufferRefType refBuffer(m_attribute.value);
    MessageParserBuffer buffer(refBuffer);
    bool ok;
    std::uint32_t val = buffer.NextUint32(&ok);
    NX_ASSERT(ok);
    // The first 21 bits is for reservation, but the RFC says it SHOULD be zero, so ignore it.
    std::bitset<16> value(val & 0x0000ffff);

    // First comes 3 bits class
    int _class = 0;
    _class |= static_cast<int>(value[8]);
    _class |= static_cast<int>(value[9]) << 1;
    _class |= static_cast<int>(value[10]) << 2;
    // Checking the class value for the attribute _class
    if (_class < 3 || _class > 6)
        return NULL;
    // Get the least significant byte from the 32 bits dword
    // but the code must not be the value that is the modular, so we need
    // to compute the class and add it to the code as well
    int number = (val & 0x000000ff);
    if (number >= 100)
        return NULL;
    int code = _class * 100 + number;

    // Parsing the UTF encoded error string
    std::size_t phrase_length = m_attribute.value.size() - 4;
    std::string phrase;
    if (phrase_length > 0)
    {
        phrase = std::string(m_attribute.value.data() + 4, phrase_length);
        // The RFC says that the decoded UTF8 string should only contain less than 127 characters
        if (phrase.size() > 127)
        {
            return NULL;
        }
    }

    return new ErrorCode(code, phrase);
}

Attribute* MessageParser::parseFingerprint()
{
    if (m_attribute.value.size() != 4)
        return NULL;

    MessageParserBuffer buffer(m_attribute.value);
    bool ok;
    std::uint32_t xor_crc_32 = buffer.NextUint32(&ok);
    NX_ASSERT(ok);

    // XOR back the value that we get from our CRC32 value
    uint32_t crc32 =
        ((xor_crc_32 & 0x000000ff) ^ STUN_FINGERPRINT_XORMASK[0]) |
        ((xor_crc_32 & 0x0000ff00) ^ STUN_FINGERPRINT_XORMASK[1]) |
        ((xor_crc_32 & 0x00ff0000) ^ STUN_FINGERPRINT_XORMASK[2]) |
        ((xor_crc_32 & 0xff000000) ^ STUN_FINGERPRINT_XORMASK[3]);

    return new FingerPrint(crc32);
}

Attribute* MessageParser::parseMessageIntegrity()
{
    if (m_attribute.value.size() != MessageIntegrity::SIZE)
        return NULL;
    return new MessageIntegrity(m_attribute.value);
}

Attribute* MessageParser::parseUnknownAttribute()
{
    return new Unknown(m_attribute.type, m_attribute.value);
}

Attribute* MessageParser::parseValue()
{
    switch (m_attribute.type)
    {
        case attrs::xorMappedAddress:
            return parseXORMappedAddress();

        case attrs::errorCode:
            return parseErrorCode();

        case attrs::messageIntegrity:
            return parseMessageIntegrity();

        case attrs::fingerPrint:
            return parseFingerprint();

        case attrs::userName:
            return new UserName(toStdString(m_attribute.value));

        case attrs::nonce:
            return new Nonce(m_attribute.value);

        default:
            std::unique_ptr<SerializableAttribute> serializableAttribute =
                AttributeFactory::create(m_attribute.type);
            if (serializableAttribute)
            {
                MessageParserBuffer parseBuffer(m_attribute.value);
                if (serializableAttribute->deserialize(&parseBuffer))
                    return serializableAttribute.release();
            }

            return parseUnknownAttribute();
    }
}

int MessageParser::parseHeaderInitialAndType(MessageParserBuffer& buffer)
{
    NX_ASSERT(m_legacyState == LegacyState::HEADER_INITIAL_AND_TYPE);
    bool ok;
    std::bitset<16> value = buffer.NextUint16(&ok);
    if (!ok)
    {
        return IN_PROGRESS;
    }
    // Checking the initial 2 bits, this 2 bits are the MSB and its follower
    if (value[14] || value[15])
    {
        return FAILED;
    }
    else
    {
        // The later 14 bits formed the MessageClass and MessageMethod
        // based on the RFC5389, it looks like this:
        // M M M M M C M M M C M M M M
        // The M represents the Method bits and the C represents the type
        // information bits. Also, we need to take into consideration of the
        // bit order here.

        // Method
        m_header.method = 0;
        m_header.method |= static_cast<int>(value[0]);
        m_header.method |= static_cast<int>(value[1]) << 1;
        m_header.method |= static_cast<int>(value[2]) << 2;
        m_header.method |= static_cast<int>(value[3]) << 3;
        m_header.method |= static_cast<int>(value[5]) << 4; // skip the 5th C bit
        m_header.method |= static_cast<int>(value[6]) << 5;
        m_header.method |= static_cast<int>(value[7]) << 6;
        m_header.method |= static_cast<int>(value[9]) << 7; // skip the 9th C bit
        m_header.method |= static_cast<int>(value[10]) << 8;
        m_header.method |= static_cast<int>(value[11]) << 9;
        m_header.method |= static_cast<int>(value[12]) << 10;
        m_header.method |= static_cast<int>(value[13]) << 11;

        // Type
        m_header.messageClass = 0;
        m_header.messageClass |= static_cast<int>(value[4]);
        m_header.messageClass |= static_cast<int>(value[8]) << 1;
        m_legacyState = LegacyState::HEADER_LENGTH;
        return SECTION_FINISH;
    }
}

int MessageParser::parseHeaderLength(MessageParserBuffer& buffer)
{
    NX_ASSERT(m_legacyState == LegacyState::HEADER_LENGTH);
    bool ok;
    std::uint16_t val = buffer.NextUint16(&ok);
    if (!ok)
    {
        return IN_PROGRESS;
    }
    m_header.length = static_cast<std::size_t>(val);
    // Here we do a simple testing for the message length since based on the RCF
    // the message length must contains the bytes with padding which results in
    // the last 2 bits of the message length should always be zero .
    static const std::size_t kLengthMask = 3;
    if ((m_header.length & kLengthMask) != 0)
    {
        // We don't understand such header since the last 2 bits of the length
        // is not zero zero. This is another way to tell if a packet is STUN or not
        return FAILED;
    }
    m_legacyState = LegacyState::HEADER_MAGIC_ID;
    return SECTION_FINISH;
}

int MessageParser::parseHeaderMagicCookie(MessageParserBuffer& buffer)
{
    NX_ASSERT(m_legacyState == LegacyState::HEADER_MAGIC_ID);
    bool ok;
    std::uint32_t magic_id;
    magic_id = buffer.NextUint32(&ok);
    if (!ok)
    {
        return IN_PROGRESS;
    }
    if (MAGIC_COOKIE != magic_id)
        return FAILED;
    else
    {
        m_legacyState = LegacyState::HEADER_TRANSACTION_ID;
        return SECTION_FINISH;
    }
}

int MessageParser::parseHeaderTransactionID(MessageParserBuffer& buffer)
{
    NX_ASSERT(m_legacyState == LegacyState::HEADER_TRANSACTION_ID);
    bool ok;
    buffer.readNextBytesToBuffer(m_header.transactionId.data(), m_header.transactionId.size(), &ok);
    if (!ok)
    {
        return IN_PROGRESS;
    }
    // We are finished here so we should populate the Message object right now
    // Set left message length for parsing attributes
    // Populate the header
    m_message->header.messageClass = static_cast<MessageClass>(m_header.messageClass);
    m_message->header.method = static_cast<int>(m_header.method);
    m_message->header.transactionId = m_header.transactionId;

    m_legacyState = LegacyState::MORE_VALUE;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeType(MessageParserBuffer& buffer)
{
    bool ok;
    m_attribute.type = buffer.NextUint16(&ok);
    if (!ok)
    {
        return IN_PROGRESS;
    }
    m_legacyState = LegacyState::ATTRIBUTE_LENGTH;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeLength(MessageParserBuffer& buffer)
{
    bool ok;
    m_attribute.length = buffer.NextUint16(&ok);
    if (!ok)
    {
        return IN_PROGRESS;
    }
    m_legacyState = LegacyState::ATTRIBUTE_VALUE;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeValueNotAdd(MessageParserBuffer& buffer)
{
    std::size_t valueWithPaddingLength = addPadding(m_attribute.length);
    m_attribute.value.resize(static_cast<int>(m_attribute.length));
    bool ok = false;
    buffer.readNextBytesToBuffer(m_attribute.value.data(), m_attribute.length, &ok);
    if (!ok)
        return IN_PROGRESS;
    //skipping padding (valueWithPaddingLength-m_attribute.length) bytes
    //TODO #akolesnikov add skipBytes method to buffer
    char paddingBuf[4];
    buffer.readNextBytesToBuffer(
        paddingBuf,
        valueWithPaddingLength - m_attribute.length,
        &ok);
    if (!ok)
        return IN_PROGRESS;
    // Modify the left message length field : total size
    // The value length + 2 bytes type + 2 bytes length
    return SECTION_FINISH;
}

int MessageParser::parseAttributeValue(MessageParserBuffer& buffer)
{
    int ret = parseAttributeValueNotAdd(buffer);
    if (ret != SECTION_FINISH)
        return ret;
    Attribute* attr = parseValue();
    if (attr == NULL)
        return FAILED;

    const auto attrType = attr->getType();
    m_message->addAttribute(std::unique_ptr<Attribute>(attr));

    switch (attrType)
    {
        case attrs::fingerPrint:
            m_legacyState = LegacyState::END_FINGERPRINT;
            break;
        case attrs::messageIntegrity:
            m_legacyState = LegacyState::END_MESSAGE_INTEGRITY;
            break;
        default:
            m_legacyState = LegacyState::MORE_VALUE;
    }

    return SECTION_FINISH;
}

int MessageParser::parseAttributeFingerprintType(MessageParserBuffer& buffer)
{
    m_legacyState = LegacyState::ATTRIBUTE_TYPE;
    int ret = parseAttributeType(buffer);
    m_legacyState = LegacyState::ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH;
    return ret;
}

int MessageParser::parseAttributeFingerprintLength(MessageParserBuffer& buffer)
{
    m_legacyState = LegacyState::ATTRIBUTE_LENGTH;
    int ret = parseAttributeLength(buffer);
    m_legacyState = LegacyState::ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_VALUE;
    return ret;
}

int MessageParser::parseAttributeFingerprintValue(MessageParserBuffer& buffer)
{
    int ret = parseAttributeValueNotAdd(buffer);
    if (ret != SECTION_FINISH) return ret;
    else
    {
        // Based on RFC , it says we MUST ignore the message following by message
        // integrity only fingerprint will need to handle. And we just check the
        // fingerprint message followed by the STUN packet. If we have found one
        // our parser state converts to a END_FINGERPINT states
        if (m_attribute.type == attrs::fingerPrint)
        {
            m_legacyState = LegacyState::END_FINGERPRINT;
        }
        else
        {
            m_legacyState = LegacyState::MORE_VALUE;
        }
        return SECTION_FINISH;
    }
}

std::tuple<nx::ConstBufferRefType, std::optional<server::ParserState>>
    MessageParser::getDataToParse(
        nx::ConstBufferRefType* input,
        std::size_t* bytesProcessed)
{
    if (!NX_ASSERT(m_cache.size() < m_bytesToCache))
        return {{}, {nx::network::server::ParserState::failed}};

    nx::ConstBufferRefType bufToParse;
    const auto bytesToCopy = std::min<std::size_t>(m_bytesToCache - m_cache.size(), input->size());
    if (m_cache.empty() && bytesToCopy == m_bytesToCache)
    {
        bufToParse = input->substr(0, bytesToCopy);
        input->remove_prefix(bytesToCopy);
        *bytesProcessed += bytesToCopy;
    }
    else
    {
        m_cache.append(input->data(), bytesToCopy);
        input->remove_prefix(bytesToCopy);
        *bytesProcessed += bytesToCopy;
        if (!validateCachedData())
            return {{}, {server::ParserState::failed}};
        if (m_cache.size() < m_bytesToCache)
            return {{}, {server::ParserState::readingMessage}};
        bufToParse = m_cache;
    }

    return {bufToParse, std::nullopt};
}

void MessageParser::parseInternal(
    const nx::ConstBufferRefType& buf,
    std::size_t* bytes_transferred)
{
    // NOTE: Header and attributes are always delivered separately to this function.

    MessageParserBuffer buffer(buf);

    switch (m_state)
    {
        case State::header:
        {
            bool res = parseHeader(&buffer);
            *bytes_transferred = buffer.position();
            m_messageBytesParsed += buffer.position();
            if (!res)
            {
                m_state = State::failed;
                break;
            }

            updateCurrentMessageCrc32(buf.data(), buffer.position());

            if (m_header.length <= 0)
            {
                m_state = State::done;
                break;
            }

            m_bytesToCache = kAttrHeaderSize;
            m_state = State::attrHeader;
            break;
        }

        case State::attrHeader:
        {
            const bool res = parseAttributeType(buffer) == SECTION_FINISH &&
                parseAttributeLength(buffer) == SECTION_FINISH;
            *bytes_transferred += buffer.position();
            m_messageBytesParsed += buffer.position();
            if (!res)
            {
                m_state = State::failed;
                break;
            }

            if (m_attribute.type != attrs::fingerPrint)
                updateCurrentMessageCrc32(buf.data(), buffer.position());

            m_bytesToCache = m_attribute.length;
            if ((m_bytesToCache & 0x03) > 0)
                m_bytesToCache = (m_bytesToCache & ~0x03) + 4; // Taking into account 32-bit boundary padding.
            m_state = State::attrValue;
            if (m_bytesToCache > 0)
                break;

            // The attribute value length is zero. So, processing the attribute right away.
            buffer = MessageParserBuffer(buf.substr(buffer.position()));
            [[fallthrough]];
        }

        case State::attrValue:
        {
            const bool res = parseAttributeValue(buffer) == SECTION_FINISH;
            *bytes_transferred += buffer.position();
            m_messageBytesParsed += buffer.position();
            if (!res)
            {
                m_state = State::failed;
                break;
            }

            if (m_attribute.type != attrs::fingerPrint)
            {
                updateCurrentMessageCrc32(buf.data(), buffer.position());
            }
            else
            {
                // FINGERPRINT.
                const attrs::FingerPrint* fingerprint = m_message->getAttribute<attrs::FingerPrint>();
                if (fingerprint->getCrc32() != m_currentMessageCrc32.checksum())
                {
                    m_state = State::failed;
                    break;
                }
                m_fingerprintFound = true;
            }

            m_bytesToCache = kAttrHeaderSize;
            m_state = m_messageBytesParsed < (m_header.length + kMessageHeaderSize)
                ? State::attrHeader : State::done;
            break;
        }

        default:
            m_state = State::failed;
            break;
    }

    if (m_state != State::done && m_state != State::failed &&
        m_messageBytesParsed + m_bytesToCache > kMessageHeaderSize + m_header.length)
    {
        // Packet length in the header is inconsistent with some attribute length.
        m_state = State::failed;
    }
}

bool MessageParser::parseHeader(MessageParserBuffer* buffer)
{
    return parseHeaderInitialAndType(*buffer) == SECTION_FINISH
        && parseHeaderLength(*buffer) == SECTION_FINISH
        && parseHeaderMagicCookie(*buffer) == SECTION_FINISH
        && parseHeaderTransactionID(*buffer) == SECTION_FINISH;
}

bool MessageParser::validateCachedData()
{
    if (m_state == State::header)
    {
        // The most significant 2 bits of every STUN message MUST be zeroes.
        if (!m_cache.empty() && (m_cache[0] & 0xC0) != 0)
            return false;

        if (m_cache.size() >= 8)
        {
            // The magic cookie field MUST contain the fixed value 0x2112A442 in network byte order.
            std::uint32_t magicCookie = 0;
            memcpy(&magicCookie, m_cache.data() + 4, sizeof(std::uint32_t));
            if (ntohl(magicCookie) != kMagicCookie)
                return false;
        }
    }

    return true;
}

void MessageParser::updateCurrentMessageCrc32(const char* buf, std::size_t size)
{
    m_currentMessageCrc32.process_bytes(buf, size);
}

} // nx::network::stun
