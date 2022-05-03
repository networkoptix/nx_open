// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/utils/buffer.h>

namespace nx::network::stun {

class Message;

struct MessageIntegrityOptions
{
    /**
     * When set to true, then buffer for the MESSAGE-INTEGRITY calculation
     * includes the MESSAGE-INTEGRITY attribute with value set to 20 zero bytes.
     * It is needed for compatibility between Cloud and older VMS versions.
     */
    bool legacyMode = false;
};

/**
 * Calculates message integrity value as specified at
 * https://datatracker.ietf.org/doc/html/rfc8489#page-40.
 */
NX_NETWORK_API Buffer calcMessageIntegrity(
    const Message& message,
    const std::string& key,
    MessageIntegrityOptions options);

/**
 * @return Position of MESSAGE-INTEGRITY attribute in the serializedMessage.
 * 0 if not found.
 */
NX_NETWORK_API std::size_t findMessageIntegrity(const Buffer& serializedMessage);

} // namespace nx::network::stun
