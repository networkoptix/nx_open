// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtp.h"

#include <nx/utils/log/log.h>

namespace nx::rtp {

bool RtpHeaderData::read(const uint8_t* data, int size)
{
    if (!data)
    {
        NX_ERROR(this, "Null data for RTP header parsing");
        return false;
    }

    if (size < RtpHeader::kSize)
    {
        NX_DEBUG(this, "Failed to parse RTP packet, invalid size: %1", size);
        return false;
    }

    header = *(RtpHeader*)(data);
    int headerSize = header.payloadOffset();
    if (headerSize > size)
    {
        NX_DEBUG(this, "Failed to parse RTP packet, invalid size: %1, header size %2",
            size, headerSize);
        return false;
    }

    if (header.extension)
    {
        if (headerSize + RtpHeaderExtensionHeader::kSize > size)
        {
            NX_DEBUG(this, "Invalid size of RTP header with an extension, size: %1", size);
            return false;
        }
        extension = RtpExtensionData();
        extension->header = *(RtpHeaderExtensionHeader*)(data + headerSize);
        extension->extensionOffset = headerSize + RtpHeaderExtensionHeader::kSize;
        extension->extensionSize = extension->header.lengthBytes();

        headerSize += RtpHeaderExtensionHeader::kSize + extension->header.lengthBytes();
        if (headerSize > size)
        {
            NX_DEBUG(this, "Invalid size of RTP header with an extension, size: %1, "
                "extension size %2", size, extension->header.lengthBytes());
            return false;
        }
    }

    int paddingSize = 0;
    if (header.padding)
    {
        // Zero padding is an error according to rfc3550, since value should include padding byte
        // itself, but we don't check it at this time.
        paddingSize = *(data + size - 1);
        if (paddingSize >= size)
        {
            NX_DEBUG(this, "Failed to parse RTP packet, invalid padding: %1", paddingSize);
            return false;
        }
    }

    if (size < paddingSize + headerSize)
    {
        NX_DEBUG(this, "Failed to parse RTP packet, invalid size: %1, padding: %2, header "
            "size: %3", size, paddingSize, headerSize);
        return false;
    }

    payloadOffset = headerSize;
    payloadSize = size - paddingSize - headerSize;
    return true;
}

} // namespace nx::rtp
