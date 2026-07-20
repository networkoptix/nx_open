// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>

#include <nx/codec/sei_common.h>

namespace nx::media::h265 {

// Parses an H.265 SEI NAL unit as it appears in the bitstream: the 2-byte NAL unit header
// followed by the EBSP payload, still containing the emulation prevention bytes, which are
// removed internally. Do not pass an already-decoded RBSP: it would mostly work, silently
// corrupting only the payloads that contain the 00 00 03 pattern. On success the returned
// SeiUserData owns the decoded RBSP buffer the payloads point into. Returns an error message if
// the NAL unit is malformed.
NX_CODEC_API std::expected<sei::SeiUserData, std::string> parseSeiUserData(
    std::span<const uint8_t> naluEbsp);

} // namespace nx::media::h265
