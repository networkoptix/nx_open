// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace nx::media::sei {

// A user_data_unregistered payload: a view into the decoded NAL buffer.
using UserDataPayload = std::vector<std::span<const uint8_t>>;

// Owns a decoded SEI RBSP buffer together with the user_data_unregistered payload views into it.
// The payloads are valid only as long as this object is alive. The type is move-only: moving
// transfers the buffer without relocating its bytes (so the views stay valid), while copying would
// leave the views dangling and is therefore forbidden.
struct SeiUserData
{
    std::vector<uint8_t> rbsp; //< Owns the decoded RBSP bytes the payloads point into.
    UserDataPayload payloads; //< Views into `rbsp`.

    SeiUserData() = default;
    SeiUserData(SeiUserData&&) = default;
    SeiUserData& operator=(SeiUserData&&) = default;
    SeiUserData(const SeiUserData&) = delete;
    SeiUserData& operator=(const SeiUserData&) = delete;
};

// Decodes the emulation prevention bytes of an SEI EBSP (the NAL unit payload past the NAL unit
// header) into an owned RBSP buffer, then parses the sei_message() loop of sei_rbsp() (identical
// in H.264 7.3.2.3 and H.265 7.3.2.4). On success returns a SeiUserData owning the decoded RBSP
// together with the user_data_unregistered payload views into it. Returns an error message if the
// EBSP cannot be decoded or the SEI is malformed.
// Do not pass an already-decoded RBSP: it would mostly work, silently corrupting only the
// payloads that contain the 00 00 03 pattern.
NX_CODEC_API std::expected<SeiUserData, std::string> extractUserData(
    std::span<const uint8_t> ebsp);

// Skips the NAL unit header and extracts the user_data_unregistered payloads from the rest of the
// NAL unit; the common implementation of the codec-specific parseSeiUserData functions.
inline std::expected<SeiUserData, std::string> parseSeiNalUnit(
    std::span<const uint8_t> naluEbsp, size_t nalHeaderLength)
{
    if (naluEbsp.size() <= nalHeaderLength)
        return std::unexpected("SEI NAL unit too short");

    return extractUserData(naluEbsp.subspan(nalHeaderLength));
}

} // namespace nx::media::sei
