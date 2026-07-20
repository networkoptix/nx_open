// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sei_common.h"

#include <algorithm>
#include <optional>

#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

namespace nx::media::sei {

namespace {

constexpr int kUserDataUnregisteredType = 5;

} // namespace

std::expected<SeiUserData, std::string> extractUserData(std::span<const uint8_t> ebsp)
{
    // Remove the emulation prevention bytes; the RBSP is never larger than the source EBSP.
    SeiUserData result;
    result.rbsp.resize(ebsp.size());
    const int rbspSize = nx::media::nal::decodeEpb(
        ebsp.data(), ebsp.data() + ebsp.size(), result.rbsp.data(), result.rbsp.size());
    if (rbspSize <= 0)
        return std::unexpected("Bad SEI detected. Failed to decode RBSP");
    result.rbsp.resize(rbspSize);

    const uint8_t* curBuff = result.rbsp.data();
    const uint8_t* nalEnd = result.rbsp.data() + result.rbsp.size();

    // Strip rbsp_trailing_bits, a single 0x80 byte for the byte-aligned SEI payloads. Real vendor
    // SEIs keep it even when the last payload itself is truncated, so it must not be counted as
    // payload data by the truncation tolerance below.
    if (curBuff < nalEnd && nalEnd[-1] == 0x80)
        --nalEnd;

    const auto readByte = [&curBuff, nalEnd]() -> std::optional<uint8_t>
    {
        if (curBuff >= nalEnd)
            return std::nullopt;
        return *curBuff++;
    };

    // Reads a value coded as a run of 0xff bytes terminated by a non-0xff byte, as used for both
    // payloadType and payloadSize. Returns nullopt if the buffer ends before the terminating byte.
    const auto readFfCodedValue = [&readByte]() -> std::optional<uint64_t>
    {
        uint64_t value = 0;
        for (;;)
        {
            const auto byte = readByte();
            if (!byte)
                return std::nullopt;
            value += *byte;
            if (*byte != 0xff)
                return value;
        }
    };

    // A sei_message() needs at least the payloadType and payloadSize bytes, so a single leftover
    // byte cannot start one; tolerate it as vendor padding rather than failing the whole SEI.
    while (curBuff + 1 < nalEnd)
    {
        const auto payloadType = readFfCodedValue();
        if (!payloadType)
            return std::unexpected("Bad SEI detected. SEI too short - unable to read type");

        const auto payloadSize = readFfCodedValue();
        if (!payloadSize)
            return std::unexpected("Bad SEI detected. SEI too short - unable to read size");

        const auto realPayloadSize = std::min(*payloadSize, uint64_t(nalEnd - curBuff));
        if (realPayloadSize < *payloadSize)
        {
            NX_VERBOSE(
                NX_SCOPE_TAG,
                "Truncated SEI type %1 from size %2 to %3 bytes",
                *payloadType,
                *payloadSize,
                realPayloadSize);
        }

        if (*payloadType == kUserDataUnregisteredType)
            result.payloads.emplace_back(curBuff, realPayloadSize);
        curBuff += realPayloadSize;
    }

    return result;
}

} // namespace nx::media::sei
