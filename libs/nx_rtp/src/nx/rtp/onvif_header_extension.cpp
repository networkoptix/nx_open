// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "onvif_header_extension.h"

#include <nx/rtp/rtp.h>
#include <nx/utils/bit_stream.h>

namespace nx::rtp {

bool OnvifHeaderExtension::read(const uint8_t* data, int size)
{
    try
    {
        nx::utils::BitStreamReader bitstream(data, data + size);
        const uint16_t extensionId = bitstream.getBits(16);
        const uint16_t length = bitstream.getBits(16);

        if (extensionId != kOnvifHeaderExtensionId && extensionId != kOnvifHeaderExtensionAltId)
            return false;

        if (length < kOnvifHeaderExtensionLength)
            return false;

        const uint32_t seconds = bitstream.getBits(32);
        const uint32_t fractions = bitstream.getBits(32);
        cBit = bitstream.getBit();
        eBit = bitstream.getBit();
        dBit = bitstream.getBit();
        tBit = bitstream.getBit();
        bitstream.skipBits(4);
        cSeq = bitstream.getBits(8);

        const uint64_t msec =
            uint64_t(fractions) * std::micro::den / std::numeric_limits<uint32_t>::max();
        ntp = std::chrono::seconds(seconds) + std::chrono::microseconds(msec) - kNtpEpochTimeDiff;
    }
    catch(const nx::utils::BitStreamException&)
    {
        return false;
    }
    return true;
}

int OnvifHeaderExtension::write(uint8_t* data, int size) const
{
    if (size < OnvifHeaderExtension::kSize)
        return false;

    auto fraction = makeFraction(ntp.count(), std::micro::den);
    try
    {
        nx::utils::BitStreamWriter bitstream(data, data + size);
        bitstream.putBits(16, kOnvifHeaderExtensionId);
        bitstream.putBits(16, kOnvifHeaderExtensionLength);
        bitstream.putBits(32, fraction.first + kNtpEpochTimeDiff.count());
        bitstream.putBits(32, fraction.second);
        bitstream.putBit(cBit);
        bitstream.putBit(eBit);
        bitstream.putBit(dBit);
        bitstream.putBit(tBit);
        bitstream.putBits(4, 0);
        bitstream.putBits(8, cSeq);
        bitstream.putBits(16, 0x0000); //< padding
        bitstream.flushBits();
        return bitstream.getBytesCount();
    }
    catch(const nx::utils::BitStreamException&)
    {
        return 0;
    }
}

} // namespace nx::rtp
