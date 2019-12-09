#include "rtp.h"

#include <nx/utils/log/log.h>

namespace nx::streaming::rtp {

std::optional<int> calculateFullRtpHeaderSize(
    const uint8_t* rtpHeaderStart,
    int bufferSize)
{
    //Buffer size should be at least RtpHeader::kSize bytes long.
    const auto* const rtpHeader = (const RtpHeader* const) rtpHeaderStart;
    int headerSize = rtpHeader->payloadOffset();

    if (headerSize > bufferSize)
    {
        NX_VERBOSE(NX_SCOPE_TAG,
            "Header size (%1 bytes) is more than the provided buffer size (%2 bytes)",
            headerSize, bufferSize);

        return std::nullopt;
    }

    if (rtpHeader->extension)
    {
        const int sizeOfRtpHeaderWithExtensionHeader =
            headerSize + RtpHeaderExtensionHeader::kSize;

        if (bufferSize < sizeOfRtpHeaderWithExtensionHeader)
        {
            NX_VERBOSE(NX_SCOPE_TAG,
                "Size of header with an extension header (%1 bytes) is more "
                "than the provided buffer size (%2 bytes)",
                headerSize, sizeOfRtpHeaderWithExtensionHeader);

            return std::nullopt;
        }

        const auto* const extension = (RtpHeaderExtensionHeader*)(rtpHeaderStart + headerSize);
        headerSize += RtpHeaderExtensionHeader::kSize;

        const int kWordSize = 4;
        headerSize += ntohs(extension->length) * kWordSize;
        if (bufferSize < headerSize)
        {
            NX_VERBOSE(NX_SCOPE_TAG,
                "Size of header with extension (%1 bytes) is more "
                "than the provided buffer size (%2 bytes)",
                headerSize, bufferSize);

            return std::nullopt;
        }
    }

    return headerSize;
}

} // namespace nx::streaming::rtp
