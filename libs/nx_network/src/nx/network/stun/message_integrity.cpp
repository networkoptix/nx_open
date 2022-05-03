// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_integrity.h"

#include <nx/utils/auth/utils.h>

#include "message_parser.h"
#include "message_serializer.h"
#include "parse_utils.h"

namespace nx::network::stun {

static Buffer hmacSha1(
    const std::string& key,
    MessageIntegrityOptions options,
    const Buffer& msg,
    std::size_t messageIntegrityAttrPos)
{
    static constexpr char k20ZeroBytes[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    static_assert(sizeof(k20ZeroBytes) == attrs::MessageIntegrity::SIZE);

    std::vector<std::string_view> messageParts;
    if (options.legacyMode)
    {
        messageParts = {
            std::string_view(msg.data(), messageIntegrityAttrPos + 4), // Everything up to MESSAGE-INTEGRITY and attribute header.
            std::string_view(k20ZeroBytes, sizeof(k20ZeroBytes)) }; // Dummy MESSAGE-INTEGRITY value.
    }
    else
    {
        // All message attributes up to MESSAGE-INTEGRITY.
        messageParts = { std::string_view(msg.data(), messageIntegrityAttrPos) };
    }

    return nx::utils::auth::hmacSha1(key, messageParts);
}

Buffer calcMessageIntegrity(
    const Message& message,
    const std::string& key,
    MessageIntegrityOptions options)
{
    MessageSerializer serializer;
    serializer.setAlwaysAddFingerprint(false);
    Buffer buffer = serializer.serialized(message);

    // Searching for the MESSAGE-INTEGRITY attribute.
    std::size_t messageIntegrityAttrPos = findMessageIntegrity(buffer);
    if (messageIntegrityAttrPos == 0)
        return Buffer();

    // Adjusting message header size to point to the end of MESSAGE-INTEGRITY.
    std::uint16_t newSize =
        (messageIntegrityAttrPos - MessageParser::kMessageHeaderSize) +
        4 + attrs::MessageIntegrity::SIZE;
    newSize = ntohs(newSize);
    memcpy(buffer.data() + sizeof(newSize), &newSize, sizeof(newSize));

    return hmacSha1(key, options, buffer, messageIntegrityAttrPos);
}

std::size_t findMessageIntegrity(const Buffer& serializedMessage)
{
    Buffer::size_type pos = 0;
    MessageParserBuffer source(serializedMessage);
    source.skip(MessageParser::kMessageHeaderSize);
    while (!source.eof())
    {
        // reading attribute header.
        bool ok = true;
        const int type = source.NextUint16(&ok);
        if (type == attrs::messageIntegrity)
        {
            pos = source.position() - 2;
            break;
        }

        // reading attr length taking into account padding.
        const auto len = addPadding(source.NextUint16(&ok));
        if (!ok)
            return 0;

        source.skip(len);
    }

    return pos;
}

} // namespace nx::network::stun
