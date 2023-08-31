/*
 * Copyright (C) 2011, 2012, 2013 Citrix Systems
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <chrono>
#include <ctime>
#include <optional>
#include <string>

#include <stddef.h>
#include <stdint.h>

namespace nx::network::stun {

struct EncryptedBlock
{
    uint16_t nonceLength = 0;
    uint8_t nonce[256] = {0};
    uint16_t keyLength = 0;
    uint8_t macKey[20] = {0};
    // The integer number of seconds is contained in the first 48 bits of the field, and the
    // remaining 16 bits indicate the number of 1/64000 fractions of a second. Here the last
    // 16 bits are always 0.
    uint64_t timestamp = 0;
    uint32_t lifetime = 0;
};

std::optional<EncryptedBlock> decodeToken(const std::string& serverName,
    const std::string& base64EncodedKey,
    const std::string& base64EncodedToken);

std::optional<std::string> encodeToken(
    const std::string& serverName,
    const std::string& key,
    const EncryptedBlock& encryptedBlock);

} // namespace nx::network::stun
